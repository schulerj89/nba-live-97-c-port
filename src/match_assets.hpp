#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
namespace nba97 {
std::array<uint8_t,59> loadMatchControlDefaults(const std::filesystem::path&);
}
