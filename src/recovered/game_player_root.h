#ifndef NBA97_GAME_PLAYER_ROOT_H
#define NBA97_GAME_PLAYER_ROOT_H
#include "game_player_geometry.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Uses the existing named rotation/translation/V0/MVMVA/IR/MAC operations plus
 * these actual56624/56650 results. Every operation shares retained geometry. */
enum Nba97PlayerRootMathKind {
    NBA97_ROOT_PROJECT=7,NBA97_ROOT_SCREEN,NBA97_ROOT_IR0,
    NBA97_ROOT_FLAGS,NBA97_ROOT_DEPTH
};
typedef struct Nba97GamePlayerRootInput {
    Nba97GameBodyBuffer* buffers;size_t buffer_count;
    Nba97GameBodyReference context_f0ed4,index_1029b0;
    Nba97GameBodyReference scales_105f48,camera_f9fd8,preset_26384,trig_b3254;
    Nba97GameBodyReference world_fb480,primary_103fd8,alternate_10b2b8;
    Nba97GameBodyReference ground_102f8c,screen_fea94,depth_106038;
    Nba97PlayerMath math;void* math_user;
} Nba97GamePlayerRootInput;
typedef struct Nba97GamePlayerRootProgress {
    Nba97GameBodyReference stopped_reference;
    size_t writes,math_calls;uint32_t stopped_pc;uint8_t completed;
} Nba97GamePlayerRootProgress;
/* Complete5200C plus56080/51F18/562CC/55F18/55F44/56650/51F04/56624.
 * Buffers/reference cells retain actual allocation aliases. Globals ending in
 * context/index are mutable slot locations; the other refs are fixed table or
 * resource bases. Index shifts and address additions wrap32bits before bounds.
 * Caller supplies real camera bytes, actual peractor scale and normalized pose
 * references. No actor/camera/heap defaults or bodyprojection are invented.
 *
 * Same allocation/metadata/alignment/lifetime restrictions as55368. The fixed
 *26384 template is copied opaquely in the aligned source-word domain; unused
 * halfwords may be unknown. Stack scratch does not alias input allocations.
 * Reached word spans are checked completely, but only consumed bits need be
 * known where source AND/shift/CTC2 discards a half. Halfword overwrite of a
 * relocated reference refuses ADDRESS_REQUIRED, never fabricates addressbits.
 * Journal capacity bounds visible stores only. Refusal retains the exact CPU
 * and geometry prefix; clone both for atomic publication, do not resume/retry
 * a partial run. Math callbacks change geometry only, never retained buffers.
 */
int nba97_game_player_root(const Nba97GamePlayerRootInput*,
    Nba97GamePlayerGeometryWrite*,size_t,Nba97GamePlayerRootProgress*);
#ifdef __cplusplus
}
#endif
#endif
