#ifndef NBA97_GAME_CD_DIRECTORY_INITIALIZE_H
#define NBA97_GAME_CD_DIRECTORY_INITIALIZE_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameCdDirectoryInitializeEventKind {
    NBA97_CD_DIRECTORY_INITIALIZE_CALL = 1,
    NBA97_CD_DIRECTORY_INITIALIZE_POLL = 2
};

typedef struct Nba97GameCdDirectoryInitializeValue {
    uint32_t word;
    uint8_t known;
} Nba97GameCdDirectoryInitializeValue;

typedef struct Nba97GameCdDirectoryInitializeEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t address;
    uint32_t argument[2];
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameCdDirectoryInitializeEvent;

/* CALL events are the ten original child-call boundaries. POLL is an explicit
 * opportunity for the asynchronous CD service to update 0x80103551 after the
 * source has observed 0xFF. A callback may mutate mapped bytes/knownness and
 * must return 1 only after carrying out that boundary. Child return values may
 * be unknown; the owner propagates that knownness through the original stores. */
typedef int (*Nba97GameCdDirectoryInitializeIo)(void*,
    const Nba97GameTextMemory*,
    const Nba97GameCdDirectoryInitializeEvent*,
    Nba97GameCdDirectoryInitializeValue*);

typedef struct Nba97GameCdDirectoryInitializeContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus completed callbacks. */
    size_t poll_budget;      /* Maximum observed 0xFF status bytes. */
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t frame_pointer;
    uint32_t global_pointer;
    Nba97GameCdDirectoryInitializeIo io;
    void* user;
} Nba97GameCdDirectoryInitializeContext;

typedef struct Nba97GameCdDirectoryInitializeProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t calls_completed;
    size_t poll_callbacks_completed;
    size_t polls;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t restored_return_address;
    uint32_t restored_frame_pointer;
    uint32_t disc_base_sector;
    uint32_t primary_volume_sector;
    uint32_t root_directory_lba;
    uint32_t root_directory_size;
    uint32_t return_v0;
    uint8_t disc_base_sector_known;
    uint8_t primary_volume_sector_known;
    uint8_t root_directory_lba_known;
    uint8_t root_directory_size_known;
    uint8_t cached;
    uint8_t completed;
} Nba97GameCdDirectoryInitializeProgress;

/* Original GAMEONLY subroutine 0x80091C08..0x80091DDF (118 instructions),
 * called at 0x800299D8 after gameplay-heap initialization. On a cold entry it
 * starts the CD service, waits for the 0x80103550 descriptor buffer, converts
 * its location twice, reads the ISO-9660 root record at offsets 0x9E/0xA6,
 * publishes LBA/length at 0x800D7D3C/40, and sets cache flag 0x800C4ABC.
 * The warm entry returns 1 without child calls. A failed 0x8009FA6C returns 0.
 * Stack spills and source-ordered partial effects are retained. Child routines,
 * the CD device and interrupt progress remain required callbacks. Returns
 * NBA97_TEXT_*; return_v0 is meaningful after a completed source return. */
int nba97_game_cd_directory_initialize(
    Nba97GameCdDirectoryInitializeContext*,
    Nba97GameCdDirectoryInitializeProgress*);

#ifdef __cplusplus
}
#endif
#endif
