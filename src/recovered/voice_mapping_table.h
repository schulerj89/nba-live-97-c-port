#ifndef NBA97_VOICE_MAPPING_TABLE_H
#define NBA97_VOICE_MAPPING_TABLE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Original upload a2: twelve-byte records, with body offset/FFFFFFFF sentinel
 * at+0 and SPU byte address at+4. Word+8 is not interpreted by70884.
 * Retained aligned, nonwrapping original table; no invented stack address.
 * data/known may alias registered allocations. Metadata/lifetime stay fixed;
 * known=NULL declares all bytes known. Other masks contain canonical0/1.
 * 919A0's local table is24 bytes (sp+10..27). ONLY its first word is
 * initialized at91A4C. Keep the remaining incoming bytes/knownness unchanged.
 * Aliases to executing code/callees' active frames are outside this domain. */
typedef struct Nba97VoiceMappingTable {
    uint8_t* data;
    uint8_t* known;
    size_t size;
} Nba97VoiceMappingTable;
enum Nba97VoiceMappingTableResult {
    NBA97_VOICE_TABLE_COMPLETE=1,NBA97_VOICE_TABLE_ARGUMENT=0,
    NBA97_VOICE_TABLE_RESOURCE=-1,NBA97_VOICE_TABLE_METADATA=-3
};
/* Access one aligned little-endian word, validating all four reached mask
 * entries before unknown checks or any write. No entry/allocation preflight,
 * no old-value read for writes, no caching across calls. Unknown reads refuse;
 * writes establish knownness. Out-of-span access retains previous effects.
 * These helpers do not own progress; callers count each source access once. */
int nba97_voice_mapping_table_read(const Nba97VoiceMappingTable*,uint32_t offset,uint32_t* value);
int nba97_voice_mapping_table_write(Nba97VoiceMappingTable*,uint32_t offset,uint32_t value);
#ifdef __cplusplus
}
#endif
#endif
