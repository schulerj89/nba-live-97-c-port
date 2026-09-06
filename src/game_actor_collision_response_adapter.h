#ifndef NBA97_GAME_ACTOR_COLLISION_RESPONSE_ADAPTER_H
#define NBA97_GAME_ACTOR_COLLISION_RESPONSE_ADAPTER_H

#include "recovered/game_actor_collision_response.h"
#include "recovered/game_opponent_contact.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameActorCollisionResponseBinding {
  size_t operation_budget;
  Nba97GameActorCollisionResponseIo io;
  void *user;
  Nba97GameActorCollisionResponseAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameActorCollisionResponseProgress progress;
  int result;
  size_t invocations;
} Nba97GameActorCollisionResponseBinding;

typedef struct Nba97GameActorCollisionResponseGeometryBinding {
  Nba97GameActorCollisionResponseIo fallback;
  void *fallback_user;
  int result;
  size_t geometry_invocations;
  size_t fallback_invocations;
} Nba97GameActorCollisionResponseGeometryBinding;

void nba97_game_actor_collision_response_binding_init(
    Nba97GameActorCollisionResponseBinding *, size_t,
    Nba97GameActorCollisionResponseIo, void *,
    Nba97GameActorCollisionResponseAccess *, size_t);

int nba97_game_actor_collision_response_from_opponent_contact(
    void *, const Nba97GameTextMemory *, const Nba97GameOpponentContactEvent *,
    Nba97GameOpponentContactMachine *);

void nba97_game_actor_collision_response_geometry_binding_init(
    Nba97GameActorCollisionResponseGeometryBinding *,
    Nba97GameActorCollisionResponseIo, void *);

int nba97_game_actor_collision_response_geometry_child(
    void *, const Nba97GameTextMemory *,
    const Nba97GameActorCollisionResponseEvent *,
    Nba97GameActorCollisionResponseMachine *);

#ifdef __cplusplus
}
#endif
#endif
