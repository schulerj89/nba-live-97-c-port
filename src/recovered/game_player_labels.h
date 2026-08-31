#ifndef NBA97_GAME_PLAYER_LABELS_H
#define NBA97_GAME_PLAYER_LABELS_H
#include "game_render_io.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Nba97GamePlayerLabelEntity {
    uint32_t word00;
    uint8_t side_d9;
    Nba97GameRenderBuffer player;
} Nba97GamePlayerLabelEntity;
typedef struct Nba97GamePlayerLabels {
    Nba97GamePlayerLabelEntity* entity_table[10]; /* Ten separate20BEC lookups. */
    Nba97GameRenderBuffer style; /* B2048 current style; callbacks may replace. */
    const Nba97GameRenderBuffer* position_name; /* B3058 string pointer window. */
    size_t position_count;
    uint8_t option21d83;
    uint16_t dirty_fdb4e;
} Nba97GamePlayerLabels;
enum Nba97GamePlayerLabelEventKind {
    NBA97_LABEL_RESET_GROUP_30758=1,NBA97_LABEL_CREATE_30D18,
    NBA97_LABEL_RESET_PACKET_99960
};
typedef struct Nba97GamePlayerLabelEvent {
    int kind;
    int32_t group,id,x,y,argument;
    const char* text; /* CREATE: temporary NUL string, valid during call. */
    Nba97GameRenderBuffer object; /* RESET_PACKET: actual returned object/word. */
} Nba97GamePlayerLabelEvent;
/* CREATE must run an owned text/font object allocator and return its actual
 * object, or {NULL,0} for original allocation failure. Refusal is distinct
 * from allocation failure. RESET_PACKET is99960, including backend dispatch;
 * it is NOT a boolean visible/hidden flag. No renderer is silently simulated. */
typedef int (*Nba97GamePlayerLabelIo)(void*,const Nba97GamePlayerLabelEvent*,Nba97GameRenderBuffer* created);
/* Complete35A44 direct owner with35A24 reset adapter. Leaves30758,30D18,99960
 * are explicit required native text/backend boundaries. Original local string
 * has only32 bytes; longer text refuses at that boundary rather than truncating
 * the original stack overflow. Entity IDs are not repaired to table indices.
 * Prefix writes and completed labels survive refusal. */
int nba97_game_player_labels(Nba97GamePlayerLabels*,Nba97GamePlayerLabelIo,void*,unsigned* completed);
#ifdef __cplusplus
}
#endif
#endif
