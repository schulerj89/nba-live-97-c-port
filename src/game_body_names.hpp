#pragma once
#include "game_body_resources.hpp"
#include "recovered/game_body_names.h"
#include "recovered/game_render_textures.h"

namespace nba97 {
// The actual FEBFC/FEDF0 tables. They contain no cached native pointers, so a
// copy can be paired with a copy of the SAME body owner and rebound on use.
// Before504A8 the pointer outputs may be unknown;4E3CC's bypass path computes
// the center widths without consuming geometry. No center is guessed zero.
struct GameBodyNameState {
    std::array<std::array<Nba97GameBodyReference,4>,10> polygon{};
    std::array<std::array<Nba97GamePeriodValue,4>,10> center{};
};
struct GameBodyNamesResult {
    int result=NBA97_BODY_ARGUMENT;
    Nba97GameBodyNamesProgress progress{};
    std::vector<Nba97GameBodyNameWrite> journal;
    std::string detail;
};
// In-place original504A8 tail. Rebuilds views into the retained allocations and
// imports both sidecars even on a supported-prefix refusal. Not transactional
// or resumable: stage body, names and all downstream state together if needed.
GameBodyNamesResult recenterGameBodyNames(GameBodyResources&,GameBodyNameState&,
                                         std::size_t journalCapacity=200);

struct GameBodyNameRenderResult {
    int result=NBA97_RENDER_ARGUMENT;
    bool entered=false;
    std::uint8_t centersWritten=0;
    std::string detail;
};
// Actual4E3CC, including its required upload/sync callbacks. body may be null
// only when the caller's proven F0F68 bypass is nonzero. Otherwise each packet
// is rebound into body; aliases are retained and no detached packet is made.
// Packet byte views must be fully known and free of pointer cells. This is a
// native entry precondition for the legacy byte-only C owner, not a new game
// branch. Incoming centers may be unknown: only receipt-proven stores change
// their knownness, including when a later upload or resource access refuses.
// Other textures/player/glyph/scratch buffers belong to the caller and must
// satisfy game_render_textures.h's retained fully-known resource contract.
// Callbacks may mutate source bytes/texture scalars, but may not resize/free
// body, replace its metadata, or separately edit the name sidecars mid-call.
// Rebind all external images when cloning their owners. textures' packet views
// are borrowed from body and must not outlive it; this adapter rebuilds them on
// every non-bypass call. Pair names only with its own body or a matching copy.
GameBodyNameRenderResult renderGameBodyName(GameBodyResources* body,GameBodyNameState&,
    Nba97GameRenderTextures&,unsigned player,Nba97GameRenderIo,void*);
}
