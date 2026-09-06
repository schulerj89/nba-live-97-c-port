#include "game_rule_delay.h"

#include <string.h>

#define RULE_DELAY_JR_PC UINT32_C(0x800295c8)

static int machine_valid(const Nba97GameRuleDelayMachine* machine) {
    unsigned i;
    if (machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask !=
            0x0fu ||
        machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (machine->registers.gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

int nba97_game_rule_delay(Nba97GameRuleDelayContext* context,
    Nba97GameRuleDelayProgress* out) {
    Nba97GameRuleDelayWord ra;
    if (!out)
        return NBA97_TEXT_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!context || !machine_valid(&context->machine))
        return NBA97_TEXT_ARGUMENT;

    out->machine = context->machine;
    ra = context->machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA];
    out->return_address = ra;

    /* GAMEONLY 0x800295C8..0x800295CC: JR consumes live ra and its NOP
     * delay slot leaves the complete machine untouched. The source performs
     * no alignment check, even when the fully-known target is unaligned. */
    if (ra.known_mask != 0x0fu) {
        out->stopped_pc = RULE_DELAY_JR_PC;
        out->stopped_address = ra.word;
        return NBA97_TEXT_UNKNOWN;
    }

    out->completed = 1;
    return NBA97_TEXT_COMPLETE;
}
