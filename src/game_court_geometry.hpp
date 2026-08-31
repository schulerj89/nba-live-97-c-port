#pragma once
#include "recovered/game_court_packets.h"
#include <array>
#include <cstdint>

namespace nba97 {
// Explicit camera/control inputs for the court's sf=1,lm=0 projection path.
// No Create Player camera, viewport shift, aspect correction or invented reset.
struct GameCourtCamera {
    std::array<std::int16_t,9> rotation{};
    std::array<std::int32_t,3> translation{};
    std::int32_t offset_x=0,offset_y=0; // original signed16.16 OFX/OFY
    std::uint16_t distance=0;
    std::int16_t depth_cue_a=0,average_scale4=0;
    std::int32_t depth_cue_b=0;
    bool known=false; // all above must be supplied from a proven owner
};

// Native geometry arithmetic used by the recovered court packet builder.
// Only the named vector/projection/clip/depth operations are implemented;
// this is not a CPU emulator or an opcode interpreter. Retains the touched
// geometry values so subsequent court/player passes can share their state.
// Unknown FIFO entries remain unknown until a real projection replaces them.
struct GameCourtGeometry {
    GameCourtCamera camera;
    std::array<Nba97CourtValue,6> vertex{};
    std::array<Nba97CourtValue,3> screen{};
    std::array<Nba97CourtValue,4> depth{},mac{},ir{};
    Nba97CourtValue order_depth{},leading_bits{},flags{};

    int apply(const Nba97CourtMathRequest&,Nba97CourtValue& result);
    static int callback(void*,const Nba97CourtMathRequest*,Nba97CourtValue*);
};
}
