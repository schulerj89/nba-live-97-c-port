#include "zdomf_runtime_records.hpp"

#include <limits>
#include <stdexcept>

namespace nba97 {
namespace {

std::size_t physical(std::uint32_t address) {
    return static_cast<std::size_t>(address & 0x001fffffu);
}

void require(const std::vector<std::uint8_t>& ram, std::size_t offset,
             std::size_t length) {
    if (offset > ram.size() || length > ram.size() - offset)
        throw std::runtime_error("ZDOMF runtime snapshot address is outside PS1 RAM");
}

std::uint16_t u16(const std::vector<std::uint8_t>& ram, std::size_t offset) {
    require(ram, offset, 2);
    return static_cast<std::uint16_t>(ram[offset] | (ram[offset + 1] << 8));
}

std::int16_t s16(const std::vector<std::uint8_t>& ram, std::size_t offset) {
    return static_cast<std::int16_t>(u16(ram, offset));
}

std::uint32_t u32(const std::vector<std::uint8_t>& ram, std::size_t offset) {
    require(ram, offset, 4);
    return std::uint32_t(ram[offset]) |
           (std::uint32_t(ram[offset + 1]) << 8) |
           (std::uint32_t(ram[offset + 2]) << 16) |
           (std::uint32_t(ram[offset + 3]) << 24);
}

std::int32_t s32(const std::vector<std::uint8_t>& ram, std::size_t offset) {
    return static_cast<std::int32_t>(u32(ram, offset));
}

std::int8_t resolve_parent(std::uint32_t pointer, std::uint32_t runtime_base,
                           std::uint32_t output_offset) {
    for (std::size_t part = 0; part < kZdomfRuntimeRecordCount; ++part) {
        const auto candidate = runtime_base +
            static_cast<std::uint32_t>(part) * kZdomfRuntimeRecordStride +
            output_offset;
        if (pointer == candidate) return static_cast<std::int8_t>(part);
    }
    // The three roots point to shared matrices outside the runtime block.
    return -1;
}

} // namespace

ZdomfTransform decode_zdomf_runtime_matrix(
    const std::vector<std::uint8_t>& ram, std::uint32_t address) {
    const auto offset = physical(address);
    require(ram, offset, 32);
    ZdomfTransform result{};
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t column = 0; column < 3; ++column)
            result.rotation[row][column] =
                s16(ram, offset + (row * 3 + column) * 2);
    result.translation = {{s32(ram, offset + 20), s32(ram, offset + 24),
                           s32(ram, offset + 28)}};
    return result;
}

std::array<ZdomfRuntimeRecordSnapshot, kZdomfRuntimeRecordCount>
decode_zdomf_runtime_records(const std::vector<std::uint8_t>& ram,
                             std::uint32_t runtime_base) {
    std::array<ZdomfRuntimeRecordSnapshot, kZdomfRuntimeRecordCount> records{};
    const auto base = physical(runtime_base);
    require(ram, base, kZdomfRuntimeRecordCount * kZdomfRuntimeRecordStride + 180);
    for (std::size_t part = 0; part < records.size(); ++part) {
        const auto relative = static_cast<std::uint32_t>(part) *
            kZdomfRuntimeRecordStride;
        const auto offset = base + relative;
        auto& record = records[part];
        record.current_matrix = decode_zdomf_runtime_matrix(
            ram, runtime_base + relative + 36);
        record.primary_output = decode_zdomf_runtime_matrix(
            ram, runtime_base + relative + 100);
        record.alternate_output = decode_zdomf_runtime_matrix(
            ram, runtime_base + relative + 132);
        record.primary_parent_pointer = u32(ram, offset + 164);
        record.alternate_parent_pointer = u32(ram, offset + 168);
        record.pivot_pointer = u32(ram, offset + 172);
        record.geometry_pointer = u32(ram, offset + 176);
        record.primary_parent = resolve_parent(record.primary_parent_pointer,
                                               runtime_base, 100);
        record.alternate_parent = resolve_parent(record.alternate_parent_pointer,
                                                 runtime_base, 132);
    }
    return records;
}

} // namespace nba97
