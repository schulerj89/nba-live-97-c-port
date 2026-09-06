#ifndef NBA97_GAME_TIPOFF_ANNOUNCEMENT_H
#define NBA97_GAME_TIPOFF_ANNOUNCEMENT_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameTipoffAnnouncementWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameTipoffAnnouncementRegisters;

enum Nba97GameTipoffAnnouncementRegister {
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_S1 = NBA97_MATCH_INITIALIZE_S0 + 1,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_S2 = NBA97_MATCH_INITIALIZE_S0 + 2
};

enum Nba97GameTipoffAnnouncementCallKind {
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_800887E8 = 1,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_8007FA50,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_8007EEA8,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_8007FA9C,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_8007ECA4,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_800B1E14,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_80083748,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_8007ECEC,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_8007E8C4,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_CALL_KIND_COUNT
};

typedef struct Nba97GameTipoffAnnouncementEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation; /* One-based mapped-access/call order. */
    size_t invocation; /* One-based invocation of this call kind. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameTipoffAnnouncementEvent;

/* The callback sees all 32 GPRs after JAL assigns ra and its delay slot has
 * executed. It may mutate retained memory and every GPR. Return 1 only after
 * the original child boundary has returned. */
typedef int (*Nba97GameTipoffAnnouncementIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameTipoffAnnouncementEvent*,
    Nba97GameTipoffAnnouncementRegisters*);

enum Nba97GameTipoffAnnouncementAccessKind {
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_READ = 1,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_STORE = 2
};

typedef struct Nba97GameTipoffAnnouncementAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask; /* One bit per little-endian source byte. */
    uint8_t kind;
} Nba97GameTipoffAnnouncementAccess;

typedef struct Nba97GameTipoffAnnouncementContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameTipoffAnnouncementRegisters registers;
    Nba97GameTipoffAnnouncementIo io;
    void* user;
    Nba97GameTipoffAnnouncementAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameTipoffAnnouncementContext;

typedef struct Nba97GameTipoffAnnouncementProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_count[NBA97_GAME_TIPOFF_ANNOUNCEMENT_CALL_KIND_COUNT];
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameTipoffAnnouncementWord gate;
    Nba97GameTipoffAnnouncementWord mode;
    Nba97GameTipoffAnnouncementWord restored_return_address;
    Nba97GameTipoffAnnouncementWord restored_s2;
    Nba97GameTipoffAnnouncementWord restored_s1;
    Nba97GameTipoffAnnouncementWord restored_s0;
    Nba97GameTipoffAnnouncementRegisters registers;
    uint8_t mode_path; /* 2 for mode 2, 1 for mode 1, otherwise 0. */
    uint8_t completed;
} Nba97GameTipoffAnnouncementProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8007EF4C
 * Range: 0x8007EF4C..0x8007F073 (inclusive)
 * Source size: 296 bytes / 74 instructions
 * Evidence: fresh Ghidra game_8007ef4c.txt; routine SHA-256 ae33453495d7aee1dd8f89080731150e43cd7c65a72b5e2151d213dd0172473d
 *
 * Purpose: Gate and assemble the first-period speech announcement using live mode, roster, and announcement-service results.
 * Inputs: All 32 live MIPS GPRs, retained stack memory, unsigned mode byte 0x80021D70, mode-2 words 0x80021D74/0x80021D78, signed mode-1 flag 0x8001EC94, and nine typed child services.
 * Returns: Final child/live GPRs with ra/s2/s1/s0 reloaded through child-mutable live sp, sp advanced by 0x20, and restored ra consumed by JR.
 * Guest memory: Saves ra/s2/s1/s0 at frame+0x1C/+0x18/+0x14/+0x10, conditionally reads 0x80021D70, 0x80021D74, 0x80021D78, and 0x8001EC94 in source order, then reloads the four saved words through live sp.
 * Calls: 0x800887E8; gated 0x8007FA50; mode-dependent 0x8007EEA8, 0x8007FA9C, 0x8007ECA4, 0x800B1E14, 0x80083748, 0x8007ECEC, and 0x8007E8C4 in exact source order.
 * Original quirks: The first result uses signed v0<8; mode 1 tests a signed word >0; mode 2 captures the first 0x80083748 result in the second call delay then wrapping-adds the second result in the next JAL delay; every child may replace saved registers, sp, and live temporaries.
 * Native mapping: Guest addresses stay uint32_t and use validated retained regions with per-byte knownness; unresolved children are full-GPR typed callbacks and no host pointer or fabricated service result is used.
 */
int nba97_game_tipoff_announcement(
    Nba97GameTipoffAnnouncementContext*,
    Nba97GameTipoffAnnouncementProgress*);

#ifdef __cplusplus
}
#endif
#endif
