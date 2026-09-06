#include "game_frame_ui_service_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct ParentRun {
  const Nba97MatchTickContext *parent;
  Nba97GameFrameUiServiceBinding *binding;
};

bool machineValid(const Nba97GameFrameUiServiceMachine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 15 || machine.hi.known_mask > 15 ||
      machine.lo.known_mask > 15)
    return false;
  for (unsigned i = 0; i < 32; ++i)
    if (machine.registers.gpr[i].known_mask > 15)
      return false;
  return true;
}

bool memoryValid(const Nba97GameTextMemory &memory) {
  if (!memory.region && memory.count)
    return false;
  for (std::size_t i = 0; i < memory.count; ++i) {
    const auto &a = memory.region[i];
    if (!a.data || !a.size || a.size > UINT64_C(0x100000000) ||
        std::uint64_t(a.base) + a.size > UINT64_C(0x100000000))
      return false;
    for (std::size_t j = 0; j < i; ++j) {
      const auto &b = memory.region[j];
      if (std::uint64_t(a.base) < std::uint64_t(b.base) + b.size &&
          std::uint64_t(b.base) < std::uint64_t(a.base) + a.size)
        return false;
    }
  }
  return true;
}

bool assigned(const Nba97MatchTickCall *event) {
  return event && (event->pc == 0x8002ddacu || event->entry == 0x80032b10u);
}

int parentAccess(void *opaque, std::uint32_t pc, std::uint32_t address,
                 unsigned width, unsigned kind, Nba97PlayerFrameValue *value) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (!run.parent->access)
    return NBA97_BODY_ARGUMENT;
  return run.parent->access(run.parent->user, pc, address, width, kind, value);
}

int parentService(void *opaque, const Nba97MatchTickCall *event,
                  Nba97GamePeriodValue *value) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (assigned(event))
    return nba97_game_frame_ui_service_from_match_tick(run.binding, event,
                                                       value);
  if (!run.parent->service)
    return NBA97_BODY_ARGUMENT;
  return run.parent->service(run.parent->user, event, value);
}

int parentPlayer(void *opaque, std::uint32_t pc) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (!run.parent->player_update)
    return NBA97_BODY_ARGUMENT;
  return run.parent->player_update(run.parent->user, pc);
}

int parentBall(void *opaque, std::uint32_t pc, std::uint32_t pointer) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (!run.parent->ball_simulation)
    return NBA97_BODY_ARGUMENT;
  return run.parent->ball_simulation(run.parent->user, pc, pointer);
}

int parentNet(void *opaque, std::uint32_t pc) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (!run.parent->net_transform)
    return NBA97_BODY_ARGUMENT;
  return run.parent->net_transform(run.parent->user, pc);
}

int parentFrame(void *opaque, std::uint32_t pc) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (!run.parent->match_frame)
    return NBA97_BODY_ARGUMENT;
  return run.parent->match_frame(run.parent->user, pc);
}
} // namespace

int nba97_game_frame_ui_service_from_match_tick(
    void *opaque, const Nba97MatchTickCall *event,
    Nba97GamePeriodValue *unused_result) {
  auto *binding = static_cast<Nba97GameFrameUiServiceBinding *>(opaque);
  (void)unused_result;
  if (!binding || !event || event->pc != 0x8002ddacu ||
      event->entry != 0x80032b10u || event->count != 0 ||
      !binding->explicit_caller_machine ||
      !machineValid(*binding->explicit_caller_machine) ||
      binding->explicit_caller_machine->registers.gpr[31].known_mask != 15 ||
      binding->explicit_caller_machine->registers.gpr[31].word != 0x8002ddb4u ||
      !memoryValid(binding->memory) ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return NBA97_BODY_ARGUMENT;
  }

  ++binding->invocations;
  binding->event = *event;
  Nba97GameFrameUiServiceContext context{};
  context.memory = binding->memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *binding->explicit_caller_machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result = nba97_game_frame_ui_service(&context, &binding->progress);
  if (binding->result == NBA97_TEXT_COMPLETE)
    ++binding->completions;
  return binding->result;
}

int nba97_game_match_tick_with_frame_ui_service(
    const Nba97MatchTickContext *parent,
    Nba97GameFrameUiServiceBinding *binding, Nba97MatchTickProgress *progress) {
  if (!parent || !binding || !progress)
    return NBA97_BODY_ARGUMENT;
  std::memset(&binding->event, 0, sizeof binding->event);
  std::memset(&binding->progress, 0, sizeof binding->progress);
  binding->result = NBA97_TEXT_COMPLETE;
  ParentRun run{parent, binding};
  Nba97MatchTickContext context = *parent;
  context.access = parent->access ? parentAccess : nullptr;
  context.service = parent->service ? parentService : nullptr;
  context.player_update = parent->player_update ? parentPlayer : nullptr;
  context.ball_simulation = parent->ball_simulation ? parentBall : nullptr;
  context.net_transform = parent->net_transform ? parentNet : nullptr;
  context.match_frame = parent->match_frame ? parentFrame : nullptr;
  context.user = &run;
  return nba97_game_match_tick(&context, progress);
}
