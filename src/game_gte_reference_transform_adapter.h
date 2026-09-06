#ifndef NBA97_GAME_GTE_REFERENCE_TRANSFORM_ADAPTER_H
#define NBA97_GAME_GTE_REFERENCE_TRANSFORM_ADAPTER_H

#include "recovered/game_camera_frame_transform.h"
#include "recovered/game_gte_reference_transform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameGteReferenceTransformGeometryBinding {
  void *geometry;
  int result;
  size_t invocations;
} Nba97GameGteReferenceTransformGeometryBinding;

typedef struct Nba97GameGteReferenceTransformCameraBinding {
  Nba97GameGteReferenceTransformState state;
  size_t operation_budget;
  Nba97GameGteReferenceTransformHardware hardware;
  void *hardware_user;
  Nba97GameGteReferenceTransformAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameCameraFrameTransformIo fallback;
  void *fallback_user;
  Nba97GameGteReferenceTransformProgress progress;
  int result;
  size_t invocations;
  size_t fallback_invocations;
} Nba97GameGteReferenceTransformCameraBinding;

/* geometry must point to nba97::GamePlayerGeometry. */
void nba97_game_gte_reference_transform_geometry_binding_init(
    Nba97GameGteReferenceTransformGeometryBinding *binding, void *geometry);

int nba97_game_gte_reference_transform_geometry_hardware(
    void *user, const Nba97GameGteReferenceTransformHardwareEvent *event,
    Nba97GameGteReferenceTransformState *state);

void nba97_game_gte_reference_transform_camera_binding_init(
    Nba97GameGteReferenceTransformCameraBinding *binding,
    const Nba97GameGteReferenceTransformState *state, size_t operation_budget,
    Nba97GameGteReferenceTransformHardware hardware, void *hardware_user,
    Nba97GameGteReferenceTransformAccess *access_journal,
    size_t access_journal_capacity, Nba97GameCameraFrameTransformIo fallback,
    void *fallback_user);

int nba97_game_gte_reference_transform_from_camera_frame(
    void *user, const Nba97GameTextMemory *memory,
    const Nba97GameCameraFrameTransformEvent *event,
    Nba97GameCameraFrameTransformMachine *machine);

#ifdef __cplusplus
}
#endif
#endif
