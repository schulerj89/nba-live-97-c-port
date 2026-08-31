#include "recovered/music_stream.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool ok) { if (!ok) throw std::runtime_error("music stream regression failed"); }
struct Feed {
    std::vector<Nba97MusicStreamBlock> blocks;
    std::size_t next = 0;
    uint32_t copied = 0;
};
static void fetch(void* opaque, Nba97MusicStreamBlock* out) {
    auto& f = *static_cast<Feed*>(opaque);
    *out = f.next < f.blocks.size() ? f.blocks[f.next++] : Nba97MusicStreamBlock{};
}
static void copy(void* opaque, uint32_t, uint32_t, uint32_t bytes) {
    static_cast<Feed*>(opaque)->copied += bytes;
}
static Nba97MusicStream start() {
    Nba97MusicStream s{};
    s.channels = 2;
    s.staging_size = 1024;
    s.underrun_index = s.resume_index = s.format_index = 65535;
    return s;
}
int main() {
    auto s = start();
    Feed f{{{0x1000, 3360, 0x6c444353, 0}}};
    require(nba97_music_stream_fill(&s, fetch, copy, &f) == 1);
    require(f.copied == 2048 && !s.staging_remaining && !s.producer_ended);
    require(nba97_music_stream_fill(&s, fetch, copy, &f) == 0);
    require(f.copied == 3360 && s.staging_remaining == 368 && s.producer_ended);
    // Source returns only full blocks: the final656 bytes/channel are not submitted.
    uint32_t tail = 123;
    require(nba97_music_stream_end(1, &tail) == 5 && tail == 123);
    require(nba97_music_stream_end(0, &tail) == 5 && tail == UINT32_MAX);

    s = start();
    s.write_index = 17;
    Feed headers{{{0x1000, 128, 0x6c484353, 1}, {0x2000, 128, 0x6c484353, 0}}};
    require(nba97_music_stream_fill(&s, fetch, copy, &headers) == 0);
    require(s.header_format == 1 && s.format_index == 17 && s.tag == 0x6c484353);
    require(headers.copied == 0 && s.staging_remaining == 1024);
    s = start();
    s.write_total = 17;
    Feed starved{{{0x1000, 2048, UINT32_MAX, 0}}};
    require(nba97_music_stream_fill(&s, fetch, copy, &starved) == 0);
    require(!starved.copied && s.underrun_index == 65535 && !s.producer_ended);
    s = start();
    s.channels = 0;
    Feed malformed{{{0x1000, 2048, 0x6c444353, 0}}};
    require(nba97_music_stream_next(&s, fetch, &malformed) == -1);
    require(s.data == 0x1010 && s.tag == 0x6c444353); // original pre-trap mutations

    Nba97MusicStreamDrain drain{};
    drain.producer_ended = 1;
    drain.read_index = 199;
    drain.write_index = 0;
    drain.channels = 2;
    drain.tracked_voice = 5;
    drain.paired_voice = 6;
    require(nba97_music_stream_irq_stop(&drain) == 0); // original non-modulo quirk
    drain.write_index = 200;
    require(nba97_music_stream_irq_stop(&drain) == 1);
    require(drain.keyoff_mask == ((1u << 5) | (1u << 6)));
    drain.stop_requested = 1;
    drain.producer_ended = 0;
    drain.irq_busy = drain.transfer_busy = 1;
    require(nba97_music_stream_irq_stop(&drain) == 1);
    require(!drain.stop_requested && !drain.irq_busy && !drain.transfer_busy);
    drain.read_index = 199;
    drain.read_total = 199;
    drain.write_total = 205;
    drain.slots = 400;
    drain.staging_size = 1024;
    drain.spu_base = 0x10000;
    uint32_t irq_address = 0;
    require(nba97_music_stream_irq_advance(&drain, &irq_address) == 1);
    require(drain.read_index == 0 && drain.read_total == 200 && irq_address == 0x10008);
    std::cout << "music stream staging, terminal, starvation and drain tests passed\n";
}
