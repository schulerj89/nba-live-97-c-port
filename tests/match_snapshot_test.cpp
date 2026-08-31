#include "match_snapshot.hpp"
#include "match_assets.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nba97;
using Bytes=std::vector<uint8_t>;
void check(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
void word(Bytes& b,size_t at,uint32_t v,unsigned n=4) {
    for(unsigned i=0;i<n;++i) {b.at(at+i)=uint8_t(v);v>>=8;}
}
void write(const std::filesystem::path& path,const Bytes& bytes) {
    std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    check(bool(f),"synthetic fixture write");
}
// Hand-packed v5 fixture:368 synthetic base IDs,29 complete teams,20 free
// agents and distinct owned strings. No private database or source data.
Bytes databaseFixture() {
    constexpr unsigned play=124,team=play+368*127,fall=team+29*74,free=fall+50,strings=free+200;
    Bytes b(strings,0);std::copy_n("N97RDB\0\0",8,b.begin());
    word(b,8,5);word(b,12,0x12345678);word(b,16,5);
    auto str=[&](std::string text) {
        const auto offset=uint32_t(b.size()-strings);
        b.insert(b.end(),text.begin(),text.end());b.push_back(0);return offset;
    };
    str("");
    for(unsigned p=0;p<368;++p) {
        const auto at=play+p*127;word(b,at,p,2);
        b[at+7]=uint8_t(p%100);b[at+8]=uint8_t(p%5);b[at+9]=uint8_t(70+p%15);
        for(unsigned r=0;r<17;++r)b[at+14+r]=uint8_t(20+(p*13+r*7)%75);
        for(unsigned r=0;r<10;++r)b[at+31+r]=uint8_t(1+(p+r)%100);
        for(unsigned r=0;r<7;++r)word(b,at+99+r*4,str("Player"+std::to_string(p)+"Field"+std::to_string(r)));
    }
    for(unsigned t=0;t<29;++t) {
        const auto at=team+t*74;word(b,at,t,2);word(b,at+2,12,2);
        for(unsigned n=0;n<5;++n)word(b,at+4+n*4,str("Team"+std::to_string(t)+"Name"+std::to_string(n)));
        for(unsigned i=0;i<15;++i)word(b,at+24+i*2,i<12?t*12+i:0xffff,2);
        for(unsigned m=0;m<20;++m)b[at+54+m]=uint8_t(150+m);
    }
    for(unsigned i=0;i<25;++i)word(b,fall+i*2,i,2);
    for(unsigned i=0;i<100;++i)word(b,free+i*2,i<20?348+i:0xffff,2);
    unsigned at=24;
    auto section=[&](const char* tag,unsigned offset,unsigned count,unsigned stride) {
        std::copy_n(tag,4,b.begin()+at);word(b,at+4,offset);word(b,at+8,count*stride);
        word(b,at+12,count);word(b,at+16,stride);at+=20;
    };
    section("PLAY",play,368,127);section("TEAM",team,29,74);
    section("FALL",fall,25,2);section("FREE",free,100,2);section("STRS",strings,unsigned(b.size()-strings),1);
    word(b,20,unsigned(b.size()));return b;
}
void tests() {
    const auto root=std::filesystem::path(NBA97_SOURCE_DIR)/".local/verification/gameplay";
    std::filesystem::create_directories(root);
    const auto dir=root/("snapshot-unit-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(std::filesystem::create_directory(dir),"fresh isolated fixture directory");
    const auto db_path=dir/"synthetic.n97db";write(db_path,databaseFixture());
    RosterDatabase db;db.load(db_path);const auto base_slots=db.slotTable();const auto base_identity=db.baseIdentity();
    const auto stock_metadata=db.team(0)->source_metadata;
    std::array<int16_t,29> adjustments{};
    std::array<uint8_t,59> defaults;for(unsigned b=0;b<59;++b)defaults[b]=uint8_t(255-b);
    Bytes pack(71,0);std::copy_n("N97C",4,pack.begin());word(pack,4,1,2);word(pack,6,59,2);word(pack,8,0x800c1cd8);
    std::copy(defaults.begin(),defaults.end(),pack.begin()+12);write(dir/"controls.n97ctl",pack);
    check(loadMatchControlDefaults(dir/"controls.n97ctl")==defaults,"bounded default map pack");
    pack.push_back(0);write(dir/"bad-controls.n97ctl",pack);
    bool failed=false;try {loadMatchControlDefaults(dir/"bad-controls.n97ctl");}catch(const std::exception&) {failed=true;}
    check(failed,"trailing control pack refused");
    FrontendSettings settings;
    Nba97CreatedPlayerCatalog created;nba97_created_catalog_init(&created);
    std::vector<UserProfile> profiles(1);profiles[0].id=123;profiles[0].slot=7;profiles[0].name="Saved";
    profiles[0].controls_valid=255;profiles[0].stats.games=77;
    for(unsigned b=0;b<59;++b)profiles[0].controls[b]=uint8_t(b*3);
    MatchRequest request;request.teams={0,1};
    const uint8_t assignment[8]={1,2,0,0,0,0,0,0};const int8_t selectors[8]={7,-2,-2,-2,-2,-2,-2,-2};
    check(nba97_user_setup_open(&request.users,assignment,selectors),"source User Setup entry");
    uint16_t raw[8]={0x80};check(nba97_user_setup_global(&request.users,raw,1)==NBA97_USER_CONFIRMED,"source acceptance");
    MatchSession session;session.initializeFresh(defaults);
    MatchSourceView source{db,settings,profiles,created,adjustments,81,82,83};
    std::array<uint32_t,6> rng{1,2,3,4,5,6}; // Invented source-domain seed, not a runtime initializer.
    const std::array<uint32_t,6> initial_rng{1,2,3,4,5,6},first_after{92,71,51,33,18,8};
    const auto prepared=buildMatchSnapshot(request,source,session.liveControls(),defaults,rng,session.liveStrategy());
    check(initial_rng==rng && !session.snapshot(),
          "pure preparation must not consume caller RNG or publish");
    const auto first=session.capture(request,source,rng); // Own a copy across later publications.
    check(first.presentation.value==0x40 && first.presentation.rng_draws==2 &&
          first.presentation.rejected_draws==1 && !first.presentation.from_schedule &&
          first.frontend_rng_before==initial_rng && first.frontend_rng_after==first_after &&
          first_after==rng &&
          !(first.pending&MatchPresentationVariant),"source presentation value and exact rejected-draw history");
    check(matchSnapshotReceipt(prepared)==matchSnapshotReceipt(first),"preparation and publication agree");
    check(first.teams[0].players[0].first_name=="Player0Field1","owned player string field");
    check(first.teams[0].indices.count==12 && first.teams[0].indices.active_count==12,"count is occupied prefix");
    check(first.controls.profile_ids[0]==123 && first.controls.provenance[0]==NBA97_CONTROLS_PROFILE,"saved controls receipt");
    check(first.roster_generation==81 && first.profile_generation==82 && first.created_generation==83,"source generation receipt");
    for(unsigned r=0;r<5;++r)check(first.teams[0].metadata[r]==first.ranks.value[r][0],"rank overlay");
    check(std::equal(stock_metadata.begin()+5,stock_metadata.end(),first.teams[0].metadata.begin()+5),"metadata remainder preserved");
    session.initializeFresh(defaults);
    check(!std::memcmp(session.liveControls().map[0],profiles[0].controls.data(),59),"reentry cannot reset live maps");
    {
        // These are explicit native field-boundary calls with invented postgame
        // values, not a simulated gameplay return or a memory-card restore.
        const Nba97MatchStrategy cold{{{1,1,0,7,5,0,0},{1,1,0,7,5,0,0}}};
        const Nba97MatchStrategy warmed{{{128,0,255,7,254,1,66},{2,3,4,5,6,7,8}}};
        const Nba97MatchStrategy restored{{{9,10,11,12,13,14,15},{255,254,253,252,251,250,249}}};
        const auto equalValues=[](const Nba97MatchStrategy& a,const Nba97MatchStrategy& b) {
            return std::memcmp(a.side,b.side,sizeof(a.side))==0;
        };
        const auto teamFields=[](const Nba97MatchStrategy& values) {
            std::array<Nba97MatchTeamStrategy,2> teams{};
            for(unsigned side=0;side<2;++side)std::copy_n(values.side[side],7,teams[side].fields);
            return teams;
        };
        const auto rejects=[](const auto& action) {
            bool caught=false;try {action();}catch(const std::runtime_error&) {caught=true;}
            check(caught,"unsupported strategy operation was accepted");
        };
        MatchSession strategies;
        auto strategy_rng=initial_rng;
        auto current=teamFields(warmed);
        check(!strategies.liveStrategy().known && !strategies.initialized(),"uninitialized strategy must be unknown");
        rejects([&]{strategies.writebackStrategy(0,0,current);});
        rejects([&]{strategies.capture(request,source,strategy_rng);});
        check(strategy_rng==initial_rng && !strategies.snapshot() && !strategies.revision() &&
              !strategies.liveStrategy().known,"uninitialized refusal changed strategy or RNG");
        strategies.initializeFresh(defaults);
        check(strategies.liveStrategy().known && !strategies.liveStrategy().writeback_revision &&
              equalValues(strategies.liveStrategy().values,cold),"fresh native strategy values/order");
        rejects([&]{strategies.writebackStrategy(0,0,current);});
        const auto cold_snapshot=strategies.capture(request,source,strategy_rng);
        check(cold_snapshot.strategy.known && !cold_snapshot.strategy.writeback_revision &&
              equalValues(cold_snapshot.strategy.values,cold) &&
              (cold_snapshot.pending&MatchExtensionSettings) && !(cold_snapshot.pending&MatchStrategyFields),
              "known strategy must resolve only its own pending group");
        check(first.strategy.known && equalValues(first.strategy.values,cold),"ordinary capture owns cold strategy");
        const auto cold_revision=strategies.revision();
        const auto prior_snapshot=strategies.snapshot();
        const auto cold_receipt=matchSnapshotReceipt(*prior_snapshot);
        const auto prior_controls=strategies.liveControls();
        const auto prior_rng=strategy_rng;
        strategies.writebackStrategy(cold_revision,0,current);
        check(strategies.liveStrategy().known && strategies.liveStrategy().writeback_revision==cold_revision &&
              equalValues(strategies.liveStrategy().values,warmed),"all fourteen warm bytes must copy unsigned");
        check(strategies.snapshot()==prior_snapshot && strategies.revision()==cold_revision &&
              matchSnapshotReceipt(*strategies.snapshot())==cold_receipt && strategy_rng==prior_rng &&
              !std::memcmp(&strategies.liveControls(),&prior_controls,sizeof(prior_controls)),
              "writeback must not mutate frozen snapshot, controls, capture revision or RNG");
        current={};
        check(equalValues(strategies.liveStrategy().values,warmed),"strategy writeback borrowed caller storage");
        strategies.initializeFresh(defaults);
        check(equalValues(strategies.liveStrategy().values,warmed) &&
              strategies.liveStrategy().writeback_revision==cold_revision,"reinitialization reset warmed strategy");
        auto changed_defaults=defaults;changed_defaults[0]^=1;
        rejects([&]{strategies.initializeFresh(changed_defaults);});
        check(equalValues(strategies.liveStrategy().values,warmed),"refused defaults changed strategy");
        const auto warm_snapshot=strategies.capture(request,source,strategy_rng);
        check(warm_snapshot.strategy.known && warm_snapshot.strategy.writeback_revision==cold_revision &&
              equalValues(warm_snapshot.strategy.values,warmed) &&
              equalValues(cold_snapshot.strategy.values,cold) &&
              warm_snapshot.frontend_rng_before==prior_rng,"recapture reset or borrowed strategy history");

        const auto unchanged=[&](const auto& action) {
            const auto live=strategies.liveStrategy();const auto controls=strategies.liveControls();
            const auto before_rng=strategy_rng;const auto revision=strategies.revision();
            const auto snapshot=strategies.snapshot();const auto receipt=matchSnapshotReceipt(*snapshot);
            action();
            check(strategies.liveStrategy().known==live.known &&
                  strategies.liveStrategy().writeback_revision==live.writeback_revision &&
                  equalValues(strategies.liveStrategy().values,live.values) && strategy_rng==before_rng &&
                  strategies.revision()==revision && strategies.snapshot()==snapshot &&
                  matchSnapshotReceipt(*snapshot)==receipt &&
                  !std::memcmp(&strategies.liveControls(),&controls,sizeof(controls)),
                  "strategy no-op or refusal changed live values, ownership, controls or RNG");
        };
        const auto preserve=[&](const auto& action) {unchanged([&]{rejects(action);});};
        current=teamFields(restored);
        // The original exit reads the live launch word again. It may differ
        // from the zero launch word owned by this accepted snapshot.
        for(const uint16_t live_launch:{uint16_t(1),uint16_t(0x62),uint16_t(0x100),uint16_t(0xffff)})
            unchanged([&]{strategies.writebackStrategy(strategies.revision(),live_launch,current);});
        for(const auto stale:{uint64_t(0),cold_revision,strategies.revision()+1})
            preserve([&]{strategies.writebackStrategy(stale,0,current);});
        auto special=request;special.teams[0]=29;
        preserve([&]{strategies.capture(special,source,strategy_rng);});
        auto season=request;season.setup[1]=1;
        preserve([&]{strategies.capture(season,source,strategy_rng);});

        const auto before_invalidate=matchSnapshotReceipt(*strategies.snapshot());
        strategies.invalidateStrategy();
        check(!strategies.liveStrategy().known && equalValues(strategies.liveStrategy().values,warmed) &&
              matchSnapshotReceipt(*strategies.snapshot())==before_invalidate,
              "invalidation must mark live strategy unknown without rewriting its old snapshot");
        strategies.initializeFresh(defaults);
        check(!strategies.liveStrategy().known && equalValues(strategies.liveStrategy().values,warmed),
              "repeated fresh initializer promoted unknown warm state to cold");
        for(const uint16_t live_launch:{uint16_t(1),uint16_t(0x62),uint16_t(0x100),uint16_t(0xffff)})
            unchanged([&]{strategies.writebackStrategy(strategies.revision(),live_launch,current);});
        preserve([&]{strategies.capture(request,source,strategy_rng);});
        const auto unknown_rng=strategy_rng;
        rejects([&]{buildMatchSnapshot(request,source,strategies.liveControls(),defaults,
                                      strategy_rng,strategies.liveStrategy());});
        check(strategy_rng==unknown_rng,"unknown pure preparation changed the borrowed RNG");
        const auto restored_revision=strategies.revision();
        strategies.writebackStrategy(restored_revision,0,current);
        check(strategies.liveStrategy().known && strategies.liveStrategy().writeback_revision==restored_revision &&
              equalValues(strategies.liveStrategy().values,restored) && strategy_rng==unknown_rng &&
              matchSnapshotReceipt(*strategies.snapshot())==before_invalidate,
              "complete writeback must restore knownness without changing the preceding snapshot or RNG");
        const auto next=strategies.capture(request,source,strategy_rng);
        check(next.strategy.known && next.strategy.writeback_revision==restored_revision &&
              equalValues(next.strategy.values,restored) && (next.pending&MatchExtensionSettings) &&
              !(next.pending&MatchStrategyFields),"restored strategy was not retained by next capture");
    }
    const auto prior_revision=session.revision();const auto prior_receipt=matchSnapshotReceipt(*session.snapshot());
    auto refuse=[&](const MatchRequest& bad,const MatchSourceView& view) {
        bool caught=false;try {session.capture(bad,view,rng);}catch(const std::exception&) {caught=true;}
        check(caught && session.revision()==prior_revision && matchSnapshotReceipt(*session.snapshot())==prior_receipt,
              "refusal must preserve previous snapshot and revision");
        check(!std::memcmp(session.liveControls().map[0],profiles[0].controls.data(),59),"refusal changed live controls");
        check(first_after==rng,"refused preparation consumed live RNG");
    };
    auto bad=request;bad.teams[0]=29;refuse(bad,source);
    bad=request;bad.setup[1]=1;refuse(bad,source);
    bad=request;bad.users.result=0;refuse(bad,source);
    bad=request;bad.users.profile[7]=20;refuse(bad,source);
    RosterDatabase hole=db;Nba97ReorderSession malformed{};
    std::copy_n(base_slots.begin(),15,malformed.original);std::copy_n(base_slots.begin(),15,malformed.slots);
    std::swap(malformed.slots[0],malformed.slots[14]);malformed.phase=NBA97_REORDER_CLOSED;malformed.accepted=1;
    check(hole.applyReorderSession(0,malformed),"isolated interior-hole adapter fixture");
    refuse(request,{hole,settings,profiles,created,adjustments});
    const auto stock_ranks=first.ranks;
    auto moved=base_slots;std::swap(moved[0],moved[8]);
    auto reordered=db.prepareSlotTable(moved);
    const auto reorder_snapshot=session.capture(request,{reordered,settings,profiles,created,adjustments},rng);
    check(reorder_snapshot.frontend_rng_before==first.frontend_rng_after &&
          reorder_snapshot.frontend_rng_after==rng,
          "successive captures continue the caller's RNG without reseeding");
    check(reorder_snapshot.teams[0].roster[0]==8 && reorder_snapshot.accepted_slots==moved,"accepted current order");
    check(std::memcmp(&stock_ranks,&reorder_snapshot.ranks,sizeof(stock_ranks))!=0,"current ranks recomputed after reorder");
    for(unsigned count:{8u,11u,12u,15u}) {
        auto slots=base_slots;std::vector<uint16_t> team_ids,free_ids;
        for(unsigned i=0;i<12;++i)team_ids.push_back(uint16_t(i));
        for(unsigned i=348;i<368;++i)free_ids.push_back(uint16_t(i));
        while(team_ids.size()>count) {free_ids.push_back(team_ids.back());team_ids.pop_back();}
        while(team_ids.size()<count) {team_ids.push_back(free_ids.back());free_ids.pop_back();}
        std::fill_n(slots.begin(),15,uint16_t(0xffff));std::copy(team_ids.begin(),team_ids.end(),slots.begin());
        std::fill(slots.begin()+435,slots.end(),uint16_t(0xffff));std::copy(free_ids.begin(),free_ids.end(),slots.begin()+435);
        const auto variant=db.prepareSlotTable(slots);
        const auto snap=session.capture(request,{variant,settings,profiles,created,adjustments},rng);
        check(snap.teams[0].indices.count==count && snap.teams[0].indices.active_count==(std::min)(count,12u) &&
              snap.teams[0].players.size()==count && snap.accepted_slots==slots,"short/full accepted roster projection");
        for(unsigned i=0;i<12;++i)check(snap.teams[0].indices.alias[i]==(i<count?i:0) &&
                                      snap.teams[0].indices.initial_lineup[i]==i,"aliases and initial lineup");
        check(variant.baseIdentity()==base_identity && variant.originalSlots()==base_slots,"immutable original identity");
    }
    auto none=request;std::fill_n(none.users.profile,8,int8_t(-2));
    profiles.clear();
    const auto retained=session.capture(none,{db,settings,profiles,created,adjustments},rng);
    check(!std::memcmp(retained.controls.controls.map[0],first.controls.controls.map[0],59),"FE retains prior saved controls");
    auto neutral=request;neutral.users.side[0]=1;neutral.users.assignment[0]=0;
    const auto deleted=session.capture(neutral,{db,settings,profiles,created,adjustments},rng);
    check(deleted.controls.profile_ids[0]==0 && deleted.controls.provenance[0]==NBA97_CONTROLS_DEFAULT,
          "deleted neutral profile uses cleared-record defaults");
    MatchSession fresh;fresh.initializeFresh(defaults);
    check(!std::memcmp(fresh.liveControls().map[0],defaults.data(),59),"fresh process defaults");
    created.records[0].raw[0]=uint8_t(493);created.records[0].raw[1]=uint8_t(493>>8);
    created.metadata[0].team=0;created.metadata[0].roster_slot=5;std::strcpy(created.metadata[0].first_name,"Created");
    const auto with_created=session.capture(none,{db,settings,profiles,created,adjustments},rng);
    check((with_created.pending&MatchCreatedMembership) && with_created.teams[0].roster==first.teams[0].roster &&
          !std::memcmp(&with_created.created,&created,sizeof(created)),"created catalogue retained without invented insertion");
    for(unsigned style=0;style<3;++style) {
        const auto path=dir/("settings-"+std::to_string(style)+".ini");
        {std::ofstream f(path);f<<"style="<<style<<"\nrules=9,9,7,1,1,1,1,1,1,1,1,1,1,1\n"
            "custom_rules=2,3,4,1,0,1,0,1,0,1,0,1,0,1\noptions=0,1,2,3,4,5,1,1,4,2,0\n";}
        check(settings.load(path),"isolated inconsistent settings fixture");none.setup[2]=uint8_t(style);
        const auto snapshot=session.capture(none,{db,settings,profiles,created,adjustments},rng);
        check(snapshot.rules[0]==(style==0?0:style==1?4:2) && snapshot.rules[11]==1,"style reapplied including custom backup");
        check(snapshot.options[1]==1 && snapshot.options[10]==0 && settings.rule(0)==9,"settings copied without mutation");
    }
    check(db.slotTable()==base_slots && db.baseIdentity()==base_identity && db.team(0)->source_metadata==stock_metadata,
          "capture never mutates source roster/metadata");
    // Drop every database copy that shares the source catalogue/name storage.
    hole=RosterDatabase{};reordered=RosterDatabase{};db=RosterDatabase{};
    created={};settings=FrontendSettings{};defaults.fill(0);
    check(first.teams[0].players[0].first_name=="Player0Field1" && first.teams[0].names[1]=="Team0Name1" &&
          first.controls.controls.map[0][1]==3,"frozen result owns destroyed source values");
    std::cout<<"MATCH SNAPSHOT PASS: ordinary roster counts/order/ownership/ranks; styles/options; controls lifetime; presentation/shared RNG; persistent strategy/knownness/writeback; created pending; atomic publication\n";
}
}
int main(){try{tests();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
