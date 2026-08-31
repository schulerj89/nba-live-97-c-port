#include "recovered/game_gameplay_audio.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <tuple>
#include <vector>

namespace {
int checks;
int failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; \
    std::cerr << "check failed line " << __LINE__ << ": " #x "\n"; } } while (0)

struct Store { std::uint32_t pc, address, word; unsigned width; };
struct Fixture {
    std::vector<std::uint8_t> memory = std::vector<std::uint8_t>(0x200000);
    std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
    std::vector<Store> stores;
    std::vector<Nba97GameplayAudioCall> calls;
    Nba97GameplayAudioContext context{};
    Nba97GameplayAudioProgress progress{};
    Nba97GameplayAudioResult result{};
    int service_status = NBA97_BODY_OK;
    int scheduler_mutation = -1;
    unsigned scheduler_callbacks = 0;
    Nba97PlayerFrameValue service_result{};

    Fixture() {
        service_result.word = 0x12345678u;
        service_result.known_mask = 15;
        context.access = &Fixture::access;
        context.service = &Fixture::service;
        context.user = this;
        context.operation_budget = 1000;
    }
    static std::size_t offset(std::uint32_t address, unsigned width) {
        if (address < 0x80000000u ||
            std::uint64_t(address) + width > 0x80200000ull) throw 1;
        return address - 0x80000000u;
    }
    void put(std::uint32_t address, std::uint32_t word, unsigned width) {
        const auto at = offset(address, width);
        for (unsigned i = 0; i < width; ++i)
            memory[at + i] = std::uint8_t(word >> (i * 8));
    }
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        const auto at = offset(address, width);
        std::uint32_t word = 0;
        for (unsigned i = 0; i < width; ++i)
            word |= std::uint32_t(memory[at + i]) << (i * 8);
        return word;
    }
    void standard() {
        put(0x800f9ffe, 0, 2);
        put(0x800fe860, 0x100, 4);
        put(0x800fdc48, 0x80120000, 4);
        put(0x80120014, 96, 2);
        put(0x80120016, 0, 2);
        put(0x80120018, 30, 2);
        put(0x80021d7e, 128, 1);
        put(0x80021d7f, 1, 1);
        put(0x80021d6c, 7, 4);
        put(0x800d7b89, 1, 1);
        put(0x800d793c, 2, 1);
        put(0x800d793d, 100, 1);
        put(0x800d793e, 3, 1);
        put(0x800d7c64, 0x80130000, 4);
        for (unsigned sequence = 0; sequence < 16; ++sequence) {
            const auto record = 0x80130028u + sequence * 16;
            put(record, 1, 1);
            put(record + 1, 5 + sequence, 1);
            put(record + 4, 2, 2);
            put(record + 6, 20, 2);
            put(record + 8, 11 + sequence, 4);
        }
        put(0x800d7948, 10u << 16, 4);
        put(0x800c4b0c, 0, 4);
        put(0x800c4b08, 0, 4);
    }
    int run(Nba97GameplayAudioEntry entry, std::uint32_t request) {
        return nba97_game_gameplay_audio(&context, entry, request,
                                         &result, &progress);
    }
    static int access(void* user, std::uint32_t pc, std::uint32_t address,
                      unsigned width, unsigned kind,
                      Nba97PlayerFrameValue* value) {
        auto& f = *static_cast<Fixture*>(user);
        try {
            const auto at = offset(address, width);
            if (kind == NBA97_FRAME_READ) {
                value->word = 0;
                value->known_mask = 0;
                for (unsigned i = 0; i < width; ++i) {
                    if (f.known[at + i]) {
                        value->word |= std::uint32_t(f.memory[at + i]) << (i * 8);
                        value->known_mask |= std::uint8_t(1u << i);
                    }
                }
            } else {
                f.put(address, value->word, width);
                for (unsigned i = 0; i < width; ++i) f.known[at + i] = 1;
                f.stores.push_back({pc, address, value->word, width});
            }
            return NBA97_BODY_OK;
        } catch (...) { return NBA97_BODY_ARGUMENT; }
    }
    static int service(void* user, const Nba97GameplayAudioCall* call,
                       Nba97PlayerFrameValue* result) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back(*call);
        if (f.service_status != NBA97_BODY_OK) return f.service_status;
        if (call->entry == 0x80093734u) {
            ++f.scheduler_callbacks;
            if (f.scheduler_callbacks == 1 && f.scheduler_mutation >= 0)
                f.put(0x800c4b08, std::uint32_t(f.scheduler_mutation), 4);
        }
        *result = f.service_result;
        return NBA97_BODY_OK;
    }
};

void arguments() {
    Fixture f;
    CHECK(nba97_game_gameplay_audio(nullptr, NBA97_GAMEPLAY_AUDIO_SOUND_29258,
                                    0, &f.result, &f.progress) == NBA97_BODY_ARGUMENT);
    CHECK(nba97_game_gameplay_audio(&f.context, NBA97_GAMEPLAY_AUDIO_SOUND_29258,
                                    0, nullptr, &f.progress) == NBA97_BODY_ARGUMENT);
    CHECK(nba97_game_gameplay_audio(&f.context,
              static_cast<Nba97GameplayAudioEntry>(0x800295c8u), 0,
              &f.result, &f.progress) == NBA97_BODY_ARGUMENT);
}

void event_gate() {
    Fixture f; f.standard(); f.put(0x80021d7f, 0, 1);
    CHECK(f.run(NBA97_GAMEPLAY_AUDIO_EVENT_29590, 0x1234) == NBA97_BODY_OK);
    CHECK(f.result.known && f.result.word == 0);
    CHECK(f.calls.empty());
    Fixture g; g.standard();
    CHECK(g.run(NBA97_GAMEPLAY_AUDIO_EVENT_29590, 7) == NBA97_BODY_OK);
    CHECK(g.calls.size() == 1);
    CHECK(g.calls[0].pc == 0x800ab1b8u && g.calls[0].entry == 0x800ac080u);
    CHECK(g.calls[0].count == 3 && g.calls[0].return_bytes == 0);
    CHECK(g.calls[0].argument[0] == 3 && g.calls[0].argument[1] == 12);
    CHECK(g.calls[0].argument[2] == 100);
    CHECK(g.result.word == 0);
    CHECK(g.get(0x800d794c, 4) == (12u << 16));
    CHECK(g.get(0x800d793f, 1) == 7);
    CHECK(g.get(0x800d7950, 4) == 54);
    CHECK(g.get(0x800c4b0c, 4) == 0);
}

void fixed_programs() {
    const std::uint32_t requests[] = {0x60, 0x61, 0x62, 0x63, 0x1234};
    const std::uint32_t programs[] = {2, 4, 6, 0, 0x1234};
    for (unsigned i = 0; i < 5; ++i) {
        Fixture f; f.standard();
        CHECK(f.run(NBA97_GAMEPLAY_AUDIO_SOUND_29258, requests[i]) == NBA97_BODY_OK);
        CHECK(f.calls.size() == 1);
        CHECK(f.calls[0].entry == 0x800ac080u && f.calls[0].pc == 0x800294dcu);
        CHECK(f.calls[0].count == 3 && f.calls[0].argument[0] == 7);
        CHECK(f.calls[0].argument[1] == programs[i]);
        CHECK(f.calls[0].argument[2] == 127);
    }
}

void dynamic_programs() {
    {
        Fixture f; f.standard(); f.put(0x80120018, 30, 2);
        CHECK(f.run(NBA97_GAMEPLAY_AUDIO_SOUND_29258, 0) == NBA97_BODY_OK);
        CHECK(f.get(0x800fe860, 4) == 0x101);
        CHECK(f.calls.size() == 1 && f.calls[0].argument[1] == 0);
        CHECK(f.calls[0].argument[2] == 127); // level 32, then *12
    }
    {
        Fixture f; f.standard(); f.put(0x80120018, 0, 2);
        CHECK(f.run(NBA97_GAMEPLAY_AUDIO_SOUND_29258, 0) == NBA97_BODY_OK);
        CHECK(f.result.word == 0x12345678u && f.calls.size() == 1);
        CHECK(f.calls[0].argument[2] == 127);
        CHECK(f.get(0x800fe860, 4) == 0x101);
    }
    {
        Fixture f; f.standard();
        CHECK(f.run(NBA97_GAMEPLAY_AUDIO_SOUND_29258, 1) == NBA97_BODY_OK);
        CHECK(f.calls.size() == 1 && f.calls[0].argument[1] == 1);
        CHECK(f.calls[0].argument[2] == 127);
    }
    for (std::uint32_t request : {2u, 3u}) {
        Fixture f; f.standard();
        CHECK(f.run(NBA97_GAMEPLAY_AUDIO_SOUND_29258, request) == NBA97_BODY_OK);
        CHECK(f.calls.size() == 1 && f.calls[0].argument[1] == request);
        CHECK(f.calls[0].argument[2] == 127);
    }
}

void source_prefixes() {
    Fixture f; f.standard(); f.service_status = NBA97_GAMEPLAY_AUDIO_SERVICE_REQUIRED;
    CHECK(f.run(NBA97_GAMEPLAY_AUDIO_SOUND_29258, 0x12) ==
          NBA97_GAMEPLAY_AUDIO_SERVICE_REQUIRED);
    CHECK(f.get(0x800fe860, 4) == (0x100u | (1u << 18)));
    CHECK(!f.progress.completed && f.progress.stopped_pc == 0x800294dcu);
    CHECK(f.progress.stopped_address == 0x800ac080u);

    Fixture g; g.standard(); g.context.operation_budget = 2;
    CHECK(g.run(NBA97_GAMEPLAY_AUDIO_SOUND_29258, 0) == NBA97_BODY_JOURNAL_LIMIT);
    CHECK(g.progress.stopped_pc == 0x800292a4u);
    CHECK(g.get(0x800fe860, 4) == 0x100);

    Fixture h; h.standard();
    h.known[0x80021d7f - 0x80000000] = 0;
    CHECK(h.run(NBA97_GAMEPLAY_AUDIO_EVENT_29590, 1) == NBA97_BODY_UNKNOWN);
    CHECK(h.progress.stopped_pc == 0x80029594u);
}

void callback_validation() {
    Fixture f; f.standard(); f.service_result.known_mask = 7;
    CHECK(f.run(NBA97_GAMEPLAY_AUDIO_SOUND_29258, 0x60) == NBA97_BODY_ARGUMENT);
    Fixture g; g.standard(); g.service_result.is_reference = 1;
    g.service_result.reference.known = 1;
    CHECK(g.run(NBA97_GAMEPLAY_AUDIO_SOUND_29258, 0x60) == NBA97_BODY_ARGUMENT);
    Fixture h; h.standard(); h.context.service = nullptr;
    CHECK(h.run(NBA97_GAMEPLAY_AUDIO_EVENT_29590, 1) ==
          NBA97_GAMEPLAY_AUDIO_SERVICE_REQUIRED);
}

void sequence_scheduler() {
    Fixture f; f.standard();
    f.put(0x80130029, 0xff, 1); // Negative program skips AC080.
    f.put(0x800c4b08, 3, 4);
    CHECK(f.run(NBA97_GAMEPLAY_AUDIO_EVENT_29590, 0) == NBA97_BODY_OK);
    CHECK(f.calls.size() == 3);
    for (const auto& call : f.calls) {
        CHECK(call.entry == 0x80093734u && call.pc == 0x80093e54u);
        CHECK(call.count == 0 && call.return_bytes == 0);
    }
    CHECK(f.progress.scheduler_calls == 3 && f.progress.program_calls == 0);
    CHECK(f.get(0x800c4b08, 4) == 0 && f.get(0x800c4b0c, 4) == 0);

    Fixture nested; nested.standard(); nested.put(0x800c4b0c, 2, 4);
    nested.put(0x800c4b08, 2, 4);
    nested.put(0x80130029, 0xff, 1);
    CHECK(nested.run(NBA97_GAMEPLAY_AUDIO_EVENT_29590, 0) == NBA97_BODY_OK);
    CHECK(nested.calls.empty());
    CHECK(nested.get(0x800c4b0c, 4) == 2);
    CHECK(nested.get(0x800c4b08, 4) == 2);

    Fixture refused; refused.standard(); refused.service_status = -14;
    CHECK(refused.run(NBA97_GAMEPLAY_AUDIO_EVENT_29590, 0) == -14);
    CHECK(refused.get(0x800c4b0c, 4) == 1); // AC080 refusal before unlock.
    CHECK(!refused.progress.completed);

    Fixture changed; changed.standard(); changed.put(0x80130029, 0xff, 1);
    changed.put(0x800c4b08, 1, 4); changed.scheduler_mutation = 2;
    CHECK(changed.run(NBA97_GAMEPLAY_AUDIO_EVENT_29590, 0) == NBA97_BODY_OK);
    CHECK(changed.progress.scheduler_calls == 3);
    CHECK(changed.get(0x800c4b08, 4) == 0);

    Fixture scheduler_refused; scheduler_refused.standard();
    scheduler_refused.put(0x80130029, 0xff, 1);
    scheduler_refused.put(0x800c4b08, 1, 4);
    scheduler_refused.service_status = -14;
    CHECK(scheduler_refused.run(NBA97_GAMEPLAY_AUDIO_EVENT_29590, 0) == -14);
    CHECK(scheduler_refused.get(0x800c4b0c, 4) == 0);
    CHECK(scheduler_refused.get(0x800c4b08, 4) == 0);
    CHECK(scheduler_refused.progress.stopped_pc == 0x80093e54u);
}
}

int main() {
    arguments();
    event_gate();
    fixed_programs();
    dynamic_programs();
    source_prefixes();
    callback_validation();
    sequence_scheduler();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
