#ifndef NBA97_GAME_CAMERA_OVERLAY_PACKETS_H
#define NBA97_GAME_CAMERA_OVERLAY_PACKETS_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameCameraOverlayPacketsWord;
typedef Nba97GameMatchClocksMachine Nba97GameCameraOverlayPacketsMachine;
typedef Nba97GameMatchClocksAccess Nba97GameCameraOverlayPacketsAccess;

enum Nba97GameCameraOverlayPacketsCallKind {
  NBA97_GAME_CAMERA_OVERLAY_PACKETS_LINK_80056914 = 1,
  NBA97_GAME_CAMERA_OVERLAY_PACKETS_SET_ROTATION_80055F18,
  NBA97_GAME_CAMERA_OVERLAY_PACKETS_SET_TRANSLATION_80055F44,
  NBA97_GAME_CAMERA_OVERLAY_PACKETS_PROJECT_QUAD_80055FE4,
  NBA97_GAME_CAMERA_OVERLAY_PACKETS_CALL_KIND_COUNT
};

typedef struct Nba97GameCameraOverlayPacketsEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameCameraOverlayPacketsEvent;

/* The callback observes JAL's ra and the completed delay slot with the whole
 * mutable machine and retained memory. Return NBA97_TEXT_COMPLETE only after
 * the original child boundary returns; every other status is propagated. */
typedef int (*Nba97GameCameraOverlayPacketsIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GameCameraOverlayPacketsEvent *,
    Nba97GameCameraOverlayPacketsMachine *);

typedef struct Nba97GameCameraOverlayPacketsContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameCameraOverlayPacketsMachine machine;
  Nba97GameCameraOverlayPacketsIo io;
  void *user;
  Nba97GameCameraOverlayPacketsAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameCameraOverlayPacketsContext;

typedef struct Nba97GameCameraOverlayPacketsProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_count[NBA97_GAME_CAMERA_OVERLAY_PACKETS_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GameCameraOverlayPacketsWord restored_return_address;
  Nba97GameCameraOverlayPacketsWord restored_s3;
  Nba97GameCameraOverlayPacketsWord restored_s2;
  Nba97GameCameraOverlayPacketsWord restored_s1;
  Nba97GameCameraOverlayPacketsWord restored_s0;
  Nba97GameCameraOverlayPacketsMachine machine;
  uint8_t completed;
} Nba97GameCameraOverlayPacketsProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80075D40
 * Range: 0x80075D40..0x80076273 (inclusive)
 * Source size: 1332 bytes / 333 instructions
 * Evidence: fresh Ghidra game_80075d40.txt; instruction SHA-256
 * fa9289782279377d89580a8cfb6df571849596abd97bc4922d1fb24dcb93c2f8
 *
 * Purpose: Prepare and link camera overlay packets and an optional projected
 * quad.
 * Inputs: Full 32-GPR/HI-LO machine, retained stack, overlay globals, tables,
 * packets, and typed children.
 * Returns: Preserves source-live child effects, restores ra/s3/s2/s1/s0
 * through live sp, adds 0x48 to sp, and consumes restored ra in JR.
 * Guest memory: Reads live selection, bank, mask, camera and table data;
 * writes rectangle fields, quad vertices, ten call arguments, and the source
 * stack frame.
 * Calls: 0x80075F14->0x80056914; 0x80076058->0x80056914;
 * 0x80076070->0x80056914; 0x80076080->0x80056914;
 * 0x800760DC->0x80056914; 0x80076108->0x80056914;
 * 0x800761B0->0x80055F18; 0x800761B8->0x80055F44;
 * 0x80076220->0x80055FE4; 0x8007624C->0x80056914.
 * Original quirks: Signed state/coordinate tests accept negative coordinates,
 * all address arithmetic wraps, the mask loop uses callback-live registers,
 * and the projected call forwards six live stack arguments.
 * Native mapping: Guest addresses use validated uint32_t regions with
 * per-byte knownness; callbacks carry the complete mutable machine and observe
 * JAL delay slots.
 */
int nba97_game_camera_overlay_packets(
    Nba97GameCameraOverlayPacketsContext *,
    Nba97GameCameraOverlayPacketsProgress *);

#ifdef __cplusplus
}
#endif
#endif
