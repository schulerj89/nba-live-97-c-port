#include "recovered/team_header.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)

static Nba97TeamHeaderRef reference(uint8_t kind,uint32_t payload=0) {
    Nba97TeamHeaderRef ref{};ref.kind=kind;ref.payload=payload;return ref;
}
static bool equalRef(const Nba97TeamHeaderRef& a,const Nba97TeamHeaderRef& b) {
    return a.kind==b.kind && a.payload==b.payload;
}
static Nba97TeamHeaderInput input(bool away=false) {
    Nba97TeamHeaderInput in{};
    in.side_word=away ? 5:0;in.opponent_side_word=away ? 0:5;
    in.count=9;in.injury_slot=3;in.difficulty=2;in.rank54=29;in.rank57=1;
    const uint16_t lineup[5]={7,0,11,0xffff,0x8000};
    std::memcpy(in.lineup,lineup,sizeof(lineup));
    in.table12=reference(NBA97_TEAM_REF_OPAQUE_WORD,0xfedcba98);
    in.table24=reference(NBA97_TEAM_REF_OPAQUE_WORD,0x01234567);
    return in;
}

static void completeFixtures() {
    const uint16_t availability[12]={0x7fff,0x7fff,0x7fff,0xfffe,0x7fff,0x7fff,
                                    0x7fff,0x7fff,0x7fff,0xfffe,0xfffe,0xfffe};
    const Nba97TeamHeaderEntityEffect home[5]={{4,4,9},{3,3,8},{2,2,7},{1,1,6},{0,0,5}};
    const Nba97TeamHeaderEntityEffect away[5]={{9,9,4},{8,8,3},{7,7,2},{6,6,1},{5,5,0}};
    for(bool side:{false,true}) {
        const auto in=input(side);const auto before=in;Nba97TeamHeaderEffects out;
        std::memset(&out,0xa5,sizeof(out));
        CHECK(nba97_team_header_initialize(&out,&in));
        CHECK(std::memcmp(&in,&before,sizeof(in))==0);
        CHECK(out.metadata_side==static_cast<unsigned>(side) && out.alias_side==out.metadata_side);
        CHECK(out.opponent_side==static_cast<unsigned>(!side));
        CHECK(equalRef(out.word08,side ? in.table12:reference(NBA97_TEAM_REF_ENTITY,0)));
        CHECK(equalRef(out.word0c,side ? in.table24:in.table12));
        CHECK(out.direction10==(side ? 85504:-85504));
        CHECK(out.field34==7 && out.field38==7 && out.field39==5);
        CHECK(out.count66==9 && out.count68==9);
        CHECK(out.field62==118 && out.field72==14 && out.field74==332);
        CHECK(std::memcmp(out.saved_lineup,in.lineup,sizeof(in.lineup))==0);
        CHECK(std::memcmp(out.status,availability,sizeof(availability))==0);
        CHECK(std::memcmp(out.entity,side ? away:home,sizeof(home))==0);
    }
}

static void countAndInjury() {
    // Status is independent of the aliased player record or starter value.
    // Exercise every raw byte pair, including injury outside the active prefix.
    for(unsigned count=0;count<256;++count) for(unsigned injury=0;injury<256;++injury) {
        auto in=input((injury&1)!=0);in.count=static_cast<uint8_t>(count);
        in.injury_slot=static_cast<uint8_t>(injury);Nba97TeamHeaderEffects out{};
        CHECK(nba97_team_header_initialize(&out,&in));
        CHECK(out.count66==(count<12 ? count:12) && out.count68==out.count66);
        for(unsigned slot=0;slot<12;++slot) {
            const bool available=slot<count && slot!=injury;
            CHECK(out.status[slot]==(available ? 0x7fff:0xfffe));
        }
        // Count0 still writes all five entity registrations and starter copies.
        CHECK(out.entity[0].entity_id==in.side_word+4 && out.entity[4].entity_id==in.side_word);
        CHECK(std::memcmp(out.saved_lineup,in.lineup,sizeof(in.lineup))==0);
    }
}

static void ranksAndRawLineup() {
    struct ThresholdCase {uint8_t rank54,rank57,difficulty;uint16_t f62,f72,f74;};
    // Hand fixtures include low16 underflow and the exact unsigned difficulty cut.
    const ThresholdCase cases[]={
        {0,0,0,120,28,1260},{1,1,1,118,29,1228},{29,29,2,62,28,332},
        {255,255,255,65146,141,58636},{40,61,2,65534,44,65516},
        {255,255,1,65146,283,58636},{0,0,0x80,120,14,1260}
    };
    for(const auto& item:cases) {
        auto in=input();in.rank54=item.rank54;in.rank57=item.rank57;in.difficulty=item.difficulty;
        Nba97TeamHeaderEffects out{};CHECK(nba97_team_header_initialize(&out,&in));
        CHECK(out.field62==item.f62 && out.field72==item.f72 && out.field74==item.f74);
    }
    // The rank inputs are separate byte fields, with no 1..29 clamp in655B0.
    for(unsigned rank=0;rank<256;++rank) for(unsigned difficulty=0;difficulty<256;++difficulty) {
        auto in=input();in.rank54=static_cast<uint8_t>(255-rank);
        in.rank57=static_cast<uint8_t>(rank);in.difficulty=static_cast<uint8_t>(difficulty);
        Nba97TeamHeaderEffects out{};CHECK(nba97_team_header_initialize(&out,&in));
        CHECK(out.field62==static_cast<uint16_t>(120-2*static_cast<int>(rank)));
        CHECK(out.field72==(rank+28)/(difficulty>1 ? 2:1));
        CHECK(out.field74==static_cast<uint16_t>(1260-32*static_cast<int>(255-rank)));
    }
    for(unsigned value=0;value<=UINT16_MAX;++value) {
        auto in=input();for(auto& slot:in.lineup) slot=static_cast<uint16_t>(value);
        Nba97TeamHeaderEffects out{};CHECK(nba97_team_header_initialize(&out,&in));
        CHECK(std::memcmp(out.saved_lineup,in.lineup,sizeof(in.lineup))==0);
    }
}

static void references() {
    const Nba97TeamHeaderRef refs[]={reference(NBA97_TEAM_REF_UNKNOWN),reference(NBA97_TEAM_REF_NULL),
        reference(NBA97_TEAM_REF_ENTITY,0),reference(NBA97_TEAM_REF_ENTITY,9),
        reference(NBA97_TEAM_REF_OPAQUE_WORD,0),reference(NBA97_TEAM_REF_OPAQUE_WORD,1),
        reference(NBA97_TEAM_REF_OPAQUE_WORD,0x12345678),reference(NBA97_TEAM_REF_OPAQUE_WORD,UINT32_MAX)};
    for(const auto& first:refs) for(const auto& second:refs) for(bool side:{false,true}) {
        auto in=input(side);in.table12=first;in.table24=second;
        Nba97TeamHeaderEffects out{};CHECK(nba97_team_header_initialize(&out,&in));
        CHECK(equalRef(out.word08,side ? first:reference(NBA97_TEAM_REF_ENTITY,0)));
        CHECK(equalRef(out.word0c,side ? second:first));
    }
    // Equal raw zero does not erase explicit UNKNOWN/NULL/OPAQUE provenance.
    for(auto kind:{NBA97_TEAM_REF_UNKNOWN,NBA97_TEAM_REF_NULL,NBA97_TEAM_REF_OPAQUE_WORD}) {
        auto in=input();in.table12=reference(static_cast<uint8_t>(kind));
        Nba97TeamHeaderEffects out{};CHECK(nba97_team_header_initialize(&out,&in));
        CHECK(out.word0c.kind==kind && out.word0c.payload==0);
    }
}

static void refuse(const Nba97TeamHeaderInput& in) {
    Nba97TeamHeaderEffects out;std::memset(&out,0xa5,sizeof(out));
    const auto before=out;const auto input_before=in;
    CHECK(!nba97_team_header_initialize(&out,&in));
    CHECK(std::memcmp(&out,&before,sizeof(out))==0);
    CHECK(std::memcmp(&in,&input_before,sizeof(in))==0);
}
static void refusals() {
    auto in=input();Nba97TeamHeaderEffects out{};const auto before=out;
    CHECK(!nba97_team_header_initialize(nullptr,&in));
    CHECK(!nba97_team_header_initialize(&out,nullptr));
    CHECK(std::memcmp(&out,&before,sizeof(out))==0);
    CHECK(!nba97_team_header_initialize(nullptr,nullptr));
    // Ordinary caller domain is a complementary pair, not any nonzero side.
    for(unsigned word=0;word<=UINT16_MAX;++word) {
        if(word!=0) {in=input();in.side_word=static_cast<uint16_t>(word);refuse(in);}
        if(word!=5) {in=input();in.opponent_side_word=static_cast<uint16_t>(word);refuse(in);}
    }
    for(unsigned kind=4;kind<256;++kind) for(bool second:{false,true}) {
        in=input();auto& ref=second ? in.table24:in.table12;
        ref=reference(static_cast<uint8_t>(kind));refuse(in);
    }
    for(bool second:{false,true}) {
        for(auto kind:{NBA97_TEAM_REF_UNKNOWN,NBA97_TEAM_REF_NULL}) {
            in=input();auto& ref=second ? in.table24:in.table12;
            ref=reference(static_cast<uint8_t>(kind),1);refuse(in);
        }
        for(uint32_t entity:{uint32_t{10},uint32_t{65535},UINT32_MAX}) {
            in=input();auto& ref=second ? in.table24:in.table12;
            ref=reference(NBA97_TEAM_REF_ENTITY,entity);refuse(in);
        }
    }
}

static void overlapAndBounds() {
    const auto in=input(true);Nba97TeamHeaderEffects expected{};
    CHECK(nba97_team_header_initialize(&expected,&in));
    // Byte-level overlap is explicitly supported; guard every byte outside output.
    alignas(Nba97TeamHeaderEffects) std::array<uint8_t,384> storage;
    const unsigned source_at=128;
    for(unsigned destination:{64u,112u,128u,144u,160u}) {
        storage.fill(0x6d);std::memcpy(storage.data()+source_at,&in,sizeof(in));
        auto wanted=storage;std::memcpy(wanted.data()+destination,&expected,sizeof(expected));
        CHECK(nba97_team_header_initialize(reinterpret_cast<Nba97TeamHeaderEffects*>(storage.data()+destination),
              reinterpret_cast<const Nba97TeamHeaderInput*>(storage.data()+source_at)));
        CHECK(storage==wanted);
    }
    auto invalid=in;invalid.table12=reference(NBA97_TEAM_REF_ENTITY,10);
    storage.fill(0x3a);std::memcpy(storage.data()+source_at,&invalid,sizeof(in));
    const auto original=storage;
    CHECK(!nba97_team_header_initialize(reinterpret_cast<Nba97TeamHeaderEffects*>(storage.data()+source_at),
          reinterpret_cast<const Nba97TeamHeaderInput*>(storage.data()+source_at)));
    CHECK(storage==original);
}

int main() {
    completeFixtures();countAndInjury();ranksAndRawLineup();references();refusals();overlapAndBounds();
    std::printf("TEAM HEADER PASS: %u checks; complete owned effects, raw byte/halfword domains, "
                "typed references and atomic refusals; no later gameplay-stage claim\n",checks);
}
