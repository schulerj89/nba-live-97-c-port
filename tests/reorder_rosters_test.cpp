#include "roster_database.hpp"
#include "reorder_preview.hpp"
#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>

void rosterListsTests();
void rosterListsLocalTests(const nba97::RosterDatabase&, const std::string&);

namespace {
void check(bool condition, const char* detail) {
    if (!condition) throw std::runtime_error(detail);
}
void pass(const char* id, const char* detail) {
    std::cout << "REORDER PASS " << id << " | " << detail << '\n';
}
using Slots = std::array<std::uint16_t, NBA97_TEAM_SLOTS>;
Slots synthetic(int occupied = NBA97_TEAM_SLOTS) {
    Slots slots;
    slots.fill(UINT16_MAX);
    for (int i = 0; i < occupied; ++i) slots[i] = static_cast<std::uint16_t>(100 + i);
    return slots; // Invented test IDs, not game data.
}
void screenConstruction() {
    nba97::RosterDatabase::SlotTable table;
    for (std::size_t i = 0; i < table.size(); ++i) table[i] = static_cast<std::uint16_t>(i);
    Nba97ReorderScreen s{};
    check(nba97_reorder_screen_enter(&s, table.data(), 29, 0, nullptr, nullptr, nullptr, 0), "screen entry");
    check(s.team == 3 && s.layout == 13 && s.visible_rows == 6 &&
        s.list_kind[0] == 1 && s.list_kind[1] == 2, "wrapper arguments");
    check(std::equal(table.begin(), table.end(), s.snapshot) &&
        std::equal(table.begin(), table.end(), s.working), "full 535 snapshot");
    check(s.selection.changes == 0 && s.selection.selected_ids[0] == 45 &&
        s.selection.selected_ids[1] == 45, "entry identities");
    pass("screen_entry_snapshot", "80056AEC/56494: normalize, same-team kinds 1/2, 535 isolated IDs, zero changes");
    for (int p=0;p<2;++p) for (int row=0;row<15;++row) {
        const auto& r=s.rows[p*15+row];
        check(r.id==p*15+row && r.player_id==45+row && r.type==0x33 && r.page==p && r.alignment==1, "row binding");
        check(r.x==(p?270:60) && r.y==112+16*row && r.up==(row!=0) && r.down==(row!=14) && r.team_scan, "row geometry/hooks");
    }
    check(s.heading_x==256 && s.heading_y==70 && s.image_object[0]==18 && s.image_object[1]==19, "header objects");
    for(int i=0;i<4;++i) check(s.arrow_x[i]==6 && s.arrow_y[i]==10,"arrow offsets");
    // 8003DD38 constructs both pairs; changing focus must not erase the
    // inactive list's scroll cues. Exercise every pair of valid scroll tops.
    const auto before_markers = s;
    for (int active_page=0;active_page<2;++active_page)
        for (int left=0;left<=9;++left) for (int right=0;right<=9;++right) {
            s.selection.active_page=static_cast<std::uint8_t>(active_page);
            s.selection.top[0]=static_cast<std::uint8_t>(left);
            s.selection.top[1]=static_cast<std::uint8_t>(right);
            Nba97ReorderMarker markers[4]{};
            nba97_reorder_screen_markers(&s,markers);
            for(int p=0;p<2;++p) for(int down=0;down<2;++down) {
                const auto& m=markers[p+2*down];
                const int top=p?right:left;
                check((m.visible != 0)==(down?top<9:top>0),"independent list marker visibility");
                check(m.glyph==(down?0x8c:0x8b) && m.x==(p?256:46) &&
                    m.y==(down?196:116),"original marker glyph/geometry");
            }
        }
    for(int i=0;i<4;++i) { s.arrow_x[i]=static_cast<std::uint8_t>(i+1); s.arrow_y[i]=static_cast<std::uint8_t>(i+5); }
    Nba97ReorderMarker custom_markers[4]{};
    nba97_reorder_screen_markers(&s,custom_markers);
    for(int i=0;i<4;++i) check(custom_markers[i].x==((i%2)?250:40)+i+1 &&
        custom_markers[i].y==(i<2?106:186)+i+5,"each marker owns its authored offsets");
    nba97_reorder_screen_markers(nullptr,custom_markers);
    for(const auto& m:custom_markers) check(!m.visible,"null marker source");
    s=before_markers;
    check(std::strcmp(nba97_reorder_screen_help_tag(&s),"hel1")==0,"first Help graphic");
    nba97_reorder_begin_second(&s.selection);
    check(std::strcmp(nba97_reorder_screen_help_tag(&s),"hel2")==0,"replacement Help graphic");
    nba97_reorder_finish_second(&s.selection,0);
    check(std::strcmp(nba97_reorder_screen_help_tag(&s),"hel1")==0,"returned Help graphic");
    for(int invalid=2;invalid<=255;++invalid) {
        s.selection.descriptor_page=static_cast<std::uint8_t>(invalid);
        check(nba97_reorder_screen_help_tag(&s)==nullptr,"invalid Help descriptor guard");
    }
    check(nba97_reorder_screen_help_tag(nullptr)==nullptr,"null Help footer guard");
    s=before_markers;
    check(s.first_callback==0x800568e4 && s.second_callback==0x800569bc &&
          s.entry_callback==0x800560bc && s.exit_callback==0x80056254, "lifecycle hooks");
    pass("screen_object_construction", "30 typed rows, 60/270 x, 112+16n y, endpoint callbacks, portrait/header/arrow metadata");
    const std::int16_t cursor[]{8,27},top[]{4,24};
    check(nba97_reorder_screen_enter(&s,table.data(),0,0,nullptr,cursor,top,1),"saved entry");
    check(s.selection.cursor[0]==8 && s.selection.cursor[1]==12 && s.selection.top[0]==4 &&
        s.selection.top[1]==9 && s.selection.phase==NBA97_REORDER_REPLACEMENT,"restored state");
    std::int16_t saved_c[2],saved_t[2]; std::uint8_t active=0;
    nba97_reorder_screen_save(&s,saved_c,saved_t,&active);
    check(std::equal(cursor,cursor+2,saved_c) && std::equal(top,top+2,saved_t) && active==1,"absolute roundtrip");
    check(s.rows[4].y==112 && s.rows[24].y==112,"restored clipping origin");
    pass("screen_saved_entry", "both absolute object cursors/tops restore, active replacement and identity/header refresh, roundtrip");
    check(nba97_reorder_screen_result(&s)==0,"running result");
    nba97_reorder_second_callback(&s.selection,0x10,1);
    check(nba97_reorder_screen_result(&s)==2 && s.selection.child_ids[0]==12,"view handoff result");
    nba97_reorder_clear_screen_result(&s.selection);
    nba97_reorder_second_callback(&s.selection,0x40,1);
    check(nba97_reorder_screen_result(&s)==3 && s.selection.child_ids[0]==8 && s.selection.child_ids[1]==12,"compare handoff");
    nba97_reorder_clear_screen_result(&s.selection);
    pass("screen_result_contract", "signed result0/2/3 propagated, active selected IDs; actual child screens explicitly outside this owner");
    const auto before=s;
    const std::int16_t bad_cursor[]{15,15};
    check(!nba97_reorder_screen_enter(&s,table.data(),-1,0,nullptr,nullptr,nullptr,0) &&
        !nba97_reorder_screen_enter(&s,table.data(),0,0,nullptr,bad_cursor,top,0) &&
        !nba97_reorder_screen_enter(&s,table.data(),29,2,nullptr,nullptr,nullptr,0),"entry guards");
    check(std::memcmp(&s,&before,sizeof(s))==0,"invalid entry atomic");
    pass("screen_entry_guards", "native malformed team/context/cursor guards leave existing screen untouched; not MIPS credit");
    std::int8_t eligible[16]; std::fill(std::begin(eligible),std::end(eligible),std::int8_t{7}); eligible[1]=9;
    check(nba97_reorder_screen_enter(&s,table.data(),3,2,eligible,nullptr,nullptr,0) && s.team==7,"restricted fallback");
    check(nba97_reorder_screen_scan(&s,1) && s.team==9 && nba97_reorder_screen_scan(&s,1) && s.team==7,"restricted scan");
    nba97_reorder_screen_input(&s,NBA97_REORDER_SELECT);
    check(!nba97_reorder_screen_scan(&s,1) && s.team==7,"replacement team gate");
    pass("screen_team_eligibility", "mode2 first-list fallback, both descriptors follow, eligible scan cycle, replacement gate");
    check(nba97_reorder_screen_enter(&s,table.data(),0,0,nullptr,nullptr,nullptr,0),"fresh");
    for(int i=0;i<9;++i) nba97_reorder_screen_input(&s,NBA97_REORDER_DOWN);
    check(nba97_reorder_screen_scan(&s,-1) && s.team==28 && s.selection.cursor[0]==9 && s.selection.top[0]==4,"wrapped preserved scan");
    check(s.selection.selected_ids[0]==429 && s.rows[4].y==112,"scan rebind");
    check(nba97_reorder_screen_scan(&s,1) && s.team==0,"scan wraps right");
    pass("screen_scan_rebind", "29-team wrap, both list identities and portrait IDs refreshed; cursor/scroll retained");
    auto swap = [&] {
        nba97_reorder_screen_input(&s,NBA97_REORDER_SELECT);
        nba97_reorder_screen_input(&s,NBA97_REORDER_DOWN);
        check(nba97_reorder_screen_input(&s,NBA97_REORDER_SELECT)==NBA97_REORDER_SWAPPED,"screen swap");
    };
    swap();
    check(nba97_reorder_screen_scan(&s,1),"scan with edits"); swap();
    check(s.selection.changes==2 && s.working[9]!=table[9] && s.working[24]!=table[24],"multi-team staging");
    check(std::equal(table.begin(),table.end(),s.snapshot),"entry baseline immutable");
    nba97_reorder_screen_input(&s,NBA97_REORDER_CANCEL);
    check(s.selection.phase==NBA97_REORDER_DISCARD_PROMPT,"whole transaction prompt");
    nba97_reorder_screen_input(&s,NBA97_REORDER_DISCARD_NO);
    check(s.selection.phase==NBA97_REORDER_FIRST && s.selection.changes==2,"resume editing");
    nba97_reorder_screen_input(&s,NBA97_REORDER_CANCEL);
    nba97_reorder_screen_input(&s,NBA97_REORDER_DISCARD_YES);
    check(s.result==1 && !s.selection.accepted && std::equal(table.begin(),table.end(),s.working),"whole table discard");
    pass("screen_multi_team_discard", "staged swaps on two teams, resume/no, confirmed discard restores all 535 slots");
}
void arrowFlash() {
    for(unsigned initial=0;initial<256;++initial) {
        Nba97ReorderTint t{};
        std::fill_n(t.start,3,static_cast<uint8_t>(initial));
        std::fill_n(t.rgb,3,static_cast<uint8_t>(initial));
        nba97_reorder_tint_flash(&t);
        const int gold[3]={120,102,0};
        for(unsigned frame=1;frame<=21;++frame) {
            nba97_reorder_tint_tick(&t);
            for(unsigned c=0;c<3;++c) {
                const int expected=frame<=4 ? int(initial)+(gold[c]-int(initial))*int(frame)/4 :
                    frame<=16 ? gold[c] : frame<=20 ? gold[c]+(128-gold[c])*int(frame-16)/4 : 128;
                check(t.rgb[c]==expected,"arrow flash interpolation/hold/return");
            }
            const unsigned phase=frame<5 ? 0x40 : frame<16 ? 0xc0 : frame<21 ? 0x80 : 0;
            check((t.flags&0xc0)==phase,"arrow flash boundary must use > duration");
        }
        check(!t.flags,"arrow flash did not settle");
        const auto settled=t;nba97_reorder_tint_tick(&t);
        check(!std::memcmp(&settled,&t,sizeof(t)),"idle flash tick mutated state");
    }
    Nba97ReorderTint t{};std::fill_n(t.start,3,uint8_t{128});std::fill_n(t.rgb,3,uint8_t{128});
    nba97_reorder_tint_flash(&t);nba97_reorder_tint_tick(&t);
    auto before=t;nba97_reorder_tint_flash(&t);
    check(!std::memcmp(&before,&t,sizeof(t)),"retrigger restarted fade-in");
    for(unsigned i=0;i<4;++i) nba97_reorder_tint_tick(&t);
    for(unsigned i=0;i<9;++i) nba97_reorder_tint_tick(&t);
    before=t;nba97_reorder_tint_flash(&t);
    check(t.elapsed==0 && t.flags==before.flags && !std::memcmp(t.rgb,before.rgb,3),"hold retrigger changed color/phase");
    for(unsigned i=0;i<11;++i) nba97_reorder_tint_tick(&t);
    nba97_reorder_tint_tick(&t); // First return color122/108/32.
    nba97_reorder_tint_flash(&t);
    check(t.start[0]==120 && t.start[1]==128 && t.start[2]==128 && t.duration==4 && t.elapsed==0,
        "flash retarget lost the source red-channel quirk");
    nba97_reorder_tint_tick(&t);
    check(t.rgb[0]==120 && t.rgb[1]==122 && t.rgb[2]==96,"return-phase retrigger wrong RGB");
    pass("arrow_flash_256_starts_21_updates_retrigger", "2ADEC/2AE5C; four-update fade, hold, return; red-channel quirk");
}

void screenPublication(const std::filesystem::path& path) {
    nba97::RosterDatabase db; db.load(path);
    const auto before=db.slotTable();
    Nba97ReorderScreen s{};
    nba97_reorder_screen_enter(&s,before.data(),0,0,nullptr,nullptr,nullptr,0);
    for(int team=0;team<2;++team) {
        nba97_reorder_screen_input(&s,NBA97_REORDER_SELECT);
        nba97_reorder_screen_input(&s,NBA97_REORDER_DOWN);
        nba97_reorder_screen_input(&s,NBA97_REORDER_SELECT);
        if(!team) nba97_reorder_screen_scan(&s,1);
    }
    check(db.slotTable()==before,"live isolation");
    check(!db.applyReorderScreen(s),"active cannot publish");
    nba97_reorder_screen_input(&s,NBA97_REORDER_ACCEPT);
    check(db.applyReorderScreen(s),"atomic publication");
    const auto after=db.slotTable();
    check(std::equal(after.begin(),after.end(),s.working) && after!=before,"whole result");
    for(int team=0;team<29;++team) {
        const auto resolved=db.resolveTeamSlots(static_cast<std::int16_t>(team));
        for(int slot=0;slot<15;++slot) check(resolved[slot]==db.player(after[team*15+slot]),"rebuilt pointers");
    }
    check(!db.applyReorderScreen(s) && db.slotTable()==after,"stale snapshot rejected");
    db.load(path); s.working[435]^=1;
    check(!db.applyReorderScreen(s) && db.slotTable()==before,"free agent corruption rejected");
    pass("screen_database_publication", "535-slot isolation, multi-team atomic accept, resolved indexes, stale/free-agent rejection, original file unchanged");
}

void core() {
    for (int team = INT16_MIN; team <= INT16_MAX; ++team) {
        if (team == 29) continue;
        for (int mode : {0, 1, 2, 3})
            check(nba97_reorder_normalize_team(static_cast<std::int16_t>(team),
                static_cast<std::int16_t>(mode), -7) == team, "normal team must be unchanged");
    }
    pass("normalize_identity", "0x80056A94: all other signed-short values preserved");
    for (int mode = INT16_MIN; mode <= INT16_MAX; ++mode)
        check(nba97_reorder_normalize_team(29, static_cast<std::int16_t>(mode), 17) ==
            (mode == 2 ? 17 : 3), "sentinel mode branch");
    pass("normalize_sentinel", "0x80056A94: team 29 maps to 3 except mode 2");
    for (int team = INT8_MIN; team <= INT8_MAX; ++team)
        check(nba97_reorder_normalize_team(29, 2, static_cast<std::int8_t>(team)) == team,
            "context team sign extension");
    pass("normalize_signed_context", "0x80056A94: all signed-byte context teams");

    auto slots = synthetic();
    const auto before = slots;
    std::uint16_t changes = 7;
    check(nba97_reorder_swap(slots.data(), 0, 14, &changes) == NBA97_REORDER_CHANGED,
        "occupied swap");
    for (int i = 0; i < 15; ++i)
        check(slots[i] == before[i == 0 ? 14 : i == 14 ? 0 : i], "swap must not shift middle");
    check(changes == 8, "single change count");
    pass("swap_exact", "0x800558E0 occupied path: 0 <-> 14; middle slots unchanged");
    check(nba97_reorder_swap(slots.data(), 0, 14, &changes) == NBA97_REORDER_CHANGED &&
        slots == before && changes == 9, "swap roundtrip");
    pass("swap_roundtrip", "second swap restores all slots");
    check(nba97_reorder_swap(slots.data(), 4, 4, &changes) == NBA97_REORDER_SAME_PLAYER &&
        slots == before && changes == 9, "same slot must not mutate");
    slots[5] = slots[4];
    const auto duplicates = slots;
    check(nba97_reorder_swap(slots.data(), 4, 5, &changes) == NBA97_REORDER_SAME_PLAYER &&
        slots == duplicates && changes == 9, "equal IDs must not mutate");
    pass("reject_same_player", "0x800556B0 re-order branch: same slot and duplicate ID");
    slots = synthetic(5);
    const auto partial = slots;
    for (auto pair : {std::pair<int,int>{0, 5}, {5, 0}, {5, 6}, {5, 5}})
        check(nba97_reorder_swap(slots.data(), pair.first, pair.second, &changes) ==
            NBA97_REORDER_EMPTY_SLOT && slots == partial && changes == 9, "empty rejection");
    pass("reject_empty", "source/destination/both empty; no writes");
    for (int bad : {INT_MIN, -1, 15, INT_MAX}) {
        check(nba97_reorder_swap(slots.data(), bad, 0, &changes) == NBA97_REORDER_INVALID_ARGUMENT,
            "source bound");
        check(nba97_reorder_swap(slots.data(), 0, bad, &changes) == NBA97_REORDER_INVALID_ARGUMENT,
            "destination bound");
    }
    check(nba97_reorder_swap(nullptr, 0, 1, &changes) == NBA97_REORDER_INVALID_ARGUMENT &&
        nba97_reorder_swap(slots.data(), 0, 1, nullptr) == NBA97_REORDER_INVALID_ARGUMENT &&
        slots == partial && changes == 9, "null inputs must not mutate");
    pass("reject_bounds", "host guards: negative/overflow indices and null pointers");
    slots = synthetic();
    changes = UINT16_MAX;
    check(nba97_reorder_swap(slots.data(), 1, 2, &changes) == NBA97_REORDER_CHANGED &&
        changes == 0, "16-bit counter wrap");
    pass("counter_wrap", "0x80055AD4: 65535 -> 0; not a persistent dirty flag");

    int cases = 0;
    for (int occupied = 0; occupied <= 15; ++occupied) {
        for (int from = 0; from < 15; ++from) for (int to = 0; to < 15; ++to) {
            slots = synthetic(occupied);
            auto expected = slots;
            auto result = NBA97_REORDER_EMPTY_SLOT;
            if (from < occupied && to < occupied) {
                result = from == to ? NBA97_REORDER_SAME_PLAYER : NBA97_REORDER_CHANGED;
                std::swap(expected[from], expected[to]);
            }
            changes = 0;
            check(nba97_reorder_swap(slots.data(), from, to, &changes) == result &&
                slots == expected && changes == (result == NBA97_REORDER_CHANGED ? 1 : 0),
                "exhaustive slot-pair state mismatch");
            ++cases;
        }
    }
    check(cases == 3600, "exhaustive case count");
    pass("all_slot_pairs", "3600 exact-state vectors across 0..15 occupied slots");
}
void selection() {
    const auto original = synthetic(12);
    Nba97ReorderSession session{};
    check(nba97_reorder_begin(&session, original.data()) != 0, "begin selection");
    auto input = [&](Nba97ReorderAction action) {
        const auto event = nba97_reorder_input(&session, action);
        std::cout << "REORDER ACTION=" << static_cast<int>(action)
                  << " event=" << nba97_reorder_event_name(event)
                  << " phase=" << nba97_reorder_phase_name(session.phase)
                  << " source=" << static_cast<int>(session.cursor[0])
                  << " replacement=" << static_cast<int>(session.cursor[1])
                  << " changes=" << session.changes << '\n';
        // These legacy scenarios model separate, acknowledged button presses.
        // Modal blocking/lifecycle has its own frame-level regression below.
        if (session.modal) nba97_reorder_dismiss_modal(&session);
        return event;
    };
    auto intact = [&] { return std::equal(original.begin(), original.end(), session.slots); };
    check(input(NBA97_REORDER_SELECT) == NBA97_REORDER_PICKED &&
        session.phase == NBA97_REORDER_REPLACEMENT && intact() && session.changes == 0,
        "picking source must not swap");
    check(input(NBA97_REORDER_ACCEPT) == NBA97_REORDER_NO_CHANGE &&
        session.phase == NBA97_REORDER_REPLACEMENT, "Start must not exit second stage");
    input(NBA97_REORDER_DOWN);
    check(input(NBA97_REORDER_SELECT) == NBA97_REORDER_SWAPPED &&
        session.phase == NBA97_REORDER_FIRST && session.slots[0] == original[1] &&
        session.slots[1] == original[0] && session.changes == 1, "replacement swap");
    for (int i = 2; i < 15; ++i) check(session.slots[i] == original[i], "unrelated slots");
    pass("selection_swap", "first select does not mutate; replacement swaps; returns first focus");

    nba97_reorder_begin(&session, original.data());
    input(NBA97_REORDER_DOWN);
    input(NBA97_REORDER_SELECT);
    for (int i = 0; i < 8; ++i) input(NBA97_REORDER_DOWN);
    const auto destination_top = session.top[1];
    check(input(NBA97_REORDER_CANCEL) == NBA97_REORDER_CANCELLED_PICK && intact() &&
        session.phase == NBA97_REORDER_FIRST && session.cursor[0] == 1 &&
        session.cursor[1] == 8 && session.top[1] == destination_top &&
        session.changes == 0 && session.input_latch == 10, "cancel replacement preserves cursors");
    input(NBA97_REORDER_SELECT);
    check(session.cursor[1] == 8 && session.top[1] == destination_top, "reselect preserves destination");
    input(NBA97_REORDER_CANCEL);
    pass("cancel_replacement", "state-2 cancel bypasses swap, restores first focus and retains both cursors");
    check(input(NBA97_REORDER_CANCEL) == NBA97_REORDER_EXIT_DISCARDED &&
        session.phase == NBA97_REORDER_CLOSED && !session.accepted && intact(), "clean cancel exits");
    check(input(NBA97_REORDER_SELECT) == NBA97_REORDER_NO_CHANGE && intact(), "closed input inert");
    pass("cancel_first_clean", "cancel first stage exits without a prompt or changes");

    nba97_reorder_begin(&session, original.data());
    for (int i = 0; i < 12; ++i) input(NBA97_REORDER_DOWN);
    check(input(NBA97_REORDER_SELECT) == NBA97_REORDER_REJECTED_EMPTY &&
        session.phase == NBA97_REORDER_FIRST && intact(), "empty first");
    nba97_reorder_begin(&session, original.data());
    input(NBA97_REORDER_SELECT);
    session.input_latch = 10;
    check(input(NBA97_REORDER_SELECT) == NBA97_REORDER_REJECTED_SAME &&
        session.phase == NBA97_REORDER_REPLACEMENT && intact() && session.input_latch == 0,
        "same replacement remains active");
    for (int i = 0; i < 12; ++i) input(NBA97_REORDER_DOWN);
    check(input(NBA97_REORDER_SELECT) == NBA97_REORDER_REJECTED_EMPTY &&
        session.phase == NBA97_REORDER_REPLACEMENT && intact() && session.changes == 0,
        "empty replacement remains active");
    pass("selection_rejections", "empty source/destination and same-player do not change stage or slots");

    nba97_reorder_begin(&session, original.data());
    check(input(NBA97_REORDER_UP) == NBA97_REORDER_NO_CHANGE, "top bound");
    for (int i = 0; i < 14; ++i) input(NBA97_REORDER_DOWN);
    check(session.cursor[0] == 14 && session.top[0] == 9 &&
        input(NBA97_REORDER_DOWN) == NBA97_REORDER_NO_CHANGE, "six-row bottom bound");
    for (int i = 0; i < 14; ++i) input(NBA97_REORDER_UP);
    check(session.cursor[0] == 0 && session.top[0] == 0 && session.cursor[1] == 0, "independent cursor");
    pass("selection_scroll", "15 slots, six-row viewport; boundaries and independent cursor memory");

    input(NBA97_REORDER_SELECT);
    input(NBA97_REORDER_DOWN);
    input(NBA97_REORDER_SELECT);
    const auto swapped = session;
    check(input(NBA97_REORDER_CANCEL) == NBA97_REORDER_ASK_DISCARD &&
        session.phase == NBA97_REORDER_DISCARD_PROMPT, "dirty cancel asks");
    check(input(NBA97_REORDER_SELECT) == NBA97_REORDER_NO_CHANGE &&
        session.phase == NBA97_REORDER_DISCARD_PROMPT, "prompt requires explicit answer");
    check(input(NBA97_REORDER_DISCARD_NO) == NBA97_REORDER_RESUMED &&
        session.phase == NBA97_REORDER_FIRST && session.changes == 1 &&
        std::equal(std::begin(swapped.slots), std::end(swapped.slots), session.slots), "decline keeps swaps");
    input(NBA97_REORDER_SELECT);
    input(NBA97_REORDER_CANCEL);
    check(session.changes == 1 && session.slots[0] == original[1], "cancel pick keeps previous swap");
    input(NBA97_REORDER_CANCEL);
    check(input(NBA97_REORDER_DISCARD_YES) == NBA97_REORDER_EXIT_DISCARDED &&
        session.phase == NBA97_REORDER_CLOSED && !session.accepted && intact(), "discard restores baseline");
    pass("cancel_first_dirty", "prompt decline resumes; confirmation discards; second-stage cancel keeps prior swaps");
    nba97_reorder_begin(&session, original.data());
    input(NBA97_REORDER_SELECT); input(NBA97_REORDER_DOWN); input(NBA97_REORDER_SELECT);
    check(input(NBA97_REORDER_ACCEPT) == NBA97_REORDER_EXIT_ACCEPTED && session.accepted &&
        session.phase == NBA97_REORDER_CLOSED && session.slots[0] == original[1], "accept retains swaps");
    pass("selection_accept", "continue exits first stage with accepted working order");
}

void callbackMasks() {
    auto slots = synthetic();
    Nba97ReorderSession session{};
    for (unsigned mask = 0; mask <= UINT16_MAX; ++mask) {
        nba97_reorder_begin(&session, slots.data());
        session.input_latch = 10;
        auto expected = NBA97_REORDER_NO_CHANGE;
        if (mask == 0x10) expected = NBA97_REORDER_REQUEST_VIEW;
        if (mask == 0x40) expected = NBA97_REORDER_REQUEST_COMPARE;
        if (mask == 0x800) expected = NBA97_REORDER_PICKED;
        check(nba97_reorder_first_callback(&session, static_cast<std::uint16_t>(mask)) == expected,
            "exact callback mask dispatch");
        check(session.input_latch == (expected == NBA97_REORDER_NO_CHANGE ? 0 : 10), "callback latch");
        check(session.phase == (mask == 0x800 ? NBA97_REORDER_REPLACEMENT : NBA97_REORDER_FIRST) &&
            session.changes == 0 && std::equal(slots.begin(), slots.end(), session.slots),
            "callback must not swap or advance on combined masks");
    }
    pass("first_callback_masks", "65536 exact masks; 0x10 View, 0x40 Compare, 0x800 confirm; others clear latch");
    slots[0] = UINT16_MAX;
    nba97_reorder_begin(&session, slots.data());
    check(nba97_reorder_first_callback(&session, 0x800) == NBA97_REORDER_REJECTED_EMPTY &&
        session.phase == NBA97_REORDER_FIRST, "empty raw confirm stays first");
    nba97_reorder_dismiss_modal(&session);
    check(nba97_reorder_first_callback(&session, 0x10) == NBA97_REORDER_REJECTED_EMPTY &&
        session.modal == NBA97_REORDER_MODAL_VIEW_EMPTY && !session.screen_result, "empty View rejected");
    nba97_reorder_dismiss_modal(&session);
    check(nba97_reorder_first_callback(&session, 0x40) == NBA97_REORDER_REJECTED_EMPTY &&
        session.modal == NBA97_REORDER_MODAL_COMPARE_EMPTY && !session.screen_result, "empty Compare rejected");
    nba97_reorder_dismiss_modal(&session);
    session.phase = NBA97_REORDER_REPLACEMENT;
    session.input_latch = 10;
    check(nba97_reorder_first_callback(&session, 0x800) == NBA97_REORDER_NO_CHANGE &&
        session.input_latch == 10 && session.changes == 0, "wrong-phase guard");
    check(nba97_reorder_first_callback(nullptr, 0) == NBA97_REORDER_NO_CHANGE, "null guard");
    pass("first_callback_guards", "empty confirm/View/Compare rejected; wrong phase/null are inert");
}

void secondCallbackMasks() {
    const auto original = synthetic();
    struct GuardedSession {
        std::uint64_t before = 0x12345678abcdef01ULL;
        Nba97ReorderSession session{};
        std::uint64_t after = 0xfedcba9876543210ULL;
    } guarded;
    auto& session = guarded.session;
    for (std::uint8_t state : {1, 2}) for (unsigned mask = 0; mask <= UINT16_MAX; ++mask) {
        nba97_reorder_begin(&session, original.data());
        nba97_reorder_first_callback(&session, 0x800);
        nba97_reorder_input(&session, NBA97_REORDER_DOWN);
        session.input_latch = 7;
        auto expected_slots = original;
        auto expected = NBA97_REORDER_NO_CHANGE;
        auto expected_phase = NBA97_REORDER_REPLACEMENT;
        int expected_latch = 0, expected_changes = 0;
        if (mask == 0x10 || mask == 0x40) {
            expected = mask == 0x10 ? NBA97_REORDER_REQUEST_VIEW : NBA97_REORDER_REQUEST_COMPARE;
            expected_latch = 7;
        } else if (mask == 0x800) {
            expected_phase = NBA97_REORDER_FIRST;
            if (state == 2) {
                expected = NBA97_REORDER_CANCELLED_PICK;
                expected_latch = 10;
            } else {
                expected = NBA97_REORDER_SWAPPED;
                expected_latch = 7;
                expected_changes = 1;
                std::swap(expected_slots[0], expected_slots[1]);
            }
        }
        check(nba97_reorder_second_callback(&session, static_cast<std::uint16_t>(mask), state) == expected,
            "second callback exact mask result");
        check(session.phase == expected_phase && session.input_latch == expected_latch &&
            session.changes == expected_changes && session.cursor[0] == 0 && session.cursor[1] == 1 &&
            session.top[0] == 0 && session.top[1] == 0 && !session.accepted &&
            std::equal(expected_slots.begin(), expected_slots.end(), session.slots) &&
            std::equal(original.begin(), original.end(), session.original), "second callback state delta");
        check(guarded.before == 0x12345678abcdef01ULL && guarded.after == 0xfedcba9876543210ULL,
            "callback return must preserve surrounding caller data");
    }
    pass("second_callback_masks", "131072 normal/cancel mask cases; exact results, slots, cursors, latch and caller guards");

    // State 2 alone bypasses validation. Check every original object-state byte.
    for (unsigned state = 0; state <= UINT8_MAX; ++state) {
        nba97_reorder_begin(&session, original.data());
        nba97_reorder_first_callback(&session, 0x800);
        nba97_reorder_input(&session, NBA97_REORDER_DOWN);
        check(nba97_reorder_second_callback(&session, 0x800, static_cast<std::uint8_t>(state)) ==
            (state == 2 ? NBA97_REORDER_CANCELLED_PICK : NBA97_REORDER_SWAPPED), "state-2-only bypass");
    }
    pass("second_callback_object_state", "all 256 state bytes; only 2 bypasses validation/mutation");

    nba97_reorder_begin(&session, original.data());
    nba97_reorder_first_callback(&session, 0x800);
    check(nba97_reorder_second_callback(&session, 0x800, 1) == NBA97_REORDER_REJECTED_SAME &&
        session.phase == NBA97_REORDER_REPLACEMENT && session.changes == 0, "same-player return");
    session.slots[1] = UINT16_MAX;
    nba97_reorder_input(&session, NBA97_REORDER_DOWN);
    check(nba97_reorder_second_callback(&session, 0x800, 1) == NBA97_REORDER_REJECTED_EMPTY &&
        session.phase == NBA97_REORDER_REPLACEMENT && session.changes == 0, "empty-player return");
    nba97_reorder_dismiss_modal(&session);
    check(nba97_reorder_second_callback(&session, 0x800, 2) == NBA97_REORDER_CANCELLED_PICK &&
        session.phase == NBA97_REORDER_FIRST && session.slots[1] == UINT16_MAX &&
        session.changes == 0 && session.input_latch == 10, "cancel bypasses even invalid pair");
    for (auto phase : {NBA97_REORDER_FIRST, NBA97_REORDER_DISCARD_PROMPT, NBA97_REORDER_CLOSED}) {
        session.phase = phase;
        check(nba97_reorder_second_callback(&session, 0x800, 2) == NBA97_REORDER_NO_CHANGE &&
            session.phase == phase && session.input_latch == 10 && session.changes == 0, "wrong-phase return");
    }
    check(nba97_reorder_second_callback(nullptr, 0x800, 1) == NBA97_REORDER_NO_CHANGE, "null return");
    check(guarded.before == 0x12345678abcdef01ULL && guarded.after == 0xfedcba9876543210ULL, "guarded exit paths");
    pass("second_callback_returns", "same/empty/cancel/invalid-phase/null returns; caller canaries intact, no accidental commit");
}

void selectionDependencies() {
    const auto slots = synthetic(12);
    Nba97ReorderSession s{};
    nba97_reorder_begin(&s, slots.data());
    const auto entry_header = s.header_revision, entry_rows = s.row_revision;
    nba97_reorder_first_callback(&s, 0x800);
    check(s.active_page == 1 && s.descriptor_page == 1 && s.object_state == 1 &&
        s.header_revision == entry_header+1 && s.row_revision == entry_rows, "begin page/header contract");
    auto& tint = s.tint[1][0];
    check(tint.flags == 3 && tint.duration == 20 && tint.elapsed == 0, "begin pulse constants");
    for (int frame = 1; frame <= 20; ++frame) {
        nba97_reorder_frame(&s, 0);
        check(tint.rgb[0] == 120 && tint.rgb[1] == 102 && tint.rgb[2] == 0, "initial gold hold");
    }
    nba97_reorder_frame(&s, 0); // endpoint reversal occurs at duration+1, not duration
    check(tint.elapsed == 0 && tint.target[0] == 128 && tint.target[2] == 128, "pulse reversal boundary");
    for (int frame = 1; frame <= 20; ++frame) {
        nba97_reorder_frame(&s, 0);
        check(tint.rgb[0] == 120+8*frame/20 && tint.rgb[1] == 102+26*frame/20 &&
            tint.rgb[2] == 128*frame/20, "gold to neutral interpolation");
    }
    nba97_reorder_frame(&s, 0);
    for (int frame = 1; frame <= 10; ++frame) nba97_reorder_frame(&s, 0);
    check(tint.rgb[0] == 124 && tint.rgb[1] == 115 && tint.rgb[2] == 64, "reverse midpoint signed division");
    nba97_reorder_second_callback(&s, 0x800, 2);
    check(tint.duration == 8 && tint.flags == 2 && tint.start[0] == 128 &&
        tint.start[1] == 102 && tint.start[2] == 0, "reset preserves original red-channel quirk");
    for (int frame = 1; frame <= 8; ++frame) {
        check(!nba97_reorder_frame(&s, 0x100), "held cancel barrier");
        check(tint.rgb[0] == 128 && tint.rgb[1] == 102+26*frame/8 &&
            tint.rgb[2] == 128*frame/8, "eight-update neutral transition");
    }
    nba97_reorder_frame(&s, 0x100);
    check(tint.flags == 0 && tint.rgb[2] == 128, "reset completion freezes final neutral color");
    check(s.active_page == 0 && s.descriptor_page == 0 && s.object_state == 0 &&
        s.row_revision == entry_rows && s.header_revision == entry_header+2 && s.input_latch == 10,
        "cancel restores pages/header without rebuilding rows");
    check(nba97_reorder_frame(&s, 0) && !s.waiting_input_change, "release permits next dispatch");
    pass("selection_highlight_frames", "20+1 reversal, signed RGB interpolation, 8-frame reset including original red quirk");
    pass("selection_cancel_barrier", "cancel pumps frames without roster changes; held input blocked until changed; latch=10");

    for (int source = 0; source < 12; ++source) for (int dest = 0; dest < 15; ++dest) {
        nba97_reorder_begin(&s, slots.data());
        for (int n=0; n<source; ++n) nba97_reorder_input(&s, NBA97_REORDER_DOWN);
        nba97_reorder_first_callback(&s, 0x800);
        for (int n=0; n<dest; ++n) nba97_reorder_input(&s, NBA97_REORDER_DOWN);
        const auto original = s;
        const auto event = nba97_reorder_second_callback(&s, 0x800, 1);
        const bool success = source != dest && dest < 12;
        check(event == (success ? NBA97_REORDER_SWAPPED : dest >= 12 ?
            NBA97_REORDER_REJECTED_EMPTY : NBA97_REORDER_REJECTED_SAME), "refresh pair result");
        check(s.row_revision == original.row_revision + (success ? 1 : 0), "row rebuild only on successful mutation");
        check(s.header_revision == original.header_revision + (success ? 2 : 0), "swap refresh precedes finish refresh");
        check(s.top[0] == original.top[0] && s.top[1] == original.top[1] &&
            s.cursor[0] == source && s.cursor[1] == dest, "refresh preserves viewport ownership");
        for (int page=0; page<2; ++page) for (int row=0; row<15; ++row)
            check(s.row_ids[page][row] == s.slots[row], "both cached row lists rebound to working order");
        for (int page=0; page<2; ++page)
            check(s.selected_ids[page] == s.slots[s.cursor[page]], "selected IDs must not be stale after swap");
    }
    pass("selection_refresh_pairs", "180 source/destination cases; both row caches, selected IDs, refresh order, viewport preservation");

    for (int phase=0; phase<2; ++phase) for (int source=0; source<15; ++source)
        for (int dest=0; dest<15; ++dest) for (auto mask : {0x10, 0x40}) {
            nba97_reorder_begin(&s, slots.data());
            s.phase = phase ? NBA97_REORDER_REPLACEMENT : NBA97_REORDER_FIRST;
            s.cursor[0] = static_cast<std::uint8_t>(source); s.cursor[1] = static_cast<std::uint8_t>(dest);
            s.input_latch = 7;
            const bool denied = mask == 0x10 ? (phase ? dest : source) >= 12 : source >= 12 || dest >= 12;
            const auto event = phase ? nba97_reorder_second_callback(&s, mask, 1) : nba97_reorder_first_callback(&s, mask);
            check(event == (denied ? NBA97_REORDER_REJECTED_EMPTY : mask == 0x10 ?
                NBA97_REORDER_REQUEST_VIEW : NBA97_REORDER_REQUEST_COMPARE), "child availability matrix");
            check(s.input_latch == 7 && !s.changes, "child route preserves latch and working order");
            if (denied) check(s.modal && !s.screen_result, "denied child cannot dispatch");
            else check(s.screen_result == (mask == 0x10 ? 2 : 3) &&
                s.child_ids[0] == slots[mask == 0x10 && phase ? dest : source] &&
                s.child_ids[1] == (mask == 0x10 ? UINT16_MAX : slots[dest]), "original result and child identity");
        }
    nba97_reorder_begin(&s, slots.data());
    nba97_reorder_first_callback(&s, 0x800); nba97_reorder_input(&s, NBA97_REORDER_DOWN);
    nba97_reorder_second_callback(&s, 0x800, 1);
    nba97_reorder_first_callback(&s, 0x40);
    check(s.child_ids[0] == slots[1] && s.child_ids[1] == slots[0] && s.screen_result == 3,
        "child handoff uses edited IDs, not immutable database order");
    nba97_reorder_clear_screen_result(&s);
    check(!s.screen_result && s.child_ids[0] == UINT16_MAX && s.changes == 1, "consume route without publishing");
    pass("selection_child_handoff", "900 first/second View/Compare availability cases; same-ID comparison allowed; edited IDs retained");

    nba97_reorder_begin(&s, slots.data());
    for (int n=0; n<12; ++n) nba97_reorder_input(&s, NBA97_REORDER_DOWN);
    nba97_reorder_first_callback(&s, 0x800);
    check(s.modal == NBA97_REORDER_MODAL_EMPTY, "empty-selection message");
    const auto blocked = s;
    for (auto action : {NBA97_REORDER_UP, NBA97_REORDER_SELECT, NBA97_REORDER_CANCEL, NBA97_REORDER_ACCEPT})
        check(nba97_reorder_input(&s, action) == NBA97_REORDER_NO_CHANGE, "modal owns input");
    check(std::memcmp(&blocked, &s, sizeof s) == 0, "blocked input cannot mutate modal/session");
    nba97_reorder_dismiss_modal(&s); nba97_reorder_frame(&s, 0);
    check(!s.modal && s.phase == NBA97_REORDER_FIRST && s.cursor[0] == 12 && !s.input_latch, "ack restores selection");
    check(nba97_reorder_input(&s, NBA97_REORDER_UP) == NBA97_REORDER_MOVED, "input resumes after modal");
    pass("selection_modal_blocking", "original empty modal; actions blocked during message; explicit acknowledgement restores focus");
}

void writeLabels(const PshImage& image, const char* name) {
    const auto root = std::filesystem::path(".local/verification/reorder");
    std::filesystem::create_directories(root);
    std::ofstream out(root / name, std::ios::binary);
    out << "P6\n" << image.width << ' ' << image.height << "\n255\n";
    for (std::size_t i = 0; i < image.rgba.size(); i += 4)
        out.write(reinterpret_cast<const char*>(image.rgba.data() + i), 3);
    if (!out) throw std::runtime_error("Re-order diagnostic capture write failed");
}

void fontAssets(const std::string& path) {
    nba97::RosterDatabase db;
    db.load(path);
    const auto asset_root = std::filesystem::path(path).parent_path().parent_path();
    nba97::ReorderLabelPreview preview(asset_root);
    check(preview.font().glyphCount() == 156 && preview.font().transposedGlyphCount() == 47,
        "expected original ZFONT0 inventory");
    Nba97ReorderSession session{};
    nba97_reorder_begin(&session, db.teams().front().roster.data());
    const auto before = preview.render(session, db);
    check(std::count_if(before.rgba.begin(), before.rgba.end(), [](auto c) {return c != 0;}) > 100,
        "original glyphs must render");
    nba97_reorder_first_callback(&session, 0x800);
    check(preview.render(session, db).rgba == before.rgba, "pick must preserve label layer");
    nba97_reorder_input(&session, NBA97_REORDER_DOWN);
    check(nba97_reorder_second_callback(&session, 0x10, 1) == NBA97_REORDER_REQUEST_VIEW &&
        nba97_reorder_second_callback(&session, 0x40, 1) == NBA97_REORDER_REQUEST_COMPARE &&
        preview.render(session, db).rgba == before.rgba, "child requests must preserve asset-backed labels");
    nba97_reorder_second_callback(&session, 0x800, 2);
    check(preview.render(session, db).rgba == before.rgba, "cancel must preserve label layer");
    nba97_reorder_first_callback(&session, 0x800);
    nba97_reorder_second_callback(&session, 0x800, 1);
    const auto swapped = preview.render(session, db);
    check(swapped.rgba != before.rgba, "swap must refresh both label columns from working slots");
    // Rows 2..5 and all pixels outside the first two glyph rows stay unchanged.
    for (int y = 128; y < 240; ++y) for (int x = 0; x < 512 * 4; ++x)
        check(swapped.rgba[y * 512 * 4 + x] == before.rgba[y * 512 * 4 + x], "unrelated label rows changed");
    nba97_reorder_input(&session, NBA97_REORDER_CANCEL);
    nba97_reorder_input(&session, NBA97_REORDER_DISCARD_YES);
    check(preview.render(session, db).rgba == before.rgba, "discard must restore original glyph output");
    writeLabels(before, "labels_before.ppm");
    writeLabels(swapped, "labels_swapped.ppm");
    bool rejected = false;
    try { nba97::ReorderLabelPreview missing(asset_root / "missing-pack"); }
    catch (const std::exception&) { rejected = true; }
    check(rejected, "missing font pack cannot silently substitute a system font");
    std::cout << "REORDER ASSET fonts/ZFONT0.PSH glyphs=156 transposed=47; database/roster.n97db; "
                 "captures=.local/verification/reorder/labels_*.ppm; diagnostic-layer-only\n";
    pass("selection_font_assets", "real local PSH glyphs + database; pick/cancel stable, swap refresh, discard restores, missing pack rejected");

    nba97_reorder_begin(&session, db.teams().front().roster.data());
    const auto idle = preview.renderFeedback(session, db, 0);
    nba97_reorder_first_callback(&session, 0x800);
    nba97_reorder_frame(&session, 0);
    const auto gold = preview.renderFeedback(session, db, 0);
    check(idle.rgba != gold.rgba, "original glyph pixels must consume pulse color");
    for (int y=0; y<240; ++y) for (int x=0; x<270; ++x) for (int c=0; c<4; ++c)
        check(idle.rgba[(y*512+x)*4+c] == gold.rgba[(y*512+x)*4+c], "second highlight must not tint source column");
    nba97_reorder_second_callback(&session, 0x800, 2);
    for (int frame=0; frame<9; ++frame) nba97_reorder_frame(&session, 0);
    check(preview.renderFeedback(session, db, 0).rgba == idle.rgba, "cancel returns actual glyph pixels to neutral");
    nba97_reorder_first_callback(&session, 0x800);
    nba97_reorder_input(&session, NBA97_REORDER_DOWN);
    nba97_reorder_second_callback(&session, 0x800, 1);
    for (int frame=0; frame<9; ++frame) nba97_reorder_frame(&session, 0);
    const auto refreshed = preview.renderFeedback(session, db, 0);
    check(refreshed.rgba != idle.rgba, "feedback consumes refreshed working row IDs");
    for (int col=0; col<2; ++col) {
        bool changed = false;
        for (int y=112; y<144; ++y) for (int x=col ? 270 : 60; x<(col ? 490 : 265); ++x)
            changed |= refreshed.rgba[(y*512+x)*4] != idle.rgba[(y*512+x)*4];
        check(changed, "both rendered columns change after swap");
    }
    writeLabels(idle, "feedback_idle.ppm"); writeLabels(gold, "feedback_gold.ppm");
    writeLabels(refreshed, "feedback_swap.ppm");
    pass("selection_feedback_assets", "pack fonts, team header, position/jersey/surname, actual pulse/reset pixels and both-list refresh");

    nba97_reorder_begin(&session, db.teams().front().roster.data());
    while (session.cursor[0] < 14) nba97_reorder_input(&session, NBA97_REORDER_DOWN);
    check(nba97_reorder_first_callback(&session, 0x800) == NBA97_REORDER_REJECTED_EMPTY, "local empty slot expected");
    const auto small = preview.renderFeedback(session, db, 0, 0);
    const auto open = preview.renderFeedback(session, db, 0, 32);
    const auto closing = preview.renderFeedback(session, db, 0, -4);
    check(small.rgba != open.rgba && closing.rgba != open.rgba && closing.rgba != small.rgba,
        "modal must grow and shrink, not replace full surface");
    auto no_modal = session; no_modal.modal = NBA97_REORDER_MODAL_NONE;
    const auto underlay = preview.renderFeedback(no_modal, db, 0);
    for (int y=0; y<240; ++y) for (int x=0; x<512; ++x) if (x<146 || x>=366 || y<106 || y>=171)
        for (int c=0; c<4; ++c) check(open.rgba[(y*512+x)*4+c] == underlay.rgba[(y*512+x)*4+c],
            "empty dialog must stay inside original 146,106,220,65 rectangle");
    writeLabels(small, "modal_opening.ppm"); writeLabels(open, "modal_empty.ppm");
    writeLabels(closing, "modal_closing.ppm");
    nba97_reorder_dismiss_modal(&session); nba97_reorder_frame(&session, 0);
    nba97_reorder_first_callback(&session, 0x10);
    const auto unavailable_view = preview.renderFeedback(session, db, 0);
    nba97_reorder_dismiss_modal(&session); nba97_reorder_frame(&session, 0);
    nba97_reorder_first_callback(&session, 0x40);
    const auto unavailable_compare = preview.renderFeedback(session, db, 0);
    check(unavailable_view.rgba != unavailable_compare.rgba, "original format placeholder substitutes requested child name");
    writeLabels(unavailable_view, "modal_view.ppm"); writeLabels(unavailable_compare, "modal_compare.ppm");
    std::cout << "REORDER ASSET reorder/dialogs.n97ui=155 bytes; origins=800AFFFA,800AFC22; "
                 "warning colors=20/10/10,100/0/0; open-step=9/4/18/8; sound calls=5(open),8(close); host-audio-tested-separately\n";
    pass("selection_modal_assets", "two original private descriptors/text; ZFONT1; bounds, growth/shrink and child-specific formatting");
}

void database(const std::string& path) {
    nba97::RosterDatabase db;
    db.load(path);
    rosterListsLocalTests(db,path);
    const auto initial = db;
    const auto free_agents = db.freeAgentSlots();
    const auto assigned = db.assignedPlayerCount();
    std::uint16_t changes = 0;
    check(db.reorderSlots(-1, 0, 1, changes) == NBA97_REORDER_INVALID_ARGUMENT &&
        db.reorderSlots(29, 0, 1, changes) == NBA97_REORDER_INVALID_ARGUMENT && changes == 0,
        "invalid database team");
    int cases = 0;
    for (const auto& team : initial.teams()) {
        check(team.roster.size() == 15, "fixed-slot database invariant");
        for (int from = 0; from < 15; ++from) for (int to = 0; to < 15; ++to) {
            db = initial;
            changes = 0;
            auto expected = team.roster;
            auto result = NBA97_REORDER_EMPTY_SLOT;
            if (expected[from] != UINT16_MAX && expected[to] != UINT16_MAX) {
                result = expected[from] == expected[to] ? NBA97_REORDER_SAME_PLAYER : NBA97_REORDER_CHANGED;
                std::swap(expected[from], expected[to]);
            }
            check(db.reorderSlots(static_cast<std::int16_t>(team.id), from, to, changes) == result &&
                changes == (result == NBA97_REORDER_CHANGED ? 1 : 0), "database result/count");
            for (const auto& other : initial.teams())
                check(db.team(other.id)->roster == (other.id == team.id ? expected : other.roster),
                    "database changed unrelated slots/team");
            const auto resolved = db.resolveTeamSlots(static_cast<std::int16_t>(team.id));
            for (int slot = 0; slot < 15; ++slot)
                check((resolved[slot] ? resolved[slot]->id : UINT16_MAX) == expected[slot],
                    "resolved slots must match edit");
            check(db.assignedPlayerCount() == assigned && db.freeAgentSlots() == free_agents &&
                db.freeAgentCount() == initial.freeAgentCount() &&
                db.unlistedPlayerCount() == initial.unlistedPlayerCount(), "membership counts drift");
            for (const auto& player : initial.players())
                check(db.rosterOwner(player.id) == initial.rosterOwner(player.id), "owner drift");
            ++cases;
        }
        std::cout << "REORDER DATABASE team=" << team.id << " pairs=225 ownership=unchanged\n";
    }
    check(cases == 29 * 225, "database case count");
    pass("database_invariants", "6525 local-team pairs; slots/resolution/owners/counts verified; no saves written");
    db = initial;
    Nba97ReorderSession session{};
    const auto& team = db.teams().front();
    const auto team_id = static_cast<std::int16_t>(team.id);
    check(nba97_reorder_begin(&session, team.roster.data()) != 0, "local session begin");
    check(!db.applyReorderSession(team_id, session), "cannot publish open session");
    nba97_reorder_input(&session, NBA97_REORDER_SELECT);
    nba97_reorder_input(&session, NBA97_REORDER_DOWN);
    nba97_reorder_input(&session, NBA97_REORDER_SELECT);
    check(db.team(team_id)->roster == initial.team(team_id)->roster, "working edits isolated");
    nba97_reorder_input(&session, NBA97_REORDER_ACCEPT);
    check(db.applyReorderSession(team_id, session), "publish accepted order");
    check(db.team(team_id)->roster[0] == initial.team(team_id)->roster[1] &&
        db.assignedPlayerCount() == initial.assignedPlayerCount() && db.freeAgentSlots() == free_agents,
        "publish order/counts");
    check(!db.applyReorderSession(team_id, session), "reject stale baseline");
    db = initial;
    session.slots[0] = UINT16_MAX;
    check(!db.applyReorderSession(team_id, session) &&
        db.team(team_id)->roster == initial.team(team_id)->roster, "reject altered membership");
    nba97_reorder_begin(&session, db.team(team_id)->roster.data());
    nba97_reorder_input(&session, NBA97_REORDER_CANCEL);
    check(!db.applyReorderSession(team_id, session), "cancelled session never publishes");
    pass("database_session", "working-copy isolation, accept, stale/tampered rejection and cancelled-session rejection");
}

void interactive(const std::string& path, int team_id) {
    nba97::RosterDatabase db;
    db.load(path);
    nba97::ReorderLabelPreview preview(std::filesystem::path(path).parent_path().parent_path());
    if (team_id < 0 || team_id >= 29 || !db.team(static_cast<std::uint16_t>(team_id)))
        throw std::runtime_error("team must be 0..28");
    Nba97ReorderSession session{};
    nba97_reorder_begin(&session, db.team(static_cast<std::uint16_t>(team_id))->roster.data());
    std::cout << "REORDER CLI interaction harness; not original screen; no disk saves\n"
                 "commands: up down select back continue yes no view compare tick ok (EOF aborts without publishing)\n"
                 "private pack-backed feedback: .local/verification/reorder/cli_feedback.ppm\n";
    while (session.phase != NBA97_REORDER_CLOSED) {
        nba97_reorder_frame(&session, 0); // CLI commands are distinct presses with release between them.
        writeLabels(preview.render(session, db), "cli_labels.ppm");
        writeLabels(preview.renderFeedback(session, db, static_cast<std::uint16_t>(team_id)), "cli_feedback.ppm");
        if (session.screen_result) {
            std::cout << "REORDER HANDOFF original-result=" << static_cast<int>(session.screen_result)
                      << " phase-preserved=" << nba97_reorder_phase_name(session.phase)
                      << " working-ids=" << session.child_ids[0] << ',' << session.child_ids[1]
                      << " (CLI inspection only; game child routing remains screen-slice work)\n";
            for (auto id : session.child_ids) if (const auto* p = db.player(id))
                std::cout << "REORDER CHILD-DATA " << p->displayName() << " jersey=" << p->jerseyNumberText()
                          << " games=" << p->season_1995_96.format(0) << '\n';
            nba97_reorder_clear_screen_result(&session);
        }
        const int active = session.phase == NBA97_REORDER_REPLACEMENT ? 1 : 0;
        std::cout << "REORDER STATE phase=" << nba97_reorder_phase_name(session.phase)
                  << " source=" << static_cast<int>(session.cursor[0])
                  << " destination=" << static_cast<int>(session.cursor[1])
                  << " changes=" << session.changes
                  << " pages=" << static_cast<int>(session.active_page) << '/' << static_cast<int>(session.descriptor_page)
                  << " rows-rev=" << session.row_revision << " header-rev=" << session.header_revision
                  << " modal=" << static_cast<int>(session.modal) << " latch=" << static_cast<int>(session.input_latch) << '\n';
        for (int slot = session.top[active]; slot < session.top[active] + 6; ++slot) {
            const auto* player = db.player(session.slots[slot]);
            std::cout << (slot == session.cursor[active] ? "> " : "  ") << slot << ' '
                      << (player ? player->displayName() : "empty") << '\n';
        }
        std::string command;
        if (!std::getline(std::cin, command)) { std::cout << "REORDER ABORT no changes published\n"; return; }
        if (command == "tick") continue;
        if (session.modal) {
            if (command == "ok") {
                for (int frame=1; frame<=32; ++frame)
                    writeLabels(preview.renderFeedback(session, db, static_cast<std::uint16_t>(team_id), -frame), "cli_feedback.ppm");
                nba97_reorder_dismiss_modal(&session);
                std::cout << "REORDER MODAL acknowledged; close animation consumed; selection preserved\n";
            } else std::cout << "REORDER MODAL owns input; type ok to acknowledge\n";
            continue;
        }
        if (session.phase == NBA97_REORDER_FIRST &&
            (command == "select" || command == "view" || command == "compare")) {
            const std::uint16_t mask = command == "select" ? 0x800 : command == "view" ? 0x10 : 0x40;
            std::cout << "REORDER CALLBACK mask=" << mask << " event="
                      << nba97_reorder_event_name(nba97_reorder_first_callback(&session, mask)) << '\n';
            continue;
        }
        if (session.phase == NBA97_REORDER_REPLACEMENT &&
            (command == "select" || command == "back" || command == "view" || command == "compare")) {
            const std::uint16_t mask = command == "view" ? 0x10 : command == "compare" ? 0x40 : 0x800;
            const std::uint8_t object_state = command == "back" ? 2 : 1;
            std::cout << "REORDER SECOND-CALLBACK mask=" << mask << " object-state=" << static_cast<int>(object_state)
                      << " event=" << nba97_reorder_event_name(
                          nba97_reorder_second_callback(&session, mask, object_state)) << '\n';
            continue;
        }
        Nba97ReorderAction action;
        if (command == "up") action = NBA97_REORDER_UP;
        else if (command == "down") action = NBA97_REORDER_DOWN;
        else if (command == "select") action = NBA97_REORDER_SELECT;
        else if (command == "back") action = NBA97_REORDER_CANCEL;
        else if (command == "continue") action = NBA97_REORDER_ACCEPT;
        else if (command == "yes") action = NBA97_REORDER_DISCARD_YES;
        else if (command == "no") action = NBA97_REORDER_DISCARD_NO;
        else { std::cout << "REORDER INPUT unknown command\n"; continue; }
        std::cout << "REORDER EVENT " << nba97_reorder_event_name(nba97_reorder_input(&session, action)) << '\n';
    }
    if (session.accepted && !db.applyReorderSession(static_cast<std::int16_t>(team_id), session))
        throw std::runtime_error("session publish rejected");
    std::cout << "REORDER EXIT accepted=" << session.accepted << "; disk saves=none\n";
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 4 && std::string(argv[1]) == "--reorder-cli") {
            interactive(argv[2], std::stoi(argv[3]));
            return 0;
        }
        if (argc != 1 && !(argc == 3 && std::string(argv[1]) == "--database"))
            throw std::runtime_error("usage: nba97_reorder_tests [--database <local roster.n97db>]");
        core();
        selection();
        callbackMasks();
        secondCallbackMasks();
        selectionDependencies();
        arrowFlash();
        screenConstruction();
        rosterListsTests();
        if (argc == 3) { database(argv[2]); fontAssets(argv[2]); screenPublication(argv[2]); }
        else std::cout << "REORDER SKIP database_invariants | local database not supplied\n";
        std::cout << "REORDER CORE PASS | UI/audio/persistence have separate suites; original-reference parity not established by this test\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "REORDER FAIL " << error.what() << '\n';
        return 1;
    }
}
