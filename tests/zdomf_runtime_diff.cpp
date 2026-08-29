#include "zdomf_gte_compose.hpp"
#include "zdomf_hierarchy.hpp"
#include "zdomf_mocap.hpp"
#include "zdomf_model.hpp"
#include "zdomf_projection.hpp"
#include "zdomf_runtime_records.hpp"
#include "zdomf_runtime.hpp"
#include "zdomf_transform.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Rgb { std::uint8_t r, g, b; };
struct Point { double x, y; };
struct DrawFace {
    std::array<Point, 3> points{};
    double depth = 0;
    std::uint8_t part = 0;
};

double edge(Point a, Point b, Point p) {
    return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
}

void fill_triangle(std::vector<Rgb>& image, const DrawFace& face, Rgb color) {
    const auto area = edge(face.points[0], face.points[1], face.points[2]);
    if (std::abs(area) < 0.01) return;
    const int min_x = std::max(0, int(std::floor(std::min(
        {face.points[0].x, face.points[1].x, face.points[2].x}))));
    const int max_x = std::min(511, int(std::ceil(std::max(
        {face.points[0].x, face.points[1].x, face.points[2].x}))));
    const int min_y = std::max(0, int(std::floor(std::min(
        {face.points[0].y, face.points[1].y, face.points[2].y}))));
    const int max_y = std::min(239, int(std::ceil(std::max(
        {face.points[0].y, face.points[1].y, face.points[2].y}))));
    for (int y = min_y; y <= max_y; ++y) for (int x = min_x; x <= max_x; ++x) {
        const Point p{double(x) + 0.5, double(y) + 0.5};
        const auto a = edge(face.points[1], face.points[2], p) / area;
        const auto b = edge(face.points[2], face.points[0], p) / area;
        const auto c = 1.0 - a - b;
        if (a >= 0 && b >= 0 && c >= 0)
            image[std::size_t(y) * 512 + x] = color;
    }
}

void write_ppm(const std::filesystem::path& path, const std::vector<Rgb>& image) {
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot write differential frame");
    output << "P6\n512 240\n255\n";
    output.write(reinterpret_cast<const char*>(image.data()),
                 static_cast<std::streamsize>(image.size() * sizeof(Rgb)));
}

std::vector<std::uint8_t> bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("missing differential input: " + path.string());
    return {(std::istreambuf_iterator<char>(input)), {}};
}

std::uint32_t u32(const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset + 4 > data.size()) throw std::runtime_error("truncated RAM pointer");
    return std::uint32_t(data[offset]) | (std::uint32_t(data[offset + 1]) << 8) |
           (std::uint32_t(data[offset + 2]) << 16) |
           (std::uint32_t(data[offset + 3]) << 24);
}

std::int16_t s16(const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset + 2 > data.size()) throw std::runtime_error("truncated RAM s16");
    return static_cast<std::int16_t>(std::uint16_t(data[offset]) |
                                     (std::uint16_t(data[offset + 1]) << 8));
}

nba97::ZdomfVec3 vec3(const std::vector<std::uint8_t>& ram,
                      std::uint32_t address) {
    const auto offset = static_cast<std::size_t>(address & 0x001fffffu);
    return {s16(ram, offset), s16(ram, offset + 2), s16(ram, offset + 4)};
}

std::uint64_t rotation_error(const nba97::ZdomfTransform& a,
                             const nba97::ZdomfTransform& b) {
    std::uint64_t error = 0;
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t column = 0; column < 3; ++column) {
            const auto delta = std::int64_t(a.rotation[row][column]) -
                               b.rotation[row][column];
            error += static_cast<std::uint64_t>(delta * delta);
        }
    return error;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 6) throw std::runtime_error(
            "usage: zdomf-runtime-diff <nopsx-ram.bin> <ZFEMOCAP.BIN> <model-root> <ZDOMF*.BIN> <output.ppm>");
        const auto ram = bytes(argv[1]);
        if (ram.size() != 0x200000)
            throw std::runtime_error("no$psx snapshot must contain exactly 2 MiB");
        const auto runtime_base = u32(ram, 0x0e4578);
        const auto records = nba97::decode_zdomf_runtime_records(ram, runtime_base);
        const auto& expected_parents = nba97::zdomf_parent_table();
        std::cout << "[DIFF] FUN_80069098 runtime-base=0x" << std::hex
                  << runtime_base << std::dec << " records=20 stride=148\n";
        for (std::size_t part = 0; part < records.size(); ++part) {
            const auto& record = records[part];
            std::cout << "[DIFF] part=" << part
                      << " parent=" << int(record.primary_parent)
                      << " alternate-parent=" << int(record.alternate_parent)
                      << " pivot=0x" << std::hex << record.pivot_pointer
                      << " geometry=0x" << record.geometry_pointer << std::dec
                      << " translation=" << record.current_matrix.translation[0] << '/'
                      << record.current_matrix.translation[1] << '/'
                      << record.current_matrix.translation[2] << '\n';
            if (record.primary_parent != expected_parents[part] ||
                record.alternate_parent != expected_parents[part])
                throw std::runtime_error("first mismatch: FUN_80069098 parent routing part " +
                                         std::to_string(part));
        }
        std::cout << "[DIFF] FUN_80069098 primary+alternate pointer graph MATCH\n";

        const std::filesystem::path model_root = argv[3];
        auto model = nba97::load_zdomf_model(argv[4]);
        const auto transforms = nba97::load_zdomf_base_transforms(
            model_root / "ZDEFLIST.BIN", model_root / "ZDOMTRIG.BIN");
        std::array<nba97::ZdomfVec3, 20> preprocessed_pivots{};
        std::size_t pivot_matches = 0;
        for (std::size_t part = 0; part < records.size(); ++part) {
            const auto native = nba97::apply_zdomf_transform(
                transforms.parts[part], model.pivots[part]);
            preprocessed_pivots[part] = native;
            const auto original = vec3(ram, records[part].pivot_pointer);
            if (native.x != original.x || native.y != original.y || native.z != original.z)
                throw std::runtime_error("first mismatch: preprocessed pivot part " +
                                         std::to_string(part));
            ++pivot_matches;
        }
        std::cout << "[DIFF] FUN_80062F4C pivots MATCH " << pivot_matches << "/20\n";

        std::size_t vertex_matches = 0;
        for (const auto& face : model.primary_faces) {
            for (const auto& corner : face.corners) {
                const auto geometry = records[corner.part].geometry_pointer;
                const auto geometry_offset =
                    static_cast<std::size_t>(geometry & 0x001fffffu);
                const auto count = u32(ram, geometry_offset);
                const auto vertex_pointer = u32(ram, geometry_offset + 4);
                if (count != model.part_triangle_counts[corner.part])
                    throw std::runtime_error("first mismatch: geometry count part " +
                                             std::to_string(corner.part));
                const auto original = vec3(ram, vertex_pointer +
                    std::uint32_t(corner.triangle_index) * 24u +
                    std::uint32_t(corner.component) * 8u);
                const auto native = nba97::apply_zdomf_transform(
                    transforms.parts[corner.part], corner.position);
                if (native.x != original.x || native.y != original.y ||
                    native.z != original.z) {
                    std::cerr << "[DIFF] vertex mismatch part=" << int(corner.part)
                              << " triangle=" << corner.triangle_index
                              << " component=" << int(corner.component)
                              << " native=" << native.x << '/' << native.y << '/'
                              << native.z << " original=" << original.x << '/'
                              << original.y << '/' << original.z << '\n';
                    throw std::runtime_error("first mismatch: FUN_800631B8 transformed vertex");
                }
                ++vertex_matches;
            }
        }
        std::cout << "[DIFF] FUN_800631B8 transformed vertices MATCH "
                  << vertex_matches << '/' << model.primary_faces.size() * 3 << '\n';

        const std::array<Rgb, 8> palette{{
            {238, 196, 74}, {81, 157, 225}, {205, 91, 117}, {168, 91, 192},
            {86, 190, 153}, {225, 127, 65}, {225, 226, 230}, {116, 102, 211},
        }};
        std::vector<DrawFace> draw;
        draw.reserve(model.primary_faces.size());
        for (const auto& face : model.primary_faces) {
            DrawFace target{};
            target.part = face.corners[2].part;
            for (std::size_t corner_index = 0; corner_index < 3; ++corner_index) {
                const auto& corner = face.corners[corner_index];
                const auto& record = records[corner.part];
                const auto geometry_offset = static_cast<std::size_t>(
                    record.geometry_pointer & 0x001fffffu);
                const auto vertex_pointer = u32(ram, geometry_offset + 4);
                const auto original_vertex = vec3(ram, vertex_pointer +
                    std::uint32_t(corner.triangle_index) * 24u +
                    std::uint32_t(corner.component) * 8u);
                nba97::ZdomfProjectionConfig projection{};
                projection.camera = record.current_matrix;
                projection.camera.translation = nba97::decode_zdomf_runtime_matrix(
                    ram, record.primary_parent_pointer).translation;
                projection.draw_offset_x = 0;
                const auto projected = nba97::project_zdomf_vertex(
                    projection, original_vertex);
                target.points[corner_index] = {double(projected.x), double(projected.y)};
                target.depth += projected.depth / 3.0;
            }
            draw.push_back(target);
        }
        std::sort(draw.begin(), draw.end(), [](const DrawFace& a, const DrawFace& b) {
            return a.depth > b.depth;
        });
        std::vector<Rgb> frame(512 * 240, {5, 8, 16});
        for (const auto& face : draw)
            fill_triangle(frame, face, palette[face.part % palette.size()]);
        write_ppm(argv[5], frame);
        std::cout << "[DIFF] exact record-contract frame=" << argv[5] << '\n';

        const auto mocap = nba97::load_zdomf_mocap(argv[2]);
        const auto trig = bytes(model_root / "ZDOMTRIG.BIN");
        const auto original_frontend = nba97::decode_zdomf_runtime_matrix(
            ram, 0x800ed278u);
        auto recovered_frontend = nba97::make_zdomf_rotation(
            trig, {2051, 191, 0});
        for (auto& value : recovered_frontend.rotation[0])
            value = static_cast<std::int16_t>(
                (static_cast<std::int32_t>(value) * 16) / 10);
        std::cout << "[DIFF] frontend angles=2051/191/0 error2="
                  << rotation_error(recovered_frontend, original_frontend) << '\n';

        const auto original_scaled_root = nba97::decode_zdomf_runtime_matrix(
            ram, 0x800ee6e0u);
        std::uint64_t best_scaled_error = std::numeric_limits<std::uint64_t>::max();
        std::uint16_t best_angle = 0;
        nba97::ZdomfTransform best_scaled_root{};
        for (std::uint16_t angle = 0; angle < 4096; ++angle) {
            const auto yaw = nba97::make_zdomf_rotation(
                trig, {0, static_cast<std::int16_t>(angle), 0});
            const auto scaled = nba97::scale_zdomf_gte_rotation(
                yaw, std::int32_t(63) * 0x270);
            const auto error = rotation_error(scaled, original_scaled_root);
            if (error < best_scaled_error) {
                best_scaled_error = error;
                best_angle = angle;
                best_scaled_root = scaled;
            }
        }
        const auto original_root = nba97::decode_zdomf_runtime_matrix(
            ram, records[0].primary_parent_pointer);
        const auto recovered_root = nba97::compose_zdomf_gte_columns(
            recovered_frontend, best_scaled_root).matrix;
        std::cout << "[DIFF] scaled root context-word=808 expected-shift=3232"
                  << " recovered-angle=" << best_angle
                  << " height=63 error2=" << best_scaled_error << '\n';
        std::cout << "[DIFF] composed root error2="
                  << rotation_error(recovered_root, original_root) << '\n';
        const auto root = original_root;
        std::uint64_t best_total = std::numeric_limits<std::uint64_t>::max();
        std::size_t best_tick = 0;
        std::array<std::uint64_t, 20> best_part_errors{};
        for (std::size_t tick = 0; tick < mocap.clips[1].logical_ticks; ++tick) {
            const auto pose = nba97::sample_zdomf_mocap(mocap, 1, tick);
            std::uint64_t total = 0;
            std::array<std::uint64_t, 20> errors{};
            for (std::size_t part = 0; part < records.size(); ++part) {
                auto local = nba97::make_zdomf_rotation(trig, pose.joints[part].angles);
                const auto composed = nba97::compose_zdomf_gte_rows(root, local).matrix;
                errors[part] = rotation_error(composed, records[part].current_matrix);
                if (part != 11) total += errors[part]; // part 11 has a live editor offset
            }
            if (total < best_total) {
                best_total = total;
                best_tick = tick;
                best_part_errors = errors;
            }
        }
        std::cout << "[DIFF] closest native mocap tick=" << best_tick
                  << " root-word="
                  << nba97::sample_zdomf_mocap(mocap, 1, best_tick).root_word
                  << " aggregate-error2=" << best_total << '\n';
        for (std::size_t part = 0; part < records.size(); ++part)
            std::cout << "[DIFF] matrix part=" << part
                      << " error2=" << best_part_errors[part] << '\n';

        nba97::ZdomfRuntimeConfig captured_config{};
        captured_config.height_value = 63;
        captured_config.root_position = {256, 0, 640};
        captured_config.root_yaw = 808;
        captured_config.apply_frontend_view = true;
        captured_config.frontend_angles = {2051, 191, 0};
        const auto native_runtime = nba97::build_zdomf_runtime_pose(
            preprocessed_pivots, trig,
            nba97::sample_zdomf_mocap(mocap, 1, best_tick), captured_config);
        const auto original_root_translation = original_root.translation;
        std::uint64_t endpoint_error = 0;
        for (std::size_t part = 0; part < records.size(); ++part) {
            const auto& native = native_runtime.record_part_endpoints[part];
            const std::array<std::int32_t, 3> aligned{{
                native.x - native_runtime.record_root_translation.x +
                    original_root_translation[0],
                native.y - native_runtime.record_root_translation.y +
                    original_root_translation[1],
                native.z - native_runtime.record_root_translation.z +
                    original_root_translation[2],
            }};
            const auto& original = records[part].primary_output.translation;
            std::uint64_t part_error = 0;
            for (std::size_t axis = 0; axis < 3; ++axis) {
                const auto delta = std::int64_t(aligned[axis]) - original[axis];
                part_error += static_cast<std::uint64_t>(delta * delta);
            }
            endpoint_error += part_error;
            std::cout << "[DIFF] endpoint part=" << part
                      << " native-aligned=" << aligned[0] << '/' << aligned[1]
                      << '/' << aligned[2] << " original=" << original[0] << '/'
                      << original[1] << '/' << original[2]
                      << " error2=" << part_error << '\n';
        }
        std::cout << "[DIFF] endpoint aggregate-error2=" << endpoint_error << '\n';
        std::cout << "ZDOMF RUNTIME DIFF: PASS - record graph, vertices, matrices, and exact-contract frame captured\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ZDOMF RUNTIME DIFF: FAIL - " << error.what() << '\n';
        return 1;
    }
}
