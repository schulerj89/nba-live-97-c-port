#ifndef NBA97_GAME_HEAP_PAYLOAD_SIZE_H
#define NBA97_GAME_HEAP_PAYLOAD_SIZE_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameHeapPayloadSizeEventKind {
    NBA97_GAME_HEAP_PAYLOAD_SIZE_FIND_DESCRIPTOR = 1
};

typedef struct Nba97GameHeapPayloadSizeValue {
    uint32_t word;
    uint8_t known;
} Nba97GameHeapPayloadSizeValue;

typedef struct Nba97GameHeapPayloadSizeEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[1];
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameHeapPayloadSizeEvent;

/* The callback performs the exact synchronous 0x80090618 descriptor lookup.
 * Returning 1 acknowledges the call; its v0 value may legitimately be zero.
 * Unknown v0 cannot form the following descriptor+0x14 address. The callback
 * may mutate mapped stack and descriptor bytes, but not region metadata. */
typedef int (*Nba97GameHeapPayloadSizeIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameHeapPayloadSizeEvent*,
    Nba97GameHeapPayloadSizeValue*);

typedef struct Nba97GameHeapPayloadSizeContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus admitted child attempts. */
    uint32_t payload;
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t global_pointer;
    Nba97GameHeapPayloadSizeIo io;
    void* user;
} Nba97GameHeapPayloadSizeContext;

typedef struct Nba97GameHeapPayloadSizeProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t descriptor_lookup_calls;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t payload;
    uint32_t descriptor;
    uint32_t requested_size;
    uint32_t restored_return_address;
    uint32_t return_v0;
    uint8_t descriptor_known;
    uint8_t return_v0_known;
    uint8_t completed;
} Nba97GameHeapPayloadSizeProgress;

/* Original GAMEONLY routine 0x80090D60..0x80090D83 (9 instructions).
 * Main reaches it at call PC 0x80029B08 to obtain the feload.bin allocation's
 * original requested size before 0x800AA468 copies/relocates that overlay.
 * A second source caller exists at 0x800A7200.
 *
 * 0x80090D68 calls 0x80090618(payload), then 0x80090D70 unconditionally
 * reads returned_descriptor+0x14. There is no null check: a zero descriptor
 * reads low RAM address 0x00000014. Preserve that original bug and 32-bit
 * address wrapping rather than manufacturing zero or an error. The epilogue
 * reloads ra from mutable stack storage after the size read.
 *
 * Source bytes SHA-256:
 * 665368c63a001c084cd5c009548768ad5db5a385cad175c378e9f10f7ccdaaa0. */
int nba97_game_heap_payload_size(Nba97GameHeapPayloadSizeContext*,
    Nba97GameHeapPayloadSizeProgress*);

#ifdef __cplusplus
}
#endif
#endif
