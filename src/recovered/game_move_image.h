#ifndef NBA97_GAME_MOVE_IMAGE_H
#define NBA97_GAME_MOVE_IMAGE_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameMoveImageEventKind {
    NBA97_GAME_MOVE_IMAGE_DIAGNOSTIC = 1,
    NBA97_GAME_MOVE_IMAGE_GPU_DISPATCH
};

typedef struct Nba97GameMoveImageValue {
    uint32_t word;
    uint8_t known;
} Nba97GameMoveImageValue;

typedef struct Nba97GameMoveImageEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[4];
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t saved_register[3]; /* Live s0, s1, s2 at the boundary. */
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameMoveImageEvent;

/* Each callback acknowledges one complete synchronous source boundary. The
 * diagnostic event represents 0x80099560("MoveImage", rect); its return is
 * ignored. The GPU event invokes the live target loaded from driver-table
 * offset +8 and must perform the represented 20-byte packet operation or
 * refuse. It may mutate mapped bytes/knownness, including the rectangle,
 * shared packet, driver table, and live stack frame. No successful no-op is
 * supplied by this owner. */
typedef int (*Nba97GameMoveImageIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameMoveImageEvent*, Nba97GameMoveImageValue*);

typedef struct Nba97GameMoveImageContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus acknowledged calls. */
    uint32_t rectangle_address;
    uint32_t destination_x;
    uint32_t destination_y;
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t saved_register[3]; /* Incoming s0, s1, s2. */
    uint32_t global_pointer;
    Nba97GameMoveImageIo io;
    void* user;
} Nba97GameMoveImageContext;

typedef struct Nba97GameMoveImageProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t rectangle_address;
    uint32_t requested_destination_x;
    uint32_t requested_destination_y;
    uint32_t packet_address;
    uint32_t source_coordinate_word;
    uint32_t destination_coordinate_word;
    uint32_t extent_word;
    uint32_t driver_table;
    uint32_t dispatch_context;
    uint32_t dispatch_entry;
    uint32_t restored_return_address;
    uint32_t restored_saved_register[3];
    uint32_t return_v0;
    int32_t signed_width;
    int32_t signed_height;
    uint8_t return_v0_known;
    uint8_t width_read;
    uint8_t height_read;
    uint8_t zero_extent_return;
    uint8_t diagnostic_called;
    uint8_t gpu_dispatched;
    uint8_t completed;
} Nba97GameMoveImageProgress;

/* Original GAMEONLY PsyQ MoveImage routine 0x800997E4..0x800998A7
 * (49 instructions). Startup calls it at 0x80029A94 and 0x80029AA4 with
 * RECT(512,0,512,256), copying the right-hand VRAM page first to (0,0), then
 * to (0,256). It writes source/destination/extent words at 0x800C5670..78 and
 * dispatches the enclosing 20-byte packet at 0x800C5668 through the live GPU
 * driver table at 0x800C55B8.
 *
 * Compatibility quirks are intentional: the diagnostic call precedes extent
 * validation; only exactly-zero signed halfwords return -1, so negative sizes
 * still dispatch; destination coordinates truncate to 16 bits; the first two
 * packet words remain untouched; and the indirect target is not null-checked.
 * Returns NBA97_TEXT_* while return_v0 carries the raw SDK result. */
int nba97_game_move_image(Nba97GameMoveImageContext*,
    Nba97GameMoveImageProgress*);

#ifdef __cplusplus
}
#endif
#endif
