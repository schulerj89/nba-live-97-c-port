#include "game_audio_stream_pump_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,
            "game audio stream pump integration check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807ff000u;
constexpr std::uint32_t EntrySp = 0x807fff00u;
constexpr std::uint32_t SpeechFrame = EntrySp - 0x20u;
constexpr std::uint32_t PumpFrame = SpeechFrame - 0x20u;
constexpr std::uint32_t CallerRa = 0x8002da8cu;

struct Composition {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x110000);
    std::vector<std::uint8_t> ram_known =
        std::vector<std::uint8_t>(0x110000, 1);
    std::array<std::uint8_t, 0x1000> stack{};
    std::array<std::uint8_t, 0x1000> stack_known{};
    std::array<Nba97GameTextRegion, 2> regions{{
        {Ram, ram.data(), ram_known.data(), ram.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}}};
    Nba97GameSpeechStartupContext speech{};
    Nba97GameAudioStreamPumpContext pump{};
    Nba97GameSpeechStartupProgress speech_progress{};
    Nba97GameAudioStreamPumpAdapterProgress adapter_progress{};
    std::vector<Nba97GameSpeechStartupEvent> speech_children;
    std::vector<Nba97GameAudioStreamPumpEvent> pump_children;
    std::vector<std::uint32_t> clocks{100u, 340u};
    std::vector<std::uint32_t> readiness{0u, 1u};
    unsigned clock_index = 0;
    unsigned ready_index = 0;
    bool refuse_pump_gate = false;

    Composition() {
        stack.fill(0xcd);
        stack_known.fill(1);
        put32(0x80015018u, 1);
        put8(0x800c43b0u, 5);
        put32(0x800c438cu, 0x87654320u);
        speech.memory = {regions.data(), regions.size()};
        speech.operation_budget = 64;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            speech.registers.gpr[i] = {
                0x21000000u + i * 0x01010101u, 0x0f};
        speech.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        speech.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        speech.registers.gpr[NBA97_MATCH_INITIALIZE_FP] = {0xa1b2c3d4u, 0x05};
        speech.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {CallerRa, 0x0f};
        speech.io = speechIo;
        speech.user = this;
        pump.operation_budget = 32;
        pump.io = pumpIo;
        pump.user = this;
    }

    std::uint8_t* bytes(std::uint32_t address) {
        return address >= Stack ? stack.data() + (address - Stack) :
            ram.data() + (address - Ram);
    }
    void put8(std::uint32_t address, std::uint8_t value) {
        *bytes(address) = value;
    }
    void put32(std::uint32_t address, std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i)
            bytes(address)[i] = static_cast<std::uint8_t>(value >> (8u * i));
    }
    std::uint32_t get32(std::uint32_t address) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(bytes(address)[i]) << (8u * i);
        return value;
    }

    static int speechIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameSpeechStartupEvent* event,
        Nba97GameSpeechStartupRegisters* registers) {
        auto& c = *static_cast<Composition*>(user);
        c.speech_children.push_back(*event);
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_800853F4)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x81234560u, 0x0f};
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_80083D38)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x8abcdef0u, 0x0f};
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_800A5810) {
            const auto i = c.clock_index < c.clocks.size() ? c.clock_index :
                static_cast<unsigned>(c.clocks.size() - 1u);
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {c.clocks[i], 0x0f};
            ++c.clock_index;
        }
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_8008847C) {
            const auto i = c.ready_index < c.readiness.size() ? c.ready_index :
                static_cast<unsigned>(c.readiness.size() - 1u);
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {c.readiness[i], 0x0f};
            ++c.ready_index;
        }
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_8002ABB4)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0xdecafbadu, 0x0f};
        return 1;
    }

    static int pumpIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameAudioStreamPumpEvent* event,
        Nba97GameAudioStreamPumpRegisters* registers) {
        auto& c = *static_cast<Composition*>(user);
        c.pump_children.push_back(*event);
        if (event->kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_8008472C) {
            if (c.refuse_pump_gate)
                return 0;
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 0x0f};
        }
        if (event->kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 0x0f};
        return 1;
    }

    int run() {
        return nba97_game_speech_startup_with_audio_stream_pump(&speech,
            &pump, &speech_progress, &adapter_progress);
    }
};

void natural_both_speech_call_sites_use_production_owner() {
    Composition c;
    check(c.run() == NBA97_TEXT_COMPLETE && c.speech_progress.completed &&
        c.adapter_progress.pump_result == NBA97_TEXT_COMPLETE &&
        c.adapter_progress.pump_invocations == 2 &&
        c.adapter_progress.pump_completions == 2);
    check(c.adapter_progress.pump_event[0].pc == 0x800801e4u &&
        c.adapter_progress.pump_event[0].delay_slot_pc == 0x800801e8u &&
        c.adapter_progress.pump_event[0].entry == 0x80083eecu &&
        c.adapter_progress.pump_event[1].pc == 0x8008021cu &&
        c.adapter_progress.pump_event[1].delay_slot_pc == 0x80080220u &&
        c.adapter_progress.pump_event[1].entry == 0x80083eecu);
    check(c.adapter_progress.pump.completed &&
        c.adapter_progress.pump.frame_stack_pointer == PumpFrame &&
        c.adapter_progress.pump.restored_return_address.word == 0x80080224u &&
        c.adapter_progress.pump.restored_s8.word == 0xa1b2c3d4u &&
        c.adapter_progress.pump.restored_s8.known_mask == 0x05);
    check(c.speech_progress.restored_return_address.word == CallerRa &&
        c.speech_progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
        c.speech_progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0xdecafbadu);
    check(c.pump_children.size() == 6 &&
        c.pump_children[0].pc == 0x80083f00u &&
        c.pump_children[1].pc == 0x80083f78u &&
        c.pump_children[2].pc == 0x80083f88u &&
        c.pump_children[3].pc == 0x80083f00u);
    check(c.speech_children.size() == 12 &&
        c.adapter_progress.unresolved_callbacks_completed == 12 &&
        c.get32(PumpFrame + 0x1cu) == 0x80080224u &&
        c.get32(0x8002149cu) == 0x81234560u &&
        c.get32(0x800dc7e8u) == 0x8abcdef0u);
}

void nested_prefixes_and_adapter_validation() {
    Composition limit;
    limit.pump.operation_budget = 3;
    check(limit.run() == NBA97_TEXT_IO_REFUSED &&
        limit.adapter_progress.pump_result == NBA97_TEXT_LIMIT &&
        limit.adapter_progress.pump_invocations == 1 &&
        limit.adapter_progress.pump.operations == 3 &&
        limit.adapter_progress.pump.stopped_pc == 0x80083f00u &&
        limit.speech_progress.stopped_pc == 0x800801e4u);

    Composition refused;
    refused.refuse_pump_gate = true;
    check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.adapter_progress.pump_result == NBA97_TEXT_IO_REFUSED &&
        refused.adapter_progress.pump.callbacks_completed == 0 &&
        refused.speech_progress.stopped_pc == 0x800801e4u);

    Composition args;
    Nba97GameAudioStreamPumpAdapterProgress out{};
    check(nba97_game_speech_startup_with_audio_stream_pump(nullptr,
        &args.pump, &args.speech_progress, &out) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_speech_startup_with_audio_stream_pump(&args.speech,
        nullptr, &args.speech_progress, &out) == NBA97_TEXT_ARGUMENT);
    Nba97GameSpeechStartupEvent wrong{};
    Nba97GameSpeechStartupRegisters registers{};
    check(nba97_game_audio_stream_pump_from_speech_startup(
        &args.speech.memory, &wrong, &registers, &args.pump, &out) ==
        NBA97_TEXT_ARGUMENT);
}
void natural_controller_reset_and_nested_failures() {
    for (unsigned mode=0; mode<3; ++mode) {
        Composition c;
        c.put32(0x800fe90eu,5);c.put32(0x800fdb6cu,2);
        for(unsigned i=0;i<8;++i){c.put32(0x800fdc50u+i*4u,0x80002000u+i*64u);c.put32(0x80002028u+i*64u,0xaaaabeefu);}
        Nba97GameControllerFrameResetContext reset{};
        reset.memory=c.speech.memory;reset.registers=c.speech.registers;reset.operation_budget=40;
        Nba97GameAudioStreamPumpProgress pump_progress{};
        struct Binding {Composition* fixture;Nba97GameAudioStreamPumpProgress* progress;} binding{&c,&pump_progress};
        reset.user=&binding;
        reset.io=[](void* u,const Nba97GameTextMemory* m,const Nba97GameControllerFrameResetEvent* e,
            Nba97GameControllerFrameResetRegisters* r)->int {
            auto& b=*static_cast<Binding*>(u);
            return nba97_game_audio_stream_pump_from_controller_reset(m,e,r,&b.fixture->pump,b.progress)==NBA97_TEXT_COMPLETE;
        };
        if(mode==1)c.pump.operation_budget=3;
        if(mode==2)c.refuse_pump_gate=true;
        Nba97GameControllerFrameResetProgress progress{};
        const int result=nba97_game_controller_frame_reset(&reset,&progress);
        check((c.get32(0x800fe90eu)&0xffffu)==3);
        for(unsigned i=0;i<8;++i)check(c.get32(0x80002028u+i*64u)==0xaaaa0000u);
        if(mode==0){
            check(result==NBA97_TEXT_COMPLETE && progress.completed && pump_progress.completed);
            check(pump_progress.restored_return_address.word==0x80067654u && pump_progress.frame_stack_pointer==PumpFrame);
            check(progress.registers.gpr[29].word==EntrySp && progress.restored_return_address.word==CallerRa);
            check(pump_progress.restored_s8.word==0xa1b2c3d4u && pump_progress.restored_s8.known_mask==5);
        }else{
            check(result==NBA97_TEXT_IO_REFUSED && !progress.completed && !pump_progress.completed);
            check(progress.stopped_pc==0x8006764cu && pump_progress.stopped_pc==0x80083f00u);
            check(progress.registers.gpr[29].word==PumpFrame && progress.registers.gpr[31].word==0x80083f08u);
        }
    }
    Composition c;Nba97GameControllerFrameResetEvent event{};Nba97GameAudioStreamPumpProgress p{};
    check(nba97_game_audio_stream_pump_from_controller_reset(&c.speech.memory,&event,&c.speech.registers,&c.pump,&p)==NBA97_TEXT_ARGUMENT);
}

}

int main() {
    natural_both_speech_call_sites_use_production_owner();
    nested_prefixes_and_adapter_validation();
    natural_controller_reset_and_nested_failures();
    std::printf("%u game audio stream pump integration checks passed\n", checks);
    return 0;
}
