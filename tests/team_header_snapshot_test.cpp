#include "match_snapshot.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
using namespace nba97;
using Bytes=std::vector<uint8_t>;
unsigned checks=0;
void check(bool ok,const char* why) {++checks;if(!ok)throw std::runtime_error(why);}
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

// Matrix inputs intentionally separate database player IDs, roster-local
// indices, side0/1 and saved profile slots. No source assets or real stores.
RosterDatabase pairedRosters(const RosterDatabase& database,unsigned home_count,unsigned away_count) {
    auto slots=database.slotTable();
    std::vector<uint16_t> pool;
    for(unsigned i=435;i<slots.size();++i)if(slots[i]!=0xffff)pool.push_back(slots[i]);
    for(const auto spec:{std::pair<unsigned,unsigned>{8,home_count},{19,away_count}}) {
        std::vector<uint16_t> roster;
        for(unsigned i=0;i<15;++i)if(slots[spec.first*15+i]!=0xffff)roster.push_back(slots[spec.first*15+i]);
        while(roster.size()>spec.second){pool.push_back(roster.back());roster.pop_back();}
        while(roster.size()<spec.second){check(!pool.empty(),"synthetic free-agent pool");roster.push_back(pool.back());pool.pop_back();}
        std::rotate(roster.begin(),roster.begin()+3,roster.end());
        std::fill_n(slots.begin()+spec.first*15,15,uint16_t(0xffff));
        std::copy(roster.begin(),roster.end(),slots.begin()+spec.first*15);
    }
    std::fill(slots.begin()+435,slots.end(),uint16_t(0xffff));
    std::copy(pool.begin(),pool.end(),slots.begin()+435);
    return database.prepareSlotTable(slots);
}
MatchRequest acceptedRequest(uint16_t home=8,uint16_t away=19) {
    MatchRequest request;request.teams={home,away};
    const uint8_t assignment[8]={1,2,0,0,0,0,0,0};
    const int8_t selected[8]={7,-2,-2,-2,-2,-2,-2,-2};
    check(nba97_user_setup_open(&request.users,assignment,selected),"source UserSetup input");
    uint16_t raw[8]={0x80};
    check(nba97_user_setup_global(&request.users,raw,1)==NBA97_USER_CONFIRMED,"source accepted state5");
    return request;
}
std::vector<UserProfile> profileFixture() {
    std::vector<UserProfile> profiles(3);
    for(unsigned i=0;i<3;++i){
        profiles[i].id=100+i;profiles[i].slot=uint8_t(i==0?7:i==1?0:19);
        profiles[i].name="Profile"+std::to_string(i);profiles[i].stats.games=0x12345678+i;
        profiles[i].stats.wins=0x87654321+i;profiles[i].controls_valid=i==1?0:255;
        for(unsigned j=0;j<59;++j)profiles[i].controls[j]=uint8_t(i*67+j*3);
    }
    profiles[1].controls[14]=0x12;profiles[1].controls[15]=0x34;
    profiles[1].controls[16]=0x56;profiles[1].controls[17]=0x78;
    return profiles;
}

bool sameRef(const Nba97TeamHeaderRef& a,const Nba97TeamHeaderRef& b) {
    return a.kind==b.kind && a.payload==b.payload;
}
void verifyStage(const MatchSnapshot& s,const std::array<unsigned,2>& counts,
                 const Nba97TeamHeaderRef& table24) {
    const auto& stage=s.team_initialization;
    check(stage.stage==MatchTeamStage::After655B0Before65328,"stage must stop before controller/period owners");
    check(stage.table12.kind==NBA97_TEAM_REF_UNKNOWN && !stage.table12.payload,
          "native stats cannot substitute for the raw retail profile prefix");
    check(sameRef(stage.table24,table24),"source table24 raw fixed-slot input");
    check(s.pending==(MatchExtensionSettings|MatchTeamReferenceWords),
          "opaque reference dependency and unrelated extensions must remain pending");
    for(unsigned side=0;side<2;++side) {
        const auto& team=s.teams[side];const auto& h=stage.teams[side];
        const unsigned active=(std::min)(counts[side],12u);
        check(team.indices.count==counts[side] && team.indices.active_count==active &&
              team.players.size()==counts[side],"accepted prefix/active/player counts differ");
        check(h.count66==active && h.count68==active,"both source counts clamp independently to12");
        check(h.opponent_side==1-side && h.metadata_side==side && h.alias_side==side,
              "owned links select sides rather than database team IDs");
        check(h.direction10==(side?0x14e00:-0x14e00),"655B0 direction before period overwrite");
        check(h.field34==7 && h.field38==7 && h.field39==5,
              "655B0 constants must not apply later warm strategy fields");
        const auto scoring=team.metadata[0],defense=team.metadata[3];
        check(scoring==s.ranks.value[0][team.id] && defense==s.ranks.value[3][team.id] &&
              scoring>=1 && scoring<=29 && defense>=1 && defense<=29,"current rank metadata binding");
        check(h.field62==120-2*defense && h.field74==1260-32*scoring &&
              h.field72==(defense+28)/(s.request.setup[3]>1?2:1),
              "source rank54/rank57 thresholds or difficulty branch");
        for(unsigned i=0;i<12;++i) {
            check(team.indices.initial_lineup[i]==i && team.indices.alias[i]==(i<active?i:0),
                  "lineup slots and short-roster aliases remain separate");
            check(h.status[i]==(i<active?0x7fff:0xfffe),"inactive alias cannot become available");
            const auto& player=team.players.at(team.indices.alias[i]);
            check(player.id==team.roster[i<active?i:0],"alias resolves local owned record, not global player ID");
        }
        for(unsigned i=0;i<5;++i) {
            check(h.saved_lineup[i]==i,"saved first-five lineup is natural index order");
            const auto& entity=h.entity[i];const unsigned local=4-i;
            check(entity.table_slot==side*5+local && entity.entity_id==side*5+local &&
                  entity.opponent_d6==(1-side)*5+local,"source descending entity write order or opponent index");
        }
        if(side==0)check(h.word08.kind==NBA97_TEAM_REF_ENTITY && !h.word08.payload &&
                         sameRef(h.word0c,stage.table12),"home source table0/table12 reads");
        else check(sameRef(h.word08,stage.table12) && sameRef(h.word0c,stage.table24),
                   "away source table12/table24 reads");
    }
}
void tests() {
    const auto root=std::filesystem::path(NBA97_SOURCE_DIR)/".local/verification/gameplay";
    std::filesystem::create_directories(root);
    const auto dir=root/("team-header-unit-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(std::filesystem::create_directory(dir),"fresh isolated fixture directory");
    const auto path=dir/"synthetic.n97db";write(path,databaseFixture());
    MatchSnapshot detached;
    std::string detached_receipt;
    {
        RosterDatabase database;database.load(path);
        const auto original_slots=database.slotTable();const auto identity=database.baseIdentity();
        const auto original_metadata=database.team(8)->source_metadata;
        FrontendSettings settings;Nba97CreatedPlayerCatalog created{};
        nba97_created_catalog_init(&created);
        auto profiles=profileFixture();std::array<int16_t,29> adjustments{};
        std::array<uint8_t,59> defaults{};defaults.fill(0xaa);
        std::array<uint32_t,6> rng{1,2,3,4,5,6}; // Invented; no source seed claim.
        MatchSession session;session.initializeFresh(defaults);
        const auto request=acceptedRequest();
        MatchSourceView base{database,settings,profiles,created,adjustments};
        const Nba97TeamHeaderRef raw{0x78563412,NBA97_TEAM_REF_OPAQUE_WORD};
        const Nba97TeamHeaderRef unknown{0,NBA97_TEAM_REF_UNKNOWN};
        const auto initial_rng=rng;
        const auto prepared=buildMatchSnapshot(request,base,session.liveControls(),defaults,rng,session.liveStrategy());
        check(rng==initial_rng && !session.snapshot(),"header preparation cannot consume caller RNG or publish");
        const auto first=session.capture(request,base,rng);
        check(matchSnapshotReceipt(prepared)==matchSnapshotReceipt(first),"prepared header and publication differ");
        verifyStage(first,{12,12},raw);
        check(first.teams[0].players[0].id==96 && first.teams[1].players[0].id==228,
              "fixture must separate player IDs from roster-local indices");
        check(first.teams[0].metadata[0]!=original_metadata[0] &&
              first.teams[0].metadata[3]!=original_metadata[3],"fixture must expose stale stock rank use");
        check(first.controls.controls.map[0][14]!=profiles[1].controls[14],
              "fixture must separate saved slot0 from finalized controller0 controls");

        // Explicit field-only test writeback, not an executed gameplay return.
        std::array<Nba97MatchTeamStrategy,2> warm{};
        for(unsigned side=0;side<2;++side)for(unsigned i=0;i<7;++i)warm[side].fields[i]=uint8_t(190+side*20+i);
        session.writebackStrategy(session.revision(),0,warm);
        bool changed_rank_threshold=false;
        for(const unsigned home:{8u,11u,12u,15u})for(const unsigned away:{8u,11u,12u,15u}) {
            const auto variant=pairedRosters(database,home,away);
            auto selected=request;selected.setup[3]=uint8_t((home+away)%3);
            const auto snapshot=session.capture(selected,{variant,settings,profiles,created,adjustments},rng);
            verifyStage(snapshot,{home,away},raw);
            check(snapshot.accepted_slots==variant.slotTable() && snapshot.base_identity==identity,
                  "header initialization reverted the accepted roster or identity");
            check(snapshot.teams[0].players[0].id==99 && snapshot.teams[1].players[0].id==231,
                  "accepted rotated roster must keep its player order");
            for(unsigned side=0;side<2;++side) {
                check(!std::memcmp(snapshot.strategy.values.side[side],warm[side].fields,7),
                      "header stage changed the separate persistent strategy snapshot");
                changed_rank_threshold|=snapshot.team_initialization.teams[side].field74!=
                                       first.team_initialization.teams[side].field74;
            }
        }
        check(changed_rank_threshold,"fixture must exercise rank changes after accepted order/count changes");

        const auto mirror=session.capture(acceptedRequest(8,8),base,rng);
        verifyStage(mirror,{12,12},raw);
        check(mirror.teams[0].id==mirror.teams[1].id &&
              mirror.team_initialization.teams[0].metadata_side!=mirror.team_initialization.teams[1].metadata_side,
              "mirror team IDs must not collapse distinct side objects");
        std::rotate(profiles.begin(),profiles.begin()+1,profiles.end());
        auto none=request;std::fill_n(none.users.profile,8,int8_t(-2));
        const auto reordered_profiles=session.capture(none,{database,settings,profiles,created,adjustments},rng);
        verifyStage(reordered_profiles,{12,12},raw);
        check(profiles[0].slot==0 && !profiles[0].controls_valid,"profile fixture lost raw valid0 case");
        profiles[0].stats={};
        const auto zero_stats=session.capture(none,{database,settings,profiles,created,adjustments},rng);
        verifyStage(zero_stats,{12,12},raw);
        std::fill_n(profiles[0].controls.begin()+14,4,uint8_t(0));
        const auto zero_word=session.capture(none,{database,settings,profiles,created,adjustments},rng);
        // NULL here means a known32-bit zero word, not a dereferenceable field role.
        verifyStage(zero_word,{12,12},{0,NBA97_TEAM_REF_NULL});
        profiles[0].controls[14]=0xec;profiles[0].controls[15]=0xdc;
        profiles[0].controls[16]=0x0f;profiles[0].controls[17]=0x80;
        const auto address_bits=session.capture(none,{database,settings,profiles,created,adjustments},rng);
        verifyStage(address_bits,{12,12},{0x800fdcec,NBA97_TEAM_REF_OPAQUE_WORD});
        profiles.erase(profiles.begin());
        const auto missing=session.capture(none,{database,settings,profiles,created,adjustments},rng);
        verifyStage(missing,{12,12},unknown);

        const auto refuse=[&](const MatchRequest& bad,const MatchSourceView& input) {
            const auto before_rng=rng;const auto before_revision=session.revision();
            const auto before=session.snapshot();const auto receipt=matchSnapshotReceipt(*before);
            const auto controls=session.liveControls();const auto strategy=session.liveStrategy();
            bool caught=false;try{session.capture(bad,input,rng);}catch(const std::runtime_error&){caught=true;}
            check(caught && rng==before_rng && session.revision()==before_revision && session.snapshot()==before &&
                  matchSnapshotReceipt(*before)==receipt &&
                  !std::memcmp(&controls,&session.liveControls(),sizeof(controls)) &&
                  strategy.known==session.liveStrategy().known &&
                  strategy.writeback_revision==session.liveStrategy().writeback_revision &&
                  !std::memcmp(strategy.values.side,session.liveStrategy().values.side,sizeof(strategy.values.side)),
                  "refused header preparation changed publication, RNG, controls or strategy");
        };
        auto bad=none;bad.teams[0]=29;refuse(bad,{database,settings,profiles,created,adjustments});
        bad=none;bad.setup[3]=3;refuse(bad,{database,settings,profiles,created,adjustments});
        auto duplicates=profileFixture();duplicates[0].slot=0;
        refuse(none,{database,settings,duplicates,created,adjustments});
        const auto short_roster=pairedRosters(database,7,12);
        refuse(none,{short_roster,settings,profiles,created,adjustments});
        session.invalidateStrategy();
        refuse(none,{database,settings,profiles,created,adjustments});

        auto copy=address_bits;detached=std::move(copy);
        detached_receipt=matchSnapshotReceipt(detached);
        check(database.slotTable()==original_slots && database.team(8)->source_metadata==original_metadata,
              "stage mutated the accepted source database");
    }
    // Every source database, profile vector, session and earlier snapshot has
    // died. Only the copied/moved result remains; links are side/entity IDs.
    verifyStage(detached,{12,12},{0x800fdcec,NBA97_TEAM_REF_OPAQUE_WORD});
    check(matchSnapshotReceipt(detached)==detached_receipt &&
          detached.teams[0].players[0].first_name=="Player96Field1" &&
          detached.teams[1].names[1]=="Team19Name1","copied/moved header stage borrowed destroyed sources");
    for(unsigned side=0;side<2;++side) {
        const auto& h=detached.team_initialization.teams[side];
        check(detached.teams[h.metadata_side].metadata[0]==detached.ranks.value[0][detached.teams[side].id] &&
              detached.teams[h.alias_side].players.at(detached.teams[h.alias_side].indices.alias[0]).id==
                  detached.teams[side].roster[0] &&
              detached.teams[h.opponent_side].id==detached.request.teams[1-side],
              "relocatable references do not resolve against their new owning snapshot");
    }
    std::cout<<"TEAM HEADER SNAPSHOT PASS: "<<checks<<" assertions;16 asymmetric roster pairs; fresh ranks; side/entity references; raw slot0 provenance; pre-period stage; ownership and atomic refusal\n";
}
}
int main(){try{tests();return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
