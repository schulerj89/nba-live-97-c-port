#ifndef NBA97_GAME_POSE_SAMPLE_H
#define NBA97_GAME_POSE_SAMPLE_H
#include "gameplay_mocap.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameEuler { int16_t x, y, z; } Nba97GameEuler;
/* Only values consumed by model composition. GAME530FC leaves scratch joint
 * marker halfwords and secondary root word+0 unwritten when converting/blending.
 * They are deliberately absent; this API never invents their prior contents. */
typedef struct Nba97GamePose {
    Nba97GameEuler joint[20]; /* secondary0..7, primary8..19 */
    int16_t root_height;
} Nba97GamePose;
typedef struct Nba97GamePosePacket {
    uint16_t clip[2][2]; /* entity84,86 / 88,8A; signed-negative B means absent */
    uint16_t frame[2][2]; /* entity8C,8E / 90,92, already physical */
    uint16_t weight[2]; /* entity94,96, full unsigned range; no clamp */
    uint16_t conversion; /* entity9A bits1,2,4,8 */
} Nba97GamePosePacket;
typedef enum Nba97GamePoseResult {
    NBA97_GAME_POSE_OK = 0,
    NBA97_GAME_POSE_ARGUMENT,
    NBA97_GAME_POSE_REFERENCE,
    NBA97_GAME_POSE_EXTENT,
    NBA97_GAME_POSE_UNKNOWN,
    NBA97_GAME_POSE_FOOT_REQUIRED
} Nba97GamePoseResult;

/* GAME55018: adjusts A, using the exact asymmetric finite candidate search and
 * strict ties, then interpolates toward B. Weight128 has a source midpoint path.
 * Deliberately does not normalize angles or clamp weight (original quirks). */
Nba97GameEuler nba97_game_euler_blend(Nba97GameEuler a, Nba97GameEuler b,
                                    uint16_t weight);
/* GAME530FC render-value projection using GAME54FCC conversion maps. Index must
 * come from nba97_game_mocap_index over these same immutable original bytes.
 * Native safety guards bound each actual read to the file. Source does NOT check
 * physical/logical count here: in-file reads past a clip's declared count remain
 * possible, including mode2's copied frame quirk. No modulo, marker validation,
 * paired-count assumption or frontend clip limit. Output unchanged on failure.
 * Inputs/output must be nonnull and disjoint (ordinary C caller contract). */
Nba97GamePoseResult nba97_game_pose_sample(const uint8_t* bytes, size_t size,
    const Nba97GameMocapIndex* index, const Nba97GamePosePacket* packet,
    Nba97GamePose* out);

/* GAME66F88's table projection only (not loader retries/temporary height writes).
 * Optional directory entries contribute0. Prefix stores are unsigned halfwords. */
Nba97GamePoseResult nba97_game_foot_prefixes(const Nba97GameMocapIndex* index,
    uint16_t out[84], uint32_t* rows);
typedef struct Nba97GameFootInput {
    uint16_t clip4a, frame54, scale_c6, conversion9a;
    int16_t angle_a8;
    int32_t height10;
    uint32_t leg; /* source zero uses row+6; every nonzero uses row+9 */
} Nba97GameFootInput;
typedef struct Nba97GameFootOffset { int32_t x, z, height; } Nba97GameFootOffset;
/* Complete2D76C + AA814 + AA788 arithmetic. ZHOTS is raw bytes, NOT mocap or
 * inferred skeleton feet. Trig is257 little-endian signed words from GAME D6E30.
 * Read guards bound actual row bytes only; original has no per-clip frame guard.
 * Output unchanged on failure. All resources retained by the caller. */
Nba97GamePoseResult nba97_game_foot_offset(const uint8_t* zhots, size_t size,
    const uint16_t prefixes[84], const uint8_t* trig, size_t trig_size,
    const Nba97GameFootInput* input, Nba97GameFootOffset* out);
#ifdef __cplusplus
}
#endif
#endif
