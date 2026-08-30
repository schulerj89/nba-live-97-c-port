#include "win32_keyboard.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <initializer_list>

int main() {
    using nba97::normalizeWin32Shift;
    assert(normalizeWin32Shift(0x10, 0x00360001) == 0xa1); // Right Shift down
    assert(normalizeWin32Shift(0x10, 0xc0360001) == 0xa1); // Right Shift up
    assert(normalizeWin32Shift(0x10, 0x002a0001) == 0xa0); // Left Shift is NOT Select
    assert(normalizeWin32Shift(0x10, 0) == 0x10); // unknown scan code
    for (auto key : {'C','V','D','F','S','X'})
        assert(normalizeWin32Shift(key, 0x00360001) == unsigned(key));
    assert(normalizeWin32Shift(0x0d, 0x001c0001) == 0x0d); // Start
    assert(normalizeWin32Shift(0xa1, 0) == 0xa1); // already normalized
    using nba97::createPlayerNameKeyMask;
    assert(createPlayerNameKeyMask(normalizeWin32Shift(0x10,0x00360001))==0x100);
    assert(createPlayerNameKeyMask(normalizeWin32Shift(0x10,0x002a0001))==0);
    assert(createPlayerNameKeyMask('C')==0x800);
    assert(createPlayerNameKeyMask('V')==0x40); // authored Circle/backspace
    assert(createPlayerNameKeyMask('D')==0x10);
    assert(createPlayerNameKeyMask('F')==0x20);
    assert(createPlayerNameKeyMask('S')==0); // R1 is not Circle
    assert(createPlayerNameKeyMask('X')==0); // R2 is not Select
    assert(createPlayerNameKeyMask(0x0d)==0x80);
}
