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
    size_t object_offset; /* Checked entry:0/4 from stable returned identity.
                          * Original entry:0; object.data already advances. */
} Nba97GamePlayerLabelEvent;
/* CREATE must run an owned text/font object allocator and return its actual
 * object, or {NULL,0} for original allocation failure. Refusal is distinct
 * from allocation failure. RESET_PACKET is99960, including backend dispatch;
 * it is NOT a boolean visible/hidden flag. No renderer is silently simulated. */
typedef int (*Nba97GamePlayerLabelIo)(void*,const Nba97GamePlayerLabelEvent*,Nba97GameRenderBuffer* created);
enum Nba97GamePlayerLabelResult {NBA97_LABEL_UNKNOWN=-5,NBA97_LABEL_ALIGNMENT=-6};
typedef struct Nba97GamePlayerLabelAccess {
    Nba97GameRenderBuffer buffer;
    size_t offset,size;
    uint32_t source_pc;
    uint8_t role,write; /*0=buffer,1=current B2048 style,2=returned object. */
} Nba97GamePlayerLabelAccess;
typedef struct Nba97GamePlayerLabelStorage {
    uint8_t* data;
    uint8_t* known; /* NULL means all known; otherwise one canonical0/1 per byte. */
    uint8_t address_mod2,address_mod2_known;
} Nba97GamePlayerLabelStorage;
/* Resolve only the reached span, without changing its bytes/knownness. Return
 * a render/label result and retained storage valid through the immediate C
 * access. STYLE ignores buffer and resolves the current original B2048 pointer;
 * OBJECT resolves the last CREATE identity plus offset, allowing split native
 * storage. BUFFER preserves the supplied native alias. Never resize/free retained
 * allocations during a run. The C owner checks metadata, reads or writes, and
 * establishes knownness exactly when each source store occurs. */
typedef int (*Nba97GamePlayerLabelResolve)(void*,const Nba97GamePlayerLabelAccess*,Nba97GamePlayerLabelStorage*);
/* Complete35A44 direct owner with35A24 reset adapter. Leaves30758,30D18,99960
 * are explicit required native text/backend boundaries. Original local string
 * has only32 bytes; longer text refuses at that boundary rather than truncating
 * the original stack overflow. Entity IDs are not repaired to table indices.
 * Prefix writes and completed labels survive refusal. */
int nba97_game_player_labels(Nba97GamePlayerLabels*,Nba97GamePlayerLabelIo,void*,unsigned* completed);
/* Checked counterpart for partial-known retained resources. Unread padding is
 * untouched. Unknown consumed bytes and unproven/misaligned halfword stores
 * explicitly refuse, retaining earlier source writes. The original entry above
 * remains the compatibility boundary for fully known, source-aligned buffers. */
int nba97_game_player_labels_checked(Nba97GamePlayerLabels*,Nba97GamePlayerLabelIo,
    Nba97GamePlayerLabelResolve,void*,unsigned* completed);
#ifdef __cplusplus
}
#endif
#endif
