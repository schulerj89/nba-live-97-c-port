#include "game_setup.h"
int nba97_setup_step(uint8_t choices[4], unsigned card, int direction) {
    unsigned count;
    if (!choices || card >= 4 || !direction) return 0;
    count = card == 0 ? 4 : 3;
    if (choices[card] >= count) return 0;
    choices[card] = (uint8_t)((choices[card] + (direction < 0 ? count-1 : 1)) % count);
    return 1;
}
