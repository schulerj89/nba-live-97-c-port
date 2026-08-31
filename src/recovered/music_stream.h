#ifndef NBA97_MUSIC_STREAM_H
#define NBA97_MUSIC_STREAM_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Source726C4 block provider boundary. token is a nonzero adapter address;
 * payload starts at token+16. Zero token and zero bytes means terminal input.
 * tag is the original queue node's word8, not the CNK checksum at file word8.
 * For SCHl, header_format is the byte at its resolved tone+48. */
typedef struct Nba97MusicStreamBlock {
    uint32_t token, bytes, tag;
    uint8_t header_format;
} Nba97MusicStreamBlock;
typedef void (*Nba97MusicStreamFetch)(void*, Nba97MusicStreamBlock*);
/* Original909A8 copy boundary: source address token, staging offset, bytes.
 * Staging layout has one contiguous staging_size region per channel.
 * Provider/copy callbacks are synchronous and may mutate owned source state. */
typedef void (*Nba97MusicStreamCopy)(void*, uint32_t source,
    uint32_t staging_offset, uint32_t bytes);

typedef struct Nba97MusicStream {
    uint32_t data, bytes, tag, previous_tag;
    uint32_t channel_bytes, consumed_bytes, write_total, read_total;
    uint16_t staging_remaining, staging_size, write_index;
    uint16_t underrun_index, resume_index, format_index, underrun_active;
    uint8_t channels, header_format, producer_ended;
} Nba97MusicStream;

/* Entire726C4 scalar control after projecting metadata reads into fetch.
 * Returns1 success, -1 original divide-by-zero trap (retain prior mutations),
 * -2 missing argument. Source leaves prior tag/cursors unchanged on null fetch. */
int nba97_music_stream_next(Nba97MusicStream*, Nba97MusicStreamFetch, void*);
/* Entire72254 with next/copy boundaries above. Returns1 only for a FULL staging
 * block,0 for wait/end/partial, negative for fault. Caller72954 submits only1.
 * A partial last block is NOT padded/submitted: preserve the source tail loss.
 * Valid buffers, channels and positive progress are the adapter's contract;
 * this does not repair malformed source inputs or cap source loops. */
int nba97_music_stream_fill(Nba97MusicStream*, Nba97MusicStreamFetch,
    Nba97MusicStreamCopy, void*);

/* Source6BB48 changes the queue tail link toFFFFFFFF only if keep_open==0.
 * SCEl release remains the adapter's ownership action, after this mutation.
 * Returns the source classification5; missing tail returns-2. */
int nba97_music_stream_end(uint8_t keep_open, uint32_t* tail_link);

typedef struct Nba97MusicStreamDrain {
    uint32_t stop_requested, keyoff_mask;
    uint16_t read_index, write_index;
    uint8_t producer_ended, tracked_voice, paired_voice, channels;
    uint8_t protected_update, irq_busy, transfer_busy;
    uint32_t write_total, read_total, spu_base;
    uint16_t slots, staging_size;
} Nba97MusicStreamDrain;
/* SPU IRQ stop arms7313C..7319C +734C4..73554, scalar projection. Called after
 * source IRQ entry has acquired/disabled its platform interrupt state.
 * Returns1 if key-off queued,0 if remaining IRQ path is required,-2 bad pointer.
 * Does NOT set FINISHED. Do not replace its signed read==write-1 predicate
 * with a repaired modulo-ring comparison. */
int nba97_music_stream_irq_stop(Nba97MusicStreamDrain*);
/*7333C..73424: advance source read counters after a non-stopping slot IRQ.
 * Returns next SPU IRQ byte address through out_address (slot base+8).
 * Invoke after the remaining underrun/format arms, not instead of them.
 * Returns1 success,-1 source divide-by-zero trap,-2 missing argument. */
int nba97_music_stream_irq_advance(Nba97MusicStreamDrain*, uint32_t* out_address);

#ifdef __cplusplus
}
#endif
#endif
