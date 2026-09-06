#ifndef NBA97_GAME_TEXTURE_WINDOW_COMMAND_ADAPTER_H
#define NBA97_GAME_TEXTURE_WINDOW_COMMAND_ADAPTER_H

#include "recovered/game_draw_packet.h"
#include "recovered/game_texture_window_command.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameTextureWindowCommandPacketBinding {
  size_t operation_budget;
  Nba97GameTextureWindowCommandAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameDrawPacketIo fallback;
  void *fallback_user;
  Nba97GameTextureWindowCommandProgress progress;
  Nba97GameDrawPacketEvent event;
  int result;
  size_t invocations;
  size_t completions;
  size_t fallback_invocations;
} Nba97GameTextureWindowCommandPacketBinding;

void nba97_game_texture_window_command_packet_binding_init(
    Nba97GameTextureWindowCommandPacketBinding *, size_t operation_budget,
    Nba97GameTextureWindowCommandAccess *, size_t access_journal_capacity,
    Nba97GameDrawPacketIo fallback, void *fallback_user);

int nba97_game_texture_window_command_from_packet(
    void *, const Nba97GameTextMemory *, const Nba97GameDrawPacketEvent *,
    Nba97GameDrawPacketMachine *);

#ifdef __cplusplus
}
#endif
#endif
