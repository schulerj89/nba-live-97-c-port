#include "recovered/game_lineup_recovery.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static unsigned checks;
static void check(bool value) { ++checks; if(!value) {std::fprintf(stderr,"lineup recovery check%u failed\n",checks);std::exit(1);} }
static Nba97GameLineupRecoveryState base() {
    Nba97GameLineupRecoveryState state{};
    for(unsigned side=0;side<2;++side) {
        auto& t=state.team[side]; t.human_count=1;
        for(unsigned i=0;i<12;++i) t.lineup[i]=t.inverse[i]=std::uint16_t(i);
        for(unsigned i=0;i<5;++i) t.preferred[i]=std::uint16_t(i);
    }
    state.marker=0x1234;state.substitution_lock=0x5678;
    return state;
}
struct Calls {std::vector<std::array<std::int32_t,5>> values;bool mutate=false;bool fail=false;};
static int substitute(void* context,Nba97GameLineupRecoveryState* s,unsigned side,
                      std::int32_t active,std::int32_t bench,std::int32_t reason,std::uint32_t first) {
    auto& c=*static_cast<Calls*>(context);
    check(s->substitution_lock==1);
    c.values.push_back({std::int32_t(side),active,bench,reason,std::int32_t(first)});
    if(c.mutate && side==0 && active==0) {
        // Deliberately alter fields that the source must read after this call.
        s->team[0].preferred[1]=7;s->team[0].inverse[7]=9;s->status[7]=0x7fff;
        s->team[1].human_count=0;s->team[1].inverse[0]=8;s->status[12]=0x7fff;
    }
    return c.fail ? 0:1;
}
int main() {
    {
        auto s=base();
        s.status[0]=0xffff;s.status[1]=0xfffe;s.status[2]=0x8000;
        s.status[3]=0x7fff;s.status[4]=0x7ff0;s.status[5]=10;
        check(nba97_game_lineup_recover(&s,1,nullptr,nullptr)==NBA97_RECOVERY_OK);
        check(s.status[0]==0xffff && s.status[1]==0xfffe && s.status[2]==0x8000);
        check(s.status[3]==0x7fff && s.status[4]==0x7fff && s.status[5]==33);
        check(s.marker==0x1234 && s.substitution_lock==0x5678);
        s=base();s.status[0]=100;
        check(nba97_game_lineup_recover(&s,-1,nullptr,nullptr)==NBA97_RECOVERY_OK);
        check(s.status[0]==77 && s.status[1]==0x7fff);
        s=base();check(nba97_game_lineup_recover(&s,65536,nullptr,nullptr)==NBA97_RECOVERY_OK);
        check(s.status[0]==0 && s.marker==0x11); // Wide overflow is NOT saturation.
    }
    {
        auto s=base();s.status[0]=0xffff;
        check(nba97_game_lineup_recover(&s,120,nullptr,nullptr)==NBA97_RECOVERY_OK);
        check(s.status[0]==0xffff && s.team[0].recovery_count==0);
        s=base();s.status[1]=0xffff;s.team[0].recovery_count=0xffff;
        check(nba97_game_lineup_recover(&s,120,nullptr,nullptr)==NBA97_RECOVERY_OK);
        check(s.status[1]==0x7332 && s.team[0].lineup[1]==1 && s.team[0].recovery_count==0);
        s=base();s.status[1]=0xfffe;s.status[2]=0xffff;
        check(nba97_game_lineup_recover(&s,120,nullptr,nullptr)==NBA97_RECOVERY_OK);
        check(s.team[0].lineup[1]==2 && s.team[0].lineup[2]==1 && s.status[2]==0x7332);
        s=base();s.status[0]=s.status[1]=0xffff;
        check(nba97_game_lineup_recover(&s,120,nullptr,nullptr)==NBA97_RECOVERY_OUTSIDE_STORAGE);
        check(s.status[0]==0xffff && s.status[1]==0xffff && s.marker==0x1234);
        s=base();s.status[12]=0xffff;s.team[1].lineup[0]=0xffff;s.team[1].lineup[1]=0;
        // Signed away lineup -1 reads home status11 after adding12.
        check(nba97_game_lineup_recover(&s,120,nullptr,nullptr)==NBA97_RECOVERY_OK);
        check(s.status[12]==0x7332);
    }
    {
        auto s=base();Calls c;
        s.team[0].inverse[0]=5;s.team[0].inverse[1]=6;
        s.status[0]=0x7331;s.status[1]=0x7332;
        check(nba97_game_lineup_auto_substitute(&s,0,0,substitute,&c)==NBA97_RECOVERY_OK);
        check(c.values==std::vector<std::array<std::int32_t,5>>{{0,1,6,0,1}} && s.substitution_lock==0);
        s=base();c={};c.mutate=true;s.team[0].human_count=0;s.team[0].inverse[0]=5;s.status[0]=0x7fff;
        check(nba97_game_lineup_recover(&s,120,substitute,&c)==NBA97_RECOVERY_OK);
        check(c.values==std::vector<std::array<std::int32_t,5>>{{0,0,5,0,1},{0,1,9,0,0},{1,0,8,0,1}});
        check(s.marker==0x11 && s.substitution_lock==0);
        s=base();s.team[0].inverse[0]=32767;s.status[0]=0x7fff;c={};
        check(nba97_game_lineup_auto_substitute(&s,0,0,substitute,&c)==NBA97_RECOVERY_OK);
        check(c.values[0][2]==32767); // No guessed inverse-entry clamp.
    }
    {
        auto s=base();auto before=s;
        check(nba97_game_lineup_auto_substitute(&s,2,0,nullptr,nullptr)==NBA97_RECOVERY_ARGUMENT);
        check(std::memcmp(&s,&before,sizeof(s))==0);
        check(nba97_game_lineup_recover(nullptr,120,nullptr,nullptr)==NBA97_RECOVERY_ARGUMENT);
        s.team[0].preferred[0]=0xffff;
        check(nba97_game_lineup_auto_substitute(&s,0,0,nullptr,nullptr)==NBA97_RECOVERY_OUTSIDE_STORAGE);
        check(s.substitution_lock==1);
        s=base();s.team[0].inverse[0]=5;s.status[0]=0x7fff;
        check(nba97_game_lineup_auto_substitute(&s,0,0,nullptr,nullptr)==NBA97_RECOVERY_CALLBACK_REQUIRED);
        check(s.substitution_lock==1);
        Calls c;c.fail=true;
        check(nba97_game_lineup_auto_substitute(&s,0,0,substitute,&c)==NBA97_RECOVERY_CALLBACK_FAILED);
        check(c.values.size()==1 && s.substitution_lock==1);
    }
    std::printf("GAME LINEUP RECOVERY PASS: %u checks; source quirks, live callback reads and explicit unfinished boundaries\n",checks);
}
