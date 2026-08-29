#include "zdomf_transform.hpp"

#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

namespace nba97 {
namespace {

constexpr std::size_t kPartCount = 20;
constexpr std::size_t kAngleRecordSize = 8;
constexpr std::size_t kSetSize = kPartCount * kAngleRecordSize;
constexpr std::size_t kTrigEntries = 4096;

std::uint16_t read_u16(const std::vector<std::uint8_t>& data, std::size_t at,
                       const char* what) {
    if (at > data.size() || 2 > data.size() - at) {
        throw std::runtime_error(std::string("truncated ") + what);
    }
    return static_cast<std::uint16_t>(data[at] | (data[at + 1] << 8));
}

std::int16_t read_s16(const std::vector<std::uint8_t>& data, std::size_t at,
                      const char* what) {
    return static_cast<std::int16_t>(read_u16(data, at, what));
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& data, std::size_t at,
                       const char* what) {
    if (at > data.size() || 4 > data.size() - at) {
        throw std::runtime_error(std::string("truncated ") + what);
    }
    return std::uint32_t(data[at]) | (std::uint32_t(data[at + 1]) << 8) |
           (std::uint32_t(data[at + 2]) << 16) |
           (std::uint32_t(data[at + 3]) << 24);
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("missing private transform asset: " + path.string());
    return {std::istreambuf_iterator<char>(input), {}};
}

std::int32_t shift12(std::int64_t value) {
    // All retail matrix products are far inside the signed range. Division is
    // written explicitly as a floor for negative values to match MIPS SRA.
    if (value >= 0) return static_cast<std::int32_t>(value / 4096);
    return static_cast<std::int32_t>(-(((-value) + 4095) / 4096));
}

struct SinCos {
    std::int32_t sine = 0;
    std::int32_t cosine = 4096;
};

SinCos lookup(const std::vector<std::uint8_t>& trig, std::int16_t angle) {
    const auto signed_angle = static_cast<std::int32_t>(angle);
    const auto magnitude = static_cast<std::uint32_t>(
        signed_angle < 0 ? -signed_angle : signed_angle) & 0x0FFF;
    const auto packed = read_u32(trig, static_cast<std::size_t>(magnitude) * 4,
                                 "ZDOM packed trig table");
    auto sine = static_cast<std::int16_t>(packed & 0xFFFF);
    const auto cosine = static_cast<std::int16_t>(packed >> 16);
    if (signed_angle < 0) sine = static_cast<std::int16_t>(-sine);
    return {sine, cosine};
}

ZdomfTransform make_rotation_impl(const std::vector<std::uint8_t>& trig,
                                  const ZdomfEulerAngles& angles) {
    // Direct native transcription of FUN_80067100. Intermediate results are
    // shifted at the same points as the original MIPS multiplication chain.
    const auto x = lookup(trig, angles.x);
    const auto y = lookup(trig, angles.y);
    const auto z = lookup(trig, angles.z);
    ZdomfTransform out{};
    out.rotation[2][0] = static_cast<std::int16_t>(-y.sine);
    out.rotation[2][1] = static_cast<std::int16_t>(shift12(
        std::int64_t(x.sine) * y.cosine));
    out.rotation[2][2] = static_cast<std::int16_t>(shift12(
        std::int64_t(x.cosine) * y.cosine));
    out.rotation[0][0] = static_cast<std::int16_t>(shift12(
        std::int64_t(y.cosine) * z.cosine));
    out.rotation[1][0] = static_cast<std::int16_t>(shift12(
        std::int64_t(z.sine) * y.cosine));

    const auto sxsy = shift12(std::int64_t(x.sine) * y.sine);
    const auto cxsy = shift12(std::int64_t(x.cosine) * y.sine);
    out.rotation[0][1] = static_cast<std::int16_t>(
        shift12(std::int64_t(sxsy) * z.cosine) -
        shift12(std::int64_t(z.sine) * x.cosine));
    out.rotation[1][1] = static_cast<std::int16_t>(
        shift12(std::int64_t(sxsy) * z.sine) +
        shift12(std::int64_t(x.cosine) * z.cosine));
    out.rotation[0][2] = static_cast<std::int16_t>(
        shift12(std::int64_t(cxsy) * z.cosine) +
        shift12(std::int64_t(x.sine) * z.sine));
    out.rotation[1][2] = static_cast<std::int16_t>(
        shift12(std::int64_t(cxsy) * z.sine) -
        shift12(std::int64_t(x.sine) * z.cosine));
    return out;
}

} // namespace

ZdomfTransform make_zdomf_rotation(
    const std::vector<std::uint8_t>& packed_trig,
    const ZdomfEulerAngles& angles) {
    if (packed_trig.size() != kTrigEntries * 4 ||
        read_u32(packed_trig, 0, "ZDOM packed trig table") != 0x10000000) {
        throw std::runtime_error("invalid ZDOM packed trig table");
    }
    return make_rotation_impl(packed_trig, angles);
}

ZdomfTransformSet decode_zdomf_base_transforms(
    const std::vector<std::uint8_t>& deflist,
    const std::vector<std::uint8_t>& packed_trig,
    std::size_t set_index) {
    if (packed_trig.size() != kTrigEntries * 4 ||
        read_u32(packed_trig, 0, "ZDOM packed trig table") != 0x10000000) {
        throw std::runtime_error("invalid ZDOM packed trig table");
    }
    const auto available_sets = deflist.size() / kSetSize;
    if (available_sets == 0 || set_index >= available_sets) {
        throw std::runtime_error("invalid ZDEFLIST transform set");
    }
    ZdomfTransformSet set{};
    set.available_sets = available_sets;
    const auto base = set_index * kSetSize;
    for (std::size_t part = 0; part < kPartCount; ++part) {
        const auto at = base + part * kAngleRecordSize;
        set.angles[part] = {read_s16(deflist, at, "ZDEFLIST angle"),
                            read_s16(deflist, at + 2, "ZDEFLIST angle"),
                            read_s16(deflist, at + 4, "ZDEFLIST angle")};
        set.parts[part] = make_rotation_impl(packed_trig, set.angles[part]);
    }
    return set;
}

ZdomfTransformSet load_zdomf_base_transforms(
    const std::filesystem::path& deflist_path,
    const std::filesystem::path& packed_trig_path,
    std::size_t set_index) {
    return decode_zdomf_base_transforms(read_file(deflist_path),
                                        read_file(packed_trig_path), set_index);
}

ZdomfVec3 apply_zdomf_transform(const ZdomfTransform& transform,
                                const ZdomfVec3& value) {
    std::array<std::int32_t, 3> result{};
    const std::array<std::int32_t, 3> input{{value.x, value.y, value.z}};
    for (std::size_t row = 0; row < 3; ++row) {
        std::int64_t sum = 0;
        for (std::size_t column = 0; column < 3; ++column) {
            sum += std::int64_t(transform.rotation[row][column]) * input[column];
        }
        result[row] = shift12(sum) + transform.translation[row];
    }
    return {static_cast<std::int16_t>(result[0]),
            static_cast<std::int16_t>(result[1]),
            static_cast<std::int16_t>(result[2])};
}

} // namespace nba97
