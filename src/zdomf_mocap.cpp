#include "zdomf_mocap.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace nba97 {
namespace {

std::uint16_t u16(const std::vector<std::uint8_t>& data, std::size_t at) {
    if (at + 2 > data.size()) throw std::runtime_error("truncated ZFEMOCAP u16");
    return static_cast<std::uint16_t>(data[at] | (data[at + 1] << 8));
}
std::int16_t s16(const std::vector<std::uint8_t>& data, std::size_t at) {
    return static_cast<std::int16_t>(u16(data, at));
}
std::uint32_t u32(const std::vector<std::uint8_t>& data, std::size_t at) {
    if (at + 4 > data.size()) throw std::runtime_error("truncated ZFEMOCAP u32");
    return std::uint32_t(data[at]) | (std::uint32_t(data[at + 1]) << 8) |
           (std::uint32_t(data[at + 2]) << 16) | (std::uint32_t(data[at + 3]) << 24);
}

std::int32_t wrapped_delta(std::int32_t from, std::int32_t to) {
    std::int32_t delta = to - from;
    while (delta > 0x800) delta -= 0x1000;
    while (delta < -0x800) delta += 0x1000;
    return delta;
}

std::array<std::int32_t, 3> closest_euler(const ZdomfEulerAngles& a,
                                          const ZdomfEulerAngles& b) {
    const std::array<std::int32_t, 3> av{{a.x, a.y, a.z}};
    const std::array<std::int32_t, 3> direct{{
        av[0] + wrapped_delta(av[0], b.x),
        av[1] + wrapped_delta(av[1], b.y),
        av[2] + wrapped_delta(av[2], b.z)}};
    // Equivalent XYZ Euler representation used by FUN_80065D40 when it is
    // closer across the +/-0x800 half-turn boundary.
    const std::array<std::int32_t, 3> flipped_raw{{
        std::int32_t(b.x) + 0x800,
        0x800 - std::int32_t(b.y),
        std::int32_t(b.z) + 0x800}};
    const std::array<std::int32_t, 3> flipped{{
        av[0] + wrapped_delta(av[0], flipped_raw[0]),
        av[1] + wrapped_delta(av[1], flipped_raw[1]),
        av[2] + wrapped_delta(av[2], flipped_raw[2])}};
    const auto distance = [&](const auto& candidate) {
        return std::abs(candidate[0] - av[0]) + std::abs(candidate[1] - av[1]) +
               std::abs(candidate[2] - av[2]);
    };
    return distance(flipped) < distance(direct) ? flipped : direct;
}

ZdomfMocapJoint joint(const std::vector<std::uint8_t>& data, std::size_t at,
                      std::uint16_t expected_marker) {
    ZdomfMocapJoint out{{s16(data, at), s16(data, at + 2), s16(data, at + 4)},
                         u16(data, at + 6)};
    if (out.marker != expected_marker) {
        throw std::runtime_error("ZFEMOCAP joint marker mismatch");
    }
    return out;
}

} // namespace

ZdomfMocap decode_zdomf_mocap(const std::vector<std::uint8_t>& data) {
    if (data.size() < 56) throw std::runtime_error("ZFEMOCAP is truncated");
    ZdomfMocap out{};
    out.body_directory_offset = u32(data, 0);
    out.secondary_directory_offset = u32(data, 4);
    if (out.body_directory_offset + 24 > data.size() ||
        out.secondary_directory_offset + 24 > data.size()) {
        throw std::runtime_error("ZFEMOCAP directory lies outside the file");
    }
    for (std::size_t clip_index = 0; clip_index < out.clips.size(); ++clip_index) {
        const auto body_at = std::size_t(u32(data, out.body_directory_offset + clip_index * 4));
        const auto secondary_at = std::size_t(u32(data, out.secondary_directory_offset + clip_index * 4));
        if (body_at + 12 > data.size() || secondary_at + 12 > data.size() ||
            u32(data, body_at + 8) != 12 || u32(data, secondary_at + 8) != 12) {
            throw std::runtime_error("ZFEMOCAP stream relocation header is invalid");
        }
        auto& clip = out.clips[clip_index];
        clip.body_flags = u16(data, body_at);
        clip.secondary_flags = u16(data, secondary_at);
        clip.timing_code = data[body_at + 3];
        const auto body_frames = std::size_t(data[body_at + 7]);
        const auto secondary_frames = std::size_t(data[secondary_at + 7]);
        if (!body_frames || body_frames != secondary_frames ||
            body_at + 12 + body_frames * 0x60 > data.size() ||
            secondary_at + 12 + secondary_frames * 0x44 > data.size()) {
            throw std::runtime_error("ZFEMOCAP paired stream frame counts are invalid");
        }
        clip.physical_frames = body_frames;
        // FUN_80035260 expands playback duration for flag 8, but leaves the
        // physical keyframe records in place.
        clip.logical_ticks = body_frames;
        if ((clip.body_flags & 0x08) && !(clip.body_flags & 0x10)) {
            clip.timing_code = static_cast<std::uint8_t>(clip.timing_code >> 1);
            clip.logical_ticks = body_frames * 2 - ((clip.body_flags & 1) ? 1 : 0);
        }
        clip.frames.resize(body_frames);
        for (std::size_t frame = 0; frame < body_frames; ++frame) {
            auto& pose = clip.frames[frame];
            const auto body_frame = body_at + 12 + frame * 0x60;
            const auto secondary_frame = secondary_at + 12 + frame * 0x44;
            pose.root_word = s16(data, secondary_frame);
            pose.root_height = s16(data, secondary_frame + 2);
            for (std::size_t part = 0; part < 8; ++part) {
                pose.joints[part] = joint(data, secondary_frame + 4 + part * 8, 0xabcd);
            }
            for (std::size_t part = 0; part < 12; ++part) {
                pose.joints[part + 8] = joint(data, body_frame + part * 8, 0xdcba);
            }
        }
    }
    return out;
}

ZdomfMocap load_zdomf_mocap(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("missing ZFEMOCAP: " + path.string());
    const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)), {});
    return decode_zdomf_mocap(data);
}

ZdomfEulerAngles blend_zdomf_euler(const ZdomfEulerAngles& a,
                                   const ZdomfEulerAngles& b,
                                   std::uint16_t weight) {
    weight = std::min<std::uint16_t>(weight, 0x100);
    const auto target = closest_euler(a, b);
    const std::array<std::int32_t, 3> start{{a.x, a.y, a.z}};
    std::array<std::int16_t, 3> result{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const auto delta = target[axis] - start[axis];
        result[axis] = static_cast<std::int16_t>(start[axis] +
            static_cast<std::int32_t>((std::int64_t(delta) * weight) >> 8));
    }
    return {result[0], result[1], result[2]};
}

ZdomfMocapPose blend_zdomf_pose(const ZdomfMocapPose& a,
                                const ZdomfMocapPose& b,
                                std::uint16_t weight) {
    weight = std::min<std::uint16_t>(weight, 0x100);
    ZdomfMocapPose out{};
    for (std::size_t part = 0; part < out.joints.size(); ++part) {
        out.joints[part].angles = blend_zdomf_euler(
            a.joints[part].angles, b.joints[part].angles, weight);
        out.joints[part].marker = a.joints[part].marker;
    }
    const auto lerp = [&](std::int16_t av, std::int16_t bv) {
        return static_cast<std::int16_t>(std::int32_t(av) +
            ((std::int32_t(bv) - std::int32_t(av)) * weight >> 8));
    };
    out.root_word = lerp(a.root_word, b.root_word);
    out.root_height = lerp(a.root_height, b.root_height);
    return out;
}

ZdomfMocapPose sample_zdomf_mocap(const ZdomfMocap& mocap,
                                  std::size_t clip_index,
                                  std::size_t logical_tick) {
    if (clip_index >= mocap.clips.size()) throw std::runtime_error("invalid mocap clip");
    const auto& clip = mocap.clips[clip_index];
    if (clip.frames.empty()) throw std::runtime_error("empty mocap clip");
    logical_tick %= std::max<std::size_t>(1, clip.logical_ticks);
    if (clip.logical_ticks == clip.physical_frames) return clip.frames[logical_tick];
    const auto frame = (logical_tick / 2) % clip.physical_frames;
    if ((logical_tick & 1) == 0) return clip.frames[frame];
    return blend_zdomf_pose(clip.frames[frame],
                            clip.frames[(frame + 1) % clip.physical_frames], 0x80);
}

ZdomfMocapPose canonicalize_zdomf_pose(const ZdomfMocapPose& pose,
                                       bool body,
                                       bool secondary) {
    ZdomfMocapPose out = pose;
    const auto converted = [](const ZdomfMocapJoint& source) {
        auto result = source;
        result.angles.x = static_cast<std::int16_t>(0x800 - source.angles.x);
        result.angles.z = static_cast<std::int16_t>(0x800 - source.angles.z);
        return result;
    };
    if (secondary) {
        constexpr std::array<std::size_t, 8> map{{4,5,6,7,0,1,2,3}};
        for (std::size_t source = 0; source < map.size(); ++source)
            out.joints[map[source]] = converted(pose.joints[source]);
    }
    if (body) {
        constexpr std::array<std::size_t, 12> map{{0,1,2,3,8,9,10,11,4,5,6,7}};
        for (std::size_t source = 0; source < map.size(); ++source)
            out.joints[8 + map[source]] = converted(pose.joints[8 + source]);
    }
    return out;
}

} // namespace nba97
