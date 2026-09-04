#include "recovered/game_static_initializers.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game static initializers check %u failed\n", checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Flag = 0x800c4b14u;
constexpr std::uint32_t Stack = 0x807fff00u;
constexpr std::uint32_t EntrySp = 0x807fffd0u;
constexpr std::uint32_t FrameSp = EntrySp - 0x10u;

struct Fixture {
    std::array<std::uint8_t,4> flag{};
    std::array<std::uint8_t,4> flag_known{1,1,1,1};
    std::array<std::uint8_t,0x100> stack{};
    std::array<std::uint8_t,0x100> stack_known{};
    Nba97GameTextRegion regions[2] = {
        {Flag,flag.data(),flag_known.data(),flag.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}
    };
    Nba97GameStaticInitializersContext context{{regions,2},100,EntrySp,
        0x12345678u,{0xa0a0a0a0u,0xb1b1b1b1u}};
    Nba97GameStaticInitializersProgress progress{};
    Fixture() { stack.fill(0xcd);stack_known.fill(1); }
    void put(std::uint32_t address,std::uint32_t value) {
        for(auto& region:regions)if(address>=region.base && std::uint64_t(address-region.base)+4<=region.size) {
            const auto offset=address-region.base;
            for(unsigned i=0;i<4;++i) {region.data[offset+i]=std::uint8_t(value>>(i*8));region.known[offset+i]=1;}
            return;
        }
        check(false);
    }
    std::uint32_t get(std::uint32_t address) const {
        for(const auto& region:regions)if(address>=region.base && std::uint64_t(address-region.base)+4<=region.size) {
            const auto offset=address-region.base;std::uint32_t value=0;
            for(unsigned i=0;i<4;++i)value|=std::uint32_t(region.data[offset+i])<<(i*8);
            return value;
        }
        check(false);return 0;
    }
    int run() { return nba97_game_static_initializers(&context,&progress); }
};

void cold_and_warm() {
    Fixture cold;
    check(cold.run()==NBA97_TEXT_COMPLETE && cold.progress.completed && cold.progress.initialized &&
        !cold.progress.already_initialized);
    check(cold.progress.initialization_flag==0 && cold.get(Flag)==1);
    check(cold.progress.operations==8 && cold.progress.accesses==8 && cold.progress.reads==4 &&
        cold.progress.stores==4);
    check(cold.progress.frame_stack_pointer==FrameSp && cold.progress.stack_pointer==EntrySp);
    check(cold.get(FrameSp+4)==0xa0a0a0a0u && cold.get(FrameSp+8)==0xb1b1b1b1u &&
        cold.get(FrameSp+12)==0x12345678u);
    check(cold.progress.restored_register[0]==0xa0a0a0a0u &&
        cold.progress.restored_register[1]==0xb1b1b1b1u &&
        cold.progress.restored_return_address==0x12345678u);
    check(!cold.progress.stopped_pc && !cold.progress.stopped_address);

    Fixture warm;warm.put(Flag,5);
    check(warm.run()==NBA97_TEXT_COMPLETE && warm.progress.completed && warm.progress.already_initialized &&
        !warm.progress.initialized);
    check(warm.progress.initialization_flag==5 && warm.get(Flag)==5);
    check(warm.progress.operations==7 && warm.progress.accesses==7 && warm.progress.reads==4 &&
        warm.progress.stores==3);
    check(warm.progress.restored_return_address==0x12345678u &&
        warm.progress.restored_register[0]==0xa0a0a0a0u && warm.progress.restored_register[1]==0xb1b1b1b1u);
}

void prefixes_and_memory() {
    {Fixture f;f.flag_known[0]=0;check(f.run()==NBA97_TEXT_UNKNOWN &&
        f.progress.stopped_pc==0x800948d4u && f.progress.stopped_address==Flag &&
        f.progress.operations==1 && !f.progress.reads && !f.progress.stores);}
    {Fixture f;f.context.operation_budget=0;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x800948d4u && !f.progress.operations);}
    {Fixture f;f.context.operation_budget=1;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x800948dcu && f.progress.reads==1 && !f.progress.stores);}
    {Fixture f;f.context.operation_budget=4;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x800948f4u && f.progress.stores==3 && f.get(Flag)==0);}
    {Fixture f;f.context.operation_budget=5;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x80094928u && f.progress.initialized && f.get(Flag)==1);}
    {Fixture f;f.context.memory={f.regions+1,1};check(f.run()==NBA97_TEXT_RESOURCE &&
        f.progress.stopped_pc==0x800948d4u && f.progress.stopped_address==Flag);}
    {Fixture f;f.context.memory={f.regions,1};check(f.run()==NBA97_TEXT_RESOURCE &&
        f.progress.stopped_pc==0x800948dcu && f.progress.stopped_address==FrameSp+4 && f.progress.reads==1);}
    {Fixture f;f.flag_known[2]=2;check(f.run()==NBA97_TEXT_ARGUMENT && !f.progress.reads && !f.progress.stores);}
    {Fixture f;Nba97GameTextRegion overlap[2]={f.regions[0],f.regions[0]};f.context.memory={overlap,2};
        check(f.run()==NBA97_TEXT_ARGUMENT && !f.progress.operations);}
    Nba97GameStaticInitializersProgress progress{};
    check(nba97_game_static_initializers(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;check(nba97_game_static_initializers(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}

void native_backing_alias() {
    Fixture f;const auto offset=FrameSp+12-Stack;
    f.regions[0].data=f.stack.data()+offset;f.regions[0].known=f.stack_known.data()+offset;
    f.put(Flag,0);
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.initialized);
    check(f.get(Flag)==1 && f.get(FrameSp+12)==1 && f.progress.restored_return_address==1);
    check(f.progress.restored_register[0]==0xa0a0a0a0u && f.progress.restored_register[1]==0xb1b1b1b1u);
}
}

int main() {
    cold_and_warm();
    prefixes_and_memory();
    native_backing_alias();
    std::printf("game_static_initializers: %u checks passed\n",checks);
}
