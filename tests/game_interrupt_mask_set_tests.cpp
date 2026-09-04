#include "recovered/game_interrupt_mask_set.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,"game interrupt-mask set check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t MaskAddress=0x800c54acu;

struct Fixture {
    std::array<std::uint8_t,12> bytes{};
    std::array<std::uint8_t,12> known{};
    Nba97GameTextRegion region{MaskAddress-4u,bytes.data(),known.data(),bytes.size()};
    Nba97GameInterruptMaskSetContext context{{&region,1},10,0};
    Nba97GameInterruptMaskSetProgress progress{};

    Fixture() { bytes.fill(0xcd);known.fill(1);put(0x7ffu); }
    void put(std::uint32_t value) {
        for(unsigned i=0;i<4;++i)bytes[4+i]=std::uint8_t(value>>(i*8));
    }
    std::uint32_t get() const {
        std::uint32_t value=0;
        for(unsigned i=0;i<4;++i)value|=std::uint32_t(bytes[4+i])<<(i*8);
        return value;
    }
    int run() {return nba97_game_interrupt_mask_set(&context,&progress);}
};

void clear_for_callback_reset() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.get()==0 && f.progress.requested_mask==0 &&
        f.progress.previous_mask==0x7ffu && f.progress.published_mask==0 &&
        f.progress.return_v0==0x7ffu);
    check(f.progress.operations==2 && f.progress.accesses==2 &&
        f.progress.reads==1 && f.progress.stores==1);
    check(!f.progress.stopped_pc && !f.progress.stopped_address);
    check(f.bytes[3]==0xcd && f.bytes[8]==0xcd &&
        f.known[3]==1 && f.known[4]==1 && f.known[7]==1 && f.known[8]==1);

    Fixture replacement;replacement.put(0x10203040u);
    replacement.context.interrupt_mask=0xa5a55a5au;
    check(replacement.run()==NBA97_TEXT_COMPLETE &&
        replacement.get()==0xa5a55a5au &&
        replacement.progress.previous_mask==0x10203040u &&
        replacement.progress.return_v0==0x10203040u);

    Fixture no_known;no_known.region.known=nullptr;
    check(no_known.run()==NBA97_TEXT_COMPLETE && no_known.get()==0 &&
        no_known.progress.previous_mask==0x7ffu);
}

void limits_and_memory_failures() {
    {Fixture f;f.context.operation_budget=0;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x800985b8u &&
        f.progress.stopped_address==MaskAddress && !f.progress.operations &&
        !f.progress.accesses && !f.progress.reads && !f.progress.stores &&
        f.get()==0x7ffu);}
    {Fixture f;f.context.operation_budget=1;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x800985c0u &&
        f.progress.stopped_address==MaskAddress && f.progress.operations==1 &&
        f.progress.accesses==1 && f.progress.reads==1 && !f.progress.stores &&
        f.progress.previous_mask==0x7ffu && f.get()==0x7ffu);}
    {Fixture f;f.region.base=MaskAddress+4u;check(f.run()==NBA97_TEXT_RESOURCE &&
        f.progress.operations==1 && f.progress.accesses==1 &&
        f.progress.stopped_pc==0x800985b8u && !f.progress.reads &&
        !f.progress.stores);}
    {Fixture f;f.known[4]=0;check(f.run()==NBA97_TEXT_UNKNOWN &&
        f.progress.operations==1 && f.progress.accesses==1 &&
        !f.progress.reads && !f.progress.stores && f.get()==0x7ffu);}
    {Fixture f;f.known[6]=2;check(f.run()==NBA97_TEXT_ARGUMENT &&
        f.progress.operations==1 && f.progress.accesses==1 &&
        !f.progress.reads && !f.progress.stores);}
    {Fixture f;Nba97GameTextRegion overlap[2]={f.region,f.region};
        f.context.memory={overlap,2};check(f.run()==NBA97_TEXT_ARGUMENT &&
        !f.progress.operations);}
    {Fixture f;f.region.size=0;check(f.run()==NBA97_TEXT_ARGUMENT);}
    {Fixture f;f.region.data=nullptr;check(f.run()==NBA97_TEXT_ARGUMENT);}
    {Fixture f;f.context.memory={nullptr,1};check(f.run()==NBA97_TEXT_ARGUMENT);}
    Nba97GameInterruptMaskSetProgress progress{};
    check(nba97_game_interrupt_mask_set(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;check(nba97_game_interrupt_mask_set(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    clear_for_callback_reset();
    limits_and_memory_failures();
    std::printf("game_interrupt_mask_set: %u checks passed\n",checks);
}
