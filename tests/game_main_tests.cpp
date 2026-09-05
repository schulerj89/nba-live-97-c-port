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
#include "recovered/game_move_image.h"
#include "recovered/game_gpu_sync.h"
#include "recovered/game_display_mask_set.h"
#include "recovered/game_resource_validator_install.h"
#include "recovered/game_frame_rate_reset.h"
#include "recovered/game_match_session.h"
#include "recovered/game_loading_screen.h"
#include "recovered/game_resource_loader.h"
#include "recovered/game_heap_payload_size.h"
#include "recovered/game_cd_sync.h"
#include "recovered/game_cd_ready_callback.h"
#include "recovered/game_cd_sync_callback.h"
#include "recovered/game_vblank_shutdown.h"
#include "recovered/game_clock_shutdown.h"
#include "recovered/game_controller_suspend.h"
#include "recovered/game_memory_zero.h"
#include "recovered/game_heap_release.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game main check %u failed at line %u\n", checks,
            line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

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
    std::array<Nba97GameMoveImageProgress,2> move_image_progress{};
    Nba97GameGpuSyncState gpu_sync_state{};
    Nba97GameGpuSyncProgress gpu_sync_progress{};
    Nba97GameGpuSyncWord gpu_sync_source_v0{};
    Nba97GameDisplayMaskSetProgress display_mask_progress{};
    Nba97GameResourceValidatorInstallProgress resource_validator_progress{};
    Nba97GameFrameRateResetProgress frame_rate_reset_progress{};
    Nba97GameMatchSessionProgress match_session_progress{};
    Nba97GameLoadingScreenProgress loading_screen_progress{};
    std::array<Nba97GameResourceLoaderProgress,2> resource_loader_progress{};
    Nba97GameHeapPayloadSizeProgress heap_payload_size_progress{};
    Nba97GameHeapReleaseProgress heap_payload_lookup_progress{};
    Nba97GameCdSyncProgress cd_sync_progress{};
    Nba97GameCdReadyCallbackProgress cd_ready_callback_progress{};
    Nba97GameCdSyncCallbackProgress cd_sync_callback_progress{};
    Nba97GameVblankShutdownProgress vblank_shutdown_progress{};
    Nba97GameClockShutdownProgress clock_shutdown_progress{};
    Nba97GameControllerSuspendProgress controller_suspend_progress{};
    Nba97GameMemoryZeroProgress memory_zero_progress{};
    Nba97GameFrameRateResetProgress match_session_frame_rate_reset_progress{};
    std::array<Nba97GamePresentationWaitProgress,11>
        match_session_presentation_wait_progress{};
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
    std::vector<Nba97GameMoveImageEvent> move_image_calls;
    std::vector<Nba97GameGpuSyncAccess> gpu_sync_reads;
    std::vector<Nba97GameGpuSyncWrite> gpu_sync_writes;
    std::vector<Nba97GameGpuSyncCall> gpu_sync_callbacks;
    std::vector<Nba97GameDisplayMaskSetEvent> display_mask_calls;
    std::vector<Nba97GameFrameRateResetEvent> frame_rate_reset_calls;
    std::vector<Nba97GameMatchSessionEvent> match_session_calls;
    std::vector<Nba97GameLoadingScreenEvent> loading_screen_calls;
    std::vector<Nba97GameResourceLoaderEvent> resource_loader_calls;
    std::vector<Nba97GameHeapPayloadSizeEvent> heap_payload_size_calls;
    std::vector<Nba97GameCdSyncEvent> cd_sync_calls;
    std::vector<Nba97GameVblankShutdownEvent> vblank_shutdown_calls;
    std::vector<Nba97GameClockShutdownEvent> clock_shutdown_calls;
    std::vector<Nba97GameControllerSuspendEvent> controller_suspend_calls;
    std::vector<Nba97GameFrameRateResetEvent>
        match_session_frame_rate_reset_calls;
    std::vector<Nba97GamePresentationWaitEvent>
        match_session_presentation_wait_calls;
    unsigned heap_format_calls = 0;
    unsigned controller_resume_invocations = 0;
    unsigned presentation_wait_invocations = 0;
    unsigned presentation_vblank_signals = 0;
    unsigned video_environment_invocations = 0;
    unsigned video_environment_child_callbacks = 0;
    unsigned move_image_invocations = 0;
    unsigned move_image_child_callbacks = 0;
    unsigned gpu_sync_invocations = 0;
    unsigned gpu_sync_dispatch_resolutions = 0;
    unsigned gpu_sync_backend_observations = 0;
    unsigned display_mask_invocations = 0;
    unsigned display_mask_child_callbacks = 0;
    unsigned resource_validator_install_invocations = 0;
    unsigned frame_rate_reset_invocations = 0;
    unsigned frame_rate_reset_child_callbacks = 0;
    unsigned match_session_invocations = 0;
    unsigned match_session_frame_rate_reset_child_callbacks = 0;
    unsigned match_session_presentation_wait_invocations = 0;
    unsigned match_session_vblank_signals = 0;
    unsigned loading_screen_invocations = 0;
    unsigned resource_loader_invocations = 0;
    unsigned heap_payload_size_invocations = 0;
    unsigned cd_sync_invocations = 0;
    unsigned cd_ready_callback_invocations = 0;
    unsigned cd_sync_callback_invocations = 0;
    unsigned vblank_shutdown_invocations = 0;
    unsigned vblank_shutdown_child_callbacks = 0;
    unsigned clock_shutdown_invocations = 0;
    unsigned clock_shutdown_child_callbacks = 0;
    unsigned controller_suspend_invocations = 0;
    unsigned controller_suspend_child_callbacks = 0;
    unsigned memory_zero_invocations = 0;
    unsigned gpu_sync_dma_busy_reads = 0;
    std::uint64_t gpu_submitted = 0;
    std::uint64_t gpu_completed = 0;
    bool gpu_idle = true;
    std::uint32_t gpu_i_mask = 0;
    std::uint32_t gpu_dma_chcr = 0;
    std::uint32_t gpu_status = 0x04000000u;
    std::uint32_t gpu_read = 0;
    std::uint32_t gpu_dpcr = 0;
    std::uint32_t gpu_timer_status = 0;
    std::uint32_t gpu_timer_count = 0;
    std::uint32_t display_control_word = UINT32_MAX;
    bool display_visible = false;
    std::uint32_t active_display_environment = 0;
    std::uint32_t active_draw_environment = 0;
    bool video_environment_synchronized = false;
    bool vblank_set_rcnt_rejected = false;
    bool vblank_started_after_rejection = false;
    bool vblank_interrupt_installed = false;
    bool clock_critical_section = false;
    bool clock_interrupt_installed = false;
    bool clock_interrupt_was_installed = false;
    bool clock_shutdown_registered = false;
    bool controller_shutdown_service_called = false;
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
    bool compose_move_image = false;
    bool compose_gpu_sync = false;
    bool compose_display_mask = false;
    bool compose_resource_validator_install = false;
    bool compose_frame_rate_reset = false;
    bool compose_match_session = false;
    bool compose_loading_screen = false;
    bool compose_resource_loader = false;
    bool compose_heap_payload_size = false;
    bool compose_cd_sync = false;
    bool compose_cd_ready_callback = false;
    bool compose_cd_sync_callback = false;
    bool compose_vblank_shutdown = false;
    bool compose_clock_shutdown = false;
    bool compose_controller_suspend = false;
    bool compose_memory_zero = false;

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
    bool installFeloadDescriptor() {
        const auto descriptor=get(0x800eb688u);
        if(descriptor!=0x8010b66cu ||
           get(0x80103d50u)!=0x8010b61cu ||
           get(0x80103d54u)!=0x8010b644u)return false;
        const auto next_free=get(descriptor+0x20u);
        /* Model the retained allocation that still-unrecovered loader 941C8
           owns, so main's 90D60 child can use the real recovered 90618 lookup
           instead of receiving another hard-coded outer-boundary size. */
        put(0x800eb688u,next_free);
        put(descriptor,0x80123400u);
        put(descriptor+0x10u,0x1410u);
        put(descriptor+0x14u,0x1410u);
        put(descriptor+0x18u,0);
        put(descriptor+0x20u,0x8010b644u);
        put(descriptor+0x24u,0x8010b61cu);
        put(0x8010b61cu+0x20u,descriptor);
        put(0x8010b644u+0x24u,descriptor);
        return true;
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
        if(event->entry==0x8009860cu) {
            if(f.get(0x800c54d0u)!=0)return 0;
            f.put(0x800c54d0u,0x800a450cu);
            f.vblank_interrupt_installed=true;
            return 1;
        }
        return event->entry==0x800994f4u || event->entry==0x80098394u ||
            event->entry==0x80098594u;
    }
    static int vblankShutdownIo(void* user,const Nba97GameTextMemory*,
        const Nba97GameVblankShutdownEvent* event,
        Nba97GameVblankShutdownValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        ++f.vblank_shutdown_child_callbacks;
        f.vblank_shutdown_calls.push_back(*event);
        if(!f.vblank_interrupt_installed || f.get(0x800c54d0u)!=0x800a450cu ||
           event->pc!=0x800a44ecu ||
           event->entry!=0x8009860cu || event->argument_count!=2 ||
           event->argument[0]!=0 || event->argument[1]!=0)return 0;
        f.put(0x800c54d0u,0);
        f.vblank_interrupt_installed=false;
        *value={0x800a450cu,1};
        return 1;
    }
    static int clockShutdownIo(void* user,const Nba97GameTextMemory*,
        const Nba97GameClockShutdownEvent* event,
        Nba97GameClockShutdownValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        ++f.clock_shutdown_child_callbacks;
        f.clock_shutdown_calls.push_back(*event);
        if(!f.clock_interrupt_installed || f.get(0x800c54e8u)!=0x800916b4u ||
           event->pc!=0x80091694u ||
           event->entry!=0x8009860cu || event->argument_count!=2 ||
           event->argument[0]!=6 || event->argument[1]!=0)return 0;
        f.put(0x800c54e8u,0);
        f.clock_interrupt_installed=false;
        *value={0x800916b4u,1};
        return 1;
    }
    static int controllerSuspendIo(void* user,const Nba97GameTextMemory*,
        const Nba97GameControllerSuspendEvent* event,
        Nba97GameControllerSuspendValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        ++f.controller_suspend_child_callbacks;
        f.controller_suspend_calls.push_back(*event);
        if(f.get(0x800c4a70u)!=0 ||
           event->kind!=NBA97_GAME_CONTROLLER_SUSPEND_SHUTDOWN ||
           event->pc!=0x8008f1b0u || event->entry!=0x80091224u ||
           event->argument_count!=0)return 0;
        f.controller_shutdown_service_called=true;
        /* 0x8008F19C deliberately discards this unknown child result. */
        *value={0xdeadbeefu,0};
        return 1;
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
            if(f.get(0x800c54e8u)!=0)return 0;
            f.put(0x800c54e8u,0x800916b4u);
            f.clock_interrupt_installed=true;
            f.clock_interrupt_was_installed=true;
            return 1;
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
    static int frameRateResetIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameFrameRateResetEvent* event,
        Nba97GameFrameRateResetValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        ++f.frame_rate_reset_child_callbacks;
        f.frame_rate_reset_calls.push_back(*event);
        if(event->kind!=NBA97_GAME_FRAME_RATE_RESET_READ_CLOCK ||
           event->pc!=0x800a7754u || event->entry!=0x800a5810u ||
           event->stack_pointer!=FrameSp-0x18u ||
           event->global_pointer!=0x800d79c8u ||
           event->return_address!=0x800a775cu || event->argument_count ||
           f.get(0x800d7b44u)!=0 || f.get(0x800d7b48u)!=0 ||
           f.get(0x800d7b4cu)!=0x22222222u ||
           f.get(0x800d7b50u)!=0 || f.get(0x800d7b54u)!=0 ||
           f.get(0x800d7b58u)!=0)
            return 0;
        /* Exact 0x800A5810 fixture: return the retained game clock. All five
           clears must already be visible, while the old baseline remains. */
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
    static int matchSessionFrameRateResetIo(void* user,
        const Nba97GameTextMemory*,
        const Nba97GameFrameRateResetEvent* event,
        Nba97GameFrameRateResetValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        ++f.match_session_frame_rate_reset_child_callbacks;
        f.match_session_frame_rate_reset_calls.push_back(*event);
        if(event->kind!=NBA97_GAME_FRAME_RATE_RESET_READ_CLOCK ||
           event->pc!=0x800a7754u || event->entry!=0x800a5810u ||
           event->stack_pointer!=FrameSp-0x40u ||
           event->global_pointer!=0x800d79c8u ||
           event->return_address!=0x800a775cu || event->argument_count ||
           f.get(0x800d7b44u)!=0 || f.get(0x800d7b48u)!=0 ||
           f.get(0x800d7b4cu)!=0 || f.get(0x800d7b50u)!=0 ||
           f.get(0x800d7b54u)!=0 || f.get(0x800d7b58u)!=0)
            return 0;
        *value={f.get(0x800d7a70u),1};
        return 1;
    }
    static int matchSessionPresentationWaitIo(void* user,
        const Nba97GameTextMemory*,
        const Nba97GamePresentationWaitEvent* event,
        Nba97GamePresentationWaitValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.match_session_presentation_wait_calls.push_back(*event);
        if(event->kind!=NBA97_GAME_PRESENTATION_WAIT_SERVICE ||
           event->pc!=0x80029be4u || event->entry!=0x800a9cc0u ||
           event->stack_pointer!=FrameSp-0x40u ||
           event->global_pointer!=0x800d79c8u ||
           event->return_address!=0x80029becu || event->argument_count ||
           f.get(event->global_pointer+0x1b4u)!=0 ||
           f.get(0x800d7a84u)!=0 || f.get(0x800d7b3cu)!=0 ||
           f.get(0x800d7b40u)!=0)
            return 0;
        /* Deterministic source-service fixture: no host clock or input is
           synthesized. Each recovered wrapper observes one acknowledged
           VBlank exactly as the existing click-through does. */
        f.put(0x800d7a80u,0);
        f.put(0x800d7a80u,1);
        f.put(0x800d7a88u,f.get(0x800d7a88u)+1u);
        f.put(event->global_pointer+0x1b4u,0);
        ++f.match_session_vblank_signals;
        *value={1,1};
        return 1;
    }
    static int matchSessionIo(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameMatchSessionEvent* event,
        Nba97GameMatchSessionValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.match_session_calls.push_back(*event);
        if(event->stack_pointer!=FrameSp-0x28u ||
           event->global_pointer!=0x800d79c8u ||
           event->return_address!=event->pc+8u)
            return 0;
        *value={0,1};
        switch(event->kind) {
        case NBA97_GAME_MATCH_SESSION_CLEAR_RECTANGLE:
            if(event->entry!=0x800aa0bcu || event->argument_count!=5)
                return 0;
            return 1;
        case NBA97_GAME_MATCH_SESSION_FRAME_RATE_RESET: {
            if(event->pc!=0x8002d908u || event->entry!=0x800a7738u ||
               event->argument_count)
                return 0;
            Nba97GameFrameRateResetContext context{*memory,20,
                event->stack_pointer,event->return_address,
                event->global_pointer,matchSessionFrameRateResetIo,&f};
            if(nba97_game_frame_rate_reset(&context,
                   &f.match_session_frame_rate_reset_progress)!=
                       NBA97_TEXT_COMPLETE)
                return 0;
            *value={f.match_session_frame_rate_reset_progress.return_v0,
                f.match_session_frame_rate_reset_progress.return_v0_known};
            return 1;
        }
        case NBA97_GAME_MATCH_SESSION_SET_DEF_DRAW_ENV:
        case NBA97_GAME_MATCH_SESSION_SET_DEF_DISP_ENV: {
            const bool draw=event->kind==
                NBA97_GAME_MATCH_SESSION_SET_DEF_DRAW_ENV;
            if(event->entry!=(draw ? 0x8009ca00u : 0x8009cad0u) ||
               event->argument_count!=5)
                return 0;
            const auto p=event->argument[0];
            f.put(p,event->argument[1],2);
            f.put(p+2u,event->argument[2],2);
            f.put(p+4u,event->argument[3],2);
            f.put(p+6u,event->argument[4],2);
            if(draw) {
                f.put(p+8u,event->argument[1],2);
                f.put(p+10u,event->argument[2],2);
                for(unsigned offset=12;offset<20;offset+=2)
                    f.put(p+offset,0,2);
                f.put(p+20u,10,2);
                f.put(p+22u,1,1);
                f.put(p+23u,1,1);
                for(unsigned offset=24;offset<28;++offset)
                    f.put(p+offset,0,1);
            } else {
                for(unsigned offset=8;offset<16;offset+=2)
                    f.put(p+offset,0,2);
                for(unsigned offset=16;offset<20;++offset)
                    f.put(p+offset,0,1);
            }
            *value={p,1};
            return 1;
        }
        case NBA97_GAME_MATCH_SESSION_INITIALIZE:
        case NBA97_GAME_MATCH_SESSION_LOAD_SCENE:
        case NBA97_GAME_MATCH_SESSION_RUN_LOOP:
        case NBA97_GAME_MATCH_SESSION_TEARDOWN:
            if(event->argument_count)
                return 0;
            return 1;
        case NBA97_GAME_MATCH_SESSION_PRESENTATION_WAIT: {
            if(event->entry!=0x80029bdcu || event->argument_count ||
               f.match_session_presentation_wait_invocations>=
                   f.match_session_presentation_wait_progress.size())
                return 0;
            auto& progress=f.match_session_presentation_wait_progress[
                f.match_session_presentation_wait_invocations];
            Nba97GamePresentationWaitContext context{*memory,10,
                event->stack_pointer,event->return_address,
                event->global_pointer,matchSessionPresentationWaitIo,&f};
            if(nba97_game_presentation_wait(&context,&progress)!=
                    NBA97_TEXT_COMPLETE)
                return 0;
            ++f.match_session_presentation_wait_invocations;
            *value={progress.return_v0,progress.return_v0_known};
            return 1;
        }
        case NBA97_GAME_MATCH_SESSION_DRAW_SYNC:
            /* DrawSync is already recovered independently. This parent test
               acknowledges its synchronous boundary without inventing a GPU
               packet; the scanout remains available for visual comparison. */
            if(event->entry!=0x800994f4u || event->argument_count!=1 ||
               event->argument[0]!=0)
                return 0;
            return 1;
        default:
            return 0;
        }
    }
    static int resourceLoaderIo(void* user,const Nba97GameTextMemory*,
        const Nba97GameResourceLoaderEvent* event,
        Nba97GameResourceLoaderValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        const auto invocation=f.resource_loader_invocations;
        const std::uint32_t filename=invocation ? 0x800247ecu : 0x800247f8u;
        const std::uint32_t frame_sp=invocation ? FrameSp-0x20u :
            FrameSp-0x48u;
        const std::uint32_t resource=invocation ? 0x80123400u : 0x80130000u;
        if(invocation>=2 || event->kind!=NBA97_GAME_RESOURCE_LOADER_ATTEMPT ||
           event->pc!=0x80029c18u || event->entry!=0x800941c8u ||
           event->argument_count!=2 || event->argument[0]!=filename ||
           event->argument[1]!=0 || event->stack_pointer!=frame_sp ||
           event->global_pointer!=0x800d79c8u ||
           event->saved_register[0]!=filename ||
           event->saved_register[1]!=0 ||
           !event->saved_register_known[0] ||
           !event->saved_register_known[1] ||
           event->return_address!=0x80029c20u)
            return 0;
        f.resource_loader_calls.push_back(*event);
        if(invocation==1 && f.compose_heap_payload_size &&
           !f.installFeloadDescriptor())return 0;
        *value={resource,1};
        return 1;
    }
    static int runResourceLoader(Fixture& f,const Nba97GameTextMemory* memory,
        std::uint32_t filename,std::uint32_t flags,std::uint32_t stack_pointer,
        std::uint32_t return_address,const std::uint32_t* saved_register,
        std::uint32_t global_pointer,Nba97GameResourceLoaderValue* value) {
        if(f.resource_loader_invocations>=f.resource_loader_progress.size())
            return 0;
        auto& progress=
            f.resource_loader_progress[f.resource_loader_invocations];
        Nba97GameResourceLoaderContext context{*memory,20,filename,flags,
            stack_pointer,return_address,
            {saved_register[0],saved_register[1]},global_pointer,
            resourceLoaderIo,&f};
        if(nba97_game_resource_loader(&context,&progress)!=NBA97_TEXT_COMPLETE)
            return 0;
        ++f.resource_loader_invocations;
        *value={progress.return_v0,progress.return_v0_known};
        return 1;
    }
    static int heapPayloadSizeIo(void* user,
        const Nba97GameTextMemory* memory,
        const Nba97GameHeapPayloadSizeEvent* event,
        Nba97GameHeapPayloadSizeValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        if(event->kind!=NBA97_GAME_HEAP_PAYLOAD_SIZE_FIND_DESCRIPTOR ||
           event->pc!=0x80090d68u || event->entry!=0x80090618u ||
           event->argument_count!=1 ||
           event->argument[0]!=0x80123400u ||
           event->stack_pointer!=FrameSp-0x18u ||
           event->global_pointer!=0x800d79c8u ||
           event->return_address!=0x80090d70u)return 0;
        f.heap_payload_size_calls.push_back(*event);
        Nba97GameHeapReleaseContext lookup{*memory,100};
        if(nba97_game_heap_release(&lookup,NBA97_HEAP_FIND_90618,
               event->argument[0],{0,0},nullptr,0,
               &f.heap_payload_lookup_progress)!=NBA97_TEXT_COMPLETE ||
           !f.heap_payload_lookup_progress.completed)return 0;
        *value={f.heap_payload_lookup_progress.returned.word,
            f.heap_payload_lookup_progress.returned.known};
        return 1;
    }
    static int runHeapPayloadSize(Fixture& f,
        const Nba97GameTextMemory* memory,std::uint32_t payload,
        std::uint32_t stack_pointer,std::uint32_t return_address,
        std::uint32_t global_pointer,Nba97GameHeapPayloadSizeValue* value) {
        if(f.heap_payload_size_invocations)return 0;
        Nba97GameHeapPayloadSizeContext context{*memory,10,payload,
            stack_pointer,return_address,global_pointer,heapPayloadSizeIo,&f};
        if(nba97_game_heap_payload_size(&context,
               &f.heap_payload_size_progress)!=NBA97_TEXT_COMPLETE ||
           !f.heap_payload_size_progress.completed)return 0;
        ++f.heap_payload_size_invocations;
        *value={f.heap_payload_size_progress.return_v0,
            f.heap_payload_size_progress.return_v0_known};
        return 1;
    }
    static int cdSyncIo(void* user,const Nba97GameTextMemory*,
        const Nba97GameCdSyncEvent* event,Nba97GameCdSyncValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        if(event->kind!=NBA97_GAME_CD_SYNC_SERVICE ||
           event->pc!=0x8009dba8u || event->entry!=0x8009e740u ||
           event->argument_count!=2 || event->argument[0]!=0 ||
           event->argument[1]!=0 ||
           event->stack_pointer!=FrameSp-0x18u ||
           event->global_pointer!=0x800d79c8u ||
           event->return_address!=0x8009dbb0u)return 0;
        f.cd_sync_calls.push_back(*event);
        /* CdlComplete. The 0x8009E740 device/state-machine implementation
           remains an explicit service; this parent test does not fake RAM or
           rendering effects for the eight-instruction wrapper. */
        *value={2,1};
        return 1;
    }
    static int runCdSync(Fixture& f,const Nba97GameTextMemory* memory,
        std::uint32_t mode,std::uint32_t result_buffer,
        std::uint32_t stack_pointer,std::uint32_t return_address,
        std::uint32_t global_pointer,Nba97GameCdSyncValue* value) {
        if(f.cd_sync_invocations || !memory || !value)return 0;
        Nba97GameCdSyncContext context{*memory,10,mode,result_buffer,
            stack_pointer,return_address,global_pointer,cdSyncIo,&f};
        if(nba97_game_cd_sync(&context,&f.cd_sync_progress)!=
               NBA97_TEXT_COMPLETE || !f.cd_sync_progress.completed)return 0;
        ++f.cd_sync_invocations;
        *value={f.cd_sync_progress.return_v0,
            f.cd_sync_progress.return_v0_known};
        return 1;
    }
    static int runCdReadyCallback(Fixture& f,
        const Nba97GameTextMemory* memory,std::uint32_t replacement,
        Nba97GameMainValue* value) {
        if(f.cd_ready_callback_invocations || !memory || !value)return 0;
        Nba97GameCdReadyCallbackContext context{*memory,10,replacement};
        if(nba97_game_cd_ready_callback(&context,
               &f.cd_ready_callback_progress)!=NBA97_TEXT_COMPLETE ||
           !f.cd_ready_callback_progress.completed)return 0;
        ++f.cd_ready_callback_invocations;
        *value={f.cd_ready_callback_progress.return_v0,
            f.cd_ready_callback_progress.return_v0_known};
        return 1;
    }
    static int runCdSyncCallback(Fixture& f,
        const Nba97GameTextMemory* memory,std::uint32_t replacement,
        Nba97GameMainValue* value) {
        if(f.cd_sync_callback_invocations || !memory || !value)return 0;
        Nba97GameCdSyncCallbackContext context{*memory,10,replacement};
        if(nba97_game_cd_sync_callback(&context,
               &f.cd_sync_callback_progress)!=NBA97_TEXT_COMPLETE ||
           !f.cd_sync_callback_progress.completed)return 0;
        ++f.cd_sync_callback_invocations;
        *value={f.cd_sync_callback_progress.return_v0,
            f.cd_sync_callback_progress.return_v0_known};
        return 1;
    }
    static int loadingScreenIo(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameLoadingScreenEvent* event,
        Nba97GameLoadingScreenValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        const auto call=f.loading_screen_calls.size();
        f.loading_screen_calls.push_back(*event);
        static constexpr std::uint32_t pcs[10]={0x80029e70u,0x80029e8cu,
            0x80029e98u,0x80029eb0u,0x80029eb8u,0x80029ed0u,
            0x80029ed8u,0x80029ef0u,0x80029ef8u,0x80029f00u};
        static constexpr std::uint32_t entries[10]={0x80029bfcu,
            0x800a5478u,0x800994f4u,0x800946b8u,0x800994f4u,
            0x800946b8u,0x800994f4u,0x800946b8u,0x800994f4u,
            0x80090698u};
        if(call>=10 || event->pc!=pcs[call] || event->entry!=entries[call] ||
           event->stack_pointer!=FrameSp-0x28u ||
           event->global_pointer!=0x800d79c8u ||
           event->return_address!=event->pc+8u)
            return 0;
        *value={0,1};
        if(call==0) {
            if(event->kind!=NBA97_GAME_LOADING_SCREEN_LOAD_RESOURCE ||
               event->argument_count!=2 ||
               event->argument[0]!=0x800247f8u || event->argument[1]!=0)
                return 0;
            if(f.compose_resource_loader) {
                Nba97GameResourceLoaderValue loaded{};
                if(!runResourceLoader(f,memory,event->argument[0],
                       event->argument[1],event->stack_pointer,
                       event->return_address,event->saved_register,
                       event->global_pointer,&loaded))
                    return 0;
                *value={loaded.word,loaded.known};
            } else {
                *value={0x80130000u,1};
            }
        } else if(call==1) {
            if(event->kind!=NBA97_GAME_LOADING_SCREEN_FIND_IMAGE ||
               event->argument_count!=2 ||
               event->argument[0]!=0x80130000u ||
               event->argument[1]!=0x80024808u)
                return 0;
            *value={0x80140000u,1};
        } else if(call==9) {
            if(event->kind!=NBA97_GAME_LOADING_SCREEN_RELEASE_RESOURCE ||
               event->argument_count!=1 ||
               event->argument[0]!=0x80130000u)
                return 0;
        } else if(call==2 || call==4 || call==6 || call==8) {
            if(event->kind!=NBA97_GAME_LOADING_SCREEN_DRAW_SYNC ||
               event->argument_count!=1 || event->argument[0]!=0)
                return 0;
        } else {
            const unsigned upload=(static_cast<unsigned>(call)-3u)/2u;
            static constexpr std::uint32_t x[3]={0,0,0x200u};
            static constexpr std::uint32_t y[3]={0,0x100u,0};
            if(event->kind!=NBA97_GAME_LOADING_SCREEN_UPLOAD_IMAGE ||
               event->argument_count!=5 ||
               event->argument[0]!=0x80140000u ||
               event->argument[1]!=x[upload] ||
               event->argument[2]!=y[upload] ||
               event->argument[3]!=0 || event->argument[4]!=0)
                return 0;
        }
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
    static int moveImageIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameMoveImageEvent* event,
        Nba97GameMoveImageValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        const auto call=f.move_image_calls.size();
        const auto invocation=call/2u;
        if(invocation>=2 || event->stack_pointer!=FrameSp-0x20u ||
           event->global_pointer!=0x800d79c8u ||
           event->saved_register[0]!=FrameSp+0x10u ||
           event->saved_register[1]!=(invocation ? 0x100u : 0u) ||
           event->saved_register[2]!=0)
            return 0;
        if(!(call&1u)) {
            if(event->kind!=NBA97_GAME_MOVE_IMAGE_DIAGNOSTIC ||
               event->pc!=0x8009980cu || event->entry!=0x80099560u ||
               event->return_address!=0x80099814u ||
               event->argument_count!=2 ||
               event->argument[0]!=0x8002831cu ||
               event->argument[1]!=FrameSp+0x10u)
                return 0;
            *value={0,0};
        } else {
            if(event->kind!=NBA97_GAME_MOVE_IMAGE_GPU_DISPATCH ||
               event->pc!=0x80099884u || event->entry!=0x8009b298u ||
               event->return_address!=0x8009988cu ||
               event->argument_count!=4 ||
               event->argument[0]!=0x8009b1f8u ||
               event->argument[1]!=0x800c5668u ||
               event->argument[2]!=0x14u || event->argument[3]!=0 ||
               f.get(0x800c5668u)!=0x04ffffffu ||
               f.get(0x800c566cu)!=0x80000000u ||
               f.get(0x800c5670u)!=0x00000200u ||
               f.get(0x800c5674u)!=(invocation ? 0x01000000u : 0u) ||
               f.get(0x800c5678u)!=0x01000200u)
                return 0;
            /* The source GPU dispatch is asynchronous. Leave one observable
               DMA-busy sample for the following DrawSync(0), which owns the
               wait and completes both submitted packets. */
            if(f.compose_gpu_sync) {
                ++f.gpu_submitted;
                f.gpu_idle=false;
                f.gpu_sync_dma_busy_reads=1;
            }
            *value={0,1};
        }
        ++f.move_image_child_callbacks;
        f.move_image_calls.push_back(*event);
        return 1;
    }
    static int gpuSyncRead(void* user,const Nba97GameGpuSyncAccess* access,
        Nba97GameGpuSyncWord* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.gpu_sync_reads.push_back(*access);
        value->known_mask=access->width==2 ? 0xffffu : 0xffffffffu;
        if(access->address==f.gpu_sync_state.c56a0_dma2_chcr_ptr) {
            if(f.gpu_sync_dma_busy_reads) {
                value->word=f.gpu_dma_chcr|0x01000000u;
                --f.gpu_sync_dma_busy_reads;
                /* Completion occurs after this read has sampled BUSY, so the
                   recovered source loop must execute its timeout check and
                   poll DMA again before it may return. */
                f.gpu_completed=f.gpu_submitted;
                f.gpu_idle=true;
            } else value->word=f.gpu_dma_chcr&~0x01000000u;
        } else if(access->address==f.gpu_sync_state.c5694_gpu_status_ptr)
            value->word=f.gpu_status;
        else if(access->address==f.gpu_sync_state.c5698_gpu_read_ptr)
            value->word=f.gpu_read;
        else if(access->address==f.gpu_sync_state.c56b0_dpcr_ptr)
            value->word=f.gpu_dpcr;
        else if(access->address==f.gpu_sync_state.c5714_timer_status_ptr)
            value->word=f.gpu_timer_status;
        else if(access->address==f.gpu_sync_state.c5718_timer_counter_ptr)
            value->word=f.gpu_timer_count;
        else if(access->address==f.gpu_sync_state.c5534_i_mask_ptr)
            value->word=f.gpu_i_mask;
        else return NBA97_GAME_GPU_SYNC_ARGUMENT;
        return NBA97_GAME_GPU_SYNC_OK;
    }
    static int gpuSyncWrite(void* user,const Nba97GameGpuSyncWrite* write) {
        auto& f=*static_cast<Fixture*>(user);
        f.gpu_sync_writes.push_back(*write);
        if(write->address==f.gpu_sync_state.c56a0_dma2_chcr_ptr)
            f.gpu_dma_chcr=write->value.word;
        else if(write->address==f.gpu_sync_state.c5694_gpu_status_ptr)
            f.gpu_status=write->value.word;
        else if(write->address==f.gpu_sync_state.c56b0_dpcr_ptr)
            f.gpu_dpcr=write->value.word;
        else if(write->address==f.gpu_sync_state.c5534_i_mask_ptr)
            f.gpu_i_mask=write->value.word&0xffffu;
        else return NBA97_GAME_GPU_SYNC_ARGUMENT;
        return NBA97_GAME_GPU_SYNC_OK;
    }
    static int gpuSyncResolve(void* user,std::uint32_t pc,
        std::uint32_t table,std::uint32_t offset,Nba97GameGpuSyncWord* value) {
        auto& f=*static_cast<Fixture*>(user);
        ++f.gpu_sync_dispatch_resolutions;
        if(pc!=0x8009953cu || table!=0x800c5578u || offset!=0x3cu)
            return NBA97_GAME_GPU_SYNC_ARGUMENT;
        *value={0x8009b9b4u,0xffffffffu};
        return NBA97_GAME_GPU_SYNC_OK;
    }
    static int gpuSyncInvoke(void* user,const Nba97GameGpuSyncCall* call,
        Nba97GameGpuSyncState*) {
        auto& f=*static_cast<Fixture*>(user);
        f.gpu_sync_callbacks.push_back(*call);
        return NBA97_GAME_GPU_SYNC_OK;
    }
    static int gpuSyncObserve(void* user,Nba97GameGpuSyncBackend* backend) {
        auto& f=*static_cast<Fixture*>(user);
        ++f.gpu_sync_backend_observations;
        *backend={f.gpu_submitted,f.gpu_completed,
            static_cast<std::uint8_t>(f.gpu_idle),1};
        return NBA97_GAME_GPU_SYNC_OK;
    }
    static int displayMaskIo(void* user,const Nba97GameTextMemory*,
        const Nba97GameDisplayMaskSetEvent* event,
        Nba97GameDisplayMaskSetValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.display_mask_calls.push_back(*event);
        ++f.display_mask_child_callbacks;
        if(event->kind==NBA97_GAME_DISPLAY_MASK_CLEAR_ENVIRONMENTS) {
            if(event->entry!=0x8009bd78u || event->argument_count!=3)
                return 0;
            for(std::uint32_t i=0;i<event->argument[2];++i)
                f.put(event->argument[0]+i,event->argument[1],1);
            *value={0,0};
            return 1;
        }
        if(event->kind==NBA97_GAME_DISPLAY_MASK_DIAGNOSTIC) {
            *value={0,0};
            return 1;
        }
        if(event->kind!=NBA97_GAME_DISPLAY_MASK_GPU_CONTROL ||
           event->entry!=0x8009b16cu || event->argument_count!=1)
            return 0;
        /* The concrete retained display service applies GP1(03h)'s active-low
           enable bit. The recovered 0x8009B16C leaf leaves command id 3 in v0. */
        f.display_control_word=event->argument[0];
        f.display_visible=(event->argument[0]&1u)==0;
        *value={3,1};
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
        if (f.compose_move_image && event->entry == 0x800997e4u) {
            if(f.move_image_invocations>=f.move_image_progress.size())
                return 0;
            auto& move_progress=f.move_image_progress[f.move_image_invocations++];
            Nba97GameMoveImageContext context{*memory,100,
                event->argument[0],event->argument[1],event->argument[2],
                event->stack_pointer,event->return_address,
                {event->saved_register[0],event->saved_register[1],
                 event->saved_register[2]},event->global_pointer,
                moveImageIo,&f};
            if(nba97_game_move_image(&context,&move_progress)!=
                    NBA97_TEXT_COMPLETE)
                return 0;
            *value={move_progress.return_v0,move_progress.return_v0_known};
        }
        if (f.compose_gpu_sync && event->entry == 0x800994f4u) {
            if(event->pc!=0x80029aacu || event->argument_count!=1 ||
               event->argument[0]!=0 || event->stack_pointer!=FrameSp ||
               event->return_address!=0x80029ab4u)
                return 0;
            ++f.gpu_sync_invocations;
            /* The wrapper reloads these globals after any debug callback.
               Refresh them from the same retained RAM mutated by the earlier
               ResetGraph/SetGraphDebug owners. */
            f.gpu_sync_state.c55c2_debug_level=
                static_cast<std::uint8_t>(f.get(0x800c55c2u,1));
            f.gpu_sync_state.c55bc_debug_callback=f.get(0x800c55bcu);
            f.gpu_sync_state.c55b8_dispatch_table=f.get(0x800c55b8u);
            Nba97GameGpuSyncAbi abi{*memory,event->stack_pointer,
                event->return_address,event->saved_register[0]};
            Nba97GameGpuSyncContext context{gpuSyncRead,gpuSyncWrite,
                gpuSyncResolve,gpuSyncInvoke,gpuSyncObserve,&f,64,1000,&abi};
            if(nba97_game_gpu_sync(&context,&f.gpu_sync_state,
                   event->argument[0],&f.gpu_sync_source_v0,
                   &f.gpu_sync_progress)!=NBA97_GAME_GPU_SYNC_OK)
                return 0;
            *value={f.gpu_sync_source_v0.word,
                static_cast<std::uint8_t>(
                    f.gpu_sync_source_v0.known_mask==0xffffffffu)};
        }
        if (f.compose_display_mask && event->entry == 0x80099458u) {
            if(event->pc!=0x80029ab4u || event->argument_count!=1 ||
               event->argument[0]!=1 || event->stack_pointer!=FrameSp ||
               event->return_address!=0x80029abcu)
                return 0;
            ++f.display_mask_invocations;
            Nba97GameDisplayMaskSetContext context{*memory,30,
                event->argument[0],event->stack_pointer,event->return_address,
                {event->saved_register[0],event->saved_register[1]},
                displayMaskIo,&f};
            if(nba97_game_display_mask_set(&context,&f.display_mask_progress)!=
                    NBA97_TEXT_COMPLETE)
                return 0;
            *value={f.display_mask_progress.return_v0,
                f.display_mask_progress.return_v0_known};
        }
        if (f.compose_resource_validator_install &&
                event->entry == 0x800a3e20u) {
            if(event->pc!=0x80029abcu || event->argument_count!=0 ||
               event->stack_pointer!=FrameSp ||
               event->return_address!=0x80029ac4u)
                return 0;
            ++f.resource_validator_install_invocations;
            Nba97GameResourceValidatorInstallContext context{*memory,10};
            if(nba97_game_resource_validator_install(&context,
                   &f.resource_validator_progress)!=NBA97_TEXT_COMPLETE)
                return 0;
            *value={f.resource_validator_progress.return_v0,
                f.resource_validator_progress.return_v0_known};
        }
        if (f.compose_frame_rate_reset && event->entry == 0x800a7738u) {
            if(event->pc!=0x80029ad4u || event->argument_count!=0 ||
               event->stack_pointer!=FrameSp ||
               event->global_pointer!=0x800d79c8u ||
               event->return_address!=0x80029adcu)
                return 0;
            ++f.frame_rate_reset_invocations;
            Nba97GameFrameRateResetContext context{*memory,20,
                event->stack_pointer,event->return_address,
                event->global_pointer,frameRateResetIo,&f};
            if(nba97_game_frame_rate_reset(&context,
                   &f.frame_rate_reset_progress)!=NBA97_TEXT_COMPLETE)
                return 0;
            *value={f.frame_rate_reset_progress.return_v0,
                f.frame_rate_reset_progress.return_v0_known};
        }
        if (f.compose_match_session && event->entry == 0x8002d8d4u) {
            if(event->pc!=0x80029adcu || event->argument_count!=0 ||
               event->stack_pointer!=FrameSp ||
               event->global_pointer!=0x800d79c8u ||
               event->return_address!=0x80029ae4u)
                return 0;
            ++f.match_session_invocations;
            Nba97GameMatchSessionContext context{*memory,100,
                event->stack_pointer,event->return_address,
                {event->saved_register[0],event->saved_register[1],
                 event->saved_register[2]},event->global_pointer,
                matchSessionIo,&f};
            if(nba97_game_match_session(&context,
                   &f.match_session_progress)!=NBA97_TEXT_COMPLETE)
                return 0;
            *value={f.match_session_progress.return_v0,
                f.match_session_progress.return_v0_known};
        }
        if (f.compose_loading_screen && event->entry == 0x80029e58u) {
            if(event->pc!=0x80029ae4u || event->argument_count!=0 ||
               event->stack_pointer!=FrameSp ||
               event->global_pointer!=0x800d79c8u ||
               event->return_address!=0x80029aecu)
                return 0;
            ++f.loading_screen_invocations;
            Nba97GameLoadingScreenContext context{*memory,30,
                event->stack_pointer,event->return_address,
                {event->saved_register[0],event->saved_register[1]},
                event->global_pointer,loadingScreenIo,&f};
            if(nba97_game_loading_screen(&context,
                   &f.loading_screen_progress)!=NBA97_TEXT_COMPLETE)
                return 0;
            *value={f.loading_screen_progress.return_v0,
                f.loading_screen_progress.return_v0_known};
        }
        if (f.compose_resource_loader && event->entry == 0x80029bfcu) {
            Nba97GameResourceLoaderValue loaded{};
            if(!runResourceLoader(f,memory,event->argument[0],
                   event->argument[1],event->stack_pointer,
                   event->return_address,event->saved_register,
                   event->global_pointer,&loaded))
                return 0;
            *value={loaded.word,loaded.known};
        }
        if(f.compose_heap_payload_size && event->entry==0x80090d60u) {
            Nba97GameHeapPayloadSizeValue size{};
            if(!runHeapPayloadSize(f,memory,event->argument[0],
                   event->stack_pointer,event->return_address,
                   event->global_pointer,&size))return 0;
            *value={size.word,size.known};
        }
        if(f.compose_cd_sync && event->entry==0x8009dba0u) {
            if(event->pc!=0x80029b34u || event->argument_count!=2 ||
               event->argument[0]!=0 || event->argument[1]!=0 ||
               event->stack_pointer!=FrameSp ||
               event->global_pointer!=0x800d79c8u ||
               event->return_address!=0x80029b3cu)return 0;
            Nba97GameCdSyncValue sync{};
            if(!runCdSync(f,memory,event->argument[0],event->argument[1],
                   event->stack_pointer,event->return_address,
                   event->global_pointer,&sync))return 0;
            *value={sync.word,sync.known};
        }
        if(f.compose_cd_ready_callback && event->entry==0x8009dbe0u) {
            if(event->pc!=0x80029b3cu || event->argument_count!=1 ||
               event->argument[0]!=0 || event->stack_pointer!=FrameSp ||
               event->global_pointer!=0x800d79c8u ||
               event->return_address!=0x80029b44u)return 0;
            if(!runCdReadyCallback(f,memory,event->argument[0],value))return 0;
        }
        if(f.compose_cd_sync_callback && event->entry==0x8009dbf8u) {
            if(event->pc!=0x80029b44u || event->argument_count!=1 ||
               event->argument[0]!=0 || event->stack_pointer!=FrameSp ||
               event->global_pointer!=0x800d79c8u ||
               event->return_address!=0x80029b4cu)return 0;
            if(!runCdSyncCallback(f,memory,event->argument[0],value))return 0;
        }
        if(f.compose_vblank_shutdown && event->entry==0x800a44d4u) {
            if(event->pc!=0x80029b64u || event->argument_count!=0 ||
               event->stack_pointer!=FrameSp ||
               event->global_pointer!=0x800d79c8u ||
               event->return_address!=0x80029b6cu)return 0;
            ++f.vblank_shutdown_invocations;
            Nba97GameVblankShutdownContext context{*memory,10,
                event->stack_pointer,event->return_address,0xf6f6f6f6u,
                event->global_pointer,vblankShutdownIo,&f};
            if(nba97_game_vblank_shutdown(&context,
                   &f.vblank_shutdown_progress)!=NBA97_TEXT_COMPLETE)return 0;
            *value={f.vblank_shutdown_progress.return_v0,
                f.vblank_shutdown_progress.return_v0_known};
        }
        if(f.compose_clock_shutdown && event->entry==0x8009167cu) {
            if(event->pc!=0x80029b6cu || event->argument_count!=0 ||
               event->stack_pointer!=FrameSp ||
               event->global_pointer!=0x800d79c8u ||
               event->return_address!=0x80029b74u)return 0;
            ++f.clock_shutdown_invocations;
            Nba97GameClockShutdownContext context{*memory,10,
                event->stack_pointer,event->return_address,0xf7f7f7f7u,
                event->global_pointer,clockShutdownIo,&f};
            if(nba97_game_clock_shutdown(&context,
                   &f.clock_shutdown_progress)!=NBA97_TEXT_COMPLETE)return 0;
            *value={f.clock_shutdown_progress.return_v0,
                f.clock_shutdown_progress.return_v0_known};
        }
        if(f.compose_controller_suspend && event->entry==0x8008f19cu) {
            if(event->pc!=0x80029b74u || event->argument_count!=0 ||
               event->stack_pointer!=FrameSp ||
               event->return_address!=0x80029b7cu)return 0;
            ++f.controller_suspend_invocations;
            Nba97GameControllerSuspendContext context{*memory,10,
                event->stack_pointer,event->return_address,
                controllerSuspendIo,&f};
            if(nba97_game_controller_suspend(&context,
                   &f.controller_suspend_progress)!=NBA97_TEXT_COMPLETE)return 0;
            *value={f.controller_suspend_progress.return_v0,
                f.controller_suspend_progress.return_v0_known};
        }
        if(f.compose_memory_zero && event->entry==0x800a3a74u) {
            if(event->pc!=0x80029b84u || event->argument_count!=2 ||
               event->argument[0]!=0x800d6decu ||
               event->argument[1]!=0x20u ||
               event->stack_pointer!=FrameSp ||
               event->return_address!=0x80029b8cu)return 0;
            ++f.memory_zero_invocations;
            /* 0x800A3A74 never writes v0. In this exact source sequence the
               preceding recovered suspend owner left known one live. */
            Nba97GameMemoryZeroContext context{*memory,20,
                event->argument[0],event->argument[1],
                f.controller_suspend_progress.return_v0,
                f.controller_suspend_progress.return_v0_known};
            if(nba97_game_memory_zero(&context,&f.memory_zero_progress)!=
               NBA97_TEXT_COMPLETE)return 0;
            *value={f.memory_zero_progress.return_v0,
                f.memory_zero_progress.return_v0_known};
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
        if (event->entry == 0x80029bfcu && !f.compose_resource_loader) {
            value->word = 0x80123400u;
            value->known = f.mode == MissingImage ? 0 : 1;
        } else if (event->entry == 0x80090d60u &&
                   !f.compose_heap_payload_size) {
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
    check(f.calls[20].pc == 0x80029aacu && f.calls[20].entry == 0x800994f4u &&
        f.calls[21].pc == 0x80029ab4u && f.calls[21].entry == 0x80099458u &&
        f.calls[21].argument_count == 1 && f.calls[21].argument[0] == 1);
    check(f.calls[22].pc == 0x80029abcu &&
        f.calls[22].entry == 0x800a3e20u &&
        f.calls[22].return_address == 0x80029ac4u &&
        f.calls[22].argument_count == 0);
    check(f.calls[24].entry == 0x8002d8d4u && f.calls[26].entry == 0x80029bfcu &&
        f.calls[26].argument[0] == 0x800247ecu);
    for (unsigned i = 0; i < 20; ++i)
        check(f.calls[28 + i].pc == 0x80029b20u && f.calls[28 + i].saved_register[0] == i + 1);
    check(f.calls[48].entry == 0x8009dba0u && f.calls[49].entry == 0x8009dbe0u &&
        f.calls[50].entry == 0x8009dbf8u &&
        f.calls[71].pc == 0x80029b64u && f.calls[71].entry == 0x800a44d4u &&
        f.calls[72].pc == 0x80029b6cu && f.calls[72].entry == 0x8009167cu &&
        f.calls[73].pc == 0x80029b74u && f.calls[73].entry == 0x8008f19cu &&
        f.calls[73].argument_count == 0 &&
        f.calls[73].return_address == 0x80029b7cu);
    check(f.calls[74].pc==0x80029b84u &&
        f.calls[74].entry==0x800a3a74u &&
        f.calls[74].argument_count==2 &&
        f.calls[74].argument[0]==0x800d6decu &&
        f.calls[74].argument[1]==0x20u &&
        f.calls[74].return_address==0x80029b8cu);
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
        game.put(0x800c54d0u, 0);
        game.put(0x800c54e8u, 0);
        /* Raw GAMEONLY data starts suspended, so the first 0x8008F1D4 call
         * takes its resume branch and the later startup call takes fast path. */
        game.put(0x800c4a70u,1);
        game.put(0x800c4a74u,0);
        game.put(0x800d7a48u,0);
        game.put(0x800c55b8u,0x800c5578u);
        game.put(0x800c55bcu,0x8009cb2cu);
        game.put(0x800c5580u,0x8009b298u);
        game.put(0x800c5588u,0x8009b16cu);
        game.put(0x800c5590u,0x8009b1f8u);
        game.put(0x800d7b1cu,0);
        /* CdInit 0x8009D94C remains a typed earlier boundary. Seed its source
         * default ready and sync callbacks so both recovered exchanges prove
         * real nonzero-to-NULL transitions without claiming full CdInit. */
        game.put(0x800c57e4u,0x8009d9dcu);
        game.put(0x800c57e8u,0x8009da04u);
        /* Nonzero retained frame-rate state proves 0x800A7738 performs every
         * clear and leaves the old clock baseline live until its child call. */
        game.put(0x800d7b44u,9);
        game.put(0x800d7b48u,0x11111111u);
        game.put(0x800d7b4cu,0x22222222u);
        game.put(0x800d7b50u,0x33333333u);
        game.put(0x800d7b54u,0x44444444u);
        game.put(0x800d7b58u,0x55555555u);
        /* Exercise the ordinary retail path; optional-location mutation and
         * its preserved recheck/index bugs have dedicated routine tests. */
        game.put(0x8001ec94u,0);
        game.put(0x80021d74u,1);
        game.put(0x800c5668u,0x04ffffffu);
        game.put(0x800c566cu,0x80000000u);
        game.put(0x800c5640u,0x400u,2);
        game.put(0x800c5654u,0x200u,2);
        game.gpu_sync_state.c5534_i_mask_ptr=0x1f801074u;
        game.gpu_sync_state.c5694_gpu_status_ptr=0x1f801814u;
        game.gpu_sync_state.c5698_gpu_read_ptr=0x1f801810u;
        game.gpu_sync_state.c56a0_dma2_chcr_ptr=0x1f8010a8u;
        game.gpu_sync_state.c56b0_dpcr_ptr=0x1f8010f0u;
        game.gpu_sync_state.c5714_timer_status_ptr=0x1f801124u;
        game.gpu_sync_state.c5718_timer_counter_ptr=0x1f801120u;
        game.putText(0x800247e4u,"cdrom:");
        game.putText(0x800247ecu,"feload.bin");
        game.putText(0x800247f8u,"zloadscr.psh");
        game.putText(0x80024808u,"LdS1");
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
        game.compose_move_image = true;
        game.compose_gpu_sync = true;
        game.compose_display_mask = true;
        game.compose_resource_validator_install = true;
        game.compose_frame_rate_reset = true;
        game.compose_match_session = true;
        game.compose_loading_screen = true;
        game.compose_resource_loader = true;
        game.compose_heap_payload_size = true;
        game.compose_cd_sync = true;
        game.compose_cd_ready_callback = true;
        game.compose_cd_sync_callback = true;
        game.compose_vblank_shutdown = true;
        game.compose_clock_shutdown = true;
        game.compose_controller_suspend = true;
        game.compose_memory_zero = true;
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
        c.game.get(0x800eb688u) == 0x8010b694u &&
        c.game.get(0x800d7c3cu) == 0);
    check(c.game.get(0x8010b61cu+0x20u)==0x8010b66cu &&
        c.game.get(0x8010b66cu)==0x80123400u &&
        c.game.get(0x8010b66cu+0x10u)==0x1410u &&
        c.game.get(0x8010b66cu+0x14u)==0x1410u &&
        c.game.get(0x8010b66cu+0x18u)==0 &&
        c.game.get(0x8010b66cu+0x20u)==0x8010b644u &&
        c.game.get(0x8010b66cu+0x24u)==0x8010b61cu &&
        c.game.get(0x8010b644u+0x24u)==0x8010b66cu);
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
    /* The initializer reset 7A88 to zero; the main owner's 41 waits plus the
       recovered match-session owner's eleven waits each acknowledge one
       deterministic source VBlank. */
    check(c.game.get(0x800d7a88u)==52 && c.game.get(0x800d7afcu)==0 &&
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
    check(c.game.clock_interrupt_was_installed &&
        !c.game.clock_interrupt_installed &&
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
        c.game.get(0x80021eecu+24u,1)==1 &&
        c.game.get(0x80021f48u+24u,1)==1 &&
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
    check(c.game.move_image_invocations==2 &&
        c.game.move_image_child_callbacks==4 &&
        c.game.move_image_calls.size()==4);
    for(unsigned i=0;i<2;++i) {
        const auto& move=c.game.move_image_progress[i];
        check(move.completed && move.diagnostic_called && move.gpu_dispatched &&
            move.operations==20 && move.accesses==18 && move.reads==11 &&
            move.stores==7 && move.callbacks_completed==2 &&
            move.frame_stack_pointer==FrameSp-0x20u &&
            move.stack_pointer==FrameSp &&
            move.global_pointer==0x800d79c8u &&
            move.rectangle_address==FrameSp+0x10u &&
            move.signed_width==512 && move.signed_height==256 &&
            move.source_coordinate_word==0x00000200u &&
            move.destination_coordinate_word==(i ? 0x01000000u : 0u) &&
            move.extent_word==0x01000200u &&
            move.driver_table==0x800c5578u &&
            move.dispatch_context==0x8009b1f8u &&
            move.dispatch_entry==0x8009b298u &&
            move.return_v0==0 && move.return_v0_known &&
            move.restored_return_address==(i ? 0x80029aacu : 0x80029a9cu) &&
            move.restored_saved_register[0]==1 &&
            move.restored_saved_register[1]==0 &&
            move.restored_saved_register[2]==0);
    }
    check(c.game.calls[18].pc==0x80029a94u &&
        c.game.calls[18].entry==0x800997e4u &&
        c.game.calls[18].argument_count==3 &&
        c.game.calls[18].argument[0]==FrameSp+0x10u &&
        c.game.calls[18].argument[1]==0 && c.game.calls[18].argument[2]==0 &&
        c.game.calls[19].pc==0x80029aa4u &&
        c.game.calls[19].entry==0x800997e4u &&
        c.game.calls[19].argument[0]==FrameSp+0x10u &&
        c.game.calls[19].argument[1]==0 &&
        c.game.calls[19].argument[2]==0x100u);
    check(c.game.move_image_calls[0].kind==
            NBA97_GAME_MOVE_IMAGE_DIAGNOSTIC &&
        c.game.move_image_calls[1].kind==
            NBA97_GAME_MOVE_IMAGE_GPU_DISPATCH &&
        c.game.move_image_calls[2].kind==
            NBA97_GAME_MOVE_IMAGE_DIAGNOSTIC &&
        c.game.move_image_calls[3].kind==
            NBA97_GAME_MOVE_IMAGE_GPU_DISPATCH &&
        c.game.get(0x800c5668u)==0x04ffffffu &&
        c.game.get(0x800c566cu)==0x80000000u &&
        c.game.get(0x800c5670u)==0x00000200u &&
        c.game.get(0x800c5674u)==0x01000000u &&
        c.game.get(0x800c5678u)==0x01000200u);
    check(c.game.gpu_sync_invocations==1 &&
        c.game.gpu_sync_dispatch_resolutions==1 &&
        c.game.gpu_sync_backend_observations==2 &&
        c.game.gpu_submitted==2 && c.game.gpu_completed==2 &&
        c.game.gpu_idle && c.game.gpu_sync_dma_busy_reads==0);
    check(c.game.gpu_sync_progress.source_completed &&
        c.game.gpu_sync_progress.synchronized &&
        !c.game.gpu_sync_progress.source_timed_out &&
        c.game.gpu_sync_progress.abi_completed &&
        c.game.gpu_sync_progress.device_reads==7 &&
        c.game.gpu_sync_progress.device_writes==0 &&
        c.game.gpu_sync_progress.calls==0 &&
        c.game.gpu_sync_progress.dispatch_resolutions==1 &&
        c.game.gpu_sync_progress.backend_observations==2 &&
        c.game.gpu_sync_progress.gpu_polls==0 &&
        c.game.gpu_sync_progress.source_steps==4 &&
        c.game.gpu_sync_progress.stack_writes==2 &&
        c.game.gpu_sync_progress.stack_reads==2 &&
        c.game.gpu_sync_progress.queued_through==2 &&
        c.game.gpu_sync_progress.frame_stack_pointer==FrameSp-0x18u &&
        c.game.gpu_sync_progress.stack_pointer==FrameSp &&
        c.game.gpu_sync_progress.restored_return_address==0x80029ab4u &&
        c.game.gpu_sync_progress.restored_saved_register_s0==1);
    check(c.game.gpu_sync_source_v0.word==0 &&
        c.game.gpu_sync_source_v0.known_mask==0xffffffffu &&
        c.game.gpu_sync_state.c55c2_debug_level==0 &&
        c.game.gpu_sync_state.c55bc_debug_callback==0x8009cb2cu &&
        c.game.gpu_sync_state.c55b8_dispatch_table==0x800c5578u &&
        c.game.gpu_sync_state.c56d8_deadline==0xf0u &&
        c.game.gpu_sync_state.c56dc_poll_count==1 &&
        c.game.gpu_sync_callbacks.empty() && c.game.gpu_sync_writes.empty());
    check(c.game.gpu_sync_reads.size()==7 &&
        c.game.gpu_sync_reads[0].pc==0x8009bdd4u &&
        c.game.gpu_sync_reads[1].pc==0x8009bdd8u &&
        c.game.gpu_sync_reads[2].pc==0x8009ba2cu &&
        c.game.gpu_sync_reads[3].pc==0x8009bdd4u &&
        c.game.gpu_sync_reads[4].pc==0x8009bdd8u &&
        c.game.gpu_sync_reads[5].pc==0x8009ba2cu &&
        c.game.gpu_sync_reads[6].pc==0x8009ba4cu);
    check(c.game.calls[20].pc==0x80029aacu &&
        c.game.calls[20].entry==0x800994f4u &&
        c.game.calls[20].return_address==0x80029ab4u &&
        c.game.calls[20].argument_count==1 &&
        c.game.calls[20].argument[0]==0);
    check(c.game.display_mask_invocations==1 &&
        c.game.display_mask_child_callbacks==1 &&
        c.game.display_mask_calls.size()==1 &&
        c.game.display_mask_progress.completed &&
        c.game.display_mask_progress.operations==10 &&
        c.game.display_mask_progress.accesses==9 &&
        c.game.display_mask_progress.reads==6 &&
        c.game.display_mask_progress.stores==3 &&
        c.game.display_mask_progress.callbacks_completed==1 &&
        c.game.display_mask_progress.requested_mask==1 &&
        c.game.display_mask_progress.debug_level==0 &&
        !c.game.display_mask_progress.diagnostic_called &&
        !c.game.display_mask_progress.environment_cache_clear_called &&
        c.game.display_mask_progress.display_enabled &&
        c.game.display_mask_progress.gpu_control_word==0x03000000u &&
        c.game.display_mask_progress.driver_table==0x800c5578u &&
        c.game.display_mask_progress.dispatch_target==0x8009b16cu &&
        c.game.display_mask_progress.return_v0==3 &&
        c.game.display_mask_progress.return_v0_known &&
        c.game.display_control_word==0x03000000u &&
        c.game.display_visible &&
        c.game.display_mask_progress.frame_stack_pointer==FrameSp-0x20u &&
        c.game.display_mask_progress.stack_pointer==FrameSp &&
        c.game.display_mask_progress.restored_return_address==0x80029abcu &&
        c.game.display_mask_progress.restored_saved_register[0]==1 &&
        c.game.display_mask_progress.restored_saved_register[1]==0);
    check(c.game.display_mask_calls[0].kind==
            NBA97_GAME_DISPLAY_MASK_GPU_CONTROL &&
        c.game.display_mask_calls[0].pc==0x800994d4u &&
        c.game.display_mask_calls[0].entry==0x8009b16cu &&
        c.game.display_mask_calls[0].argument_count==1 &&
        c.game.display_mask_calls[0].argument[0]==0x03000000u &&
        c.game.display_mask_calls[0].stack_pointer==FrameSp-0x20u &&
        c.game.display_mask_calls[0].return_address==0x800994dcu &&
        c.game.display_mask_calls[0].saved_register[0]==1 &&
        c.game.display_mask_calls[0].saved_register[1]==0x800c55c2u);
    check(c.game.calls[21].pc==0x80029ab4u &&
        c.game.calls[21].entry==0x80099458u &&
        c.game.calls[21].return_address==0x80029abcu &&
        c.game.calls[21].argument_count==1 &&
        c.game.calls[21].argument[0]==1);
    check(c.game.resource_validator_install_invocations==1 &&
        c.game.resource_validator_progress.completed &&
        c.game.resource_validator_progress.operations==1 &&
        c.game.resource_validator_progress.accesses==1 &&
        c.game.resource_validator_progress.stores==1 &&
        c.game.resource_validator_progress.callback_global==0x800d7b1cu &&
        c.game.resource_validator_progress.installed_callback==0x800a3d60u &&
        c.game.resource_validator_progress.return_v0==0x800a3d60u &&
        c.game.resource_validator_progress.return_v0_known &&
        c.game.get(0x800d7b1cu)==0x800a3d60u);
    check(c.game.calls[22].pc==0x80029abcu &&
        c.game.calls[22].entry==0x800a3e20u &&
        c.game.calls[22].return_address==0x80029ac4u &&
        c.game.calls[22].argument_count==0);
    check(c.game.frame_rate_reset_invocations==1 &&
        c.game.frame_rate_reset_child_callbacks==1 &&
        c.game.frame_rate_reset_calls.size()==1 &&
        c.game.frame_rate_reset_progress.completed &&
        c.game.frame_rate_reset_progress.operations==9 &&
        c.game.frame_rate_reset_progress.accesses==8 &&
        c.game.frame_rate_reset_progress.reads==1 &&
        c.game.frame_rate_reset_progress.stores==7 &&
        c.game.frame_rate_reset_progress.callbacks_completed==1);
    check(c.game.frame_rate_reset_progress.frame_counter_address==0x800d7b44u &&
        c.game.frame_rate_reset_progress.auxiliary_address==0x800d7b48u &&
        c.game.frame_rate_reset_progress.clock_baseline_address==0x800d7b4cu &&
        c.game.frame_rate_reset_progress.instantaneous_rate_address==0x800d7b50u &&
        c.game.frame_rate_reset_progress.average_rate_address==0x800d7b54u &&
        c.game.frame_rate_reset_progress.last_report_clock_address==0x800d7b58u &&
        c.game.frame_rate_reset_progress.sampled_clock==0 &&
        c.game.frame_rate_reset_progress.sampled_clock_known &&
        c.game.frame_rate_reset_progress.return_v0==0 &&
        c.game.frame_rate_reset_progress.return_v0_known);
    check(c.game.frame_rate_reset_progress.frame_stack_pointer==FrameSp-0x18u &&
        c.game.frame_rate_reset_progress.stack_pointer==FrameSp &&
        c.game.frame_rate_reset_progress.restored_return_address==0x80029adcu &&
        c.game.frame_rate_reset_calls[0].pc==0x800a7754u &&
        c.game.frame_rate_reset_calls[0].entry==0x800a5810u &&
        c.game.frame_rate_reset_calls[0].return_address==0x800a775cu);
    check(c.game.get(0x800d7b44u)==0 && c.game.get(0x800d7b48u)==0 &&
        c.game.get(0x800d7b4cu)==0 && c.game.get(0x800d7b50u)==0 &&
        c.game.get(0x800d7b54u)==0 && c.game.get(0x800d7b58u)==0);
    check(c.game.calls[23].pc==0x80029ad4u &&
        c.game.calls[23].entry==0x800a7738u &&
        c.game.calls[23].return_address==0x80029adcu &&
        c.game.calls[23].argument_count==0);
    check(c.game.match_session_invocations==1 &&
        c.game.match_session_progress.completed &&
        c.game.match_session_calls.size()==23 &&
        c.game.match_session_progress.operations==54 &&
        c.game.match_session_progress.accesses==31 &&
        c.game.match_session_progress.reads==6 &&
        c.game.match_session_progress.stores==25 &&
        c.game.match_session_progress.callbacks_completed==23 &&
        c.game.match_session_progress.direct_control_bytes_written==14);
    check(c.game.match_session_progress.clear_rectangle_calls==2 &&
        c.game.match_session_progress.frame_rate_reset_calls==1 &&
        c.game.match_session_progress.environment_calls==4 &&
        c.game.match_session_progress.location_lookup_calls==0 &&
        c.game.match_session_progress.session_stage_calls==4 &&
        c.game.match_session_progress.presentation_wait_calls==11 &&
        c.game.match_session_progress.draw_sync_calls==1 &&
        !c.game.match_session_progress.initial_custom_location_active &&
        !c.game.match_session_progress.final_custom_location_active &&
        c.game.match_session_progress.return_v0==0 &&
        c.game.match_session_progress.return_v0_known);
    check(c.game.match_session_progress.frame_stack_pointer==FrameSp-0x28u &&
        c.game.match_session_progress.stack_pointer==FrameSp &&
        c.game.match_session_progress.restored_return_address==0x80029ae4u &&
        c.game.match_session_progress.restored_saved_register[0]==1 &&
        c.game.match_session_progress.restored_saved_register[1]==0 &&
        c.game.match_session_progress.restored_saved_register[2]==0);
    check(c.game.match_session_frame_rate_reset_child_callbacks==1 &&
        c.game.match_session_frame_rate_reset_calls.size()==1 &&
        c.game.match_session_frame_rate_reset_progress.completed &&
        c.game.match_session_frame_rate_reset_progress.operations==9 &&
        c.game.match_session_frame_rate_reset_progress.accesses==8 &&
        c.game.match_session_frame_rate_reset_progress.reads==1 &&
        c.game.match_session_frame_rate_reset_progress.stores==7 &&
        c.game.match_session_frame_rate_reset_progress.callbacks_completed==1 &&
        c.game.match_session_frame_rate_reset_progress.frame_stack_pointer==
            FrameSp-0x40u &&
        c.game.match_session_frame_rate_reset_progress.stack_pointer==
            FrameSp-0x28u &&
        c.game.match_session_frame_rate_reset_progress.restored_return_address==
            0x8002d910u);
    check(c.game.match_session_presentation_wait_invocations==11 &&
        c.game.match_session_vblank_signals==11 &&
        c.game.match_session_presentation_wait_calls.size()==11);
    for(unsigned i=0;i<11;++i) {
        const auto& wait=c.game.match_session_presentation_wait_progress[i];
        check(wait.completed && wait.operations==3 && wait.accesses==2 &&
            wait.reads==1 && wait.stores==1 && wait.callbacks_completed==1 &&
            wait.frame_stack_pointer==FrameSp-0x40u &&
            wait.stack_pointer==FrameSp-0x28u &&
            wait.global_pointer==0x800d79c8u &&
            wait.restored_return_address==(i ? 0x8002db40u : 0x8002db30u));
    }
    static constexpr std::uint32_t match_stage_entry[4]={0x8002db90u,
        0x8002db68u,0x8002dc38u,0x8002dc58u};
    for(unsigned i=0;i<4;++i)
        check(c.game.match_session_calls[6+i].entry==match_stage_entry[i]);
    check(c.game.match_session_calls[0].entry==0x800aa0bcu &&
        c.game.match_session_calls[1].entry==0x800a7738u &&
        c.game.match_session_calls[10].entry==0x800aa0bcu &&
        c.game.match_session_calls[11].entry==0x80029bdcu &&
        c.game.match_session_calls[12].entry==0x800994f4u &&
        c.game.match_session_calls[22].entry==0x80029bdcu);
    check(c.game.get(0x80021498u,2)==0 &&
        c.game.get(0x80021f05u,1)==0 &&
        c.game.get(0x80021f60u,1)==1 &&
        c.game.get(0x8002206du,1)==0 &&
        c.game.get(0x80022081u,1)==0 &&
        c.game.get(0x800eb680u,1)==1 &&
        c.game.get(0x80015021u,1)==0);
    check(c.game.calls[24].pc==0x80029adcu &&
        c.game.calls[24].entry==0x8002d8d4u &&
        c.game.calls[24].return_address==0x80029ae4u &&
        c.game.calls[24].argument_count==0 &&
        c.game.calls[25].pc==0x80029ae4u &&
        c.game.calls[25].entry==0x80029e58u &&
        c.game.calls[25].return_address==0x80029aecu);
    check(c.game.loading_screen_invocations==1 &&
        c.game.loading_screen_progress.completed &&
        c.game.loading_screen_progress.operations==16 &&
        c.game.loading_screen_progress.accesses==6 &&
        c.game.loading_screen_progress.reads==3 &&
        c.game.loading_screen_progress.stores==3 &&
        c.game.loading_screen_progress.callbacks_completed==10 &&
        c.game.loading_screen_progress.load_calls==1 &&
        c.game.loading_screen_progress.lookup_calls==1 &&
        c.game.loading_screen_progress.draw_sync_calls==4 &&
        c.game.loading_screen_progress.upload_calls==3 &&
        c.game.loading_screen_progress.release_calls==1 &&
        c.game.loading_screen_progress.loaded_resource==0x80130000u &&
        c.game.loading_screen_progress.resolved_image==0x80140000u &&
        c.game.loading_screen_progress.image_lookup_completed &&
        c.game.loading_screen_progress.resolved_image_known &&
        c.game.loading_screen_progress.frame_stack_pointer==FrameSp-0x28u &&
        c.game.loading_screen_progress.stack_pointer==FrameSp &&
        c.game.loading_screen_progress.restored_return_address==0x80029aecu &&
        c.game.loading_screen_progress.restored_saved_register[0]==1 &&
        c.game.loading_screen_progress.restored_saved_register[1]==0 &&
        c.game.loading_screen_calls.size()==10);
    check(c.game.loading_screen_calls[0].entry==0x80029bfcu &&
        c.game.loading_screen_calls[1].entry==0x800a5478u &&
        c.game.loading_screen_calls[3].entry==0x800946b8u &&
        c.game.loading_screen_calls[5].entry==0x800946b8u &&
        c.game.loading_screen_calls[7].entry==0x800946b8u &&
        c.game.loading_screen_calls[9].entry==0x80090698u);
    check(c.game.resource_loader_invocations==2 &&
        c.game.resource_loader_calls.size()==2);
    check(c.game.resource_loader_progress[0].completed &&
        c.game.resource_loader_progress[0].operations==7 &&
        c.game.resource_loader_progress[0].accesses==6 &&
        c.game.resource_loader_progress[0].reads==3 &&
        c.game.resource_loader_progress[0].stores==3 &&
        c.game.resource_loader_progress[0].callbacks_completed==1 &&
        c.game.resource_loader_progress[0].load_attempts==1 &&
        !c.game.resource_loader_progress[0].null_results &&
        c.game.resource_loader_progress[0].filename==0x800247f8u &&
        c.game.resource_loader_progress[0].return_v0==0x80130000u &&
        c.game.resource_loader_progress[0].frame_stack_pointer==FrameSp-0x48u &&
        c.game.resource_loader_progress[0].stack_pointer==FrameSp-0x28u &&
        c.game.resource_loader_progress[0].restored_return_address==0x80029e78u);
    check(c.game.resource_loader_progress[1].completed &&
        c.game.resource_loader_progress[1].operations==7 &&
        c.game.resource_loader_progress[1].accesses==6 &&
        c.game.resource_loader_progress[1].reads==3 &&
        c.game.resource_loader_progress[1].stores==3 &&
        c.game.resource_loader_progress[1].callbacks_completed==1 &&
        c.game.resource_loader_progress[1].load_attempts==1 &&
        !c.game.resource_loader_progress[1].null_results &&
        c.game.resource_loader_progress[1].filename==0x800247ecu &&
        c.game.resource_loader_progress[1].return_v0==0x80123400u &&
        c.game.resource_loader_progress[1].frame_stack_pointer==FrameSp-0x20u &&
        c.game.resource_loader_progress[1].stack_pointer==FrameSp &&
        c.game.resource_loader_progress[1].restored_return_address==0x80029b04u);
    check(c.game.resource_loader_calls[0].argument[0]==0x800247f8u &&
        c.game.resource_loader_calls[0].stack_pointer==FrameSp-0x48u &&
        c.game.resource_loader_calls[1].argument[0]==0x800247ecu &&
        c.game.resource_loader_calls[1].stack_pointer==FrameSp-0x20u);
    check(c.game.heap_payload_size_invocations==1 &&
        c.game.heap_payload_size_calls.size()==1 &&
        c.game.heap_payload_size_progress.completed &&
        c.game.heap_payload_size_progress.operations==4 &&
        c.game.heap_payload_size_progress.accesses==3 &&
        c.game.heap_payload_size_progress.reads==2 &&
        c.game.heap_payload_size_progress.stores==1 &&
        c.game.heap_payload_size_progress.callbacks_completed==1 &&
        c.game.heap_payload_size_progress.descriptor_lookup_calls==1 &&
        c.game.heap_payload_size_progress.payload==0x80123400u &&
        c.game.heap_payload_size_progress.descriptor==0x8010b66cu &&
        c.game.heap_payload_size_progress.descriptor_known &&
        c.game.heap_payload_size_progress.requested_size==0x1410u &&
        c.game.heap_payload_size_progress.return_v0==0x1410u &&
        c.game.heap_payload_size_progress.return_v0_known &&
        c.game.heap_payload_size_progress.frame_stack_pointer==FrameSp-0x18u &&
        c.game.heap_payload_size_progress.stack_pointer==FrameSp &&
        c.game.heap_payload_size_progress.restored_return_address==0x80029b10u);
    check(c.game.heap_payload_size_calls[0].pc==0x80090d68u &&
        c.game.heap_payload_size_calls[0].entry==0x80090618u &&
        c.game.heap_payload_size_calls[0].argument[0]==0x80123400u &&
        c.game.heap_payload_size_calls[0].stack_pointer==FrameSp-0x18u &&
        c.game.heap_payload_size_calls[0].return_address==0x80090d70u);
    check(c.game.heap_payload_lookup_progress.completed &&
        c.game.heap_payload_lookup_progress.accesses==5 &&
        !c.game.heap_payload_lookup_progress.stores &&
        c.game.heap_payload_lookup_progress.descriptor==0x8010b66cu &&
        c.game.heap_payload_lookup_progress.returned.known &&
        c.game.heap_payload_lookup_progress.returned.word==0x8010b66cu);
    check(c.game.cd_sync_invocations==1 && c.game.cd_sync_calls.size()==1 &&
        c.game.cd_sync_progress.completed &&
        c.game.cd_sync_progress.operations==3 &&
        c.game.cd_sync_progress.accesses==2 &&
        c.game.cd_sync_progress.reads==1 &&
        c.game.cd_sync_progress.stores==1 &&
        c.game.cd_sync_progress.callbacks_completed==1 &&
        c.game.cd_sync_progress.mode==0 &&
        c.game.cd_sync_progress.result_buffer==0 &&
        c.game.cd_sync_progress.service_entry==0x8009e740u &&
        c.game.cd_sync_progress.frame_stack_pointer==FrameSp-0x18u &&
        c.game.cd_sync_progress.stack_pointer==FrameSp &&
        c.game.cd_sync_progress.global_pointer==0x800d79c8u &&
        c.game.cd_sync_progress.restored_return_address==0x80029b3cu &&
        c.game.cd_sync_progress.return_v0==2 &&
        c.game.cd_sync_progress.return_v0_known &&
        !c.game.cd_sync_progress.stopped_pc &&
        !c.game.cd_sync_progress.stopped_address &&
        !c.game.cd_sync_progress.stopped_entry);
    check(c.game.cd_sync_calls[0].kind==NBA97_GAME_CD_SYNC_SERVICE &&
        c.game.cd_sync_calls[0].pc==0x8009dba8u &&
        c.game.cd_sync_calls[0].entry==0x8009e740u &&
        c.game.cd_sync_calls[0].argument_count==2 &&
        c.game.cd_sync_calls[0].argument[0]==0 &&
        c.game.cd_sync_calls[0].argument[1]==0 &&
        c.game.cd_sync_calls[0].stack_pointer==FrameSp-0x18u &&
        c.game.cd_sync_calls[0].global_pointer==0x800d79c8u &&
        c.game.cd_sync_calls[0].return_address==0x8009dbb0u);
    check(c.game.cd_ready_callback_invocations==1 &&
        c.game.cd_ready_callback_progress.completed &&
        c.game.cd_ready_callback_progress.operations==2 &&
        c.game.cd_ready_callback_progress.accesses==2 &&
        c.game.cd_ready_callback_progress.reads==1 &&
        c.game.cd_ready_callback_progress.stores==1 &&
        c.game.cd_ready_callback_progress.callback_global==0x800c57e4u &&
        c.game.cd_ready_callback_progress.requested_callback==0 &&
        c.game.cd_ready_callback_progress.previous_callback==0x8009d9dcu &&
        c.game.cd_ready_callback_progress.previous_callback_known &&
        c.game.cd_ready_callback_progress.return_v0==0x8009d9dcu &&
        c.game.cd_ready_callback_progress.return_v0_known &&
        !c.game.cd_ready_callback_progress.stopped_pc &&
        !c.game.cd_ready_callback_progress.stopped_address &&
        c.game.get(0x800c57e4u)==0);
    check(c.game.cd_sync_callback_invocations==1 &&
        c.game.cd_sync_callback_progress.completed &&
        c.game.cd_sync_callback_progress.operations==2 &&
        c.game.cd_sync_callback_progress.accesses==2 &&
        c.game.cd_sync_callback_progress.reads==1 &&
        c.game.cd_sync_callback_progress.stores==1 &&
        c.game.cd_sync_callback_progress.callback_global==0x800c57e8u &&
        c.game.cd_sync_callback_progress.requested_callback==0 &&
        c.game.cd_sync_callback_progress.previous_callback==0x8009da04u &&
        c.game.cd_sync_callback_progress.previous_callback_known &&
        c.game.cd_sync_callback_progress.return_v0==0x8009da04u &&
        c.game.cd_sync_callback_progress.return_v0_known &&
        !c.game.cd_sync_callback_progress.stopped_pc &&
        !c.game.cd_sync_callback_progress.stopped_address &&
        c.game.get(0x800c57e8u)==0);
    check(c.game.vblank_shutdown_invocations==1 &&
        c.game.vblank_shutdown_child_callbacks==1 &&
        c.game.vblank_shutdown_calls.size()==1 &&
        c.game.vblank_shutdown_progress.completed &&
        c.game.vblank_shutdown_progress.operations==5 &&
        c.game.vblank_shutdown_progress.accesses==4 &&
        c.game.vblank_shutdown_progress.reads==2 &&
        c.game.vblank_shutdown_progress.stores==2 &&
        c.game.vblank_shutdown_progress.callbacks_completed==1 &&
        c.game.vblank_shutdown_progress.interrupt_callback_entry==0x8009860cu &&
        c.game.vblank_shutdown_progress.interrupt_number==0 &&
        c.game.vblank_shutdown_progress.replacement_callback==0 &&
        c.game.vblank_shutdown_progress.frame_stack_pointer==FrameSp-0x18u &&
        c.game.vblank_shutdown_progress.stack_pointer==FrameSp &&
        c.game.vblank_shutdown_progress.global_pointer==0x800d79c8u &&
        c.game.vblank_shutdown_progress.incoming_frame_pointer==0xf6f6f6f6u &&
        c.game.vblank_shutdown_progress.restored_return_address==0x80029b6cu &&
        c.game.vblank_shutdown_progress.restored_frame_pointer==0xf6f6f6f6u &&
        c.game.vblank_shutdown_progress.return_v0==0x800a450cu &&
        c.game.vblank_shutdown_progress.return_v0_known &&
        !c.game.vblank_shutdown_progress.stopped_pc &&
        !c.game.vblank_shutdown_progress.stopped_address &&
        !c.game.vblank_shutdown_progress.stopped_entry &&
        !c.game.vblank_interrupt_installed &&
        c.game.get(0x800c54d0u)==0);
    check(c.game.vblank_shutdown_calls[0].pc==0x800a44ecu &&
        c.game.vblank_shutdown_calls[0].entry==0x8009860cu &&
        c.game.vblank_shutdown_calls[0].argument_count==2 &&
        c.game.vblank_shutdown_calls[0].argument[0]==0 &&
        c.game.vblank_shutdown_calls[0].argument[1]==0 &&
        c.game.vblank_shutdown_calls[0].stack_pointer==FrameSp-0x18u &&
        c.game.vblank_shutdown_calls[0].frame_pointer==FrameSp-0x18u &&
        c.game.vblank_shutdown_calls[0].global_pointer==0x800d79c8u &&
        c.game.vblank_shutdown_calls[0].return_address==0x800a44f4u);
    check(c.game.clock_shutdown_invocations==1 &&
        c.game.clock_shutdown_child_callbacks==1 &&
        c.game.clock_shutdown_calls.size()==1 &&
        c.game.clock_shutdown_progress.completed &&
        c.game.clock_shutdown_progress.operations==5 &&
        c.game.clock_shutdown_progress.accesses==4 &&
        c.game.clock_shutdown_progress.reads==2 &&
        c.game.clock_shutdown_progress.stores==2 &&
        c.game.clock_shutdown_progress.callbacks_completed==1 &&
        c.game.clock_shutdown_progress.interrupt_callback_entry==0x8009860cu &&
        c.game.clock_shutdown_progress.interrupt_number==6 &&
        c.game.clock_shutdown_progress.replacement_callback==0 &&
        c.game.clock_shutdown_progress.frame_stack_pointer==FrameSp-0x18u &&
        c.game.clock_shutdown_progress.stack_pointer==FrameSp &&
        c.game.clock_shutdown_progress.global_pointer==0x800d79c8u &&
        c.game.clock_shutdown_progress.incoming_frame_pointer==0xf7f7f7f7u &&
        c.game.clock_shutdown_progress.restored_return_address==0x80029b74u &&
        c.game.clock_shutdown_progress.restored_frame_pointer==0xf7f7f7f7u &&
        c.game.clock_shutdown_progress.return_v0==0x800916b4u &&
        c.game.clock_shutdown_progress.return_v0_known &&
        !c.game.clock_shutdown_progress.stopped_pc &&
        !c.game.clock_shutdown_progress.stopped_address &&
        !c.game.clock_shutdown_progress.stopped_entry &&
        !c.game.clock_interrupt_installed &&
        c.game.get(0x800c54e8u)==0);
    check(c.game.clock_shutdown_calls[0].pc==0x80091694u &&
        c.game.clock_shutdown_calls[0].entry==0x8009860cu &&
        c.game.clock_shutdown_calls[0].argument_count==2 &&
        c.game.clock_shutdown_calls[0].argument[0]==6 &&
        c.game.clock_shutdown_calls[0].argument[1]==0 &&
        c.game.clock_shutdown_calls[0].stack_pointer==FrameSp-0x18u &&
        c.game.clock_shutdown_calls[0].frame_pointer==FrameSp-0x18u &&
        c.game.clock_shutdown_calls[0].global_pointer==0x800d79c8u &&
        c.game.clock_shutdown_calls[0].return_address==0x8009169cu);
    check(c.game.controller_suspend_invocations==1 &&
        c.game.controller_suspend_child_callbacks==1 &&
        c.game.controller_suspend_calls.size()==1 &&
        c.game.controller_shutdown_service_called &&
        c.game.controller_suspend_progress.completed &&
        c.game.controller_suspend_progress.operations==5 &&
        c.game.controller_suspend_progress.accesses==4 &&
        c.game.controller_suspend_progress.reads==2 &&
        c.game.controller_suspend_progress.stores==2 &&
        c.game.controller_suspend_progress.callbacks_completed==1 &&
        c.game.controller_suspend_progress.initial_suspend_flag==0 &&
        c.game.controller_suspend_progress.shutdown_called &&
        c.game.controller_suspend_progress.input_suspended &&
        c.game.controller_suspend_progress.frame_stack_pointer==FrameSp-0x18u &&
        c.game.controller_suspend_progress.stack_pointer==FrameSp &&
        c.game.controller_suspend_progress.restored_return_address==0x80029b7cu &&
        c.game.controller_suspend_progress.return_v0==1 &&
        c.game.controller_suspend_progress.return_v0_known &&
        !c.game.controller_suspend_progress.stopped_pc &&
        !c.game.controller_suspend_progress.stopped_address &&
        !c.game.controller_suspend_progress.stopped_entry);
    check(c.game.controller_suspend_calls[0].pc==0x8008f1b0u &&
        c.game.controller_suspend_calls[0].entry==0x80091224u &&
        c.game.controller_suspend_calls[0].argument_count==0 &&
        c.game.controller_suspend_calls[0].stack_pointer==FrameSp-0x18u &&
        c.game.controller_suspend_calls[0].return_address==0x8008f1b8u);
    check(c.game.memory_zero_invocations==1 &&
        c.game.memory_zero_progress.completed &&
        c.game.memory_zero_progress.destination==0x800d6decu &&
        c.game.memory_zero_progress.requested_length==0x20u &&
        c.game.memory_zero_progress.operations==9 &&
        c.game.memory_zero_progress.accesses==9 &&
        c.game.memory_zero_progress.stores==9 &&
        c.game.memory_zero_progress.bytes_stored==36 &&
        c.game.memory_zero_progress.working_destination==0x800d6e08u &&
        c.game.memory_zero_progress.working_count==0xfffffffcu &&
        c.game.memory_zero_progress.return_v0==1 &&
        c.game.memory_zero_progress.return_v0_known &&
        !c.game.memory_zero_progress.used_small_path &&
        !c.game.memory_zero_progress.stopped_pc &&
        !c.game.memory_zero_progress.stopped_address);
    for(unsigned i=0;i<8;++i)
        check(c.game.get(0x800d6decu+i*4u)==0);
    check(c.game.get(0x800c55c0u) == 0x00000100u &&
        c.game.get(0x800c55c4u) == 0x02000400u &&
        c.game.get(0x800c55d0u) == UINT32_MAX &&
        c.game.get(0x800c562cu) == UINT32_MAX);
    check(c.game.get(0x800c4a70u) == 1 &&
        c.game.get(0x800c4a74u) == 37 &&
        c.game.get(0x800d7a48u) == 8 &&
        /* Controller suspend reuses clock shutdown's frame and overwrites its
           saved-s8 slot with the later main return address. */
        c.game.get(FrameSp - 8u) == 0x80029b7cu &&
        c.game.get(FrameSp - 4u) == 0x80029b74u);
    check(c.game.get(0x800d7a80u)==1 && c.game.get(0x800d7a84u)==0 &&
        c.game.get(0x800d7a88u)==52 && c.game.get(0x800d7b3cu)==0 &&
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
