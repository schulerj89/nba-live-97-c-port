#ifndef NBA97_GAME_RULE_DELAY_H
#define NBA97_GAME_RULE_DELAY_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameRuleDelayWord;
typedef Nba97GameMatchClocksMachine Nba97GameRuleDelayMachine;

typedef struct Nba97GameRuleDelayContext {
    Nba97GameRuleDelayMachine machine;
} Nba97GameRuleDelayContext;

typedef struct Nba97GameRuleDelayProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    Nba97GameRuleDelayWord return_address;
    Nba97GameRuleDelayMachine machine;
    uint8_t completed;
} Nba97GameRuleDelayProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800295C8
 * Range: 0x800295C8..0x800295CF (inclusive)
 * Source size: 8 bytes / 2 instructions
 * Evidence: fresh Ghidra game_800295c8.txt; instruction SHA-256 6d64edf91449c1b17746c1ef18afa2eb25c70bdf1322ab3df5a2630993b7e2f1
 *
 * Purpose: Return immediately from a rule-effect delay boundary whose source body is a no-op.
 * Inputs: All 32 live MIPS GPRs, HI/LO, and live ra for JR; a0 is ignored.
 * Returns: Every GPR and HI/LO unchanged, with live ra consumed only as the JR target.
 * Guest memory: None observed.
 * Calls: None observed.
 * Original quirks: The caller-supplied duration causes no wait, clock read, alignment check, memory access, or state change; unknown ra is reported only after the NOP delay slot.
 * Native mapping: The complete full-machine value and per-byte knownness pass through directly; no guest address is treated as a host pointer.
 */
int nba97_game_rule_delay(
    Nba97GameRuleDelayContext*, Nba97GameRuleDelayProgress*);

#ifdef __cplusplus
}
#endif
#endif
