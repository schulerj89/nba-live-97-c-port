#pragma once
#include <cstdint>

namespace nba97 {
// WM_KEYDOWN/UP report VK_SHIFT, not VK_LSHIFT/VK_RSHIFT. Decode the
// scan code before dispatching Select; leave every other key unchanged.
constexpr std::uint32_t normalizeWin32Shift(std::uint32_t key,
                                           std::uintptr_t message_bits) noexcept {
    if (key != 0x10u) return key;
    const auto scan = (message_bits >> 16) & 0xffu;
    if (scan == 0x36u) return 0xa1u;
    if (scan == 0x2au) return 0xa0u;
    return key;
}

// Shared recovered frontend callback tokens, NOT raw PS1 pad bit positions.
// Meaning belongs to the active descriptor; X/R2 is not Cross or Cancel.
constexpr std::uint16_t frontendSelectorKeyMask(std::uint32_t key) noexcept {
    switch (key) {
    case 0x26: return 1; // Up
    case 0x28: return 2; // Down
    case 0x27: return 4; // Right
    case 0x25: return 8; // Left
    case 'D': return 0x10;
    case 'F': case 'H': case 0x70: return 0x20;
    case 'V': case 0x08: return 0x40;
    case 0x0d: return 0x80;
    case 0xa1: case 0x1b: return 0x100;
    case 'C': case 0x20: return 0x800;
    default: return 0;
    }
}
// Keep the verified Create Player API and its context-specific interpretation.
constexpr std::uint16_t createPlayerNameKeyMask(std::uint32_t key) noexcept {
    return frontendSelectorKeyMask(key);
}
// State5 consumes76198's whole normalized mask. Shoulder buttons have no
// individual callback here, but must survive chords with Start/Select.
constexpr std::uint16_t userSetupKeyMask(std::uint32_t key) noexcept {
    switch(key) {
    case 'A': return 0x200; // L1
    case 'Z': return 0x1000; // L2
    case 'S': return 0x400; // R1
    case 'X': return 0x2000; // R2
    default: return frontendSelectorKeyMask(key);
    }
}
}
