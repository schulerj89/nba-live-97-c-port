#include "recovered/game_main.h"
#include "recovered/game_overlay_entry.h"
#include "recovered/game_static_initializers.h"
#include "recovered/game_global_pointer_save.h"
#include "recovered/game_heap_initialize.h"
#include "recovered/game_cd_directory_initialize.h"
#include "recovered/game_path_prefix_set.h"
#include "recovered/game_directory_cache_configure.h"
#include "recovered/game_interrupt_mask_set.h"
#include "recovered/game_reset_callback.h"
#include "recovered/game_controller_resume.h"
#include "recovered/game_reset_graph.h"
#include "recovered/game_graph_debug_set.h"
#include "recovered/game_vblank_initialize.h"
#include "recovered/game_clock_initialize.h"
#include "recovered/game_gte_initialize.h"
#include "recovered/game_clock_delta.h"
#include "recovered/game_presentation_wait.h"
#include "recovered/game_video_environment_initialize.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game main check %u failed\n", checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807fff00u;
constexpr std::uint32_t EntrySp = 0x807ffff8u;
constexpr std::uint32_t FrameSp = EntrySp - 0x28u;

struct Fixture {
    enum Mode {
        Transfer,
        Return,
        Refuse,
        MissingImage,
        MissingSize,
        DirectTransfer,
        InvalidOutcome,
        UnknownEntry,
        UnalignedEntry
    } mode = Transfer;
    unsigned fail_call = 0;
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x200000, 0xcd);
    std::vector<std::uint8_t> ram_known = std::vector<std::uint8_t>(0x200000, 1);
    std::vector<std::uint8_t> stack = std::vector<std::uint8_t>(0x100, 0xcd);
    std::vector<std::uint8_t> stack_known = std::vector<std::uint8_t>(0x100, 1);
    Nba97GameTextRegion regions[2] = {
        {Ram, ram.data(), ram_known.data(), ram.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}
    };
    Nba97GameMainContext context{{regions, 2}, 1000, EntrySp, 0x11223344u,
        {0xa0a0a0a0u, 0xb1b1b1b1u, 0xc2c2c2c2u}, 0x800d79c8u, io, this};
    Nba97GameMainProgress progress{};
    Nba97GameStaticInitializersProgress static_progress{};
    Nba97GameGlobalPointerSaveProgress global_pointer_progress{};
    Nba97GameHeapInitializeProgress heap_progress{};
    Nba97GameCdDirectoryInitializeProgress cd_directory_progress{};
    Nba97GameGlobalPointerSaveProgress cd_global_pointer_progress{};
    Nba97GamePathPrefixSetProgress path_prefix_progress{};
    Nba97GameDirectoryCacheConfigureProgress directory_cache_progress{};
    Nba97GameInterruptMaskSetProgress interrupt_mask_progress{};
    Nba97GameResetCallbackProgress reset_callback_progress{};
    std::array<Nba97GameControllerResumeProgress,2> controller_resume_progress{};
    Nba97GameResetGraphProgress reset_graph_progress{};
    Nba97GameResetCallbackProgress reset_graph_reset_callback_progress{};
    Nba97GameGraphDebugSetProgress graph_debug_progress{};
    Nba97GameVblankInitializeProgress vblank_progress{};
    Nba97GameGlobalPointerSaveProgress vblank_global_pointer_progress{};
    Nba97GameClockInitializeProgress clock_progress{};
    Nba97GameGteInitializeState gte_state{};
    Nba97GameGteInitializeProgress gte_progress{};
    Nba97GameClockDeltaProgress clock_delta_progress{};
    std::array<Nba97GamePresentationWaitProgress,41> presentation_wait_progress{};
    Nba97GameVideoEnvironmentInitializeProgress video_environment_progress{};
    std::vector<Nba97GameHeapInitializeEvent> heap_journal =
        std::vector<Nba97GameHeapInitializeEvent>(300);
    std::vector<Nba97GameMainEvent> calls;
    std::vector<Nba97GameCdDirectoryInitializeEvent> cd_calls;
    std::vector<Nba97GamePathPrefixSetEvent> path_calls;
    std::vector<Nba97GameResetCallbackEvent> reset_callback_calls;
    std::vector<Nba97GameControllerResumeEvent> controller_resume_calls;
    std::vector<Nba97GameResetGraphEvent> reset_graph_calls;
    std::vector<Nba97GameResetCallbackEvent> reset_graph_reset_callback_calls;
    std::vector<Nba97GameGraphDebugSetEvent> graph_debug_calls;
    std::vector<Nba97GameVblankInitializeEvent> vblank_calls;
    std::vector<Nba97GameClockInitializeEvent> clock_calls;
    std::vector<Nba97GameClockDeltaEvent> clock_delta_calls;
    std::vector<Nba97GamePresentationWaitEvent> presentation_wait_calls;
    std::vector<Nba97GameVideoEnvironmentInitializeEvent> video_environment_calls;
    unsigned heap_format_calls = 0;
    unsigned controller_resume_invocations = 0;
    unsigned presentation_wait_invocations = 0;
    unsigned presentation_vblank_signals = 0;
    unsigned video_environment_invocations = 0;
    unsigned video_environment_child_callbacks = 0;
    std::uint32_t active_display_environment = 0;
    std::uint32_t active_draw_environment = 0;
    bool video_environment_synchronized = false;
    bool vblank_set_rcnt_rejected = false;
    bool vblank_started_after_rejection = false;
    bool clock_critical_section = false;
    bool clock_interrupt_installed = false;
    bool clock_shutdown_registered = false;
    bool clock_counter_set = false;
    bool clock_counter_started = false;
    bool compose_static = false;
    bool compose_global_pointer = false;
    bool compose_heap = false;
    bool compose_cd_directory = false;
    bool compose_path_prefix = false;
    bool compose_directory_cache = false;
    bool compose_interrupt_mask = false;
    bool compose_reset_callback = false;
    bool compose_controller_resume = false;
    bool compose_reset_graph = false;
    bool compose_graph_debug = false;
    bool compose_vblank = false;
    bool compose_clock = false;
    bool compose_gte = false;
    bool compose_clock_delta = false;
    bool compose_presentation_wait = false;
    bool compose_video_environment = false;

    std::uint8_t* byte(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base && std::uint64_t(address - region.base) < region.size)
                return region.data + (address - region.base);
        check(false);
        return nullptr;
    }
    std::uint8_t* known(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base && std::uint64_t(address - region.base) < region.size)
                return region.known + (address - region.base);
        check(false);
        return nullptr;
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width = 4) {
        for (unsigned i = 0; i < width; ++i) {
            *byte(address + i) = std::uint8_t(value >> (i * 8));
            *known(address + i) = 1;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width = 4) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(*byte(address + i)) << (i * 8);
        return value;
    }
    void putText(std::uint32_t address, const char* text) {
        do {
            *byte(address) = static_cast<std::uint8_t>(*text);
            *known(address) = 1;
            ++address;
        } while (*text++);
    }
    static int heapIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameHeapInitializeEvent* event) {
        auto& f = *static_cast<Fixture*>(user);
        ++f.heap_format_calls;
        if (event->kind != NBA97_HEAP_INITIALIZE_FORMAT_9CB7C ||
            event->argument[2] != 0x8002802cu)
            return 0;
        if (f.heap_format_calls == 1 && event->argument[0] == 0x8010b620u &&
            event->argument[1] == 0x80028034u) {
            f.putText(event->argument[0], "LOW MB_RAM  ");
            return 1;
        }
        if (f.heap_format_calls == 2 && event->argument[0] == 0x8010b648u &&
            event->argument[1] == 0x80028040u) {
            f.putText(event->argument[0], "HIGH MB_RAM ");
            return 1;
        }
        return 0;
    }
    static int cdDirectoryIo(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameCdDirectoryInitializeEvent* event,
        Nba97GameCdDirectoryInitializeValue* value) {
        auto& f = *static_cast<Fixture*>(user);
        f.cd_calls.push_back(*event);
        if (event->kind == NBA97_CD_DIRECTORY_INITIALIZE_POLL) {
            f.put(0x80103551u, 0, 1);
            return 1;
        }
        switch (event->entry) {
        case 0x800a4830u: {
            Nba97GameGlobalPointerSaveContext context{*memory,10,event->global_pointer};
            return nba97_game_global_pointer_save(&context,
                &f.cd_global_pointer_progress) == NBA97_TEXT_COMPLETE;
        }
        case 0x800985a4u:
        case 0x8009d94cu:
            return 1;
        case 0x8009fa6cu:
            if (event->argument_count != 1 || event->argument[0] != 0x80103550u)
                return 0;
            f.put(0x80103551u,0,1);
            f.put(0x80103554u,0x00000200u);
            *value={1,1};
            return 1;
        case 0x80091870u:
            if (event->argument_count != 1)
                return 0;
            if (event->argument[0] == 0x80103554u)
                *value={0x100u,1};
            else if (event->argument[0] == event->stack_pointer + 0x18u &&
                f.get(event->argument[0]) == 0x00160200u)
                *value={0x110u,1};
            else
                return 0;
            return 1;
        case 0x80091e1cu:
            return event->argument_count == 1 && event->argument[0] == 0x10u;
        case 0x80091e80u:
            if (event->argument_count != 2 || event->argument[0] != 0x80103550u ||
                event->argument[1] != 1)
                return 0;
            f.put(0x801035eeu,23u);
            f.put(0x801035f6u,2048u);
            return 1;
        case 0x800aa04cu:
            if (event->argument_count != 2 || event->argument[1] != 4 ||
                (event->argument[0] != 0x801035eeu &&
                 event->argument[0] != 0x801035f6u))
                return 0;
            *value={f.get(event->argument[0]),1};
            return 1;
        default:
            return 0;
        }
    }
    static int pathPrefixIo(void* user, const Nba97GameTextMemory*,
        const Nba97GamePathPrefixSetEvent* event,
        Nba97GamePathPrefixSetValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.path_calls.push_back(*event);
        if (event->kind==NBA97_GAME_PATH_PREFIX_COPY) {
            if (event->argument_count!=2 || event->argument[0]!=0x800d6dacu ||
                event->argument[1]!=0x800247e4u)
                return 0;
            for (unsigned i=0;i<64;++i) {
                const auto source=*f.byte(event->argument[1]+i);
                *f.byte(event->argument[0]+i)=source;
                *f.known(event->argument[0]+i)=*f.known(event->argument[1]+i);
                if (!source) {
                    *value={event->argument[0],1};
                    return 1;
                }
            }
            return 0;
        }
        if (event->kind!=NBA97_GAME_PATH_PREFIX_LENGTH ||
            event->argument_count!=1 || event->argument[0]!=0x800d6dacu)
            return 0;
        for (unsigned length=0;length<64;++length)
            if (!*f.byte(event->argument[0]+length)) {
                *value={length,1};
                return 1;
            }
        return 0;
    }
    static int resetCallbackIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameResetCallbackEvent* event,
        Nba97GameResetCallbackValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.reset_callback_calls.push_back(*event);
        *value={1,1};
        return 1;
    }
    static int controllerResumeIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameControllerResumeEvent* event,
        Nba97GameControllerResumeValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.controller_resume_calls.push_back(*event);
        if(event->kind==NBA97_GAME_CONTROLLER_RESUME_INITIALIZE &&
            event->entry==0x80091184u) {
            *value={0,0};
            return 1;
        }
        if(event->kind==NBA97_GAME_CONTROLLER_RESUME_CLOCK &&
            event->entry==0x800a5810u) {
            *value={37,1};
            return 1;
        }
        return 0;
    }
    static int resetGraphResetCallbackIo(void* user,
        const Nba97GameTextMemory*, const Nba97GameResetCallbackEvent* event,
        Nba97GameResetCallbackValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.reset_graph_reset_callback_calls.push_back(*event);
        *value={1,1};
        return 1;
    }
    static int resetGraphIo(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameResetGraphEvent* event,
        Nba97GameResetGraphValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.reset_graph_calls.push_back(*event);
        if(event->entry==0x8009bd78u) {
            if(event->argument_count!=3)return 0;
            for(std::uint32_t i=0;i<event->argument[2];++i)
                f.put(event->argument[0]+i,event->argument[1],1);
            return 1;
        }
        if(event->entry==0x800985dcu) {
            Nba97GameResetCallbackContext context{*memory,10,
                event->stack_pointer,event->return_address,
                resetGraphResetCallbackIo,&f};
            if(nba97_game_reset_callback(&context,
                    &f.reset_graph_reset_callback_progress)!=NBA97_TEXT_COMPLETE)
                return 0;
            *value={f.reset_graph_reset_callback_progress.return_v0,
                f.reset_graph_reset_callback_progress.return_v0_known};
            return 1;
        }
        if(event->entry==0x8009cb2cu || event->entry==0x8009bda4u)
            return 1;
        if(event->entry==0x8009b878u && event->pc==0x800990d0u) {
            *value={0,1};
            return 1;
        }
        return 0;
    }
    static int graphDebugIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameGraphDebugSetEvent* event) {
        auto& f=*static_cast<Fixture*>(user);
        f.graph_debug_calls.push_back(*event);
        return 1;
    }
    static int vblankIo(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameVblankInitializeEvent* event,
        Nba97GameVblankInitializeValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.vblank_calls.push_back(*event);
        value->word=0;
        value->known=1;
        if(event->entry==0x800a4830u) {
            Nba97GameGlobalPointerSaveContext context{*memory,10,
                event->global_pointer};
            return nba97_game_global_pointer_save(&context,
                &f.vblank_global_pointer_progress)==NBA97_TEXT_COMPLETE;
        }
        if(event->entry==0x800983b4u) {
            /* Retail SetRCnt masks the spec to index 3, rejects it and
               returns zero. The VBlank initializer deliberately continues. */
            f.vblank_set_rcnt_rejected=true;
            return 1;
        }
        if(event->entry==0x80098488u) {
            /* Retail StartRCnt still unmasks table entry 3 before returning
               zero. This fixture records that reached service-side effect. */
            f.vblank_started_after_rejection=true;
            return 1;
        }
        if(event->entry==0x800a3e48u) {
            f.put(0x800d7a88u,0);
            f.put(0x800d7afcu,0);
            f.put(0x800d7b00u,0);
            return 1;
        }
        return event->entry==0x800994f4u || event->entry==0x80098394u ||
            event->entry==0x8009860cu || event->entry==0x80098594u;
    }
    static int clockIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameClockInitializeEvent* event,
        Nba97GameClockInitializeValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.clock_calls.push_back(*event);
        *value={0,1};
        switch(event->entry) {
        case 0x80098394u:
            if(event->pc!=0x800914ecu || event->argument_count ||
               f.clock_critical_section)return 0;
            f.clock_critical_section=true;return 1;
        case 0x8009860cu:
            if(!f.clock_critical_section || event->pc!=0x80091578u ||
               event->argument_count!=2 || event->argument[0]!=6 ||
               event->argument[1]!=0x800916b4u)return 0;
            f.clock_interrupt_installed=true;return 1;
        case 0x800a575cu:
            if(!f.clock_critical_section || event->pc!=0x80091594u ||
               event->argument_count!=1 || event->argument[0]!=0x8009167cu)
                return 0;
            f.put(0x800d7234u,0x8009167cu);
            f.clock_shutdown_registered=true;return 1;
        case 0x800983b4u:
            if(!f.clock_critical_section || event->pc!=0x8009163cu ||
               event->argument_count!=3 || event->argument[0]!=0xf2000002u ||
               event->argument[1]!=35280 || event->argument[2]!=0x1000u)
                return 0;
            f.clock_counter_set=true;value->word=1;return 1;
        case 0x80098488u:
            if(!f.clock_counter_set || event->pc!=0x8009164cu ||
               event->argument_count!=1 || event->argument[0]!=0xf2000002u)
                return 0;
            f.clock_counter_started=true;value->word=1;return 1;
        case 0x80098594u:
            if(!f.clock_critical_section || event->pc!=0x80091654u ||
               event->argument_count)return 0;
            f.clock_critical_section=false;return 1;
        case 0x800a5880u:
            if(f.clock_critical_section || event->pc!=0x8009165cu ||
               event->argument_count)return 0;
            f.put(0x800d7a7cu,0);f.put(0x800d7a70u,0);
            f.put(event->global_pointer+0x164u,0);
            f.put(event->global_pointer+0x160u,0);
            return 1;
        default:return 0;
        }
    }
    static int clockDeltaIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameClockDeltaEvent* event,
        Nba97GameClockDeltaValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.clock_delta_calls.push_back(*event);
        if(event->kind!=NBA97_GAME_CLOCK_DELTA_READ_CLOCK ||
           event->pc!=0x800a585cu || event->entry!=0x800a5810u ||
           event->argument_count)
            return 0;
        /* This is the already-recovered 0x800A5810 leaf: it returns the live
           source-clock word without creating host cadence. */
        *value={f.get(0x800d7a70u),1};
        return 1;
    }
    static int presentationWaitIo(void* user, const Nba97GameTextMemory*,
        const Nba97GamePresentationWaitEvent* event,
        Nba97GamePresentationWaitValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.presentation_wait_calls.push_back(*event);
        if(event->kind!=NBA97_GAME_PRESENTATION_WAIT_SERVICE ||
           event->pc!=0x80029be4u || event->entry!=0x800a9cc0u ||
           event->return_address!=0x80029becu || event->argument_count ||
           f.get(event->global_pointer+0x1b4u)!=0 ||
           f.get(0x800d7a84u)!=0 || f.get(0x800d7b3cu)!=0 ||
           f.get(0x800d7b40u)!=0)
            return 0;
        /* Exact cold/common 0x800A9CC0 service fixture: the child clears the
           ready word, then an acknowledged source VBlank ISR sets it and
           increments the retained frame counter. This is not host cadence. */
        f.put(0x800d7a80u,0);
        f.put(0x800d7a80u,1);
        f.put(0x800d7a88u,f.get(0x800d7a88u)+1u);
        f.put(event->global_pointer+0x1b4u,0);
        ++f.presentation_vblank_signals;
        *value={1,1};
        return 1;
    }
    static int videoEnvironmentIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameVideoEnvironmentInitializeEvent* event,
        Nba97GameVideoEnvironmentInitializeValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        const auto call=f.video_environment_calls.size();
        static constexpr std::uint32_t pcs[9]={0x80029f60u,0x80029f7cu,
            0x80029f9cu,0x80029fb8u,0x8002a040u,0x8002a048u,
            0x8002a050u,0x8002a058u,0x8002a060u};
        static constexpr std::uint32_t entries[9]={0x8009cad0u,0x8009cad0u,
            0x8009ca00u,0x8009ca00u,0x80099ca4u,0x80099accu,
            0x80099ca4u,0x80099accu,0x800994f4u};
        if(call>=9 || event->pc!=pcs[call] || event->entry!=entries[call] ||
           event->return_address!=event->pc+8u ||
           event->stack_pointer!=FrameSp-0x38u ||
           event->global_pointer!=0x800d79c8u)
            return 0;
        f.video_environment_calls.push_back(*event);
        ++f.video_environment_child_callbacks;
        if(call<4) {
            const std::uint32_t pointer[4]={0x8002205cu,0x80022070u,
                0x80021eecu,0x80021f48u};
            const std::uint32_t y[4]={0x100u,0,0,0x100u};
            if(event->argument_count!=5 || event->argument[0]!=pointer[call] ||
               event->argument[1]!=0 || event->argument[2]!=y[call] ||
               event->argument[3]!=0x200u || event->argument[4]!=0xf0u)
                return 0;
            const auto p=event->argument[0];
            f.put(p,event->argument[1],2);f.put(p+2,event->argument[2],2);
            f.put(p+4,event->argument[3],2);f.put(p+6,event->argument[4],2);
            if(call<2) {
                for(unsigned offset=8;offset<16;offset+=2)f.put(p+offset,0,2);
                for(unsigned offset=16;offset<20;++offset)f.put(p+offset,0,1);
            } else {
                f.put(p+8,event->argument[1],2);
                f.put(p+10,event->argument[2],2);
                for(unsigned offset=12;offset<20;offset+=2)f.put(p+offset,0,2);
                f.put(p+20,10,2);f.put(p+22,1,1);f.put(p+23,1,1);
                for(unsigned offset=24;offset<28;++offset)f.put(p+offset,0,1);
            }
            *value={p,1};
            return 1;
        }
        const std::uint32_t pointer[4]={0x8002205cu,0x80021eecu,
            0x80022070u,0x80021f48u};
        if(call<8) {
            if(event->argument_count!=1 || event->argument[0]!=pointer[call-4])
                return 0;
            if(event->kind==NBA97_GAME_VIDEO_PUT_DISP_ENV)
                f.active_display_environment=event->argument[0];
            else if(event->kind==NBA97_GAME_VIDEO_PUT_DRAW_ENV)
                f.active_draw_environment=event->argument[0];
            else return 0;
            *value={event->argument[0],1};
            return 1;
        }
        if(event->kind!=NBA97_GAME_VIDEO_DRAW_SYNC ||
           event->argument_count!=1 || event->argument[0]!=0)
            return 0;
        f.video_environment_synchronized=true;
        *value={0,1};
        return 1;
    }
    static int io(void* user, const Nba97GameTextMemory* memory, const Nba97GameMainEvent* event,
        Nba97GameMainValue* value, Nba97GameMainCalleeOutcome* outcome) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back(*event);
        if (f.compose_static && event->entry == 0x800948d0u) {
            Nba97GameStaticInitializersContext context{*memory,100,event->stack_pointer,
                event->return_address,{event->saved_register[0],event->saved_register[1]}};
            if (nba97_game_static_initializers(&context,&f.static_progress) != NBA97_TEXT_COMPLETE)
                return 0;
        }
        if (f.compose_global_pointer && event->entry == 0x800a4830u) {
            Nba97GameGlobalPointerSaveContext context{*memory,10,event->global_pointer};
            if (nba97_game_global_pointer_save(&context,&f.global_pointer_progress) !=
                NBA97_TEXT_COMPLETE)
                return 0;
        }
        if (f.compose_heap && event->entry == 0x8008fa6cu) {
            Nba97GameHeapInitializeArguments arguments{event->argument[0],event->argument[1],
                event->argument[2],event->global_pointer};
            Nba97GameHeapInitializeContext context{*memory,10000,heapIo,&f};
            if (nba97_game_heap_initialize(&context,&arguments,f.heap_journal.data(),
                    f.heap_journal.size(),&f.heap_progress) != NBA97_TEXT_COMPLETE)
                return 0;
        }
        if (f.compose_cd_directory && event->entry == 0x80091c08u) {
            Nba97GameCdDirectoryInitializeContext context{*memory,200,4,
                event->stack_pointer,event->return_address,0x0f0f0f0fu,
                event->global_pointer,cdDirectoryIo,&f};
            if (nba97_game_cd_directory_initialize(&context,
                    &f.cd_directory_progress) != NBA97_TEXT_COMPLETE)
                return 0;
        }
        if (f.compose_path_prefix && event->entry == 0x800a35d8u) {
            Nba97GamePathPrefixSetContext context{*memory,100,event->argument[0],
                event->stack_pointer,event->return_address,event->saved_register[0],
                event->global_pointer,pathPrefixIo,&f};
            if (nba97_game_path_prefix_set(&context,&f.path_prefix_progress) !=
                    NBA97_TEXT_COMPLETE)
                return 0;
        }
        if (f.compose_directory_cache && event->entry == 0x80092c7cu) {
            Nba97GameDirectoryCacheConfigureContext context{*memory,100,
                event->argument[0],event->argument[1],event->stack_pointer,
                0xf3f3f3f3u};
            if (nba97_game_directory_cache_configure(&context,
                    &f.directory_cache_progress) != NBA97_TEXT_COMPLETE)
                return 0;
        }
        if (f.compose_interrupt_mask && event->entry == 0x800985b4u) {
            Nba97GameInterruptMaskSetContext context{*memory,10,
                event->argument[0]};
            if (nba97_game_interrupt_mask_set(&context,
                    &f.interrupt_mask_progress) != NBA97_TEXT_COMPLETE)
                return 0;
            *value={f.interrupt_mask_progress.return_v0,1};
        }
        if (f.compose_reset_callback && event->entry == 0x800985dcu) {
            Nba97GameResetCallbackContext context{*memory,10,
                event->stack_pointer,event->return_address,resetCallbackIo,&f};
            if (nba97_game_reset_callback(&context,
                    &f.reset_callback_progress) != NBA97_TEXT_COMPLETE)
                return 0;
            *value={f.reset_callback_progress.return_v0,
                f.reset_callback_progress.return_v0_known};
        }
        if (f.compose_controller_resume && event->entry == 0x8008f1d4u) {
            if (f.controller_resume_invocations >= f.controller_resume_progress.size())
                return 0;
            auto& progress=f.controller_resume_progress[f.controller_resume_invocations++];
            Nba97GameControllerResumeContext context{*memory,20,event->argument[0],
                event->stack_pointer,event->return_address,controllerResumeIo,&f};
            if (nba97_game_controller_resume(&context,&progress) != NBA97_TEXT_COMPLETE)
                return 0;
            *value={progress.return_v0,progress.return_v0_known};
        }
        if (f.compose_reset_graph && event->entry == 0x80099058u) {
            Nba97GameResetGraphContext context{*memory,100,
                event->argument[0],event->stack_pointer,event->return_address,
                {event->saved_register[0],event->saved_register[1]},
                resetGraphIo,&f};
            if (nba97_game_reset_graph(&context,&f.reset_graph_progress) !=
                    NBA97_TEXT_COMPLETE)
                return 0;
            *value={f.reset_graph_progress.return_v0,
                f.reset_graph_progress.return_v0_known};
        }
        if (f.compose_graph_debug && event->entry == 0x800992c4u) {
            Nba97GameGraphDebugSetContext context{*memory,20,
                event->argument[0],event->stack_pointer,event->return_address,
                event->saved_register[0],graphDebugIo,&f};
            if (nba97_game_graph_debug_set(&context,&f.graph_debug_progress) !=
                    NBA97_TEXT_COMPLETE)
                return 0;
            *value={f.graph_debug_progress.return_v0,
                f.graph_debug_progress.return_v0_known};
        }
        if (f.compose_vblank && event->entry == 0x800a43e8u) {
            Nba97GameVblankInitializeContext context{*memory,100,
                event->stack_pointer,event->return_address,0xf4f4f4f4u,
                event->global_pointer,vblankIo,&f};
            if (nba97_game_vblank_initialize(&context,&f.vblank_progress) !=
                    NBA97_TEXT_COMPLETE)
                return 0;
            *value={f.vblank_progress.return_v0,
                f.vblank_progress.return_v0_known};
        }
        if (f.compose_clock && event->entry == 0x800914d8u) {
            Nba97GameClockInitializeContext context{*memory,100,
                event->argument[0],event->stack_pointer,event->return_address,
                0xf5f5f5f5u,event->global_pointer,clockIo,&f};
            if (nba97_game_clock_initialize(&context,&f.clock_progress) !=
                    NBA97_TEXT_COMPLETE)
                return 0;
            *value={f.clock_progress.return_v0,
                f.clock_progress.return_v0_known};
        }
        if (f.compose_gte && event->entry == 0x80056678u) {
            Nba97GameGteInitializeContext context{&f.gte_state,20};
            if (nba97_game_gte_initialize(&context,&f.gte_progress) !=
                    NBA97_TEXT_COMPLETE)
                return 0;
            *value={f.gte_progress.return_v0,
                f.gte_progress.return_v0_known};
        }
        if (f.compose_clock_delta && event->entry == 0x800a584cu) {
            Nba97GameClockDeltaContext context{*memory,20,
                event->stack_pointer,event->return_address,
                event->saved_register[0],event->global_pointer,clockDeltaIo,&f};
            if (nba97_game_clock_delta(&context,&f.clock_delta_progress) !=
                    NBA97_TEXT_COMPLETE)
                return 0;
            *value={f.clock_delta_progress.return_v0,
                f.clock_delta_progress.return_v0_known};
        }
        if (f.compose_presentation_wait && event->entry == 0x80029bdcu) {
            if(f.presentation_wait_invocations>=f.presentation_wait_progress.size())
                return 0;
            auto& wait_progress=
                f.presentation_wait_progress[f.presentation_wait_invocations];
            Nba97GamePresentationWaitContext context{*memory,10,
                event->stack_pointer,event->return_address,event->global_pointer,
                presentationWaitIo,&f};
            if(nba97_game_presentation_wait(&context,&wait_progress)!=
                    NBA97_TEXT_COMPLETE)
                return 0;
            ++f.presentation_wait_invocations;
            *value={wait_progress.return_v0,wait_progress.return_v0_known};
        }
        if (f.compose_video_environment && event->entry == 0x80029f20u) {
            ++f.video_environment_invocations;
            Nba97GameVideoEnvironmentInitializeContext context{*memory,100,
                event->argument[0],event->stack_pointer,event->return_address,
                {event->saved_register[0],event->saved_register[1],
                 event->saved_register[2],0xd3d3d3d3u,0xd4d4d4d4u,
                 0xd5d5d5d5u},event->global_pointer,videoEnvironmentIo,&f};
            if(nba97_game_video_environment_initialize(&context,
                   &f.video_environment_progress)!=NBA97_TEXT_COMPLETE)
                return 0;
            *value={f.video_environment_progress.return_v0,
                f.video_environment_progress.return_v0_known};
        }
        if (f.mode == Refuse && f.calls.size() == f.fail_call)
            return 0;
        if (f.mode == InvalidOutcome && f.calls.size() == f.fail_call) {
            *outcome = static_cast<Nba97GameMainCalleeOutcome>(9);
            return 1;
        }
        if (f.mode == DirectTransfer && f.calls.size() == f.fail_call) {
            *outcome = NBA97_GAME_MAIN_CALLEE_TRANSFERRED;
            return 1;
        }
        *outcome = NBA97_GAME_MAIN_CALLEE_RETURNED;
        if (event->entry == 0x80029bfcu) {
            value->word = 0x80123400u;
            value->known = f.mode == MissingImage ? 0 : 1;
        } else if (event->entry == 0x80090d60u) {
            value->word = 0x1410u;
            value->known = f.mode == MissingSize ? 0 : 1;
        } else if (event->entry == 0x800aa468u) {
            if (f.mode != UnknownEntry) {
                const auto entry = f.mode == UnalignedEntry ? 0x801e0102u : 0x801e0100u;
                f.put(0x801e0000u, entry);
            } else {
                for (unsigned i = 0; i < 4; ++i)
                    *f.known(0x801e0000u + i) = 0;
            }
        } else if (event->kind == NBA97_GAME_MAIN_INDIRECT_CALL) {
            if (f.mode == Transfer)
                *outcome = NBA97_GAME_MAIN_CALLEE_TRANSFERRED;
            else if (f.mode == Return) {
                f.put(FrameSp + 0x24u, 0x55667788u);
                f.put(FrameSp + 0x20u, 0x12121212u);
                f.put(FrameSp + 0x1cu, 0x34343434u);
                f.put(FrameSp + 0x18u, 0x56565656u);
            }
        }
        return 1;
    }
    int run() { return nba97_game_main(&context, &progress); }
};

void transferred_path() {
    Fixture f;
    check(f.run() == NBA97_TEXT_COMPLETE);
    check(f.progress.completed && f.progress.transferred && f.progress.loaded_feload &&
        f.progress.reached_match_orchestration);
    check(f.calls.size() == 77 && f.progress.callbacks_completed == 77);
    check(f.progress.stores == 15 && f.progress.reads == 1 && f.progress.accesses == 16 &&
        f.progress.operations == 93);
    check(f.progress.frame_stack_pointer == FrameSp && f.progress.stack_pointer == FrameSp &&
        f.progress.global_pointer == 0x800d79c8u);
    check(f.get(FrameSp + 0x24u) == 0x11223344u &&
        f.get(FrameSp + 0x20u) == 0xc2c2c2c2u &&
        f.get(FrameSp + 0x1cu) == 0xb1b1b1b1u &&
        f.get(FrameSp + 0x18u) == 0xa0a0a0a0u);
    check(f.get(0x800d7b04u) == 0 && f.get(0x8002148cu, 2) == 0 &&
        f.get(0x800d7a94u) == 0x78u && f.get(0x800d7af4u) == 0 &&
        f.get(0x800d7af8u) == 0);
    check(f.get(FrameSp + 0x10u, 2) == 0x200u && f.get(FrameSp + 0x12u, 2) == 0 &&
        f.get(FrameSp + 0x14u, 2) == 0x200u && f.get(FrameSp + 0x16u, 2) == 0x100u);
    check(f.progress.loaded_image == 0x80123400u && f.progress.loaded_image_size == 0x1410u &&
        f.progress.indirect_entry == 0x801e0100u);
    check(f.calls.front().pc == 0x800299a4u && f.calls.front().entry == 0x800948d0u &&
        f.calls.front().return_address == 0x800299acu && f.calls.front().stack_pointer == FrameSp);
    check(f.calls[2].entry == 0x8008fa6cu && f.calls[2].argument_count == 3 &&
        f.calls[2].argument[0] == 0xdcu && f.calls[2].argument[1] == 0x8010b61cu &&
        f.calls[2].argument[2] == 0xf21e4u);
    check(f.calls[4].entry == 0x800a35d8u && f.calls[4].argument[0] == 0x800247e4u &&
        f.calls[5].entry == 0x80092c7cu && f.calls[5].argument[0] == 0x8001000cu &&
        f.calls[5].argument[1] == 0x2c3u);
    check(f.calls[6].pc == 0x80029a08u && f.calls[6].entry == 0x800985b4u &&
        f.calls[6].argument_count == 1 && f.calls[6].argument[0] == 0);
    check(f.calls[7].pc == 0x80029a10u && f.calls[7].entry == 0x800985dcu &&
        f.calls[8].pc == 0x80029a18u && f.calls[8].entry == 0x8008f1d4u &&
        f.calls[8].argument_count == 1 && f.calls[8].argument[0] == 8 &&
        f.calls[11].pc == 0x80029a30u && f.calls[11].entry == 0x8008f1d4u &&
        f.calls[11].argument_count == 1 && f.calls[11].argument[0] == 8);
    check(f.calls[18].pc == 0x80029a94u && f.calls[18].argument[0] == FrameSp + 0x10u &&
        f.calls[19].argument[2] == 0x100u);
    check(f.calls[24].entry == 0x8002d8d4u && f.calls[26].entry == 0x80029bfcu &&
        f.calls[26].argument[0] == 0x800247ecu);
    for (unsigned i = 0; i < 20; ++i)
        check(f.calls[28 + i].pc == 0x80029b20u && f.calls[28 + i].saved_register[0] == i + 1);
    check(f.calls[48].entry == 0x8009dba0u && f.calls[49].entry == 0x8009dbe0u &&
        f.calls[50].entry == 0x8009dbf8u);
    for (unsigned i = 0; i < 20; ++i)
        check(f.calls[51 + i].pc == 0x80029b50u && f.calls[51 + i].saved_register[0] == i + 1);
    check(f.calls[75].entry == 0x800aa468u && f.calls[75].argument[0] == 0x80123400u &&
        f.calls[75].argument[1] == 0x801e0000u && f.calls[75].argument[2] == 0x1410u);
    check(f.calls[76].kind == NBA97_GAME_MAIN_INDIRECT_CALL &&
        f.calls[76].entry == 0x801e0100u && f.calls[76].return_address == 0x80029bb0u &&
        f.calls[76].saved_register[0] == 20u);
    check(!f.progress.stopped_pc && !f.progress.stopped_address && !f.progress.stopped_entry);
}

void returning_epilogue() {
    Fixture f;
    f.mode = Fixture::Return;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed && !f.progress.transferred);
    check(f.progress.operations == 97 && f.progress.accesses == 20 && f.progress.reads == 5);
    check(f.progress.stack_pointer == EntrySp && f.progress.restored_return_address == 0x55667788u);
    check(f.progress.saved_register[0] == 0x56565656u &&
        f.progress.saved_register[1] == 0x34343434u &&
        f.progress.saved_register[2] == 0x12121212u);
}

void refusals_and_unknowns() {
    { Fixture f; f.mode = Fixture::Refuse; f.fail_call = 25;
      check(f.run() == NBA97_TEXT_IO_REFUSED && f.calls.size() == 25 &&
          f.progress.stopped_pc == 0x80029adcu && f.progress.stopped_entry == 0x8002d8d4u &&
          f.progress.callbacks_completed == 24 && f.progress.reached_match_orchestration); }
    { Fixture f; f.mode = Fixture::DirectTransfer; f.fail_call = 1;
      check(f.run() == NBA97_TEXT_ARGUMENT && f.progress.stopped_pc == 0x800299a4u &&
          !f.progress.callbacks_completed); }
    { Fixture f; f.mode = Fixture::InvalidOutcome; f.fail_call = 2;
      check(f.run() == NBA97_TEXT_ARGUMENT && f.progress.stopped_pc == 0x800299acu &&
          f.progress.callbacks_completed == 1); }
    { Fixture f; f.mode = Fixture::MissingImage;
      check(f.run() == NBA97_TEXT_UNKNOWN && f.progress.stopped_pc == 0x80029b04u &&
          f.progress.callbacks_completed == 27); }
    { Fixture f; f.mode = Fixture::MissingSize;
      check(f.run() == NBA97_TEXT_UNKNOWN && f.progress.stopped_pc == 0x80029b10u &&
          f.progress.callbacks_completed == 28); }
    { Fixture f; f.mode = Fixture::UnknownEntry;
      check(f.run() == NBA97_TEXT_UNKNOWN && f.progress.stopped_pc == 0x80029ba0u &&
          f.progress.stopped_address == 0x801e0000u && f.progress.callbacks_completed == 76); }
    { Fixture f; f.mode = Fixture::UnalignedEntry;
      check(f.run() == NBA97_TEXT_ALIGNMENT_TRAP && f.progress.stopped_pc == 0x80029ba8u &&
          f.progress.stopped_entry == 0x801e0102u && f.progress.callbacks_completed == 76); }
}

void memory_and_budget() {
    { Fixture f; f.context.operation_budget = 0;
      check(f.run() == NBA97_TEXT_LIMIT && f.progress.stopped_pc == 0x80029998u &&
          f.progress.stopped_address == FrameSp + 0x24u && !f.progress.operations); }
    { Fixture f; f.context.operation_budget = 4;
      check(f.run() == NBA97_TEXT_LIMIT && f.progress.stores == 4 &&
          f.progress.stopped_pc == 0x800299a4u && f.progress.callbacks_completed == 0); }
    { Fixture f; f.context.operation_budget = 92;
      check(f.run() == NBA97_TEXT_LIMIT && f.progress.callbacks_completed == 76 &&
          f.progress.stopped_pc == 0x80029ba8u && f.progress.stopped_entry == 0x801e0100u); }
    { Fixture f; f.regions[1].size = 0x20;
      check(f.run() == NBA97_TEXT_RESOURCE && f.progress.stopped_pc == 0x80029998u); }
    { Fixture f; *f.known(FrameSp + 0x24u) = 2;
      check(f.run() == NBA97_TEXT_ARGUMENT && !f.progress.stores); }
    { Fixture f; Nba97GameTextRegion overlap[2] = {f.regions[0], f.regions[0]};
      f.context.memory = {overlap, 2}; check(f.run() == NBA97_TEXT_ARGUMENT && !f.progress.operations); }
    Nba97GameMainProgress progress{};
    check(nba97_game_main(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_main(&f.context, nullptr) == NBA97_TEXT_ARGUMENT);
}

struct Composition {
    Fixture game;
    Nba97GameOverlayEntryProgress overlay_progress{};
    Nba97GameMainProgress main_progress{};
    Nba97GameOverlayEntryContext overlay{{game.regions, 2}, 100000,
        0x99887766u, overlayIo, this};
    Composition() {
        game.put(0x800c4b3cu, 0x00800000u);
        game.put(0x800c4b38u, 0x00008000u);
        game.put(0x800c4b14u, 0);
        game.put(0x800c4abcu, 0);
        game.put(0x800c4aa4u, 0);
        game.put(0x800c54acu, 0x7ffu);
        game.put(0x800c54c8u, 0x800c54b0u);
        game.put(0x800c54bcu, 0x80098714u);
        /* Raw GAMEONLY data starts suspended, so the first 0x8008F1D4 call
         * takes its resume branch and the later startup call takes fast path. */
        game.put(0x800c4a70u,1);
        game.put(0x800c4a74u,0);
        game.put(0x800d7a48u,0);
        game.put(0x800c55b8u,0x800c5578u);
        game.put(0x800c55bcu,0x8009cb2cu);
        game.put(0x800c5640u,0x400u,2);
        game.put(0x800c5654u,0x200u,2);
        game.putText(0x800247e4u,"cdrom:");
        game.put(0x800d7a0cu,0x5cu,1);
        game.put(0x800d7a0du,0,1);
        /* Raw BSS state used by 0x800A9CC0's ordinary one-VBlank path. */
        game.put(0x800d7a80u,0);
        game.put(0x800d7a84u,0);
        game.put(0x800d7a88u,0);
        game.put(0x800d7b3cu,0);
        game.put(0x800d7b40u,0);
        game.put(0x800d7b7cu,0);
        for(unsigned i=0;i<32;++i)game.put(0x800d7234u+i*4u,0);
        game.gte_state.cop0_status={0x10900401u,1};
        for(unsigned i=0;i<32;++i)
            game.gte_state.control[i]={0xa5000000u+i,1};
        game.compose_static = true;
        game.compose_global_pointer = true;
        game.compose_heap = true;
        game.compose_cd_directory = true;
        game.compose_path_prefix = true;
        game.compose_directory_cache = true;
        game.compose_interrupt_mask = true;
        game.compose_reset_callback = true;
        game.compose_controller_resume = true;
        game.compose_reset_graph = true;
        game.compose_graph_debug = true;
        game.compose_vblank = true;
        game.compose_clock = true;
        game.compose_gte = true;
        game.compose_clock_delta = true;
        game.compose_presentation_wait = true;
        game.compose_video_environment = true;
    }
    static int overlayIo(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameOverlayEntryEvent* event, Nba97GameOverlayEntryCalleeOutcome* outcome) {
        auto& self = *static_cast<Composition*>(user);
        if (event->kind == NBA97_GAME_OVERLAY_BIOS_A0_39_INIT_HEAP) {
            *outcome = NBA97_GAME_OVERLAY_CALLEE_RETURNED;
            return 1;
        }
        self.game.context.memory = *memory;
        self.game.context.stack_pointer = event->stack_pointer;
        self.game.context.return_address = event->return_address;
        self.game.context.global_pointer = event->global_pointer;
        self.game.context.saved_register[0] = 0;
        self.game.context.saved_register[1] = 0;
        self.game.context.saved_register[2] = 0;
        const auto result = nba97_game_main(&self.game.context, &self.main_progress);
        if (result != NBA97_TEXT_COMPLETE || !self.main_progress.transferred)
            return 0;
        *outcome = NBA97_GAME_OVERLAY_CALLEE_TRANSFERRED;
        return 1;
    }
};

void overlay_composition() {
    Composition c;
    check(nba97_game_overlay_entry(&c.overlay, &c.overlay_progress) == NBA97_TEXT_COMPLETE);
    check(c.overlay_progress.completed && c.overlay_progress.transferred &&
        c.overlay_progress.entered_main && c.main_progress.completed && c.main_progress.transferred);
    check(c.main_progress.frame_stack_pointer == 0x807fffd0u && c.game.calls.size() == 77);
    check(c.game.static_progress.completed && c.game.static_progress.initialized &&
        c.game.get(0x800c4b14u) == 1);
    check(c.game.global_pointer_progress.completed &&
        c.game.global_pointer_progress.stored_global_pointer == 0x800d79c8u &&
        c.game.get(0x800d6e2cu) == 0x800d79c8u);
    check(c.game.heap_progress.completed && c.game.heap_format_calls == 2 &&
        c.game.heap_progress.callbacks_completed == 2 &&
        c.game.heap_progress.return_v0 == 0x000f21e4u);
    check(c.game.heap_progress.accesses == 258 && c.game.heap_progress.events == 250 &&
        c.game.heap_progress.stores == 248);
    check(c.game.get(0x80103d50u) == 0x8010b61cu &&
        c.game.get(0x80103d54u) == 0x8010b644u &&
        c.game.get(0x800eb688u) == 0x8010b66cu &&
        c.game.get(0x800d7c3cu) == 0);
    check(c.game.get(0x8010b620u) == 0x20574f4cu &&
        c.game.get(0x8010b648u) == 0x48474948u);
    check(c.game.cd_directory_progress.completed &&
        c.game.cd_directory_progress.return_v0 == 1 &&
        c.game.cd_directory_progress.operations == 42 &&
        c.game.cd_directory_progress.accesses == 32 &&
        c.game.cd_directory_progress.reads == 17 &&
        c.game.cd_directory_progress.stores == 15);
    check(c.game.cd_calls.size() == 10 &&
        c.game.cd_directory_progress.calls_completed == 10 &&
        !c.game.cd_directory_progress.polls &&
        c.game.cd_global_pointer_progress.completed);
    check(c.game.get(0x800ebc3cu) == 0x100u &&
        c.game.get(0x800fb150u) == 0x110u &&
        c.game.get(0x800d7d3cu) == 23u &&
        c.game.get(0x800d7d40u) == 2048u &&
        c.game.get(0x800c4abcu) == 1);
    check(c.game.path_prefix_progress.completed &&
        c.game.path_prefix_progress.operations == 7 &&
        c.game.path_prefix_progress.accesses == 5 &&
        c.game.path_prefix_progress.reads == 3 &&
        c.game.path_prefix_progress.stores == 2 &&
        c.game.path_prefix_progress.callbacks_completed == 2 &&
        c.game.path_prefix_progress.copied_length == 6 &&
        c.game.path_prefix_progress.final_length == 6 &&
        !c.game.path_prefix_progress.separator_appended);
    check(c.game.path_calls.size() == 2 &&
        c.game.path_calls[0].entry == 0x8009cb6cu &&
        c.game.path_calls[1].entry == 0x8009cb4cu &&
        c.game.path_prefix_progress.restored_register_s0 == 1);
    check(c.game.get(0x800d6dacu) == 0x6f726463u &&
        c.game.get(0x800d6db0u,3) == 0x003a6du);
    check(c.game.directory_cache_progress.completed &&
        c.game.directory_cache_progress.operations == 8 &&
        c.game.directory_cache_progress.accesses == 8 &&
        c.game.directory_cache_progress.reads == 3 &&
        c.game.directory_cache_progress.stores == 5);
    check(c.game.directory_cache_progress.cache_address == 0x8001000cu &&
        c.game.directory_cache_progress.entry_capacity == 0x2c3u &&
        c.game.directory_cache_progress.published_cache_address == 0x8001000cu &&
        c.game.directory_cache_progress.published_entry_capacity == 0x2c3u);
    check(c.game.directory_cache_progress.frame_stack_pointer == FrameSp - 8u &&
        c.game.directory_cache_progress.stack_pointer == FrameSp &&
        c.game.directory_cache_progress.restored_frame_pointer == 0xf3f3f3f3u &&
        c.game.directory_cache_progress.return_v0 == 0x8001000cu);
    check(c.game.get(0x800c4ab8u) == 0x2c3u &&
        c.game.get(0x801046a0u) == 0x8001000cu &&
        /* 0x800914D8 later spills rate 120 into this shared ABI home slot. */
        c.game.get(FrameSp) == 120 &&
        c.game.get(FrameSp + 4u) == 0x2c3u);
    check(c.game.interrupt_mask_progress.completed &&
        c.game.interrupt_mask_progress.operations == 2 &&
        c.game.interrupt_mask_progress.accesses == 2 &&
        c.game.interrupt_mask_progress.reads == 1 &&
        c.game.interrupt_mask_progress.stores == 1);
    check(c.game.interrupt_mask_progress.requested_mask == 0 &&
        c.game.interrupt_mask_progress.previous_mask == 0x7ffu &&
        c.game.interrupt_mask_progress.published_mask == 0 &&
        c.game.interrupt_mask_progress.return_v0 == 0x7ffu &&
        c.game.get(0x800c54acu) == 0);
    check(c.game.reset_callback_progress.completed &&
        c.game.reset_callback_progress.operations == 5 &&
        c.game.reset_callback_progress.accesses == 4 &&
        c.game.reset_callback_progress.reads == 3 &&
        c.game.reset_callback_progress.stores == 1 &&
        c.game.reset_callback_progress.callbacks_completed == 1);
    check(c.game.reset_callback_progress.dispatch_table == 0x800c54b0u &&
        c.game.reset_callback_progress.dispatch_target == 0x80098714u &&
        c.game.reset_callback_progress.frame_stack_pointer == FrameSp - 0x18u &&
        c.game.reset_callback_progress.stack_pointer == FrameSp &&
        c.game.reset_callback_progress.restored_return_address == 0x80029a18u &&
        c.game.reset_callback_progress.return_v0 == 1 &&
        c.game.reset_callback_progress.return_v0_known);
    check(c.game.reset_callback_calls.size() == 1 &&
        c.game.reset_callback_calls[0].pc == 0x800985f4u &&
        c.game.reset_callback_calls[0].entry == 0x80098714u &&
        c.game.reset_callback_calls[0].stack_pointer == FrameSp - 0x18u &&
        c.game.reset_callback_calls[0].return_address == 0x800985fcu);
    check(c.game.controller_resume_invocations == 2 &&
        c.game.controller_resume_calls.size() == 2);
    const auto& resumed=c.game.controller_resume_progress[0];
    const auto& active=c.game.controller_resume_progress[1];
    check(resumed.completed && resumed.input_reinitialized &&
        resumed.operations == 8 && resumed.accesses == 6 &&
        resumed.reads == 2 && resumed.stores == 4 &&
        resumed.callbacks_completed == 2 && resumed.requested_pad_mode == 8 &&
        resumed.initial_suspend_flag == 1 && resumed.clock_snapshot == 37 &&
        resumed.clock_snapshot_known && resumed.return_v0 == 37 &&
        resumed.return_v0_known &&
        resumed.frame_stack_pointer == FrameSp - 0x18u &&
        resumed.stack_pointer == FrameSp &&
        resumed.restored_return_address == 0x80029a20u);
    check(active.completed && !active.input_reinitialized &&
        active.operations == 4 && active.accesses == 4 &&
        active.reads == 2 && active.stores == 2 &&
        active.callbacks_completed == 0 && active.requested_pad_mode == 8 &&
        active.initial_suspend_flag == 0 && active.return_v0 == 0 &&
        active.return_v0_known &&
        active.restored_return_address == 0x80029a38u);
    check(c.game.controller_resume_calls[0].pc == 0x8008f1f4u &&
        c.game.controller_resume_calls[0].entry == 0x80091184u &&
        c.game.controller_resume_calls[0].return_address == 0x8008f1fcu &&
        c.game.controller_resume_calls[1].pc == 0x8008f204u &&
        c.game.controller_resume_calls[1].entry == 0x800a5810u &&
        c.game.controller_resume_calls[1].return_address == 0x8008f20cu);
    check(c.game.reset_graph_progress.completed &&
        c.game.reset_graph_progress.initialized &&
        c.game.reset_graph_progress.requested_mode == 3 &&
        c.game.reset_graph_progress.masked_mode == 3 &&
        c.game.reset_graph_progress.operations == 23 &&
        c.game.reset_graph_progress.accesses == 16 &&
        c.game.reset_graph_progress.reads == 9 &&
        c.game.reset_graph_progress.stores == 7 &&
        c.game.reset_graph_progress.callbacks_completed == 7);
    check(c.game.reset_graph_progress.driver_table == 0x800c5578u &&
        c.game.reset_graph_progress.reset_type == 0 &&
        c.game.reset_graph_progress.display_width == 0x400u &&
        c.game.reset_graph_progress.display_height == 0x200u &&
        c.game.reset_graph_progress.display_width_known &&
        c.game.reset_graph_progress.display_height_known &&
        c.game.reset_graph_progress.return_v0 == 0 &&
        c.game.reset_graph_progress.return_v0_known &&
        c.game.reset_graph_progress.frame_stack_pointer == FrameSp-0x20u &&
        c.game.reset_graph_progress.stack_pointer == FrameSp &&
        c.game.reset_graph_progress.restored_return_address == 0x80029a28u);
    check(c.game.reset_graph_calls.size() == 7 &&
        c.game.reset_graph_calls[0].pc == 0x80099098u &&
        c.game.reset_graph_calls[0].entry == 0x8009cb2cu &&
        c.game.reset_graph_calls[3].pc == 0x800990c8u &&
        c.game.reset_graph_calls[3].entry == 0x8009bda4u &&
        c.game.reset_graph_calls[3].argument[0] == 0x000c5578u &&
        c.game.reset_graph_calls[4].pc == 0x800990d0u &&
        c.game.reset_graph_calls[4].entry == 0x8009b878u &&
        c.game.reset_graph_calls[4].argument[0] == 1);
    check(c.game.reset_graph_reset_callback_progress.completed &&
        c.game.reset_graph_reset_callback_progress.dispatch_target == 0x80098714u &&
        c.game.reset_graph_reset_callback_progress.frame_stack_pointer ==
            FrameSp-0x38u &&
        c.game.reset_graph_reset_callback_progress.restored_return_address ==
            0x800990b8u &&
        c.game.reset_graph_reset_callback_calls.size() == 1);
    check(c.game.graph_debug_progress.completed &&
        c.game.graph_debug_progress.operations == 6 &&
        c.game.graph_debug_progress.accesses == 6 &&
        c.game.graph_debug_progress.reads == 3 &&
        c.game.graph_debug_progress.stores == 3 &&
        c.game.graph_debug_progress.callbacks_completed == 0 &&
        c.game.graph_debug_progress.requested_level == 0 &&
        c.game.graph_debug_progress.previous_level == 0 &&
        c.game.graph_debug_progress.previous_level_known &&
        c.game.graph_debug_progress.published_level == 0 &&
        !c.game.graph_debug_progress.diagnostic_called &&
        c.game.graph_debug_progress.return_v0 == 0 &&
        c.game.graph_debug_progress.return_v0_known);
    check(c.game.graph_debug_progress.frame_stack_pointer == FrameSp-0x18u &&
        c.game.graph_debug_progress.stack_pointer == FrameSp &&
        c.game.graph_debug_progress.restored_return_address == 0x80029a30u &&
        c.game.graph_debug_progress.restored_saved_register_s0 == 1 &&
        c.game.graph_debug_calls.empty() && c.game.get(0x800c55c2u,1) == 0);
    check(c.game.calls[10].pc == 0x80029a28u &&
        c.game.calls[10].entry == 0x800992c4u &&
        c.game.calls[10].argument_count == 1 &&
        c.game.calls[10].argument[0] == 0);
    check(c.game.vblank_progress.completed &&
        c.game.vblank_progress.operations == 54 &&
        c.game.vblank_progress.accesses == 46 &&
        c.game.vblank_progress.reads == 27 &&
        c.game.vblank_progress.stores == 19 &&
        c.game.vblank_progress.callbacks_completed == 8 &&
        c.game.vblank_progress.callback_slots_cleared == 8);
    check(c.game.vblank_progress.interrupt_handler == 0x800a450cu &&
        c.game.vblank_progress.root_counter_spec == 0xf2000003u &&
        c.game.vblank_progress.root_counter_target == 1 &&
        c.game.vblank_progress.root_counter_mode == 0x1000u &&
        c.game.vblank_progress.set_rcnt_return == 0 &&
        c.game.vblank_progress.set_rcnt_return_known &&
        c.game.vblank_progress.start_rcnt_return == 0 &&
        c.game.vblank_progress.start_rcnt_return_known &&
        c.game.vblank_set_rcnt_rejected &&
        c.game.vblank_started_after_rejection);
    check(c.game.vblank_progress.frame_stack_pointer == FrameSp-0x20u &&
        c.game.vblank_progress.stack_pointer == FrameSp &&
        c.game.vblank_progress.global_pointer == 0x800d79c8u &&
        c.game.vblank_progress.restored_return_address == 0x80029a40u &&
        c.game.vblank_progress.restored_frame_pointer == 0xf4f4f4f4u &&
        c.game.vblank_progress.return_v0 == 0 &&
        c.game.vblank_progress.return_v0_known);
    check(c.game.vblank_global_pointer_progress.completed &&
        c.game.vblank_global_pointer_progress.stored_global_pointer ==
            0x800d79c8u && c.game.vblank_calls.size() == 8);
    check(c.game.vblank_calls[0].pc == 0x800a43f8u &&
        c.game.vblank_calls[0].entry == 0x800a4830u &&
        c.game.vblank_calls[3].pc == 0x800a447cu &&
        c.game.vblank_calls[3].entry == 0x8009860cu &&
        c.game.vblank_calls[3].argument[0] == 0 &&
        c.game.vblank_calls[3].argument[1] == 0x800a450cu &&
        c.game.vblank_calls[4].entry == 0x800983b4u &&
        c.game.vblank_calls[4].argument[0] == 0xf2000003u &&
        c.game.vblank_calls[5].entry == 0x80098488u &&
        c.game.vblank_calls[7].entry == 0x800a3e48u);
    for(unsigned i=0;i<8;++i)
        check(c.game.get(0x800d6e0cu+i*4u)==0);
    /* The initializer reset 7A88 to zero; the later 41 acknowledged waits
       advance it once apiece through their explicit VBlank fixtures. */
    check(c.game.get(0x800d7a88u)==41 && c.game.get(0x800d7afcu)==0 &&
        c.game.get(0x800d7b00u)==0 && c.game.calls[12].pc==0x80029a38u &&
        c.game.calls[12].entry==0x800a43e8u &&
        c.game.calls[12].argument_count==0);
    check(c.game.clock_progress.completed &&
        c.game.clock_progress.operations==62 &&
        c.game.clock_progress.accesses==55 &&
        c.game.clock_progress.reads==31 &&
        c.game.clock_progress.stores==24 &&
        c.game.clock_progress.callbacks_completed==7 &&
        c.game.clock_progress.initialized_once &&
        !c.game.clock_progress.initialization_guard_before &&
        c.game.clock_progress.callback_slots_cleared==8);
    check(c.game.clock_progress.incoming_rate==120 &&
        c.game.clock_progress.live_rate_divisor==120 &&
        c.game.clock_progress.clock_base==0x409980u &&
        c.game.clock_progress.timer_target==35280 &&
        c.game.clock_progress.effective_rate==120 &&
        c.game.clock_progress.interrupt_handler==0x800916b4u &&
        c.game.clock_progress.shutdown_handler==0x8009167cu &&
        c.game.clock_progress.root_counter_spec==0xf2000002u &&
        c.game.clock_progress.root_counter_mode==0x1000u);
    check(c.game.clock_progress.set_rcnt_return==1 &&
        c.game.clock_progress.set_rcnt_return_known &&
        c.game.clock_progress.start_rcnt_return==1 &&
        c.game.clock_progress.start_rcnt_return_known &&
        c.game.clock_progress.return_v0==0 &&
        c.game.clock_progress.return_v0_known &&
        !c.game.clock_progress.trap_code);
    check(c.game.clock_progress.frame_stack_pointer==FrameSp-0x20u &&
        c.game.clock_progress.stack_pointer==FrameSp &&
        c.game.clock_progress.global_pointer==0x800d79c8u &&
        c.game.clock_progress.restored_return_address==0x80029a54u &&
        c.game.clock_progress.restored_frame_pointer==0xf5f5f5f5u);
    check(c.game.clock_calls.size()==7 &&
        c.game.clock_calls[0].pc==0x800914ecu &&
        c.game.clock_calls[0].entry==0x80098394u &&
        c.game.clock_calls[1].pc==0x80091578u &&
        c.game.clock_calls[1].entry==0x8009860cu &&
        c.game.clock_calls[1].argument[0]==6 &&
        c.game.clock_calls[1].argument[1]==0x800916b4u &&
        c.game.clock_calls[2].entry==0x800a575cu &&
        c.game.clock_calls[2].argument[0]==0x8009167cu &&
        c.game.clock_calls[3].entry==0x800983b4u &&
        c.game.clock_calls[3].argument[0]==0xf2000002u &&
        c.game.clock_calls[3].argument[1]==35280 &&
        c.game.clock_calls[4].entry==0x80098488u &&
        c.game.clock_calls[6].entry==0x800a5880u);
    check(c.game.clock_interrupt_installed &&
        c.game.clock_shutdown_registered && c.game.clock_counter_set &&
        c.game.clock_counter_started && !c.game.clock_critical_section &&
        c.game.get(0x800c4aa4u)==1 && c.game.get(0x800d7234u)==0x8009167cu &&
        c.game.get(0x800d7a98u)==35280 && c.game.get(0x800d7a94u)==120 &&
        c.game.get(0x800d7a78u)==0 && c.game.get(0x800d7a7cu)==0 &&
        c.game.get(0x800d7a70u)==0 && c.game.get(0x800d7b2cu)==0 &&
        c.game.get(0x800d7b28u)==0);
    for(unsigned i=0;i<8;++i)
        check(c.game.get(0x800d6decu+i*4u)==0);
    check(c.game.calls[13].pc==0x80029a4cu &&
        c.game.calls[13].entry==0x800914d8u &&
        c.game.calls[13].argument_count==1 &&
        c.game.calls[13].argument[0]==120);
    check(c.game.gte_progress.completed &&
        c.game.gte_progress.operations==9 &&
        c.game.gte_progress.reads==1 &&
        c.game.gte_progress.stores==8 &&
        c.game.gte_progress.controls_written==7 &&
        c.game.gte_progress.status_before==0x10900401u &&
        c.game.gte_progress.status_after==0x50900401u &&
        c.game.gte_progress.return_v0==0x50900401u &&
        c.game.gte_progress.return_v0_known);
    check(c.game.gte_state.cop0_status.word==0x50900401u &&
        c.game.gte_state.control[NBA97_GAME_GTE_ZSF3].word==0x155u &&
        c.game.gte_state.control[NBA97_GAME_GTE_ZSF4].word==0x100u &&
        c.game.gte_state.control[NBA97_GAME_GTE_H].word==1000u &&
        c.game.gte_state.control[NBA97_GAME_GTE_DQA].word==0xffffef9eu &&
        c.game.gte_state.control[NBA97_GAME_GTE_DQB].word==0x01400000u &&
        c.game.gte_state.control[NBA97_GAME_GTE_OFX].word==0 &&
        c.game.gte_state.control[NBA97_GAME_GTE_OFY].word==0 &&
        /* FLAG is one of the 25 controls the source deliberately leaves live. */
        c.game.gte_state.control[31].word==0xa500001fu);
    check(c.game.calls[14].pc==0x80029a54u &&
        c.game.calls[14].entry==0x80056678u &&
        c.game.calls[14].argument_count==0);
    check(c.game.clock_delta_progress.completed &&
        c.game.clock_delta_progress.operations==7 &&
        c.game.clock_delta_progress.accesses==6 &&
        c.game.clock_delta_progress.reads==3 &&
        c.game.clock_delta_progress.stores==3 &&
        c.game.clock_delta_progress.callbacks_completed==1 &&
        c.game.clock_delta_progress.previous_snapshot==0 &&
        c.game.clock_delta_progress.sampled_clock==0 &&
        c.game.clock_delta_progress.sampled_clock_known &&
        c.game.clock_delta_progress.return_v0==0 &&
        c.game.clock_delta_progress.return_v0_known &&
        c.game.clock_delta_progress.snapshot_address==0x800d7b2cu &&
        c.game.clock_delta_progress.restored_return_address==0x80029a64u &&
        c.game.clock_delta_progress.restored_saved_register_s0==1);
    check(c.game.clock_delta_calls.size()==1 &&
        c.game.clock_delta_calls[0].pc==0x800a585cu &&
        c.game.clock_delta_calls[0].entry==0x800a5810u &&
        c.game.clock_delta_calls[0].global_pointer==0x800d79c8u);
    check(c.game.calls[15].pc==0x80029a5cu &&
        c.game.calls[15].entry==0x800a584cu &&
        c.game.calls[15].argument_count==0 &&
        c.game.calls[15].saved_register[0]==1);
    check(c.game.presentation_wait_invocations==41 &&
        c.game.presentation_vblank_signals==41 &&
        c.game.presentation_wait_calls.size()==41);
    for(const auto& wait:c.game.presentation_wait_progress)
        check(wait.completed && wait.operations==3 && wait.accesses==2 &&
            wait.reads==1 && wait.stores==1 && wait.callbacks_completed==1 &&
            wait.frame_stack_pointer==FrameSp-0x18u &&
            wait.stack_pointer==FrameSp && wait.global_pointer==0x800d79c8u &&
            wait.service_entry==0x800a9cc0u && wait.return_v0==1 &&
            wait.return_v0_known);
    check(c.game.presentation_wait_progress[0].restored_return_address==
            0x80029a6cu &&
        c.game.presentation_wait_progress[1].restored_return_address==
            0x80029b28u &&
        c.game.presentation_wait_progress[20].restored_return_address==
            0x80029b28u &&
        c.game.presentation_wait_progress[21].restored_return_address==
            0x80029b58u &&
        c.game.presentation_wait_progress[40].restored_return_address==
            0x80029b58u);
    check(c.game.presentation_wait_calls[0].pc==0x80029be4u &&
        c.game.presentation_wait_calls[0].entry==0x800a9cc0u &&
        c.game.presentation_wait_calls[0].stack_pointer==FrameSp-0x18u &&
        c.game.presentation_wait_calls[0].return_address==0x80029becu &&
        c.game.calls[16].pc==0x80029a64u &&
        c.game.calls[16].entry==0x80029bdcu &&
        c.game.calls[28].pc==0x80029b20u &&
        c.game.calls[47].pc==0x80029b20u &&
        c.game.calls[51].pc==0x80029b50u &&
        c.game.calls[70].pc==0x80029b50u);
    check(c.game.video_environment_invocations==1 &&
        c.game.video_environment_child_callbacks==9 &&
        c.game.video_environment_calls.size()==9 &&
        c.game.video_environment_progress.completed &&
        c.game.video_environment_progress.operations==44 &&
        c.game.video_environment_progress.accesses==35 &&
        c.game.video_environment_progress.reads==7 &&
        c.game.video_environment_progress.stores==28 &&
        c.game.video_environment_progress.callbacks_completed==9 &&
        c.game.video_environment_progress.direct_control_bytes_written==16 &&
        c.game.video_environment_progress.frame_stack_pointer==FrameSp-0x38u &&
        c.game.video_environment_progress.stack_pointer==FrameSp &&
        c.game.video_environment_progress.requested_background_mode==0 &&
        c.game.video_environment_progress.background_byte==0 &&
        c.game.video_environment_progress.restored_return_address==0x80029a74u &&
        c.game.video_environment_progress.restored_saved_register[0]==1 &&
        c.game.video_environment_progress.restored_saved_register[1]==0 &&
        c.game.video_environment_progress.restored_saved_register[2]==0 &&
        c.game.video_environment_progress.restored_saved_register[3]==0xd3d3d3d3u &&
        c.game.video_environment_progress.restored_saved_register[4]==0xd4d4d4d4u &&
        c.game.video_environment_progress.restored_saved_register[5]==0xd5d5d5d5u &&
        c.game.video_environment_progress.return_v0==0 &&
        c.game.video_environment_progress.return_v0_known);
    check(c.game.calls[17].pc==0x80029a6cu &&
        c.game.calls[17].entry==0x80029f20u &&
        c.game.calls[17].argument_count==1 &&
        c.game.calls[17].argument[0]==0 &&
        c.game.video_environment_calls[0].kind==
            NBA97_GAME_VIDEO_SET_DEF_DISP_ENV &&
        c.game.video_environment_calls[2].kind==
            NBA97_GAME_VIDEO_SET_DEF_DRAW_ENV &&
        c.game.video_environment_calls[4].kind==
            NBA97_GAME_VIDEO_PUT_DISP_ENV &&
        c.game.video_environment_calls[5].kind==
            NBA97_GAME_VIDEO_PUT_DRAW_ENV &&
        c.game.video_environment_calls[8].kind==
            NBA97_GAME_VIDEO_DRAW_SYNC);
    check(c.game.get(0x8002205cu+2u,2)==0x100u &&
        c.game.get(0x8002205cu+4u,2)==0x200u &&
        c.game.get(0x8002205cu+6u,2)==0xf0u &&
        c.game.get(0x80022070u+2u,2)==0 &&
        c.game.get(0x80021eecu+2u,2)==0 &&
        c.game.get(0x80021f48u+2u,2)==0x100u &&
        c.game.get(0x80021eecu+22u,1)==0 &&
        c.game.get(0x80021f48u+22u,1)==0 &&
        c.game.get(0x80021eecu+23u,1)==1 &&
        c.game.get(0x80021f48u+23u,1)==1 &&
        c.game.get(0x80021eecu+24u,1)==0 &&
        c.game.get(0x80021f48u+24u,1)==0 &&
        c.game.get(0x80021fa4u+22u,1)==0 &&
        c.game.get(0x80021fa4u+23u,1)==0xcdu &&
        c.game.get(0x80021fa4u+24u,1)==0 &&
        c.game.get(0x80022000u+22u,1)==0 &&
        c.game.get(0x80022000u+23u,1)==0xcdu &&
        c.game.get(0x80022000u+24u,1)==0 &&
        c.game.get(0x8002206du,1)==0 &&
        c.game.get(0x80022081u,1)==0 &&
        c.game.get(0x8001ede8u)==0 &&
        c.game.active_display_environment==0x80022070u &&
        c.game.active_draw_environment==0x80021f48u &&
        c.game.video_environment_synchronized);
    check(c.game.get(0x800c55c0u) == 0x00000100u &&
        c.game.get(0x800c55c4u) == 0x02000400u &&
        c.game.get(0x800c55d0u) == UINT32_MAX &&
        c.game.get(0x800c562cu) == UINT32_MAX);
    check(c.game.get(0x800c4a70u) == 0 &&
        c.game.get(0x800c4a74u) == 37 &&
        c.game.get(0x800d7a48u) == 8 &&
        /* All 41 0x80029BDC invocations reuse the clock sampler's saved-s0
           slot; the final live spill is the second loop's return address. */
        c.game.get(FrameSp - 8u) == 0x80029b58u &&
        c.game.get(FrameSp - 4u) == 0x80029a64u);
    check(c.game.get(0x800d7a80u)==1 && c.game.get(0x800d7a84u)==0 &&
        c.game.get(0x800d7a88u)==41 && c.game.get(0x800d7b3cu)==0 &&
        c.game.get(0x800d7b40u)==0 && c.game.get(0x800d7b7cu)==0);
    check(c.game.get(0x800d7bb8u) == 0x99887766u &&
        c.overlay_progress.restored_return_address == 0x99887766u);
}
}

int main() {
    transferred_path();
    returning_epilogue();
    refusals_and_unknowns();
    memory_and_budget();
    overlay_composition();
    std::printf("game_main: %u checks passed\n", checks);
}
