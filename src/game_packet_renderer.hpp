#pragma once
#include "game_render_backend.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace nba97 {
enum class GamePacketResult {
    Complete, Argument, UnknownState, UnknownVram, UnsupportedCommand,
    UnsupportedMode, IncompleteCommand, PixelLimit, LinkLimit,
    PacketUnavailable, LinkAlignment, TextureFeedbackUnsupported
};
struct GameDrawDisplay {
    bool known=false,enabled=false,interlaced=false;
    unsigned field=0;
    int x=0,y=0,width=0,height=0;
};
struct GameDrawState {
    std::uint32_t mode=0,window=0,top_left=0,bottom_right=0,offset=0,mask=0;
    unsigned known=0; // bits0..5 correspond to E1..E6, no invented reset state.
    GameDrawDisplay display;
};
struct GameDrawProgress {
    std::size_t words=0,commands=0,triangles=0,lines=0,rectangles=0;
    std::size_t candidates=0,pixels=0,transparent=0,masked=0,links=0;
    std::uint32_t stopped_link=0;
    int stopped_x=-1,stopped_y=-1;
    bool completed=false;
};
// Reader receives the real low24 ordering-table/packet address. It must map
// proven retained allocations; never synthesize a missing tag/terminator.
using GamePacketRead = GamePacketResult(*)(void*,std::uint32_t,std::uint32_t&);
struct GamePacketWord {
    std::uint32_t word=0;
    std::uint8_t known_mask=0; // One bit per byte; only bits0..3 are valid.
};
using GamePacketKnownRead = GamePacketResult(*)(void*,std::uint32_t,GamePacketWord&);

// Native drawing backend over the same VRAM that receives resource uploads.
// Supports flat/Gouraud triangles/quads, single lines, rectangles, E1..E6,
// fill, NOP and cache-clear. No CPU, bus/timer or device execution is embedded.
// Integer affine interpolation is implemented; exact PS1 raster edge/gradient
// parity remains a separate acceptance gate, not implied by successful drawing.
class GamePacketRenderer {
public:
    explicit GamePacketRenderer(GameVramWords& vram);
    GameDrawState state;
    std::size_t pixel_budget=4*1024*1024;
    // Commands may cross packet boundaries. Each call below starts a new
    // native batch; incomplete commands refuse instead of inventing words.
    // Prior pixels/state remain on failure; clone the entire owner to publish
    // atomically. Never call successful prefix output a completed frame.
    GamePacketResult drawWords(const std::uint32_t*,std::size_t,GameDrawProgress&);
    GamePacketResult drawOrderingTable(GamePacketRead,void*,std::uint32_t first,
                                      std::size_t link_budget,GameDrawProgress&);
    // Retained packet padding may be unknown. Validate command-consumed bytes,
    // including across packet links; never mark backing bytes known or guess
    // an opcode, tag, coordinate, texture page or consumed color/UV value.
    GamePacketResult drawKnownWords(const GamePacketWord*,std::size_t,GameDrawProgress&);
    GamePacketResult drawKnownOrderingTable(GamePacketKnownRead,void*,std::uint32_t first,
                                           std::size_t link_budget,GameDrawProgress&);
private:
    struct Vertex { int x=0,y=0,u=0,v=0;std::array<int,3> color{}; };
    GameVramWords& vram_;
    std::vector<std::uint8_t> written_;
    std::array<std::uint32_t,12> pending_{};
    unsigned used_=0,needed_=0;
    GameDrawProgress* progress_=nullptr;
    void begin(GameDrawProgress&);
    GamePacketResult feed(std::uint32_t,std::uint8_t known_mask=15);
    GamePacketResult execute();
    GamePacketResult ready() const;
    GamePacketResult sample(int,int,std::uint16_t,std::uint16_t&);
    GamePacketResult pixel(int,int,const std::array<int,3>&,int,int,std::uint16_t,bool,bool,bool,bool);
    GamePacketResult triangle(Vertex,Vertex,Vertex,std::uint16_t,bool,bool,bool,bool);
    GamePacketResult line(Vertex,Vertex,bool);
    GamePacketResult rectangle(Vertex,int,int,std::uint16_t,bool,bool,bool);
    bool clipped(int,int) const;
};
}
