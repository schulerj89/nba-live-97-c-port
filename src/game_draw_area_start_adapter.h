#ifndef NBA97_GAME_DRAW_AREA_START_ADAPTER_H
#define NBA97_GAME_DRAW_AREA_START_ADAPTER_H

#include "recovered/game_draw_area_start.h"
#include "recovered/game_draw_packet.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameDrawAreaStartPacketBinding {
  size_t operation_budget;
  Nba97GameDrawAreaStartAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameDrawPacketIo fallback;
  void *fallback_user;
  Nba97GameDrawAreaStartProgress progress;
  Nba97GameDrawPacketEvent event;
  int result;
  size_t invocations;
  size_t completions;
  size_t fallback_invocations;
} Nba97GameDrawAreaStartPacketBinding;

void nba97_game_draw_area_start_packet_binding_init(
    Nba97GameDrawAreaStartPacketBinding *, size_t operation_budget,
    Nba97GameDrawAreaStartAccess *, size_t access_journal_capacity,
    Nba97GameDrawPacketIo fallback, void *fallback_user);

int nba97_game_draw_area_start_from_packet(void *, const Nba97GameTextMemory *,
                                           const Nba97GameDrawPacketEvent *,
                                           Nba97GameDrawPacketMachine *);

#ifdef __cplusplus
}
#endif
#endif
