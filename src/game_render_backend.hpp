#pragma once

#include "recovered/game_image_upload.h"
#include "recovered/game_render_io.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace nba97 {

// Native ownership only. Original header walking and placement arithmetic live
// in game_image_upload.c. This is a word plane, not the Create Player atlas:
// variants in that atlas do not represent simultaneous contents of actual VRAM.
enum class GameRenderBackendResult {
    Complete, Argument, RectangleUnsupported, OverlapUnsupported,
    Resource, Unknown, AlignmentUnknown, AlignmentTrap, DmaAlignmentUnsupported, ReadbackPaddingUnsupported,
    MaskModeUnknown, SdkLimitsUnknown, SdkLimitsUnsupported, SourceUploadRefused,
    ServiceRequired, ServiceRefused, OperationUnsupported
};

class GameVramWords {
public:
    static constexpr std::size_t Width = 1024, Height = 512;
    GameVramWords(); // Every word is UNKNOWN, not an original black screen.
    // SDK9AC7C consumes ceil(w*h/2) 32-bit words. Validate the padding halfword
    // for an odd pixel count, but do not put it into an extra VRAM pixel.
    GameRenderBackendResult upload(Nba97GameImageRect, const Nba97GameImageReference&);
    // Odd counts would also write one extra CPU halfword. Its GPU value is not
    // yet recovered, so that readback domain explicitly refuses.
    GameRenderBackendResult store(Nba97GameImageRect, Nba97GameImageReference);
    // Only positive, in-range, disjoint rectangles are supported. Even an exact
    // self-copy is refused until the GPU overlap domain has independent proof.
    GameRenderBackendResult move(Nba97GameImageRect, std::int32_t x, std::int32_t y);
    bool word(std::size_t x, std::size_t y, std::uint16_t& value) const;
    // A completed native drawing operation establishes exactly one VRAM word.
    // No implicit clear, mask test, blend, wrapping or invented prior value.
    bool drawWord(std::size_t x, std::size_t y, std::uint16_t value);

private:
    std::vector<std::uint16_t> words_;
    std::vector<std::uint8_t> known_;
};

// Stable allocations can have many overlapping views. Cloning duplicates each
// allocation once, then rebind() maps aliases into that same cloned allocation.
// No fake PS1 addresses or detached copies of image records are introduced.
class GameRenderMemory {
public:
    using Allocation = std::size_t; // one-based; zero is invalid
    GameRenderMemory();
    ~GameRenderMemory();
    GameRenderMemory(const GameRenderMemory&);
    GameRenderMemory& operator=(const GameRenderMemory&);
    GameRenderMemory(GameRenderMemory&&) noexcept;
    GameRenderMemory& operator=(GameRenderMemory&&) noexcept;

    // Empty knownness means all bytes are known. Otherwise it has exactly one
    // canonical 0/1 per byte. addressMod4=-1 means original alignment unknown;
    // native heap alignment is never evidence for the original address.
    Allocation add(std::vector<std::uint8_t> bytes,
                   std::vector<std::uint8_t> known = {}, int addressMod4 = -1);
    Nba97GameRenderBuffer buffer(Allocation, std::size_t offset, std::size_t size);
    // Existing render C owners have no knownness channel. Supply their buffers
    // only through knownBuffer(), which refuses even one unknown byte.
    Nba97GameRenderBuffer knownBuffer(Allocation, std::size_t offset, std::size_t size);
    bool describe(Nba97GameRenderBuffer, Nba97GameImageMemory&) const;
    bool rebind(const GameRenderMemory& source, Nba97GameRenderBuffer original,
                Nba97GameRenderBuffer& rebound);
    bool rebind(const GameRenderMemory& source, Nba97GameRenderImage original,
                Nba97GameRenderImage& rebound);

private:
    struct Block;
    bool locate(Nba97GameRenderBuffer, std::size_t& block, std::size_t& offset) const;
    std::vector<std::unique_ptr<Block>> blocks_;
    std::shared_ptr<const int> lineage_;
};

// CPU transfers are synchronous, so a supported sync drains no queued work.
// CD/service8892C is a DIFFERENT boundary and must have a real service callback.
// Mask state must be established by the caller from source initialization; a
// zeroed native object is not proof that original GPU mask checking was off.
struct GameRenderBackend {
    GameRenderMemory memory;
    GameVramWords vram;
    Nba97GameImageUploadState uploadState{};
    std::size_t headerBudget = 4096;
    bool unmaskedTransfersKnown = false;
    // SDK9AC7C/9AED0 clamp dimensions against C55C4/C55C6. This backend
    // supports positive transfers needing no clamp, plus9AC7C's verified
    // zero-after-clamp no-data return. Both require explicit positive limits.
    bool sdkTransferLimitsKnown = false;
    std::int16_t sdkTransferWidth = 0, sdkTransferHeight = 0;
    Nba97GameRenderIo service = nullptr;
    void* serviceContext = nullptr;
    GameRenderBackendResult lastResult = GameRenderBackendResult::Complete;
    int lastUploadResult = NBA97_IMAGE_COMPLETE;
    Nba97GameImageUploadProgress uploadProgress{};

    // A copied backend stages VRAM and all allocations together. Source C
    // structs still contain old views: explicitly rebind them before execution.
    // Service context is external and is NOT transactional or cloned.
    static int renderIo(void*, const Nba97GameRenderIoEvent*);
    static int transferIo(void*, const Nba97GameImageTransfer*);
};

} // namespace nba97
