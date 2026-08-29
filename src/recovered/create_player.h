#ifndef NBA97_RECOVERED_CREATE_PLAYER_H
#define NBA97_RECOVERED_CREATE_PLAYER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    NBA97_CREATED_PLAYER_CAPACITY = 40,
    NBA97_CREATED_PLAYER_RECORD_SIZE = 68,
    NBA97_CREATED_PLAYER_FIRST_ID = 493
};

typedef struct Nba97CreatedPlayerRecord {
    uint8_t raw[NBA97_CREATED_PLAYER_RECORD_SIZE];
} Nba97CreatedPlayerRecord;

typedef struct Nba97CreatedPlayerMetadata {
    /* FUN_8004D514 clears exactly 0x0D bytes for each retail name buffer:
       twelve entered characters plus the trailing NUL. */
    char first_name[13];
    char last_name[13];
    uint8_t team;
    /* Port-side membership needed to preserve the retail Delete branches:
       0..4 starter, 5..99 bench/free-list position, FF unassigned. */
    uint8_t roster_slot;
} Nba97CreatedPlayerMetadata;

typedef struct Nba97CreatedPlayerCatalog {
    Nba97CreatedPlayerRecord records[NBA97_CREATED_PLAYER_CAPACITY];
    /* Port-side decoded fields live outside the original 68-byte record until
       their exact retail offsets/encoding are proven. */
    Nba97CreatedPlayerMetadata metadata[NBA97_CREATED_PLAYER_CAPACITY];
} Nba97CreatedPlayerCatalog;

typedef struct Nba97CreateMenuContext {
    int16_t frontend_mode;
    uint8_t global_restriction_clear;
    uint8_t active_roster_context;
    uint8_t new_context_allowed;
} Nba97CreateMenuContext;

typedef struct Nba97CreateMenu {
    uint8_t selected;
    uint8_t enabled[3]; /* edit, new, delete */
    uint16_t created_count;
} Nba97CreateMenu;

typedef struct Nba97CreateEditorTxn {
    Nba97CreatedPlayerRecord working;
    int16_t slot;
    uint8_t active;
    uint8_t is_new;
} Nba97CreateEditorTxn;

typedef enum Nba97CreateField {
    NBA97_CREATE_FIRST_NAME = 0,
    NBA97_CREATE_LAST_NAME,
    NBA97_CREATE_TEAM,
    NBA97_CREATE_JERSEY_NUMBER,
    NBA97_CREATE_POSITION,
    NBA97_CREATE_HAND,
    NBA97_CREATE_HEIGHT,
    NBA97_CREATE_WEIGHT,
    NBA97_CREATE_COLLEGE,
    NBA97_CREATE_YEARS_PRO,
    NBA97_CREATE_SKIN_TONE,
    NBA97_CREATE_HAIR_STYLE,
    NBA97_CREATE_HAIR_COLOR,
    NBA97_CREATE_FACIAL_HAIR,
    NBA97_CREATE_SHOOTING_RANGE,
    NBA97_CREATE_ENDURANCE,
    NBA97_CREATE_FIELD_GOALS,
    NBA97_CREATE_THREE_POINTERS,
    NBA97_CREATE_FREE_THROWS,
    NBA97_CREATE_DUNKING,
    NBA97_CREATE_STEALING,
    NBA97_CREATE_BLOCKING,
    NBA97_CREATE_DEF_AWARENESS,
    NBA97_CREATE_AGILITY,
    NBA97_CREATE_OFF_REBOUNDS,
    NBA97_CREATE_DEF_REBOUNDS,
    NBA97_CREATE_JUMPING,
    NBA97_CREATE_STRENGTH,
    NBA97_CREATE_BALL_HANDLING,
    NBA97_CREATE_OFF_AWARENESS,
    NBA97_CREATE_SPEED,
    NBA97_CREATE_DRIBBLING,
    NBA97_CREATE_FIELD_COUNT
} Nba97CreateField;

typedef struct Nba97CreateEditor {
    Nba97CreateEditorTxn txn;
    uint8_t selected_field;
    uint8_t team;
    uint8_t jersey_number;
    uint8_t position;
    uint8_t hand;
    uint8_t height_inches;
    uint16_t weight_pounds;
    uint16_t college;
    uint8_t years_pro;
    uint8_t skin_tone;
    uint8_t hair_style;
    uint8_t hair_color;
    uint8_t facial_hair;
    uint8_t shooting_range_feet;
    uint8_t ratings[17]; /* endurance, then the 16 detailed ratings */
    char first_name[13];
    char last_name[13];
    /* Presentation state recovered from the banked selector. This remains
       transient and is never serialized into the 68-byte player record. */
    uint8_t visible_first_field;
    uint8_t previous_visible_first_field;
    uint8_t scroll_ticks_remaining;
    uint16_t college_count;
} Nba97CreateEditor;

typedef struct Nba97CreatedPlayerPicker {
    int16_t slots[NBA97_CREATED_PLAYER_CAPACITY];
    uint8_t count;
    uint8_t visible_count;
    uint8_t cursor;
    uint8_t top;
    uint8_t frontend_state; /* recovered 0x20 Edit, 0x21 Delete */
} Nba97CreatedPlayerPicker;

void nba97_created_catalog_init(Nba97CreatedPlayerCatalog* catalog);
uint16_t nba97_created_player_id(const Nba97CreatedPlayerRecord* record);
int nba97_created_player_occupied(const Nba97CreatedPlayerRecord* record);
uint16_t nba97_created_count(const Nba97CreatedPlayerCatalog* catalog);
int16_t nba97_created_first_free(const Nba97CreatedPlayerCatalog* catalog);

/* FUN_80057BDC: availability of the parent Rosters Create Players card. */
int nba97_create_parent_available(const Nba97CreatedPlayerCatalog* catalog,
                                  Nba97CreateMenuContext context);
/* FUN_8004DA74: availability of the New card inside Create Player. */
int nba97_create_new_available(const Nba97CreatedPlayerCatalog* catalog,
                               Nba97CreateMenuContext context);
void nba97_create_menu_open(Nba97CreateMenu* menu,
                            const Nba97CreatedPlayerCatalog* catalog,
                            Nba97CreateMenuContext context);
int nba97_create_menu_move(Nba97CreateMenu* menu, int direction);
const char* nba97_create_menu_status(const Nba97CreateMenu* menu,
                                     char* output, size_t output_size);

int nba97_create_editor_begin_new(Nba97CreateEditorTxn* txn,
                                  const Nba97CreatedPlayerCatalog* catalog);
int nba97_create_editor_begin_edit(Nba97CreateEditorTxn* txn,
                                   const Nba97CreatedPlayerCatalog* catalog,
                                   int16_t slot);
void nba97_create_editor_cancel(Nba97CreateEditorTxn* txn);
int nba97_create_editor_accept(Nba97CreateEditorTxn* txn,
                               Nba97CreatedPlayerCatalog* catalog);
int nba97_created_delete(Nba97CreatedPlayerCatalog* catalog, int16_t slot);

int nba97_create_editor_open_new(Nba97CreateEditor* editor,
                                 const Nba97CreatedPlayerCatalog* catalog);
int nba97_create_editor_open_edit(Nba97CreateEditor* editor,
                                  const Nba97CreatedPlayerCatalog* catalog,
                                  int16_t slot);
int nba97_create_editor_move(Nba97CreateEditor* editor, int direction);
int nba97_create_editor_adjust(Nba97CreateEditor* editor, int direction);
void nba97_create_editor_set_college_count(Nba97CreateEditor* editor,
                                           uint16_t college_count);
void nba97_create_editor_tick(Nba97CreateEditor* editor);
int nba97_create_editor_append_letter(Nba97CreateEditor* editor, char letter);
int nba97_create_editor_backspace(Nba97CreateEditor* editor);
int nba97_create_editor_valid(const Nba97CreateEditor* editor);
int nba97_create_editor_save(Nba97CreateEditor* editor,
                             Nba97CreatedPlayerCatalog* catalog);
const char* nba97_create_field_name(uint8_t field);
int nba97_create_editor_value(const Nba97CreateEditor* editor,
                              char* output, size_t output_size);
int nba97_created_picker_open(Nba97CreatedPlayerPicker* picker,
                              const Nba97CreatedPlayerCatalog* catalog,
                              uint8_t frontend_state);
int nba97_created_picker_move(Nba97CreatedPlayerPicker* picker, int direction);
int16_t nba97_created_picker_slot(const Nba97CreatedPlayerPicker* picker);

#ifdef __cplusplus
}
#endif

#endif
