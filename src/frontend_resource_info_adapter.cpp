#include "frontend_resource_info_adapter.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
constexpr std::uint8_t FE = NBA97_FRONTEND_MAIN_PROGRAM_FEONLY;
constexpr Nba97FrontendResourceInfoSiteContract Contracts[] = {
    {0, 0, 0, 0, 0},
    {0x8008a5e8u, 0x8008a5ecu, 0x80084910u, 3, FE},
    {0x8008a610u, 0x8008a614u, 0x80074184u, 6, FE},
    {0x8008a63cu, 0x8008a640u, 0x80083b70u, 4, FE},
    {0x8008a648u, 0x8008a64cu, 0x8007f588u, 2, FE},
    {0x8008a658u, 0x8008a65cu, 0x8008a408u, 1, FE},
    {0x8008a66cu, 0x8008a670u, 0x8007f318u, 3, FE},
    {0x8008a698u, 0x8008a69cu, 0x8008a7b0u, 1, FE}};
static_assert(sizeof Contracts / sizeof Contracts[0] ==
              NBA97_FRONTEND_RESOURCE_INFO_SITE_COUNT);

bool machineValid(const Nba97FrontendResourceInfoMachine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 15 ||
      machine.hi.known_mask > 15 || machine.lo.known_mask > 15)
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

struct Composition {
  Nba97FrontendResourceLoadIo fallback;
  void *fallback_user;
  Nba97FrontendResourceInfoBinding *binding;
  Nba97FrontendResourceInfoAdapterProgress *out;
};

int composed(void *opaque, const Nba97GameTextMemory *memory,
             const Nba97FrontendResourceLoadEvent *event,
             Nba97FrontendResourceLoadMachine *machine) {
  auto &composition = *static_cast<Composition *>(opaque);
  if (event && event->site == NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B214) {
    std::size_t before = composition.binding->invocations;
    int accepted = nba97_frontend_resource_info_from_frontend_resource_load(
        composition.binding, memory, event, machine);
    if (composition.binding->invocations != before) {
      ++composition.out->invocations;
      if (composition.binding->result == NBA97_TEXT_COMPLETE)
        ++composition.out->completions;
      composition.out->parent_event = composition.binding->parent_event;
      composition.out->parent_machine = composition.binding->parent_machine;
      composition.out->progress = composition.binding->progress;
      composition.out->result = composition.binding->result;
    }
    return accepted;
  }
  return composition.fallback
             ? composition.fallback(composition.fallback_user, memory, event,
                                    machine)
             : 0;
}
} // namespace

int nba97_frontend_resource_info_site_contract(
    uint8_t site, Nba97FrontendResourceInfoSiteContract *contract) {
  if (!contract || !site || site >= NBA97_FRONTEND_RESOURCE_INFO_SITE_COUNT)
    return 0;
  *contract = Contracts[site];
  return 1;
}

int nba97_frontend_resource_info_from_frontend_resource_load(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97FrontendResourceLoadEvent *event,
    Nba97FrontendResourceLoadMachine *machine) {
  auto *binding = static_cast<Nba97FrontendResourceInfoBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) ||
      event->site != NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B214 ||
      event->pc != 0x8007b214u || event->delay_slot_pc != 0x8007b218u ||
      event->entry != 0x8008a594u || event->invocation != 1 ||
      event->argument_count != 5 || event->target_program != FE ||
      machine->registers.gpr[NBA97_FRONTEND_RESOURCE_INFO_RA].word !=
          0x8007b21cu ||
      machine->registers.gpr[NBA97_FRONTEND_RESOURCE_INFO_RA].known_mask != 15 ||
      (!binding->access_journal && binding->access_journal_capacity) ||
      (!binding->instruction_journal && binding->instruction_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  ++binding->invocations;
  binding->parent_event = *event;
  binding->parent_machine = *machine;
  Nba97FrontendResourceInfoContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  context.instruction_journal = binding->instruction_journal;
  context.instruction_journal_capacity = binding->instruction_journal_capacity;
  binding->result = nba97_frontend_resource_info(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}

int nba97_frontend_resource_load_with_recovered_info(
    Nba97FrontendResourceLoadContext *context,
    Nba97FrontendResourceInfoBinding *binding,
    Nba97FrontendResourceLoadProgress *progress,
    Nba97FrontendResourceInfoAdapterProgress *out) {
  if (!out)
    return NBA97_TEXT_ARGUMENT;
  std::memset(out, 0, sizeof *out);
  if (!context || !binding)
    return NBA97_TEXT_ARGUMENT;
  Composition composition{context->io, context->user, binding, out};
  auto composed_context = *context;
  composed_context.io = composed;
  composed_context.user = &composition;
  return nba97_frontend_resource_load(&composed_context, progress);
}
