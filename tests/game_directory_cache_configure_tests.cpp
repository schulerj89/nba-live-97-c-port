#include "recovered/game_directory_cache_configure.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,"game directory-cache configure check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram=0x80000000u;
constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807fffd0u;
constexpr std::uint32_t FrameSp=EntrySp-8u;
constexpr std::uint32_t Cache=0x8001000cu;
constexpr std::uint32_t Capacity=0x2c3u;
constexpr std::uint32_t CapacityAddress=0x800c4ab8u;
constexpr std::uint32_t PointerAddress=0x801046a0u;
constexpr std::uint32_t FramePointer=0x13579bdfu;

struct Fixture {
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x200000,0xcd);
    std::vector<std::uint8_t> ram_known=std::vector<std::uint8_t>(0x200000,1);
    std::array<std::uint8_t,0x100> stack{};
    std::array<std::uint8_t,0x100> stack_known{};
    Nba97GameTextRegion regions[2]={{Ram,ram.data(),ram_known.data(),ram.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}};
    Nba97GameDirectoryCacheConfigureContext context{{regions,2},100,Cache,Capacity,
        EntrySp,FramePointer};
    Nba97GameDirectoryCacheConfigureProgress progress{};

    Fixture() { stack.fill(0xcd);stack_known.fill(1); }
    std::uint8_t* byte(std::uint32_t address) {
        for(auto& region:regions)
            if(address>=region.base && std::uint64_t(address-region.base)<region.size)
                return region.data+(address-region.base);
        check(false);return nullptr;
    }
    std::uint8_t* known(std::uint32_t address) {
        for(auto& region:regions)
            if(address>=region.base && std::uint64_t(address-region.base)<region.size)
                return region.known ? region.known+(address-region.base):nullptr;
        check(false);return nullptr;
    }
    std::uint32_t get(std::uint32_t address) {
        std::uint32_t result=0;
        for(unsigned i=0;i<4;++i)result|=std::uint32_t(*byte(address+i))<<(i*8);
        return result;
    }
    int run() {return nba97_game_directory_cache_configure(&context,&progress);}
};

void startup_configuration() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.get(CapacityAddress)==Capacity && f.get(PointerAddress)==Cache);
    check(f.get(EntrySp)==Cache && f.get(EntrySp+4u)==Capacity &&
        f.get(FrameSp)==FramePointer);
    check(f.progress.operations==8 && f.progress.accesses==8 &&
        f.progress.reads==3 && f.progress.stores==5);
    check(f.progress.cache_address==Cache && f.progress.entry_capacity==Capacity &&
        f.progress.published_cache_address==Cache &&
        f.progress.published_entry_capacity==Capacity);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp &&
        f.progress.restored_frame_pointer==FramePointer &&
        f.progress.return_v0==Cache);
    check(!f.progress.stopped_pc && !f.progress.stopped_address);
    check(*f.known(FrameSp)==1 && *f.known(EntrySp)==1 &&
        *f.known(CapacityAddress)==1 && *f.known(PointerAddress)==1);

    Fixture custom;custom.context.cache_address=0x81234560u;
    custom.context.entry_capacity=0;
    check(custom.run()==NBA97_TEXT_COMPLETE &&
        custom.get(CapacityAddress)==0 && custom.get(PointerAddress)==0x81234560u &&
        custom.progress.return_v0==0x81234560u);

    Fixture no_known;no_known.regions[0].known=nullptr;no_known.regions[1].known=nullptr;
    check(no_known.run()==NBA97_TEXT_COMPLETE &&
        no_known.get(CapacityAddress)==Capacity && no_known.get(PointerAddress)==Cache);
}

void source_order_and_partial_effects() {
    Fixture alias;
    alias.context.stack_pointer=CapacityAddress+8u;
    check(alias.run()==NBA97_TEXT_COMPLETE && alias.progress.completed);
    check(alias.progress.frame_stack_pointer==CapacityAddress &&
        alias.progress.stack_pointer==CapacityAddress+8u);
    check(alias.get(CapacityAddress)==Capacity &&
        alias.progress.restored_frame_pointer==Capacity);
    check(alias.get(CapacityAddress+8u)==Cache &&
        alias.get(CapacityAddress+12u)==Capacity &&
        alias.get(PointerAddress)==Cache);

    Fixture limited;limited.context.operation_budget=4;
    check(limited.run()==NBA97_TEXT_LIMIT && !limited.progress.completed &&
        limited.progress.stopped_pc==0x80092c98u &&
        limited.progress.stopped_address==CapacityAddress);
    check(limited.progress.operations==4 && limited.progress.accesses==4 &&
        limited.progress.reads==1 && limited.progress.stores==3 &&
        limited.get(CapacityAddress)==0xcdcdcdcdu &&
        limited.get(EntrySp)==Cache && limited.get(EntrySp+4u)==Capacity);
}

void limits_and_memory_failures() {
    {Fixture f;f.context.operation_budget=0;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x80092c80u && f.progress.stopped_address==FrameSp &&
        !f.progress.operations && !f.progress.accesses && !f.progress.stores);}
    {Fixture f;f.context.operation_budget=1;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x80092c88u && f.progress.stopped_address==EntrySp &&
        f.progress.operations==1 && f.progress.stores==1);}
    {Fixture f;f.context.stack_pointer=EntrySp+1u;check(
        f.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        f.progress.stopped_pc==0x80092c80u && f.progress.accesses==1);}
    {Fixture f;f.regions[0].size=0x100;check(f.run()==NBA97_TEXT_RESOURCE &&
        f.progress.stopped_pc==0x80092c98u &&
        f.progress.stopped_address==CapacityAddress && f.progress.stores==3);}
    {Fixture f;*f.known(FrameSp)=2;check(f.run()==NBA97_TEXT_ARGUMENT &&
        f.progress.stopped_pc==0x80092c80u && !f.progress.stores);}
    {Fixture f;Nba97GameTextRegion overlap[2]={f.regions[0],f.regions[0]};
        f.context.memory={overlap,2};check(f.run()==NBA97_TEXT_ARGUMENT &&
        !f.progress.operations);}
    {Fixture f;f.regions[0].size=0;check(f.run()==NBA97_TEXT_ARGUMENT);}
    {Fixture f;f.regions[0].data=nullptr;check(f.run()==NBA97_TEXT_ARGUMENT);}
    {Fixture f;f.context.memory={nullptr,1};check(f.run()==NBA97_TEXT_ARGUMENT);}
    Nba97GameDirectoryCacheConfigureProgress progress{};
    check(nba97_game_directory_cache_configure(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;check(nba97_game_directory_cache_configure(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    startup_configuration();
    source_order_and_partial_effects();
    limits_and_memory_failures();
    std::printf("game_directory_cache_configure: %u checks passed\n",checks);
}
