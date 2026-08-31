#include "recovered/frontend_audio.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)

int main() {
    // Invented byte permutation, deliberately unlike a monotone pitch curve.
    // Distinct entries reveal wrong signed indexes and cents/index branching.
    std::array<uint8_t,256> table{};
    for (unsigned i=0;i<table.size();++i) table[i]=static_cast<uint8_t>(i*37u+11u);
    const auto before_table=table;
    Nba97CursorScalars result{};
    struct PitchCase {int32_t cents;uint16_t expected;};
    const PitchCase pitch_cases[]{
        {-1200,1216},{-1199,1216},{-100,1032},{-10,1796},{-9,1944},
        {-5,1944},{-4,2136},{-1,2136},{0,2136},{1,2136},{4,2136},
        {5,2432},{9,2432},{10,2728},{100,2208},{1199,3888},{1200,3888}
    };
    for (const auto& c:pitch_cases) {
        CHECK(nba97_cursor_scalars(&result,127,127,127,c.cents,table.data(),table.size()));
        CHECK(result.base_pitch==2048 && result.pitch==c.expected);
        CHECK(result.authored_volume==127 && result.effective_volume==127);
        CHECK(result.left_volume==16383 && result.right_volume==16383);
    }

    struct GainCase {uint8_t program,tone,playback,authored,effective;uint16_t volume;};
    const GainCase gain_cases[]{
        {0,127,127,0,0,0},{127,0,127,0,0,0},{127,73,0,73,0,0},
        {1,127,126,1,0,0},{2,64,127,1,1,129},{127,127,1,127,1,129},
        {41,44,108,14,11,1419},{41,66,12,21,1,129},
        {96,80,108,60,51,6579},{127,75,108,75,63,8127},
        {126,126,126,125,124,15996},{127,127,127,127,127,16383}
    };
    for (const auto& c:gain_cases) {
        CHECK(nba97_cursor_scalars(&result,c.program,c.tone,c.playback,0,table.data(),table.size()));
        CHECK(result.authored_volume==c.authored && result.effective_volume==c.effective);
        CHECK(result.left_volume==c.volume && result.right_volume==c.volume);
        CHECK(result.base_pitch==2048 && result.pitch==2136);
    }

    // For every supported gain tuple, each stored integer is the unique value
    // inside its quantization interval. This checks both truncation stages and
    // range safety without generating expectations by the implementation call.
    unsigned gain_combinations=0;
    for (unsigned p=0;p<128;++p) for (unsigned t=0;t<128;++t) for (unsigned v=0;v<128;++v) {
        CHECK(nba97_cursor_scalars(&result,static_cast<uint8_t>(p),static_cast<uint8_t>(t),
                                  static_cast<uint8_t>(v),0,table.data(),table.size()));
        const unsigned authored=result.authored_volume,effective=result.effective_volume;
        CHECK(authored<=127 && authored*127<=p*t && p*t<(authored+1)*127);
        CHECK(effective<=127 && effective*127<=authored*v && authored*v<(effective+1)*127);
        CHECK(result.left_volume==effective*129 && result.right_volume==result.left_volume);
        ++gain_combinations;
    }
    CHECK(table==before_table);

    // A borrowed table is data, not a hidden built-in curve. All possible byte
    // values must remain unsigned, at both source division scales.
    for (unsigned value=0;value<256;++value) {
        table.fill(static_cast<uint8_t>(value));
        CHECK(nba97_cursor_scalars(&result,127,127,127,5,table.data(),table.size()));
        CHECK(result.pitch==2048+value*8);
        CHECK(nba97_cursor_scalars(&result,127,127,127,-5,table.data(),table.size()));
        CHECK(result.pitch==1024+value*4);
    }
    table=before_table;

    unsigned refusals=0;
    const auto reject=[&](unsigned program,unsigned tone,unsigned playback,int32_t cents,
                          const uint8_t* curve,size_t bytes) {
        std::memset(&result,0xa5,sizeof(result));
        const auto before=result;
        CHECK(!nba97_cursor_scalars(&result,static_cast<uint8_t>(program),static_cast<uint8_t>(tone),
                                   static_cast<uint8_t>(playback),cents,curve,bytes));
        CHECK(std::memcmp(&result,&before,sizeof(result))==0);
        ++refusals;
    };
    for (unsigned invalid=128;invalid<256;++invalid) {
        reject(invalid,127,127,0,table.data(),table.size());
        reject(127,invalid,127,0,table.data(),table.size());
        reject(127,127,invalid,0,table.data(),table.size());
    }
    for (int32_t cents:{std::numeric_limits<int32_t>::min(),-1201,1201,std::numeric_limits<int32_t>::max()})
        reject(127,127,127,cents,table.data(),table.size());
    for (size_t bytes:{size_t(0),size_t(1),size_t(255),size_t(257),size_t(512),std::numeric_limits<size_t>::max()})
        reject(127,127,127,0,table.data(),bytes);
    reject(127,127,127,0,nullptr,table.size());
    CHECK(!nba97_cursor_scalars(nullptr,127,127,127,0,table.data(),table.size()));
    ++refusals;
    CHECK(table==before_table);
    std::printf("CURSOR SCALARS PASS: %u gain tuples, 17 signed-index vectors, 12 staged-gain vectors, "
                "512 unsigned-table vectors, %u argument refusals; no RNG/device/PCM claims\n",
                gain_combinations,refusals);
}
