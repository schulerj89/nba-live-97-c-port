#include "recovered/game_cd_ready_callback.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
unsigned checks;
void checkAt(bool value,unsigned line) {
    ++checks;
    if(!value) {
        std::fprintf(stderr,
            "game CD-ready-callback check %u failed at line %u\n",checks,line);
        std::exit(1);
    }
}
#define check(value) checkAt((value),__LINE__)

constexpr std::uint32_t Slot=0x800c57e4u;
constexpr std::uint32_t SourceDefault=0x8009d9dcu;

struct Fixture {
    std::array<std::uint8_t,12> bytes{},known{};
    Nba97GameTextRegion region{Slot-4u,bytes.data(),known.data(),bytes.size()};
    Nba97GameCdReadyCallbackContext context{{&region,1},10,0};
    Nba97GameCdReadyCallbackProgress progress{};

    Fixture() {bytes.fill(0xcd);known.fill(1);put(SourceDefault);}
    void put(std::uint32_t value) {
        for(unsigned i=0;i<4;++i)
            bytes[4u+i]=static_cast<std::uint8_t>(value>>(i*8u));
    }
    std::uint32_t get() const {
        std::uint32_t value=0;
        for(unsigned i=0;i<4;++i)
            value|=std::uint32_t(bytes[4u+i])<<(i*8u);
        return value;
    }
    int run() {
        return nba97_game_cd_ready_callback(&context,&progress);
    }
};

void exchanges_and_returns_previous() {
    Fixture clear;
    check(clear.run()==NBA97_TEXT_COMPLETE && clear.progress.completed);
    check(clear.get()==0 && clear.progress.callback_global==Slot &&
        clear.progress.requested_callback==0 &&
        clear.progress.previous_callback==SourceDefault &&
        clear.progress.previous_callback_known &&
        clear.progress.return_v0==SourceDefault &&
        clear.progress.return_v0_known);
    check(clear.progress.operations==2 && clear.progress.accesses==2 &&
        clear.progress.reads==1 && clear.progress.stores==1 &&
        !clear.progress.stopped_pc && !clear.progress.stopped_address);
    check(clear.bytes[3]==0xcd && clear.bytes[8]==0xcd &&
        clear.known[3]==1 && clear.known[4]==1 &&
        clear.known[7]==1 && clear.known[8]==1);

    /* The source validates neither alignment nor ownership of the callback
       value itself; it is data here, not a native pointer dereference. */
    Fixture raw;raw.put(0x10203040u);
    raw.context.replacement_callback=0xdeadbeefu;
    check(raw.run()==NBA97_TEXT_COMPLETE && raw.get()==0xdeadbeefu &&
        raw.progress.previous_callback==0x10203040u &&
        raw.progress.return_v0==0x10203040u);

    Fixture null_to_null;null_to_null.put(0);
    check(null_to_null.run()==NBA97_TEXT_COMPLETE &&
        null_to_null.progress.previous_callback==0 && null_to_null.get()==0);
}

void unknown_old_value_still_stores() {
    Fixture unknown;unknown.put(0x76543210u);unknown.known.fill(0);
    unknown.context.replacement_callback=0x89abcdefu;
    check(unknown.run()==NBA97_TEXT_COMPLETE && unknown.progress.completed &&
        unknown.progress.previous_callback==0x76543210u &&
        !unknown.progress.previous_callback_known &&
        unknown.progress.return_v0==0x76543210u &&
        !unknown.progress.return_v0_known &&
        unknown.get()==0x89abcdefu);
    check(unknown.known[3]==0 && unknown.known[4]==1 &&
        unknown.known[5]==1 && unknown.known[6]==1 &&
        unknown.known[7]==1 && unknown.known[8]==0);

    Fixture no_mask;no_mask.region.known=nullptr;no_mask.put(0xa1b2c3d4u);
    check(no_mask.run()==NBA97_TEXT_COMPLETE &&
        no_mask.progress.previous_callback_known &&
        no_mask.progress.return_v0==0xa1b2c3d4u);
}

void prefixes_and_validation() {
    Fixture before_read;before_read.context.operation_budget=0;
    check(before_read.run()==NBA97_TEXT_LIMIT && !before_read.progress.completed &&
        !before_read.progress.operations && !before_read.progress.accesses &&
        !before_read.progress.reads && !before_read.progress.stores &&
        before_read.progress.stopped_pc==0x8009dbe4u &&
        before_read.progress.stopped_address==Slot &&
        before_read.progress.return_v0==0x800c0000u &&
        before_read.progress.return_v0_known && before_read.get()==SourceDefault);

    Fixture after_read;after_read.context.operation_budget=1;
    check(after_read.run()==NBA97_TEXT_LIMIT &&
        after_read.progress.operations==1 && after_read.progress.accesses==1 &&
        after_read.progress.reads==1 && !after_read.progress.stores &&
        after_read.progress.stopped_pc==0x8009dbecu &&
        after_read.progress.stopped_address==Slot &&
        after_read.progress.previous_callback==SourceDefault &&
        after_read.progress.return_v0==SourceDefault && after_read.get()==SourceDefault);

    Fixture exact;exact.context.operation_budget=2;
    check(exact.run()==NBA97_TEXT_COMPLETE && exact.progress.operations==2);

    Fixture missing;missing.region.base=Slot+4u;
    check(missing.run()==NBA97_TEXT_RESOURCE &&
        missing.progress.operations==1 && missing.progress.accesses==1 &&
        !missing.progress.reads && !missing.progress.stores &&
        missing.progress.stopped_pc==0x8009dbe4u &&
        missing.progress.stopped_address==Slot &&
        missing.progress.return_v0==0x800c0000u);

    Fixture short_region;short_region.region.size=7;
    check(short_region.run()==NBA97_TEXT_RESOURCE &&
        !short_region.progress.reads && !short_region.progress.stores);

    Fixture malformed;malformed.known[4]=2;
    check(malformed.run()==NBA97_TEXT_ARGUMENT &&
        malformed.progress.operations==1 && malformed.progress.accesses==1 &&
        !malformed.progress.reads && !malformed.progress.stores &&
        malformed.get()==SourceDefault);

    Fixture split;Nba97GameTextRegion fragments[2]={
        {Slot-2u,split.bytes.data(),split.known.data(),2},
        {Slot,split.bytes.data()+2,split.known.data()+2,2}};
    split.context.memory={fragments,2};
    check(split.run()==NBA97_TEXT_RESOURCE && !split.progress.reads &&
        !split.progress.stores);

    Fixture overlap;Nba97GameTextRegion duplicate[2]={overlap.region,overlap.region};
    overlap.context.memory={duplicate,2};
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture zero;zero.region.size=0;
    check(zero.run()==NBA97_TEXT_ARGUMENT && !zero.progress.operations);
    Fixture null_data;null_data.region.data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT && !null_data.progress.operations);
    Fixture null_regions;null_regions.context.memory={nullptr,1};
    check(null_regions.run()==NBA97_TEXT_ARGUMENT);
    Fixture wraps;wraps.region.base=0xfffffffcu;wraps.region.size=8;
    check(wraps.run()==NBA97_TEXT_ARGUMENT && !wraps.progress.operations);
    Nba97GameCdReadyCallbackProgress progress{};
    check(nba97_game_cd_ready_callback(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture null_out;
    check(nba97_game_cd_ready_callback(&null_out.context,nullptr)==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    exchanges_and_returns_previous();
    unknown_old_value_still_stores();
    prefixes_and_validation();
    std::printf("game_cd_ready_callback: %u checks passed\n",checks);
}
