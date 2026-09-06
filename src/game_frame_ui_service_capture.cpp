#include "game_frame_ui_service_capture.h"
#include "game_frame_ui_service_adapter.h"
#include "game_countdown_ui_update_adapter.h"
#include "game_text_chain_clear_adapter.h"
#include "game_image_record_upload_adapter.h"
#include "game_rectangle_upload_submit_adapter.h"
#include "game_text_submission_adapter.h"
#include <sstream>

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace nba97 {
namespace {
using U = std::uint32_t;

void checkAt(bool value, int line) {
  if (!value)
    throw std::runtime_error("frame UI native capture check failed at line " +
                             std::to_string(line));
}
#define check(value) checkAt((value), __LINE__)

struct Fixture {
  struct ExpectedService {
    U pc;
    U entry;
    U a0;
    unsigned count;
  };
  struct Trace {
    U pc;
    U entry;
    U address;
    unsigned kind;
  };
  static constexpr std::array<ExpectedService, 31> Expected{{
      {0x80068c24u, 0x80066f88u, 0, 0}, {0x80068c2cu, 0x80079664u, 0, 1},
      {0x80068c4cu, 0x80067468u, 0, 0}, {0x80068cecu, 0x80067550u, 0, 0},
      {0x80068cf4u, 0x800675e4u, 0, 0}, {0x80068d7cu, 0x8002de34u, 0, 0},
      {0x80068e00u, 0x80060ef8u, 0, 0}, {0x80068e08u, 0x80060fbcu, 0, 0},
      {0x80068e20u, 0x80060ef8u, 0, 0}, {0x80068e28u, 0x800747b0u, 0, 0},
      {0x80068e30u, 0x8006817cu, 0, 0}, {0x80068e38u, 0x8006830cu, 0, 0},
      {0x80068e78u, 0x80076b28u, 0, 0}, {0x80068e8cu, 0x800686b8u, 0, 0},
      {0x80068e94u, 0x80062bfcu, 0, 0}, {0x80068e9cu, 0x80066e84u, 0, 0},
      {0x80068ea4u, 0x80057b18u, 0, 0}, {0x8002dd8cu, 0x8007e26cu, 0, 1},
      {0x8002dd9cu, 0x800798b4u, 1, 1}, {0x80068fe0u, 0x80076b3cu, 0, 0},
      {0x8006902cu, 0x8008f224u, 0, 1}, {0x8006902cu, 0x8008f224u, 1, 1},
      {0x8006902cu, 0x8008f224u, 2, 1}, {0x8006902cu, 0x8008f224u, 3, 1},
      {0x8006902cu, 0x8008f224u, 4, 1}, {0x8006902cu, 0x8008f224u, 5, 1},
      {0x8006902cu, 0x8008f224u, 6, 1}, {0x8006902cu, 0x8008f224u, 7, 1},
      {0x800690b8u, 0x8006720cu, 0, 0}, {0x800690ccu, 0x800a584cu, 0, 0},
      {0x800691bcu, 0x80067930u, 0, 0},
  }};
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{0x80000000u, bytes.data(), known.data(),
                             bytes.size()};
  Nba97GameFrameUiServiceMachine caller{};
  Nba97GameFrameUiServiceBinding binding{};
  Nba97GameCountdownUiUpdateBinding countdown{};
  Nba97GameTextChainClearBinding textClear{};
  Nba97MatchTickProgress parentProgress{};
  std::vector<Nba97MatchTickCall> services;
  std::vector<Trace> trace;
  std::size_t serviceIndex = 0;
  unsigned uiChildren = 0;
  unsigned framePumps = 0;
  U failServicePc = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      caller.registers.gpr[i] = {0x10101010u + i, 15};
    caller.registers.gpr[0] = {0, 15};
    caller.registers.gpr[29] = {0x801ff000u, 15};
    caller.registers.gpr[31] = {0x8002ddb4u, 15};
    caller.hi = {0x12345678u, 7};
    caller.lo = {0x89abcdefu, 11};
    put(0x8001edecu, 1, 2);
    put(0x800fdb92u, 2, 2);
    put(0x800fdb8au, 1, 2);
    put(0x80021d82u, 1, 1);
    put(0x800fdb7cu, 0, 2);
    put(0x800fe8ccu, 1, 2);
    put(0x800fe8c4u, 3, 2);
    put(0x800fdc48u, 0x80130000u, 4);
    put(0x800fdbaeu, 5, 2);
    put(0x800fdb9cu, 0, 2);
    put(0x800fa038u, 0, 2);
    put(0x800eb680u, 1, 1);
    put(0x800fdb90u, 0, 2);
    put(0x800fdb68u, 0, 2);
    put(0x800fdb78u, 0, 1);
    put(0x800fdbdeu, 0, 2);
    put(0x800fdba4u, 601, 4);
    put(0x800fea2eu, 7, 2);
    for (unsigned i = 0; i < 22; ++i)
      put(0x800249e4u + i, 0x20u + i, 1);
    countdown.operation_budget = 256;
    countdown.io = countdownChild;
    countdown.user = this;
    textClear.operation_budget = 64;
    put(0x800b2048u, 0x80110000u, 4);
    put(0x80110010u, 0x80150000u, 4);
    put(0x80110014u, 0x80120000u, 4);
    put(0x80120192u, 3, 2);
    put(0x801500d2u, 0x7777, 2);
    put(0x801500d8u, 5, 2);
    put(0x80150152u, 0x8888, 2);
    put(0x80150158u, 0xffff, 2);
    binding.memory = {&region, 1};
    binding.explicit_caller_machine = &caller;
    binding.operation_budget = 32;
    binding.io = ui;
    binding.user = this;
  }

  std::size_t at(U address, unsigned width) const {
    if (address < 0x80000000u || std::uint64_t(address) + width > 0x80200000u)
      throw std::out_of_range("unmapped integration address");
    return address - 0x80000000u;
  }

  void put(U address, U value, unsigned width) {
    const auto offset = at(address, width);
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = std::uint8_t(value >> (i * 8u));
      known[offset + i] = 1;
    }
  }

  static int access(void *opaque, U pc, U address, unsigned width,
                    unsigned kind, Nba97PlayerFrameValue *value) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.trace.push_back({pc, 0, address, 1});
    try {
      const auto offset = f.at(address, width);
      if (kind == NBA97_FRAME_READ) {
        *value = {};
        for (unsigned i = 0; i < width; ++i)
          if (f.known[offset + i]) {
            value->word |= U(f.bytes[offset + i]) << (i * 8u);
            value->known_mask |= std::uint8_t(1u << i);
          }
      } else {
        for (unsigned i = 0; i < width; ++i) {
          f.bytes[offset + i] = std::uint8_t(value->word >> (i * 8u));
          f.known[offset + i] = std::uint8_t((value->known_mask >> i) & 1u);
        }
      }
      return NBA97_BODY_OK;
    } catch (const std::out_of_range &) {
      return NBA97_BODY_BOUNDS;
    }
  }

  static int service(void *opaque, const Nba97MatchTickCall *call,
                     Nba97GamePeriodValue *value) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.services.push_back(*call);
    f.trace.push_back({call->pc, call->entry, 0, 2});
    if (f.serviceIndex >= Expected.size())
      return NBA97_BODY_BOUNDS;
    const auto expected = Expected[f.serviceIndex++];
    if (call->pc != expected.pc || call->entry != expected.entry ||
        call->args[0] != expected.a0 || call->args[1] != 0 ||
        call->count != expected.count)
      return NBA97_BODY_BOUNDS;
    if (call->pc == f.failServicePc)
      return NBA97_BODY_BOUNDS;
    /* Each accepted call is an explicit synthetic prerequisite contract for
     * this natural caller test. Only 60FBC and 8F224 have effects consumed by
     * the selected source path; the rest record typed synchronous completion.
     */
    if (call->entry == 0x80060fbcu)
      f.put(0x800fdb88u, 1, 2);
    if (value) {
      value->word = call->entry == 0x8008f224u && call->args[0] == 0 ? 1u : 0u;
      value->known = 1;
    }
    return NBA97_BODY_OK;
  }

  static int player(void *opaque, U) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.trace.push_back({0x80068d84u, 0x8006801cu, 0, 3});
    f.put(0x800fdc48u, 0x80140000u, 4);
    return NBA97_BODY_OK;
  }

  static int ball(void *opaque, U, U) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.trace.push_back({0x80068d9cu, 0x8006ef60u, 0, 4});
    f.put(0x800fdc48u, 0x80140040u, 4);
    return NBA97_BODY_OK;
  }

  static int net(void *opaque, U pc) {
    auto &f = *static_cast<Fixture *>(opaque);
    check(pc == 0x8002dda4u);
    f.trace.push_back({pc, 0x8002dc88u, 0, 5});
    f.put(0x800fdb6cu, 0xffffu, 2);
    return NBA97_BODY_OK;
  }

  static int frame(void *opaque, U pc) {
    auto &f = *static_cast<Fixture *>(opaque);
    check(pc == 0x8002ddb4u);
    /* This typed synthetic completion is observable in trace/framePumps; it
     * does not claim a recovered renderer or a presented match frame. */
    f.trace.push_back({pc, 0x80049018u, 0, 6});
    ++f.framePumps;
    return NBA97_BODY_OK;
  }

  static int countdownChild(void *opaque, const Nba97GameTextMemory *memory,
                            const Nba97GameCountdownUiUpdateEvent *event,
                            Nba97GameCountdownUiUpdateMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    check(event->pc == 0x8003295cu && event->delay_slot_pc == 0x80032960u &&
          event->entry == 0x8003066cu && event->argument_count == 1 &&
          machine->registers.gpr[4].word == 0xc9u &&
          machine->registers.gpr[31].word == 0x80032964u);
    return nba97_game_text_chain_clear_from_countdown_ui_update(
        &f.textClear, memory, event, machine);
  }

  static int ui(void *opaque, const Nba97GameTextMemory *memory,
                const Nba97GameFrameUiServiceEvent *event,
                Nba97GameFrameUiServiceMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.uiChildren;
    f.trace.push_back({event->pc, event->entry, 0, 7});
    check(event->pc == 0x80032b18u && event->entry == 0x8003287cu &&
          event->delay_slot_pc == 0x80032b1cu && event->argument_count == 0);
    return nba97_game_countdown_ui_update_from_frame_ui_service(
        &f.countdown, memory, event, machine);
  }

  int run(bool withMachine = true) {
    serviceIndex = 0;
    services.clear();
    trace.clear();
    Nba97MatchTickContext context{};
    context.access = access;
    context.service = service;
    context.player_update = player;
    context.ball_simulation = ball;
    context.net_transform = net;
    context.match_frame = frame;
    context.user = this;
    context.operation_budget = 10000;
    context.incoming_s6 = {7, 1};
    binding.explicit_caller_machine = withMachine ? &caller : nullptr;
    return nba97_game_match_tick_with_frame_ui_service(&context, &binding,
                                                       &parentProgress);
  }
};

std::size_t traceIndex(const Fixture &f, unsigned kind, U pc) {
  for (std::size_t i = 0; i < f.trace.size(); ++i)
    if (f.trace[i].kind == kind && f.trace[i].pc == pc)
      return i;
  throw std::runtime_error("missing natural trace entry");
}

struct UploadFixture : Fixture {
  Nba97GameTextSubmissionBinding textSubmit{};
  unsigned measurementCalls = 0, allocationCalls = 0, clearBackendCalls = 0;
  Nba97GameImageRecordUploadBinding upload{};
  Nba97GameRectangleUploadSubmitBinding submit{};
  std::vector<U> submitCalls;
  Nba97GameCountdownUiUpdateProgress active{};
  std::array<std::uint16_t, 4> rectangle{};
  unsigned textCalls = 0;
  unsigned uploadCalls = 0;

  UploadFixture() {
    put(0x800fdba4, 120, 4);
    put(0x800fe8cc, 0, 2);
    put(0x800fdb58, 120, 4);
    put(0x80021d92, 1, 1);
    put(0x800fea2e, 0xffff, 2);
    put(0x800249e8, 0x1555, 2);
    submit.operation_budget = 32;
    submit.io = submitChild;
    submit.user = this;
    put(0x800d7b14, 0, 4);
    // Runtime-generated empty-string font fixture. Every prerequisite service
    // below has an explicit typed contract; this is not retail text rendering.
    put(0x800b2048, 0x80110000, 4);
    put(0x80110008, 0x80130000, 4);
    put(0x8011000c, 0x80140000, 4);
    put(0x80110010, 0x80120000, 4);
    put(0x80110014, 0x80160000, 4);
    put(0x80110022, 1, 2);
    put(0x80110026, 0, 2);
    put(0x80110028, 0x33, 2);
    put(0x8011002a, 0, 2);
    for (U offset = 0x2c; offset <= 0x3e; offset += 2)
      put(0x80110000 + offset, 0xffff, 2);
    put(0x80110040, 0, 2);
    put(0x80110042, 4, 1);
    put(0x8011004a, 1, 1);
    put(0x80110052, 10, 1);
    put(0x80120012, 0xffff, 2);
    put(0x80160192, 0xffff, 2);
    put(0x800249fc, 0, 1);
    put(0x800c55c2, 0, 1);
    put(0x800c55b8, 0x80170000, 4);
    put(0x8017002c, 0x8009a97c, 4);
    textSubmit.operation_budget = 1024;
    textSubmit.io = textService;
    textSubmit.user = this;
    textSubmit.clear_operation_budget = 64;
    textSubmit.clear_io = clearService;
    textSubmit.clear_user = this;
    upload.operation_budget = 128;
    upload.io = uploadChild;
    upload.user = this;
  }
  U get(U address, unsigned width) const {
    U value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= U(bytes[at(address + i, 1)]) << (i * 8u);
    return value;
  }
  static int textChild(void *opaque, const Nba97GameTextMemory *memory,
                       const Nba97GameCountdownUiUpdateEvent *event,
                       Nba97GameCountdownUiUpdateMachine *machine) {
    auto &f = *static_cast<UploadFixture *>(opaque);
    check(event->pc == 0x800329e8 && event->entry == 0x80030d18 &&
          event->delay_slot_pc == 0x800329ec && event->argument_count == 5);
    ++f.textCalls;
    return nba97_game_text_submission_from_countdown_ui_update(
        &f.textSubmit, memory, event, machine);
  }
  static int textService(void *opaque, const Nba97GameTextMemory *,
                         const Nba97GameTextSubmissionEvent *event,
                         Nba97GameTextSubmissionMachine *machine) {
    auto &f = *static_cast<UploadFixture *>(opaque);
    if (event->pc == 0x80030e14 && event->entry == 0x8002eb50 &&
        event->delay_slot_pc == 0x80030e18 && event->argument_count == 4) {
      ++f.measurementCalls;
      check(machine->registers.gpr[4].word == 0x800249fc);
      f.put(machine->registers.gpr[5].word, 8, 2);
      f.put(machine->registers.gpr[6].word, 0, 2);
      f.put(machine->registers.gpr[7].word, 4, 2);
      return 1;
    }
    if (event->pc == 0x80030e20 && event->entry == 0x8002ef88 &&
        event->delay_slot_pc == 0x80030e24 && event->argument_count == 1) {
      ++f.allocationCalls;
      check(machine->registers.gpr[4].word == 0);
      machine->registers.gpr[2] = {0x80150000, 15};
      return 1;
    }
    return 0;
  }
  static int clearService(void *opaque, const Nba97GameTextMemory *,
                          const Nba97GameClearOrderingTableEvent *event,
                          Nba97GameClearOrderingTableMachine *) {
    auto &f = *static_cast<UploadFixture *>(opaque);
    ++f.clearBackendCalls;
    check(event->kind == NBA97_GAME_CLEAR_ORDERING_TABLE_BACKEND &&
          event->pc == 0x800999bc && event->delay_slot_pc == 0x800999c0 &&
          event->target == 0x8009a97c && event->argument_count == 2);
    return 1;
  }
  static int uploadChild(void *opaque, const Nba97GameTextMemory *memory,
                         const Nba97GameImageRecordUploadEvent *event,
                         Nba97GameImageRecordUploadMachine *machine) {
    auto &f = *static_cast<UploadFixture *>(opaque);
    check(event->pc == 0x8009464c && event->entry == 0x800944f4 &&
          event->delay_slot_pc == 0x80094650 && event->argument_count == 2);
    ++f.uploadCalls;
    for (unsigned i = 0; i < 4; ++i)
      f.rectangle[i] = std::uint16_t(f.get(machine->registers.gpr[4].word + 2 * i, 2));
    return nba97_game_rectangle_upload_submit_from_image_record_upload(
        &f.submit, memory, event, machine);
  }
  static int submitChild(void *opaque, const Nba97GameTextMemory *,
                         const Nba97GameRectangleUploadSubmitEvent *event,
                         Nba97GameRectangleUploadSubmitMachine *machine) {
    auto &f = *static_cast<UploadFixture *>(opaque);
    const bool first = f.submitCalls.empty();
    check(event->pc == (first ? 0x80094508u : 0x80094514u) &&
          event->delay_slot_pc == event->pc + 4 &&
          event->entry == (first ? 0x80094440u : 0x8009971cu) &&
          event->argument_count == (first ? 1 : 2) &&
          machine->registers.gpr[31].word == event->pc + 8 &&
          f.get(0x800d7b14, 4) == 0);
    for (unsigned i = 0; i < 4; ++i)
      check(f.get(machine->registers.gpr[4].word + 2 * i, 2) == f.rectangle[i]);
    if (!first)
      check(machine->registers.gpr[5].word == 0x800fb5d0u);
    f.submitCalls.push_back(event->entry);
    // Explicit typed prerequisites; no GPU upload or rendered-frame claim.
    return 1;
  }
};

std::string captureImageRecordUpload() {
  UploadFixture f;
  Nba97GameCountdownUiUpdateContext context{};
  context.memory = {&f.region, 1};
  context.operation_budget = 512;
  context.machine = f.caller;
  context.io = UploadFixture::textChild;
  context.user = &f;
  check(nba97_game_countdown_ui_update_with_image_record_upload(
            &context, &f.upload, &f.active) == NBA97_TEXT_COMPLETE);
  const auto &p = f.upload.progress;
  check(p.completed && f.active.completed && f.active.record_uploaded &&
        f.upload.completions == 1 && f.textCalls == 1 && f.uploadCalls == 1 &&
        f.get(0x800fb5c0, 1) == 0x2b && f.get(0x800fea2e, 2) == 2 &&
        f.rectangle == (std::array<std::uint16_t, 4>{0x340, 0xf0, 0x10, 1}));
  const auto &t = f.textSubmit.progress;
  check(t.completed && f.textSubmit.completions == 1 &&
        f.textSubmit.clear_completions == 2 && f.measurementCalls == 1 &&
        f.allocationCalls == 1 && f.clearBackendCalls == 2 &&
        t.return_v0.word == 0x80120000 &&
        t.machine.registers.gpr[29].word == 0x801fefc0 &&
        t.machine.registers.gpr[31].word == 0x800329f0 &&
        f.get(0x80160192, 2) == 0 && f.get(0x80120014, 2) == 201 &&
        f.get(0x80120000, 4) == 0x000c567c && f.get(0x80120004, 4) == 0x000c567c);
  const auto &q = f.submit.progress;
  check(q.completed && f.submit.completions == 1 && q.instruction_count == 19 &&
        q.operations == 9 && q.reads == 3 && q.stores == 4 &&
        q.callbacks_completed == 2 && f.get(0x800d7b14, 4) == 1 &&
        q.machine.registers.gpr[29].word == 0x801fef90 &&
        q.machine.registers.gpr[31].word == 0x80094654 &&
        f.submitCalls == (std::vector<U>{0x80094440, 0x8009971c}));
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x80094540\","
       "\"inclusive_end\":\"0x800946A3\",\"bytes\":356,\"instructions\":89,"
       "\"classification\":\"BLOCKED\",\"scope\":\"independent synthetic active countdown caller; "
       "explicit full entry machine; text submission, clear-table and rectangle submit owners composed; typed measurement, allocation and GPU services; no rendered match frame\","
       "\"completed\":true,\"parent_completed\":true,\"same_parent_memory\":true,"
       "\"call_pc\":" << f.upload.event.pc << ",\"instruction_count\":" << p.instruction_count
    << ",\"operations\":" << p.operations << ",\"reads\":" << p.reads
    << ",\"stores\":" << p.stores << ",\"callbacks\":" << p.callbacks_completed
    << ",\"records\":" << p.records_visited << ",\"sp\":" << p.machine.registers.gpr[29].word
    << ",\"ra\":" << p.machine.registers.gpr[31].word
    << ",\"header_before\":35,\"header_after\":43,\"cache_before\":65535,\"cache_after\":2,"
       "\"rectangle\":[832,240,16,1],\"blocked_children\":[\"0x800A3BF8\"],"
       "\"rectangle_upload_submit\":{\"program\":\"GAMEONLY\",\"address\":\"0x800944F4\","
       "\"inclusive_end\":\"0x8009453F\",\"bytes\":76,\"instructions\":19,\"classification\":\"BLOCKED\","
       "\"completed\":true,\"same_parent_memory\":true,\"call_pc\":" << f.submit.event.pc
    << ",\"instruction_count\":" << q.instruction_count << ",\"operations\":" << q.operations
    << ",\"reads\":" << q.reads << ",\"stores\":" << q.stores
    << ",\"callbacks\":" << q.callbacks_completed << ",\"sp\":" << q.machine.registers.gpr[29].word
    << ",\"ra\":" << q.machine.registers.gpr[31].word
    << ",\"pending_before\":0,\"pending_after\":1,\"blocked_children\":[\"0x80094440\",\"0x8009971C\"]},"
       "\"text_submission\":{\"program\":\"GAMEONLY\",\"address\":\"0x80030D18\","
       "\"inclusive_end\":\"0x80031523\",\"bytes\":2060,\"instructions\":515,"
       "\"classification\":\"BLOCKED\",\"completed\":true,\"same_parent_memory\":true,"
       "\"call_pc\":" << f.textSubmit.event.pc << ",\"instruction_count\":" << t.instruction_count
    << ",\"operations\":" << t.operations << ",\"reads\":" << t.reads << ",\"stores\":" << t.stores
    << ",\"callbacks\":" << t.callbacks_completed << ",\"record\":" << t.return_v0.word
    << ",\"sp\":" << t.machine.registers.gpr[29].word << ",\"ra\":" << t.machine.registers.gpr[31].word
    << ",\"clear_owners\":" << f.textSubmit.clear_completions
    << ",\"clear_backend_calls\":" << f.clearBackendCalls
    << ",\"head_before\":65535,\"head_after\":0,\"record_slot\":201,\"record_heads\":[808572,808572],"
       "\"blocked_children\":[\"0x8002EB50\",\"0x8002EF88\",\"0x8002ECD4\",\"0x800AA468\",\"0x80056914\"],"
       "\"blocked_clear_backend\":\"0x8009A97C\"}}";
  return o.str();
}

} // namespace
std::string captureGameFrameUiService() {
  Fixture f;
  check(f.run() == NBA97_BODY_OK && f.parentProgress.completed);
  const auto &p = f.binding.progress;
  check(p.completed && f.binding.invocations == 1 &&
        f.binding.completions == 1 && f.uiChildren == 1 && f.framePumps == 1);
  check(f.serviceIndex == Fixture::Expected.size() &&
        f.services.size() == Fixture::Expected.size());
  const std::array<std::size_t, 6> order{
      {traceIndex(f, 2, 0x8002dd8c), traceIndex(f, 1, 0x8002dd98),
       traceIndex(f, 2, 0x8002dd9c), traceIndex(f, 5, 0x8002dda4),
       traceIndex(f, 7, 0x80032b18), traceIndex(f, 6, 0x8002ddb4)}};
  for (std::size_t i = 1; i < order.size(); ++i)
    check(order[i - 1] < order[i]);
  check(p.machine.registers.gpr[29].word == 0x801ff000 &&
        p.machine.registers.gpr[31].word == 0x8002ddb4 &&
        p.machine.registers.gpr[2].word == 1);
  const auto &c = f.countdown.progress;
  const auto &text = f.textClear.progress;
  check(c.completed && f.countdown.completions == 1 &&
        c.callbacks_completed == 1 && !c.active_gate && !c.record_uploaded &&
        f.bytes[f.at(0x800fea2eu, 2)] == 255 &&
        f.bytes[f.at(0x800fea2fu, 1)] == 255);
  check(text.completed && text.chain_iterations == 2 &&
        f.textClear.completions == 1 && text.operations == 11 &&
        f.bytes[f.at(0x801500d2u, 2)] == 0 &&
        f.bytes[f.at(0x801500d3u, 1)] == 0 &&
        f.bytes[f.at(0x80150152u, 2)] == 0 &&
        f.bytes[f.at(0x80150153u, 1)] == 0 &&
        f.bytes[f.at(0x80120192u, 2)] == 255 &&
        f.bytes[f.at(0x80120193u, 1)] == 255);
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x80032B10\",\"inclusive_end\":"
       "\"0x80032BB7\",\"bytes\":168,\"instructions\":42,\"classification\":"
       "\"BLOCKED\",\"scope\":\"independent synthetic actual match-tick "
       "caller; explicit full caller machine; whitelisted typed prerequisite "
       "services; countdown and text clear owners composed; match-frame completion remains synthetic; no "
       "rendered match "
       "frame\",\"completed\":true,\"parent_completed\":true,\"same_parent_"
       "memory\":true,\"call_pc\":"
    << f.binding.event.pc << ",\"operations\":" << p.operations
    << ",\"instruction_count\":" << p.instruction_count
    << ",\"reads\":" << p.reads << ",\"stores\":" << p.stores
    << ",\"callbacks\":" << p.callbacks_completed
    << ",\"prerequisite_calls\":" << f.services.size()
    << ",\"synthetic_frame_completions\":" << f.framePumps
    << ",\"v0\":" << p.machine.registers.gpr[2].word
    << ",\"sp\":" << p.machine.registers.gpr[29].word
    << ",\"ra\":" << p.machine.registers.gpr[31].word
    << ",\"hilo_known_masks\":[" << unsigned(p.machine.hi.known_mask) << ','
    << unsigned(p.machine.lo.known_mask)
    << "],\"ordered_checkpoint_indices\":[";
  for (std::size_t i = 0; i < order.size(); ++i) {
    if (i)
      o << ',';
    o << order[i];
  }
  o << "],\"countdown_update\":{\"program\":\"GAMEONLY\",\"address\":\"0x8003287C\","
       "\"inclusive_end\":\"0x80032B0F\",\"bytes\":660,\"instructions\":165,"
       "\"classification\":\"BLOCKED\",\"completed\":true,\"same_parent_memory\":true,"
       "\"call_pc\":" << f.countdown.event.pc
    << ",\"instruction_count\":" << c.instruction_count
    << ",\"operations\":" << c.operations << ",\"reads\":" << c.reads
    << ",\"stores\":" << c.stores << ",\"callbacks\":" << c.callbacks_completed
    << ",\"cache_before\":7,\"cache_after\":65535,\"generated_table_bytes\":22"
    << ",\"sp\":" << c.machine.registers.gpr[29].word
    << ",\"ra\":" << c.machine.registers.gpr[31].word
    << ",\"blocked_children\":[\"0x80030D18\",\"0x80094540\"],\"text_chain_clear\":{"
       "\"program\":\"GAMEONLY\",\"address\":\"0x8003066C\",\"inclusive_end\":\"0x800306E7\","
       "\"bytes\":124,\"instructions\":31,\"classification\":\"BLOCKED\",\"completed\":true,"
       "\"same_parent_memory\":true,\"call_pc\":" << f.textClear.event.pc
    << ",\"instruction_count\":" << text.instruction_count
    << ",\"operations\":" << text.operations << ",\"reads\":" << text.reads
    << ",\"stores\":" << text.stores << ",\"chain_iterations\":" << text.chain_iterations
    << ",\"slot\":201,\"head_before\":3,\"head_after\":65535,"
       "\"link_flags_before\":[30583,34952],\"link_flags_after\":[0,0]"
    << ",\"sp\":" << text.machine.registers.gpr[29].word
    << ",\"ra\":" << text.machine.registers.gpr[31].word << "}}"
    << ",\"blocked_children\":[\"0x80031C5C\",\"0x8003066C\","
       "\"0x80032774\"]}";
  auto result = o.str();
  result.pop_back();
  return result + ",\"image_record_upload\":" + captureImageRecordUpload() + "}";
}
} // namespace nba97
