#include "create_player.h"

#include "semantic_trace.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

static uint16_t read_u16le(const uint8_t* bytes) {
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static void write_u16le(uint8_t* bytes, uint16_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

void nba97_created_catalog_init(Nba97CreatedPlayerCatalog* catalog) {
    int index;
    if (catalog == NULL) return;
    memset(catalog, 0, sizeof(*catalog));
    for (index = 0; index < NBA97_CREATED_PLAYER_CAPACITY; ++index) {
        write_u16le(catalog->records[index].raw, UINT16_MAX);
        catalog->metadata[index].roster_slot = UINT8_MAX;
    }
}

uint16_t nba97_created_player_id(const Nba97CreatedPlayerRecord* record) {
    return record == NULL ? UINT16_MAX : read_u16le(record->raw);
}

int nba97_created_player_occupied(const Nba97CreatedPlayerRecord* record) {
    return nba97_created_player_id(record) != UINT16_MAX;
}

uint16_t nba97_created_count(const Nba97CreatedPlayerCatalog* catalog) {
    uint16_t count = 0;
    int index;
    nba97_semantic_trace_record(0x8004AEBCu);
    if (catalog == NULL) return 0;
    for (index = 0; index < NBA97_CREATED_PLAYER_CAPACITY; ++index) {
        if (nba97_created_player_occupied(&catalog->records[index])) ++count;
    }
    return count;
}

int16_t nba97_created_first_free(const Nba97CreatedPlayerCatalog* catalog) {
    int16_t index;
    if (catalog == NULL) return -1;
    for (index = 0; index < NBA97_CREATED_PLAYER_CAPACITY; ++index) {
        if (!nba97_created_player_occupied(&catalog->records[index])) return index;
    }
    return -1;
}

int nba97_create_parent_available(const Nba97CreatedPlayerCatalog* catalog,
                                  Nba97CreateMenuContext context) {
    nba97_semantic_trace_record(0x80057BDCu);
    if (context.frontend_mode == 2 && context.global_restriction_clear) {
        if (!context.active_roster_context) return 1;
        return nba97_created_count(catalog) != 0;
    }
    return 1;
}

int nba97_create_new_available(const Nba97CreatedPlayerCatalog* catalog,
                               Nba97CreateMenuContext context) {
    nba97_semantic_trace_record(0x8004DA74u);
    if (!context.new_context_allowed ||
        nba97_created_count(catalog) == NBA97_CREATED_PLAYER_CAPACITY)
        return 0;
    if (context.frontend_mode == 2) return !context.active_roster_context;
    return 1;
}

void nba97_create_menu_open(Nba97CreateMenu* menu,
                            const Nba97CreatedPlayerCatalog* catalog,
                            Nba97CreateMenuContext context) {
    uint16_t count;
    if (menu == NULL) return;
    nba97_semantic_trace_record(0x8004DAE8u);
    count = nba97_created_count(catalog);
    menu->created_count = count;
    menu->enabled[0] = (uint8_t)(count != 0);
    menu->enabled[1] = (uint8_t)nba97_create_new_available(catalog, context);
    menu->enabled[2] = (uint8_t)(count != 0);
    /* The original empty catalogue opens on New, as captured on hardware. */
    menu->selected = menu->enabled[1] ? 1u : (menu->enabled[0] ? 0u : 2u);
}

int nba97_create_menu_move(Nba97CreateMenu* menu, int direction) {
    int candidate;
    int step;
    if (menu == NULL || direction == 0) return 0;
    step = direction < 0 ? -1 : 1;
    for (candidate = (int)menu->selected + step;
         candidate >= 0 && candidate < 3; candidate += step) {
        if (menu->enabled[candidate]) {
            menu->selected = (uint8_t)candidate;
            return 1;
        }
    }
    return 0;
}

const char* nba97_create_menu_status(const Nba97CreateMenu* menu,
                                     char* output, size_t output_size) {
    unsigned count;
    unsigned free_count;
    if (output == NULL || output_size == 0) return "";
    nba97_semantic_trace_record(0x8004D80Cu);
    if (menu == NULL) {
        output[0] = '\0';
        return output;
    }
    count = menu->created_count;
    free_count = NBA97_CREATED_PLAYER_CAPACITY - count;
    if (menu->selected == 1) {
        if (free_count == 1)
            snprintf(output, output_size, "there is 1 more create player slot free.");
        else
            snprintf(output, output_size, "there are %u create player slots free.", free_count);
    } else if (menu->selected == 0) {
        if (count == 1)
            snprintf(output, output_size, "there is one created player to edit.");
        else
            snprintf(output, output_size, "there are %u created players to edit.", count);
    } else {
        if (count == 1)
            snprintf(output, output_size, "there is one created player to delete.");
        else
            snprintf(output, output_size, "there are %u created players to delete.", count);
    }
    return output;
}

int nba97_create_editor_begin_new(Nba97CreateEditorTxn* txn,
                                  const Nba97CreatedPlayerCatalog* catalog) {
    int16_t slot;
    if (txn == NULL) return 0;
    nba97_semantic_trace_record(0x8004D514u);
    slot = nba97_created_first_free(catalog);
    if (slot < 0) return 0;
    memset(txn, 0, sizeof(*txn));
    write_u16le(txn->working.raw, UINT16_MAX);
    txn->slot = slot;
    txn->active = 1;
    txn->is_new = 1;
    return 1;
}

int nba97_create_editor_begin_edit(Nba97CreateEditorTxn* txn,
                                   const Nba97CreatedPlayerCatalog* catalog,
                                   int16_t slot) {
    if (txn == NULL || catalog == NULL || slot < 0 ||
        slot >= NBA97_CREATED_PLAYER_CAPACITY ||
        !nba97_created_player_occupied(&catalog->records[slot])) return 0;
    nba97_semantic_trace_record(0x8004D514u);
    memset(txn, 0, sizeof(*txn));
    txn->working = catalog->records[slot];
    txn->slot = slot;
    txn->active = 1;
    return 1;
}

void nba97_create_editor_cancel(Nba97CreateEditorTxn* txn) {
    if (txn == NULL) return;
    txn->active = 0;
}

int nba97_create_editor_accept(Nba97CreateEditorTxn* txn,
                               Nba97CreatedPlayerCatalog* catalog) {
    if (txn == NULL || catalog == NULL || !txn->active || txn->slot < 0 ||
        txn->slot >= NBA97_CREATED_PLAYER_CAPACITY) return 0;
    nba97_semantic_trace_record(0x8004D328u);
    write_u16le(txn->working.raw,
                (uint16_t)(NBA97_CREATED_PLAYER_FIRST_ID + txn->slot));
    catalog->records[txn->slot] = txn->working;
    txn->active = 0;
    return 1;
}

int nba97_created_delete(Nba97CreatedPlayerCatalog* catalog, int16_t slot) {
    if (catalog == NULL || slot < 0 || slot >= NBA97_CREATED_PLAYER_CAPACITY ||
        !nba97_created_player_occupied(&catalog->records[slot])) return 0;
    memset(&catalog->records[slot], 0, sizeof(catalog->records[slot]));
    write_u16le(catalog->records[slot].raw, UINT16_MAX);
    memset(&catalog->metadata[slot], 0, sizeof(catalog->metadata[slot]));
    catalog->metadata[slot].roster_slot = UINT8_MAX;
    return 1;
}

static uint8_t clamp_u8(int value, int minimum, int maximum) {
    if (value < minimum) return (uint8_t)minimum;
    if (value > maximum) return (uint8_t)maximum;
    return (uint8_t)value;
}

static uint8_t visible_bank_for_field(uint8_t field) {
    int first;
    if (field < NBA97_CREATE_FIELD_GOALS) {
        first = 2;
        if (field >= 2) first = (int)field - 3;
        if (first < 2) first = 2;
        if (first > NBA97_CREATE_ENDURANCE - 3)
            first = NBA97_CREATE_ENDURANCE - 3;
        return (uint8_t)first;
    }
    return (uint8_t)(NBA97_CREATE_FIELD_GOALS +
        (((int)field - NBA97_CREATE_FIELD_GOALS) / 4) * 4);
}

static void initialize_presentation(Nba97CreateEditor* editor) {
    editor->visible_first_field = visible_bank_for_field(editor->selected_field);
    editor->previous_visible_first_field = editor->visible_first_field;
    editor->scroll_ticks_remaining = 0;
}

int nba97_create_editor_open_new(Nba97CreateEditor* editor,
                                 const Nba97CreatedPlayerCatalog* catalog) {
    int rating;
    if (editor == NULL) return 0;
    memset(editor, 0, sizeof(*editor));
    if (!nba97_create_editor_begin_new(&editor->txn, catalog)) return 0;
    editor->height_inches = 63;       /* original default: 5'3\" */
    editor->weight_pounds = 200;
    editor->shooting_range_feet = 8;
    initialize_presentation(editor);
    for (rating = 0; rating < 17; ++rating) editor->ratings[rating] = 50;
    return 1;
}

int nba97_create_editor_open_edit(Nba97CreateEditor* editor,
                                  const Nba97CreatedPlayerCatalog* catalog,
                                  int16_t slot) {
    int index;
    const uint8_t* raw;
    if (editor == NULL || catalog == NULL) return 0;
    memset(editor, 0, sizeof(*editor));
    if (!nba97_create_editor_begin_edit(&editor->txn, catalog, slot)) return 0;
    raw = editor->txn.working.raw;
    editor->college = read_u16le(raw + 2);
    editor->jersey_number = raw[7];
    editor->position = raw[8];
    editor->height_inches = raw[9];
    editor->weight_pounds = (uint16_t)(raw[10] + 100u);
    editor->hand = raw[13];
    for (index = 0; index < 17; ++index) editor->ratings[index] = raw[14 + index];
    editor->years_pro = raw[31];
    editor->shooting_range_feet = raw[32];
    editor->skin_tone = raw[33];
    editor->hair_style = raw[34];
    editor->hair_color = raw[35];
    editor->facial_hair = raw[36];
    editor->team = catalog->metadata[slot].team;
    snprintf(editor->first_name, sizeof(editor->first_name), "%s",
             catalog->metadata[slot].first_name);
    snprintf(editor->last_name, sizeof(editor->last_name), "%s",
             catalog->metadata[slot].last_name);
    initialize_presentation(editor);
    return 1;
}

int nba97_create_editor_move(Nba97CreateEditor* editor, int direction) {
    int candidate;
    if (editor == NULL || direction == 0) return 0;
    candidate = (int)editor->selected_field + (direction < 0 ? -1 : 1);
    if (candidate < 0 || candidate >= NBA97_CREATE_FIELD_COUNT) return 0;
    /* The original refuses to leave either required name while it is empty. */
    if (direction > 0 && editor->selected_field == NBA97_CREATE_FIRST_NAME &&
        editor->first_name[0] == '\0') return 0;
    if (direction > 0 && editor->selected_field == NBA97_CREATE_LAST_NAME &&
        editor->last_name[0] == '\0') return 0;
    editor->selected_field = (uint8_t)candidate;
    {
        const uint8_t next_first = visible_bank_for_field(editor->selected_field);
        if (next_first != editor->visible_first_field) {
            editor->previous_visible_first_field = editor->visible_first_field;
            editor->visible_first_field = next_first;
            /* Recovered selector tints settle over six vblanks; the list bank
               moves across that same callback window instead of snapping. */
            editor->scroll_ticks_remaining = 6;
        }
    }
    return 1;
}

void nba97_create_editor_set_college_count(Nba97CreateEditor* editor,
                                           uint16_t college_count) {
    if (editor == NULL) return;
    editor->college_count = college_count;
    if (college_count != 0 && editor->college >= college_count)
        editor->college = (uint16_t)(editor->college % college_count);
}

void nba97_create_editor_tick(Nba97CreateEditor* editor) {
    if (editor != NULL && editor->scroll_ticks_remaining != 0)
        --editor->scroll_ticks_remaining;
}

int nba97_create_editor_adjust(Nba97CreateEditor* editor, int direction) {
    int step;
    uint8_t* rating;
    if (editor == NULL || direction == 0) return 0;
    step = direction < 0 ? -1 : 1;
    switch ((Nba97CreateField)editor->selected_field) {
    case NBA97_CREATE_TEAM: editor->team = clamp_u8((int)editor->team + step, 0, 28); break;
    case NBA97_CREATE_JERSEY_NUMBER: editor->jersey_number = clamp_u8((int)editor->jersey_number + step, 0, 99); break;
    case NBA97_CREATE_POSITION: editor->position = clamp_u8((int)editor->position + step, 0, 4); break;
    case NBA97_CREATE_HAND: editor->hand = clamp_u8((int)editor->hand + step, 0, 1); break;
    case NBA97_CREATE_HEIGHT: editor->height_inches = clamp_u8((int)editor->height_inches + step, 63, 90); break;
    case NBA97_CREATE_WEIGHT: {
        int value = (int)editor->weight_pounds + step;
        if (value < 150) value = 150;
        if (value > 350) value = 350;
        editor->weight_pounds = (uint16_t)value;
        break;
    }
    case NBA97_CREATE_COLLEGE: {
        int value;
        if (editor->college_count == 0) return 0;
        value = (int)editor->college + step;
        if (value < 0) value = editor->college_count - 1;
        if (value >= editor->college_count) value = 0;
        editor->college = (uint16_t)value;
        break;
    }
    case NBA97_CREATE_YEARS_PRO: editor->years_pro = clamp_u8((int)editor->years_pro + step, 0, 25); break;
    case NBA97_CREATE_SKIN_TONE: editor->skin_tone = clamp_u8((int)editor->skin_tone + step, 0, 7); break;
    case NBA97_CREATE_HAIR_STYLE: editor->hair_style = clamp_u8((int)editor->hair_style + step, 0, 15); break;
    case NBA97_CREATE_HAIR_COLOR: editor->hair_color = clamp_u8((int)editor->hair_color + step, 0, 7); break;
    case NBA97_CREATE_FACIAL_HAIR: editor->facial_hair = clamp_u8((int)editor->facial_hair + step, 0, 15); break;
    case NBA97_CREATE_SHOOTING_RANGE: editor->shooting_range_feet = clamp_u8((int)editor->shooting_range_feet + step, 8, 35); break;
    default:
        if (editor->selected_field < NBA97_CREATE_ENDURANCE ||
            editor->selected_field >= NBA97_CREATE_FIELD_COUNT) return 0;
        rating = &editor->ratings[editor->selected_field - NBA97_CREATE_ENDURANCE];
        *rating = clamp_u8((int)*rating + step, 0, 99);
        break;
    }
    return 1;
}

static char* selected_name(Nba97CreateEditor* editor, size_t* capacity) {
    if (editor->selected_field == NBA97_CREATE_FIRST_NAME) {
        *capacity = sizeof(editor->first_name);
        return editor->first_name;
    }
    if (editor->selected_field == NBA97_CREATE_LAST_NAME) {
        *capacity = sizeof(editor->last_name);
        return editor->last_name;
    }
    return NULL;
}

int nba97_create_editor_append_letter(Nba97CreateEditor* editor, char letter) {
    size_t capacity = 0;
    char* name;
    size_t length;
    if (editor == NULL || !isalpha((unsigned char)letter)) return 0;
    name = selected_name(editor, &capacity);
    if (name == NULL) return 0;
    length = strlen(name);
    if (length + 1 >= capacity) return 0;
    name[length] = (char)toupper((unsigned char)letter);
    name[length + 1] = '\0';
    return 1;
}

int nba97_create_editor_backspace(Nba97CreateEditor* editor) {
    size_t capacity = 0;
    char* name;
    size_t length;
    if (editor == NULL) return 0;
    name = selected_name(editor, &capacity);
    if (name == NULL) return 0;
    length = strlen(name);
    if (length == 0) return 0;
    name[length - 1] = '\0';
    return 1;
}

int nba97_create_editor_valid(const Nba97CreateEditor* editor) {
    return editor != NULL && editor->txn.active &&
           editor->first_name[0] != '\0' && editor->last_name[0] != '\0';
}

int nba97_create_editor_save(Nba97CreateEditor* editor,
                             Nba97CreatedPlayerCatalog* catalog) {
    int index;
    int16_t slot;
    if (!nba97_create_editor_valid(editor)) return 0;
    slot = editor->txn.slot;
    write_u16le(editor->txn.working.raw + 2, editor->college);
    editor->txn.working.raw[7] = editor->jersey_number;
    editor->txn.working.raw[8] = editor->position;
    editor->txn.working.raw[9] = editor->height_inches;
    editor->txn.working.raw[10] = (uint8_t)(editor->weight_pounds - 100);
    editor->txn.working.raw[13] = editor->hand;
    for (index = 0; index < 17; ++index)
        editor->txn.working.raw[14 + index] = editor->ratings[index];
    editor->txn.working.raw[31] = editor->years_pro;
    editor->txn.working.raw[32] = editor->shooting_range_feet;
    editor->txn.working.raw[33] = editor->skin_tone;
    editor->txn.working.raw[34] = editor->hair_style;
    editor->txn.working.raw[35] = editor->hair_color;
    editor->txn.working.raw[36] = editor->facial_hair;
    {
        const uint8_t was_new = editor->txn.is_new;
        if (!nba97_create_editor_accept(&editor->txn, catalog)) return 0;
        /* New team-assigned players enter the bench. The precise roster
           insertion slot is updated by the owning roster subsystem later. */
        if (was_new) catalog->metadata[slot].roster_slot = 5;
    }
    snprintf(catalog->metadata[slot].first_name,
             sizeof(catalog->metadata[slot].first_name), "%s", editor->first_name);
    snprintf(catalog->metadata[slot].last_name,
             sizeof(catalog->metadata[slot].last_name), "%s", editor->last_name);
    catalog->metadata[slot].team = editor->team;
    return 1;
}

const char* nba97_create_field_name(uint8_t field) {
    static const char* names[NBA97_CREATE_FIELD_COUNT] = {
        "first", "last", "team", "jersey #", "position", "hand", "height",
        "weight", "college", "years pro", "skin tone", "hair style", "hair color",
        "facial hair", "shooting range", "endurance", "field goals", "3 pointers",
        "free throws", "dunking", "stealing", "blocking", "def. awareness", "agility",
        "off. rebounds", "def. rebounds", "jumping", "strength", "ball handling",
        "off. awareness", "speed", "dribbling"
    };
    return field < NBA97_CREATE_FIELD_COUNT ? names[field] : "unknown";
}

int nba97_create_editor_value(const Nba97CreateEditor* editor,
                              char* output, size_t output_size) {
    unsigned field;
    if (editor == NULL || output == NULL || output_size == 0) return 0;
    field = editor->selected_field;
    switch ((Nba97CreateField)field) {
    case NBA97_CREATE_FIRST_NAME: snprintf(output, output_size, "%s", editor->first_name); break;
    case NBA97_CREATE_LAST_NAME: snprintf(output, output_size, "%s", editor->last_name); break;
    case NBA97_CREATE_TEAM: snprintf(output, output_size, "%u", editor->team); break;
    case NBA97_CREATE_JERSEY_NUMBER: snprintf(output, output_size, "%u", editor->jersey_number); break;
    case NBA97_CREATE_POSITION: snprintf(output, output_size, "%u", editor->position); break;
    case NBA97_CREATE_HAND: snprintf(output, output_size, "%s", editor->hand ? "left" : "right"); break;
    case NBA97_CREATE_HEIGHT: snprintf(output, output_size, "%u'%u\"", editor->height_inches / 12, editor->height_inches % 12); break;
    case NBA97_CREATE_WEIGHT: snprintf(output, output_size, "%u lbs", editor->weight_pounds); break;
    case NBA97_CREATE_COLLEGE:
        if (editor->college == 0) {
            snprintf(output, output_size, "n/a");
        } else {
            snprintf(output, output_size, "%u", editor->college);
        }
        break;
    case NBA97_CREATE_YEARS_PRO: snprintf(output, output_size, "%u", editor->years_pro); break;
    case NBA97_CREATE_SKIN_TONE: snprintf(output, output_size, "%u", editor->skin_tone); break;
    case NBA97_CREATE_HAIR_STYLE: snprintf(output, output_size, "%u", editor->hair_style); break;
    case NBA97_CREATE_HAIR_COLOR: snprintf(output, output_size, "%u", editor->hair_color); break;
    case NBA97_CREATE_FACIAL_HAIR: snprintf(output, output_size, "%u", editor->facial_hair); break;
    case NBA97_CREATE_SHOOTING_RANGE: snprintf(output, output_size, "%u ft", editor->shooting_range_feet); break;
    default:
        if (field >= NBA97_CREATE_ENDURANCE && field < NBA97_CREATE_FIELD_COUNT)
            snprintf(output, output_size, "%u", editor->ratings[field - NBA97_CREATE_ENDURANCE]);
        else output[0] = '\0';
        break;
    }
    return 1;
}

int nba97_created_picker_open(Nba97CreatedPlayerPicker* picker,
                              const Nba97CreatedPlayerCatalog* catalog,
                              uint8_t frontend_state) {
    int slot;
    if (picker == NULL || catalog == NULL ||
        (frontend_state != 0x20 && frontend_state != 0x21)) return 0;
    nba97_semantic_trace_record(0x8004E184u);
    memset(picker, 0, sizeof(*picker));
    picker->frontend_state = frontend_state;
    for (slot = 0; slot < NBA97_CREATED_PLAYER_CAPACITY; ++slot)
        if (nba97_created_player_occupied(&catalog->records[slot]))
            picker->slots[picker->count++] = (int16_t)slot;
    picker->visible_count = picker->count < 8 ? picker->count : 7;
    return picker->count != 0;
}

int nba97_created_picker_move(Nba97CreatedPlayerPicker* picker, int direction) {
    int next;
    if (picker == NULL || direction == 0 || picker->count == 0) return 0;
    next = (int)picker->cursor + (direction < 0 ? -1 : 1);
    if (next < 0 || next >= picker->count) return 0;
    picker->cursor = (uint8_t)next;
    if (picker->cursor < picker->top) picker->top = picker->cursor;
    if (picker->cursor >= picker->top + picker->visible_count)
        picker->top = (uint8_t)(picker->cursor - picker->visible_count + 1);
    return 1;
}

int16_t nba97_created_picker_slot(const Nba97CreatedPlayerPicker* picker) {
    if (picker == NULL || picker->count == 0 || picker->cursor >= picker->count) return -1;
    return picker->slots[picker->cursor];
}
