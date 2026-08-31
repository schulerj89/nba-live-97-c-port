#include "recovered/music_voice.h"
#include <stdexcept>
#include <cstdint>
#include <iostream>
#include <vector>

static void require(bool condition) {
    if (!condition) throw std::runtime_error("music voice regression failed");
}

struct Probe {
    std::vector<Nba97MusicVoiceCall> calls;
    uint32_t duration = 6, target = 0;
};
static uint32_t invoke(void* opaque, Nba97MusicVoiceCall call,
    uint32_t, uint32_t, uint32_t word) {
    auto& p = *static_cast<Probe*>(opaque);
    p.calls.push_back(call);
    return call == NBA97_VOICE_ENVELOPE_WORD ? (word ? p.target : p.duration) : 0;
}
int main() {
    Nba97MusicVoice v{};
    v.ramp_current = 127u << 16;
    require(nba97_music_voice_fade(&v, 60, UINT32_MAX) == 0);
    require(v.ramp_step == uint32_t(-419430) && v.ramp_target == 0xffff0000u);
    require(nba97_music_voice_fade(&v, 0x7fffffffu, 127) == 0 && !v.ramp_step);
    v.ramp_target = 123;
    require(nba97_music_voice_gain(&v, 0) == 0 && v.ramp_target == 123);
    require(nba97_music_voice_gain(&v, 128) == -8);
    require(nba97_music_voice_fade(&v, 0, 128) == -8);

    Nba97MusicVoice voices[24]{};
    Nba97MusicVoiceClock clock{};
    Probe probe;
    clock.rate = 120;
    clock.master_gain = 127;
    voices[0].handle = 32;
    voices[0].active = 1;
    voices[0].authored_gain = 127;
    voices[0].ramp_current = 127u << 16;
    voices[0].envelope_current = 127u << 16;
    voices[0].envelope_ticks = UINT32_MAX;
    voices[0].envelope_count = 1;
    for (int i = 0; i < 120; ++i)
        require(nba97_music_voice_timer(&clock, voices, invoke, &probe) == 1);
    require(clock.services == 101 && clock.third_counter == 101); // inclusive source boundary
    require(voices[0].envelope_ticks == UINT32_MAX - 33);
    require(nba97_music_voice_fade(&voices[0], 60, UINT32_MAX) == 0);
    for (int i = 0; i < 63; ++i)
        require(nba97_music_voice_service(&clock, voices, invoke, &probe) == 1);
    require(voices[0].ramp_current == 0xffff0000u && !voices[0].ramp_step);
    require(voices[0].active == 1); // stop request is not hardware completion
    bool stopped = false;
    for (auto c : probe.calls) stopped |= c == NBA97_VOICE_STOP;
    require(stopped);

    // Descending stages preserve original unsigned division, not a signed fade.
    voices[0].envelope_ticks = 0;
    voices[0].envelope_count = 2;
    voices[0].envelope_index = 0;
    clock.third_counter = 2;
    require(nba97_music_voice_service(&clock, voices, invoke, &probe) == 1);
    require(voices[0].envelope_step == (0u - (127u << 16)) / 2u);
    require(voices[0].envelope_ticks == 1);
    voices[0].envelope_ticks = 0;
    voices[0].envelope_index = 0;
    probe.duration = 2;
    clock.third_counter = 2;
    require(nba97_music_voice_service(&clock, voices, invoke, &probe) == -1);
    require(voices[0].envelope_index == 1 && voices[0].envelope_ticks == 0);

    for (unsigned flags = 0; flags < 256; ++flags)
        for (unsigned pending = 0; pending < 256; ++pending)
            require(nba97_music_stream_status(uint8_t(flags), uint8_t(pending)) != 0);
    require(nba97_music_hardware_status(0, 0) == 0);
    require(nba97_music_hardware_status(0, 1) == 2);
    require(nba97_music_hardware_status(1, 0) == 3);
    require(nba97_music_hardware_status(1, 1) == 1);
    Nba97MusicCompletion completion{1, 7, 0, 0};
    require(nba97_music_voice_complete(&completion, &voices[0], 0, 2) == 1);
    require(!completion.finished && completion.channel_state == 1);
    auto handle = voices[0].handle;
    require(nba97_music_voice_complete(&completion, &voices[0], 0, 0) == 1);
    require(completion.finished == 1 && completion.tracked_voice == 255);
    require(!completion.channel_state && !completion.transient && !voices[0].active);
    require(voices[0].handle == handle);
    clock.in_service = 0;
    clock.lock_depth = 1;
    auto old_count = clock.callbacks;
    require(nba97_music_voice_timer(&clock, voices, invoke, &probe) == 1);
    require(clock.pending == 1 && clock.callbacks == old_count);
    clock.lock_depth = 0;
    clock.rate = 0;
    require(nba97_music_voice_timer(&clock, voices, invoke, &probe) == -1);
    require(clock.in_service == 1);
    std::cout << "music voice timing, fade, envelope and status tests passed\n";
}
