#pragma once

#include "zdomf_transform.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace nba97 {

constexpr std::size_t kZdomfRuntimeRecordCount = 20;
constexpr std::uint32_t kZdomfRuntimeRecordStride = 148;

struct ZdomfRuntimeRecordSnapshot {
    ZdomfTransform current_matrix{};
    ZdomfTransform primary_output{};
    ZdomfTransform alternate_output{};
    std::uint32_t primary_parent_pointer = 0;
    std::uint32_t alternate_parent_pointer = 0;
    std::uint32_t pivot_pointer = 0;
    std::uint32_t geometry_pointer = 0;
    std::int8_t primary_parent = -2;
    std::int8_t alternate_parent = -2;
};

// Decodes the exact 148-byte record graph constructed by FEONLY
// FUN_80069098. The input is a private 2 MiB PS1 main-RAM snapshot; no retail
// bytes are retained in the returned typed values beyond the requested fields.
std::array<ZdomfRuntimeRecordSnapshot, kZdomfRuntimeRecordCount>
decode_zdomf_runtime_records(const std::vector<std::uint8_t>& ram,
                             std::uint32_t runtime_base);

ZdomfTransform decode_zdomf_runtime_matrix(
    const std::vector<std::uint8_t>& ram, std::uint32_t address);

} // namespace nba97
