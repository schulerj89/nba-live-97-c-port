#ifndef NBA97_GAME_POSE_REQUEST_H
#define NBA97_GAME_POSE_REQUEST_H
#include "game_pose_sample.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum Nba97GamePoseHalf {
    NBA97_POSE_46, NBA97_POSE_48, NBA97_POSE_4A, NBA97_POSE_4C,
    NBA97_POSE_50, NBA97_POSE_52, NBA97_POSE_54, NBA97_POSE_56,
    NBA97_POSE_60, NBA97_POSE_62, NBA97_POSE_64, NBA97_POSE_66,
    NBA97_POSE_84, NBA97_POSE_86, NBA97_POSE_88, NBA97_POSE_8A,
    NBA97_POSE_8C, NBA97_POSE_8E, NBA97_POSE_90, NBA97_POSE_92,
    NBA97_POSE_94, NBA97_POSE_96, NBA97_POSE_9A, NBA97_POSE_EC,
    NBA97_POSE_C6, NBA97_POSE_A8, NBA97_POSE_HALF_COUNT
} Nba97GamePoseHalf;
typedef enum Nba97GamePoseWord {
    NBA97_POSE_08, NBA97_POSE_0C, NBA97_POSE_10,
    NBA97_POSE_30, NBA97_POSE_34, NBA97_POSE_WORD_COUNT
} Nba97GamePoseWord;
typedef struct Nba97GamePoseEntity {
    uint16_t half[NBA97_POSE_HALF_COUNT];
    uint32_t half_known;
    int32_t word[NBA97_POSE_WORD_COUNT];
    uint32_t word_known;
    uint8_t foot_e0, foot_e0_known;
} Nba97GamePoseEntity;

/* Synchronous, read-only original2D76C boundary. Return nonzero only when the
 * output is known. The supplied state already contains request/cache updates and
 * E0/EC leg reset. Do not mutate entities/index behind this const interface.
 * nba97_game_foot_offset closes this with owned ZHOTS + actual GAME trig bytes. */
typedef int (*Nba97GamePoseFootCallback)(void* context, unsigned physical_entity,
    const Nba97GamePoseEntity* entity, unsigned leg, Nba97GameFootOffset* out);

/* Full57B18 owner: ten physical F4 entities beginning at *20BEC, not ten table
 * lookups. Caller resolves that contiguous span and copies fields with explicit
 * knownness; a zero payload is never evidence of initialization. Applies live
 * source writes in order; untouched fields/knownness are retained. Unknown inputs,
 * invalid motion references, and unavailable foot results are native guards,
 * not original branches. On such stops the completed prefix stays applied; do
 * not blindly retry as if this were a resumable cursor. completed counts wholly
 * completed physical entities. The failing entity may have partial writes.
 * Original quirks: negative previous IDs mean absent; each channel has its own
 * flags/count; only exact next-logical==count wraps interpolation to0; no modulo;
 * signed EC after halfword overflow decides foot lock; unused B-frame/weight
 * fields remain untouched. No animation advance or synthetic initial state. */
Nba97GamePoseResult nba97_game_pose_requests(const Nba97GameMocapIndex* index,
    Nba97GamePoseEntity entities[10], Nba97GamePoseFootCallback foot,
    void* context, unsigned* completed);

/* Extract only fields actually consumed by the render-value sampler; inactive
 * B frame/weight may remain unknown. Output unchanged on failure. */
Nba97GamePoseResult nba97_game_pose_packet(const Nba97GamePoseEntity* entity,
                                         Nba97GamePosePacket* out);
#ifdef __cplusplus
}
#endif
#endif
