#include "frontend_resource_lookup_adapter.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
constexpr std::uint8_t FE = NBA97_FRONTEND_MAIN_PROGRAM_FEONLY;
constexpr Nba97FrontendResourceLookupSiteContract Contracts[] = {
    {0, 0, 0, 0, 0},
    {0x8008a2e0u, 0x8008a2e4u, 0x8008a0a8u, 1, FE},
    {0x8008a314u, 0x8008a318u, 0x800771f0u, 4, FE},
    {0x8008a33cu, 0x8008a340u, 0x800909a8u, 3, FE},
    {0x8008a344u, 0x8008a348u, 0x80077638u, 1, FE},
    {0x8008a36cu, 0x8008a370u, 0x80089ffcu, 2, FE},
    {0x8008a3c4u, 0x8008a3c8u, 0x800771f0u, 4, FE},
    {0x8008a3dcu, 0x8008a3e0u, 0x800909a8u, 3, FE}};
static_assert(sizeof Contracts / sizeof Contracts[0] ==
              NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_COUNT);

bool machineValid(const Nba97FrontendResourceLookupMachine &machine) {
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

struct Children {
  Nba97FrontendResourceLookupBinding *binding;
};

int childDispatch(void *opaque, const Nba97GameTextMemory *memory,
                  const Nba97FrontendResourceLookupEvent *event,
                  Nba97FrontendResourceLookupMachine *machine) {
  auto *children = static_cast<Children *>(opaque);
  if (!children || !children->binding || !memory || !event || !machine)
    return 0;
  auto &owner = *children->binding;
  unsigned copyIndex;
  if (event->site == NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A33C)
    copyIndex = 0;
  else if (event->site == NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A3DC)
    copyIndex = 1;
  else
    return owner.io ? owner.io(owner.user, memory, event, machine) : 0;

  auto &copy = owner.copy[copyIndex];
  Nba97FrontendResourceLookupSiteContract contract{};
  copy.parent_event = *event;
  copy.parent_machine = *machine;
  ++copy.invocations;
  if (!nba97_frontend_resource_lookup_site_contract(event->site, &contract) ||
      event->pc != contract.pc ||
      event->delay_slot_pc != contract.delay_slot_pc ||
      event->entry != contract.target ||
      event->argument_count != contract.argument_count ||
      event->target_program != contract.target_program ||
      event->invocation != 1 ||
      machine->registers.gpr[NBA97_FRONTEND_RESOURCE_LOOKUP_RA].word !=
          event->pc + 8u ||
      machine->registers.gpr[NBA97_FRONTEND_RESOURCE_LOOKUP_RA].known_mask !=
          15 ||
      (!copy.access_journal && copy.access_journal_capacity) ||
      (!copy.instruction_journal && copy.instruction_journal_capacity)) {
    copy.result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  Nba97FrontendMemoryCopyContext context{};
  context.memory = *memory;
  context.operation_budget = copy.operation_budget;
  context.machine = *machine;
  context.access_journal = copy.access_journal;
  context.access_journal_capacity = copy.access_journal_capacity;
  context.instruction_journal = copy.instruction_journal;
  context.instruction_journal_capacity = copy.instruction_journal_capacity;
  copy.result = nba97_frontend_memory_copy(&context, &copy.progress);
  *machine = copy.progress.machine;
  if (copy.result != NBA97_TEXT_COMPLETE || !copy.progress.completed)
    return 0;
  ++copy.completions;
  return 1;
}

int runLookup(Nba97FrontendResourceLookupBinding &binding,
              const Nba97GameTextMemory &memory,
              Nba97FrontendResourceLookupMachine &machine) {
  Children children{&binding};
  Nba97FrontendResourceLookupContext context{};
  context.memory = memory;
  context.operation_budget = binding.operation_budget;
  context.machine = machine;
  context.io = childDispatch;
  context.user = &children;
  context.access_journal = binding.access_journal;
  context.access_journal_capacity = binding.access_journal_capacity;
  context.instruction_journal = binding.instruction_journal;
  context.instruction_journal_capacity = binding.instruction_journal_capacity;
  binding.result = nba97_frontend_resource_lookup(&context, &binding.progress);
  machine = binding.progress.machine;
  return binding.result;
}

struct Composition {
  Nba97FrontendResourceLoadIo fallback;
  void *fallbackUser;
  Nba97FrontendResourceLookupBinding *binding;
  Nba97FrontendResourceLookupAdapterProgress *progress;
};

int composed(void *opaque, const Nba97GameTextMemory *memory,
             const Nba97FrontendResourceLoadEvent *event,
             Nba97FrontendResourceLoadMachine *machine) {
  auto &composition = *static_cast<Composition *>(opaque);
  if (event && event->site == NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B1F0) {
    std::size_t before = composition.binding->invocations;
    int accepted = nba97_frontend_resource_lookup_from_resource_load(
        composition.binding, memory, event, machine);
    if (composition.binding->invocations != before) {
      ++composition.progress->invocations;
      if (composition.binding->result == NBA97_TEXT_COMPLETE)
        ++composition.progress->completions;
      composition.progress->parent_event = composition.binding->parent_event;
      composition.progress->parent_machine =
          composition.binding->parent_machine;
      composition.progress->progress = composition.binding->progress;
      composition.progress->result = composition.binding->result;
    }
    return accepted;
  }
  return composition.fallback ? composition.fallback(composition.fallbackUser,
                                                     memory, event, machine)
                              : 0;
}
} // namespace

int nba97_frontend_resource_lookup_site_contract(
    uint8_t site, Nba97FrontendResourceLookupSiteContract *contract) {
  if (!contract || !site || site >= NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_COUNT)
    return 0;
  *contract = Contracts[site];
  return 1;
}

int nba97_frontend_resource_lookup_from_resource_load(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97FrontendResourceLoadEvent *event,
    Nba97FrontendResourceLoadMachine *machine) {
  auto *binding = static_cast<Nba97FrontendResourceLookupBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) ||
      event->site != NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B1F0 ||
      event->pc != 0x8007b1f0u || event->delay_slot_pc != 0x8007b1f4u ||
      event->entry != 0x8008a2c8u || event->invocation != 1 ||
      event->argument_count != 1 || event->target_program != FE ||
      machine->registers.gpr[NBA97_FRONTEND_RESOURCE_LOOKUP_RA].word !=
          0x8007b1f8u ||
      machine->registers.gpr[NBA97_FRONTEND_RESOURCE_LOOKUP_RA].known_mask !=
          15 ||
      (!binding->access_journal && binding->access_journal_capacity) ||
      (!binding->instruction_journal &&
       binding->instruction_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->parent_event = *event;
  binding->parent_machine = *machine;
  if (runLookup(*binding, *memory, *machine) != NBA97_TEXT_COMPLETE ||
      !binding->progress.completed)
    return 0;
  ++binding->completions;
  return 1;
}

int nba97_frontend_resource_load_with_recovered_lookup(
    Nba97FrontendResourceLoadContext *context,
    Nba97FrontendResourceLookupBinding *binding,
    Nba97FrontendResourceLoadProgress *progress,
    Nba97FrontendResourceLookupAdapterProgress *adapterProgress) {
  if (!adapterProgress)
    return NBA97_TEXT_ARGUMENT;
  std::memset(adapterProgress, 0, sizeof *adapterProgress);
  if (!context || !binding || !progress)
    return NBA97_TEXT_ARGUMENT;
  Composition composition{context->io, context->user, binding, adapterProgress};
  auto composedContext = *context;
  composedContext.io = composed;
  composedContext.user = &composition;
  return nba97_frontend_resource_load(&composedContext, progress);
}
