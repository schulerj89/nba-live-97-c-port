#include "game_ball_actor_contact_adapter.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace {
int failures, checks;
void check_at(bool value, int line) {
  ++checks;
  if (!value) {
    ++failures;
    std::cerr << "failed line " << line << "\n";
  }
}
#define check(value) check_at((value), __LINE__)
struct Fixture {
  static constexpr uint32_t ball = 0x80001000, actor = 0x80002000;
  std::vector<uint8_t> bytes = std::vector<uint8_t>(0x200000),
                       known = std::vector<uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{0x80000000, bytes.data(), known.data(),
                             bytes.size()};
  Nba97GameBallActorContactContext context{};
  Nba97GameBallActorContactProgress progress{};
  Nba97GameBallActorContactBinding binding{};
  std::vector<uint32_t> calls, resume_calls;
  int32_t contact = 1;
  uint32_t random_value = 0;
  bool resume_accept = true;
  Fixture() {
    context.memory.region = &region;
    context.memory.count = 1;
    context.operation_budget = 10000;
    context.io = unresolved;
    context.user = this;
    for (auto &g : context.machine.registers.gpr) {
      g.word = 0;
      g.known_mask = 15;
    }
    context.machine.hi.known_mask = context.machine.lo.known_mask = 15;
    context.machine.registers.gpr[29].word = 0x801ff000;
    context.machine.registers.gpr[31].word = 0x80060edc;
    context.machine.registers.gpr[4].word = ball;
    context.machine.registers.gpr[5].word = actor;
    put16(0x800fdbcc, 0xffff);
    put32(0x800fdb58, 1);
    put16(0x800fe8c4, 0);
    put16(0x800fe8cc, 0);
    put16(0x800fdb90, 0);
    put16(0x800fdb94, 0);
    put16(0x800fdbd4, 0);
    put16(0x800fdbd2, 0);
    put16(0x800fdbd0, 0xffff);
    put32(0x800fdc40, 0x8001edf4);
    put32(0x800fdc48, actor);
    put8(actor + 0xd9, 0);
    put16(actor + 4, 0xffff);
    put32(actor + 0x20, 0x80003000);
    put8(0x8000300d, 0);
    binding.actor_resume_io = resume;
    binding.actor_resume_user = this;
    binding.child_operation_budget = 1000;
  }
  void put8(uint32_t a, uint8_t v) { bytes[a - region.base] = v; }
  void put16(uint32_t a, uint16_t v) {
    put8(a, uint8_t(v));
    put8(a + 1, uint8_t(v >> 8));
  }
  void put32(uint32_t a, uint32_t v) {
    put16(a, uint16_t(v));
    put16(a + 2, uint16_t(v >> 16));
  }
  uint16_t get16(uint32_t a) const {
    return uint16_t(bytes[a - region.base] |
                    (uint16_t(bytes[a - region.base + 1]) << 8));
  }
  uint32_t get32(uint32_t a) const {
    return get16(a) | (uint32_t(get16(a + 2)) << 16);
  }
  int run() {
    return nba97_game_ball_actor_contact_run(&context, &progress, &binding);
  }
  static int unresolved(void *u, const Nba97GameTextMemory *,
                        const Nba97GameBallActorContactEvent *e,
                        Nba97GameBallActorContactMachine *m) {
    auto &f = *static_cast<Fixture *>(u);
    f.calls.push_back(e->pc);
    if (e->entry == 0x8007066c || e->entry == 0x8005d140) {
      m->registers.gpr[2].word = 0;
      m->registers.gpr[2].known_mask = 15;
    }
    if (e->entry == 0x8002ab70) {
      m->registers.gpr[2].word = f.random_value;
      m->registers.gpr[2].known_mask = 15;
    }
    if (e->entry == 0x800601b8 || e->entry == 0x80060240 ||
        e->entry == 0x80060008) {
      m->registers.gpr[2].word = uint32_t(f.contact);
      m->registers.gpr[2].known_mask = 15;
    }
    return 1;
  }
  static int resume(void *u, const Nba97GameTextMemory *,
                    const Nba97GameActorResumeEvent *e,
                    Nba97GameActorResumeMachine *) {
    auto &f = *static_cast<Fixture *>(u);
    f.resume_calls.push_back(e->pc);
    return f.resume_accept ? 1 : 0;
  }
};
} // namespace
int main() {
  Fixture normal;
  int result = normal.run();
  if (result != NBA97_TEXT_COMPLETE)
    std::cerr << "normal=" << result << " pc=" << std::hex
              << normal.progress.stopped_pc
              << " addr=" << normal.progress.stopped_address << "\n";
  check(result == NBA97_TEXT_COMPLETE);
  check(normal.progress.completed);
  check(normal.binding.rule_delay_count == 1);
  check(normal.binding.rule_delay_event[0].pc == 0x80060788);
  check(normal.binding.actor_resume_count == 0);
  check(normal.get16(Fixture::ball + 0x18) == 0);
  Fixture phase81;
  phase81.put16(0x800fdb90, 0x81);
  phase81.put16(0x800fdbd2, 0xffff);
  phase81.put32(0x80020bec, Fixture::actor);
  phase81.put32(0x80020c00, Fixture::actor);
  phase81.put16(Fixture::actor + 0x46, 0x27);
  result = phase81.run();
  if (result != NBA97_TEXT_COMPLETE)
    std::cerr << "phase81=" << result << " pc=" << std::hex
              << phase81.progress.stopped_pc
              << " addr=" << phase81.progress.stopped_address
              << " entry=" << phase81.progress.stopped_entry << "\n";
  check(result == NBA97_TEXT_COMPLETE);
  check(phase81.binding.actor_resume_count == 2);
  check(phase81.binding.actor_resume_event[0].pc == 0x800609b4);
  check(phase81.binding.actor_resume_event[1].pc == 0x800609e0);
  check(phase81.get16(0x800fdb90) == 0x82);
  check(phase81.get16(0x800fe884) == 3);
  check(phase81.binding.actor_resume[0].completed &&
        phase81.binding.actor_resume[1].completed);
  Fixture negative;
  negative.contact = -1;
  negative.put16(0x800fdb94, 1);
  negative.put16(0x800fdb96, 1);
  negative.put16(0x800fdbcc, 0);
  negative.put32(0x800fdc34, 0x80004000);
  negative.put8(0x800040d9, 1);
  negative.put32(0x80004020, 0x80005000);
  negative.put8(0x8000500d, 0);
  result = negative.run();
  if (result != NBA97_TEXT_COMPLETE)
    std::cerr << "negative=" << result << " pc=" << std::hex
              << negative.progress.stopped_pc
              << " addr=" << negative.progress.stopped_address
              << " entry=" << negative.progress.stopped_entry << "\n";
  check(result == NBA97_TEXT_COMPLETE);
  check(negative.binding.actor_resume_count == 1);
  check(negative.binding.actor_resume_event[0].pc == 0x80060ab4);
  check(negative.binding.rule_delay_count == 1);
  check(negative.binding.rule_delay_event[0].pc == 0x80060b6c);
  check(negative.resume_calls.size() == 3);
  Fixture negative_other;
  negative_other.contact = -1;
  negative_other.put8(Fixture::actor + 0xd9, 1);
  negative_other.put16(0x800fdb94, 0);
  negative_other.put16(0x800fdb96, 0);
  result = negative_other.run();
  if (result != NBA97_TEXT_COMPLETE)
    std::cerr << "negative_other=" << result << " pc=" << std::hex
              << negative_other.progress.stopped_pc
              << " addr=" << negative_other.progress.stopped_address
              << " entry=" << negative_other.progress.stopped_entry << "\n";
  check(result == NBA97_TEXT_COMPLETE);
  check(negative_other.binding.rule_delay_count == 1);
  check(negative_other.binding.rule_delay_event[0].pc == 0x80060b38);
  Fixture exceptional;
  exceptional.contact = -1;
  exceptional.put16(0x800fdb94, 1);
  exceptional.put16(0x800fdb96, 1);
  exceptional.put16(0x800fdbd4, 1);
  exceptional.put8(0x80021d8d, 1);
  exceptional.put16(Fixture::actor + 0x18, 0x8000);
  exceptional.put16(0x800fdbd8, 1);
  exceptional.put16(0x800fdbd6, 5);
  result = exceptional.run();
  if (result != NBA97_TEXT_COMPLETE)
    std::cerr << "exceptional=" << result << " pc=" << std::hex
              << exceptional.progress.stopped_pc
              << " addr=" << exceptional.progress.stopped_address
              << " entry=" << exceptional.progress.stopped_entry << "\n";
  check(result == NBA97_TEXT_COMPLETE);
  check(exceptional.binding.rule_delay_count == 2);
  check(exceptional.binding.rule_delay_event[1].pc == 0x80060c7c);
  check(exceptional.calls.size() >= 4);
  Fixture refused;
  refused.put16(0x800fdb90, 0x81);
  refused.put16(0x800fdbd2, 0xffff);
  refused.put32(0x80020bec, Fixture::actor);
  refused.put32(0x80020c00, Fixture::actor);
  refused.put16(Fixture::actor + 0x46, 0x27);
  refused.resume_accept = false;
  result = refused.run();
  check(result == NBA97_TEXT_IO_REFUSED);
  check(refused.progress.stopped_pc == 0x800609b4);
  check(refused.binding.child_result == NBA97_TEXT_IO_REFUSED);
  Fixture limited;
  limited.put16(0x800fdb90, 0x81);
  limited.put16(0x800fdbd2, 0xffff);
  limited.put32(0x80020bec, Fixture::actor);
  limited.put32(0x80020c00, Fixture::actor);
  limited.put16(Fixture::actor + 0x46, 0x27);
  limited.binding.child_operation_budget = 0;
  result = limited.run();
  check(result == NBA97_TEXT_IO_REFUSED);
  check(limited.progress.stopped_pc == 0x800609b4);
  check(limited.binding.child_result == NBA97_TEXT_LIMIT);
  check(limited.binding.actor_resume[0].operations == 0);
  check(nba97_game_ball_actor_contact_run(
            nullptr, &normal.progress, &normal.binding) == NBA97_TEXT_ARGUMENT);
  if (failures) {
    std::cerr << failures << " failures\n";
    return 1;
  }
  std::cout << "game_ball_actor_contact integration: " << checks
            << " checks; all four AE and all three AF source sites, including "
               "AF refusal\n";
}
