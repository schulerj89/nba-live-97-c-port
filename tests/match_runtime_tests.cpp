#include "match_runtime.hpp"
#include <cstdio>
#include <cstdlib>

using namespace nba97;
using Bytes=std::vector<std::uint8_t>;
static unsigned checks;
static void check(bool ok,const char* reason) {
    ++checks;if(!ok){std::fprintf(stderr,"runtime check%u: %s\n",checks,reason);std::exit(1);}
}
static void word(Bytes& b,unsigned offset,std::uint32_t v) {
    for(unsigned i=0;i<4;++i)b.at(offset+i)=std::uint8_t(v>>(8*i));
}
static GameplaySetupResource resources() {
    Bytes motion(0x400);word(motion,0,8);word(motion,4,0x158);
    for(unsigned base:{8u,0x158u}) {word(motion,base,0x300);word(motion,base+39*4,0x310);}
    motion[0x307]=8;word(motion,0x308,12);motion[0x317]=8;word(motion,0x318,12);
    Bytes pack(2132);const char magic[]="NBA97PER";
    for(unsigned i=0;i<8;++i)pack[i]=std::uint8_t(magic[i]);
    word(pack,8,1);word(pack,12,2112);
    for(unsigned f=0;f<2;++f)for(unsigned i=0;i<5;++i) {
        const std::int16_t xyz[]={std::int16_t(100+20*i+f),std::int16_t(200+20*i+f),1};
        for(unsigned c=0;c<3;++c) {
            const unsigned at=20+f*32+i*6+c*2;const auto v=std::uint16_t(xyz[c]);
            pack[at]=std::uint8_t(v);pack[at+1]=std::uint8_t(v>>8);
        }
    }
    for(unsigned i=0;i<512;++i)word(pack,84+i*4,18000+i);
    std::uint32_t crc=0xffffffffu;
    for(unsigned i=20;i<pack.size();++i) {
        crc^=pack[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    word(pack,16,~crc);return decodeGameplaySetup(pack,decode_gameplay_mocap(std::move(motion)));
}
static MatchSnapshot snapshot(bool humans) {
    MatchSnapshot s;s.request.teams={0,1};s.request.setup={0,0,0,1};
    s.team_initialization.stage=MatchTeamStage::After655B0Before65328;
    for(unsigned side=0;side<2;++side) {
        auto& team=s.teams[side];team.id=std::uint16_t(side);team.indices.count=12;team.indices.active_count=5;
        for(unsigned i=0;i<12;++i) {
            team.indices.alias[i]=std::uint8_t(i);team.indices.initial_lineup[i]=std::uint16_t(i);
            PlayerRecord player;player.id=std::uint16_t(side*12+i);player.height_inches=std::uint8_t(70+i);
            player.source_word_12=std::uint16_t(side?0:0x100);player.ratings.fill(std::uint8_t(50+i));
            player.last_name="Owned player";team.players.push_back(player);
        }
        Nba97TeamHeaderInput in{};in.side_word=std::uint16_t(side*5);in.opponent_side_word=std::uint16_t((1-side)*5);
        in.count=12;in.injury_slot=255;in.difficulty=1;
        for(unsigned i=0;i<5;++i)in.lineup[i]=std::uint16_t(i);
        check(nba97_team_header_initialize(&s.team_initialization.teams[side],&in)==1,"team fixture source owner");
    }
    Nba97GameControllersInput in{};
    if(humans) {in.assignment[0]=1;in.assignment[1]=2;}
    check(nba97_game_controllers_initialize(&s.controller_initialization.effects,&in)==1,"controller fixture source owner");
    s.controller_initialization.prepared=true;return s;
}
static MatchRuntimeEntry entry() {
    MatchRuntimeEntry e;
    // Synthetic source-state fixture, NOT proof that every game loader has run.
    for(auto& entity:e.entity)entity.clearFromSource();
    for(auto& v:e.scalar)v={0,1};for(auto& v:e.auxiliary)v={0,1};return e;
}
static std::uint64_t fingerprint(const MatchRuntimeState& s) {
    std::uint64_t h=1469598103934665603ull;
    auto value=[&](std::uint64_t v){for(unsigned i=0;i<8;++i){h^=std::uint8_t(v>>(8*i));h*=1099511628211ull;}};
    auto field=[&](Nba97GamePeriodValue v){value(v.word);value(v.known);};
    auto record=[&](const auto& r){for(auto v:r.bytes)value(v);for(auto v:r.known)value(v);};
    auto ref=[&](Nba97GamePeriodReference r){value(r.record);value(r.known);};
    for(const auto& r:s.team)record(r);for(const auto& r:s.status)record(r);for(const auto& r:s.controller)record(r);
    for(const auto& e:s.entity){record(e.record);field(e.player);field(e.status);field(e.opponent);}
    for(auto v:s.scalar)field(v);for(auto v:s.auxiliary)field(v);field(s.incoming_s6);
    for(auto r:s.entity_table)ref(r);for(auto r:s.render_table)ref(r);for(auto r:s.controller_table)ref(r);
    for(auto v:s.active_player)field(v);for(auto v:s.active_status)field(v);ref(s.ball);ref(s.reference34);
    value(s.completed_period_initializations);return h;
}
static void complete(MatchRuntimeState& s) {
    const auto r=initializeMatchRuntimePeriod(s);
    if(r.result!=NBA97_PERIOD_COMPLETE)std::fprintf(stderr,"period result%d owner%d dependency%d %s\n",
        r.result,r.pending_owner,r.dependency_result,r.detail.c_str());
    check(r.result==NBA97_PERIOD_COMPLETE && r.receipt.complete,"composed period completed");
    for(unsigned i=0;i<r.receipt.count;++i)if(r.receipt.event[i].kind==NBA97_PERIOD_EVENT_CALL)
        check(r.receipt.event[i].call_completed==1,"every synchronous call actually completed");
}
int main() {
    auto source=snapshot(true);auto setup=resources();auto s=prepareMatchRuntime(source,setup,entry());
    source.teams[0].players[0].height_inches=1;source.teams[0].players[0].last_name="mutated source";
    check(s.players[0].height_inches==70 && s.accepted->teams[0].players[0].last_name=="Owned player","accepted lifetime isolation");
    check(s.entity[9].record.read(0,4).word==0 && s.entity[9].record.read(0xd9,1).word==0,"655B0 does not invent entity IDs");
    for(const auto& e:s.entity)check(e.player.known==0 && e.player.word==0 && e.status.known==0 && e.status.word==0 &&
        e.opponent.known==0 && e.opponent.word==0,"new native reference metadata is canonically unknown");
    check(!s.controller[0].read(0x26,2).known && !s.controller[0].read(0x28,4).known,"unknown selected/gap preserved");
    check(s.controller[2].read(0x26,2).word==0xffff,"neutral controller selected source store");
    complete(s);
    check(s.completed_period_initializations==1,"completed count");
    check(s.scalar[NBA97_PERIOD_FDB90].word==0x81 && s.scalar[NBA97_PERIOD_FDB58].word==18000,"tipoff phase/duration");
    check(s.entity[5].record.read(0xc,4).word==200u*256,"first preparation uses early home-player binding for away center hand");
    check(s.entity[5].player.word==12,"late binding resolves accepted away roster");
    check(!s.entity[10].player.known && !s.entity[10].status.known && !s.entity[10].opponent.known,
        "ball never acquires fabricated player/status/opponent reference");
    for(unsigned i=0;i<10;++i) {
        check(s.entity[i].record.read(0,4).word==i,"initialized entity ID");
        check(s.entity[i].record.read(0xd9,1).word==(i<5?0u:5u),"initialized team side");
        check(s.entity[i].player.word==(i<5?i:i+7),"current accepted player binding");
        check(s.entity[i].record.read(0xc6,2).word==unsigned(70+i%5)*256/78,"per-player height scale");
        check(s.entity[i].record.read(0xcc,2).known && s.entity[i].record.read(0xcf,1).known,"opponent halfword leaves adjacent bytes intact");
    }
    check(s.entity[0].record.read(0x46,2).word==39 && s.entity[5].record.read(0x4a,2).word==39,"actual tipoff motion owner");
    check(s.controller[0].read(0x26,2).known && s.controller[1].read(0x26,2).known,"human selection producer resolved unknown");
    complete(s);
    check(s.completed_period_initializations==2,"second source preparation retained");
    check(s.entity[5].record.read(0xc,4).word==std::uint32_t(-200*256),"second preparation sees corrected away player hand");
    check(s.team[0].read(0x48,4).word==18000,"quarter0 repeated initialization does not add cumulative time");
    auto failed=s;failed.entity[0].record.write(0,4,{});auto before=fingerprint(failed);
    auto failure=initializeMatchRuntimePeriod(failed);
    check(failure.result!=NBA97_PERIOD_COMPLETE && fingerprint(failed)==before,"unknown field refusal is whole-context atomic");
    failed=s;failed.players[0].height_inches=0;before=fingerprint(failed);failure=initializeMatchRuntimePeriod(failed);
    check(failure.result!=NBA97_PERIOD_COMPLETE && failure.detail.find("divide trap")!=std::string::npos,"original divide trap not repaired");
    check(fingerprint(failed)==before,"source trap prefix stays inside candidate");
    failed=s;failed.team[0].put(0x76,1,1);failed.team[0].put(0x98,2,5);before=fingerprint(failed);
    failure=initializeMatchRuntimePeriod(failed);
    check(failure.result==NBA97_PERIOD_CALLBACK_PENDING && failure.pending_owner==NBA97_PERIOD_CALL_65140,"uncomposed substitution explicitly pending");
    check(fingerprint(failed)==before,"nested missing dependency does not publish earlier binding writes");
    auto cpu=prepareMatchRuntime(snapshot(false),setup,entry());complete(cpu);
    check(cpu.controller[0].read(0x26,2).word==0xffff && cpu.team[0].read(0x42,2).word==0,"CPU-only preparation");
    for(unsigned quarter:{1u,2u,3u,4u}) {
        auto next=s;next.scalar[NBA97_PERIOD_FDB68]={quarter,1};complete(next);
        check(next.completed_period_initializations==3,"later period cloned context");
        check(next.scalar[NBA97_PERIOD_FDB58].word==(quarter==4?18256u:18000u),"overtime table selection");
    }
    std::printf("MATCH RUNTIME PASS: %u checks; composed period, accepted ownership, preserved two-pass bindings, atomic failures\n",checks);
}
