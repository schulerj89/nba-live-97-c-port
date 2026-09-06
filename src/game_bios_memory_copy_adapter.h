#ifndef NBA97_GAME_BIOS_MEMORY_COPY_ADAPTER_H
#define NBA97_GAME_BIOS_MEMORY_COPY_ADAPTER_H

#include "recovered/game_bios_memory_copy.h"
#include "recovered/game_speech_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*Nba97GameBiosMemoryCopyHiLoProvider)(
    void *, const Nba97GameSpeechInitializeEvent *,
    const Nba97GameSpeechInitializeRegisters *, Nba97GameBiosMemoryCopyWord *,
    Nba97GameBiosMemoryCopyWord *);

typedef struct Nba97GameBiosMemoryCopySpeechBinding {
  size_t operation_budget;
  Nba97GameBiosMemoryCopyIo bios_io;
  void *bios_user;
  Nba97GameBiosMemoryCopyHiLoProvider hilo_provider;
  void *hilo_user;
  Nba97GameSpeechInitializeIo fallback;
  void *fallback_user;
  Nba97GameBiosMemoryCopyProgress progress;
  int result;
  size_t invocations;
  size_t provider_invocations;
} Nba97GameBiosMemoryCopySpeechBinding;

void nba97_game_bios_memory_copy_speech_binding_init(
    Nba97GameBiosMemoryCopySpeechBinding *, size_t operation_budget,
    Nba97GameBiosMemoryCopyIo, void *, Nba97GameBiosMemoryCopyHiLoProvider,
    void *, Nba97GameSpeechInitializeIo fallback, void *fallback_user);

int nba97_game_bios_memory_copy_from_speech(
    void *, const Nba97GameTextMemory *, const Nba97GameSpeechInitializeEvent *,
    Nba97GameSpeechInitializeRegisters *);

int nba97_game_bios_memory_copy_with_speech(
    const Nba97GameSpeechInitializeContext *,
    Nba97GameBiosMemoryCopySpeechBinding *,
    Nba97GameSpeechInitializeProgress *);

#ifdef __cplusplus
}
#endif
#endif
