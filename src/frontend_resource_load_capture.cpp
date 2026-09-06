#include "frontend_resource_load_capture.h"
#include "frontend_resource_load_adapter.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <vector>
namespace nba97 {
namespace {
using U = std::uint32_t;
constexpr U Base = 0x80000000u, Size = 0x200000u, Sp = 0x801f0000u;
struct Call {
  Nba97FrontendResourceLoadEvent e{};
  Nba97FrontendResourceLoadMachine m{};
};
struct F {
  std::vector<std::uint8_t> b = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> k = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion r{Base, b.data(), k.data(), b.size()};
  Nba97FrontendResourceLoadContext c{};
  Nba97FrontendResourceLoadProgress p{};
  std::array<Nba97FrontendResourceLoadAccess, 32> a{};
  std::array<U, 64> pc{};
  std::vector<Call> calls;
  bool bad = false;
  F() {
    for (unsigned i = 0; i < 32; ++i)
      c.machine.registers.gpr[i] = {0x51000000u + i * 0x101u, 15};
    c.machine.registers.gpr[0] = {0, 15};
    c.machine.registers.gpr[4] = {0x80024854, 15};
    c.machine.registers.gpr[5] = {0, 15};
    c.machine.registers.gpr[6] = {1, 15};
    c.machine.registers.gpr[29] = {Sp, 15};
    c.machine.registers.gpr[31] = {0x8007b16c, 15};
    c.machine.hi = {0x12345678, 5};
    c.machine.lo = {0x9abcdef0, 10};
    put(0x800d9b50, 0x80061000);
    c.memory = {&r, 1};
    c.operation_budget = 25;
    c.io = io;
    c.user = this;
    c.access_journal = a.data();
    c.access_journal_capacity = a.size();
    c.instruction_journal = pc.data();
    c.instruction_journal_capacity = pc.size();
  }
  void put(U x, U v, unsigned mask = 15) {
    if (x < Base || x - Base > Size - 4) {
      bad = true;
      return;
    }
    for (unsigned i = 0; i < 4; ++i) {
      b[x - Base + i] = std::uint8_t(v >> (i * 8));
      k[x - Base + i] = std::uint8_t((mask >> i) & 1);
    }
  }
  static int io(void *o, const Nba97GameTextMemory *,
                const Nba97FrontendResourceLoadEvent *e,
                Nba97FrontendResourceLoadMachine *m) {
    auto &f = *static_cast<F *>(o);
    Nba97FrontendResourceLoadSiteContract q{};
    if (!e || !m || !nba97_frontend_resource_load_site_contract(e->site, &q) ||
        e->pc != q.pc || e->delay_slot_pc != q.delay_slot_pc ||
        e->argument_count != q.argument_count ||
        (e->site != 6 && e->entry != q.target)) {
      f.bad = true;
      return 0;
    }
    f.calls.push_back({*e, *m});
    if (e->site == 1)
      m->registers.gpr[2] = {0x80160000, 15};
    if (e->site == 2) {
      U s = m->registers.gpr[29].word;
      f.put(s + 24, 0x44);
      f.put(s + 32, 0x1000);
    }
    if (e->site == 3) {
      m->registers.gpr[2] = {0x80170000, 15};
      f.put(0x80170000, 0x55667788, 9);
    }
    if (e->site == 6)
      m->registers.gpr[2] = {0x89abcdef, 6};
    return 1;
  }
};
std::string hx(U x) {
  std::ostringstream o;
  o << "\"0x" << std::hex << std::setw(8) << std::setfill('0') << x << '\"';
  return o.str();
}
void w(std::ostringstream &o, const Nba97FrontendResourceLoadWord &x) {
  o << "{\"word\":" << hx(x.word)
    << ",\"known_mask\":" << unsigned(x.known_mask) << '}';
}
void cpu(std::ostringstream &o, const Nba97FrontendResourceLoadMachine &m) {
  o << "{\"gpr\":[";
  for (unsigned i = 0; i < 32; ++i) {
    if (i)
      o << ',';
    w(o, m.registers.gpr[i]);
  }
  o << "],\"hi\":";
  w(o, m.hi);
  o << ",\"lo\":";
  w(o, m.lo);
  o << '}';
}
} // namespace
std::string captureFrontendResourceLoad() {
  try {
    F f;
    int result = nba97_frontend_resource_load(&f.c, &f.p);
    if (result != NBA97_TEXT_COMPLETE || !f.p.completed ||
        f.p.instruction_count != 59 || f.p.operations != 25 ||
        f.calls.size() != 6 || f.p.instruction_events > f.pc.size() ||
        f.p.access_events > f.a.size())
      f.bad = true;
    std::ostringstream o;
    o << "{\"program\":\"FEONLY\",\"address\":" << hx(0x8007b1d0)
      << ",\"inclusive_end\":" << hx(0x8007b2bb)
      << ",\"bytes\":236,\"instructions\":59,\"source_sha256\":"
         "\"16756cd9554b869085b0f84eb6b2f1b9fe0931e7bb07f40c9f08ce90a3677c26\","
         "\"result\":"
      << result << ",\"completed\":" << unsigned(f.p.completed)
      << ",\"contract_failure\":" << f.bad
      << ",\"classification\":\"no direct visual "
         "effect\",\"gameplay_shown\":\"BLOCKED\",\"fixture_contract\":"
         "\"Synthetic standalone full-machine entry. Lookup returns "
         "V0=0x80160000/mask15, which the source discards. Info writes "
         "handle=0x44/mask15 to live sp+24 and size=0x1000/mask15 to live "
         "sp+32. Allocation returns 0x80170000/mask15 with descriptor word "
         "0x55667788/mask9. Postload and close preserve all state. Dynamic "
         "callback returns V0=0x89ABCDEF/mask6. All children otherwise "
         "preserve complete CPU and RAM; no production ABI is claimed.\","
         "\"owner\":{\"operations\":"
      << f.p.operations << ",\"accesses\":" << f.p.accesses
      << ",\"reads\":" << f.p.reads << ",\"stores\":" << f.p.stores
      << ",\"instruction_trace\":[";
    for (size_t i = 0; i < std::min(f.p.instruction_events, f.pc.size()); ++i) {
      if (i)
        o << ',';
      o << hx(f.pc[i]);
    }
    o << "],\"access_journal\":[";
    for (size_t i = 0; i < std::min(f.p.access_events, f.a.size()); ++i) {
      if (i)
        o << ',';
      auto &e = f.a[i];
      o << "{\"pc\":" << hx(e.pc) << ",\"address\":" << hx(e.address)
        << ",\"value\":" << hx(e.value) << ",\"operation\":" << e.operation
        << ",\"width\":" << unsigned(e.width)
        << ",\"known_mask\":" << unsigned(e.known_mask)
        << ",\"kind\":" << unsigned(e.kind) << '}';
    }
    o << "],\"calls\":[";
    for (size_t i = 0; i < f.calls.size(); ++i) {
      if (i)
        o << ',';
      o << "{\"pc\":" << hx(f.calls[i].e.pc)
        << ",\"delay\":" << hx(f.calls[i].e.delay_slot_pc)
        << ",\"target\":" << hx(f.calls[i].e.entry)
        << ",\"operation\":" << f.calls[i].e.operation
        << ",\"invocation\":" << f.calls[i].e.invocation
        << ",\"program\":" << unsigned(f.calls[i].e.target_program)
        << ",\"argument_count\":" << unsigned(f.calls[i].e.argument_count)
        << ",\"machine\":";
      cpu(o, f.calls[i].m);
      o << '}';
    }
    o << "]},\"final_machine\":";
    cpu(o, f.p.machine);
    o << ",\"next_unbound_boundary\":\"FEONLY 0x8007B1F0 -> 0x8008A2C8 "
         "lookup, followed by 0x8007B214 -> 0x8008A594 info/open; remaining "
         "heap, pump, and loader callbacks also stay unbound\"}";
    return o.str();
  } catch (...) {
    return "{\"program\":\"FEONLY\",\"address\":\"0x8007b1d0\",\"contract_"
           "failure\":true,\"gameplay_shown\":\"BLOCKED\"}";
  }
}
} // namespace nba97
