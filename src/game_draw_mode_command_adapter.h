#ifndef NBA97_GAME_DRAW_MODE_COMMAND_ADAPTER_H
#define NBA97_GAME_DRAW_MODE_COMMAND_ADAPTER_H

#include "recovered/game_draw_mode_command.h"
#include "recovered/game_draw_packet.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameDrawModeCommandPacketBinding {
  size_t operation_budget;
  Nba97GameDrawModeCommandAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameDrawPacketIo fallback;
  void *fallback_user;
  Nba97GameDrawModeCommandProgress progress;
  Nba97GameDrawPacketEvent event;
  int result;
  size_t invocations;
  size_t completions;
  size_t fallback_invocations;
} Nba97GameDrawModeCommandPacketBinding;

void nba97_game_draw_mode_command_packet_binding_init(
    Nba97GameDrawModeCommandPacketBinding *, size_t operation_budget,
    Nba97GameDrawModeCommandAccess *, size_t access_journal_capacity,
    Nba97GameDrawPacketIo fallback, void *fallback_user);

int nba97_game_draw_mode_command_from_packet(void *,
                                              const Nba97GameTextMemory *,
                                              const Nba97GameDrawPacketEvent *,
                                              Nba97GameDrawPacketMachine *);

#ifdef __cplusplus
}
#endif
#endif
