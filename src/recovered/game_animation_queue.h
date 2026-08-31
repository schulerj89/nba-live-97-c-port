#ifndef NBA97_GAME_ANIMATION_QUEUE_H
#define NBA97_GAME_ANIMATION_QUEUE_H
#include "game_animation_advance.h"
#ifdef __cplusplus
extern "C" {
#endif
enum Nba97GameAnimationQueueOperation {
    NBA97_ANIMATION_QUEUE_BOTH_56CE0=0,
    NBA97_ANIMATION_QUEUE_PRIMARY_56C28=1,
    NBA97_ANIMATION_QUEUE_SECONDARY_56C84=2
};
typedef struct Nba97GameAnimationQueueInput {
    Nba97GameAnimationActor previous;
    uint32_t request,blend; /* Original a1/a2; stores take low16/low8 bits. */
    uint8_t operation;
} Nba97GameAnimationQueueInput;
/* Complete56CE0: secondary then primary. Each negative signed lock independently
 * appends at the first EXACTFFFF sentinel among four slots; other negative clips
 * are occupied. Full queues silently drop. A nonlast append writes the nextFFFF
 * sentinel without clearing its auxiliary byte or later slots. Locked channels
 * do not inspect their queue, and the channels can diverge. No clip lookup occurs.
 * Queue heads70/78 are in animation; remaining slots are in extra. Atomic effects
 * and explicit unknown fields follow the shared animation contract. */
int nba97_game_animation_queue(Nba97GameAnimationEffects* out,
                              const Nba97GameAnimationQueueInput* input);
#ifdef __cplusplus
}
#endif
#endif
