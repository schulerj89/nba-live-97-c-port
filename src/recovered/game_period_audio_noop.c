#include "game_period_audio_noop.h"

#include <string.h>

#define JR_PC UINT32_C(0x8002a254)

static int machine_valid(const Nba97GamePeriodAudioNoopMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

int nba97_game_period_audio_noop(
    Nba97GamePeriodAudioNoopContext *context,
    Nba97GamePeriodAudioNoopProgress *out) {
  Nba97GamePeriodAudioNoopWord ra;
  if (!out)
    return NBA97_TEXT_ARGUMENT;
  memset(out, 0, sizeof *out);
  if (!context || !machine_valid(&context->machine))
    return NBA97_TEXT_ARGUMENT;
  out->machine = context->machine;
  ra = context->machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA];
  out->return_address = ra;

  /* GAMEONLY 0x8002A254..0x8002A258: JR consumes live ra, then its NOP delay
   * leaves v0, ignored arguments, every other GPR, and HI/LO untouched. */
  if (ra.known_mask != 0x0fu) {
    out->stopped_pc = JR_PC;
    out->stopped_address = ra.word;
    return NBA97_TEXT_UNKNOWN;
  }
  if (ra.word & 3u) {
    out->stopped_pc = JR_PC;
    out->stopped_address = ra.word;
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  return NBA97_TEXT_COMPLETE;
}
