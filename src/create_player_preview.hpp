#pragma once

#include "psh_image.hpp"
#include "ps1_vram_texture.hpp"
#include "recovered/create_player.h"
#include "zdomf_model.hpp"
#include "zdomf_hierarchy.hpp"
#include "zdomf_mocap.hpp"
#include "zdomf_projection.hpp"
#include "zdomf_runtime.hpp"
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
    std::vector<ZdomfModel> team_models_;
    ZdomfTransformSet base_transforms_{};
    ZdomfHierarchy hierarchy_{};
    ZdomfMocap mocap_{};
    ZdomfProjectionConfig projection_{};
    std::vector<std::uint8_t> packed_trig_;
    std::array<std::int32_t, 6> projection_flat_bounds_{};
    std::array<std::int32_t, 6> projection_bounds_{};
    std::size_t projection_saturated_vertices_=0;
    // FUN_80067A14 uploads five team SHPP records into fixed PS1 VRAM
    // rectangles.  Keep the decoded sources separate so packet TPAGE/UV can
    // address the same logical atlas without retaining copyrighted assets in
    // the repository.
    std::vector<Ps1VramTextureAtlas> team_texture_uploads_;
    std::vector<std::vector<Ps1VramTextureAtlas>> jersey_texture_uploads_;
    std::array<std::vector<std::uint8_t>, 26> name_letter_indices_{};
    std::array<std::uint8_t, 26> name_letter_widths_{};
    std::size_t team_family_count_=0;
    mutable bool texture_audit_logged_=false;
};

} // namespace nba97
