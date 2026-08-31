#include "music_playback.hpp"
#include "music_buffer_retirement.hpp"
#include "recovered/frontend_title.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Output final : nba97::MusicOutput {
    nba97::MusicOutputProgress reported{};
    std::uint64_t limit = 0;
    unsigned starts = 0, keyoffs = 0, retires = 0, current_gain = 0;
    std::string filename;
    void begin(std::shared_ptr<const nba97::MusicTrack> track, std::uint64_t gen,
        std::uint64_t frames, unsigned gain_value) override {
        reported = {gen, 0, false}; limit = frames; current_gain = gain_value;
        filename = track->filename; ++starts;
    }
    void gain(unsigned value) override { current_gain = value; }
    void keyOff(std::uint64_t gen) override { require(gen == reported.generation, "wrong keyoff generation"); ++keyoffs; }
    nba97::MusicOutputProgress progress() const override { return reported; }
    void retire() override { ++retires; }
};
nba97::MusicBank bank(std::uint32_t blocks = 4141) {
    nba97::MusicBank result;
    for (unsigned i = 0; i < 5; ++i) {
        auto track = std::make_shared<nba97::MusicTrack>();
        track->filename = "fixture" + std::to_string(i);
        track->full_blocks = blocks;
        result.tracks[i] = track;
    }
    for (unsigned i = 0; i < 16; ++i) result.slots[i] = static_cast<std::uint8_t>(i % 5);
    return result;
}
void boot(nba97::MusicPlayback& player, std::uint16_t& rng, const Nba97MusicInputs& inputs,
    std::uint32_t clock = 0) {
    const auto before = rng;
    require(player.update(clock, rng, inputs) == 0 && player.routing().phase == 2, "initial load");
    require(player.update(clock, rng, inputs) == 0 && player.routing().phase == 3, "initial start");
    require(rng == before, "initial menu1 consumed RNG");
}
void lifecycle() {
    Output out;
    nba97::MusicPlayback p(bank(), out, 6, 0);
    std::uint16_t rng = 0xabcdu;
    Nba97MusicInputs inputs{}; inputs.volume = 6;
    boot(p, rng, inputs);
    require(out.starts == 1 && out.filename == "fixture0" && out.current_gain == 90, "first resource/gain");
    require(p.rawStreamStatus() == 3, "resident enabled stream status flags");
    require(out.limit == 7418880, "source slot-entry prefix");
    out.reported.completed_frames = out.limit - 1;
    out.reported.drained = true; // EOF/storage signal alone cannot finish source
    p.update(1, rng, inputs);
    require(!p.completion().finished && out.keyoffs == 0, "premature native drain finished source");
    out.reported.completed_frames = out.limit; out.reported.drained = false;
    p.update(2, rng, inputs);
    require(out.keyoffs == 1 && !p.completion().finished && p.completion().channel_state == 1, "keyoff must precede native drain");
    out.reported.drained = true;
    p.update(3, rng, inputs);
    require(p.completion().finished && p.routing().phase == 4, "native drain must pass source completion owner");
    require(p.rawStreamStatus() == 3, "voice completion silently cleared source stream flags");
    p.update(3, rng, inputs);
    require(out.retires == 1 && p.routing().phase == 1, "source retirement");
    require(p.rawStreamStatus() == -14, "source detach did not clear stream flags");
    auto expected_rng = rng;
    const unsigned expected = nba97_frontend_random(&expected_rng) & 15u;
    require(p.update(3, rng, inputs) == 1 && rng == expected_rng, "selector did not share caller RNG exactly once");
    require(p.routing().current == expected % 5 + 1, "selector did not use source slot index");
    p.update(3, rng, inputs);
    require(out.starts == 2 && p.outputGeneration() == 2, "second native generation");
    out.reported = {1, UINT64_MAX, true};
    p.update(4, rng, inputs);
    require(!p.completion().finished && out.keyoffs == 1, "old generation completed current voice");
}
void allPlans(const nba97::MusicBank& real_bank) {
    const std::uint32_t blocks[]{4141, 3972, 3196, 3232, 5196};
    const std::uint16_t partial[]{544, 960, 880, 816, 800};
    const std::uint32_t decoded[]{7421609, 7119488, 5728768, 5793152, 9312613};
    for (unsigned i = 0; i < 5; ++i) {
        const auto& track = *real_bank.tracks[i];
        require(track.full_blocks == blocks[i] && track.partial_adpcm_bytes == partial[i], "real source staging prefix mismatch");
        require(track.decoded.info.sample_count == decoded[i], "real decoder metadata mismatch");
        for (unsigned pause = 0; pause < 2; ++pause) {
            auto fixture = real_bank;
            fixture.tracks[0] = real_bank.tracks[i];
            Output out; nba97::MusicPlayback p(std::move(fixture), out, 0, 0);
            std::uint16_t rng = 0; Nba97MusicInputs inputs{}; inputs.pause = pause;
            boot(p, rng, inputs);
            require(out.limit == (blocks[i] - 1) * nba97::music_staging_frames, "real source slot-entry limit mismatch");
            require(p.drain().slots == (pause ? 210 : 400), "ring geometry came from filename instead of ED2AC");
            out.reported.completed_frames = out.limit; out.reported.drained = true;
            p.update(1, rng, inputs);
            require(out.keyoffs == 1 && p.completion().finished, "real finite track did not complete through source owners");
        }
        std::cout << track.filename << " full_blocks=" << track.full_blocks << " partial=" << partial[i]
            << " decoded_frames=" << decoded[i] << " slot_entry_frames=" << (blocks[i]-1)*nba97::music_staging_frames << '\n';
    }
}
void sourceQuirks() {
    Nba97MusicInputs inputs{}; inputs.volume = 8;
    std::uint16_t rng = 19;
    Output wrap; nba97::MusicPlayback p(bank(200), wrap, 8, 0);
    boot(p, rng, inputs);
    wrap.reported.completed_frames = wrap.limit; wrap.reported.drained = true;
    p.update(120, rng, inputs);
    require(p.clock().services == 101 && p.clock().callbacks == 120, "120Hz inclusive source service cadence");
    require(!p.completion().finished && wrap.keyoffs == 0 && p.routing().phase == 3, "source non-modulo zero-wrap bug silently fixed");
    p.requestSourceStop(); p.update(120, rng, inputs);
    require(p.routing().stopping && p.routing().phase == 3, "source phase10 stopping owner");
    p.update(240, rng, inputs);
    require(p.routing().phase != 11, "BUSY raw status changed to boolean");

    Output fade; nba97::MusicPlayback f(bank(), fade, 8, 0);
    boot(f, rng, inputs);
    f.update(26501, rng, inputs); // strict deadline26500: trigger fade only now
    require(f.routing().phase == 4 && fade.keyoffs == 0, "deadline fade made immediate keyoff");
    f.update(26573, rng, inputs); // recovered60 argument ->20 every-third-service steps
    require(f.drain().stop_requested && fade.current_gain == 0 && !f.completion().finished, "recovered fade/stop request");
    fade.reported.completed_frames = nba97::music_staging_frames;
    f.update(26575, rng, inputs);
    require(fade.keyoffs == 1 && !f.completion().finished, "fade stop must wait for slot event then drain");
    fade.reported.drained = true;
    f.update(26576, rng, inputs);
    require(f.completion().finished, "fade native drain failed");

    Output rollover; nba97::MusicPlayback r(bank(), rollover, 0, UINT32_MAX - 2);
    boot(r, rng, inputs, UINT32_MAX - 2);
    r.update(2, rng, inputs);
    require(r.clock().callbacks == 5, "source clock wrap lost IRQ ticks");
}
struct Retirement final : nba97::MusicRetirementOps {
    bool reset_ok = true, close_ok = true;
    std::array<bool, 4> unprepare_ok{{true, true, true, true}};
    std::vector<int> calls;
    bool reset() noexcept override { calls.push_back(10); return reset_ok; }
    bool unprepare(std::size_t i) noexcept override { calls.push_back(static_cast<int>(i)); return unprepare_ok[i]; }
    bool close() noexcept override { calls.push_back(20); return close_ok; }
};
void lifetime() {
    nba97::MusicBufferRetirement state;
    Retirement driver;
    require(state.release(driver) && driver.calls.empty(), "never-opened owner touched driver");
    state.opened = true; state.prepared = {{true, true, true, true}};
    driver.reset_ok = false;
    require(!state.release(driver) && driver.calls == std::vector<int>{10}, "failed reset released storage");
    driver.reset_ok = true; driver.unprepare_ok[2] = false; driver.calls.clear();
    require(!state.release(driver) && driver.calls == std::vector<int>({10,0,1,2,3}) && state.prepared[2], "still-playing header closed or freed");
    driver.unprepare_ok[2] = true; driver.close_ok = false; driver.calls.clear();
    require(!state.release(driver) && driver.calls == std::vector<int>({10,2,20}) && state.opened, "failed close released callback event");
    driver.close_ok = true; driver.calls.clear();
    require(state.release(driver) && driver.calls == std::vector<int>({10,20}) && !state.opened, "safe retry repeated unprepare or leaked normal owner");
}
}
int main(int argc, char** argv) {
    try {
        lifecycle(); sourceQuirks(); lifetime();
        if (argc == 2) allPlans(nba97::loadMusicBank(argv[1]));
        std::cout << "music playback lifecycle, RNG, cadence, source bugs and driver lifetime passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
    return 0;
}
