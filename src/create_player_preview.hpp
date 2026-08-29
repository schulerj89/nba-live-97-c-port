#pragma once

#include "psh_image.hpp"
#include "recovered/create_player.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nba97 {

class CreatePlayerPreview final {
public:
    explicit CreatePlayerPreview(const std::filesystem::path& asset_root);
    void draw(PshImage& image, const Nba97CreateEditor& editor,
              std::uint32_t elapsed_ms) const;
    [[nodiscard]] std::string description() const;

private:
    struct Vec3 { std::int16_t x=0,y=0,z=0; };
    std::array<Vec3,20> bones_{};
    std::vector<Vec3> motion_samples_;
    std::vector<PshImage> team_textures_;
    std::uint32_t vertex_count_=0, face_count_=0;
    std::size_t team_family_count_=0;
};

} // namespace nba97
