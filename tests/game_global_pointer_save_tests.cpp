#include "recovered/game_global_pointer_save.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game global-pointer save check %u failed\n", checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Destination = 0x800d6e2cu;

struct Fixture {
    std::array<std::uint8_t,12> bytes{};
    std::array<std::uint8_t,12> known{};
    Nba97GameTextRegion region{Destination - 4u,bytes.data(),known.data(),bytes.size()};
    Nba97GameGlobalPointerSaveContext context{{&region,1},10,0x800d79c8u};
    Nba97GameGlobalPointerSaveProgress progress{};
    Fixture() { bytes.fill(0xcd); }
    std::uint32_t get() const {
        std::uint32_t value=0;
        for(unsigned i=0;i<4;++i)value|=std::uint32_t(bytes[4+i])<<(i*8);
        return value;
    }
    int run() { return nba97_game_global_pointer_save(&context,&progress); }
};

void successful_store() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.get()==0x800d79c8u && f.progress.stored_global_pointer==0x800d79c8u);
    check(f.progress.operations==1 && f.progress.accesses==1 && f.progress.stores==1);
    check(!f.progress.stopped_pc && !f.progress.stopped_address);
    check(f.bytes[3]==0xcd && f.bytes[8]==0xcd);
    check(f.known[3]==0 && f.known[4]==1 && f.known[5]==1 &&
        f.known[6]==1 && f.known[7]==1 && f.known[8]==0);

    Fixture zero;zero.context.global_pointer=0;
    check(zero.run()==NBA97_TEXT_COMPLETE && zero.get()==0 &&
        zero.progress.stored_global_pointer==0);
    Fixture no_known;no_known.region.known=nullptr;
    check(no_known.run()==NBA97_TEXT_COMPLETE && no_known.get()==0x800d79c8u);
}

void limits_and_memory() {
    {Fixture f;f.context.operation_budget=0;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x800a4838u && f.progress.stopped_address==Destination &&
        !f.progress.operations && !f.progress.accesses && !f.progress.stores &&
        !f.progress.stored_global_pointer && f.get()==0xcdcdcdcdu);}
    {Fixture f;f.region.base=Destination+4u;check(f.run()==NBA97_TEXT_RESOURCE &&
        f.progress.operations==1 && f.progress.accesses==1 && !f.progress.stores &&
        f.progress.stopped_pc==0x800a4838u && f.progress.stopped_address==Destination);}
    {Fixture f;f.known[6]=2;check(f.run()==NBA97_TEXT_ARGUMENT &&
        f.progress.operations==1 && f.progress.accesses==1 && !f.progress.stores &&
        f.get()==0xcdcdcdcdu);}
    {Fixture f;Nba97GameTextRegion overlap[2]={f.region,f.region};f.context.memory={overlap,2};
        check(f.run()==NBA97_TEXT_ARGUMENT && !f.progress.operations);}
    {Fixture f;f.region.size=0;check(f.run()==NBA97_TEXT_ARGUMENT && !f.progress.operations);}
    {Fixture f;f.region.data=nullptr;check(f.run()==NBA97_TEXT_ARGUMENT && !f.progress.operations);}
    {Fixture f;f.context.memory={nullptr,1};check(f.run()==NBA97_TEXT_ARGUMENT && !f.progress.operations);}
    Nba97GameGlobalPointerSaveProgress progress{};
    check(nba97_game_global_pointer_save(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;check(nba97_game_global_pointer_save(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    successful_store();
    limits_and_memory();
    std::printf("game_global_pointer_save: %u checks passed\n",checks);
}
