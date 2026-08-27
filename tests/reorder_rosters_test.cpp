#include "roster_database.hpp"
#include "reorder_preview.hpp"
#include <algorithm>
#include <array>
#include <climits>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>

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
    // Child requests remain requests: their own availability/callee isn't claimed.
    check(nba97_reorder_first_callback(&session, 0x10) == NBA97_REORDER_REQUEST_VIEW &&
        nba97_reorder_first_callback(&session, 0x40) == NBA97_REORDER_REQUEST_COMPARE,
        "view/compare must not become confirm");
    session.phase = NBA97_REORDER_REPLACEMENT;
    session.input_latch = 10;
    check(nba97_reorder_first_callback(&session, 0x800) == NBA97_REORDER_NO_CHANGE &&
        session.input_latch == 10 && session.changes == 0, "wrong-phase guard");
    check(nba97_reorder_first_callback(nullptr, 0) == NBA97_REORDER_NO_CHANGE, "null guard");
    pass("first_callback_guards", "empty confirm; child requests unconsumed; wrong phase/null are inert");
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
}

void database(const std::string& path) {
    nba97::RosterDatabase db;
    db.load(path);
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
                 "commands: up down select back continue yes no view compare (EOF aborts without publishing)\n"
                 "label layer from local ZFONT0.PSH: .local/verification/reorder/cli_labels.ppm\n";
    while (session.phase != NBA97_REORDER_CLOSED) {
        writeLabels(preview.render(session, db), "cli_labels.ppm");
        const int active = session.phase == NBA97_REORDER_REPLACEMENT ? 1 : 0;
        std::cout << "REORDER STATE phase=" << nba97_reorder_phase_name(session.phase)
                  << " source=" << static_cast<int>(session.cursor[0])
                  << " destination=" << static_cast<int>(session.cursor[1])
                  << " changes=" << session.changes << '\n';
        for (int slot = session.top[active]; slot < session.top[active] + 6; ++slot) {
            const auto* player = db.player(session.slots[slot]);
            std::cout << (slot == session.cursor[active] ? "> " : "  ") << slot << ' '
                      << (player ? player->displayName() : "empty") << '\n';
        }
        std::string command;
        if (!std::getline(std::cin, command)) { std::cout << "REORDER ABORT no changes published\n"; return; }
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
        if (argc == 3) { database(argv[2]); fontAssets(argv[2]); }
        else std::cout << "REORDER SKIP database_invariants | local database not supplied\n";
        std::cout << "REORDER CORE PASS | UI/audio/persistence/original-trace comparison still pending\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "REORDER FAIL " << error.what() << '\n';
        return 1;
    }
}
