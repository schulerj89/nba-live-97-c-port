#ifndef NBA97_VOICE_MAPPING_H
#define NBA97_VOICE_MAPPING_H
#include "voice_patl_upload.h"
#include "voice_mapping_table.h"
#ifdef __cplusplus
extern "C" {
#endif

enum Nba97VoiceMappingCall {
    NBA97_MAPPING_ALLOCATE_7EC2C=1,
    NBA97_MAPPING_TRANSFER_7DC90,
    NBA97_MAPPING_TEST_EVENT_7F568,
    NBA97_MAPPING_FREE_7E56C,
    NBA97_MAPPING_STREAM_RESET_7390C,
    NBA97_MAPPING_STREAM_PRIME_73580,
    /* Scheduling boundary after a nonzero C6D2C read. This is not an original
     * function call: an actual concurrent/service owner must make progress.
     * Never clear the flag merely to let the upload continue. */
    NBA97_MAPPING_WAIT_CHANNEL
};
typedef struct Nba97VoiceMappingEvent {
    enum Nba97VoiceMappingCall call;
    uint32_t a0,a1;
} Nba97VoiceMappingEvent;
/* Required allocator/platform operations over this SAME retained registry.
 * Return1 only after the operation executes and write its actual result bits.
 * TRANSFER must own/read the already-clamped source range, start the real transfer and retain
 * borrowed storage until its real completion event. TEST_EVENT must use that
 * event, not WinMM active/finished state. An unimplemented operation refuses.
 * Callbacks may mutate bytes/knownness, including shared channel/stream fields,
 * but never span/table metadata, progress, or event lifetime. */
typedef int (*Nba97VoiceMappingInvoke)(void*,const Nba97VoicePatlMemory*,
    const Nba97VoiceMappingEvent*,uint32_t* result);
typedef struct Nba97VoiceMapping {
    Nba97VoicePatlMemory memory;
    Nba97VoiceMappingInvoke call;
    void* context;
    size_t step_budget;
} Nba97VoiceMapping;
typedef struct Nba97VoiceMappingProgress {
    size_t steps,callbacks_completed;
    uint32_t stopped_address; /* original address or table-relative offset */
    uint8_t stopped_in_table;
} Nba97VoiceMappingProgress;
enum Nba97VoiceMappingCompletion {
    NBA97_MAPPING_LIMIT=-4, NBA97_MAPPING_TRAP=-5
    /* Other completion values are NBA97_PATL_*. */
};
/* Complete70884/714B8 control, plus reached 7E994/7E9C8/7DDC8/6F7EC/7EA04/
 * 7E898 CPU wrappers. The 7DDC8 mode is -1; it computes the SPU address units
 * without register I/O. Alignment/shift globals remain actual retained data.
 * The registry must alias actual C6D2D stop->changing and C6D2C
 * channels->stream_pending, never duplicate them. Other reached global/source
 * fields likewise require their real shared storage and provenance.
 * Source failure leaves allocated SPU storage, mapping mutations and table
 * writes intact. Native refusal leaves its completed prefix, including the
 * changing flag, and is not resumable. No automatic cleanup is implied. */
Nba97VoiceApiResult nba97_voice_mapping_upload(Nba97VoiceMapping*,
    uint32_t mapping,uint32_t body,Nba97VoiceMappingTable*,Nba97VoiceMappingProgress*);
Nba97VoiceApiResult nba97_voice_mapping_unload(Nba97VoiceMapping*,
    uint32_t mapping,Nba97VoiceMappingProgress*);

#ifdef __cplusplus
}
#endif
#endif
