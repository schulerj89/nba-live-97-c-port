#pragma once

#include "psh_image.hpp"
#include "recovered/create_player.h"
#include "zdomf_model.hpp"
#include "zdomf_projection.hpp"
#include "zdomf_transform.hpp"

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
    ZdomfModel model_{};
    ZdomfTransformSet base_transforms_{};
    ZdomfProjectionConfig projection_{};
    std::array<std::int32_t, 6> projection_bounds_{};
    std::size_t projection_saturated_vertices_=0;
    std::vector<ZdomfVec3> motion_samples_;
    std::vector<PshImage> team_jerseys_;
    std::vector<PshImage> team_shorts_;
    std::vector<PshImage> team_shorts_alt_;
    std::size_t team_family_count_=0;
};

} // namespace nba97
