#include "frontend_load_payload_adapter.h"
#include "frontend_resource_load_adapter.h"
#include "frontend_resource_load_capture.h"
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
using U = std::uint32_t;
unsigned checks;
void check(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("frontend-resource integration line " +
                             std::to_string(line));
}
#define CHECK(value) check((value), __LINE__)
struct F {
  std::vector<std::uint8_t> b = std::vector<std::uint8_t>(0x200000);
  std::vector<std::uint8_t> k = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion r{0x80000000, b.data(), k.data(), b.size()};
  unsigned calls = 0;
  U size = 0x1000u;
  U allocation = 0x80170000u;
  bool refuse = false, relocate = false;
  F() { put(0x800d9b50u, 0); }
  void put(U address, U value) {
    if (address < 0x80000000u || address - 0x80000000u > b.size() - 4)
      throw std::runtime_error("integration fixture write outside RAM");
    for (unsigned i = 0; i < 4; ++i) {
      b[address - 0x80000000u + i] = std::uint8_t(value >> (i * 8));
      if (r.known)
        k[address - 0x80000000u + i] = 1;
    }
  }
  static int io(void *o, const Nba97GameTextMemory *,
                const Nba97FrontendResourceLoadEvent *e,
                Nba97FrontendResourceLoadMachine *m) {
    auto &f = *static_cast<F *>(o);
    ++f.calls;
    if (f.refuse)
      return 0;
    if (e->site == 1)
      m->registers.gpr[2] = {0, 15};
    if (e->site == 2) {
      U s = m->registers.gpr[29].word;
      if (f.relocate) {
        U n = 0x801ed000;
        if (s < 0x80000000u || s - 0x80000000u > f.b.size() - 128 ||
            n - 0x80000000u > f.b.size() - 128)
          return 0;
        for (unsigned i = 0; i < 128; ++i) {
          f.b[n - 0x80000000 + i] = f.b[s - 0x80000000 + i];
          if (f.r.known)
            f.k[n - 0x80000000 + i] = f.k[s - 0x80000000 + i];
        }
        m->registers.gpr[29] = {n, 15};
        s = n;
      }
      f.put(s + 24, 0x44);
      f.put(s + 32, f.size);
    }
    if (e->site == 3) {
      m->registers.gpr[2] = {f.allocation, 15};
      f.put(f.allocation, 0x55667788u);
    }
    return 1;
  }
};
int chain(F &f, Nba97FrontendOverlayLoadProgress &dd,
          Nba97FrontendLoadPayloadAdapterProgress &de,
          Nba97FrontendResourceLoadBinding &df) {
  Nba97FrontendOverlayLoadContext c{};
  for (unsigned i = 0; i < 32; ++i)
    c.machine.registers.gpr[i] = {0x10000000u + i, 15};
  c.machine.registers.gpr[0] = {0, 15};
  c.machine.registers.gpr[4] = {0x80024854, 15};
  c.machine.registers.gpr[5] = {0, 15};
  c.machine.registers.gpr[29] = {0x801f0000, 15};
  c.machine.registers.gpr[31] = {0x80028ad4, 15};
  c.memory = {&f.r, 1};
  c.operation_budget = 3;
  Nba97FrontendLoadPayloadBinding deb{};
  deb.operation_budget = 8;
  deb.io = nba97_frontend_resource_load_from_frontend_load_payload;
  deb.user = &df;
  return nba97_frontend_overlay_load_with_recovered_payload(&c, &deb, &dd, &de);
}
void checkFullChainMachine(const Nba97FrontendOverlayLoadProgress &p, U sp) {
  U expected[32];
  for (unsigned i = 0; i < 32; ++i) expected[i] = 0x10000000u + i;
  expected[0] = 0; expected[1] = 0x800e0000u; expected[2] = 0x55667788u;
  expected[4] = 0x80170000u; expected[5] = 0x55667788u;
  expected[6] = 4096; expected[7] = 1; expected[29] = sp;
  expected[31] = 0x80028ad4u;
  for (unsigned i = 0; i < 32; ++i)
    CHECK(p.machine.registers.gpr[i].word == expected[i] &&
          p.machine.registers.gpr[i].known_mask == 15);
  CHECK(p.machine.hi.word == 0 && p.machine.hi.known_mask == 0 &&
        p.machine.lo.word == 0 && p.machine.lo.known_mask == 0);
}
int main() {
  try {
    F f;
    Nba97FrontendResourceLoadBinding df{};
    df.operation_budget = 24;
    df.io = F::io;
    df.user = &f;
    Nba97FrontendOverlayLoadProgress dd{};
    Nba97FrontendLoadPayloadAdapterProgress de{};
    int x = chain(f, dd, de, df);
    CHECK(x == NBA97_TEXT_COMPLETE && dd.completed && de.progress.completed &&
          df.progress.completed && df.parent_event.pc == 0x8007b164 &&
          df.parent_machine.registers.gpr[31].word == 0x8007b16c &&
          df.progress.instruction_count == 53 && f.calls == 5 &&
          dd.machine.registers.gpr[2].word == 0x55667788u &&
          dd.machine.registers.gpr[29].word == 0x801f0000u &&
          dd.machine.registers.gpr[31].word == 0x80028ad4u);
    /* Exact nested failure propagation from DF through DE and DD. */
    CHECK(df.invocations == 1 && df.completions == 1);
    for (unsigned reg : {8u, 16u, 17u, 18u, 19u, 24u, 28u, 30u})
      CHECK(dd.machine.registers.gpr[reg].word == 0x10000000u + reg &&
            dd.machine.registers.gpr[reg].known_mask == 15);
    F refused;
    Nba97FrontendResourceLoadBinding rdf{};
    rdf.operation_budget = 15;
    rdf.io = F::io;
    rdf.user = &refused;
    refused.refuse = true;
    refused.size = 0;
    Nba97FrontendOverlayLoadProgress rdd{};
    Nba97FrontendLoadPayloadAdapterProgress rde{};
    CHECK(chain(refused, rdd, rde, rdf) == NBA97_TEXT_IO_REFUSED &&
          rdf.progress.stopped_pc == 0x8007b1f0 &&
          rdf.progress.operations == 6 && !rde.progress.completed &&
          !rdd.completed);
    F limited;
    Nba97FrontendResourceLoadBinding ldf{};
    ldf.operation_budget = 5;
    ldf.io = F::io;
    ldf.user = &limited;
    Nba97FrontendOverlayLoadProgress ldd{};
    Nba97FrontendLoadPayloadAdapterProgress lde{};
    CHECK(chain(limited, ldd, lde, ldf) == NBA97_TEXT_IO_REFUSED &&
          ldf.result == NBA97_TEXT_LIMIT && ldf.progress.operations == 5 &&
          ldf.progress.stopped_pc == 0x8007b1f0u && !ldd.completed);
    F moved;
    Nba97FrontendResourceLoadBinding mdf{};
    mdf.operation_budget = 24;
    mdf.io = F::io;
    mdf.user = &moved;
    moved.relocate = true;
    Nba97FrontendOverlayLoadProgress mdd{};
    Nba97FrontendLoadPayloadAdapterProgress mde{};
    CHECK(chain(moved, mdd, mde, mdf) == NBA97_TEXT_COMPLETE &&
          mdf.progress.completed &&
          mdd.machine.registers.gpr[29].word == 0x801ed070u &&
          mdd.machine.registers.gpr[31].word == 0x80028ad4u &&
          mdd.machine.registers.gpr[2].word == 0x55667788u);
    F absent;
    absent.r.known = nullptr;
    Nba97FrontendResourceLoadBinding adf{};
    adf.operation_budget = 24;
    adf.io = F::io;
    adf.user = &absent;
    Nba97FrontendOverlayLoadProgress add{};
    Nba97FrontendLoadPayloadAdapterProgress ade{};
    CHECK(chain(absent, add, ade, adf) == NBA97_TEXT_COMPLETE &&
          adf.progress.completed &&
          add.machine.registers.gpr[2].word == 0x55667788u);

    checkFullChainMachine(dd, 0x801f0000u);
    checkFullChainMachine(mdd, 0x801ed070u);
    checkFullChainMachine(add, 0x801f0000u);
    CHECK(rdd.stopped_pc == 0x8007b124u && rde.progress.stopped_pc == 0x8007b164u);
    CHECK(ldd.stopped_pc == 0x8007b124u && lde.progress.stopped_pc == 0x8007b164u);

    Nba97FrontendLoadPayloadEvent event{
        0x8007b164u, 0x8007b168u,
        0x8007b1d0u, 2,
        1,           NBA97_FRONTEND_LOAD_PAYLOAD_SITE_8007B164,
        3,           NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
    Nba97FrontendLoadPayloadMachine parent{};
    for (unsigned i = 0; i < 32; ++i)
      parent.registers.gpr[i] = {0x40000000u + i, 15};
    parent.registers.gpr[0] = {0, 15};
    parent.registers.gpr[31] = {0x8007b16cu, 15};
    Nba97GameTextMemory memory{&f.r, 1};
    auto rejects = [&](Nba97FrontendLoadPayloadEvent bad_event,
                       Nba97FrontendLoadPayloadMachine bad_machine) {
      Nba97FrontendResourceLoadBinding binding{};
      CHECK(!nba97_frontend_resource_load_from_frontend_load_payload(
                &binding, &memory, &bad_event, &bad_machine) &&
            binding.result == NBA97_TEXT_ARGUMENT && binding.invocations == 0);
    };
    auto bad_event = event;
    bad_event.pc ^= 4u;
    rejects(bad_event, parent);
    bad_event = event;
    bad_event.delay_slot_pc ^= 4u;
    rejects(bad_event, parent);
    bad_event = event;
    bad_event.entry ^= 4u;
    rejects(bad_event, parent);
    bad_event = event;
    bad_event.invocation = 2;
    rejects(bad_event, parent);
    bad_event = event;
    bad_event.argument_count = 2;
    rejects(bad_event, parent);
    bad_event = event;
    bad_event.site = NBA97_FRONTEND_LOAD_PAYLOAD_SITE_NONE;
    rejects(bad_event, parent);
    bad_event = event;
    bad_event.target_program = NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD;
    rejects(bad_event, parent);
    bad_event = event;
    bad_event.site = NBA97_FRONTEND_LOAD_PAYLOAD_SITE_NONE;
    rejects(bad_event, parent);
    auto bad_parent = parent;
    bad_parent.registers.gpr[31].word ^= 4u;
    rejects(event, bad_parent);
    bad_parent = parent;
    bad_parent.registers.gpr[31].known_mask = 7;
    rejects(event, bad_parent);

    std::string json = nba97::captureFrontendResourceLoad();
    CHECK(json.find("\"contract_failure\":0") != std::string::npos &&
          json.find("\"argument_count\":5") != std::string::npos);
    std::printf("frontend_resource_load_integration_tests passed %u checks\n",
                checks);
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
