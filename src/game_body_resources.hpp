#pragma once
#include "game_render_backend.hpp"
#include "recovered/game_body_geometry.h"
#include <array>
#include <memory>
#include <string>

namespace nba97 {
struct GameBodyBytes {
    std::vector<std::uint8_t> bytes,known;
    int originalAddressMod4=-1;
};
class GameBodyResources;
struct GameBodyResourceResult {
    int result=NBA97_BODY_ARGUMENT;
    unsigned sidesCompleted=0;
    std::string detail;
    std::array<Nba97GameBodyGeometryProgress,2> side{};
    std::vector<Nba97GameBodyWrite> journal;
    std::unique_ptr<GameBodyResources> resource;
};

// Retained ordinary TLST/V/W resources, not the FATL Create Player decoder.
// Native references live in parallel cells. No emulated RAM or fabricated
// original heap addresses. Copies duplicate bytes AND cells, retaining aliases.
class GameBodyResources {
public:
    enum Allocation : unsigned { Contexts=0,Home=1,Away=2,RootsA=3,RootsB=4,Count=5 };
    static constexpr std::uint32_t ContextStride=0xbcc;
    GameBodyResources(const GameBodyResources&)=default;
    GameBodyResources& operator=(const GameBodyResources&);
    GameBodyResources(GameBodyResources&&) noexcept=default;
    GameBodyResources& operator=(GameBodyResources&&) noexcept=default;
    // Explicit typed C view. Its raw zero bytes at pointer cells are unused
    // metadata, never source NULL pointers. Rebuild views after copying/moving.
    Nba97GameBodyBuffer buffer(unsigned allocation);
    Nba97GameBodyReference referenceAt(Nba97GameBodyReference slot);
    // For legacy consumers without knownness. Rejects any unknown byte or
    // reference cell, including a partial overlap with a pointer word.
    Nba97GameRenderBuffer knownBuffer(Nba97GameBodyReference,std::size_t size);
    static Nba97GameBodyReference context(unsigned physicalPlayer);
    Nba97GameBodyReference partHeader(unsigned physicalPlayer,unsigned part);
    Nba97GameBodyReference partPivot(unsigned physicalPlayer,unsigned part);
    Nba97GameBodyReference descriptor(unsigned physicalPlayer);
    Nba97GameBodyReference alternateHeaders(unsigned physicalPlayer);
private:
    GameBodyResources()=default;
    GameRenderMemory memory_;
    std::array<GameRenderMemory::Allocation,Count> allocation_{};
    std::array<std::size_t,Count> size_{};
    std::array<std::vector<Nba97GameBodyCell>,Count> cells_;
    void add(unsigned,GameBodyBytes);
    Nba97GameBodyBuffer span(Nba97GameBodyReference,std::size_t);
    friend GameBodyResourceResult prepareGameBodyResources(GameBodyBytes,GameBodyBytes,GameBodyBytes,std::size_t);
};
// Inputs are already validated logical loader payloads, excluding trailers.
// This operation does not execute29BFC/CRC/504A8's name-UV tail or invent cold
// loader state. It binds the actual two50768 calls: home physical0 then away5.
// Two320-byte fixed source root arrays start UNKNOWN;50768 only stores their
// references. Camera/pose owners must establish their contents before use.
// Publishes a new owner only after both calls complete. Diagnostic ordered
// writes remain on refusal; this native publication policy is not source
// rollback. journalCapacity bounds the combined two-call diagnostic storage.
GameBodyResourceResult prepareGameBodyResources(GameBodyBytes contexts,GameBodyBytes home,
    GameBodyBytes away,std::size_t journalCapacity=100000);
}
