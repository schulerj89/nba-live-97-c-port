#ifndef NBA97_GAME_CAMERA_H
#define NBA97_GAME_CAMERA_H
#include "game_text_objects.h"
#include "game_player_root.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Nba97GameCameraEvent {
    uint32_t pc,address,value,target,argument[4];
    uint16_t rectangle[4];
    uint8_t kind,width,known_mask,completed;
} Nba97GameCameraEvent;
/* kind0: retained store; kind1: actual external source call. Calls may mutate
 * retained bytes synchronously, but not descriptors/journal/progress. Return1
 * only after implementing the requested effects. A5810 and913BC also supply
 * a canonical known return. No successful input/device/monitor defaults exist.
 *997E4 gets typed private-stack rectangle values, not a fabricated address.
 */
typedef int (*Nba97GameCameraIo)(void*,Nba97GameTextMemory*,const Nba97GameCameraEvent*,Nba97GamePeriodValue*);
typedef struct Nba97GameCameraContext {
    Nba97GameTextMemory memory;
    size_t access_budget;
    Nba97GameCameraIo io;void* user;
    Nba97PlayerMath math;void* math_user;
} Nba97GameCameraContext;
typedef struct Nba97GameCameraProgress {
    size_t accesses,events,stores,callbacks,math_calls;
    uint32_t stopped_pc,stopped_address,return_v0;
    int math_result;
    uint8_t completed;
} Nba97GameCameraProgress;
/* Original numeric-address memory contract from game_text_objects.h. Source
 * code/private CPU stack do not alias mutable regions. Actual global slots,
 * tables, resources, scratchpad and original-address provenance are required.
 * Canonical byte knownness is preserved. Journal/access exhaustion or unowned
 * effects retain the exact completed prefix; clone all memory AND geometry
 * for atomic publication, never rerun a partly applied call as a continuation.
 * Math uses the existing named native fixed-point operations; it cannot mutate
 * memory. Original scratch stack is represented by typed local values.
 * These entry points do not manufacture loader/input/camera state.
 */
int nba97_game_camera_input_8f224(Nba97GameCameraContext*,uint32_t controller,
    Nba97GameCameraEvent*,size_t,Nba97GameCameraProgress*);
int nba97_game_camera_controller(Nba97GameCameraContext*,Nba97GameCameraEvent*,size_t,Nba97GameCameraProgress*);
int nba97_game_camera(Nba97GameCameraContext*,Nba97GameCameraEvent*,size_t,Nba97GameCameraProgress*);
#ifdef __cplusplus
}
#endif
#endif
