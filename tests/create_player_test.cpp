#include "recovered/create_player.h"

#include <cassert>
#include <cstring>
#include <string>

static Nba97CreateMenuContext normal_context() {
    Nba97CreateMenuContext context{};
    context.new_context_allowed = 1;
    return context;
}

int main() {
    Nba97CreatedPlayerCatalog catalog{};
    Nba97CreateMenu menu{};
    Nba97CreateEditorTxn txn{};
    char status[96]{};

    nba97_created_catalog_init(&catalog);
    assert(sizeof(Nba97CreatedPlayerRecord) == 68);
    assert(nba97_created_count(&catalog) == 0);
    assert(nba97_created_first_free(&catalog) == 0);

    nba97_create_menu_open(&menu, &catalog, normal_context());
    assert(menu.selected == 1 && !menu.enabled[0] && menu.enabled[1] && !menu.enabled[2]);
    assert(std::string(nba97_create_menu_status(&menu, status, sizeof(status))) ==
           "there are 40 create player slots free.");
    assert(!nba97_create_menu_move(&menu, -1) && !nba97_create_menu_move(&menu, 1));

    assert(nba97_create_editor_begin_new(&txn, &catalog));
    txn.working.raw[10] = 77;
    nba97_create_editor_cancel(&txn);
    assert(nba97_created_count(&catalog) == 0);

    assert(nba97_create_editor_begin_new(&txn, &catalog));
    txn.working.raw[10] = 77;
    assert(nba97_create_editor_accept(&txn, &catalog));
    assert(nba97_created_count(&catalog) == 1);

    Nba97CreatedPlayerPicker picker{};
    assert(nba97_created_picker_open(&picker, &catalog, 0x20));
    assert(picker.count == 1 && picker.visible_count == 1 &&
           nba97_created_picker_slot(&picker) == 0);
    assert(!nba97_created_picker_move(&picker, -1) &&
           !nba97_created_picker_move(&picker, 1));

    Nba97CreatedPlayerCatalog sparse{};
    nba97_created_catalog_init(&sparse);
    for (int slot : {1, 3, 5, 7, 9, 11, 13, 15, 17}) {
        Nba97CreateEditorTxn sparse_txn{};
        assert(nba97_create_editor_begin_edit(&sparse_txn, &catalog, 0));
        sparse_txn.slot = static_cast<int16_t>(slot);
        assert(nba97_create_editor_accept(&sparse_txn, &sparse));
    }
    assert(nba97_created_picker_open(&picker, &sparse, 0x21));
    assert(picker.count == 9 && picker.visible_count == 7 && picker.top == 0);
    for (int move = 0; move < 8; ++move) assert(nba97_created_picker_move(&picker, 1));
    assert(picker.cursor == 8 && picker.top == 2 && nba97_created_picker_slot(&picker) == 17);
    assert(!nba97_created_picker_move(&picker, 1));
    assert(nba97_created_player_id(&catalog.records[0]) == 493);
    assert(catalog.records[0].raw[10] == 77);

    nba97_create_menu_open(&menu, &catalog, normal_context());
    assert(menu.enabled[0] && menu.enabled[1] && menu.enabled[2]);
    assert(nba97_create_menu_move(&menu, -1) && menu.selected == 0);
    assert(std::string(nba97_create_menu_status(&menu, status, sizeof(status))) ==
           "there is one created player to edit.");
    assert(nba97_create_menu_move(&menu, 1) && menu.selected == 1);
    assert(nba97_create_menu_move(&menu, 1) && menu.selected == 2);
    assert(std::string(nba97_create_menu_status(&menu, status, sizeof(status))) ==
           "there is one created player to delete.");

    assert(nba97_create_editor_begin_edit(&txn, &catalog, 0));
    txn.working.raw[10] = 99;
    nba97_create_editor_cancel(&txn);
    assert(catalog.records[0].raw[10] == 77);
    assert(nba97_create_editor_begin_edit(&txn, &catalog, 0));
    txn.working.raw[10] = 99;
    assert(nba97_create_editor_accept(&txn, &catalog));
    assert(catalog.records[0].raw[10] == 99);

    Nba97CreateMenuContext special = normal_context();
    special.frontend_mode = 2;
    special.global_restriction_clear = 1;
    special.active_roster_context = 1;
    assert(nba97_create_parent_available(&catalog, special));
    assert(!nba97_create_new_available(&catalog, special));
    assert(nba97_created_delete(&catalog, 0));
    assert(!nba97_create_parent_available(&catalog, special));
    special.active_roster_context = 0;
    assert(nba97_create_parent_available(&catalog, special));
    assert(nba97_create_new_available(&catalog, special));

    for (int i = 0; i < NBA97_CREATED_PLAYER_CAPACITY; ++i) {
        assert(nba97_create_editor_begin_new(&txn, &catalog));
        assert(txn.slot == i);
        assert(nba97_create_editor_accept(&txn, &catalog));
        assert(nba97_created_player_id(&catalog.records[i]) ==
               NBA97_CREATED_PLAYER_FIRST_ID + i);
    }
    assert(nba97_created_count(&catalog) == 40);
    assert(nba97_created_first_free(&catalog) == -1);
    assert(!nba97_create_editor_begin_new(&txn, &catalog));
    nba97_create_menu_open(&menu, &catalog, normal_context());
    assert(menu.selected == 0 && menu.enabled[0] && !menu.enabled[1] && menu.enabled[2]);

    nba97_created_catalog_init(&catalog);
    Nba97CreateEditor editor{};
    assert(nba97_create_editor_open_new(&editor, &catalog));
    assert(editor.selected_field == NBA97_CREATE_FIRST_NAME);
    assert(editor.height_inches == 63 && editor.weight_pounds == 200);
    assert(editor.shooting_range_feet == 8);
    for (auto rating : editor.ratings) assert(rating == 50);
    assert(!nba97_create_editor_move(&editor, 1));
    assert(nba97_create_editor_append_letter(&editor, 'a'));
    assert(std::string(editor.first_name) == "A");
    assert(nba97_create_editor_move(&editor, 1));
    assert(!nba97_create_editor_move(&editor, 1));
    assert(nba97_create_editor_append_letter(&editor, 'b'));
    assert(std::string(editor.last_name) == "B");
    assert(nba97_create_editor_move(&editor, 1));
    assert(editor.selected_field == NBA97_CREATE_TEAM);
    assert(nba97_create_editor_adjust(&editor, 1) && editor.team == 1);

    while (editor.selected_field < NBA97_CREATE_HEIGHT)
        assert(nba97_create_editor_move(&editor, 1));
    assert(nba97_create_editor_adjust(&editor, 1) && editor.height_inches == 64);
    while (editor.selected_field < NBA97_CREATE_WEIGHT)
        assert(nba97_create_editor_move(&editor, 1));
    assert(nba97_create_editor_adjust(&editor, 1) && editor.weight_pounds == 201);
    while (editor.selected_field < NBA97_CREATE_SHOOTING_RANGE)
        assert(nba97_create_editor_move(&editor, 1));
    assert(nba97_create_editor_adjust(&editor, 1) && editor.shooting_range_feet == 9);
    while (editor.selected_field < NBA97_CREATE_ENDURANCE)
        assert(nba97_create_editor_move(&editor, 1));
    assert(nba97_create_editor_adjust(&editor, 1) && editor.ratings[0] == 51);
    while (editor.selected_field < NBA97_CREATE_DUNKING)
        assert(nba97_create_editor_move(&editor, 1));
    assert(nba97_create_editor_adjust(&editor, 1) && editor.ratings[4] == 51);
    while (editor.selected_field < NBA97_CREATE_DRIBBLING)
        assert(nba97_create_editor_move(&editor, 1));
    assert(!nba97_create_editor_move(&editor, 1));
    assert(nba97_create_editor_valid(&editor));
    assert(nba97_create_editor_save(&editor, &catalog));
    assert(nba97_created_count(&catalog) == 1);
    const auto& saved = catalog.records[0].raw;
    assert(saved[7] == 0 && saved[8] == 0);
    assert(saved[9] == 64 && saved[10] == 101);
    assert(saved[14] == 51 && saved[18] == 51);
    assert(saved[32] == 9);
    assert(std::string(catalog.metadata[0].first_name) == "A");
    assert(std::string(catalog.metadata[0].last_name) == "B");
    assert(catalog.metadata[0].team == 1);

    Nba97CreateEditor edited{};
    assert(nba97_create_editor_open_edit(&edited, &catalog, 0));
    assert(std::string(edited.first_name) == "A" && std::string(edited.last_name) == "B");
    assert(edited.team == 1 && edited.height_inches == 64 && edited.weight_pounds == 201);
    assert(edited.ratings[0] == 51 && edited.ratings[4] == 51);

    assert(nba97_create_editor_open_new(&editor, &catalog));
    assert(nba97_create_editor_append_letter(&editor, 'x'));
    assert(nba97_create_editor_backspace(&editor));
    assert(editor.first_name[0] == '\0');
    assert(!nba97_create_editor_save(&editor, &catalog));
    nba97_create_editor_cancel(&editor.txn);
    assert(nba97_created_count(&catalog) == 1);

    return 0;
}
