#include "music_stream.h"
#include <stddef.h>

static int64_t s32(uint32_t x) {
    return x < UINT32_C(0x80000000) ? (int64_t)x : (int64_t)x - INT64_C(4294967296);
}
static int32_t s16(uint16_t x) { return x < 32768 ? x : (int32_t)x - 65536; }
#define SCHL UINT32_C(0x6c484353)

int nba97_music_stream_next(Nba97MusicStream* s, Nba97MusicStreamFetch fetch,
    void* context) {
    Nba97MusicStreamBlock block;
    if (!s || !fetch) return -2;
    fetch(context, &block);
    s->data = block.token;
    s->bytes = block.bytes;
    if (!s->data) return 1;
    if (block.tag == SCHL) {
        const uint8_t format = (uint8_t)(block.header_format == 1);
        if (format != s->header_format) {
            s->header_format = format;
            s->format_index = s->write_index;
        }
        fetch(context, &block);
        s->data = block.token;
        s->bytes = block.bytes;
        if (!s->data) return 1;
        if (block.tag == SCHL) { s->tag = block.tag; return 1; }
    }
    if (s->previous_tag == UINT32_MAX && block.tag != UINT32_MAX &&
        s16(s->resume_index) == -1)
        s->resume_index = s->write_index;
    s->previous_tag = block.tag;
    s->tag = block.tag;
    s->data += 16u;
    if (!s->channels) return -1; /* Source break7, not stereo fallback. */
    s->channel_bytes = (uint32_t)(s32(s->bytes) / s->channels);
    s->consumed_bytes = 0;
    return 1;
}

static int mark_underrun(Nba97MusicStream* s) {
    if (s->tag != UINT32_MAX) return 0;
    if (s32(s->write_total - s->read_total) >= 17) return 1;
    if (s16(s->underrun_index) == -1 && !s16(s->underrun_active))
        s->underrun_index = s->write_index;
    return 0;
}

int nba97_music_stream_fill(Nba97MusicStream* s, Nba97MusicStreamFetch fetch,
    Nba97MusicStreamCopy copy, void* context) {
    uint32_t left, offset;
    int result;
    if (!s || !fetch || !copy) return -2;
    left = (uint32_t)s16(s->staging_remaining);
    if (!left) left = (uint32_t)s16(s->staging_size);
    offset = (uint32_t)s16(s->staging_size) - left;
    if (!s->data || s->tag == UINT32_MAX || s->tag == SCHL ||
        s32(s->consumed_bytes) >= s32(s->channel_bytes)) {
        result = nba97_music_stream_next(s, fetch, context);
        if (result < 0) return result;
        if (s->tag == SCHL || mark_underrun(s)) goto done;
    }
    while (s32(left) > 0) {
        uint32_t take, source, destination, channel;
        if (!s->data && !s->bytes) {
            s->producer_ended = 1;
            goto done;
        }
        take = s->channel_bytes - s->consumed_bytes;
        if (s32(take) > s32(left)) take = left;
        source = s->data + s->consumed_bytes;
        destination = offset;
        for (channel = 0; channel < s->channels; ++channel) {
            copy(context, source, destination, take);
            source += s->channel_bytes;
            destination += (uint32_t)s16(s->staging_size);
        }
        offset += take;
        left -= take;
        s->consumed_bytes += take;
        if (s32(s->consumed_bytes) >= s32(s->channel_bytes)) {
            result = nba97_music_stream_next(s, fetch, context);
            if (result < 0) return result;
            if (s->tag == SCHL || mark_underrun(s)) goto done;
        }
    }
done:
    s->staging_remaining = (uint16_t)left;
    return left == 0;
}

int nba97_music_stream_end(uint8_t keep_open, uint32_t* tail_link) {
    if (!tail_link) return -2;
    if (!keep_open) *tail_link = UINT32_MAX;
    return 5;
}

int nba97_music_stream_irq_stop(Nba97MusicStreamDrain* s) {
    if (!s) return -2;
    if (s->stop_requested) s->stop_requested = 0;
    else if (!s->producer_ended || s16(s->read_index) != s16(s->write_index) - 1)
        return 0;
    s->protected_update = 1;
    s->keyoff_mask |= UINT32_C(1) << (s->tracked_voice & 31);
    if (s->channels == 2)
        s->keyoff_mask |= UINT32_C(1) << (s->paired_voice & 31);
    s->protected_update = 0;
    s->transfer_busy = 0;
    s->irq_busy = s->transfer_busy;
    return 1;
}

int nba97_music_stream_irq_advance(Nba97MusicStreamDrain* s, uint32_t* address) {
    int32_t capacity;
    uint32_t offset;
    if (!s || !address) return -2;
    if (s32(s->read_total) < s32(s->write_total)) {
        ++s->read_total;
        ++s->read_index;
    }
    if (!s->channels) return -1;
    capacity = s16(s->slots) / s->channels;
    if (s16(s->read_index) == capacity) s->read_index = 0;
    offset = (uint32_t)s16(s->read_index) * (uint32_t)s16(s->staging_size);
    *address = s->spu_base + offset + 8u;
    return 1;
}
