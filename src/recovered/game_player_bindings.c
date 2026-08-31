#include "game_player_bindings.h"
#include <string.h>

Nba97GamePlayerBindingsResult nba97_game_player_bindings(
    Nba97GamePlayerBindingsEffects* out, const Nba97GamePlayerBindingsInput* input) {
    Nba97GamePlayerBindingsInput in;
    Nba97GamePlayerBindingsEffects next;
    int i;
    unsigned side;
    if (!out || !input) return NBA97_PLAYER_BINDINGS_ARGUMENT;
    memcpy(&in, input, sizeof(in));
    if (!in.player_byte9) return NBA97_PLAYER_BINDINGS_ARGUMENT;
    memset(&next, 0, sizeof(next));
    next.trap_table_slot = 255;

    /*646E8..64768: home then away for each slot, descending4..0. */
    for (i = 4; i >= 0; --i) for (side = 0; side < 2; ++side) {
        uint16_t slot = in.lineup[side][i];
        uint16_t player;
        unsigned binding = side * 5 + (unsigned)i;
        if (slot >= 12) return NBA97_PLAYER_BINDINGS_LINEUP;
        player = in.player_reference[side][slot];
        if (player >= in.player_count) return NBA97_PLAYER_BINDINGS_PLAYER_REFERENCE;
        next.player_reference[binding] = player;
        next.status_reference[binding] = (uint8_t)(side * 12 + slot);
    }
    /*6477C..647F4: clear12 entries, then scatter the entire signed lineup.
     * Source quirk: duplicate entries overwrite earlier inverse indices. */
    for (i = 0; i < 12; ++i) for (side = 0; side < 2; ++side)
        next.inverse_lineup[side][i] = 0xffff;
    for (i = 0; i < 12; ++i) for (side = 0; side < 2; ++side) {
        uint16_t slot = in.lineup[side][i];
        if (slot & 0x8000) continue;
        if (slot >= 12) return NBA97_PLAYER_BINDINGS_LINEUP;
        next.inverse_lineup[side][slot] = (uint16_t)i;
    }

    for (i = 9; i >= 0; --i) {
        unsigned entity = in.entity_table[i];
        uint32_t binding;
        uint16_t opponent_slot;
        uint8_t divisor;
        Nba97GameBindingEntityEffect* effect;
        if (entity >= 10) return NBA97_PLAYER_BINDINGS_ENTITY_REFERENCE;
        binding = in.entity[entity].binding_index;
        if (binding >= 10) return NBA97_PLAYER_BINDINGS_ENTITY_INDEX;
        effect = &next.entity[entity];
        ++next.visited_entities;
        /*64834/6483C: low16 subtraction, including underflow for raw+D9. */
        effect->word38 = (uint16_t)(binding - in.entity[entity].side_byte);
        effect->status_reference = next.status_reference[binding];
        effect->player_reference = next.player_reference[binding];
        divisor = in.player_byte9[effect->player_reference];
        /* Signed multiply-high byD20D20D3, add, arithmetic>>6 at64868..6487C
         * equals floor(byte9*256/78) over the complete unsigned-byte domain. */
        effect->scale_c6 = (uint16_t)(((unsigned)divisor * 256u) / 78u);
        effect->written |= NBA97_BINDING_WORD38 | NBA97_BINDING_STATUS1C |
                           NBA97_BINDING_PLAYER20 | NBA97_BINDING_SCALEC6;
        if (!divisor) {
            /* Original bug/trap is observable AFTER these writes. No repaired
             * height, fabricated player, reciprocal0, or later helper calls. */
            next.trap_table_slot = (uint8_t)i;
            memcpy(out, &next, sizeof(next));
            return NBA97_PLAYER_BINDINGS_DIVIDE_TRAP;
        }
        effect->inverse_c8 = (uint16_t)(0x4e00u / divisor);
        effect->written |= NBA97_BINDING_INVERSEC8;
        opponent_slot = in.entity[entity].opponent_slot;
        if (opponent_slot >= 10) return NBA97_PLAYER_BINDINGS_OPPONENT_REFERENCE;
        entity = in.entity_table[opponent_slot];
        if (entity >= 10) return NBA97_PLAYER_BINDINGS_ENTITY_REFERENCE;
        /*648DC delay-slot store. A repeated destination is last-write-wins;
         * use actual table aliases, never assume opponent_side+local_index. */
        next.entity[entity].opponent_cc = (uint16_t)binding;
        next.entity[entity].written |= NBA97_BINDING_OPPONENTCC;
    }
    next.tail_count = 4;
    next.first_6459c_fallback_byte = 0x84;
    next.tail[0].owner = next.tail[1].owner = NBA97_BINDING_6459C;
    next.tail[2].owner = next.tail[3].owner = NBA97_BINDING_644FC;
    next.tail[0].side_word = next.tail[2].side_word = 0;
    next.tail[1].side_word = next.tail[3].side_word = 5;
    memcpy(out, &next, sizeof(next));
    return NBA97_PLAYER_BINDINGS_READY;
}
