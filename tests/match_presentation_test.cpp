#include "recovered/match_setup.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)

int main() {
    using Seed=std::array<uint32_t,6>;
    struct Case {Seed before,after;uint8_t value;uint64_t draws;};
    // Invented six-word seeds and independently obtained semantic expectations.
    // They distinguish zero rejection, every result, updated-neighbor sums,
    // and the complete multiword increment carry. No original seed/table bytes.
    const Case cases[]{
        {{0,0,0,0,0,0},{84,56,35,20,10,5},0x40,5},
        {{1,2,3,4,5,6},{92,71,51,33,18,8},0x40,2},
        {{29,0,0,0,0,0},{36,6,5,4,3,3},0x20,3},
        {{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX},
         {UINT32_MAX-1,UINT32_MAX-1,UINT32_MAX-1,UINT32_MAX-1,UINT32_MAX,0},0x60,1},
        {{0,0,0,0,0,UINT32_MAX},{84,56,35,20,10,5},0x40,6},
        {{0x20,0,0,0,0,0},{0x20,0,0,0,0,1},0x20,1},
        {{0x40,0,0,0,0,0},{0x40,0,0,0,0,1},0x40,1},
        {{0x60,0,0,0,0,0},{0x60,0,0,0,0,1},0x60,1},
    };
    Nba97MatchPresentation result{};
    for(const auto& test:cases) {
        auto rng=test.before;
        CHECK(nba97_match_presentation(&result,rng.data(),0,0));
        CHECK(result.value==test.value && !result.from_schedule);
        CHECK(result.rng_draws==test.draws && result.rejected_draws==test.draws-1);
        CHECK(rng==test.after);
        // Season with no presentation bits takes precisely the same fallback.
        rng=test.before;
        CHECK(nba97_match_presentation(&result,rng.data(),1,0x9f));
        CHECK(result.value==test.value && !result.from_schedule);
        CHECK(result.rng_draws==test.draws && result.rejected_draws==test.draws-1);
        CHECK(rng==test.after);
    }

    const Seed initial{{1,2,3,4,5,6}};
    const Seed after_two{{92,71,51,33,18,8}};
    for(unsigned flags=0;flags<256;++flags) {
        auto rng=initial;
        CHECK(nba97_match_presentation(&result,rng.data(),1,static_cast<uint8_t>(flags)));
        const auto scheduled=static_cast<uint8_t>(flags&0x60);
        if(scheduled) {
            CHECK(result.value==scheduled && result.from_schedule==1);
            CHECK(result.rng_draws==0 && result.rejected_draws==0);
            CHECK(rng==initial);
        } else {
            CHECK(result.value==0x40 && !result.from_schedule);
            CHECK(result.rng_draws==2 && result.rejected_draws==1);
            CHECK(rng==after_two);
        }
        rng=initial;
        CHECK(nba97_match_presentation(&result,rng.data(),0,static_cast<uint8_t>(flags)));
        CHECK(result.value==0x40 && !result.from_schedule);
        CHECK(result.rng_draws==2 && result.rejected_draws==1 && rng==after_two);
    }

    // The caller supplies an unsigned16 mode, not a boolean or a truncated byte.
    // Exactly1 may use the schedule; all other bit patterns ignore its0x60.
    for(unsigned mode=0;mode<=UINT16_MAX;++mode) {
        auto rng=initial;
        CHECK(nba97_match_presentation(&result,rng.data(),static_cast<uint16_t>(mode),0x60));
        if(mode==1) {
            CHECK(result.value==0x60 && result.from_schedule==1);
            CHECK(result.rng_draws==0 && result.rejected_draws==0 && rng==initial);
        } else {
            CHECK(result.value==0x40 && !result.from_schedule);
            CHECK(result.rng_draws==2 && result.rejected_draws==1 && rng==after_two);
        }
    }

    // Missing native objects refuse before consuming the borrowed stream or
    // overwriting the prior receipt. Successful publication belongs to C++.
    auto rng=initial;
    CHECK(!nba97_match_presentation(nullptr,rng.data(),0,0));
    CHECK(rng==initial);
    std::memset(&result,0xa5,sizeof(result));
    const auto before=result;
    CHECK(!nba97_match_presentation(&result,nullptr,0,0));
    CHECK(std::memcmp(&result,&before,sizeof(result))==0);
    CHECK(!nba97_match_presentation(&result,nullptr,1,0x60));
    CHECK(std::memcmp(&result,&before,sizeof(result))==0);
    CHECK(!nba97_match_presentation(nullptr,nullptr,1,0x60));
    std::printf("MATCH PRESENTATION PASS: 16 seeded branch vectors, 512 flag cases, "
                "65536 unsigned-mode cases, 4 null refusals; no startup/gameplay claim\n");
}
