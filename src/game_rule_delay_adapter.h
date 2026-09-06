#ifndef NBA97_GAME_RULE_DELAY_ADAPTER_H
#define NBA97_GAME_RULE_DELAY_ADAPTER_H

#include "recovered/game_clock_violations.h"
#include "recovered/game_rule_delay.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameRuleDelaySite {
    NBA97_GAME_RULE_DELAY_FIRST_VIOLATION = 0,
    NBA97_GAME_RULE_DELAY_PHASE_82_VIOLATION,
    NBA97_GAME_RULE_DELAY_FINAL_VIOLATION,
    NBA97_GAME_RULE_DELAY_SITE_COUNT
};

typedef struct Nba97GameRuleDelayAdapterProgress {
    size_t invocations;
    size_t site_invocations[NBA97_GAME_RULE_DELAY_SITE_COUNT];
    size_t duration_5000_invocations;
    size_t duration_20000_invocations;
    size_t unresolved_callbacks_completed;
    int rule_result;
    Nba97GameClockViolationsEvent event[NBA97_GAME_RULE_DELAY_SITE_COUNT];
    Nba97GameRuleDelayProgress rule[NBA97_GAME_RULE_DELAY_SITE_COUNT];
} Nba97GameRuleDelayAdapterProgress;

/* Execute one source-proven clock-violation call to 0x800295C8. The adapter
 * verifies its call site, JAL link, and exact 5000/20000 a0 value before the
 * leaf ignores a0 and returns the complete machine unchanged. */
int nba97_game_rule_delay_from_clock_violations(
    const Nba97GameClockViolationsEvent*,
    Nba97GameClockViolationsMachine*, Nba97GameRuleDelayProgress*);

/* Run the actual recovered 0x80067D38 parent with all three 0x800295C8 sites
 * composed through the production leaf adapter. Every other child remains
 * the caller's explicit typed fixture. */
int nba97_game_clock_violations_with_rule_delay(
    const Nba97GameClockViolationsContext*,
    Nba97GameClockViolationsProgress*, Nba97GameRuleDelayAdapterProgress*);

#ifdef __cplusplus
}
#endif
#endif
