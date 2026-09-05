#ifndef NBA97_GAME_RESOURCE_VALIDATOR_INSTALL_H
#define NBA97_GAME_RESOURCE_VALIDATOR_INSTALL_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameResourceValidatorInstallContext {
    Nba97GameTextMemory memory;
    size_t operation_budget;
} Nba97GameResourceValidatorInstallContext;

typedef struct Nba97GameResourceValidatorInstallProgress {
    size_t operations;
    size_t accesses;
    size_t stores;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t callback_global;
    uint32_t installed_callback;
    uint32_t return_v0;
    uint8_t return_v0_known;
    uint8_t completed;
} Nba97GameResourceValidatorInstallProgress;

/* Original GAMEONLY subroutine 0x800A3E20..0x800A3E37 (6 instructions),
 * called at main PC 0x80029ABC immediately after SetDispMask(1). It stores
 * resource-completion callback 0x800A3D60 at 0x800D7B1C. The callback is the
 * GAMEONLY counterpart of FEONLY's whole-file CRCF validator; executing that
 * callback remains a separate function boundary.
 *
 * Source quirks are retained: the previous callback is overwritten without a
 * read or guard, no callback runs here, and v0 incidentally returns the newly
 * formed callback address. This changes mapped PS1 loader state only; it does
 * not validate a file, perform host I/O, or alter pixels. Returns NBA97_TEXT_*.
 */
int nba97_game_resource_validator_install(
    Nba97GameResourceValidatorInstallContext*,
    Nba97GameResourceValidatorInstallProgress*);

#ifdef __cplusplus
}
#endif
#endif
