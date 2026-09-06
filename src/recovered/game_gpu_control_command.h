#ifndef NBA97_GAME_GPU_CONTROL_COMMAND_H
#define NBA97_GAME_GPU_CONTROL_COMMAND_H

#include "game_display_environment.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameDisplayEnvironmentWord Nba97GameGpuControlCommandWord;
typedef Nba97GameDisplayEnvironmentMachine Nba97GameGpuControlCommandMachine;

enum Nba97GameGpuControlCommandAccessKind {
  NBA97_GAME_GPU_CONTROL_COMMAND_READ = 1,
  NBA97_GAME_GPU_CONTROL_COMMAND_STORE
};

typedef struct Nba97GameGpuControlCommandAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97GameGpuControlCommandAccess;

typedef struct Nba97GameGpuControlCommandContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameGpuControlCommandMachine machine;
  Nba97GameGpuControlCommandAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameGpuControlCommandContext;

typedef struct Nba97GameGpuControlCommandProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  Nba97GameGpuControlCommandWord port_pointer;
  Nba97GameGpuControlCommandWord command_byte;
  Nba97GameGpuControlCommandWord cache_address;
  Nba97GameGpuControlCommandMachine machine;
  uint8_t completed;
} Nba97GameGpuControlCommandProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8009B16C
 * Range: 0x8009B16C..0x8009B193 (inclusive)
 * Source size: 40 bytes / 10 instructions
 * Evidence: fresh Ghidra game_8009b16c.txt; instruction-byte SHA-256 43224c6b6612d2c3440ea6e3a16f7e74f9b0d9711b10aba909f1d09cd97a73f2
 *
 * Purpose: Write one GP1 control command to the runtime-selected GPU control port and cache its low byte by command class.
 * Inputs: Full live GPR/HI/LO state with the command in a0, ra consumed by JR, retained pointer at 0x800C5694, and mapped destination/cache memory.
 * Returns: v0 is the raw command high byte, at is 0x800E0000 plus that byte, a0 and every other GPR and HI/LO remain unchanged.
 * Guest memory: Reads the 32-bit port pointer at 0x800C5694, stores the full command to that port, then stores the command low byte at 0x800D8D94 plus its high byte.
 * Calls: None observed.
 * Original quirks: The port store precedes index validation and the byte cache store; aliases and partial failures retain that exact prefix.
 * Native mapping: All guest addresses use validated uint32_t retained-memory mappings and per-byte knownness; no host pointer cast or GPU command interpretation occurs.
 */
int nba97_game_gpu_control_command(Nba97GameGpuControlCommandContext*,
    Nba97GameGpuControlCommandProgress*);
// clang-format on

#ifdef __cplusplus
}
#endif

#endif
