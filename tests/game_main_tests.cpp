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
    unsigned heap_format_calls = 0;
    unsigned controller_resume_invocations = 0;
    bool vblank_set_rcnt_rejected = false;
    bool vblank_started_after_rejection = false;
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
        c.game.get(FrameSp) == 0x8001000cu &&
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
    check(c.game.get(0x800d7a88u)==0 && c.game.get(0x800d7afcu)==0 &&
        c.game.get(0x800d7b00u)==0 && c.game.calls[12].pc==0x80029a38u &&
        c.game.calls[12].entry==0x800a43e8u &&
        c.game.calls[12].argument_count==0);
    check(c.game.get(0x800c55c0u) == 0x00000100u &&
        c.game.get(0x800c55c4u) == 0x02000400u &&
        c.game.get(0x800c55d0u) == UINT32_MAX &&
        c.game.get(0x800c562cu) == UINT32_MAX);
    check(c.game.get(0x800c4a70u) == 0 &&
        c.game.get(0x800c4a74u) == 37 &&
        c.game.get(0x800d7a48u) == 8 &&
        /* The later 0x800A43E8 frame legitimately reuses the earlier
           controller/graph stack slot with its saved incoming fp. */
        c.game.get(FrameSp - 8u) == 0xf4f4f4f4u);
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
