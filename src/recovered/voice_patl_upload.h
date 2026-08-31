#ifndef NBA97_VOICE_PATL_UPLOAD_H
#define NBA97_VOICE_PATL_UPLOAD_H
#include "voice_handles.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97VoicePatlSpan {
    uint8_t* data;
    uint8_t* known; /* optional size-byte mask, each entry exactly0/1 */
    size_t size;
    uint32_t source_address; /* proven encoded32-bit address, not host pointer */
    uint8_t source_address_known, writable, fully_known; /* each exactly0/1 */
} Nba97VoicePatlSpan;
typedef struct Nba97VoicePatlMemory {
    const Nba97VoicePatlSpan* spans;
    size_t count;
} Nba97VoicePatlMemory;
enum Nba97VoicePatlCall {
    NBA97_PATL_UPLOAD_MAPPING_70884=1,NBA97_PATL_UNLOAD_MAPPING_714B8
};
/* Required synchronous mapping/backend boundary. Return1 only after executing
 * the operation, and set result to the original signed return bits. Upload
 * passes its real shared auxiliary output word; unload passes NULL and body0.
 * Use this same memory registry for mapping/body reads and metadata writes.
 * Unknown input or unimplemented SPU allocation/transfer must refuse, never
 * return fake success. Callback may change bytes/known masks but may not
 * resize/free storage or change span metadata while an owner is running. */
typedef int (*Nba97VoicePatlInvoke)(void*,const Nba97VoicePatlMemory*,
    enum Nba97VoicePatlCall,uint32_t mapping,uint32_t body,
    uint32_t* auxiliary,uint32_t* result);
typedef struct Nba97VoicePatlUpload {
    Nba97VoicePatlMemory memory;
    Nba97VoicePatlInvoke call;
    void* context;
} Nba97VoicePatlUpload;
enum Nba97VoicePatlCompletion {
    NBA97_PATL_COMPLETE=1,NBA97_PATL_ARGUMENT=0,
    NBA97_PATL_RESOURCE=-1,NBA97_PATL_IO_REFUSED=-2,NBA97_PATL_METADATA=-3
};
/* Owned little-endian memory operations for widths1/2/4. Source-address wrap
 * is performed by CPU owners before lookup; spans themselves must not wrap.
 * Span flags and every mask entry within the requested width are validated
 * first. fully_known=1 asserts all bytes known; a provided mask is checked for
 * consistency as its bytes are reached (NULL mask is also supported). A
 * malformed reached flag/mask refuses as METADATA before unknown-byte refusal
 * or store. Unvisited allocation bytes are not scanned. No mask validation is
 * cached across calls. Reads enforce byte knownness. Writes check metadata without reading old
 * bytes, then mark the affected bytes known. Unknown writable bytes therefore
 * remain supported when a known-mask exists. Ambiguous source-address ranges,
 * unaligned accesses and unowned/read-only writes refuse without mutation.
 * Distinct source-address aliases may share the same native bytes/known mask. */
int nba97_voice_patl_read(const Nba97VoicePatlMemory*,uint32_t address,
    uint32_t width,uint32_t* value);
int nba97_voice_patl_write(const Nba97VoicePatlMemory*,uint32_t address,
    uint32_t width,uint32_t value);
/* Whole924B4 and91AB4. The caller supplies actual retained mutable header/body
 * ownership, not temporary copies disconnected from the registered bank.
 * Refusal retains every prior relocation/backend effect and is not resumable.
 * Source loaded/partial-failure bugs and live pointer/count rereads are kept. */
Nba97VoiceApiResult nba97_voice_patl_upload(Nba97VoicePatlUpload*,
    uint32_t header,uint32_t body,uint32_t* auxiliary);
Nba97VoiceApiResult nba97_voice_patl_unload(Nba97VoicePatlUpload*,uint32_t header);

#ifdef __cplusplus
}
#endif
#endif
