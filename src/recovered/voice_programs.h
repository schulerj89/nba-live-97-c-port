#ifndef NBA97_VOICE_PROGRAMS_H
#define NBA97_VOICE_PROGRAMS_H
#include "voice_handles.h"
#include "voice_mapping_table.h"
#ifdef __cplusplus
extern "C" {
#endif

enum Nba97VoiceProgramCall {
    NBA97_PROGRAM_READ8=1,NBA97_PROGRAM_READ16,NBA97_PROGRAM_READ32,
    NBA97_PROGRAM_WRITE32,
    NBA97_PROGRAM_UPLOAD_PATL_924B4,NBA97_PROGRAM_UPLOAD_PT_921F4,
    NBA97_PROGRAM_PLAY_PATL_9267C,NBA97_PROGRAM_PLAY_PT_91CD8
};
typedef struct Nba97VoiceProgramRequest {
    uint32_t argument[8];
    /* SAME retained caller table passed through92628/924B4/70884. Native
     * argument[2] stays0; no source stack address is invented. Other requests
     * carry NULL. The callback may change bytes/knownness, not this pointer
     * or the table metadata. There is no scalar-output compatibility mode. */
    Nba97VoiceMappingTable* mapping_table;
} Nba97VoiceProgramRequest;
/* Required owned-memory/platform boundary. READ uses argument0=source token;
 * WRITE32 uses argument0=token,argument1=value. Return1 only after the requested
 * operation completed; set result to actual read/original-call return bits.
 * Reads must not mutate source state. Upload/play callbacks may mutate live
 * bank/voice state but must retain all borrowed objects for this invocation.
 * No successful upload/launch stub is permitted. */
typedef int (*Nba97VoiceProgramInvoke)(void*,enum Nba97VoiceProgramCall,
    Nba97VoiceProgramRequest*,uint32_t* result);
typedef struct Nba97VoicePrograms {
    Nba97VoiceHandles* shared; /* SAME enabled field used by status/service */
    uint32_t* banks; /* SAME10 D99E4+i*8 header pointer tokens */
    Nba97VoiceProgramInvoke call;
    void* context;
} Nba97VoicePrograms;
enum Nba97VoiceProgramCompletion {
    NBA97_PROGRAM_COMPLETE=1,NBA97_PROGRAM_ARGUMENT=0,NBA97_PROGRAM_IO_REFUSED=-1,
    NBA97_PROGRAM_TABLE_RESOURCE=-2,NBA97_PROGRAM_TABLE_METADATA=-3
};
/* Whole93098,9180C+92B74,919A0+92628. Result.value is the original signed
 * return only on COMPLETE. Refusal preserves preceding source mutations and
 * is not resumable. Opaque memory tokens use original32-bit wrapping; the
 * memory backend must refuse unowned ranges rather than dereference a host
 * pointer or invent a readable empty program. */
Nba97VoiceApiResult nba97_voice_bank_validate(Nba97VoicePrograms*,uint32_t bank);
Nba97VoiceApiResult nba97_voice_program_play(Nba97VoicePrograms*,uint32_t bank,
    uint32_t program,uint32_t volume);
/* Registration borrows exactly24 incoming caller-stack bytes/knownness.
 * Validation and ONLY the first-word sentinel store occur after vacancy,
 * before tag dispatch. Invalid bank/input/full-bank paths do not use table. */
Nba97VoiceApiResult nba97_voice_program_register(Nba97VoicePrograms*,uint32_t bank,
    uint32_t* output_program,uint32_t header,uint32_t body,Nba97VoiceMappingTable*);

#ifdef __cplusplus
}
#endif
#endif
