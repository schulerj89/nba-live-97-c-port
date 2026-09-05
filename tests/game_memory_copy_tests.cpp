#include "recovered/game_memory_copy.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value,unsigned line) {
    ++checks;
    if(!value) {
        std::fprintf(stderr,"game memory-copy check %u failed at line %u\n",
            checks,line);
        std::exit(1);
    }
}
#define check(value) checkAt((value),__LINE__)

constexpr std::uint32_t Base=0x80100000u;

struct Fixture {
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint8_t> known;
    Nba97GameTextRegion region{};
    Nba97GameMemoryCopyContext context{};
    Nba97GameMemoryCopyProgress progress{};

    Fixture(std::uint32_t source=0x80120000u,
            std::uint32_t destination=0x80130000u,
            std::uint32_t length=64u,std::size_t budget=100000)
        :bytes(0x100000),known(0x100000,1),
         region{Base,bytes.data(),known.data(),bytes.size()},
         context{{&region,1},budget,source,destination,length} {
        for(std::size_t i=0;i<bytes.size();++i)
            bytes[i]=static_cast<std::uint8_t>((i*37u+(i>>8u)+0x5au)&0xffu);
    }
    int run() {return nba97_game_memory_copy(&context,&progress);}
    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address-region.base);
    }
    std::uint8_t at(std::uint32_t address) const {
        return bytes[offset(address)];
    }
};

void natural_feload_transfer_matches_source_oracle() {
    constexpr std::uint32_t source=0x80123400u;
    constexpr std::uint32_t destination=0x801e0000u;
    constexpr std::uint32_t length=0x1410u;
    Fixture f(source,destination,length);
    f.bytes[f.offset(source)+0]=0x00;
    f.bytes[f.offset(source)+1]=0x01;
    f.bytes[f.offset(source)+2]=0x1e;
    f.bytes[f.offset(source)+3]=0x80;
    const std::vector<std::uint8_t> expected(
        f.bytes.begin()+f.offset(source),f.bytes.begin()+f.offset(source+length));
    std::fill_n(f.bytes.begin()+f.offset(destination),length,0xa5);
    f.bytes[f.offset(destination-1u)]=0x7b;
    f.bytes[f.offset(destination+length)]=0x6c;

    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(!f.progress.backward && !f.progress.unaligned &&
        f.progress.return_v0_known && f.progress.return_v0==0);
    check(f.progress.operations==2568 && f.progress.accesses==2568 &&
        f.progress.reads==1284 && f.progress.stores==1284 &&
        f.progress.bytes_read==5136 && f.progress.bytes_stored==5136);
    check(f.progress.source==source && f.progress.destination==destination &&
        f.progress.requested_length==length &&
        f.progress.working_source==source+length &&
        f.progress.working_destination==destination+length &&
        f.progress.working_count==0xffffffffu &&
        !f.progress.stopped_pc && !f.progress.stopped_address);
    check(std::equal(expected.begin(),expected.end(),
        f.bytes.begin()+f.offset(destination)));
    check(f.at(destination-1u)==0x7b && f.at(destination+length)==0x6c);
}

void direction_alignment_and_group_schedules_match_oracle() {
    struct Case {
        std::uint32_t source,destination,length,result;
        std::size_t accesses,bytes_read,bytes_stored;
        bool backward,unaligned;
    };
    constexpr Case cases[]={
        {0x80120000u,0x80130000u,65,0,34,65,65,false,false},
        {0x80120001u,0x80130002u,17,3,18,17,17,false,true},
        {0x80120000u,0x80120004u,40,0,24,48,48,true,false},
        {0x80120001u,0x80120006u,19,1,22,35,19,true,true},
        {0x80120008u,0x80120000u,40,0,20,40,40,false,false}
    };
    for(const auto& c:cases) {
        Fixture f(c.source,c.destination,c.length);
        std::vector<std::uint8_t> expected=f.bytes;
        std::memmove(expected.data()+f.offset(c.destination),
            expected.data()+f.offset(c.source),c.length);
        check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
            bool(f.progress.backward)==c.backward &&
            bool(f.progress.unaligned)==c.unaligned &&
            f.progress.return_v0==c.result);
        check(f.progress.operations==c.accesses &&
            f.progress.reads+f.progress.stores==c.accesses &&
            f.progress.bytes_read==c.bytes_read &&
            f.progress.bytes_stored==c.bytes_stored);
        check(f.bytes==expected);
    }
}

void source_snapshot_groups_and_bounded_prefixes_are_exact() {
    Fixture before_writes(0x80120000u,0x80130000u,64,8);
    const auto initial=before_writes.bytes;
    check(before_writes.run()==NBA97_TEXT_LIMIT &&
        before_writes.progress.operations==8 &&
        before_writes.progress.reads==8 && !before_writes.progress.stores &&
        before_writes.progress.stopped_pc==0x800aa4acu &&
        before_writes.progress.stopped_address==0x80130000u &&
        before_writes.bytes==initial);

    Fixture one_half(0x80120000u,0x80130000u,64,16);
    const auto half_initial=one_half.bytes;
    check(one_half.run()==NBA97_TEXT_LIMIT &&
        one_half.progress.reads==8 && one_half.progress.stores==8 &&
        one_half.progress.bytes_read==32 &&
        one_half.progress.bytes_stored==32 &&
        one_half.progress.stopped_pc==0x800aa4ccu &&
        one_half.progress.stopped_address==0x80120020u);
    check(std::equal(half_initial.begin()+one_half.offset(0x80120000u),
        half_initial.begin()+one_half.offset(0x80120020u),
        one_half.bytes.begin()+one_half.offset(0x80130000u)));
    check(std::equal(half_initial.begin()+one_half.offset(0x80130020u),
        half_initial.begin()+one_half.offset(0x80130040u),
        one_half.bytes.begin()+one_half.offset(0x80130020u)));

    /* INT_MIN is signed-negative initially, but subtracting 64 wraps it to a
       huge positive loop count. Preserve a bounded prefix of that retail bug. */
    Fixture runaway(0x80120000u,0x80110000u,0x80000000u,10);
    check(runaway.run()==NBA97_TEXT_LIMIT && !runaway.progress.completed &&
        runaway.progress.operations==10 && runaway.progress.reads==8 &&
        runaway.progress.stores==2 && runaway.progress.bytes_read==32 &&
        runaway.progress.bytes_stored==8 &&
        runaway.progress.working_count==0x7fffffc0u &&
        runaway.progress.stopped_pc==0x800aa4b4u &&
        runaway.progress.stopped_address==0x80110008u);
}

void knowledge_and_partial_word_traffic_are_preserved() {
    Fixture f(0x80120001u,0x80130002u,17);
    for(unsigned i=0;i<17;++i)
        f.known[f.offset(f.context.source+i)]=(i%3u)!=1u;
    const std::vector<std::uint8_t> source_data(
        f.bytes.begin()+f.offset(f.context.source),
        f.bytes.begin()+f.offset(f.context.source+17u));
    const std::vector<std::uint8_t> copied_known(
        f.known.begin()+f.offset(f.context.source),
        f.known.begin()+f.offset(f.context.source+17u));
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.unaligned &&
        f.progress.bytes_read==17 && f.progress.bytes_stored==17);
    check(std::equal(source_data.begin(),source_data.end(),
        f.bytes.begin()+f.offset(f.context.destination)));
    check(std::equal(copied_known.begin(),copied_known.end(),
        f.known.begin()+f.offset(f.context.destination)));

    Fixture cannot_represent(0x80120000u,0x80130000u,4);
    cannot_represent.known[cannot_represent.offset(0x80120001u)]=0;
    cannot_represent.region.known=nullptr;
    check(cannot_represent.run()==NBA97_TEXT_COMPLETE);
    /* known=NULL marks the source known too, so use two disjoint regions to
       exercise an unknown source and an all-known destination contract. */
    std::array<std::uint8_t,4> source{{1,2,3,4}};
    std::array<std::uint8_t,4> source_known{{1,0,1,1}};
    std::array<std::uint8_t,4> destination{{9,9,9,9}};
    Nba97GameTextRegion regions[2]={{0x1000u,source.data(),
        source_known.data(),4},{0x2000u,destination.data(),nullptr,4}};
    Nba97GameMemoryCopyContext context{{regions,2},10,0x1000u,0x2000u,4};
    Nba97GameMemoryCopyProgress progress{};
    const std::array<std::uint8_t,4> untouched{{9,9,9,9}};
    check(nba97_game_memory_copy(&context,&progress)==NBA97_TEXT_UNKNOWN &&
        progress.operations==2 && progress.reads==1 && !progress.stores &&
        progress.stopped_pc==0x800aa56cu &&
        progress.stopped_address==0x2000u &&
        destination==untouched);
}

void zero_length_and_signed_add_traps_remain_source_behavior() {
    Fixture zero(0x80120000u,0x80130000u,0);
    check(zero.run()==NBA97_TEXT_COMPLETE && zero.progress.completed &&
        !zero.progress.operations && zero.progress.working_source==0x80120000u &&
        zero.progress.working_destination==0x80130000u &&
        zero.progress.working_count==0xffffffffu &&
        zero.progress.return_v0==0);

    Fixture source_trap(0x7ffffff0u,0x7ffffff8u,32);
    check(source_trap.run()==NBA97_GAME_MEMORY_COPY_ARITHMETIC_TRAP &&
        !source_trap.progress.completed && !source_trap.progress.operations &&
        source_trap.progress.stopped_pc==0x800aa65cu &&
        source_trap.progress.return_v0==0x7ffffff8u);

    Fixture destination_trap(0x7ffffe00u,0x7fffff00u,0x180u);
    check(destination_trap.run()==NBA97_GAME_MEMORY_COPY_ARITHMETIC_TRAP &&
        destination_trap.progress.backward &&
        destination_trap.progress.working_source==0x7fffff80u &&
        destination_trap.progress.working_destination==0x7fffff00u &&
        destination_trap.progress.working_count==0x180u &&
        destination_trap.progress.stopped_pc==0x800aa670u &&
        destination_trap.progress.return_v0==0x7fffff00u);
}

void mapping_and_metadata_failures_keep_prefixes() {
    Fixture missing(0x80120000u,0x80130000u,4);
    missing.region.base=0x80130000u;
    missing.region.size=4;
    check(missing.run()==NBA97_TEXT_RESOURCE &&
        missing.progress.operations==1 && !missing.progress.reads &&
        missing.progress.stopped_pc==0x800aa564u &&
        missing.progress.stopped_address==0x80120000u);

    Fixture malformed(0x80120000u,0x80130000u,4);
    malformed.known[malformed.offset(0x80120000u)]=2;
    check(malformed.run()==NBA97_TEXT_ARGUMENT &&
        malformed.progress.operations==1 && !malformed.progress.reads);

    Fixture overlap;
    Nba97GameTextRegion duplicate[2]={overlap.region,overlap.region};
    overlap.context.memory={duplicate,2};
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture empty;empty.region.size=0;
    check(empty.run()==NBA97_TEXT_ARGUMENT && !empty.progress.operations);
    Fixture null_data;null_data.region.data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT && !null_data.progress.operations);
    Fixture null_regions;null_regions.context.memory={nullptr,1};
    check(null_regions.run()==NBA97_TEXT_ARGUMENT);
    Fixture wraps;wraps.region.base=0xfffffffcu;wraps.region.size=8;
    check(wraps.run()==NBA97_TEXT_ARGUMENT && !wraps.progress.operations);
    Nba97GameMemoryCopyProgress progress{};
    check(nba97_game_memory_copy(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture null_out;
    check(nba97_game_memory_copy(&null_out.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    natural_feload_transfer_matches_source_oracle();
    direction_alignment_and_group_schedules_match_oracle();
    source_snapshot_groups_and_bounded_prefixes_are_exact();
    knowledge_and_partial_word_traffic_are_preserved();
    zero_length_and_signed_add_traps_remain_source_behavior();
    mapping_and_metadata_failures_keep_prefixes();
    std::printf("game_memory_copy: %u checks passed\n",checks);
}
