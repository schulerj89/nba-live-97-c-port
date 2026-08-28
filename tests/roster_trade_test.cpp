#include "recovered/roster_trade.h"
#include <cstdint>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const char* why) {
    if (!ok) throw std::runtime_error(why);
}
// Independent expected values for normalizer + collision branch; not a MIPS
// execution oracle. Signed context is significant, including negative bytes.
int expectedTeam(int input, int mode, int context) {
    if (input == 29) return mode == 2 ? context : 3;
    return input;
}
void verify(int left, int right, int mode, int context) {
    auto actual = nba97_trade_prepare_teams(static_cast<int16_t>(left),
        static_cast<int16_t>(right), static_cast<int16_t>(mode), static_cast<int8_t>(context));
    const int expectedRight = expectedTeam(right, mode, context);
    int expectedLeft = expectedTeam(left, mode, context);
    if (expectedLeft == expectedRight) expectedLeft = expectedRight == 0 ? 3 : 0;
    check(actual.left == expectedLeft && actual.right == expectedRight,
          "team preparation differs from reviewed entry rule");
    check(actual.left != actual.right, "entry must resolve the normalized collision");
}
struct EntryFixture {
    std::array<uint16_t,535> table{};
    std::array<uint8_t,435> positions{},injuries{};
    std::array<uint8_t,25> preference{};
    Nba97TradeScreen screen{};
    EntryFixture() {
        table.fill(UINT16_MAX);
        for(int t=0;t<29;++t)for(int r=0;r<10;++r)table[t*15+r]=uint16_t(t*15+r);
        for(unsigned i=0;i<positions.size();++i)positions[i]=uint8_t(i%5);
        for(unsigned i=0;i<preference.size();++i)preference[i]=uint8_t(i%5);
        begin();
    }
    void begin() {check(nba97_trade_begin(&screen,table.data(),0,1,0,nullptr,nullptr,nullptr)!=0,"constructor failed");}
    Nba97TradeEvent key(uint16_t raw) {
        const Nba97TradeData data{positions.data(),injuries.data(),preference.data(),positions.size(),1};
        return nba97_trade_input(&screen,raw,&data);
    }
    void release() {nba97_trade_frame(&screen,0);}
};
}

int main() {
    try {
        auto teams = nba97_trade_prepare_teams(3, 5, 0, 0);
        check(teams.left == 3 && teams.right == 5, "distinct teams changed");
        std::cout << "TRADE PASS distinct_teams\n";

        teams = nba97_trade_prepare_teams(0, 0, 0, 0);
        check(teams.left == 3 && teams.right == 0, "zero collision must change LEFT to 3");
        for (int team = 1; team < 29; ++team) {
            teams = nba97_trade_prepare_teams(team, team, 0, 0);
            check(teams.left == 0 && teams.right == team, "nonzero collision must change LEFT to 0");
        }
        std::cout << "TRADE PASS collision_changes_left_only\n";

        for (int mode : {-32768, -1, 0, 1, 2, 3, 32767})
            for (int context = -128; context <= 127; ++context)
                for (int left = 0; left <= 29; ++left)
                    for (int right = 0; right <= 29; ++right)
                        verify(left, right, mode, context);
        std::cout << "TRADE PASS team_pairs_modes_signed_contexts\n";

        for (int raw = -32768; raw <= 32767; ++raw) {
            verify(raw, 5, 0, 0);
            verify(5, raw, 0, 0);
            verify(raw, raw, 0, 0);
            verify(raw, 29, 2, -1);
            verify(29, raw, 2, -128);
        }
        std::cout << "TRADE PASS signed_halfword_boundaries_no_clamping\n";

        teams = nba97_trade_prepare_teams(29, 29, 0, 0);
        check(teams.left == 0 && teams.right == 3, "normalize both before collision");
        teams = nba97_trade_prepare_teams(29, 0, 2, 0);
        check(teams.left == 3 && teams.right == 0, "special zero context collision");
        teams = nba97_trade_prepare_teams(29, 29, 2, -128);
        check(teams.left == 0 && teams.right == -128, "signed context must not be clamped");
        std::cout << "TRADE PASS sentinel_normalization_precedes_collision\n";
        const auto before = teams;
        for (int repeat = 0; repeat < 20; ++repeat) {
            teams = nba97_trade_prepare_teams(29, 29, 2, -128);
            check(teams.left == before.left && teams.right == before.right, "hidden persistent state");
        }
        std::cout << "TRADE PASS deterministic_reentry\n";
        {
            EntryFixture f;const uint8_t cursor[]{6,8},top[]{1,3};
            check(nba97_trade_begin(&f.screen,f.table.data(),29,29,0,nullptr,cursor,top)!=0,"normalized constructor");
            check(f.screen.team[0]==0&&f.screen.team[1]==3,"normalized teams not handed to constructor");
            check(f.screen.list_kind[0]==1&&f.screen.list_kind[1]==1,"Trade must construct two kind1 lists");
            check(f.screen.input_callback[0]&&f.screen.input_callback[1]&&
                f.screen.input_callback[0]!=f.screen.input_callback[1],"missing/distinct callback roles");
            check(f.screen.cursor[0]==6&&f.screen.cursor[1]==8&&f.screen.top[0]==1&&f.screen.top[1]==3,"saved viewport lost");
            check(!std::memcmp(f.screen.snapshot,f.table.data(),sizeof(f.screen.snapshot))&&
                !std::memcmp(f.screen.working,f.table.data(),sizeof(f.screen.working))&&
                nba97_trade_result(&f.screen)==0,"constructor snapshot/result");
        }
        std::cout << "TRADE PASS constructor_handoff_kinds_and_callbacks\n";
        {
            EntryFixture f;
            check(f.key(0x800)==NBA97_TRADE_PICK&&!nba97_trade_dirty(&f.screen),"first callback must select, not mutate");
            f.key(2);
            check(f.key(0x800)==NBA97_TRADE_SWAPPED&&f.screen.working[0]==16&&f.screen.working[16]==0,
                "second callback must validate/mutate chosen pair");
            check(f.screen.phase==NBA97_TRADE_FIRST&&nba97_trade_result(&f.screen)==0,"swap is not a screen exit");
            f.begin();f.screen.mode=1;f.injuries[0]=1;
            check(f.key(0x800)==NBA97_TRADE_NOTICE&&f.screen.phase==NBA97_TRADE_FIRST&&!nba97_trade_dirty(&f.screen),"first injury gate before phase change");
            f.injuries[0]=0;f.begin();
            for(int i=0;i<14;++i)f.key(2);
            check(f.key(0x800)==NBA97_TRADE_PICK,"kind1 first callback must permit empty receiver");
            check(f.key(0x800)==NBA97_TRADE_SWAPPED&&f.screen.counts[0]==11&&f.screen.counts[1]==9,
                "kind1 second callback must permit transfer");
        }
        std::cout << "TRADE PASS selection_callback_roles\n";
        {
            EntryFixture f;int16_t teams[]{2,3};uint8_t slots[]{1,2};
            check(f.key(0x10)==NBA97_TRADE_VIEW&&nba97_trade_result(&f.screen)==2&&f.screen.child==0x24,"View route");
            check(nba97_trade_return_child(&f.screen,0x100,teams,slots,0)&&nba97_trade_result(&f.screen)==0,"View cancel resumes parent");
            f.release();f.key(0x800);
            check(f.key(0x40)==NBA97_TRADE_COMPARE&&nba97_trade_result(&f.screen)==3&&f.screen.child==0x23,"Compare route in second phase");
            check(nba97_trade_return_child(&f.screen,0x80,teams,slots,1)&&nba97_trade_result(&f.screen)==0&&
                f.screen.phase==NBA97_TRADE_SECOND&&f.screen.team[0]==2&&f.screen.team[1]==3,"Compare reentry preserves phase");
            f.release();check(f.key(0x100)==NBA97_TRADE_CANCEL_PICK&&nba97_trade_result(&f.screen)==0,"cancel pick must not exit parent");
            f.release();check(f.key(0x100)==NBA97_TRADE_DISCARD&&nba97_trade_result(&f.screen)==-1,"clean cancel signed result");
            f.begin();check(f.key(0x80)==NBA97_TRADE_ACCEPT&&nba97_trade_result(&f.screen)==1,"accept result");
            f.begin();f.key(0x800);f.key(0x800);
            check(f.key(0x100)==NBA97_TRADE_DISCARD_PROMPT&&nba97_trade_result(&f.screen)==0,"dirty cancel is not yet an exit");
            check(nba97_trade_discard_answer(&f.screen,0,0x800)==NBA97_TRADE_IDLE&&nba97_trade_result(&f.screen)==0,"declined discard keeps selector running");
            f.release();check(nba97_trade_discard_answer(&f.screen,1,0x800)==NBA97_TRADE_DISCARD&&nba97_trade_result(&f.screen)==-1,"confirmed discard route");
        }
        std::cout << "TRADE PASS selector_routes_and_child_reentry\n";
        {
            Nba97TradeScreen screen{};
            for(int value=-32768;value<=32767;++value) {
                screen.selector_result=static_cast<int16_t>(value);
                check(nba97_trade_result(&screen)==value,"signed halfword return narrowed to bool/unsigned");
            }
        }
        std::cout << "TRADE PASS signed_halfword_selector_result\n";
        std::cout << "TRADE ENTRY constructor/callback/signed-return contracts tested; full dependencies and original timing verified separately\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "TRADE FAIL " << e.what() << '\n';
        return 1;
    }
}
