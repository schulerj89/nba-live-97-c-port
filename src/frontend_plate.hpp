#pragma once

#include "psh_image.hpp"

namespace nba97 {

// Fixed FE plate shapes from8009370C/80093714;34A5C adds the same offsets
// to XY and UV. Coverage retains the native two-triangle pixel-center rule.
// The caller must draw foreground later, unscaled, at frame_x/frame_y.
// An unavailable source texel is omitted only when that actual foreground
// texel is fully opaque. Refusal leaves destination unchanged.
void drawFrontendPlate(PshImage& destination, const PshImage& source,
                       int x, int y, int side,
                       const PshImage& foreground, int frame_x, int frame_y);

} // namespace nba97
