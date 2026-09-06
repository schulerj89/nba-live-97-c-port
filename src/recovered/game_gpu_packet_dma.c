#include "game_gpu_packet_dma.h"

#include <string.h>

typedef struct Run {
  Nba97GameGpuPacketDmaContext *context;
  Nba97GameGpuPacketDmaProgress *progress;
  Nba97GameGpuPacketDmaMachine machine;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression)                                                        \
  do {                                                                         \
    int status_ = (expression);                                                \
    if (status_ != NBA97_TEXT_COMPLETE)                                        \
      return status_;                                                          \
  } while (0)

static void publish(Run *run) { run->progress->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address) {
  run->progress->stopped_pc = pc;
  run->progress->stopped_address = address;
  publish(run);
}

static void set_known(Nba97GameGpuPacketDmaWord *value, uint32_t word) {
  value->word = word;
  value->known_mask = 15u;
}

static int valid_machine(const Nba97GameGpuPacketDmaMachine *machine) {
  unsigned index;
  if (machine->registers.gpr[0].word != 0u ||
      machine->registers.gpr[0].known_mask != 15u ||
      machine->hi.known_mask > 15u || machine->lo.known_mask > 15u)
    return 0;
  for (index = 0u; index != 32u; ++index)
    if (machine->registers.gpr[index].known_mask > 15u)
      return 0;
  return 1;
}

static int valid_memory(const Nba97GameTextMemory *memory) {
  size_t index, earlier;
  if (memory->count != 0u && memory->region == NULL)
    return 0;
  for (index = 0u; index != memory->count; ++index) {
    const Nba97GameTextRegion *region = &memory->region[index];
    if (region->data == NULL || region->size == 0u ||
        region->size > UINT64_C(0x100000000) ||
        (uint64_t)region->base + region->size > UINT64_C(0x100000000))
      return 0;
    for (earlier = 0u; earlier != index; ++earlier) {
      const Nba97GameTextRegion *other = &memory->region[earlier];
      if ((uint64_t)region->base < (uint64_t)other->base + other->size &&
          (uint64_t)other->base < (uint64_t)region->base + region->size)
        return 0;
    }
  }
  return 1;
}

static int initialize(Nba97GameGpuPacketDmaContext *context,
                      Nba97GameGpuPacketDmaProgress *progress, Run *run) {
  if (progress == NULL)
    return NBA97_TEXT_ARGUMENT;
  memset(progress, 0, sizeof(*progress));
  if (context == NULL || !valid_memory(&context->memory) ||
      !valid_machine(&context->machine) ||
      (context->access_journal_capacity != 0u &&
       context->access_journal == NULL))
    return NBA97_TEXT_ARGUMENT;
  run->context = context;
  run->progress = progress;
  run->machine = context->machine;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int spend(Run *run) {
  if (run->progress->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->progress->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    const Nba97GameGpuPacketDmaWord *value) {
  size_t index = run->progress->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameGpuPacketDmaAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value->word;
    event->operation = run->progress->operations;
    event->width = 4u;
    event->known_mask = value->known_mask;
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t pc, uint32_t address, uint8_t **data,
                  uint8_t **known) {
  size_t index, byte;
  stop(run, pc, address);
  TRY(spend(run));
  ++run->progress->accesses;
  if ((address & 3u) != 0u)
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (index = 0u; index != run->context->memory.count; ++index) {
    Nba97GameTextRegion *region = &run->context->memory.region[index];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        4u > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known = region->known != NULL ? region->known + (size_t)offset : NULL;
    if (*known != NULL)
      for (byte = 0u; byte != 4u; ++byte)
        if ((*known)[byte] > 1u)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int read_word(Run *run, uint32_t pc, uint32_t address,
                     Nba97GameGpuPacketDmaWord *destination) {
  Nba97GameGpuPacketDmaWord loaded = {0u, 0u};
  uint8_t *data, *known;
  unsigned byte;
  TRY(locate(run, pc, address, &data, &known));
  for (byte = 0u; byte != 4u; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (8u * byte);
    if (known == NULL || known[byte] != 0u)
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << byte));
  }
  *destination = loaded;
  ++run->progress->reads;
  journal(run, NBA97_GAME_MATCH_CLOCKS_READ, pc, address, &loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_word(Run *run, uint32_t pc, uint32_t address,
                      const Nba97GameGpuPacketDmaWord *source) {
  uint8_t *data, *known;
  unsigned byte;
  TRY(locate(run, pc, address, &data, &known));
  if (known == NULL && source->known_mask != 15u)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0u; byte != 4u; ++byte) {
    data[byte] = (uint8_t)(source->word >> (8u * byte));
    if (known != NULL)
      known[byte] = (uint8_t)((source->known_mask >> byte) & 1u);
  }
  ++run->progress->stores;
  journal(run, NBA97_GAME_MATCH_CLOCKS_STORE, pc, address, source);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int port_address(Run *run, uint32_t pc,
                        Nba97GameGpuPacketDmaWord pointer,
                        uint32_t *address) {
  if (pointer.known_mask != 15u) {
    stop(run, pc, pointer.word);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = pointer.word;
  return NBA97_TEXT_COMPLETE;
}

static int write_loaded_port(Run *run, uint32_t pc,
                             const Nba97GameGpuPacketDmaWord *value) {
  uint32_t address;
  TRY(port_address(run, pc, R(NBA97_MATCH_INITIALIZE_V0), &address));
  return write_word(run, pc, address, value);
}

int nba97_game_gpu_packet_dma(Nba97GameGpuPacketDmaContext *context,
                              Nba97GameGpuPacketDmaProgress *progress) {
  Run storage;
  Run *run = &storage;

  TRY(initialize(context, progress, run));

  /* 0x8009B1F8..0x8009B208: publish the DMA direction/control word through
   * the first live port pointer. Both LUI effects precede its pointer load. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x04000000));
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
  TRY(read_word(run, UINT32_C(0x8009b200), UINT32_C(0x800c5694),
                &R(NBA97_MATCH_INITIALIZE_V0)));
  set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x04000002));
  TRY(write_loaded_port(run, UINT32_C(0x8009b208),
                        &R(NBA97_MATCH_INITIALIZE_V1)));

  /* 0x8009B20C..0x8009B218: reload the second pointer after the first store,
   * then write raw a0 bits and their exact per-byte knownness. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
  TRY(read_word(run, UINT32_C(0x8009b210), UINT32_C(0x800c5698),
                &R(NBA97_MATCH_INITIALIZE_V0)));
  TRY(write_loaded_port(run, UINT32_C(0x8009b218),
                        &R(NBA97_MATCH_INITIALIZE_A0)));

  /* 0x8009B21C..0x8009B228: the 0x01000000 LUI is observable before the
   * third port store even though that store writes architectural zero. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
  TRY(read_word(run, UINT32_C(0x8009b220), UINT32_C(0x800c569c),
                &R(NBA97_MATCH_INITIALIZE_V0)));
  set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x01000000));
  TRY(write_loaded_port(run, UINT32_C(0x8009b228),
                        &R(NBA97_MATCH_INITIALIZE_ZERO)));

  /* 0x8009B22C..0x8009B240: reload the final aliased pointer, store the DMA
   * start word, then consume live ra only after all eight mapped accesses. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
  TRY(read_word(run, UINT32_C(0x8009b230), UINT32_C(0x800c56a0),
                &R(NBA97_MATCH_INITIALIZE_V0)));
  set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x01000401));
  TRY(write_loaded_port(run, UINT32_C(0x8009b238),
                        &R(NBA97_MATCH_INITIALIZE_V1)));
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15u) {
    stop(run, UINT32_C(0x8009b23c), 0u);
    return NBA97_TEXT_UNKNOWN;
  }

  progress->completed = 1u;
  stop(run, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}
