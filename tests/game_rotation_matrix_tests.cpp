#include "recovered/game_rotation_matrix.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "rotation matrix check %u failed at %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Angles = 0x80010000u;
constexpr std::uint32_t Matrix = 0x80011000u;
constexpr std::uint32_t Table = 0x800b3254u;

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x100000u, 0);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x100000u, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameRotationMatrixAccess, 32> journal{};
  Nba97GameRotationMatrixContext context{};
  Nba97GameRotationMatrixProgress progress{};
  Nba97GameRotationMatrixMachine incoming{};

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      incoming.registers.gpr[i] = {0x21000000u + i * 0x01010101u,
                                   static_cast<std::uint8_t>((i % 15u) + 1u)};
    incoming.registers.gpr[0] = {0, 15};
    incoming.registers.gpr[4] = {Angles, 15};
    incoming.registers.gpr[5] = {Matrix, 15};
    incoming.registers.gpr[29] = {0x800ff000u, 7};
    incoming.registers.gpr[31] = {0x81234568u, 15};
    incoming.hi = {0x13572468u, 5};
    incoming.lo = {0x89abcdefu, 10};
    context.memory = {&region, 1};
    context.operation_budget = 1000;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned i = 0; i < 4096; ++i)
      put(Table + i * 4u, 0x10001000u, 4);
    angles(0, 0, 0);
    std::fill(bytes.begin() + offset(Matrix),
              bytes.begin() + offset(Matrix) + 18, 0x5a);
  }
  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - Ram);
  }
  void put(std::uint32_t address, std::uint32_t value, unsigned width,
           std::uint8_t mask = 15) {
    const auto at = offset(address);
    for (unsigned i = 0; i < width; ++i) {
      bytes[at + i] = static_cast<std::uint8_t>(value >> (8u * i));
      known[at + i] = static_cast<std::uint8_t>((mask >> i) & 1u);
    }
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t value = 0;
    const auto at = offset(address);
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[at + i]) << (8u * i);
    return value;
  }
  void angles(std::int16_t x, std::int16_t y, std::int16_t z) {
    put(Angles, static_cast<std::uint16_t>(x), 2);
    put(Angles + 2, static_cast<std::uint16_t>(y), 2);
    put(Angles + 4, static_cast<std::uint16_t>(z), 2);
  }
  int run() {
    context.machine = incoming;
    return nba97_game_rotation_matrix(&context, &progress);
  }
};

std::int16_t half(std::uint32_t word) {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(word));
}

struct ProductMasks {
  std::uint8_t lo;
  std::uint8_t hi;
};

bool word_matches(std::uint32_t word, Nba97GameRotationMatrixWord observed) {
  for (unsigned byte = 0; byte < 4; ++byte)
    if ((observed.known_mask & (1u << byte)) != 0 &&
        ((word >> (byte * 8u)) & 0xffu) !=
            ((observed.word >> (byte * 8u)) & 0xffu))
      return false;
  return true;
}

std::vector<std::uint32_t>
half_completions(Nba97GameRotationMatrixWord observed,
                 bool include_negated_extreme) {
  std::vector<std::uint32_t> result;
  for (std::uint32_t half_word = 0; half_word < 0x10000u; ++half_word) {
    std::uint32_t word = half_word;
    if ((word & 0x8000u) != 0)
      word |= 0xffff0000u;
    if (word_matches(word, observed))
      result.push_back(word);
  }
  if (include_negated_extreme && word_matches(0x00008000u, observed))
    result.push_back(0x00008000u);
  return result;
}

ProductMasks completed_product_masks(Nba97GameRotationMatrixWord left,
                                     bool left_negated_extreme,
                                     Nba97GameRotationMatrixWord right,
                                     bool right_negated_extreme) {
  const auto left_words = half_completions(left, left_negated_extreme);
  const auto right_words = half_completions(right, right_negated_extreme);
  const std::uint64_t representative =
      std::uint64_t(left.word) * std::uint64_t(right.word);
  ProductMasks result{15, 15};
  for (const auto left_word : left_words)
    for (const auto right_word : right_words) {
      const std::uint64_t product =
          std::uint64_t(left_word) * std::uint64_t(right_word);
      for (unsigned byte = 0; byte < 4; ++byte) {
        if (((product >> (byte * 8u)) & 0xffu) !=
            ((representative >> (byte * 8u)) & 0xffu))
          result.lo = static_cast<std::uint8_t>(result.lo & ~(1u << byte));
        if (((product >> (32u + byte * 8u)) & 0xffu) !=
            ((representative >> (32u + byte * 8u)) & 0xffu))
          result.hi = static_cast<std::uint8_t>(result.hi & ~(1u << byte));
      }
    }
  return result;
}

void all_signs_and_store_order() {
  for (unsigned signs = 0; signs < 8; ++signs) {
    Fixture f;
    const std::int32_t sx = (signs & 1u) ? -4096 : 4096;
    const std::int32_t sy = (signs & 2u) ? -4096 : 4096;
    const std::int32_t sz = (signs & 4u) ? -4096 : 4096;
    f.angles((signs & 1u) ? -1 : 1, (signs & 2u) ? -1 : 1,
             (signs & 4u) ? -1 : 1);
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.instruction_count > 100u);
    check(half(f.get(Matrix + 4, 2)) == sy &&
          half(f.get(Matrix + 0x0a, 2)) == -sx &&
          half(f.get(Matrix + 0x10, 2)) == 4096);
    check(half(f.get(Matrix, 2)) == 4096 && half(f.get(Matrix + 2, 2)) == -sz);
    check(half(f.get(Matrix + 6, 2)) == sz + sy * sx / 4096 &&
          half(f.get(Matrix + 0x0c, 2)) == sz * sx / 4096 - sy);
    check(half(f.get(Matrix + 8, 2)) ==
              4096 - std::int64_t(sz) * sy * sx / (4096 * 4096) &&
          half(f.get(Matrix + 0x0e, 2)) == sx + sz * sy / 4096);
    constexpr std::array<std::uint32_t, 9> store_pcs{{
        0x80056154u,
        0x80056168u,
        0,
        0x800561ecu,
        0x80056204u,
        0x8005623cu,
        0x80056264u,
        0x8005629cu,
        0x800562c0u,
    }};
    constexpr std::array<unsigned, 9> offsets{
        {4, 0x0a, 0x10, 0, 2, 6, 0x0c, 8, 0x0e}};
    unsigned store = 0;
    for (unsigned i = 0; i < f.progress.access_events; ++i)
      if (f.journal[i].kind == NBA97_GAME_MATCH_CLOCKS_STORE) {
        const auto expected_pc =
            store == 2 ? ((signs & 4u) ? 0x8005617cu : 0x800561bcu)
                       : store_pcs[store];
        check(f.journal[i].pc == expected_pc &&
              f.journal[i].address == Matrix + offsets[store] &&
              f.journal[i].width == 2);
        ++store;
      }
    check(store == 9 && f.progress.stores == 9 && f.progress.reads == 6);
  }
}

void extrema_indices_and_unsigned_hilo() {
  constexpr std::array<std::int16_t, 8> values{{
      INT16_MIN,
      -4096,
      -1,
      0,
      1,
      4095,
      4096,
      INT16_MAX,
  }};
  for (auto value : values) {
    Fixture f;
    f.angles(value, value, value);
    check(f.run() == NBA97_TEXT_COMPLETE);
    const unsigned index = (value < 0 ? 0u - static_cast<std::uint32_t>(value)
                                      : static_cast<std::uint32_t>(value)) &
                           0xfffu;
    unsigned table_reads = 0;
    for (unsigned i = 0; i < f.progress.access_events; ++i)
      if (f.journal[i].width == 4) {
        check(f.journal[i].address == Table + index * 4u);
        ++table_reads;
      }
    check(table_reads == 3);
  }

  Fixture unsigned_product;
  for (unsigned i = 0; i < 4096; ++i)
    unsigned_product.put(Table + i * 4u, 0x80008000u, 4);
  unsigned_product.angles(1, 1, 1);
  check(unsigned_product.run() == NBA97_TEXT_COMPLETE);
  const std::uint64_t product =
      std::uint64_t(UINT32_C(0xffff8000)) * UINT32_C(0xffff8000);
  check(unsigned_product.progress.machine.lo.word == std::uint32_t(product) &&
        unsigned_product.progress.machine.hi.word ==
            std::uint32_t(product >> 32u) &&
        unsigned_product.progress.machine.hi.word != 0);
}

void untouched_registers_and_return() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE &&
        f.progress.returned_value.word == Matrix &&
        f.progress.returned_value.known_mask == 15);
  constexpr std::array<unsigned, 12> written{
      {1, 2, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25}};
  for (unsigned reg = 0; reg < 32; ++reg) {
    bool changed = false;
    for (auto item : written)
      changed |= reg == item;
    if (!changed)
      check(f.progress.machine.registers.gpr[reg].word ==
                f.incoming.registers.gpr[reg].word &&
            f.progress.machine.registers.gpr[reg].known_mask ==
                f.incoming.registers.gpr[reg].known_mask);
  }
  check(f.progress.machine.registers.gpr[29].word == 0x800ff000u &&
        f.progress.machine.registers.gpr[29].known_mask == 7 &&
        f.progress.machine.registers.gpr[31].word == 0x81234568u);
}

void aliases_and_live_third_lookup() {
  Fixture table_alias;
  table_alias.incoming.registers.gpr[5] = {Table - 4u, 15};
  table_alias.angles(1, 1, 0);
  table_alias.put(Table + 4u, 0x22221111u, 4);
  table_alias.put(Table, 0x44443333u, 4);
  check(table_alias.run() == NBA97_TEXT_COMPLETE);
  check(table_alias.journal[8].pc == 0x800561ccu &&
        table_alias.journal[8].address == Table &&
        table_alias.journal[8].value == 0x44441111u);

  Fixture angle_alias;
  angle_alias.incoming.registers.gpr[5] = {Angles, 15};
  angle_alias.angles(1, 1, 0x345);
  check(angle_alias.run() == NBA97_TEXT_COMPLETE &&
        angle_alias.progress.angle_z.word == 0x345u &&
        angle_alias.get(Angles + 4, 2) != 0x345u);

  Fixture same_pointer;
  same_pointer.incoming.registers.gpr[4] = {Matrix, 15};
  same_pointer.incoming.registers.gpr[5] = {Matrix, 15};
  same_pointer.put(Matrix, 1, 2);
  same_pointer.put(Matrix + 2, 1, 2);
  same_pointer.put(Matrix + 4, 1, 2);
  check(same_pointer.run() == NBA97_TEXT_COMPLETE &&
        same_pointer.progress.angle_z.word == 1);
}

void wrapped_addresses_and_partial_products() {
  Fixture wrap;
  std::array<std::uint8_t, 16> high{};
  std::array<std::uint8_t, 16> high_known{};
  std::array<std::uint8_t, 2> low{};
  std::array<std::uint8_t, 2> low_known{};
  high_known.fill(1);
  low_known.fill(1);
  high[12] = 1;
  high[14] = 1;
  low[0] = 1;
  std::array<Nba97GameTextRegion, 3> regions{{
      wrap.region,
      {0xfffffff0u, high.data(), high_known.data(), high.size()},
      {0, low.data(), low_known.data(), low.size()},
  }};
  wrap.context.memory = {regions.data(), regions.size()};
  wrap.incoming.registers.gpr[4] = {0xfffffffcu, 15};
  wrap.incoming.registers.gpr[5] = {0xfffffff0u, 15};
  check(wrap.run() == NBA97_TEXT_COMPLETE &&
        wrap.progress.returned_value.word == 0xfffffff0u &&
        wrap.progress.angle_x.word == 1 && wrap.progress.angle_y.word == 1 &&
        wrap.progress.angle_z.word == 1 && wrap.progress.stores == 9);

  Fixture partial;
  partial.put(Table, 0x12345678u, 4, 5);
  check(partial.run() == NBA97_TEXT_COMPLETE &&
        (partial.progress.machine.lo.known_mask & 1u) == 1u &&
        partial.progress.machine.hi.known_mask == 0);

  Fixture invariant_product;
  invariant_product.angles(1, 0, 2);
  invariant_product.put(Table + 4u, 0x10000001u, 4, 14);
  invariant_product.put(Table + 8u, 0x00011000u, 4);
  check(invariant_product.run() == NBA97_TEXT_COMPLETE &&
        invariant_product.progress.machine.lo.word == 1 &&
        invariant_product.progress.machine.lo.known_mask == 14 &&
        invariant_product.progress.machine.hi.word == 0 &&
        invariant_product.progress.machine.hi.known_mask == 15);

  Fixture negated_extreme;
  negated_extreme.angles(-1, 0, 2);
  negated_extreme.put(Table + 4u, 0x10008000u, 4, 14);
  negated_extreme.put(Table + 8u, 0x00011000u, 4);
  check(negated_extreme.run() == NBA97_TEXT_COMPLETE);
  const auto completed_masks = completed_product_masks(
      negated_extreme.progress.machine.registers.gpr[10], false,
      negated_extreme.progress.machine.registers.gpr[11], true);
  const auto negated_domain = half_completions(
      negated_extreme.progress.machine.registers.gpr[11], true);
  check(negated_extreme.progress.machine.registers.gpr[11].word == 0x8000u &&
        negated_extreme.progress.machine.registers.gpr[11].known_mask == 12);
  check(negated_domain.size() == 32769u &&
        negated_domain.back() == 0x00008000u);
  check(negated_extreme.progress.machine.lo.known_mask == completed_masks.lo &&
        negated_extreme.progress.machine.hi.known_mask == completed_masks.hi &&
        completed_masks.lo == 12 && completed_masks.hi == 15);

  Fixture no_known_output;
  std::array<Nba97GameTextRegion, 3> split{{
      {Angles, no_known_output.bytes.data() + no_known_output.offset(Angles),
       no_known_output.known.data() + no_known_output.offset(Angles), 6},
      {Table, no_known_output.bytes.data() + no_known_output.offset(Table),
       no_known_output.known.data() + no_known_output.offset(Table), 0x4000},
      {Matrix, no_known_output.bytes.data() + no_known_output.offset(Matrix),
       nullptr, 18},
  }};
  no_known_output.context.memory = {split.data(), split.size()};
  no_known_output.put(Table, 0x10001000u, 4, 14);
  const auto before = no_known_output.get(Matrix + 4, 2);
  check(no_known_output.run() == NBA97_TEXT_ARGUMENT &&
        no_known_output.progress.stopped_pc == 0x80056154u &&
        no_known_output.progress.stores == 0 &&
        no_known_output.get(Matrix + 4, 2) == before);
}

void unknowns_budgets_and_mapping() {
  Fixture full;
  check(full.run() == NBA97_TEXT_COMPLETE);
  Fixture entry_prefix;
  entry_prefix.context.operation_budget = 0;
  check(entry_prefix.run() == NBA97_TEXT_LIMIT &&
        entry_prefix.progress.stopped_pc == 0x80056080u &&
        entry_prefix.progress.machine.registers.gpr[2].word ==
            entry_prefix.incoming.registers.gpr[2].word);
  for (std::size_t budget = 0; budget < full.progress.operations; ++budget) {
    Fixture prefix;
    prefix.context.operation_budget = budget;
    check(prefix.run() == NBA97_TEXT_LIMIT &&
          prefix.progress.operations == budget);
  }

  Fixture angle_unknown;
  angle_unknown.put(Angles, 1, 2, 1);
  check(angle_unknown.run() == NBA97_TEXT_UNKNOWN &&
        angle_unknown.progress.stopped_pc == 0x80056088u &&
        angle_unknown.progress.stores == 0 &&
        angle_unknown.progress.machine.registers.gpr[25].known_mask == 13);
  Fixture redundant_branch;
  redundant_branch.put(Angles, 0xff01u, 2, 2);
  check(redundant_branch.run() == NBA97_TEXT_UNKNOWN &&
        redundant_branch.progress.stopped_pc == 0x800560a8u &&
        redundant_branch.progress.instruction_count == 11 &&
        redundant_branch.progress.machine.registers.gpr[15].known_mask == 12);
  Fixture table_partial;
  table_partial.put(Table, 0x12345678u, 4, 5);
  check(table_partial.run() == NBA97_TEXT_COMPLETE &&
        table_partial.known[table_partial.offset(Matrix + 4)] == 1 &&
        table_partial.known[table_partial.offset(Matrix + 5)] == 0);
  Fixture unknown_output;
  unknown_output.incoming.registers.gpr[5].known_mask = 7;
  check(unknown_output.run() == NBA97_TEXT_UNKNOWN &&
        unknown_output.progress.stopped_pc == 0x80056154u);
  Fixture unaligned_angle;
  ++unaligned_angle.incoming.registers.gpr[4].word;
  check(unaligned_angle.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_angle.progress.stopped_pc == 0x80056080u);
  Fixture unaligned_output;
  ++unaligned_output.incoming.registers.gpr[5].word;
  check(unaligned_output.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_output.progress.stopped_pc == 0x80056154u);
  Fixture missing;
  missing.region.size = 0x100u;
  check(missing.run() == NBA97_TEXT_RESOURCE);
  Fixture zero_region;
  zero_region.region.size = 0;
  check(zero_region.run() == NBA97_TEXT_ARGUMENT);
  Fixture invalid_byte;
  invalid_byte.known[invalid_byte.offset(Angles)] = 2;
  check(invalid_byte.run() == NBA97_TEXT_ARGUMENT &&
        invalid_byte.progress.stopped_pc == 0x80056080u);
  Fixture invalid_machine;
  invalid_machine.incoming.hi.known_mask = 16;
  check(invalid_machine.run() == NBA97_TEXT_ARGUMENT);
  Fixture zero_register;
  zero_register.incoming.registers.gpr[0] = {1, 15};
  check(zero_register.run() == NBA97_TEXT_ARGUMENT);
  Fixture invalid_journal;
  invalid_journal.context.access_journal = nullptr;
  invalid_journal.context.access_journal_capacity = 1;
  check(invalid_journal.run() == NBA97_TEXT_ARGUMENT);
  Fixture overlap;
  std::array<Nba97GameTextRegion, 2> regions{{
      overlap.region,
      {Ram + 4u, overlap.bytes.data() + 4u, overlap.known.data() + 4u, 8},
  }};
  overlap.context.memory = {regions.data(), regions.size()};
  check(overlap.run() == NBA97_TEXT_ARGUMENT);
  check(nba97_game_rotation_matrix(nullptr, &overlap.progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_rotation_matrix(&overlap.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);

  Fixture unknown_ra;
  unknown_ra.incoming.registers.gpr[31].known_mask = 7;
  check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x800562c4u &&
        unknown_ra.progress.stores == 9 &&
        unknown_ra.progress.instruction_count ==
            full.progress.instruction_count);
}
} // namespace

int main() {
  all_signs_and_store_order();
  extrema_indices_and_unsigned_hilo();
  untouched_registers_and_return();
  aliases_and_live_third_lookup();
  wrapped_addresses_and_partial_products();
  unknowns_budgets_and_mapping();
  std::printf("game rotation matrix: %u checks\n", checks);
}
