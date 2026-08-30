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
    MatchSession session;session.initialize(defaults);
    MatchSourceView source{db,settings,profiles,created,adjustments,81,82,83};
    const auto first=session.capture(request,source); // Own a copy across later publications.
    check(first.teams[0].players[0].first_name=="Player0Field1","owned player string field");
    check(first.teams[0].indices.count==12 && first.teams[0].indices.active_count==12,"count is occupied prefix");
    check(first.controls.profile_ids[0]==123 && first.controls.provenance[0]==NBA97_CONTROLS_PROFILE,"saved controls receipt");
    check(first.roster_generation==81 && first.profile_generation==82 && first.created_generation==83,"source generation receipt");
    for(unsigned r=0;r<5;++r)check(first.teams[0].metadata[r]==first.ranks.value[r][0],"rank overlay");
    check(std::equal(stock_metadata.begin()+5,stock_metadata.end(),first.teams[0].metadata.begin()+5),"metadata remainder preserved");
    session.initialize(defaults);
    check(!std::memcmp(session.liveControls().map[0],profiles[0].controls.data(),59),"reentry cannot reset live maps");
    const auto prior_revision=session.revision();const auto prior_receipt=matchSnapshotReceipt(*session.snapshot());
    auto refuse=[&](const MatchRequest& bad,const MatchSourceView& view) {
        bool caught=false;try {session.capture(bad,view);}catch(const std::exception&) {caught=true;}
        check(caught && session.revision()==prior_revision && matchSnapshotReceipt(*session.snapshot())==prior_receipt,
              "refusal must preserve previous snapshot and revision");
        check(!std::memcmp(session.liveControls().map[0],profiles[0].controls.data(),59),"refusal changed live controls");
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
    const auto reorder_snapshot=session.capture(request,{reordered,settings,profiles,created,adjustments});
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
        const auto snap=session.capture(request,{variant,settings,profiles,created,adjustments});
        check(snap.teams[0].indices.count==count && snap.teams[0].indices.active_count==(std::min)(count,12u) &&
              snap.teams[0].players.size()==count && snap.accepted_slots==slots,"short/full accepted roster projection");
        for(unsigned i=0;i<12;++i)check(snap.teams[0].indices.alias[i]==(i<count?i:0) &&
                                      snap.teams[0].indices.initial_lineup[i]==i,"aliases and initial lineup");
        check(variant.baseIdentity()==base_identity && variant.originalSlots()==base_slots,"immutable original identity");
    }
    auto none=request;std::fill_n(none.users.profile,8,int8_t(-2));
    profiles.clear();
    const auto retained=session.capture(none,{db,settings,profiles,created,adjustments});
    check(!std::memcmp(retained.controls.controls.map[0],first.controls.controls.map[0],59),"FE retains prior saved controls");
    auto neutral=request;neutral.users.side[0]=1;neutral.users.assignment[0]=0;
    const auto deleted=session.capture(neutral,{db,settings,profiles,created,adjustments});
    check(deleted.controls.profile_ids[0]==0 && deleted.controls.provenance[0]==NBA97_CONTROLS_DEFAULT,
          "deleted neutral profile uses cleared-record defaults");
    MatchSession fresh;fresh.initialize(defaults);
    check(!std::memcmp(fresh.liveControls().map[0],defaults.data(),59),"fresh process defaults");
    created.records[0].raw[0]=uint8_t(493);created.records[0].raw[1]=uint8_t(493>>8);
    created.metadata[0].team=0;created.metadata[0].roster_slot=5;std::strcpy(created.metadata[0].first_name,"Created");
    const auto with_created=session.capture(none,{db,settings,profiles,created,adjustments});
    check((with_created.pending&MatchCreatedMembership) && with_created.teams[0].roster==first.teams[0].roster &&
          !std::memcmp(&with_created.created,&created,sizeof(created)),"created catalogue retained without invented insertion");
    for(unsigned style=0;style<3;++style) {
        const auto path=dir/("settings-"+std::to_string(style)+".ini");
        {std::ofstream f(path);f<<"style="<<style<<"\nrules=9,9,7,1,1,1,1,1,1,1,1,1,1,1\n"
            "custom_rules=2,3,4,1,0,1,0,1,0,1,0,1,0,1\noptions=0,1,2,3,4,5,1,1,4,2,0\n";}
        check(settings.load(path),"isolated inconsistent settings fixture");none.setup[2]=uint8_t(style);
        const auto snapshot=session.capture(none,{db,settings,profiles,created,adjustments});
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
    std::cout<<"MATCH SNAPSHOT PASS: ordinary roster counts/order/ownership/ranks; styles/options; controls lifetime; created pending; atomic publication\n";
}
}
int main(){try{tests();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
