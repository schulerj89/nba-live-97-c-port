#include "game_render_backend.hpp"
#include <algorithm>
#include <utility>

namespace nba97 {
namespace {
using Result = GameRenderBackendResult;
bool validRect(Nba97GameImageRect r) {
    return r.x >= 0 && r.y >= 0 && r.w > 0 && r.h > 0 &&
        static_cast<std::size_t>(r.x) + static_cast<std::size_t>(r.w) <= GameVramWords::Width &&
        static_cast<std::size_t>(r.y) + static_cast<std::size_t>(r.h) <= GameVramWords::Height;
}
Result bytesAt(Nba97GameImageReference ref, std::size_t size, bool reading,
               std::uint8_t*& bytes, std::uint8_t*& known) {
    if (!ref.memory || !ref.memory->data || ref.offset < 0 ||
        static_cast<std::uint64_t>(ref.offset) > ref.memory->size) return Result::Resource;
    const auto offset = static_cast<std::size_t>(ref.offset);
    if (size > ref.memory->size - offset) return Result::Resource;
    if (ref.memory->address_mod4_known > 1 || ref.memory->address_mod4 > 3) return Result::Argument;
    if (!ref.memory->address_mod4_known) return Result::AlignmentUnknown;
    if ((ref.memory->address_mod4 + (offset & 3u)) & 3u) {
        // SDK transfers the remainder with CPU LW/SW, but exact16-word DMA
        // blocks skip those instructions. Misaligned pure DMA is unverified;
        // it is not evidence of a MIPS CPU alignment trap.
        return ((size / 4u) & 15u) ? Result::AlignmentTrap : Result::DmaAlignmentUnsupported;
    }
    bytes = ref.memory->data + offset;
    known = ref.memory->known ? ref.memory->known + offset : nullptr;
    if (known) {
        // Check the entire span: an earlier unknown byte must not hide a later
        // malformed knownness value. Reads do not fabricate unknown payloads.
        bool unknown = false;
        for (std::size_t i = 0; i < size; ++i) {
            if (known[i] > 1) return Result::Argument;
            unknown |= known[i] == 0;
        }
        if (reading && unknown) return Result::Unknown;
    }
    return Result::Complete;
}
Nba97GameImageRect imageRect(Nba97GameRenderRect r) { return {r.x,r.y,r.w,r.h}; }
std::int32_t signedLow16(std::int32_t value) {
    const auto low = static_cast<std::uint32_t>(value) & 65535u;
    return low < 32768u ? static_cast<std::int32_t>(low) : static_cast<std::int32_t>(low) - 65536;
}
Result checkSdkLimits(const GameRenderBackend& backend, Nba97GameImageRect rect) {
    if (!backend.sdkTransferLimitsKnown) return Result::SdkLimitsUnknown;
    if (backend.sdkTransferWidth <= 0 || backend.sdkTransferHeight <= 0 ||
        rect.w <= 0 || rect.h <= 0 || rect.w > backend.sdkTransferWidth || rect.h > backend.sdkTransferHeight)
        return Result::SdkLimitsUnsupported;
    return Result::Complete;
}
}

GameVramWords::GameVramWords() : words_(Width * Height), known_(Width * Height) {}

Result GameVramWords::upload(Nba97GameImageRect r, const Nba97GameImageReference& source) {
    if (!validRect(r)) return Result::RectangleUnsupported;
    std::uint8_t *bytes = nullptr, *known = nullptr;
    const auto count = static_cast<std::size_t>(r.w) * static_cast<std::size_t>(r.h);
    const auto result = bytesAt(source, (count + (count & 1u)) * 2, true, bytes, known);
    if (result != Result::Complete) return result;
    std::size_t input = 0;
    for (int y = 0; y < r.h; ++y) for (int x = 0; x < r.w; ++x) {
        const auto index = static_cast<std::size_t>(r.y + y) * Width + static_cast<std::size_t>(r.x + x);
        words_[index] = static_cast<std::uint16_t>(bytes[input] | (static_cast<std::uint16_t>(bytes[input + 1]) << 8));
        known_[index] = 1;
        input += 2;
    }
    return Result::Complete;
}

Result GameVramWords::store(Nba97GameImageRect r, Nba97GameImageReference destination) {
    if (!validRect(r)) return Result::RectangleUnsupported;
    std::uint8_t *bytes = nullptr, *known = nullptr;
    const auto count = static_cast<std::size_t>(r.w) * static_cast<std::size_t>(r.h);
    if (count & 1u) return Result::ReadbackPaddingUnsupported;
    const auto result = bytesAt(destination, count * 2, false, bytes, known);
    if (result != Result::Complete) return result;
    // Refusal is a native operation boundary, not a simulated partial GPU read.
    for (int y = 0; y < r.h; ++y) for (int x = 0; x < r.w; ++x)
        if (!known_[static_cast<std::size_t>(r.y + y) * Width + static_cast<std::size_t>(r.x + x)])
            return Result::Unknown;
    std::size_t output = 0;
    for (int y = 0; y < r.h; ++y) for (int x = 0; x < r.w; ++x) {
        const auto value = words_[static_cast<std::size_t>(r.y + y) * Width + static_cast<std::size_t>(r.x + x)];
        bytes[output] = static_cast<std::uint8_t>(value);
        bytes[output + 1] = static_cast<std::uint8_t>(value >> 8);
        if (known) known[output] = known[output + 1] = 1;
        output += 2;
    }
    return Result::Complete;
}

Result GameVramWords::move(Nba97GameImageRect r, std::int32_t x, std::int32_t y) {
    if (!validRect(r) || x < 0 || y < 0 || x > static_cast<std::int32_t>(Width) - r.w ||
        y > static_cast<std::int32_t>(Height) - r.h) return Result::RectangleUnsupported;
    if (x < r.x + r.w && r.x < x + r.w && y < r.y + r.h && r.y < y + r.h)
        return Result::OverlapUnsupported;
    // Unknown VRAM can be moved without interpreting it. Keep its unknownness
    // instead of silently making a destination's zero payload into known pixels.
    for (int row = 0; row < r.h; ++row) for (int col = 0; col < r.w; ++col) {
        const auto from = static_cast<std::size_t>(r.y + row) * Width + static_cast<std::size_t>(r.x + col);
        const auto to = static_cast<std::size_t>(y + row) * Width + static_cast<std::size_t>(x + col);
        words_[to] = words_[from];
        known_[to] = known_[from];
    }
    return Result::Complete;
}

bool GameVramWords::word(std::size_t x, std::size_t y, std::uint16_t& value) const {
    if (x >= Width || y >= Height || !known_[y * Width + x]) return false;
    value = words_[y * Width + x];
    return true;
}

struct GameRenderMemory::Block {
    std::vector<std::uint8_t> bytes, known;
    int addressMod4 = -1;
    std::shared_ptr<const int> identity = std::make_shared<const int>(0);
};
GameRenderMemory::GameRenderMemory() : lineage_(std::make_shared<const int>(0)) {}
GameRenderMemory::~GameRenderMemory() = default;
GameRenderMemory::GameRenderMemory(const GameRenderMemory& source) : lineage_(source.lineage_) {
    blocks_.reserve(source.blocks_.size());
    for (const auto& block : source.blocks_) blocks_.push_back(std::make_unique<Block>(*block));
}
GameRenderMemory& GameRenderMemory::operator=(const GameRenderMemory& source) {
    if (this != &source) { GameRenderMemory copy(source); *this = std::move(copy); }
    return *this;
}
GameRenderMemory::GameRenderMemory(GameRenderMemory&&) noexcept = default;
GameRenderMemory& GameRenderMemory::operator=(GameRenderMemory&&) noexcept = default;

GameRenderMemory::Allocation GameRenderMemory::add(std::vector<std::uint8_t> bytes,
    std::vector<std::uint8_t> known, int addressMod4) {
    if (bytes.empty() || addressMod4 < -1 || addressMod4 > 3 ||
        (!known.empty() && known.size() != bytes.size()) ||
        std::any_of(known.begin(), known.end(), [](std::uint8_t v) { return v > 1; })) return 0;
    auto block = std::make_unique<Block>();
    block->bytes = std::move(bytes);
    block->known = std::move(known);
    block->addressMod4 = addressMod4;
    if (!lineage_) lineage_ = std::make_shared<const int>(0);
    blocks_.push_back(std::move(block));
    return blocks_.size();
}

Nba97GameRenderBuffer GameRenderMemory::buffer(Allocation id, std::size_t offset, std::size_t size) {
    if (!id || id > blocks_.size()) return {};
    auto& b = *blocks_[id - 1];
    if (offset > b.bytes.size() || size > b.bytes.size() - offset) return {};
    return {b.bytes.data() + offset, size};
}
Nba97GameRenderBuffer GameRenderMemory::knownBuffer(Allocation id, std::size_t offset, std::size_t size) {
    auto result = buffer(id, offset, size);
    if (!result.data) return {};
    const auto& known = blocks_[id - 1]->known;
    if (!known.empty() && std::any_of(known.begin() + offset, known.begin() + offset + size,
        [](std::uint8_t v) { return v != 1; })) return {};
    return result;
}

bool GameRenderMemory::locate(Nba97GameRenderBuffer view, std::size_t& block, std::size_t& offset) const {
    if (!view.data) return false;
    const auto pointer = reinterpret_cast<std::uintptr_t>(view.data);
    for (std::size_t i = 0; i < blocks_.size(); ++i) {
        const auto& bytes = blocks_[i]->bytes;
        const auto first = reinterpret_cast<std::uintptr_t>(bytes.data());
        if (pointer < first || pointer - first > bytes.size()) continue;
        const auto pos = static_cast<std::size_t>(pointer - first);
        if (view.size > bytes.size() - pos) continue;
        block = i; offset = pos; return true;
    }
    return false;
}
bool GameRenderMemory::describe(Nba97GameRenderBuffer view, Nba97GameImageMemory& out) const {
    std::size_t id = 0, offset = 0;
    if (!locate(view, id, offset)) return false;
    auto& block = *blocks_[id];
    out = {view.data, block.known.empty() ? nullptr : block.known.data() + offset, view.size,
        static_cast<std::uint8_t>(block.addressMod4 < 0 ? 0 : (static_cast<std::size_t>(block.addressMod4) + (offset & 3u)) & 3u),
        static_cast<std::uint8_t>(block.addressMod4 >= 0)};
    return true;
}
bool GameRenderMemory::rebind(const GameRenderMemory& source, Nba97GameRenderBuffer original,
    Nba97GameRenderBuffer& rebound) {
    if (!original.data && !original.size) { rebound = {}; return true; }
    std::size_t id = 0, offset = 0;
    if (!lineage_ || lineage_ != source.lineage_ || !source.locate(original, id, offset) ||
        id >= blocks_.size() || blocks_[id]->identity != source.blocks_[id]->identity ||
        blocks_[id]->bytes.size() != source.blocks_[id]->bytes.size()) return false;
    rebound = buffer(id + 1, offset, original.size);
    return rebound.data != nullptr;
}
bool GameRenderMemory::rebind(const GameRenderMemory& source, Nba97GameRenderImage original,
    Nba97GameRenderImage& rebound) {
    if (original.offset > original.storage.size) return false;
    Nba97GameRenderBuffer mapped{};
    if (!rebind(source, original.storage, mapped)) return false;
    rebound = {mapped, original.offset};
    return true;
}

int GameRenderBackend::transferIo(void* context, const Nba97GameImageTransfer* event) {
    if (!context) return 0;
    auto& self = *static_cast<GameRenderBackend*>(context);
    if (!event) { self.lastResult = Result::Argument; return 0; }
    if (!self.unmaskedTransfersKnown) { self.lastResult = Result::MaskModeUnknown; return 0; }
    self.lastResult = checkSdkLimits(self, event->rect);
    if (self.lastResult != Result::Complete) return 0;
    // C owns this descriptor during its synchronous callback. Its bytes must
    // still belong to our retained registry, including exactly this envelope.
    Nba97GameImageMemory owned{};
    if (!event->source.memory || !self.memory.describe(
        {event->source.memory->data, event->source.memory->size}, owned) ||
        owned.known != event->source.memory->known) { self.lastResult = Result::Resource; return 0; }
    self.lastResult = self.vram.upload(event->rect, {&owned, event->source.offset});
    return self.lastResult == Result::Complete ? 1 : 0;
}

int GameRenderBackend::renderIo(void* context, const Nba97GameRenderIoEvent* event) {
    if (!context) return 0;
    auto& self = *static_cast<GameRenderBackend*>(context);
    if (!event) { self.lastResult = Result::Argument; return 0; }
    self.lastResult = Result::Complete;
    switch (event->kind) {
    case NBA97_RENDER_UPLOAD_946B8: {
        Nba97GameImageMemory image{};
        if (!self.memory.describe(event->image.storage, image) ||
            event->image.offset > static_cast<std::size_t>(INT64_MAX)) { self.lastResult = Result::Resource; return 0; }
        // Do not preflight mask mode/whole payload: source header mutations that
        // precede the eventual unsupported transfer must remain in the prefix.
        self.lastUploadResult = nba97_game_image_upload(&self.uploadState,
            {&image, static_cast<std::int64_t>(event->image.offset)},
            {event->x,event->y,event->clut_x,event->clut_y}, self.headerBudget,
            &transferIo, &self, &self.uploadProgress);
        if (self.lastUploadResult != NBA97_IMAGE_COMPLETE) {
            if (self.lastResult == Result::Complete) self.lastResult = Result::SourceUploadRefused;
            return 0;
        }
        return 1;
    }
    case NBA97_RENDER_STORE_99780: {
        self.lastResult = checkSdkLimits(self, imageRect(event->rect));
        if (self.lastResult != Result::Complete) return 0;
        Nba97GameImageMemory destination{};
        if (!self.memory.describe(event->destination, destination)) { self.lastResult = Result::Resource; return 0; }
        self.lastResult = self.vram.store(imageRect(event->rect), {&destination,0});
        break;
    }
    case NBA97_RENDER_MOVE_997E4:
        // Original997E4 returns before GPU dispatch for either zero dimension;
        // its render callers ignore that SDK return. Do not turn this into a
        // full-size PS1 zero-width GPU transfer or a port-only operation error.
        if (!event->rect.w || !event->rect.h) return 1;
        if (!self.unmaskedTransfersKnown) { self.lastResult = Result::MaskModeUnknown; return 0; }
        self.lastResult = self.vram.move(imageRect(event->rect), signedLow16(event->x), signedLow16(event->y));
        break;
    case NBA97_RENDER_SYNC_994F4:
        // Transfers already consumed bytes synchronously. No device work is
        // pending. CPU wrapper side effects, if any, belong to a recovered owner.
        return 1;
    case NBA97_RENDER_SERVICE_8892C:
        if (!self.service) { self.lastResult = Result::ServiceRequired; return 0; }
        if (self.service(self.serviceContext, event) != 1) { self.lastResult = Result::ServiceRefused; return 0; }
        self.lastResult = Result::Complete;
        return 1;
    default: self.lastResult = Result::OperationUnsupported; return 0;
    }
    return self.lastResult == Result::Complete ? 1 : 0;
}

} // namespace nba97
