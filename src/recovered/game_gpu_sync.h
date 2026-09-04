#ifndef NBA97_GAME_GPU_SYNC_H
#define NBA97_GAME_GPU_SYNC_H

#include "game_text_objects.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameGpuSyncWord {
    uint32_t word;
    uint32_t known_mask;
} Nba97GameGpuSyncWord;

typedef struct Nba97GameGpuSyncQueueEntry {
    Nba97GameGpuSyncWord handler;
    Nba97GameGpuSyncWord a0;
    Nba97GameGpuSyncWord a1;
} Nba97GameGpuSyncQueueEntry;

typedef struct Nba97GameGpuSyncState {
    uint8_t c55c2_debug_level;
    uint32_t c55bc_debug_callback;
    uint32_t c55b8_dispatch_table;
    uint32_t c55c8_completion_pending;
    uint32_t c55cc_completion_callback;
    uint32_t c5534_i_mask_ptr;
    uint32_t c5694_gpu_status_ptr;
    uint32_t c5698_gpu_read_ptr;
    uint32_t c56a0_dma2_chcr_ptr;
    uint32_t c56b0_dpcr_ptr;
    uint32_t c5714_timer_status_ptr;
    uint32_t c5718_timer_counter_ptr;
    uint32_t c56b4_last_handler;
    uint32_t c56b8_last_a0;
    uint32_t c56bc_last_a1;
    uint32_t c56c4_queue_write;
    uint32_t c56c8_queue_read;
    uint32_t c56d0_saved_i_mask;
    uint32_t c56d4_reset_i_mask;
    uint32_t c56d8_deadline;
    uint32_t c56dc_poll_count;
    uint32_t c5574_tick;
    uint32_t c571c_timer_origin;
    uint32_t c5720_timer_base;
    Nba97GameGpuSyncQueueEntry queue[64];
} Nba97GameGpuSyncState;

typedef struct Nba97GameGpuSyncAccess {
    uint32_t pc;
    uint32_t address;
    uint8_t width;
} Nba97GameGpuSyncAccess;

typedef struct Nba97GameGpuSyncWrite {
    uint32_t pc;
    uint32_t address;
    uint8_t width;
    Nba97GameGpuSyncWord value;
} Nba97GameGpuSyncWrite;

enum Nba97GameGpuSyncCallKind {
    NBA97_GAME_GPU_SYNC_CALL_DEBUG = 1,
    NBA97_GAME_GPU_SYNC_CALL_QUEUE_HANDLER,
    NBA97_GAME_GPU_SYNC_CALL_COMPLETION,
    NBA97_GAME_GPU_SYNC_CALL_DIAGNOSTIC,
    NBA97_GAME_GPU_SYNC_CALL_WAIT_9863C
};

typedef struct Nba97GameGpuSyncCall {
    uint32_t pc;
    uint32_t entry;
    uint32_t arguments[5];
    uint8_t argument_count;
    uint8_t kind;
} Nba97GameGpuSyncCall;

typedef struct Nba97GameGpuSyncBackend {
    uint64_t submitted;
    uint64_t completed;
    uint8_t idle;
    uint8_t known;
} Nba97GameGpuSyncBackend;

typedef int (*Nba97GameGpuSyncReadDevice)(void *,
    const Nba97GameGpuSyncAccess *,Nba97GameGpuSyncWord *);
typedef int (*Nba97GameGpuSyncWriteDevice)(void *,
    const Nba97GameGpuSyncWrite *);
typedef int (*Nba97GameGpuSyncResolveDispatch)(void *,uint32_t,uint32_t,
    uint32_t,Nba97GameGpuSyncWord *);
typedef int (*Nba97GameGpuSyncInvoke)(void *,const Nba97GameGpuSyncCall *,
    Nba97GameGpuSyncState *);
typedef int (*Nba97GameGpuSyncObserveBackend)(void *,
    Nba97GameGpuSyncBackend *);

/* Optional retained o32 call-frame mapping for natural source composition.
 * Standalone users may leave this null. When supplied, 800994F4 stores the
 * incoming s0/ra words and reloads both from live mapped bytes, so callbacks
 * may reproduce the original aliasing behavior rather than receiving a
 * sanitized native frame. */
typedef struct Nba97GameGpuSyncAbi {
    Nba97GameTextMemory memory;
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t saved_register_s0;
} Nba97GameGpuSyncAbi;

typedef struct Nba97GameGpuSyncContext {
    Nba97GameGpuSyncReadDevice read_device;
    Nba97GameGpuSyncWriteDevice write_device;
    Nba97GameGpuSyncResolveDispatch resolve_dispatch;
    Nba97GameGpuSyncInvoke invoke;
    Nba97GameGpuSyncObserveBackend observe_backend;
    void *user;
    size_t poll_budget;
    size_t source_step_budget;
    Nba97GameGpuSyncAbi *abi;
} Nba97GameGpuSyncContext;

typedef struct Nba97GameGpuSyncProgress {
    size_t device_reads;
    size_t device_writes;
    size_t calls;
    size_t dispatch_resolutions;
    size_t backend_observations;
    size_t gpu_polls;
    size_t source_steps;
    size_t stack_reads;
    size_t stack_writes;
    uint64_t queued_through;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t restored_return_address;
    uint32_t restored_saved_register_s0;
    uint8_t source_completed;
    uint8_t synchronized;
    uint8_t source_timed_out;
    uint8_t abi_completed;
} Nba97GameGpuSyncProgress;

enum Nba97GameGpuSyncStatus {
    NBA97_GAME_GPU_SYNC_OK = 1,
    NBA97_GAME_GPU_SYNC_ARGUMENT = 0,
    NBA97_GAME_GPU_SYNC_UNKNOWN = -1,
    NBA97_GAME_GPU_SYNC_READ_REQUIRED = -2,
    NBA97_GAME_GPU_SYNC_WRITE_REQUIRED = -3,
    NBA97_GAME_GPU_SYNC_RESOLVE_REQUIRED = -4,
    NBA97_GAME_GPU_SYNC_INVOKE_REQUIRED = -5,
    NBA97_GAME_GPU_SYNC_OBSERVE_REQUIRED = -6,
    NBA97_GAME_GPU_SYNC_DYNAMIC_DISPATCH = -7,
    NBA97_GAME_GPU_SYNC_POLL_BUDGET = -8,
    NBA97_GAME_GPU_SYNC_SOURCE_BUDGET = -9,
    NBA97_GAME_GPU_SYNC_DEVICE_INCOMPLETE = -10,
    NBA97_GAME_GPU_SYNC_STACK_RESOURCE = -11,
    NBA97_GAME_GPU_SYNC_STACK_UNKNOWN = -12,
    NBA97_GAME_GPU_SYNC_STACK_ALIGNMENT = -13
};

/* Canonical recovered owner for GAME 800994F4..8009955F (PsyQ DrawSync) and
 * its default 8009B9B4
 * dispatch closure: 8009BAFC, 8009B57C, 8009BB30, reached 8009BDB4(-1),
 * and 800986F8. GAMEONLY startup calls DrawSync(0) at 80029AAC immediately
 * after its two MoveImage submissions. Native status is separate from source
 * V0. Unknown/refused leaves preserve every earlier source-ordered effect. */
int nba97_game_gpu_sync(Nba97GameGpuSyncContext *,Nba97GameGpuSyncState *,
    uint32_t mode,Nba97GameGpuSyncWord *source_v0,
    Nba97GameGpuSyncProgress *);

#ifdef __cplusplus
}
#endif
#endif
