#ifndef NBA97_FRONTEND_RESOURCE_H
#define NBA97_FRONTEND_RESOURCE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* 9045C, valid resident byte spans only; no source table is embedded. */
int nba97_resource_crc16(const uint8_t* data, uint32_t bytes, uint16_t* checksum);

typedef struct Nba97ResourceValidation {
    uint32_t payload_bytes;
    uint32_t calculated_checksum;
    uint32_t stored_checksum;
    uint32_t trailer_present;
} Nba97ResourceValidation;
/* 8AAF0/8AB6C/8ABF0 content decision with source D9B3C=12. Returns1 accepted,
 * 0 rejected, -1 outside the supported span domain (no output mutation).
 * Missing CRCF is accepted iff strict==0. The trailer's own length word is
 * ignored, as in the source. No allocation/free/shrink or queue side effects.
 * Accepted payload_bytes is the original file length minus12 when present. */
int nba97_resource_validate_file(const uint8_t* data, uint32_t bytes,
    uint32_t strict, Nba97ResourceValidation* result);
/* 30E78..30EFC checksum gate, after resolving the CURRENT cache record's raw
 * slice. 1 clears F9720 immediately, BEFORE texture decoding; 0 leaves it
 * unchanged and means queue retry; -1 refuses an invalid native span. This is
 * NOT the full callback's ownership acceptance or a PNG/render event. */
int nba97_portrait_checksum_accept(const uint8_t* data, uint32_t bytes,
    uint32_t* selection_blocked);

typedef struct Nba97CoolIndexLoad {
    uint32_t loaded_data;     /*93708: accepted whole-file data, not handle */
    uint32_t graphics;        /*F1478: zero chooses I/O-only pump */
    uint32_t pending;         /*FDC00 */
    uint32_t voice;           /*DED08: announcer voice, not streamed music */
    uint32_t bank;            /*DE484: announcer program */
    uint32_t sample_data;     /*ECF8C: separate speech allocation */
    uint32_t bank_context;    /*21D6C */
    uint32_t archive_path;    /*FB214 */
} Nba97CoolIndexLoad;
typedef enum Nba97CoolIndexCall {
    NBA97_COOL_INDEX_FREE_DATA,   /*7760C(data), NOT77638(handle) */
    NBA97_COOL_INDEX_DRAIN,       /*393F0; pending whole-file callbacks survive */
    NBA97_COOL_INDEX_REQUEST,     /*2F8F4(index_path,0,400) */
    NBA97_COOL_INDEX_PUMP_UI,     /*3282C */
    NBA97_COOL_INDEX_PUMP_IO,     /*38E84 */
    NBA97_COOL_INDEX_VOICE_STATUS,/*92BFC(voice) */
    NBA97_COOL_INDEX_FADE,        /*7B2BC(voice,100,-1) */
    NBA97_COOL_INDEX_UNLOAD_BANK  /*91B28(bank_context,bank) */
} Nba97CoolIndexCall;
typedef uint32_t (*Nba97CoolIndexInvoke)(void*, Nba97CoolIndexCall,
    uint32_t a0, uint32_t a1, uint32_t a2);
/* Complete3122C scalar/call owner. index_data points to the SAME F84C8 field
 * used by music_transition.resource_handle. uint32 values are native resource
 * tokens, never truncated host pointers. Request must clear loaded_data and
 * publish only a2F870-validated resource; pumps may mutate live state. The
 * original waits have NO timeout: the adapter must ensure eventual progress.
 * Returns1 on completion,0 if required pointers are missing, before effects.
 * Return value is native API status, not a reconstructed source return value. */
int nba97_cool_index_load(Nba97CoolIndexLoad*, uint32_t* index_data,
    uint32_t index_path, uint32_t archive_path, Nba97CoolIndexInvoke, void*);

#ifdef __cplusplus
}
#endif
#endif
