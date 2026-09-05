#include "recovered/game_roster_bindings.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_impl(bool value,int line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game roster-bindings check %u failed at line %d\n", checks,line);
        std::exit(1);
    }
}
#define check(value) check_impl((value),__LINE__)

constexpr std::uint32_t Base=0x80015000u,End=0x80024800u;
constexpr std::uint32_t Team0=0x80021d74u,Team1=0x80021d78u;
constexpr std::uint32_t Records=0x8002208cu,AwayRecords=0x800225b4u;

struct Fixture {
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(End-Base,0xcc);
    std::vector<std::uint8_t> known=std::vector<std::uint8_t>(End-Base,1);
    Nba97GameTextRegion regions[2]{{Base,bytes.data(),known.data(),bytes.size()},{}};
    std::array<Nba97GameRosterBindingsAccess,200> journal{};
    Nba97GameRosterBindingsContext context{};
    Nba97GameRosterBindingsProgress progress{};
    std::uint8_t wrapped_count=0;
    std::uint8_t wrapped_known=1;

    Fixture(std::uint32_t home=2,std::uint32_t away=5,
            std::uint8_t home_count=12,std::uint8_t away_count=12) {
        context.memory={regions,1};
        context.operation_budget=1000;
        context.access_journal=journal.data();
        context.access_journal_capacity=journal.size();
        for(unsigned i=0;i<NBA97_MATCH_INITIALIZE_REGISTER_COUNT;++i) {
            context.registers.gpr[i].word=0x55000000u+i;
            context.registers.gpr[i].known_mask=0x0f;
        }
        context.registers.gpr[0]={0,0x0f};
        put32(Team0,home);put32(Team1,away);
        const std::uint32_t home_address=0x80023aecu+home*0x68u;
        const std::uint32_t away_address=0x80023aecu+away*0x68u;
        if(home_address>=Base&&home_address<End)put8(home_address,home_count);
        if(away_address>=Base&&away_address<End)put8(away_address,away_count);
    }
    std::size_t offset(std::uint32_t address) const {return address-Base;}
    void put8(std::uint32_t address,std::uint8_t value) {bytes[offset(address)]=value;}
    void put16(std::uint32_t address,std::uint16_t value) {
        put8(address,std::uint8_t(value));put8(address+1,std::uint8_t(value>>8));
    }
    void put32(std::uint32_t address,std::uint32_t value) {
        for(unsigned i=0;i<4;++i)put8(address+i,std::uint8_t(value>>(8*i)));
    }
    std::uint16_t get16(std::uint32_t address) const {
        return std::uint16_t(bytes[offset(address)]|
            (std::uint16_t(bytes[offset(address+1)])<<8));
    }
    std::uint32_t get32(std::uint32_t address) const {
        std::uint32_t value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(bytes[offset(address+i)])<<(8*i);return value;
    }
    int run() {return nba97_game_roster_bindings(&context,&progress);}
};

void normal_tables_and_registers() {
    Fixture f(2,5,4,9);
    const auto original_s0=f.context.registers.gpr[NBA97_MATCH_INITIALIZE_S0];
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations==159 && f.progress.accesses==159 &&
        f.progress.reads==50 && f.progress.stores==109 &&
        f.progress.access_events==159 && !f.progress.stopped_pc);
    check(f.get16(0x8001edf4u)==2 && f.get16(0x8001eeb8u)==5);
    check(f.get32(0x8001edf8u)==0x8001eeb8u &&
        f.get32(0x8001eebcu)==0x8001edf4u &&
        f.get32(0x80015030u)==0x80015034u);
    for(unsigned i=0;i<32;++i)
        check(f.get32(0x80020b88u-i*4u)==0x80024748u-i*0x68u);
    for(unsigned i=0;i<12;++i) {
        const std::uint32_t home=i<4?Records+i*0x6eu:Records;
        const std::uint32_t away=i<9?AwayRecords+i*0x6eu:AwayRecords;
        check(f.get32(0x80020b8cu+i*4u)==home &&
            f.get32(0x80015034u+i*4u)==home);
        check(f.get32(0x80020bbcu+i*4u)==away &&
            f.get32(0x80015064u+i*4u)==away);
        check(f.get16(0x8001ee0au+i*2u)==i &&
            f.get16(0x8001eeceu+i*2u)==i);
    }
    const auto& r=f.progress.registers.gpr;
    check(r[NBA97_MATCH_INITIALIZE_A0].word==AwayRecords &&
        r[NBA97_MATCH_INITIALIZE_A1].word==0x80020bbcu &&
        r[NBA97_MATCH_INITIALIZE_A2].word==0x80015064u &&
        r[NBA97_MATCH_INITIALIZE_A3].word==12u &&
        r[NBA97_MATCH_INITIALIZE_V0].word==0u &&
        r[NBA97_MATCH_INITIALIZE_V1].word==5u*0x68u &&
        r[NBA97_MATCH_INITIALIZE_AT].word==0x80020016u &&
        r[NBA97_MATCH_INITIALIZE_T0].word==0x80020becu &&
        r[9].word==0x80015094u && r[10].word==0x8001ee22u &&
        r[11].word==0xa50u && r[12].word==24u &&
        r[13].word==Records && r[14].word==AwayRecords);
    check(r[NBA97_MATCH_INITIALIZE_S0].word==original_s0.word &&
        r[NBA97_MATCH_INITIALIZE_S0].known_mask==original_s0.known_mask);
}

void every_unsigned_count() {
    for(unsigned count=0;count<256;++count) {
        Fixture f(0,1,std::uint8_t(count),std::uint8_t(count));
        check(f.run()==NBA97_TEXT_COMPLETE);
        for(unsigned i=0;i<12;++i) {
            const auto home=i<count?Records+i*0x6eu:Records;
            const auto away=i<count?AwayRecords+i*0x6eu:AwayRecords;
            check(f.get32(0x80020b8cu+i*4u)==home);
            check(f.get32(0x80020bbcu+i*4u)==away);
        }
    }
    Fixture unclamped(0,1,255,255);
    check(unclamped.run()==NBA97_TEXT_COMPLETE &&
        unclamped.get32(0x80020bb8u)==Records+11u*0x6eu &&
        unclamped.get32(0x80020be8u)==AwayRecords+11u*0x6eu);
}

struct MutationState {Fixture* fixture;unsigned home_counts;unsigned away_counts;};
int mutate_live(void* user,const Nba97GameTextMemory*,
    const Nba97GameRosterBindingsAccess* event,
    Nba97GameMatchInitializeRegisters* registers) {
    auto& state=*static_cast<MutationState*>(user);
    if(event->pc==0x80063e20u && state.home_counts++==0) {
        state.fixture->put32(Team0,1);
        state.fixture->put8(0x80023aecu+0x68u,0);
        registers->gpr[NBA97_MATCH_INITIALIZE_S0]={0x12345678u,0x0f};
    }
    if(event->pc==0x80063e74u && state.away_counts++==0) {
        state.fixture->put32(Team1,3);
        state.fixture->put8(0x80023aecu+3u*0x68u,0);
    }
    return 1;
}

void live_reloads_and_observation() {
    Fixture f(0,2,12,12);f.put8(0x80023aecu+0x68u,12);f.put8(0x80023aecu+3u*0x68u,12);
    MutationState state{&f};f.context.observer=mutate_live;f.context.user=&state;
    check(f.run()==NBA97_TEXT_COMPLETE && state.home_counts==12 && state.away_counts==12);
    check(f.get32(0x80020b8cu)==Records && f.get32(0x80020b90u)==Records);
    check(f.get32(0x80020bbcu)==AwayRecords && f.get32(0x80020bc0u)==AwayRecords);
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word==0x12345678u);
    unsigned team0_reads=0,team1_reads=0;
    for(std::size_t i=0;i<f.progress.access_events;++i) {
        team0_reads+=f.journal[i].pc==0x80063dfcu;
        team1_reads+=f.journal[i].pc==0x80063e50u;
    }
    check(team0_reads==12 && team1_reads==12);
}

struct DelayState {bool reverse_seen=false,home_changed=false,away_changed=false;};
int mutate_delay_stores(void* user,const Nba97GameTextMemory*,
    const Nba97GameRosterBindingsAccess* event,
    Nba97GameMatchInitializeRegisters* registers) {
    auto& state=*static_cast<DelayState*>(user);
    if(event->pc==0x80063dbcu)
        state.reverse_seen=registers->gpr[NBA97_MATCH_INITIALIZE_A2].word==0x80023a48u;
    if(event->pc==0x80063e34u&&!state.home_changed) {
        registers->gpr[NBA97_MATCH_INITIALIZE_A0]={0xdeadbeefu,0x0f};
        state.home_changed=true;
    } else if(event->pc==0x80063e3cu&&state.home_changed)
        registers->gpr[NBA97_MATCH_INITIALIZE_A0]={Records,0x0f};
    if(event->pc==0x80063e88u&&!state.away_changed) {
        registers->gpr[NBA97_MATCH_INITIALIZE_V0]={0xcafebabeu,0x0f};
        state.away_changed=true;
    }
    return 1;
}

void live_delay_slot_sources() {
    Fixture f(0,1,12,12);DelayState state{};
    f.context.observer=mutate_delay_stores;f.context.user=&state;
    check(f.run()==NBA97_TEXT_COMPLETE && state.reverse_seen &&
        state.home_changed && state.away_changed);
    check(f.get32(0x80020b8cu)==Records && f.get32(0x80015034u)==0xdeadbeefu);
    check(f.get32(0x80020bbcu)==AwayRecords && f.get32(0x80015064u)==0xcafebabeu);
}

struct SnapshotMutation {Fixture* fixture;};
int snapshot_mutation(void* user,const Nba97GameTextMemory*,
    const Nba97GameRosterBindingsAccess* event,Nba97GameMatchInitializeRegisters*) {
    auto& state=*static_cast<SnapshotMutation*>(user);
    if(event->pc==0x80063d98u) {
        state.fixture->put32(Team0,0);state.fixture->put32(Team1,1);
        state.fixture->put8(0x80023aecu,0);state.fixture->put8(0x80023b54u,0);
    }
    return 1;
}

void truncation_and_access_order() {
    Fixture f(0x12345678u,0xabcdef01u,0,0);
    SnapshotMutation state{&f};f.context.observer=snapshot_mutation;f.context.user=&state;
    check(f.run()==NBA97_TEXT_COMPLETE);
    check(f.get16(0x8001edf4u)==0x5678u && f.get16(0x8001eeb8u)==0xef01u);
    const std::array<std::uint32_t,7> first={0x80063d68u,0x80063d70u,
        0x80063d88u,0x80063d90u,0x80063d94u,0x80063d98u,0x80063d9cu};
    for(unsigned i=0;i<first.size();++i)check(f.journal[i].pc==first[i]);
    for(unsigned i=0;i<32;++i)check(f.journal[6+i].address==0x80020b88u-i*4u);
    check(f.journal[38].pc==0x80063dbcu);
    const std::array<std::uint32_t,10> loop={0x80063dfcu,0x80063e20u,
        0x80063e40u,0x80063e44u,0x80063e48u,0x80063e50u,
        0x80063e74u,0x80063e94u,0x80063e98u,0x80063ea4u};
    for(unsigned iteration=0;iteration<12;++iteration)
        for(unsigned i=0;i<loop.size();++i)
            check(f.journal[39+iteration*10+i].pc==loop[i]);
}

void every_budget_prefix() {
    Fixture complete(0,1,5,7);const auto initial=complete.bytes;
    check(complete.run()==NBA97_TEXT_COMPLETE);
    for(std::size_t budget=0;budget<complete.progress.operations;++budget) {
        Fixture f(0,1,5,7);f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations==budget && f.progress.accesses==budget);
        check(f.progress.stopped_pc==complete.journal[budget].pc &&
            f.progress.stopped_address==complete.journal[budget].address);
        auto expected=initial;
        for(std::size_t i=0;i<budget;++i) if(complete.journal[i].kind==NBA97_GAME_ROSTER_BINDINGS_STORE)
            for(unsigned b=0;b<complete.journal[i].width;++b)
                expected[complete.journal[i].address-Base+b]=std::uint8_t(complete.journal[i].value>>(8*b));
        check(f.bytes==expected);
    }
}

int refuse_first(void*,const Nba97GameTextMemory*,
    const Nba97GameRosterBindingsAccess*,Nba97GameMatchInitializeRegisters*) {return 0;}

void wrap_unknown_mapping_alignment_alias_and_arguments() {
    Fixture wrap(0x013b0e34u,1,0,0);wrap.regions[1]={0x0000000cu,&wrap.wrapped_count,&wrap.wrapped_known,1};
    wrap.context.memory.count=2;wrap.wrapped_count=1;
    check(wrap.run()==NBA97_TEXT_COMPLETE);
    bool saw_wrap=false;for(std::size_t i=0;i<wrap.progress.access_events;++i)
        saw_wrap|=wrap.journal[i].pc==0x80063e20u&&wrap.journal[i].address==0x0000000cu;
    check(saw_wrap && wrap.get32(0x80020b8cu)==Records);

    Fixture unknown(0,1,12,12);unknown.known[unknown.offset(0x80023aecu)]=0;
    check(unknown.run()==NBA97_TEXT_UNKNOWN && unknown.progress.operations==41 &&
        unknown.progress.stopped_pc==0x80063e2cu);
    Fixture malformed;malformed.known[malformed.offset(Team0)]=2;
    check(malformed.run()==NBA97_TEXT_ARGUMENT && malformed.progress.operations==1);
    Fixture missing;missing.regions[0].size=missing.offset(Team0);
    check(missing.run()==NBA97_TEXT_RESOURCE && missing.progress.operations==1 &&
        missing.progress.stopped_address==Team0);
    Fixture refused;refused.context.observer=refuse_first;
    check(refused.run()==NBA97_TEXT_IO_REFUSED && refused.progress.operations==1 &&
        refused.progress.reads==1 && refused.progress.access_events==1);

    struct Unalign {bool done;};Unalign unalign{};
    auto observer=[](void* user,const Nba97GameTextMemory*,const Nba97GameRosterBindingsAccess* e,
            Nba97GameMatchInitializeRegisters* r)->int {
        auto& state=*static_cast<Unalign*>(user);
        if(!state.done&&e->pc==0x80063e20u){r->gpr[NBA97_MATCH_INITIALIZE_A1].word|=1;state.done=true;}return 1;};
    Fixture alignment(0,1,12,12);alignment.context.observer=observer;alignment.context.user=&unalign;
    check(alignment.run()==NBA97_TEXT_ALIGNMENT_TRAP && alignment.progress.stopped_pc==0x80063e34u);

    std::array<std::uint8_t,4> shared{0,0,0,0};
    std::array<std::uint8_t,4> shared_known{1,1,1,1};
    Nba97GameTextRegion alias_regions[2]={{Team0,shared.data(),shared_known.data(),4},
        {Team1,shared.data(),shared_known.data(),4}};
    Fixture alias;alias.context.memory={alias_regions,2};
    check(alias.run()==NBA97_TEXT_RESOURCE && alias.progress.operations==3);
    Nba97GameTextRegion overlap[2]={{Team0,shared.data(),nullptr,4},{Team0+2,shared.data(),nullptr,4}};
    Fixture bad_regions;bad_regions.context.memory={overlap,2};
    check(bad_regions.run()==NBA97_TEXT_ARGUMENT);
    Nba97GameRosterBindingsProgress progress{};
    check(nba97_game_roster_bindings(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture null_out;check(nba97_game_roster_bindings(&null_out.context,nullptr)==NBA97_TEXT_ARGUMENT);
    Fixture bad_zero;bad_zero.context.registers.gpr[0].word=1;
    check(bad_zero.run()==NBA97_TEXT_ARGUMENT);
    Fixture unknown_ra;unknown_ra.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask=0;
    check(unknown_ra.run()==NBA97_TEXT_UNKNOWN && unknown_ra.progress.operations==159 &&
        unknown_ra.progress.stopped_pc==0x80063ed4u);
}
}

int main() {
    normal_tables_and_registers();every_unsigned_count();
    live_reloads_and_observation();live_delay_slot_sources();truncation_and_access_order();
    every_budget_prefix();wrap_unknown_mapping_alignment_alias_and_arguments();
    std::printf("game_roster_bindings: %u checks passed\n",checks);
}
