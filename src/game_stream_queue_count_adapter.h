#ifndef NBA97_GAME_STREAM_QUEUE_COUNT_ADAPTER_H
#define NBA97_GAME_STREAM_QUEUE_COUNT_ADAPTER_H

#include "recovered/game_stream_queue_count.h"
#include "recovered/game_stream_readiness.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameStreamQueueCountAdapterProgress {
    Nba97GameStreamQueueCountProgress queue_count;
    Nba97GameStreamReadinessEvent queue_event;
    int queue_result;
    int readiness_result;
    size_t queue_invocations;
    size_t queue_completions;
    size_t unresolved_callbacks_completed;
} Nba97GameStreamQueueCountAdapterProgress;

/* Bind only AD's exact no-argument 0x80088D30 -> 0x80084448 event. The live
 * parent machine must already contain JAL ra=0x80088D38 and its NOP delay. */
int nba97_game_stream_queue_count_from_stream_readiness(
    const Nba97GameTextMemory*, const Nba97GameStreamReadinessEvent*,
    Nba97GameStreamReadinessMachine*,
    const Nba97GameStreamQueueCountContext*,
    Nba97GameStreamQueueCountProgress*);

/* Execute the actual recovered AD owner with AG at its natural child event.
 * AD has no other child, while AG's lock/unlock services remain its caller's
 * explicit typed full-machine callbacks. */
int nba97_game_stream_readiness_with_queue_count(
    const Nba97GameStreamReadinessContext*,
    const Nba97GameStreamQueueCountContext*,
    Nba97GameStreamReadinessProgress*,
    Nba97GameStreamQueueCountAdapterProgress*);

#ifdef __cplusplus
}
#endif
#endif
