#include "recovered/team_select_text.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)

using Seed=std::array<uint8_t,NBA97_TEAM_TEXT_SEED_BYTES>;
static Seed pattern() {
    Seed seed{};
    for(unsigned i=0;i<200;++i) {
        seed[i*3]=static_cast<uint8_t>(i);
        seed[i*3+1]=static_cast<uint8_t>(i*3+11);
        seed[i*3+2]=static_cast<uint8_t>(i*7+99);
    }
    return seed;
}
static unsigned occupied(const Nba97TeamTextState& s) {
    unsigned n=0;for(const auto& slot:s.slots) n+=slot.lifetime>=0;return n;
}
static void frames(Nba97TeamTextState& s,unsigned n) {
    while(n--) CHECK(nba97_team_text_present(&s));
}

static void entryAndReplacement() {
    const auto seed=pattern();
    for(unsigned hint:{0u,17u,197u}) for(unsigned focus=0;focus<12;++focus) {
        Nba97TeamTextState s{};
        CHECK(nba97_team_text_seed(&s,seed.data(),seed.size(),hint));
        CHECK(nba97_team_text_open(&s,focus));
        CHECK(s.anchored && s.hint==(hint+28)%200 && occupied(s)==29);
        CHECK(s.label[0]==NBA97_TEAM_TEXT_NONE && s.label[6]==NBA97_TEAM_TEXT_NONE);
        CHECK(s.value[0]==(hint+26)%200 && s.value[6]==(hint+27)%200);
        CHECK(s.slots[hint].lifetime==0 && s.slots[(hint+11)%200].lifetime==0);
        CHECK(s.group_head[focus]==s.value[focus]);
        for(unsigned a=0;a<4;++a) {
            const auto index=(hint+22+a)%200;
            const auto& n=s.slots[index];
            CHECK(s.arrow[a]==index && n.group==120+focus/6 && n.layer==0);
            CHECK(!std::memcmp(n.tint.start,seed.data()+index*3,3));
            CHECK(n.known.start==7 && n.known.rgb==7 && n.tint.flags==0);
            CHECK(n.tint.rgb[0]==128 && n.tint.rgb[1]==128 && n.tint.rgb[2]==128);
        }
        CHECK(s.slots[s.header].group==190 && s.slots[s.header].layer==0);
        Nba97TeamTextView view{};CHECK(nba97_team_text_view(&s,&view));
        CHECK(view.header.active && !view.label[0].active && view.value[0].active);
        frames(s,2);CHECK(occupied(s)==27);

        const auto before=s;
        CHECK(nba97_team_text_direction(&s,focus/6,focus));
        CHECK(occupied(s)==34); // Six old values plus the intermediate selected value retire.
        for(unsigned i=focus/6*6;i<focus/6*6+6;++i)
            CHECK(s.slots[before.value[i]].lifetime==0);
        CHECK(s.slots[(before.hint+1+focus%6)%200].lifetime==0);
        CHECK(s.value[focus]==(before.hint+7)%200);
        CHECK(!std::memcmp(s.arrow,before.arrow,sizeof(s.arrow)));
        CHECK(!std::memcmp(s.label,before.label,sizeof(s.label)));
        frames(s,1);CHECK(occupied(s)==27);

        const unsigned old_hint=s.hint;
        CHECK(nba97_team_text_refresh(&s,focus/6));
        CHECK(s.hint==(old_hint+6)%200 && occupied(s)==33); // Random's six, no selected extra.
        frames(s,1);CHECK(occupied(s)==27);
    }
}

static void helpExitAndReuse() {
    const auto seed=pattern();Nba97TeamTextState s{};
    CHECK(nba97_team_text_seed(&s,seed.data(),seed.size(),0));
    CHECK(nba97_team_text_open(&s,0));frames(s,3);
    const auto arrows=s;const unsigned before=s.hint;
    CHECK(nba97_team_text_help_create(&s));CHECK(s.hint==before+6);
    for(unsigned i=0;i<6;++i) {
        const auto& n=s.slots[s.help[i]];
        CHECK(n.group==200+i && n.layer==2 && n.lifetime==32767);
    }
    frames(s,2);CHECK(nba97_team_text_help_retire(&s));
    for(unsigned i=0;i<6;++i) {
        const auto& n=s.slots[s.help[i]];
        CHECK(n.lifetime==0 && n.layer==2 && !n.group_linked);
        CHECK(s.group_head[200+i]==NBA97_TEAM_TEXT_NONE);
    }
    CHECK(s.layer_head[2]==s.help[0] && s.layer_tail[2]==s.help[5]);
    frames(s,1);for(auto index:s.help) CHECK(s.slots[index].lifetime==-1);
    CHECK(!std::memcmp(s.arrow,arrows.arrow,sizeof(s.arrow)));
    CHECK(s.hint==before+6); // Retirement is not an allocator-hint reset.
    const auto before_exit=s;
    CHECK(nba97_team_text_retire_all(&s));
    // Layer-wide exit retirement keeps group heads until the final pass;
    // Help's group retirement above has a different immediate effect.
    CHECK(!std::memcmp(s.group_head,before_exit.group_head,sizeof(s.group_head)));
    for(const auto& n:s.slots) if(n.lifetime==0) CHECK(n.group_linked);
    Nba97TeamTextView view{};CHECK(nba97_team_text_view(&s,&view));
    CHECK(!view.header.active && !view.value[0].active && !view.arrow[0].active);
    const auto retiring=s;frames(s,1);CHECK(occupied(s)==0);
    for(unsigned i=0;i<200;++i)
        CHECK(!std::memcmp(&s.slots[i].tint,&retiring.slots[i].tint,sizeof(Nba97ReorderTint)));
    const unsigned retained_hint=s.hint;
    CHECK(nba97_team_text_open(&s,6));
    CHECK(s.arrow[0]==(retained_hint+22)%200 && s.slots[s.arrow[0]].group==121);
    CHECK(!std::memcmp(s.slots[s.arrow[0]].tint.start,seed.data()+s.arrow[0]*3,3));
}

static void unknownnessAndKernelReuse() {
    Nba97TeamTextState unknown{};nba97_team_text_unknown(&unknown);
    CHECK(nba97_team_text_open(&unknown,0));
    CHECK(!unknown.anchored && unknown.slots[unknown.arrow[0]].known.start==0);
    CHECK(unknown.slots[unknown.arrow[0]].known.rgb==7);
    CHECK(nba97_team_text_flash(&unknown,0));
    CHECK(unknown.slots[unknown.arrow[0]].known.rgb==7); // Scheduling did not draw.
    std::array<Nba97TeamTextNode,22> phases{};phases[0]=unknown.slots[unknown.arrow[0]];
    for(unsigned f=1;f<=21;++f) {
        frames(unknown,1);phases[f]=unknown.slots[unknown.arrow[0]];
        CHECK(phases[f].known.rgb==(f<4 ? 0:7));
        CHECK(phases[f].known.start==(f<5 ? 0:f<16 ? 6:7));
        CHECK(phases[f].known.alternate==0);
    }
    // Every claimed channel must agree for every possible inherited byte.
    // These witnesses do not derive expected values from the mask algorithm.
    for(unsigned inherited=0;inherited<256;++inherited) {
        Seed seed{};seed.fill(static_cast<uint8_t>(inherited));Nba97TeamTextState known{};
        CHECK(nba97_team_text_seed(&known,seed.data(),seed.size(),0));
        CHECK(nba97_team_text_open(&known,0));CHECK(nba97_team_text_flash(&known,0));
        for(unsigned f=0;f<=21;++f) {
            if(f) frames(known,1);
            const auto& actual=known.slots[known.arrow[0]];
            for(unsigned c=0;c<3;++c) {
                if(phases[f].known.rgb&(1u<<c)) CHECK(phases[f].tint.rgb[c]==actual.tint.rgb[c]);
                if(phases[f].known.start&(1u<<c)) CHECK(phases[f].tint.start[c]==actual.tint.start[c]);
            }
        }
    }
    Seed zero{};Nba97TeamTextState observed{};
    CHECK(nba97_team_text_seed(&observed,zero.data(),zero.size(),0));
    CHECK(nba97_team_text_open(&observed,0));CHECK(nba97_team_text_flash(&observed,0));frames(observed,1);
    const auto& first=observed.slots[observed.arrow[0]].tint;
    CHECK(first.rgb[0]==30 && first.rgb[1]==25 && first.rgb[2]==0);
    nba97_team_text_invalidate(&observed);
    CHECK(!observed.anchored && !observed.opened && occupied(observed)==0);
    for(const auto& n:observed.slots) CHECK(n.known.start==0);
    CHECK(nba97_team_text_open(&observed,0));
    CHECK(observed.slots[observed.arrow[0]].known.start==0);
}

static void transactionalGuards() {
    Seed seed{};Nba97TeamTextState s{};
    CHECK(!nba97_team_text_open(&s,0));
    CHECK(nba97_team_text_seed(&s,seed.data(),seed.size(),0));
    auto saved=s;
    CHECK(!nba97_team_text_seed(&s,nullptr,600,0));CHECK(!std::memcmp(&saved,&s,sizeof(s)));
    CHECK(!nba97_team_text_seed(&s,seed.data(),599,0));CHECK(!std::memcmp(&saved,&s,sizeof(s)));
    CHECK(!nba97_team_text_seed(&s,seed.data(),600,200));CHECK(!std::memcmp(&saved,&s,sizeof(s)));
    CHECK(!nba97_team_text_open(&s,12));CHECK(!std::memcmp(&saved,&s,sizeof(s)));
    CHECK(nba97_team_text_open(&s,0));frames(s,1);
    while(200-occupied(s)>=6) CHECK(nba97_team_text_refresh(&s,0));
    saved=s;
    CHECK(!nba97_team_text_refresh(&s,0));CHECK(!std::memcmp(&saved,&s,sizeof(s)));
    CHECK(!nba97_team_text_direction(&s,0,0));CHECK(!std::memcmp(&saved,&s,sizeof(s)));
    CHECK(!nba97_team_text_help_create(&s));CHECK(!std::memcmp(&saved,&s,sizeof(s)));
    CHECK(!nba97_team_text_direction(&s,1,0));CHECK(!std::memcmp(&saved,&s,sizeof(s)));
    CHECK(!nba97_team_text_flash(&s,4));CHECK(!std::memcmp(&saved,&s,sizeof(s)));
    Nba97TeamTextView view{};
    CHECK(!nba97_team_text_view(nullptr,&view));CHECK(!nba97_team_text_view(&s,nullptr));
    frames(s,1);CHECK(occupied(s)==27);CHECK(nba97_team_text_refresh(&s,0));
}

int main() {
    entryAndReplacement();helpExitAndReuse();unknownnessAndKernelReuse();transactionalGuards();
    std::puts("Team Select text history: explicit seeds, retained allocation, seven/six replacements, Help, knownness and rollback passed");
}
