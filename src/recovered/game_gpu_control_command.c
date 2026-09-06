#include "game_gpu_control_command.h"

#include <string.h>

#define REG(run, index) ((run)->machine.registers.gpr[(index)])
#define TRY(expression)                                                        \
  do {                                                                         \
    int result_ = (expression);                                                \
    if (result_ != NBA97_TEXT_COMPLETE)                                        \
      return result_;                                                          \
  } while (0)

typedef struct Nba97GameGpuControlCommandRun {
  Nba97GameGpuControlCommandContext *context;
  Nba97GameGpuControlCommandProgress *progress;
  Nba97GameGpuControlCommandMachine machine;
} Nba97GameGpuControlCommandRun;

static void publish(Nba97GameGpuControlCommandRun *run) {
  run->progress->machine = run->machine;
}

static void stop(Nba97GameGpuControlCommandRun *run, uint32_t pc,
                 uint32_t address) {
  run->progress->stopped_pc = pc;
  run->progress->stopped_address = address;
  publish(run);
}

static void assign_known(Nba97GameGpuControlCommandWord *destination,
                         uint32_t value) {
  destination->word = value;
  destination->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameGpuControlCommandMachine *machine) {
  unsigned index;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (index = 0; index < 32; ++index)
    if (machine->registers.gpr[index].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int initialize(Nba97GameGpuControlCommandContext *context,
                      Nba97GameGpuControlCommandProgress *progress,
                      Nba97GameGpuControlCommandRun *run) {
  size_t i;
  size_t j;
  if (!progress)
    return NBA97_TEXT_ARGUMENT;
  memset(progress, 0, sizeof(*progress));
  if (!context || (!context->memory.region && context->memory.count) ||
      (!context->access_journal && context->access_journal_capacity) ||
      !machine_valid(&context->machine))
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < context->memory.count; ++i) {
    Nba97GameTextRegion *region = &context->memory.region[i];
    if (!region->data || region->size == 0 ||
        region->size > UINT64_C(0x100000000) ||
        (uint64_t)region->base + region->size > UINT64_C(0x100000000))
      return NBA97_TEXT_ARGUMENT;
    for (j = 0; j < i; ++j) {
      Nba97GameTextRegion *earlier = &context->memory.region[j];
      if ((uint64_t)region->base < (uint64_t)earlier->base + earlier->size &&
          (uint64_t)earlier->base < (uint64_t)region->base + region->size)
        return NBA97_TEXT_ARGUMENT;
    }
  }
  run->context = context;
  run->progress = progress;
  run->machine = context->machine;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int spend(Nba97GameGpuControlCommandRun *run) {
  if (run->progress->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->progress->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameGpuControlCommandRun *run, uint8_t kind,
                    uint32_t pc, uint32_t address, unsigned width,
                    Nba97GameGpuControlCommandWord value) {
  size_t index = run->progress->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameGpuControlCommandAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value.word;
    event->operation = run->progress->operations;
    event->width = (uint8_t)width;
    event->known_mask =
        (uint8_t)(value.known_mask & ((UINT32_C(1) << width) - 1u));
    event->kind = kind;
  }
}

static int locate(Nba97GameGpuControlCommandRun *run, uint32_t address,
                  unsigned width, unsigned alignment, uint32_t pc,
                  uint8_t **data, uint8_t **known) {
  size_t region_index;
  size_t byte_index;
  stop(run, pc, address);
  TRY(spend(run));
  ++run->progress->accesses;
  if (address & (alignment - 1u))
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (region_index = 0; region_index < run->context->memory.count;
       ++region_index) {
    Nba97GameTextRegion *region = &run->context->memory.region[region_index];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        width > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known = region->known ? region->known + (size_t)offset : NULL;
    if (*known)
      for (byte_index = 0; byte_index < width; ++byte_index)
        if ((*known)[byte_index] > 1u)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int read_word(Nba97GameGpuControlCommandRun *run, uint32_t address,
                     uint32_t pc, Nba97GameGpuControlCommandWord *destination) {
  uint8_t *data;
  uint8_t *known;
  Nba97GameGpuControlCommandWord loaded = {0, 0};
  unsigned byte;
  TRY(locate(run, address, 4, 4, pc, &data, &known));
  for (byte = 0; byte < 4; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (8u * byte);
    if (!known || known[byte])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << byte));
  }
  *destination = loaded;
  ++run->progress->reads;
  journal(run, NBA97_GAME_GPU_CONTROL_COMMAND_READ, pc, address, 4, loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int store(Nba97GameGpuControlCommandRun *run, uint32_t address,
                 unsigned width, unsigned alignment, uint32_t pc,
                 Nba97GameGpuControlCommandWord value) {
  uint8_t *data;
  uint8_t *known;
  uint8_t required = (uint8_t)((1u << width) - 1u);
  unsigned byte;
  TRY(locate(run, address, width, alignment, pc, &data, &known));
  if (!known && (value.known_mask & required) != required)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0; byte < width; ++byte) {
    data[byte] = (uint8_t)(value.word >> (8u * byte));
    if (known)
      known[byte] = (uint8_t)((value.known_mask >> byte) & 1u);
  }
  ++run->progress->stores;
  journal(run, NBA97_GAME_GPU_CONTROL_COMMAND_STORE, pc, address, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int known_address(Nba97GameGpuControlCommandRun *run,
                         Nba97GameGpuControlCommandWord address, uint32_t pc,
                         uint32_t *result) {
  if (address.known_mask != 0x0fu) {
    stop(run, pc, address.word);
    return NBA97_TEXT_UNKNOWN;
  }
  *result = address.word;
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_gpu_control_command(
    Nba97GameGpuControlCommandContext *context,
    Nba97GameGpuControlCommandProgress *progress) {
  Nba97GameGpuControlCommandRun state;
  Nba97GameGpuControlCommandRun *run = &state;
  Nba97GameGpuControlCommandWord cache_address;
  uint32_t address;
  TRY(initialize(context, progress, run));

  /* 0x8009B16C..0x8009B174: retain the LUI prefix if the pointer load fails. */
  assign_known(&REG(run, NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
  TRY(read_word(run, UINT32_C(0x800c5694), UINT32_C(0x8009b170),
                &REG(run, NBA97_MATCH_INITIALIZE_V0)));
  progress->port_pointer = REG(run, NBA97_MATCH_INITIALIZE_V0);

  /* 0x8009B178: the complete command reaches the dynamic port first. */
  TRY(known_address(run, REG(run, NBA97_MATCH_INITIALIZE_V0),
                    UINT32_C(0x8009b178), &address));
  TRY(store(run, address, 4, 4, UINT32_C(0x8009b178),
            REG(run, NBA97_MATCH_INITIALIZE_A0)));

  /* 0x8009B17C..0x8009B188: classify by command byte 3 and cache byte 0. */
  REG(run, NBA97_MATCH_INITIALIZE_V0).word =
      REG(run, NBA97_MATCH_INITIALIZE_A0).word >> 24;
  REG(run, NBA97_MATCH_INITIALIZE_V0).known_mask =
      (uint8_t)(0x0eu |
                ((REG(run, NBA97_MATCH_INITIALIZE_A0).known_mask & 8u) ? 1u
                                                                       : 0u));
  progress->command_byte = REG(run, NBA97_MATCH_INITIALIZE_V0);
  assign_known(&REG(run, NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800e0000));
  REG(run, NBA97_MATCH_INITIALIZE_AT).word +=
      REG(run, NBA97_MATCH_INITIALIZE_V0).word;
  REG(run, NBA97_MATCH_INITIALIZE_AT).known_mask =
      REG(run, NBA97_MATCH_INITIALIZE_V0).known_mask;
  cache_address.word =
      REG(run, NBA97_MATCH_INITIALIZE_AT).word + UINT32_C(0xffff8d94);
  cache_address.known_mask =
      REG(run, NBA97_MATCH_INITIALIZE_AT).known_mask == 0x0fu ? 0x0fu : 0;
  progress->cache_address = cache_address;
  TRY(known_address(run, cache_address, UINT32_C(0x8009b188), &address));
  TRY(store(run, address, 1, 1, UINT32_C(0x8009b188),
            REG(run, NBA97_MATCH_INITIALIZE_A0)));

  /* 0x8009B18C..0x8009B190: JR consumes live ra after its NOP delay. */
  if (REG(run, NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x8009b18c), 0);
    return NBA97_TEXT_UNKNOWN;
  }
  progress->completed = 1;
  stop(run, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
