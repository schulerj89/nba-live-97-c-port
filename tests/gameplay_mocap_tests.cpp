#include "gameplay_mocap.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
using Bytes = std::vector<std::uint8_t>;

static void word(Bytes& b, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) b.at(at + i) = static_cast<std::uint8_t>(value >> (8 * i));
}
static void record(Bytes& b, std::size_t at, std::uint16_t flags = 0,
                   std::uint8_t timing = 7, std::uint8_t count = 7, std::uint32_t data = 12) {
    b.at(at) = static_cast<std::uint8_t>(flags); b.at(at + 1) = static_cast<std::uint8_t>(flags >> 8);
    b.at(at + 2) = 0xa2; b.at(at + 3) = timing;
    b.at(at + 4) = 0x11; b.at(at + 5) = 0x22; b.at(at + 6) = 0x33; b.at(at + 7) = count;
    word(b, at + 8, data);
}
static Bytes fixture() {
    Bytes b(0x400); word(b, 0, 8); word(b, 4, 0x158);
    word(b, 8, 0x300); word(b, 0x158 + 83 * 4, 0x300); record(b, 0x300); return b;
}
static Nba97GameMocapIndex parse(const Bytes& b) {
    struct Guarded { std::uint64_t before; Nba97GameMocapIndex index; std::uint64_t after; } out{};
    out.before = 0x0123456789abcdef; out.after = 0xfedcba9876543210;
    std::memset(&out.index, 0xa5, sizeof(out.index)); const auto original = b;
    CHECK(nba97_game_mocap_index(b.data(), b.size(), &out.index) == NBA97_GAME_MOCAP_OK);
    CHECK(out.before == 0x0123456789abcdef && out.after == 0xfedcba9876543210);
    CHECK(b == original);
    return out.index;
}
static void refuse(const Bytes& b, Nba97GameMocapResult reason) {
    Nba97GameMocapIndex out; std::memset(&out, 0xa5, sizeof(out));
    const auto before = out; const auto original = b;
    CHECK(nba97_game_mocap_index(b.data(), b.size(), &out) == reason);
    CHECK(std::memcmp(&out, &before, sizeof(out)) == 0); CHECK(b == original);
}

static void normalization() {
    struct Case { std::uint16_t flags; std::uint8_t timing, count; std::uint16_t flags_out; std::uint8_t timing_out, count_out; };
    // Hand-calculated source quirks are intentional. In particular, count0 with
    // flag09 underflows to255 and count128 with flag08 wraps to0; do not clamp.
    const Case cases[] = {
        {0,7,7,0x20,7,7}, {8,7,7,0x38,3,14}, {9,7,7,0x39,3,13},
        {0x18,7,7,0x38,7,7}, {0x10,7,7,0x30,7,7}, {8,0,0,0x38,0,0},
        {9,0,0,0x39,0,255}, {8,255,128,0x38,127,0}, {9,255,128,0x39,127,255},
        {8,1,255,0x38,0,254}, {9,1,255,0x39,0,253}, {0xa549,255,255,0xa579,127,253}
    };
    for (const auto& c : cases) {
        auto b = fixture(); record(b, 0x300, c.flags, c.timing, c.count);
        const auto result = parse(b); CHECK(result.header_count == 1);
        const auto& h = result.header[0];
        CHECK(h.header_offset == 0x300 && h.data_offset == 0x30c);
        CHECK(h.source_flags == c.flags && h.source_timing == c.timing && h.source_count == c.count);
        CHECK(h.flags == c.flags_out && h.timing == c.timing_out && h.count == c.count_out);
        CHECK(result.reference[0][0] == 0 && result.reference[1][83] == 0);
        for (unsigned channel = 0; channel < 2; ++channel)
            for (unsigned slot = 0; slot < 84; ++slot)
                if (!((channel == 0 && slot == 0) || (channel == 1 && slot == 83)))
                    CHECK(result.reference[channel][slot] == NBA97_GAME_MOCAP_NONE);
        const auto again = parse(b);
        CHECK(std::memcmp(&result, &again, sizeof(result)) == 0);
    }
}

static void aliasesAndDirectories() {
    auto b = fixture(); word(b, 8, 0); word(b, 0x158 + 83 * 4, 0);
    CHECK(parse(b).header_count == 0);
    b.assign(336, 0); const auto empty = parse(b);
    CHECK(empty.directory_offset[0] == 0 && empty.directory_offset[1] == 0);
    CHECK(empty.header_count == 0);
    for (const auto& channel : empty.reference) for (auto at : channel) CHECK(at == NBA97_GAME_MOCAP_NONE);

    b = fixture(); record(b, 0x300, 9, 5, 9); word(b, 8 + 83 * 4, 0x300);
    auto out = parse(b); CHECK(out.header_count == 1 && out.header[0].count == 17);
    CHECK(out.reference[0][83] == out.reference[1][83]);
    word(b, 4, 8); out = parse(b);
    CHECK(out.header_count == 1 && out.reference[0][0] == out.reference[1][0]);
    // Read-only directory overlap is legal: neither directory is mutated.
    b = fixture(); word(b, 8, 0); word(b, 12, 0x300); word(b, 4, 12);
    out = parse(b); CHECK(out.header_count == 1);
    CHECK(out.reference[0][1] == 0 && out.reference[1][0] == 0);

    // Discovery follows channel/slot order, not numeric header/data offset.
    b = fixture(); word(b, 8, 0x30c); word(b, 0x158, 0x300);
    record(b, 0x30c, 9, 9, 4, 0x74); record(b, 0x300, 8, 5, 9, 0x80);
    out = parse(b); CHECK(out.header_count == 2);
    CHECK(out.header[0].header_offset == 0x30c && out.header[1].header_offset == 0x300);
    CHECK(out.header[0].data_offset == 0x380 && out.header[1].data_offset == 0x380);
    CHECK(out.header[0].count == 7 && out.header[1].count == 18);
    CHECK(out.reference[1][0] == 1 && out.reference[1][83] == 1);

    b = fixture(); word(b, 8, 0); out = parse(b);
    CHECK(out.reference[0][0] == NBA97_GAME_MOCAP_NONE && out.reference[1][83] == 0);
    // Last header ends exactly at EOF; target may be an unaligned final byte.
    b = fixture(); word(b, 8, 0x3f4); word(b, 0x158 + 83 * 4, 0);
    record(b, 0x3f4, 0, 0, 0, 11); out = parse(b);
    CHECK(out.header[0].data_offset == 0x3ff);
    // Backward target0 and backwards into ordinary payload are supported.
    word(b, 0x3fc, static_cast<std::uint32_t>(-0x3f4));
    CHECK(parse(b).header[0].data_offset == 0);
    b = fixture(); word(b, 0x308, static_cast<std::uint32_t>(-0x40));
    CHECK(parse(b).header[0].data_offset == 0x2c0);
    // The resolver does not establish payload non-overlap, alignment or stride.
    word(b, 0x308, 0); CHECK(parse(b).header[0].data_offset == 0x300);

    b.assign(8 + 672 + 168 * 12, 0); word(b, 0, 8); word(b, 4, 344);
    for (unsigned i = 0; i < 168; ++i) {
        const auto at = 680 + i * 12; word(b, 8 + i * 4, at);
        record(b, at, 0, 7, static_cast<std::uint8_t>(i), 0);
    }
    out = parse(b); CHECK(out.header_count == 168);
    for (unsigned i = 0; i < 168; ++i) {
        CHECK(out.reference[i / 84][i % 84] == i);
        CHECK(out.header[i].header_offset == 680 + i * 12);
    }
}

static void guards() {
    auto b = fixture(); b.resize(7); refuse(b, NBA97_GAME_MOCAP_FILE_SIZE);
    for (auto root : {0u, 4u}) for (auto at : {0x3fcu, 0xfffffffcu, 9u}) {
        b = fixture(); word(b, root, at); refuse(b, NBA97_GAME_MOCAP_DIRECTORY);
    }
    for (auto at : {0x3f8u, 0x301u, 0xfffffffcu}) {
        b = fixture(); word(b, 8, at); refuse(b, NBA97_GAME_MOCAP_HEADER);
    }
    for (auto relative : {0xfffffcffu, 0x100u, 0x7fffffffu, 0x80000000u}) {
        b = fixture(); word(b, 0x308, relative); refuse(b, NBA97_GAME_MOCAP_DATA_TARGET);
    }
    for (auto flags : {0x20u, 0x28u}) {
        b = fixture(); record(b, 0x300, static_cast<std::uint16_t>(flags));
        refuse(b, NBA97_GAME_MOCAP_RELOCATED_INPUT);
    }
    for (auto at : {4u, 0x20u, 0x154u, 0x158u, 0x2a4u}) {
        b = fixture(); word(b, 8, at); refuse(b, NBA97_GAME_MOCAP_OVERLAP);
    }
    for (auto at : {0x2f8u, 0x2fcu, 0x304u, 0x308u}) {
        b = fixture(); word(b, 0x158, at); refuse(b, NBA97_GAME_MOCAP_OVERLAP);
    }
    // A late failure must not publish the already-validated first channel.
    b = fixture(); word(b, 0x158 + 83 * 4, 0xfffffffc); refuse(b, NBA97_GAME_MOCAP_HEADER);
    Nba97GameMocapIndex out{}; const auto before = out; b = fixture();
    CHECK(nba97_game_mocap_index(nullptr, b.size(), &out) == NBA97_GAME_MOCAP_ARGUMENT);
    CHECK(nba97_game_mocap_index(b.data(), b.size(), nullptr) == NBA97_GAME_MOCAP_ARGUMENT);
    if (std::numeric_limits<std::size_t>::max() > UINT32_MAX)
        CHECK(nba97_game_mocap_index(b.data(), static_cast<std::size_t>(UINT32_MAX) + 1, &out) == NBA97_GAME_MOCAP_FILE_SIZE);
    CHECK(std::memcmp(&before, &out, sizeof(out)) == 0);
    // Public C argument-buffer aliasing is rejected before reading or writing.
    alignas(Nba97GameMocapIndex) std::array<std::uint8_t, 8192> storage{};
    std::memcpy(storage.data() + 2048, b.data(), b.size()); const auto original = storage;
    for (auto offset : {0u, 2048u, 2304u}) {
        CHECK(nba97_game_mocap_index(storage.data() + 2048, b.size(),
              reinterpret_cast<Nba97GameMocapIndex*>(storage.data() + offset)) == NBA97_GAME_MOCAP_ARGUMENT);
        CHECK(storage == original);
    }
}

static void ownership() {
    auto b = fixture(); auto resource = nba97::decode_gameplay_mocap(b);
    CHECK(resource->bytes() == b && resource->index().header_count == 1);
    CHECK(resource->header(0, 0) == resource->header(1, 83));
    CHECK(resource->header(1, 0) == nullptr);
    for (const auto& invalid : {std::pair<std::size_t, std::size_t>{2,0}, {0,84}, {SIZE_MAX,SIZE_MAX}}) {
        bool threw = false;
        try { resource->header(invalid.first, invalid.second); } catch (const std::out_of_range&) { threw = true; }
        CHECK(threw);
    }
    const auto retained = resource; const auto* old_header = retained->header(0, 0);
    record(b, 0x300, 9, 5, 9); resource = nba97::decode_gameplay_mocap(b);
    CHECK(resource != retained && old_header->count == 7 && resource->header(0,0)->count == 17);
    CHECK(retained->bytes() != resource->bytes());
    const auto valid = resource;
    b.resize(7); bool threw = false;
    try { resource = nba97::decode_gameplay_mocap(b); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw && resource == valid && resource->header(0,0)->count == 17);
    resource.reset(); CHECK(retained->header(0,0) == old_header && old_header->count == 7);
}

int main() {
    normalization(); aliasesAndDirectories(); guards(); ownership();
    std::printf("GAMEPLAY MOCAP PASS: %u checks; raw resolver, aliases, byte quirks, native guards and ownership; no sampler/gameplay claim\n", checks);
}
