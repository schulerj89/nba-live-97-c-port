#ifndef NBA97_GAME_NET_H
#define NBA97_GAME_NET_H
#include "game_player_frame.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {NBA97_NET_AVERAGE_FOUR=32,NBA97_NET_VECTOR_BASE=64,NBA97_NET_CODEC_REQUIRED=-13};
typedef struct Nba97GameNetProgress {
    size_t operations,reads,stores,math_calls,decodes,initializations,links;
    uint32_t stopped_pc,stopped_address;uint8_t completed;
} Nba97GameNetProgress;
/* Uses FrameAccess's exact original-address, byte-knowledge and alias contract;
 * child is unused. math shares live player/court geometry. NET_VECTOR_BASE+
 * PLAYER_* names the existing MVMVA operations; AVERAGE_FOUR consumes actual
 * retained ZSF4. No camera, bank, packet, resource or previous state is invented.
 * Source stack/code must not alias visible allocations. Each refusal retains
 * all prior CPU and geometry effects; progress is not a resumable original PC.
 */
int nba97_game_net_frame(Nba97PlayerFrameContext*,Nba97GameNetProgress*);
int nba97_game_net_initialize(Nba97PlayerFrameContext*,Nba97GameNetProgress*);
int nba97_game_net_draw(Nba97PlayerFrameContext*,Nba97GameNetProgress*);
/* Full A4744/A464C dispatch, with the actual ZNET10FB/11FB AA168 codec owned.
 * Other selected decoder entries explicitly return CODEC_REQUIRED at their
 * reached JAL. Invalid signatures/unsupported selector values retain original
 * zero-return behavior. The raw live jump table at288B4 is required, not an
 * inferred default. The stream's declared output length is RETURNED, not an
 * output bound: AA168 stops on its command, even if the length disagrees.
 * Opaque literal/backreference bytes preserve unknownness; control bytes must
 * be known. Memory and operation bounds are native guards on original reads
 * and writes. Caller must map actual compressed ZNET and destination ownership.
 */
int nba97_game_net_decode(Nba97PlayerFrameContext*,uint32_t source,uint32_t destination,
    Nba97GamePeriodValue* declared_length,Nba97GameNetProgress*);
#ifdef __cplusplus
}
#endif
#endif
