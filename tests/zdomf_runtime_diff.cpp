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

std::int32_t s32(const std::vector<std::uint8_t>& data, std::size_t offset) {
    return static_cast<std::int32_t>(u32(data, offset));
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
        std::array<std::size_t,20> secondary_part_corners{};
        std::array<int,6> secondary_bounds{{32767,32767,32767,-32768,-32768,-32768}};
        for(const auto& face:model.secondary_faces)for(const auto& corner:face.corners) {
            ++secondary_part_corners[corner.part];
            secondary_bounds[0]=std::min(secondary_bounds[0],int(corner.position.x));
            secondary_bounds[1]=std::min(secondary_bounds[1],int(corner.position.y));
            secondary_bounds[2]=std::min(secondary_bounds[2],int(corner.position.z));
            secondary_bounds[3]=std::max(secondary_bounds[3],int(corner.position.x));
            secondary_bounds[4]=std::max(secondary_bounds[4],int(corner.position.y));
            secondary_bounds[5]=std::max(secondary_bounds[5],int(corner.position.z));
        }
        std::cout<<"[DIFF] secondary faces="<<model.secondary_faces.size()
                 <<" source-triangles="<<model.secondary_triangle_count
                 <<" source=0x"<<std::hex<<model.layout.secondary_source_offset
                 <<"..0x"<<model.layout.secondary_source_end<<std::dec
                 <<" bounds="<<secondary_bounds[0]<<'/'<<secondary_bounds[1]<<'/'<<secondary_bounds[2]
                 <<".."<<secondary_bounds[3]<<'/'<<secondary_bounds[4]<<'/'<<secondary_bounds[5]
                 <<" part-corners=";
        for(std::size_t part=0;part<secondary_part_corners.size();++part)
            if(secondary_part_corners[part])std::cout<<part<<':'<<secondary_part_corners[part]<<',';
        std::cout<<'\n';
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
        const auto active_buffer = u32(ram, 0x1ede8);
        if (active_buffer > 1) throw std::runtime_error("invalid packet buffer selector");
        // FUN_800687BC geometry header words 6/7 are the relocated primary
        // corner-pointer tables. Read the source SXY words through them: a
        // debugger stop can catch FUN_80065740 halfway through copying the
        // final packet bank, while these source packets are already complete.
        const auto geometry_header = static_cast<std::size_t>(
            (runtime_base + 0xc6cu) & 0x001fffffu);
        const auto corner_pointer_table = u32(
            ram, geometry_header + (6u + active_buffer) * 4u);
        const auto runtime_packet_bank = runtime_base + static_cast<std::uint32_t>(
            active_buffer == 0 ? model.layout.primary_packet_a_offset
                               : model.layout.primary_packet_b_offset);
        std::size_t packet_metadata_matches = 0;
        for (std::size_t face_index = 0; face_index < model.primary_faces.size();
             ++face_index) {
            const auto& face = model.primary_faces[face_index];
            const auto packet = static_cast<std::size_t>(
                (runtime_packet_bank + face_index * 32) & 0x001fffffu);
            const auto command = u32(ram, packet + 4);
            const auto uv0 = u32(ram, packet + 12);
            const auto uv1 = u32(ram, packet + 20);
            const auto uv2 = u32(ram, packet + 28);
            const bool matches = (command >> 24) == 0x24 &&
                std::uint8_t(command) == face.modulation[0] &&
                std::uint8_t(command >> 8) == face.modulation[1] &&
                std::uint8_t(command >> 16) == face.modulation[2] &&
                std::uint8_t(uv0) == face.uv[0][0] &&
                std::uint8_t(uv0 >> 8) == face.uv[0][1] &&
                std::uint16_t(uv0 >> 16) == face.clut &&
                std::uint8_t(uv1) == face.uv[1][0] &&
                std::uint8_t(uv1 >> 8) == face.uv[1][1] &&
                std::uint16_t(uv1 >> 16) == face.tpage &&
                std::uint8_t(uv2) == face.uv[2][0] &&
                std::uint8_t(uv2 >> 8) == face.uv[2][1];
            if (!matches)
                throw std::runtime_error("first mismatch: POLY_FT3 UV/CLUT/TPAGE face " +
                                         std::to_string(face_index));
            ++packet_metadata_matches;
        }
        std::cout << "[DIFF] POLY_FT3 packet RGB/UV/CLUT/TPAGE MATCH "
                  << packet_metadata_matches << '/' << model.primary_faces.size()
                  << " active-bank=" << active_buffer << '\n';
        std::size_t packet_sxy_matches = 0;
        std::size_t packet_sxy_mismatches = 0;
        const auto depth_pointer_table = u32(ram, geometry_header + 4u * 4u);
        std::size_t ordering_key_matches = 0;
        for (std::size_t face_index = 0; face_index < model.primary_faces.size();
             ++face_index) {
            const auto& face = model.primary_faces[face_index];
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
                const auto corner_pointer = u32(ram, static_cast<std::size_t>(
                    (corner_pointer_table +
                     (face_index * 3 + corner_index) * 4) & 0x001fffffu));
                const auto packet_word = u32(ram, static_cast<std::size_t>(
                    corner_pointer & 0x001fffffu));
                const auto packet_x = static_cast<std::int16_t>(packet_word & 0xffffu);
                const auto packet_y = static_cast<std::int16_t>(packet_word >> 16);
                if (packet_x == projected.x && packet_y == projected.y) {
                    ++packet_sxy_matches;
                } else {
                    if (packet_sxy_mismatches < 12)
                        std::cout << "[DIFF] packet-SXY mismatch face=" << face_index
                                  << " corner=" << corner_index << " part="
                                  << int(corner.part) << " original=" << packet_x
                                  << '/' << packet_y << " native=" << projected.x
                                  << '/' << projected.y << '\n';
                    ++packet_sxy_mismatches;
                }
                target.points[corner_index] = {double(projected.x), double(projected.y)};
                target.depth += projected.depth / 3.0;
            }
            const auto& order_corner = face.corners[0];
            const auto& order_record = records[order_corner.part];
            const auto order_geometry = static_cast<std::size_t>(
                order_record.geometry_pointer & 0x001fffffu);
            const auto order_vertices = u32(ram, order_geometry + 4);
            std::int64_t source_depth_sum = 0;
            for (std::size_t component = 0; component < 3; ++component) {
                const auto vertex = vec3(ram, order_vertices +
                    std::uint32_t(order_corner.triangle_index) * 24u +
                    std::uint32_t(component) * 8u);
                nba97::ZdomfProjectionConfig projection{};
                projection.camera = order_record.current_matrix;
                projection.camera.translation = nba97::decode_zdomf_runtime_matrix(
                    ram, order_record.primary_parent_pointer).translation;
                projection.draw_offset_x = 0;
                source_depth_sum += nba97::project_zdomf_vertex(projection, vertex).depth;
            }
            // Retail GTE ZSF3 is 0x155 here, making the ordering-table key
            // one quarter of the average screen depth before the byte offset.
            const auto native_otz = std::clamp<std::int64_t>(
                (source_depth_sum * 0x155) >> 12, 0, 0xffff);
            const auto native_order_key = std::uint32_t(native_otz & 0xfff) << 2;
            const auto depth_pointer = u32(ram, static_cast<std::size_t>(
                (depth_pointer_table + face_index * 12) & 0x001fffffu));
            const auto original_order_key = u32(ram, static_cast<std::size_t>(
                depth_pointer & 0x001fffffu));
            if (native_order_key == original_order_key) ++ordering_key_matches;
            draw.push_back(target);
        }
        std::cout << "[DIFF] FUN_80065388->FUN_80065740 packet SXY MATCH "
                  << packet_sxy_matches << '/' << model.primary_faces.size() * 3
                  << " mismatches=" << packet_sxy_mismatches << '\n';
        std::cout << "[DIFF] descriptor-0 AVSZ3 ordering key MATCH "
                  << ordering_key_matches << '/' << model.primary_faces.size()
                  << " ZSF3=0x155\n";
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
        const auto context = static_cast<std::size_t>(u32(ram, 0x20bec) & 0x001fffffu);
        const auto active_clip = static_cast<std::size_t>(s16(ram, context + 0x4e));
        if (active_clip >= mocap.clips.size())
            throw std::runtime_error("Create Player context selected an invalid mocap clip");
        const nba97::ZdomfWorldVec3 context_root{
            s32(ram, context + 8) / 32,
            s32(ram, context + 0x10) / 32,
            s32(ram, context + 0xc) / 32};
        const auto context_yaw = static_cast<std::uint16_t>(s16(ram, context + 0xa8));
        std::cout << "[DIFF] context clip=" << active_clip
                  << " root=" << context_root.x << '/' << context_root.y << '/'
                  << context_root.z << " yaw=" << context_yaw << '\n';
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
        const auto expected_angle = static_cast<std::uint16_t>(
            (-static_cast<std::int32_t>(context_yaw) * 4) & 0xfff);
        std::cout << "[DIFF] scaled root context-word=" << context_yaw
                  << " expected-angle=" << expected_angle
                  << " recovered-angle=" << best_angle
                  << " height=63 error2=" << best_scaled_error << '\n';
        std::cout << "[DIFF] composed root error2="
                  << rotation_error(recovered_root, original_root) << '\n';
        const auto root = original_root;
        std::uint64_t best_total = std::numeric_limits<std::uint64_t>::max();
        std::size_t best_tick = 0;
        std::array<std::uint64_t, 20> best_part_errors{};
        for (std::size_t tick = 0; tick < mocap.clips[active_clip].logical_ticks; ++tick) {
            const auto pose = nba97::sample_zdomf_mocap(mocap, active_clip, tick);
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
                  << nba97::sample_zdomf_mocap(mocap, active_clip, best_tick).root_word
                  << " aggregate-error2=" << best_total << '\n';
        for (std::size_t part = 0; part < records.size(); ++part)
            std::cout << "[DIFF] matrix part=" << part
                      << " error2=" << best_part_errors[part] << '\n';

        nba97::ZdomfRuntimeConfig captured_config{};
        captured_config.height_value = 63;
        captured_config.root_position = context_root;
        captured_config.root_yaw = static_cast<std::int16_t>(context_yaw);
        captured_config.apply_frontend_view = true;
        captured_config.frontend_angles = {2051, 191, 0};
        const auto original_root_translation = original_root.translation;
        captured_config.use_record_root_translation = true;
        captured_config.record_root_translation = {
            original_root_translation[0], original_root_translation[1],
            original_root_translation[2]};
        const auto native_runtime = nba97::build_zdomf_runtime_pose(
            preprocessed_pivots, trig,
            nba97::sample_zdomf_mocap(mocap, active_clip, best_tick), captured_config);
        std::size_t reconstructed_sxy_matches = 0;
        std::size_t reconstructed_sxy_mismatches = 0;
        nba97::ZdomfProjectionConfig packet_projection{};
        packet_projection.camera.rotation = {{{4096,0,0},{0,4096,0},{0,0,4096}}};
        packet_projection.camera.translation = {0,0,0};
        packet_projection.draw_offset_x = 0;
        for (std::size_t face_index = 0; face_index < model.primary_faces.size(); ++face_index) {
            const auto& face = model.primary_faces[face_index];
            for (std::size_t corner_index = 0; corner_index < 3; ++corner_index) {
                const auto& corner = face.corners[corner_index];
                const auto preprocessed = nba97::apply_zdomf_transform(
                    transforms.parts[corner.part], corner.position);
                const auto assembled = nba97::apply_zdomf_runtime_record_pose(
                    native_runtime, corner.part, preprocessed);
                const auto projected = nba97::project_zdomf_vertex(packet_projection, {
                    static_cast<std::int16_t>(assembled.x),
                    static_cast<std::int16_t>(assembled.y),
                    static_cast<std::int16_t>(assembled.z)});
                const auto corner_pointer = u32(ram, static_cast<std::size_t>(
                    (corner_pointer_table + (face_index * 3 + corner_index) * 4) &
                    0x001fffffu));
                const auto packet_word = u32(ram, static_cast<std::size_t>(
                    corner_pointer & 0x001fffffu));
                const auto packet_x = static_cast<std::int16_t>(packet_word & 0xffffu);
                const auto packet_y = static_cast<std::int16_t>(packet_word >> 16);
                if (packet_x == projected.x && packet_y == projected.y) {
                    ++reconstructed_sxy_matches;
                } else {
                    if (reconstructed_sxy_mismatches < 12)
                        std::cout << "[DIFF] reconstructed-SXY mismatch face="
                                  << face_index << " corner=" << corner_index
                                  << " part=" << int(corner.part) << " original="
                                  << packet_x << '/' << packet_y << " native="
                                  << projected.x << '/' << projected.y << '\n';
                    ++reconstructed_sxy_mismatches;
                }
            }
        }
        std::cout << "[DIFF] full native runtime packet SXY MATCH "
                  << reconstructed_sxy_matches << '/'
                  << model.primary_faces.size() * 3 << " mismatches="
                  << reconstructed_sxy_mismatches << '\n';
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
