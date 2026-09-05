#include "recovered/game_match_initialize.h"
#include "recovered/game_roster_bindings.h"
#include "game_match_initialize_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if(!value) {
        std::fprintf(stderr,"game roster-bindings integration check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Base=0x80015000u,End=0x80024800u;
struct Fixture {
    std::vector<std::uint8_t> globals=std::vector<std::uint8_t>(End-Base,0);
    std::vector<std::uint8_t> global_known=std::vector<std::uint8_t>(End-Base,1);
    std::vector<std::uint8_t> match=std::vector<std::uint8_t>(0xe7c,0xa5);
    std::vector<std::uint8_t> match_known=std::vector<std::uint8_t>(0xe7c,1);
    std::array<std::uint8_t,0x100> stack{};
    std::array<std::uint8_t,0x100> stack_known{};
    Nba97GameTextRegion regions[3]{{Base,globals.data(),global_known.data(),globals.size()},
        {0x800fdb4cu,match.data(),match_known.data(),match.size()},
        {0x80100000u,stack.data(),stack_known.data(),stack.size()}};
    std::array<Nba97GameMatchInitializeAccess,32> parent_journal{};
    std::array<Nba97GameRosterBindingsAccess,159> roster_journal{};
    Nba97GameMatchInitializeContext parent{};
    Nba97GameMatchInitializeProgress parent_progress{};
    Nba97GameRosterBindingsProgress roster_progress{};
    std::vector<std::uint32_t> calls;
    int roster_status=NBA97_TEXT_ARGUMENT;

    Fixture() {
        stack_known.fill(1);
        parent.memory={regions,3};parent.operation_budget=1000;
        parent.registers.gpr[0]={0,0x0f};
        for(unsigned i=1;i<NBA97_MATCH_INITIALIZE_REGISTER_COUNT;++i)
            parent.registers.gpr[i]={0x42000000u+i,0x0f};
        parent.registers.gpr[NBA97_MATCH_INITIALIZE_SP]={0x80100080u,0x0f};
        parent.registers.gpr[NBA97_MATCH_INITIALIZE_RA]={0x81234567u,0x0f};
        parent.io=dispatch;parent.user=this;
        parent.access_journal=parent_journal.data();
        parent.access_journal_capacity=parent_journal.size();
        put32(0x80021d74u,0);put32(0x80021d78u,1);
        put8(0x80023aecu,3);put8(0x80023b54u,6);
    }
    void put8(std::uint32_t address,std::uint8_t value){globals[address-Base]=value;}
    void put32(std::uint32_t address,std::uint32_t value){for(unsigned i=0;i<4;++i)put8(address+i,std::uint8_t(value>>(8*i)));}
    std::uint32_t get32(std::uint32_t address) const {std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(globals[address-Base+i])<<(8*i);return v;}
    static int dispatch(void* user,const Nba97GameTextMemory* memory,
        const Nba97GameMatchInitializeEvent* event,
        Nba97GameMatchInitializeRegisters* registers) {
        auto& f=*static_cast<Fixture*>(user);f.calls.push_back(event->entry);
        if(event->kind==NBA97_MATCH_INITIALIZE_CHILD_80063D58) {
            Nba97GameRosterBindingsContext child{};
            child.memory=*memory;child.operation_budget=1000;child.registers=*registers;
            child.access_journal=f.roster_journal.data();
            child.access_journal_capacity=f.roster_journal.size();
            f.roster_status=nba97_game_roster_bindings(&child,&f.roster_progress);
            *registers=f.roster_progress.registers;
            return f.roster_status==NBA97_TEXT_COMPLETE;
        }
        return 1;
    }
};

void natural_initializer_composition() {
    Fixture f;
    Nba97GameMatchInitializeAdapterProgress adapter{};
    check(nba97_game_match_initialize_with_zero(&f.parent,1100,
        &f.parent_progress,&adapter)==NBA97_TEXT_COMPLETE);
    check(f.parent_progress.completed && f.parent_progress.callbacks_completed==12);
    check(f.calls.size()==11 && f.calls[0]==0x80063d58u &&
        adapter.memory_zero.completed && adapter.memory_zero_invocations==1);
    check(f.roster_status==NBA97_TEXT_COMPLETE && f.roster_progress.completed &&
        f.roster_progress.operations==159);
    check(f.get32(0x80020b8cu)==0x8002208cu &&
        f.get32(0x80020b90u)==0x800220fau &&
        f.get32(0x80020b98u)==0x8002208cu);
    check(f.get32(0x80020bbcu)==0x800225b4u &&
        f.get32(0x80020bd0u)==0x800227dau &&
        f.get32(0x80020bd4u)==0x800225b4u);
    check(f.parent_progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word==0x80100080u &&
        f.parent_progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word==0x81234567u);
    check(f.roster_progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word==0x8002dbd0u);
    for(auto byte:f.match)check(byte==0);
}
}

int main() {
    natural_initializer_composition();
    std::printf("game_roster_bindings_integration: %u checks passed\n",checks);
}
