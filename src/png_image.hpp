#pragma once

#include "psh_image.hpp"

#include <filesystem>

PshImage load_png_image(const std::filesystem::path& path);
