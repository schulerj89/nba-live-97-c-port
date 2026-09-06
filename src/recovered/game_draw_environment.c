#include "game_draw_environment.h"

#include <string.h>

typedef struct Run {
  Nba97GameDrawEnvironmentContext *context;
  Nba97GameDrawEnvironmentProgress *progress;
  Nba97GameDrawEnvironmentMachine machine;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression)                                                        \
  do {                                                                         \
    int status_ = (expression);                                                \
    if (status_ != NBA97_TEXT_COMPLETE)                                        \
      return status_;                                                          \
  } while (0)

static void publish(Run *run) { run->progress->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t entry) {
  run->progress->stopped_pc = pc;
  run->progress->stopped_address = address;
  run->progress->stopped_entry = entry;
  publish(run);
}

static void set_known(Nba97GameDrawEnvironmentWord *value, uint32_t word) {
  value->word = word;
  value->known_mask = 15u;
}

static int valid_machine(const Nba97GameDrawEnvironmentMachine *machine) {
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

static int initialize(Nba97GameDrawEnvironmentContext *context,
                      Nba97GameDrawEnvironmentProgress *progress, Run *run) {
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

static Nba97GameDrawEnvironmentWord
add_words(Nba97GameDrawEnvironmentWord left,
          Nba97GameDrawEnvironmentWord right) {
  Nba97GameDrawEnvironmentWord result;
  unsigned carry_mask = 1u;
  unsigned byte;
  result.word = left.word + right.word;
  result.known_mask = 0u;
  if (left.known_mask == 15u && right.known_mask == 15u) {
    result.known_mask = 15u;
    return result;
  }
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned next_carry_mask = 0u;
    unsigned first_output = 0u;
    unsigned first = 1u;
    unsigned invariant = 1u;
    unsigned left_start = (left.known_mask & (1u << byte)) != 0u
                              ? (left.word >> (8u * byte)) & 255u
                              : 0u;
    unsigned left_end =
        (left.known_mask & (1u << byte)) != 0u ? left_start : 255u;
    unsigned right_start = (right.known_mask & (1u << byte)) != 0u
                               ? (right.word >> (8u * byte)) & 255u
                               : 0u;
    unsigned right_end =
        (right.known_mask & (1u << byte)) != 0u ? right_start : 255u;
    unsigned carry;
    for (carry = 0u; carry != 2u; ++carry) {
      unsigned a;
      if ((carry_mask & (1u << carry)) == 0u)
        continue;
      for (a = left_start; a <= left_end; ++a) {
        unsigned b;
        for (b = right_start; b <= right_end; ++b) {
          unsigned sum = a + b + carry;
          unsigned output = sum & 255u;
          next_carry_mask |= 1u << (sum >> 8u);
          if (first != 0u) {
            first_output = output;
            first = 0u;
          } else if (first_output != output) {
            invariant = 0u;
          }
        }
      }
    }
    if (invariant != 0u)
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
    carry_mask = next_carry_mask;
  }
  return result;
}

static Nba97GameDrawEnvironmentWord
add_constant(Nba97GameDrawEnvironmentWord value, uint32_t constant) {
  Nba97GameDrawEnvironmentWord immediate;
  set_known(&immediate, constant);
  return add_words(value, immediate);
}

static Nba97GameDrawEnvironmentWord
or_words(Nba97GameDrawEnvironmentWord left,
         Nba97GameDrawEnvironmentWord right) {
  Nba97GameDrawEnvironmentWord result;
  unsigned byte;
  result.word = left.word | right.word;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned mask = 1u << byte;
    unsigned left_byte = (left.word >> (byte * 8u)) & 255u;
    unsigned right_byte = (right.word >> (byte * 8u)) & 255u;
    if (((left.known_mask & mask) != 0u && (right.known_mask & mask) != 0u) ||
        ((left.known_mask & mask) != 0u && left_byte == 255u) ||
        ((right.known_mask & mask) != 0u && right_byte == 255u))
      result.known_mask = (uint8_t)(result.known_mask | mask);
  }
  return result;
}

static int spend(Run *run) {
  if (run->progress->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->progress->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width, const Nba97GameDrawEnvironmentWord *value) {
  size_t index = run->progress->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameDrawEnvironmentAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value->word;
    event->operation = run->progress->operations;
    event->width = (uint8_t)width;
    event->known_mask =
        (uint8_t)(value->known_mask & (uint8_t)((1u << width) - 1u));
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t pc, uint32_t address, unsigned width,
                  uint8_t **data, uint8_t **known_bytes) {
  size_t index, byte;
  stop(run, pc, address, 0u);
  TRY(spend(run));
  ++run->progress->accesses;
  if ((address & (width - 1u)) != 0u)
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (index = 0u; index != run->context->memory.count; ++index) {
    Nba97GameTextRegion *region = &run->context->memory.region[index];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        width > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known_bytes =
        region->known != NULL ? region->known + (size_t)offset : NULL;
    if (*known_bytes != NULL)
      for (byte = 0u; byte != width; ++byte)
        if ((*known_bytes)[byte] > 1u)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int read_memory(Run *run, uint32_t pc, uint32_t address, unsigned width,
                       Nba97GameDrawEnvironmentWord *destination) {
  Nba97GameDrawEnvironmentWord loaded = {0u, 0u};
  uint8_t *data, *known_bytes;
  unsigned byte;
  TRY(locate(run, pc, address, width, &data, &known_bytes));
  for (byte = 0u; byte != width; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (byte * 8u);
    if (known_bytes == NULL || known_bytes[byte] != 0u)
      loaded.known_mask = (uint8_t)(loaded.known_mask | (uint8_t)(1u << byte));
  }
  if (width == 1u)
    loaded.known_mask = (uint8_t)(loaded.known_mask | 14u);
  *destination = loaded;
  ++run->progress->reads;
  journal(run, NBA97_GAME_MATCH_CLOCKS_READ, pc, address, width, &loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_memory(Run *run, uint32_t pc, uint32_t address, unsigned width,
                        const Nba97GameDrawEnvironmentWord *value) {
  uint8_t *data, *known_bytes;
  unsigned byte;
  uint8_t width_mask = (uint8_t)((1u << width) - 1u);
  TRY(locate(run, pc, address, width, &data, &known_bytes));
  if (known_bytes == NULL && (value->known_mask & width_mask) != width_mask)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0u; byte != width; ++byte) {
    data[byte] = (uint8_t)(value->word >> (byte * 8u));
    if (known_bytes != NULL)
      known_bytes[byte] = (uint8_t)((value->known_mask >> byte) & 1u);
  }
  ++run->progress->stores;
  journal(run, NBA97_GAME_MATCH_CLOCKS_STORE, pc, address, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int address(Run *run, Nba97GameDrawEnvironmentWord base, uint32_t offset,
                   uint32_t pc, uint32_t *result) {
  Nba97GameDrawEnvironmentWord sum = add_constant(base, offset);
  if (sum.known_mask != 15u) {
    stop(run, pc, sum.word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *result = sum.word;
  return NBA97_TEXT_COMPLETE;
}

static int read_at(Run *run, unsigned base_register, uint32_t offset,
                   unsigned width, uint32_t pc,
                   Nba97GameDrawEnvironmentWord *destination) {
  uint32_t guest_address;
  TRY(address(run, R(base_register), offset, pc, &guest_address));
  return read_memory(run, pc, guest_address, width, destination);
}

static int write_at(Run *run, unsigned base_register, uint32_t offset,
                    unsigned width, uint32_t pc,
                    const Nba97GameDrawEnvironmentWord *value) {
  uint32_t guest_address;
  TRY(address(run, R(base_register), offset, pc, &guest_address));
  return write_memory(run, pc, guest_address, width, value);
}

static int debug_at_least_two(Run *run,
                              const Nba97GameDrawEnvironmentWord *value,
                              int *at_least_two) {
  unsigned byte;
  for (byte = 1u; byte != 4u; ++byte)
    if ((value->known_mask & (1u << byte)) != 0u &&
        ((value->word >> (byte * 8u)) & 255u) != 0u) {
      *at_least_two = 1;
      return NBA97_TEXT_COMPLETE;
    }
  if ((value->known_mask & 1u) != 0u && (value->word & 255u) >= 2u) {
    *at_least_two = 1;
    return NBA97_TEXT_COMPLETE;
  }
  if (value->known_mask == 15u) {
    *at_least_two = value->word >= 2u;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, UINT32_C(0x80099af4), 0u, 0u);
  return NBA97_TEXT_UNKNOWN;
}

static int invoke(Run *run, uint32_t pc, uint32_t entry, uint8_t kind,
                  uint8_t argument_count) {
  Nba97GameDrawEnvironmentEvent event;
  int accepted;
  stop(run, pc, 0u, entry);
  TRY(spend(run));
  if ((entry & 3u) != 0u)
    return NBA97_TEXT_ALIGNMENT_TRAP;
  memset(&event, 0, sizeof(event));
  event.pc = pc;
  event.delay_slot_pc = pc + 4u;
  event.entry = entry;
  event.operation = run->progress->operations;
  event.invocation = run->progress->call_count[kind] + 1u;
  event.kind = kind;
  event.argument_count = argument_count;
  publish(run);
  if (run->context->io == NULL)
    return NBA97_TEXT_IO_REFUSED;
  accepted = run->context->io(run->context->user, &run->context->memory, &event,
                              &run->machine);
  publish(run);
  if (accepted != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!valid_machine(&run->machine))
    return NBA97_TEXT_ARGUMENT;
  ++run->progress->callbacks_completed;
  ++run->progress->call_count[kind];
  return NBA97_TEXT_COMPLETE;
}

static int invoke_indirect(Run *run, uint32_t pc,
                           Nba97GameDrawEnvironmentWord entry, uint8_t kind,
                           uint8_t argument_count) {
  if (entry.known_mask != 15u) {
    stop(run, pc, 0u, entry.word);
    return NBA97_TEXT_UNKNOWN;
  }
  return invoke(run, pc, entry.word, kind, argument_count);
}

static int restore(Run *run, uint32_t pc, uint32_t offset, unsigned reg,
                   Nba97GameDrawEnvironmentWord *restored) {
  Nba97GameDrawEnvironmentWord loaded;
  TRY(read_at(run, NBA97_MATCH_INITIALIZE_SP, offset, 4u, pc, &loaded));
  R(reg) = loaded;
  *restored = loaded;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_draw_environment(Nba97GameDrawEnvironmentContext *context,
                                Nba97GameDrawEnvironmentProgress *progress) {
  Run storage;
  Run *run = &storage;
  Nba97GameDrawEnvironmentWord debug, loaded, tagged;
  int debug_enabled;

  TRY(initialize(context, progress, run));

  /* 0x80099ACC..0x80099AE4: allocate the wrapping frame and save s2, ra,
   * s1, and s0 in the source's non-address order. */
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe0));
  progress->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x18u, 4u, UINT32_C(0x80099ad0),
               &R(NBA97_GAME_MATCH_CLOCKS_S2)));
  set_known(&R(NBA97_GAME_MATCH_CLOCKS_S2), UINT32_C(0x800c0000));
  R(NBA97_GAME_MATCH_CLOCKS_S2) =
      add_constant(R(NBA97_GAME_MATCH_CLOCKS_S2), UINT32_C(0x000055c2));
  TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x1cu, 4u, UINT32_C(0x80099adc),
               &R(NBA97_MATCH_INITIALIZE_RA)));
  TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x14u, 4u, UINT32_C(0x80099ae0),
               &R(NBA97_GAME_MATCH_CLOCKS_S1)));
  TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x10u, 4u, UINT32_C(0x80099ae4),
               &R(NBA97_MATCH_INITIALIZE_S0)));

  /* 0x80099AE8..0x80099B14: the branch delay captures a0 in s1 even if the
   * debug predicate is unknown. Values 0 and 1 skip the indirect diagnostic. */
  TRY(read_at(run, NBA97_GAME_MATCH_CLOCKS_S2, 0u, 1u, UINT32_C(0x80099ae8),
              &debug));
  R(NBA97_MATCH_INITIALIZE_V0).word = debug.word < 2u ? 1u : 0u;
  R(NBA97_MATCH_INITIALIZE_V0).known_mask = 14u;
  R(NBA97_GAME_MATCH_CLOCKS_S1) = R(NBA97_MATCH_INITIALIZE_A0);
  TRY(debug_at_least_two(run, &debug, &debug_enabled));
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), debug_enabled != 0 ? 0u : 1u);
  if (debug_enabled != 0) {
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80030000));
    R(NBA97_MATCH_INITIALIZE_A0) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0xffff836c));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
    TRY(read_at(run, NBA97_MATCH_INITIALIZE_V0, 0x55bcu, 4u,
                UINT32_C(0x80099b08), &loaded));
    R(NBA97_MATCH_INITIALIZE_V0) = loaded;
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x80099b18));
    R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_GAME_MATCH_CLOCKS_S1);
    TRY(invoke_indirect(run, UINT32_C(0x80099b10), R(NBA97_MATCH_INITIALIZE_V0),
                        NBA97_GAME_DRAW_ENVIRONMENT_DEBUG_INDIRECT, 2u));
  }

  /* 0x80099B18..0x80099B24: the packet builder consumes callback-live s1;
   * its delay slot publishes callback-live s1 in a1. */
  R(NBA97_MATCH_INITIALIZE_S0) =
      add_constant(R(NBA97_GAME_MATCH_CLOCKS_S1), 0x1cu);
  R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0);
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x80099b28));
  R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_GAME_MATCH_CLOCKS_S1);
  TRY(invoke(run, UINT32_C(0x80099b20), UINT32_C(0x8009a344),
             NBA97_GAME_DRAW_ENVIRONMENT_PACKET_8009A344, 2u));

  /* 0x80099B28..0x80099B58: capture the dynamic table pointer before tagging
   * the packet. That captured pointer survives native packet/table aliases. */
  set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x00ffffff));
  R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_MATCH_INITIALIZE_S0);
  set_known(&R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x00000040));
  TRY(read_at(run, NBA97_GAME_MATCH_CLOCKS_S1, 0x1cu, 4u, UINT32_C(0x80099b38),
              &loaded));
  R(NBA97_MATCH_INITIALIZE_V0) = loaded;
  set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x800c0000));
  TRY(read_at(run, NBA97_MATCH_INITIALIZE_V1, 0x55b8u, 4u, UINT32_C(0x80099b40),
              &loaded));
  R(NBA97_MATCH_INITIALIZE_V1) = loaded;
  tagged = or_words(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_A0));
  R(NBA97_MATCH_INITIALIZE_V0) = tagged;
  TRY(write_at(run, NBA97_GAME_MATCH_CLOCKS_S1, 0x1cu, 4u, UINT32_C(0x80099b48),
               &R(NBA97_MATCH_INITIALIZE_V0)));
  TRY(read_at(run, NBA97_MATCH_INITIALIZE_V1, 0x18u, 4u, UINT32_C(0x80099b4c),
              &loaded));
  R(NBA97_MATCH_INITIALIZE_A0) = loaded;
  TRY(read_at(run, NBA97_MATCH_INITIALIZE_V1, 8u, 4u, UINT32_C(0x80099b50),
              &loaded));
  R(NBA97_MATCH_INITIALIZE_V0) = loaded;
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x80099b60));
  set_known(&R(NBA97_MATCH_INITIALIZE_A3), 0u);
  TRY(invoke_indirect(run, UINT32_C(0x80099b58), R(NBA97_MATCH_INITIALIZE_V0),
                      NBA97_GAME_DRAW_ENVIRONMENT_SUBMIT_INDIRECT, 4u));

  /* 0x80099B60..0x80099B6C: form the copy arguments from callback-live s2/s1;
   * the delay slot fixes the exact 0x5C-byte extent. */
  R(NBA97_MATCH_INITIALIZE_A0) =
      add_constant(R(NBA97_GAME_MATCH_CLOCKS_S2), 0x0eu);
  R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_GAME_MATCH_CLOCKS_S1);
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x80099b70));
  set_known(&R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x0000005c));
  TRY(invoke(run, UINT32_C(0x80099b68), UINT32_C(0x8009cb0c),
             NBA97_GAME_DRAW_ENVIRONMENT_COPY_8009CB0C, 3u));

  /* 0x80099B70..0x80099B8C: ignore copy v0, capture callback-live s1, then
   * restore through callback-live sp before validating the JR target. */
  R(NBA97_MATCH_INITIALIZE_V0) = R(NBA97_GAME_MATCH_CLOCKS_S1);
  TRY(restore(run, UINT32_C(0x80099b74), 0x1cu, NBA97_MATCH_INITIALIZE_RA,
              &progress->restored_return_address));
  TRY(restore(run, UINT32_C(0x80099b78), 0x18u, NBA97_GAME_MATCH_CLOCKS_S2,
              &progress->restored_s2));
  TRY(restore(run, UINT32_C(0x80099b7c), 0x14u, NBA97_GAME_MATCH_CLOCKS_S1,
              &progress->restored_s1));
  TRY(restore(run, UINT32_C(0x80099b80), 0x10u, NBA97_MATCH_INITIALIZE_S0,
              &progress->restored_s0));
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x20u);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15u) {
    stop(run, UINT32_C(0x80099b88), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }

  progress->completed = 1u;
  stop(run, 0u, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}
