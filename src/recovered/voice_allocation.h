#ifndef NBA97_VOICE_ALLOCATION_H
#define NBA97_VOICE_ALLOCATION_H
#include "voice_handles.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Additional fields of the SAME F06B8+i*68 records. linked[] projects bytes
 * +4..+10 written by this owner for up to8 selected voices; they are not
 * F0D58 physical-channel kind/paired metadata. Do not create duplicate voice
 * active/handle/service-clock state when composing these owners. */
typedef struct Nba97VoiceAllocationRecord {
    uint32_t age; /* record+16, sampled from shared clock.services/D9CDC */
    uint8_t priority; /* record+15 */
    uint8_t linked[7];
} Nba97VoiceAllocationRecord;
typedef struct Nba97VoiceAllocation {
    Nba97VoiceHandles* shared;
    Nba97VoiceAllocationRecord* records; /* exactly24, same record identities */
    uint32_t* generation; /* GP+178/D9BAC, shared source producer word */
    uint32_t* membership_count; /* GP+254/D9C88: NOT requested count */
    uint8_t* selected; /* actual8 scratch bytes D9C8C..D9C93 */
} Nba97VoiceAllocation;
enum Nba97VoiceAllocationCompletion {
    NBA97_ALLOCATION_COMPLETE=1,NBA97_ALLOCATION_ARGUMENT=0,
    NBA97_ALLOCATION_TIMER_TRAP=-1,NBA97_ALLOCATION_UNOWNED_SLOT=-2,
    NBA97_ALLOCATION_UNOWNED_SCRATCH=-3
};
/* Complete91338+912E8 CPU control flow within the explicitly owned scratch
 * and24 records. output_handle is the caller-owned word (normally D9C94),
 * distinct from the eight scratch bytes/other projected fields. It can be
 * written even when the source return is -9. Native completion must be
 * checked before using value, which is the original signed return.
 * Refusal/trap preserves the source mutation prefix and is not resumable.
 * Never synthesize status success, repair stale handles, synchronize the
 * separate membership_count, or roll back a failed original allocation. */
Nba97VoiceApiResult nba97_voice_allocate(Nba97VoiceAllocation*,uint32_t mask,
    uint32_t requested_count,uint32_t priority,uint32_t* output_handle);

#ifdef __cplusplus
}
#endif
#endif
