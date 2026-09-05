#include "recovered/game_memory_zero.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value,unsigned line) {
    ++checks;
    if(!value) {
        std::fprintf(stderr,"game memory-zero check %u failed at line %u\n",
            checks,line);
        std::exit(1);
    }
}
#define check(value) checkAt((value),__LINE__)

constexpr std::uint32_t Base=0x800d6f00u;
constexpr std::uint32_t Natural=0x800d6decu;

struct Fixture {
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint8_t> known;
    Nba97GameTextRegion region{};
    Nba97GameMemoryZeroContext context{};
    Nba97GameMemoryZeroProgress progress{};

    Fixture(std::uint32_t base=Base,std::size_t size=0x400,
            std::uint32_t destination=0x800d7000u,std::uint32_t length=32)
        :bytes(size,0xa5),known(size,0),region{base,bytes.data(),known.data(),size},
         context{{&region,1},1000,destination,length,0x76543210u,1} {}

    int run() {return nba97_game_memory_zero(&context,&progress);}
    std::uint8_t at(std::uint32_t address) const {
        return bytes[static_cast<std::size_t>(address-region.base)];
    }
    std::uint8_t knownAt(std::uint32_t address) const {
        return known[static_cast<std::size_t>(address-region.base)];
    }
};

void natural_main_clear_matches_source_oracle() {
    Fixture f(Natural-8u,48,Natural,0x20u);
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.destination==Natural &&
        f.progress.requested_length==0x20u &&
        f.progress.operations==9 && f.progress.accesses==9 &&
        f.progress.stores==9 && f.progress.bytes_stored==36 &&
        !f.progress.used_small_path);
    check(f.progress.working_destination==Natural+0x1cu &&
        f.progress.working_count==0xfffffffcu &&
        f.progress.return_v0==0x76543210u &&
        f.progress.return_v0_known && !f.progress.stopped_pc &&
        !f.progress.stopped_address);
    check(f.at(Natural-1u)==0xa5 && f.at(Natural+0x20u)==0xa5);
    for(unsigned i=0;i<0x20u;++i)
        check(f.at(Natural+i)==0 && f.knownAt(Natural+i)==1);
    check(!f.knownAt(Natural-1u) && !f.knownAt(Natural+0x20u));

    Fixture unknown_return(Natural-8u,48,Natural,0x20u);
    unknown_return.context.incoming_v0=0xdeadbeefu;
    unknown_return.context.incoming_v0_known=0;
    check(unknown_return.run()==NBA97_TEXT_COMPLETE &&
        unknown_return.progress.return_v0==0xdeadbeefu &&
        !unknown_return.progress.return_v0_known);

    Fixture no_known(Natural-8u,48,Natural,0x20u);
    no_known.region.known=nullptr;
    check(no_known.run()==NBA97_TEXT_COMPLETE && no_known.progress.completed);
}

void every_alignment_and_unrolled_tier_matches_oracle() {
    struct Case {unsigned alignment,length,stores,traffic;};
    constexpr Case cases[]={
        {0,4,2,8},{0,5,2,5},{0,15,4,15},{0,16,5,20},
        {0,17,5,17},{0,31,8,31},{0,32,9,36},{0,127,32,127},
        {0,128,33,132},{0,129,33,129},{0,260,66,264},
        {1,4,2,4},{1,5,2,5},{1,15,5,19},{1,16,5,16},
        {1,17,5,17},{1,31,9,35},{1,32,9,32},{1,127,33,131},
        {1,128,33,128},{1,129,33,129},{1,260,66,260},
        {2,4,2,4},{2,5,2,5},{2,15,5,15},{2,16,5,16},
        {2,17,5,17},{2,31,9,31},{2,32,9,32},{2,127,33,127},
        {2,128,33,128},{2,129,33,129},{2,260,66,260},
        {3,4,2,4},{3,5,3,9},{3,15,5,15},{3,16,5,16},
        {3,17,6,21},{3,31,9,31},{3,32,9,32},{3,127,33,127},
        {3,128,33,128},{3,129,34,133},{3,260,66,260}
    };
    for(const auto& c:cases) {
        const std::uint32_t destination=0x800d7000u+c.alignment;
        Fixture f(Base,0x400,destination,c.length);
        check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
            !f.progress.used_small_path);
        check(f.progress.stores==c.stores &&
            f.progress.operations==c.stores &&
            f.progress.bytes_stored==c.traffic);
        check(f.at(destination-1u)==0xa5 &&
            f.at(destination+c.length)==0xa5);
        for(unsigned i=0;i<c.length;++i)
            check(f.at(destination+i)==0 && f.knownAt(destination+i)==1);
    }
}

void delay_slot_small_path_bugs_are_preserved() {
    struct Case {std::uint32_t length,writes;};
    constexpr Case cases[]={
        {0,1},{1,1},{2,2},{3,3},{0x80000001u,1},{0xffffffffu,1}
    };
    for(const auto& c:cases) {
        Fixture f(Base,0x400,0x800d7001u,c.length);
        check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
            f.progress.used_small_path && f.progress.stores==c.writes &&
            f.progress.bytes_stored==c.writes);
        for(unsigned i=0;i<c.writes;++i)
            check(f.at(f.context.destination+i)==0);
        check(f.at(f.context.destination+c.writes)==0xa5);
    }

    /* INT_MIN - 1 wraps to INT_MAX. The retail byte loop would issue
       0x80000000 writes; the native diagnostic bound retains six in order. */
    Fixture int_min(Base,0x400,0x800d7001u,0x80000000u);
    int_min.context.operation_budget=6;
    check(int_min.run()==NBA97_TEXT_LIMIT &&
        int_min.progress.used_small_path && !int_min.progress.completed &&
        int_min.progress.operations==6 && int_min.progress.accesses==6 &&
        int_min.progress.stores==6 && int_min.progress.bytes_stored==6 &&
        int_min.progress.stopped_pc==0x800a3ba0u &&
        int_min.progress.stopped_address==0x800d7007u &&
        int_min.progress.working_destination==0x800d7007u &&
        int_min.progress.working_count==0x7ffffff9u);
    for(unsigned i=0;i<6;++i)check(int_min.at(0x800d7001u+i)==0);
    check(int_min.at(0x800d7007u)==0xa5);
}

void natural_store_prefixes_are_ordered_and_not_rolled_back() {
    struct Store {std::uint32_t pc,address;unsigned width;};
    constexpr Store writes[]={
        {0x800a3a94u,Natural,4},
        {0x800a3b50u,Natural+0x04u,4},
        {0x800a3b54u,Natural+0x08u,4},
        {0x800a3b58u,Natural+0x0cu,4},
        {0x800a3b5cu,Natural+0x10u,4},
        {0x800a3b78u,Natural+0x14u,4},
        {0x800a3b78u,Natural+0x18u,4},
        {0x800a3b78u,Natural+0x1cu,4},
        {0x800a3b8cu,Natural+0x1cu,4}
    };
    for(unsigned budget=0;budget<9;++budget) {
        Fixture f(Natural-8u,48,Natural,0x20u);
        f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations==budget && f.progress.accesses==budget &&
            f.progress.stores==budget &&
            f.progress.stopped_pc==writes[budget].pc &&
            f.progress.stopped_address==writes[budget].address);
        unsigned traffic=0;
        std::array<std::uint8_t,48> expected{};expected.fill(0xa5);
        for(unsigned i=0;i<budget;++i) {
            traffic+=writes[i].width;
            const auto offset=writes[i].address-(Natural-8u);
            std::fill_n(expected.begin()+offset,writes[i].width,0);
        }
        check(f.progress.bytes_stored==traffic &&
            std::equal(expected.begin(),expected.end(),f.bytes.begin()));
    }
    Fixture exact(Natural-8u,48,Natural,0x20u);
    exact.context.operation_budget=9;
    check(exact.run()==NBA97_TEXT_COMPLETE && exact.progress.operations==9);
}

void mapping_wrap_and_validation_are_explicit() {
    /* Source address arithmetic is 32-bit. Let a head SWR cross via the next
       source address zero; the two source regions themselves remain valid. */
    std::array<std::uint8_t,4> high{};high.fill(0xa5);
    std::array<std::uint8_t,8> low{};low.fill(0xa5);
    std::array<std::uint8_t,4> high_known{};
    std::array<std::uint8_t,8> low_known{};
    Nba97GameTextRegion wrapped_regions[2]={
        {0xfffffffcu,high.data(),high_known.data(),high.size()},
        {0,low.data(),low_known.data(),low.size()}};
    Nba97GameMemoryZeroContext wrapped{{wrapped_regions,2},10,
        0xffffffffu,4,0x12345678u,1};
    Nba97GameMemoryZeroProgress wrapped_progress{};
    check(nba97_game_memory_zero(&wrapped,&wrapped_progress)==
        NBA97_TEXT_COMPLETE && wrapped_progress.completed &&
        high[3]==0 && low[0]==0 && low[1]==0 && low[2]==0 &&
        low[3]==0xa5);

    Fixture missing;missing.region.base=missing.context.destination+4u;
    check(missing.run()==NBA97_TEXT_RESOURCE &&
        missing.progress.operations==1 && missing.progress.accesses==1 &&
        !missing.progress.stores &&
        missing.progress.stopped_pc==0x800a3a94u &&
        missing.progress.stopped_address==missing.context.destination);

    Fixture malformed;malformed.known[0x100]=2;
    check(malformed.run()==NBA97_TEXT_ARGUMENT &&
        malformed.progress.operations==1 && !malformed.progress.stores);

    Fixture split;Nba97GameTextRegion fragments[2]={
        {split.context.destination,split.bytes.data()+0x100,
            split.known.data()+0x100,2},
        {split.context.destination+2u,split.bytes.data()+0x102,
            split.known.data()+0x102,2}};
    split.context.memory={fragments,2};split.context.length=4;
    check(split.run()==NBA97_TEXT_RESOURCE && !split.progress.stores);

    Fixture overlap;Nba97GameTextRegion duplicate[2]={overlap.region,overlap.region};
    overlap.context.memory={duplicate,2};
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture zero;zero.region.size=0;
    check(zero.run()==NBA97_TEXT_ARGUMENT && !zero.progress.operations);
    Fixture null_data;null_data.region.data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT && !null_data.progress.operations);
    Fixture null_regions;null_regions.context.memory={nullptr,1};
    check(null_regions.run()==NBA97_TEXT_ARGUMENT);
    Fixture bad_known;bad_known.context.incoming_v0_known=2;
    check(bad_known.run()==NBA97_TEXT_ARGUMENT && !bad_known.progress.operations);
    Fixture wraps;wraps.region.base=0xfffffffcu;wraps.region.size=8;
    check(wraps.run()==NBA97_TEXT_ARGUMENT && !wraps.progress.operations);
    Nba97GameMemoryZeroProgress progress{};
    check(nba97_game_memory_zero(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture null_out;
    check(nba97_game_memory_zero(&null_out.context,nullptr)==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    natural_main_clear_matches_source_oracle();
    every_alignment_and_unrolled_tier_matches_oracle();
    delay_slot_small_path_bugs_are_preserved();
    natural_store_prefixes_are_ordered_and_not_rolled_back();
    mapping_wrap_and_validation_are_explicit();
    std::printf("game_memory_zero: %u checks passed\n",checks);
}
