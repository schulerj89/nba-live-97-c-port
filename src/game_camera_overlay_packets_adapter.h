#ifndef NBA97_GAME_CAMERA_OVERLAY_PACKETS_ADAPTER_H
#define NBA97_GAME_CAMERA_OVERLAY_PACKETS_ADAPTER_H

#include "recovered/game_camera_overlay_packets.h"
#include "recovered/game_court_packets.h"
#include "recovered/game_match_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { NBA97_GAME_CAMERA_OVERLAY_PACKETS_MATCH_FRAME_CHILD_INCOMPLETE = -24 };

typedef struct Nba97GameCameraOverlayPacketsChildren {
  Nba97GameCameraOverlayPacketsIo fallback;
  void *fallback_user;
  Nba97CourtProgress link_progress;
  int link_result;
  size_t link_operation_budget;
  size_t links_composed;
} Nba97GameCameraOverlayPacketsChildren;

void nba97_game_camera_overlay_packets_children_init(
    Nba97GameCameraOverlayPacketsChildren *,
    Nba97GameCameraOverlayPacketsIo fallback, void *fallback_user);

int nba97_game_camera_overlay_packets_children_io(
    void *, const Nba97GameTextMemory *,
    const Nba97GameCameraOverlayPacketsEvent *,
    Nba97GameCameraOverlayPacketsMachine *);

typedef struct Nba97GameCameraOverlayPacketsMatchFrameBinding {
  /* 49018 supplies no machine or retained-memory payload at 490C8. These are
   * explicit independent inputs and require the source JAL ra, 0x800490D0. */
  Nba97GameTextMemory memory;
  Nba97GameCameraOverlayPacketsMachine entry_machine;
  size_t operation_budget;
  Nba97GameCameraOverlayPacketsIo io;
  void *user;
  Nba97GameCameraOverlayPacketsAccess *access_journal;
  size_t access_journal_capacity;
  Nba97MatchFrameIo fallback;
  void *fallback_user;
  Nba97GameCameraOverlayPacketsProgress progress;
  int result;
  size_t invocations;
} Nba97GameCameraOverlayPacketsMatchFrameBinding;

void nba97_game_camera_overlay_packets_match_frame_binding_init(
    Nba97GameCameraOverlayPacketsMatchFrameBinding *,
    const Nba97GameTextMemory *,
    const Nba97GameCameraOverlayPacketsMachine *, size_t operation_budget,
    Nba97GameCameraOverlayPacketsIo, void *,
    Nba97GameCameraOverlayPacketsAccess *, size_t,
    Nba97MatchFrameIo fallback, void *fallback_user);

int nba97_game_camera_overlay_packets_from_match_frame(
    void *, const Nba97MatchFrameCall *, Nba97GamePeriodValue *);

#ifdef __cplusplus
}
#endif
#endif
