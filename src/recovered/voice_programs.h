#ifndef NBA97_VOICE_PROGRAMS_H
#define NBA97_VOICE_PROGRAMS_H
#include "voice_handles.h"
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
    /* Upload's original a2 points to a stack-local output word. The native
     * backend uses this field instead; argument[2] is0, never a fake address.
     * 919A0 ignores its contents after upload, including on negative return. */
    uint32_t auxiliary;
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
    NBA97_PROGRAM_COMPLETE=1,NBA97_PROGRAM_ARGUMENT=0,NBA97_PROGRAM_IO_REFUSED=-1
};
/* Whole93098,9180C+92B74,919A0+92628. Result.value is the original signed
 * return only on COMPLETE. Refusal preserves preceding source mutations and
 * is not resumable. Opaque memory tokens use original32-bit wrapping; the
 * memory backend must refuse unowned ranges rather than dereference a host
 * pointer or invent a readable empty program. */
Nba97VoiceApiResult nba97_voice_bank_validate(Nba97VoicePrograms*,uint32_t bank);
Nba97VoiceApiResult nba97_voice_program_play(Nba97VoicePrograms*,uint32_t bank,
    uint32_t program,uint32_t volume);
Nba97VoiceApiResult nba97_voice_program_register(Nba97VoicePrograms*,uint32_t bank,
    uint32_t* output_program,uint32_t header,uint32_t body);

#ifdef __cplusplus
}
#endif
#endif
