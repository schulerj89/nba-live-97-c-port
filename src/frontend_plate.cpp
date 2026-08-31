#include "frontend_plate.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace nba97 {
namespace {
void validateImage(const PshImage& image, const char* message) {
    const auto bytes = std::uint64_t(image.width) * image.height * 4;
    if (!image.width || !image.height || bytes != image.rgba.size())
        throw std::runtime_error(message);
}

template<class Visit>
void visitPlate(int side, Visit visit) {
    static constexpr int quads[2][8] = {
        {0,0,86,10,0,58,106,60}, {20,10,106,0,0,60,106,58}};
    static constexpr int triangles[2][3] = {{0,1,2}, {1,3,2}};
    const auto& q = quads[side];
    for (const auto& triangle : triangles) {
        const int a = triangle[0], b = triangle[1], c = triangle[2];
        const double denominator = (q[2*b+1]-q[2*c+1])*(q[2*a]-q[2*c]) +
                                   (q[2*c]-q[2*b])*(q[2*a+1]-q[2*c+1]);
        for (int yy = 0; yy < 60; ++yy) for (int xx = 0; xx < 106; ++xx) {
            const double u = ((q[2*b+1]-q[2*c+1])*(xx+0.5-q[2*c]) +
                              (q[2*c]-q[2*b])*(yy+0.5-q[2*c+1]))/denominator;
            const double v = ((q[2*c+1]-q[2*a+1])*(xx+0.5-q[2*c]) +
                              (q[2*a]-q[2*c])*(yy+0.5-q[2*c+1]))/denominator;
            const double w = 1-u-v;
            if (u >= 0 && v >= 0 && w >= 0) visit(xx, yy);
        }
    }
}
} // namespace

void drawFrontendPlate(PshImage& destination, const PshImage& source,
                       int x, int y, int side,
                       const PshImage& foreground, int frame_x, int frame_y) {
    if (side < 0 || side > 1)
        throw std::runtime_error("invalid frontend plate side");
    if (&destination == &source || &destination == &foreground)
        throw std::runtime_error("frontend plate destination aliases an input");
    validateImage(destination, "invalid frontend plate destination extent");
    validateImage(source, "invalid frontend plate source extent");
    validateImage(foreground, "invalid frontend plate foreground extent");

    const auto visible = [&](int xx, int yy) {
        const auto dx = std::int64_t(x) + xx, dy = std::int64_t(y) + yy;
        return dx >= 0 && dx < destination.width && dy >= 0 && dy < destination.height;
    };
    // Preflight every unavailable visible texel before writing destination.
    // This is foreground occlusion, not transparent or edge-clamped padding.
    visitPlate(side, [&](int xx, int yy) {
        if (!visible(xx, yy) || (xx < source.width && yy < source.height)) return;
        const auto fx = std::int64_t(x) + xx - frame_x;
        const auto fy = std::int64_t(y) + yy - frame_y;
        if (fx < 0 || fx >= foreground.width || fy < 0 || fy >= foreground.height ||
            foreground.rgba[(std::size_t(fy)*foreground.width+std::size_t(fx))*4+3] != 255)
            throw std::runtime_error("frontend plate exposes an unavailable source texel");
    });

    visitPlate(side, [&](int xx, int yy) {
        if (!visible(xx, yy) || xx >= source.width || yy >= source.height) return;
        // Equal XY/UV offsets make this an identity sample in plate-local
        // coordinates. Image dimensions must never scale the fixed shape.
        const auto from = (std::size_t(yy)*source.width+std::size_t(xx))*4;
        if (!source.rgba[from+3]) return;
        const auto dx = std::int64_t(x) + xx, dy = std::int64_t(y) + yy;
        const auto to = (std::size_t(dy)*destination.width+std::size_t(dx))*4;
        for (unsigned channel = 0; channel < 3; ++channel)
            destination.rgba[to+channel] = source.rgba[from+channel];
        destination.rgba[to+3] = 255;
    });
}
} // namespace nba97
