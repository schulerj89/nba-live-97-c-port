#ifndef NBA97_GAME_DRAW_PACKET_ADAPTER_H
#define NBA97_GAME_DRAW_PACKET_ADAPTER_H

#include "recovered/game_draw_environment.h"
#include "recovered/game_draw_packet.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameDrawPacketEnvironmentBinding {
  size_t operation_budget;
  Nba97GameDrawPacketIo io;
  void *user;
  Nba97GameDrawPacketAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameDrawEnvironmentIo fallback;
  void *fallback_user;
  Nba97GameDrawPacketProgress progress;
  int result;
  size_t invocations;
  size_t fallback_invocations;
} Nba97GameDrawPacketEnvironmentBinding;

void nba97_game_draw_packet_environment_binding_init(
    Nba97GameDrawPacketEnvironmentBinding *, size_t operation_budget,
    Nba97GameDrawPacketIo, void *user, Nba97GameDrawPacketAccess *,
    size_t access_journal_capacity, Nba97GameDrawEnvironmentIo fallback,
    void *fallback_user);

int nba97_game_draw_packet_from_draw_environment(
    void *, const Nba97GameTextMemory *, const Nba97GameDrawEnvironmentEvent *,
    Nba97GameDrawEnvironmentMachine *);

#ifdef __cplusplus
}
#endif
#endif
