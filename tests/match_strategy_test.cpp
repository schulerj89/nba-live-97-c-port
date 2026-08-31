#include "recovered/match_strategy.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)

static_assert(sizeof(Nba97MatchStrategy)==14,"owned resident fields only");
static_assert(sizeof(Nba97MatchTeamStrategy)==7,"owned header fields only");
static const Nba97MatchStrategy source={{{0,2,0x7f,0x80,0xfe,0xff,0x35},
                                       {0xff,0xfe,0x80,0x7f,2,0,0xca}}};
static const Nba97MatchTeamStrategy previous={{10,20,30,40,50,60,70}};
static const Nba97MatchTeamStrategy cpu={{1,1,30,40,50,60,70}};
static const Nba97MatchTeamStrategy launched={{1,20,30,40,50,60,70}};

static void coldAndLifetime() {
    const Nba97MatchStrategy cold={{{1,1,0,7,5,0,0},{1,1,0,7,5,0,0}}};
    auto resident=source;
    CHECK(nba97_match_strategy_cold(&resident,0));
    CHECK(std::memcmp(&resident,&cold,sizeof(resident))==0);
    // High bits of the full32-bit guard must not be truncated to byte/halfword.
    for(unsigned bit=0;bit<32;++bit) {
        resident=source;
        CHECK(nba97_match_strategy_cold(&resident,uint32_t{1}<<bit));
        CHECK(std::memcmp(&resident,&source,sizeof(resident))==0);
    }
    const Nba97MatchTeamStrategy teams[2]={{{9,8,7,6,5,4,3}},{{2,1,0,255,254,253,252}}};
    CHECK(nba97_match_strategy_writeback(&resident,teams,0));
    const auto warmed=resident;
    CHECK(nba97_match_strategy_cold(&resident,UINT32_MAX));
    CHECK(std::memcmp(&resident,&warmed,sizeof(resident))==0);
    auto selected=previous;
    CHECK(nba97_match_strategy_apply(&selected,&resident,5,0,1,255));
    CHECK(std::memcmp(&selected,&teams[1],sizeof(selected))==0);
}

static void rawHalfwordsAndBytes() {
    // Independent fixed expectations distinguish nonzero side from side==5,
    // unsigned high-bit values, and preserving the correct fallback fields.
    for(unsigned raw=0;raw<=UINT16_MAX;++raw) {
        const auto word=static_cast<uint16_t>(raw);
        auto selected=previous;
        CHECK(nba97_match_strategy_apply(&selected,&source,word,0,1,255));
        CHECK(std::memcmp(selected.fields,source.side[raw!=0],7)==0);
        selected=previous;
        CHECK(nba97_match_strategy_apply(&selected,&source,5,word,1,255));
        CHECK(std::memcmp(&selected,raw ? launched.fields:source.side[1],7)==0);
        selected=previous;
        CHECK(nba97_match_strategy_apply(&selected,&source,5,0,word,255));
        CHECK(std::memcmp(&selected,raw ? source.side[1]:cpu.fields,7)==0);
        const Nba97MatchTeamStrategy teams[2]={previous,launched};
        auto resident=source;
        CHECK(nba97_match_strategy_writeback(&resident,teams,word));
        if(raw) CHECK(std::memcmp(&resident,&source,sizeof(resident))==0);
        else {
            CHECK(std::memcmp(resident.side[0],previous.fields,7)==0);
            CHECK(std::memcmp(resident.side[1],launched.fields,7)==0);
        }
    }
    for(unsigned byte=0;byte<256;++byte) {
        auto resident=source;
        Nba97MatchTeamStrategy teams[2];
        std::memset(&teams[0],static_cast<int>(byte),7);
        std::memset(&teams[1],static_cast<int>(255-byte),7);
        CHECK(nba97_match_strategy_writeback(&resident,teams,0));
        CHECK(std::memcmp(resident.side[0],teams[0].fields,7)==0);
        CHECK(std::memcmp(resident.side[1],teams[1].fields,7)==0);
        auto selected=previous;
        CHECK(nba97_match_strategy_apply(&selected,&resident,1,0,0x8000,12));
        CHECK(std::memcmp(selected.fields,teams[1].fields,7)==0);
    }
    const uint16_t words[]={0,1,5,0x100,0x8000,0xffff};
    for(auto side:words) for(auto launch:words) for(auto humans:words) {
        auto selected=previous;
        CHECK(nba97_match_strategy_apply(&selected,&source,side,launch,humans,254));
        const uint8_t* expected=launch ? launched.fields : !humans ? cpu.fields : source.side[side!=0];
        CHECK(std::memcmp(selected.fields,expected,7)==0);
    }
}

static void overlaps() {
    // The byte-oriented C boundary permits overlapping storage. Check both
    // directions and every overlap length, including a side copy onto itself.
    for(int shift=-6;shift<14;++shift) for(unsigned side=0;side<2;++side) {
        std::array<uint8_t,40> bytes;
        for(unsigned i=0;i<bytes.size();++i) bytes[i]=static_cast<uint8_t>(i+100);
        std::memcpy(bytes.data()+8,&source,sizeof(source));
        const auto before=bytes;
        auto expected=bytes;
        const auto destination=static_cast<unsigned>(8+shift);
        std::memcpy(expected.data()+destination,before.data()+8+side*7,7);
        auto* team=reinterpret_cast<Nba97MatchTeamStrategy*>(bytes.data()+destination);
        const auto* resident=reinterpret_cast<const Nba97MatchStrategy*>(bytes.data()+8);
        CHECK(nba97_match_strategy_apply(team,resident,static_cast<uint16_t>(side),0,1,255));
        CHECK(bytes==expected);
    }
    for(int shift=-13;shift<14;++shift) {
        std::array<uint8_t,64> bytes;
        for(unsigned i=0;i<bytes.size();++i) bytes[i]=static_cast<uint8_t>(i*3);
        const auto before=bytes;
        auto expected=bytes;
        const auto destination=static_cast<unsigned>(20+shift);
        std::memcpy(expected.data()+destination,before.data()+20,14);
        auto* resident=reinterpret_cast<Nba97MatchStrategy*>(bytes.data()+destination);
        const auto* teams=reinterpret_cast<const Nba97MatchTeamStrategy*>(bytes.data()+20);
        CHECK(nba97_match_strategy_writeback(resident,teams,0));
        CHECK(bytes==expected);
        const auto warmed=bytes;
        CHECK(nba97_match_strategy_writeback(resident,teams,1));
        CHECK(bytes==warmed);
    }
}

static void refusals() {
    CHECK(!nba97_match_strategy_cold(nullptr,0));
    CHECK(!nba97_match_strategy_cold(nullptr,UINT32_MAX));
    const Nba97MatchTeamStrategy teams[2]={previous,cpu};
    const uint16_t launches[]={0,1,UINT16_MAX};
    for(uint16_t launch:launches) {
        auto resident=source;
        auto selected=previous;
        CHECK(!nba97_match_strategy_apply(nullptr,&resident,0,launch,1,255));
        CHECK(std::memcmp(&resident,&source,sizeof(resident))==0);
        CHECK(!nba97_match_strategy_apply(&selected,nullptr,0,launch,1,255));
        CHECK(std::memcmp(&selected,&previous,sizeof(selected))==0);
        CHECK(!nba97_match_strategy_writeback(nullptr,teams,launch));
        CHECK(!nba97_match_strategy_writeback(&resident,nullptr,launch));
        CHECK(std::memcmp(&resident,&source,sizeof(resident))==0);
        for(uint8_t injury=0;injury<12;++injury) {
            CHECK(!nba97_match_strategy_apply(&selected,&resident,5,launch,1,injury));
            CHECK(std::memcmp(&selected,&previous,sizeof(selected))==0);
            CHECK(std::memcmp(&resident,&source,sizeof(resident))==0);
        }
    }
}

int main() {
    coldAndLifetime();rawHalfwordsAndBytes();overlaps();refusals();
    std::printf("MATCH STRATEGY PASS: %u checks; cold/warm lifetime, all raw halfwords, "
                "unsigned bytes, fallback preservation, overlap and atomic refusals; no gameplay claim\n",checks);
}
