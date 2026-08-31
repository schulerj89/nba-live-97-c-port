#ifndef NBA97_GAME_RENDER_TEXTURES_H
#define NBA97_GAME_RENDER_TEXTURES_H
#include "game_render_io.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Nba97GameRenderPlayer {
    Nba97GameRenderBuffer record; /* Actual player bytes, including NUL name+29. */
} Nba97GameRenderPlayer;
typedef struct Nba97GameRenderTextures {
    Nba97GameRenderPlayer* player[10]; /* Physical FC654+i*F4 bindings. */
    Nba97GameRenderImage glyph[26]; /* FECA8, ZDOMLTRS entries. */
    Nba97GameRenderImage digit[2][10]; /* FAC24/FB154, signed index is NOT clamped. */
    Nba97GameRenderImage number_base[2]; /* EBC38/F0F64: palette header via word>>8. */
    Nba97GameRenderImage team_palette[2]; /* EBC48 + side*2940. */
    Nba97GameRenderBuffer skin_bank; /* Entire owned FEEB8 source window. */
    Nba97GameRenderImage name_scratch; /* FCD78,16-byte header+1500 packed bytes. */
    Nba97GameRenderBuffer number_scratch; /* 109DA8,1040 bytes. */
    Nba97GameRenderBuffer name_polygon[10][4]; /* FEBFC/FEC00, two live pairs; may alias. */
    uint32_t name_center[10][4]; /* FEDF0/FEDF4 words. */
    uint32_t bypass_name_uv; /* Actual F0F68. */
    int32_t name_xy[10][2],number_xy[10][2],number_clut_xy[10][2];
    int32_t patch_id[24];
    Nba97GameRenderRect patch_rect[34]; /* FCB18:24 sources,then10 destinations. */
    Nba97GameRenderImage patch_palette[24]; /* D9294 + slot*210. */
    int32_t patch_clut_xy[10][2];
    uint32_t height[10]; /* 51ED8 in4D9EC uses loop index, not entity.word00. */
    /* Retained scalar scratch writes from4E3CC/539FC. Pointer scratch is
     * represented by indices/offsets into the owned resources above. */
    uint32_t name_cursor,name_zero,name_glyph;
    size_t name_position;
    int32_t number_value;
    uint8_t name_spacing,name_nibble;
    Nba97GameRenderImage number_palette;
} Nba97GameRenderTextures;

/* Bounded CPU owners4E3CC,539FC(+4D8C0),4DAD8,4D944 and4D9EC's ten-player
 * loop. image uploads execute946B8 then994F4 exactly as50E40;50E6C really is
 * JR RA/NOP. Functions do not decode frontend assets or invent model UVs.
 * Call nba97_game_render_bindings(4D38C) before textures for the full4D9EC.
 * Reached invalid resources and callback refusal retain the exact written
 * prefix; they are native safety refusals, never original successful returns.
 * Names are not truncated; any raster address outside supplied owned storage
 * refuses. Signed jersey bytes except-1 can select negative digit indices;
 * those unowned table entries refuse, rather than becoming positive jerseys.
 * Inputs are live; callback side effects must be reflected in this state. */
int nba97_game_render_name(Nba97GameRenderTextures*,unsigned,Nba97GameRenderIo,void*);
/* Same4E3CC owner with a native write receipt for the four center words.
 * Bitj is set only AFTER name_center[player][j] is actually written. This
 * lets an adapter retain unknown incoming centers across an early refusal,
 * including the bypass path that computes widths before body loading.
 * The receipt must be separate from all source state/buffers. */
int nba97_game_render_name_tracked(Nba97GameRenderTextures*,unsigned,Nba97GameRenderIo,void*,uint8_t* centers_written);
int nba97_game_render_number(Nba97GameRenderTextures*,unsigned,Nba97GameRenderIo,void*);
int nba97_game_render_palette(Nba97GameRenderTextures*,unsigned,Nba97GameRenderIo,void*);
int nba97_game_render_patch(Nba97GameRenderTextures*,unsigned,unsigned,Nba97GameRenderIo,void*);
int nba97_game_render_textures(Nba97GameRenderTextures*,Nba97GameRenderIo,void*,unsigned* completed);

/* 4D38C has no resource reads besides these two12-word reference arrays.
 * entity_offset entries correspond to FC630..FC65C; indices8/9/10/11 denote
 * special references described below, not necessarily an entity field.
 * Native token copies preserve aliases. Knownness belongs to caller metadata.
 * References: FC630..64C offsets below; FC650=20BEC table; FC654=entity0;
 * FC658=entity0-A4; FC65C=entity0-B8; FC660=F9FFE; FC6C4=1F7EC.
 * Output copied arrays FC664 andFC694 do not overlap input arrays. */
typedef struct Nba97GameRenderBindings {
    int16_t entity_offset[12];
    uint32_t copied20b8c[12],copied20bbc[12];
    uint8_t render_first;
} Nba97GameRenderBindings;
int nba97_game_render_bindings(Nba97GameRenderBindings*,const uint32_t[12],const uint32_t[12]);
#ifdef __cplusplus
}
#endif
#endif
