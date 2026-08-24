#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct PshImage {
    std::filesystem::path source;
    std::string tag;
    std::uint32_t format = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    bool has_crcf = false;
    std::vector<std::uint8_t> rgba;
};

PshImage load_psh(const std::filesystem::path& path);
std::string describe_psh(const PshImage& image);
