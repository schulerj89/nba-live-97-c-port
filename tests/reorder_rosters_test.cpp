#include "roster_database.hpp"
#include <algorithm>
#include <array>
#include <climits>
#include <iostream>
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
    if (team_id < 0 || team_id >= 29 || !db.team(static_cast<std::uint16_t>(team_id)))
        throw std::runtime_error("team must be 0..28");
    Nba97ReorderSession session{};
    nba97_reorder_begin(&session, db.team(static_cast<std::uint16_t>(team_id))->roster.data());
    std::cout << "REORDER CLI interaction harness; not original screen; no disk saves\n"
                 "commands: up down select back continue yes no (EOF aborts without publishing)\n";
    while (session.phase != NBA97_REORDER_CLOSED) {
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
        if (argc == 3) database(argv[2]);
        else std::cout << "REORDER SKIP database_invariants | local database not supplied\n";
        std::cout << "REORDER CORE PASS | UI/audio/persistence/original-trace comparison still pending\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "REORDER FAIL " << error.what() << '\n';
        return 1;
    }
}
