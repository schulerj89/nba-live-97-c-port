#include "recovered/game_resource_validator_install.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,
            "game resource-validator install check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t CallbackGlobal=0x800d7b1cu;
constexpr std::uint32_t Callback=0x800a3d60u;

struct Fixture {
    std::array<std::uint8_t,12> bytes{};
    std::array<std::uint8_t,12> known{};
    Nba97GameTextRegion region{CallbackGlobal-4u,bytes.data(),known.data(),
        bytes.size()};
    Nba97GameResourceValidatorInstallContext context{{&region,1},10};
    Nba97GameResourceValidatorInstallProgress progress{};

    Fixture() { bytes.fill(0xcd);known.fill(1); }
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
        return nba97_game_resource_validator_install(&context,&progress);
    }
};

void installs_exact_callback() {
    Fixture f;f.put(0xdeadbeefu);
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.get()==Callback && f.bytes[4]==0x60 && f.bytes[5]==0x3d &&
        f.bytes[6]==0x0a && f.bytes[7]==0x80);
    check(f.progress.callback_global==CallbackGlobal &&
        f.progress.installed_callback==Callback &&
        f.progress.return_v0==Callback && f.progress.return_v0_known);
    check(f.progress.operations==1 && f.progress.accesses==1 &&
        f.progress.stores==1 && !f.progress.stopped_pc &&
        !f.progress.stopped_address);
    check(f.bytes[3]==0xcd && f.bytes[8]==0xcd && f.known[3]==1 &&
        f.known[4]==1 && f.known[7]==1 && f.known[8]==1);

    /* The source performs no read: completely unknown destination bytes are
       legal and become known only where the pointer is stored. */
    Fixture unknown;unknown.known.fill(0);
    check(unknown.run()==NBA97_TEXT_COMPLETE && unknown.get()==Callback);
    check(unknown.known[3]==0 && unknown.known[4]==1 &&
        unknown.known[5]==1 && unknown.known[6]==1 &&
        unknown.known[7]==1 && unknown.known[8]==0);

    Fixture no_known;no_known.region.known=nullptr;
    check(no_known.run()==NBA97_TEXT_COMPLETE && no_known.get()==Callback);
}

void limits_and_memory_failures() {
    {Fixture f;f.put(0x12345678u);f.context.operation_budget=0;
     check(f.run()==NBA97_TEXT_LIMIT && !f.progress.completed &&
        !f.progress.operations && !f.progress.accesses && !f.progress.stores &&
        f.progress.stopped_pc==0x800a3e2cu &&
        f.progress.stopped_address==CallbackGlobal &&
        f.progress.return_v0==Callback && f.progress.return_v0_known &&
        f.get()==0x12345678u);}
    {Fixture f;f.region.base=CallbackGlobal+4u;
     check(f.run()==NBA97_TEXT_RESOURCE && f.progress.operations==1 &&
        f.progress.accesses==1 && !f.progress.stores &&
        f.progress.stopped_pc==0x800a3e2cu &&
        f.progress.stopped_address==CallbackGlobal);}
    {Fixture f;f.region.size=7;
     check(f.run()==NBA97_TEXT_RESOURCE && !f.progress.stores);}
    {Fixture f;f.known[6]=2;
     check(f.run()==NBA97_TEXT_ARGUMENT && f.progress.operations==1 &&
        f.progress.accesses==1 && !f.progress.stores &&
        f.get()==0xcdcdcdcdu);}
    {Fixture f;Nba97GameTextRegion split[2]={
        {CallbackGlobal-2u,f.bytes.data(),f.known.data(),2},
        {CallbackGlobal,f.bytes.data()+2,f.known.data()+2,2}};
     f.context.memory={split,2};
     check(f.run()==NBA97_TEXT_RESOURCE && !f.progress.stores);}
    {Fixture f;Nba97GameTextRegion overlap[2]={f.region,f.region};
     f.context.memory={overlap,2};
     check(f.run()==NBA97_TEXT_ARGUMENT && !f.progress.operations);}
    {Fixture f;f.region.size=0;
     check(f.run()==NBA97_TEXT_ARGUMENT && !f.progress.operations);}
    {Fixture f;f.region.data=nullptr;
     check(f.run()==NBA97_TEXT_ARGUMENT && !f.progress.operations);}
    {Fixture f;f.context.memory={nullptr,1};
     check(f.run()==NBA97_TEXT_ARGUMENT && !f.progress.operations);}
    {Fixture f;f.region.base=0xfffffffcu;f.region.size=8;
     check(f.run()==NBA97_TEXT_ARGUMENT && !f.progress.operations);}
    Nba97GameResourceValidatorInstallProgress progress{};
    check(nba97_game_resource_validator_install(nullptr,&progress)==
        NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_resource_validator_install(&f.context,nullptr)==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    installs_exact_callback();
    limits_and_memory_failures();
    std::printf("game_resource_validator_install: %u checks passed\n",checks);
}
