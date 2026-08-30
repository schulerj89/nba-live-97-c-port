#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <objbase.h>

#include "boot_flow.hpp"
#include "win32_keyboard.hpp"
#include "frontend_music.hpp"
#include "recovered_audio.hpp"
#include "recovered/frontend_audio.h"
#include "recovered/semantic_trace.h"
#include "intro_player.hpp"
#include "main_menu.hpp"
#include "team_select_assets.hpp"
#include "user_setup_assets.hpp"
#include "user_setup_session.hpp"
#include "match_assets.hpp"
#include "match_snapshot.hpp"
#include "recovered/team_select_poll.h"
#include "png_image.hpp"
#include "player_photo_loader.hpp"
#include "frontend_title.hpp"
#include "psh_image.hpp"
#include "psh_font.hpp"
#include "roster_database.hpp"
#include "roster_save_store.hpp"
#include "create_player_store.hpp"
#include "create_player_delete_assets.hpp"
#include "create_player_preview.hpp"
#include "roster_reset_assets.hpp"
#include "reorder_preview.hpp"
#include "trade_assets.hpp"
#include "recovered/roster_sign.h"
#include "recovered/roster_release.h"
#include "frontend_help.hpp"
#include "recovered/reorder_children.h"
#include "user_profiles.hpp"
#include "player_notice.hpp"
#include "cool_fact_index.hpp"
#include "native_frame_capture.hpp"
#include "process_audio_capture.hpp"
#include "sha256.hpp"
#include "recovered/cool_fact_selection.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr int kPsxWidth = 512;
constexpr int kPsxHeight = 240;
constexpr UINT_PTR kFrameTimer = 1;
constexpr UINT kFrameIntervalMs = 16;
constexpr int kPressStartCenterX = 0x100;
constexpr int kPressStartY = 0x1e;
constexpr int kRecoveredPressStartWidth = 97;

PshImage decodePlayerPhotoOnWorker(const std::filesystem::path& path) {
    const auto result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(result)) throw std::runtime_error("portrait worker COM initialization failed");
    struct ComScope { ~ComScope() { CoUninitialize(); } } scope;
    return load_png_image(path);
}

constexpr std::uint32_t recoveredMenuDirectionSound(int horizontal,
                                                    int vertical) noexcept {
    // Live no$psx comparison identifies the four FUN_8003D930 transient IDs:
    // down/up/left/right are 1/2/3/4 before FUN_8002F124.
    if (horizontal < 0) return 3;
    if (horizontal > 0) return 4;
    if (vertical < 0) return 2;
    if (vertical > 0) return 1;
    return 0;
}
static_assert(recoveredMenuDirectionSound(1, 0) == 4);

std::string addressHex(std::uint32_t address) {
    char value[9]{};
    std::snprintf(value,sizeof(value),"%08X",address);
    return value;
}

struct Options {
    std::filesystem::path asset_root = ".local/assetpacks";
    std::filesystem::path trace_path = ".local/logs/boot_decomp_trace.log";
    std::filesystem::path settings_path = ".local/config/frontend_settings.ini";
    std::filesystem::path profiles_path = ".local/saves/user_profiles.n97sav";
    std::filesystem::path created_players_path = ".local/saves/created_players.n97cpl";
    std::filesystem::path roster_save_path = ".local/saves/rosters/default.n97rst";
    bool roster_save_explicit = false;
    std::string verify_reorder_save;
    std::filesystem::path reorder_save_capture_dir;
    std::filesystem::path rosters_menu_capture_dir;
    std::filesystem::path create_player_capture_dir;
    std::filesystem::path team_select_capture_dir;
    std::filesystem::path view_rosters_capture_dir;
    std::filesystem::path reorder_capture_dir;
    std::filesystem::path trade_capture_dir;
    std::filesystem::path sign_capture_dir;
    std::filesystem::path release_capture_dir;
    std::filesystem::path native_record_dir;
    bool native_record_audio=false;
    std::size_t native_record_limit=nba97::NativeFrameCapture::default_frame_limit;
    std::filesystem::path semantic_report_path =
        ".local/reports/view_rosters_semantic_trace.json";
    std::filesystem::path roster_scenario_report_path =
        ".local/reports/view_rosters_scenario_trace.json";
    std::uint32_t transition_ms = 3000;
    bool self_test = false;
};

class Trace final {
public:
    explicit Trace(const std::filesystem::path& path) {
        std::filesystem::create_directories(path.parent_path());
        file_ = std::fopen(path.string().c_str(), "w");
    }
    ~Trace() { if (file_) std::fclose(file_); }
    Trace(const Trace&) = delete;
    Trace& operator=(const Trace&) = delete;

    void log(const char* stage, const std::string& detail) {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_).count();
        std::printf("[%6lld ms] %-11s %s\n", static_cast<long long>(ms), stage,
                    detail.c_str());
        std::fflush(stdout);
        if (file_) {
            std::fprintf(file_, "[%6lld ms] %-11s %s\n",
                         static_cast<long long>(ms), stage, detail.c_str());
            std::fflush(file_);
        }
    }

private:
    std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
    std::FILE* file_ = nullptr;
};

class ComApartment final {
public:
    ComApartment() {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(result)) throw std::runtime_error("CoInitializeEx failed");
        initialized_ = true;
    }
    ~ComApartment() { if (initialized_) CoUninitialize(); }
    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;

private:
    bool initialized_ = false;
};

struct Frame {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> bgra;
};

RECT psxPresentationRect(const RECT& client) noexcept {
    const int client_width = client.right - client.left;
    const int client_height = client.bottom - client.top;
    int width = client_width;
    int height = client_height;
    if (client_width * 3 > client_height * 4)
        width = client_height * 4 / 3;
    else
        height = client_width * 3 / 4;
    const int left = client.left + (client_width - width) / 2;
    const int top = client.top + (client_height - height) / 2;
    return RECT{left, top, left + width, top + height};
}

Options parseOptions(int argc, char** argv) {
    Options options;
    // Direct EXE launch (including UI verification) must not depend on the
    // shell's current directory. The desktop shortcut still supplies it.
    if (!std::filesystem::exists(options.asset_root)) {
        wchar_t executable[MAX_PATH]{};
        const auto length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
        if (length && length < MAX_PATH) {
            auto parent = std::filesystem::path(executable).parent_path();
            for (int level = 0; level < 4 && !parent.empty(); ++level, parent = parent.parent_path()) {
                if (std::filesystem::exists(parent / options.asset_root)) {
                    options.asset_root = parent / options.asset_root;
                    options.trace_path = parent / options.trace_path;
                    options.settings_path = parent / options.settings_path;
                    options.profiles_path = parent / options.profiles_path;
                    options.created_players_path = parent / options.created_players_path;
                    options.roster_save_path = parent / options.roster_save_path;
                    break;
                }
            }
        }
    }
    if (const char* root = std::getenv("NBA97_ASSET_ROOT")) options.asset_root = root;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--self-test") options.self_test = true;
        else if (arg == "--asset-root" && i + 1 < argc) options.asset_root = argv[++i];
        else if (arg == "--transition-ms" && i + 1 < argc)
            options.transition_ms = static_cast<std::uint32_t>(
                std::strtoul(argv[++i], nullptr, 10));
        else if (arg == "--trace" && i + 1 < argc) options.trace_path = argv[++i];
        else if (arg == "--settings" && i + 1 < argc) options.settings_path = argv[++i];
        else if (arg == "--profiles" && i + 1 < argc) options.profiles_path = argv[++i];
        else if (arg == "--created-players" && i + 1 < argc) options.created_players_path = argv[++i];
        else if (arg == "--roster-save" && i + 1 < argc) {
            options.roster_save_path = argv[++i]; options.roster_save_explicit = true;
        }
        else if (arg == "--verify-reorder-save" && i + 1 < argc)
            options.verify_reorder_save = argv[++i];
        else if (arg == "--capture-reorder-save" && i + 1 < argc)
            options.reorder_save_capture_dir = argv[++i];
        else if (arg == "--capture-rosters-menu" && i + 1 < argc)
            options.rosters_menu_capture_dir = argv[++i];
        else if (arg == "--capture-create-player" && i + 1 < argc)
            options.create_player_capture_dir = argv[++i];
        else if (arg == "--capture-team-select" && i + 1 < argc)
            options.team_select_capture_dir = argv[++i];
        else if (arg == "--capture-view-rosters" && i + 1 < argc)
            options.view_rosters_capture_dir = argv[++i];
        else if (arg == "--capture-reorder" && i + 1 < argc)
            options.reorder_capture_dir = argv[++i];
        else if (arg == "--capture-trade" && i + 1 < argc)
            options.trade_capture_dir = argv[++i];
        else if (arg == "--capture-sign" && i + 1 < argc)
            options.sign_capture_dir = argv[++i];
        else if (arg == "--capture-release" && i + 1 < argc)
            options.release_capture_dir = argv[++i];
        else if (arg == "--record-native-frames" && i + 1 < argc)
            options.native_record_dir = argv[++i];
        else if (arg == "--record-native-audio") options.native_record_audio=true;
        else if (arg == "--record-native-limit" && i + 1 < argc) {
            const std::string value=argv[++i];std::size_t used=0;
            const auto limit=std::stoul(value,&used);
            if(used!=value.size() || !limit || limit>nba97::NativeFrameCapture::max_frames)
                throw std::runtime_error("native recording limit must be 1..6000");
            options.native_record_limit=limit;
        }
        else if (arg == "--semantic-report" && i + 1 < argc)
            options.semantic_report_path = argv[++i];
        else if (arg == "--roster-scenario-report" && i + 1 < argc)
            options.roster_scenario_report_path = argv[++i];
    }
    if(!options.verify_reorder_save.empty() && (!options.roster_save_explicit || options.reorder_save_capture_dir.empty()))
        throw std::runtime_error("save verification requires explicit --roster-save and --capture-reorder-save paths");
    if(options.native_record_audio && options.native_record_dir.empty())
        throw std::runtime_error("--record-native-audio requires --record-native-frames");
    if(!options.team_select_capture_dir.empty()) {
        const auto root=std::filesystem::weakly_canonical(options.team_select_capture_dir).parent_path();
        const auto private_root=std::filesystem::weakly_canonical(".local/verification/team_select");
        const auto relative=root.lexically_relative(private_root);
        if(relative.empty() || relative=="." || *relative.begin()==".." || !options.roster_save_explicit)
            throw std::runtime_error("Team Select capture requires an isolated run directory and explicit saves");
        for(const auto& path:{options.settings_path,options.profiles_path,options.created_players_path,options.roster_save_path})
            if(std::filesystem::weakly_canonical(path).parent_path()!=root)
                throw std::runtime_error("Team Select capture settings/profile/created/roster paths must all be isolated beside its frame directory");
    }
    if(!options.native_record_dir.empty() && (options.self_test || !options.verify_reorder_save.empty() ||
       !options.reorder_capture_dir.empty() || !options.trade_capture_dir.empty() || !options.sign_capture_dir.empty() || !options.view_rosters_capture_dir.empty() ||
       !options.rosters_menu_capture_dir.empty() || !options.create_player_capture_dir.empty() ||
       !options.release_capture_dir.empty() || !options.team_select_capture_dir.empty()))
        throw std::runtime_error("native recording requires the live window, not checkpoint/self-test mode");
    return options;
}

Frame makeFrame(const PshImage& image) {
    Frame frame{image.width, image.height, image.rgba};
    for (std::size_t i = 0; i < frame.bgra.size(); i += 4)
        std::swap(frame.bgra[i], frame.bgra[i + 2]);
    return frame;
}

Frame blendFrames(const Frame& from, const Frame& to, std::uint32_t elapsed,
                  std::uint32_t duration) {
    Frame result = to;
    if (from.bgra.size() != to.bgra.size() || duration == 0) return result;
    const std::uint32_t amount = (std::min)(elapsed, duration);
    for (std::size_t i = 0; i < result.bgra.size(); ++i)
        result.bgra[i] = static_cast<std::uint8_t>(
            (static_cast<std::uint32_t>(from.bgra[i]) * (duration - amount) +
             static_cast<std::uint32_t>(to.bgra[i]) * amount) / duration);
    return result;
}

void validateFullscreen(const PshImage& image, const char* name) {
    if (image.width != kPsxWidth || image.height != kPsxHeight)
        throw std::runtime_error(std::string(name) + " must be 512x240");
}

void writePpm(const PshImage& image, const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot write verification capture: " + path.string());
    output << "P6\n" << image.width << ' ' << image.height << "\n255\n";
    for (std::size_t at = 0; at < image.rgba.size(); at += 4) {
        const char rgb[3]{
            static_cast<char>(image.rgba[at]),
            static_cast<char>(image.rgba[at + 1]),
            static_cast<char>(image.rgba[at + 2])};
        output.write(rgb, sizeof(rgb));
    }
    if (!output) throw std::runtime_error("failed writing verification capture: " + path.string());
}

class BootApplication final {
public:
    explicit BootApplication(Options options)
        : options_(options), trace_(options_.trace_path),
          intro_movie_(options_.asset_root / "intro" / "Z0ZTITLE.avi") {
        loadRecoveredAssets();
        flow_.reset();
    }

    BootApplication(const BootApplication&) = delete;
    BootApplication& operator=(const BootApplication&) = delete;

    int run() {
        if(!options_.verify_reorder_save.empty()) return verifyReorderSave();
        if (options_.self_test) return runSelfTest();
        if (!options_.rosters_menu_capture_dir.empty())
            return captureRostersMenu();
        if (!options_.create_player_capture_dir.empty())
            return captureCreatePlayer();
        if (!options_.team_select_capture_dir.empty()) return captureTeamSelect();
        if (!options_.view_rosters_capture_dir.empty())
            return captureViewRosters();
        if (!options_.reorder_capture_dir.empty()) return captureReorder();
        if (!options_.trade_capture_dir.empty()) return captureTrade();
        if (!options_.sign_capture_dir.empty()) return captureSign();
        if (!options_.release_capture_dir.empty()) return captureRelease();
        registerWindowClass();
        createMainWindow();
        ShowWindow(window_, SW_SHOWNORMAL);
        SetForegroundWindow(window_);
        UpdateWindow(window_);
        if (!SetTimer(window_, kFrameTimer, kFrameIntervalMs, nullptr))
            throw std::runtime_error("SetTimer failed");
        trace_.log("DISPLAY", "native C++ Win32 app visible at (80,80); SPACE advances, ESC exits");
        if(!options_.native_record_dir.empty())
            trace_.log("RECORD-ARMED",std::string("F9 starts/stops passive frontend capture; limit=")+std::to_string(options_.native_record_limit)+" presentations; audio="+
                (options_.native_record_audio?"current-process Windows mix (no microphone/system fallback)":"absent"));

        MSG message{};
        while (true) {
            const BOOL result = GetMessageW(&message, nullptr, 0, 0);
            if (result == 0) return static_cast<int>(message.wParam);
            if (result == -1) throw std::runtime_error("GetMessageW failed");
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

private:
    void loadRecoveredAssets() {
        trace_.log("BOOT", "PS-X entry 0x801E3508 (Ghidra + recomp agreement)");
        trace_.log("RECOVERED", "0x801E1A68 loads ZLOADSCR.PSH and ZLOADING.PSH");
        trace_.log("OVERLAY", "FEONLY entry 0x8007B79C");
        trace_.log("RECOVERED", "0x80035984 calls legal 0x80036684 before title 0x8002EEF4");
        trace_.log("RECOVERED", "0x80036684 loads ZLEGAL.PSH via 0x80028BAC");
        trace_.log("RECOVERED", "0x8002D768 loads ZCPYRT97.PSH via 0x80028BAC");

        const PshImage load = load_psh(options_.asset_root / "boot" / "ZLOADSCR.PSH");
        const PshImage strip = load_psh(options_.asset_root / "boot" / "ZLOADING.PSH");
        const PshImage legal = load_psh(options_.asset_root / "frontend" / "ZLEGAL.PSH");
        PshImage title = load_psh(options_.asset_root / "frontend" / "ZCPYRT97.PSH");
        title_source_ = title;
        menu_font_ = nba97::load_psh_font(
            options_.asset_root / "fonts" / "ZFONT0.PSH", 10, 1);
        control_font_ = nba97::load_psh_font(
            options_.asset_root / "fonts" / "ZFONT1.PSH", 10, 1);
        validateFullscreen(load, "ZLOADSCR.PSH");
        validateFullscreen(legal, "ZLEGAL.PSH");
        validateFullscreen(title, "ZCPYRT97.PSH");
        trace_.log("ASSET", describe_psh(load));
        trace_.log("ASSET", describe_psh(strip));
        trace_.log("ASSET", describe_psh(legal));
        trace_.log("ASSET", describe_psh(title));
        const int prompt_width = menu_font_.textWidth("press start");
        if (prompt_width != kRecoveredPressStartWidth)
            throw std::runtime_error("ZFONT0 press-start metrics do not match recovered width");
        nba97::draw_psh_text_centered(title, menu_font_, "press start",
                                      kPressStartCenterX, kPressStartY);
        trace_.log("FONT", "ZFONT0.PSH page=0 glyphs=" +
                           std::to_string(menu_font_.glyphCount()) +
                           " transposed=" +
                           std::to_string(menu_font_.transposedGlyphCount()) +
                           " space=10 kerning=1");
        trace_.log("FONT-XPOSE", "0x80029EC0 signed Position-X UV transpose applied; lowercase r/t restored");
        trace_.log("FONT-CONTROL", "ZFONT1.PSH record 0994 loaded for recovered View Player play-cool-fact control");
        trace_.log("RECOVERED", "0x80035B00 calls 0x8002C6B0: font page 0, text=press start, center=(256,30), align=1");
        trace_.log("TITLE-TEXT", "original ZFONT0 glyphs width=97 start=(208,30 metadata-adjusted)");
        load_frame_ = makeFrame(load);
        legal_frame_ = makeFrame(legal);
        title_frame_ = makeFrame(title);

        const auto menu_root = options_.asset_root / "menu";
        validateMenuAsset(menu_root / "ZFEMODEL.BIN", 44288);
        validateMenuAsset(menu_root / "ZFEMOCAP.BIN", 22188);
        validateMenuAsset(menu_root / "ZFEPLAYR.ART", 73984);
        create_player_preview_=std::make_unique<nba97::CreatePlayerPreview>(options_.asset_root);
        trace_.log("CREATE-MODEL-DECODE", create_player_preview_->description()+
            "; 0x800687BC relocation, 0x80035260 mocap, 0x80069098 parent routing, 0x80062F4C pivots, 0x800631B8 vertices, and all 20 runtime matrices/attachment endpoints are live-RAM exact; synchronized proof matches SXY=753/753, descriptor-0 AVSZ3 ordering=251/251, NCLIP acceptance, and the merged primary/per-part FT3 sequence=139/139; field-selected close-up/full-body camera state is live-RAM exact; FUN_80067F74 shared body uploads=(dthr:r1@832/374,dthl:r4@850/374), FUN_80067A14 team uploads=(r2@832/256,r6@949/374,r7@889/374,r4@832/488,r0@886/468), FUN_800626D0 dynamic head=(898/454), number/name pages=(960/256,960/320), shared SHOE=(922/454), packet CLUT rows=(512/500,512/501,528/502,544/502), synchronized raster missing=0; pixel-exact PS1 triangle coverage/interpolation and whole-screen original scoring remain pending");
        validateMenuAsset(menu_root / "ZLOGOS.PSH", 99848);
        validateMenuAsset(menu_root / "ZTMPAL.PSH", 21544);
        validateMenuAsset(menu_root / "ZBPAL.PSH", 17264);
        validateMenuAsset(menu_root / "ZCURSOR.VH", 1836);
        validateMenuAsset(menu_root / "ZCURSOR.VB", 60940);
        validateMenuAsset(menu_root / "ZCARD.BIN", 474240);
        validateMenuAsset(menu_root / "Z1PORT.BIG", 13296378);
        validateMenuAsset(menu_root / "Z1PORT.IDX", 3970);
        validateMenuAsset(menu_root / "Z1COOL.BIG", 122580678);
        validateMenuAsset(menu_root / "Z1COOL.IDX", 19746);
        validateMenuAsset(menu_root / "ZTMENU1.CNK", 8522396);
        validateMenuAsset(menu_root / "ZSET1.PSP", 342448);
        validateMenuAsset(menu_root / "ZSET4.PSP", 332084);
        validateMenuAsset(menu_root / "ZSET5.PSP", 146888);
        validateMenuAsset(menu_root / "ZSET7.PSP", 323444);
        validateMenuAsset(menu_root / "ZSET8.PSP", 297432);
        loadMenuSprites(menu_root / "ZSET1-decoded");
        loadRecoveredBottomSprites(menu_root / "ZSET4-decoded", roster_sprites_, true);
        loadCreatePlayerSprites(menu_root / "ZSET5-decoded");
        create_player_delete_assets_ =
            std::make_unique<nba97::CreatePlayerDeleteAssets>(options_.asset_root);
        create_player_help_pack_ = std::make_unique<nba97::FrontendHelpPack>(
            options_.asset_root / "reorder/help.n97ui");
        loadTeamRosterBackgrounds(menu_root / "ZSET4-team-backgrounds");
        loadRecoveredBottomSprites(menu_root / "ZSET7-decoded", users_sprites_, false);
        loadPlayerCardSprites(menu_root / "ZSET8-decoded");
        loadMenuCards(menu_root / "ZCARD-decoded");
        nba97_created_catalog_init(&created_players_);
        trace_.log("RECOVERED", "0x8002F258 selects ZTMENU1.CNK frontend audio");
        trace_.log("RECOVERED", "0x8002FDA4 loads 33 ZTMPAL.PSH palettes; 0x80030308 loads ZBPAL.PSH");
        trace_.log("ROSTER-PALETTE", "FUN_8002FE58 patches ZSET4 Bkg colors 0..159 from ZTMPAL.PSH and preserves local colors 160..255");
        trace_.log("ROSTER-LAYOUT", "state 0x10 / ZSET4: Bkga-d x=0/128/256/384, ba35=(142,10), frml=(30,15), dynamic team logo=(40,16), team arrows=ZFONT0 0x8D/0x8A at (157/381,66), scroll arrows=0x8B/0x8C at (48,108/168), help=(235,217)");
        trace_.log("ROSTER-ANIM", "ba35 title uses FUN_8003186C/FUN_80034A5C discrete four-corner shake; FUN_8002FF80 team crossfade=17 ticks; FUN_8002AB88 selected-row neutral-to-gold pulse=20 ticks");
        trace_.log("CREATE-ASSETS", "screen 0x1F uses private ZSET5.PSP: ba05 Create Player, c00c..c05c Edit/New/Delete plates, red1 disabled Edit/Delete variants; model assets remain separate");
        trace_.log("CREATE-MODEL", "8004C344 -> 8006785C/80067A14/8006781C/80067F50; ZFEMODEL+ZFEMOCAP+ZFEPLAYR plus team ZDOM D/E/F/S families rebuild the live preview");
        trace_.log("ROSTER-FIELDS", "recomp descriptor tables: categories=6 displays=56; live no$psx confirms L2/R2 stat-layer change enters FUN_80059610; six-row repeat=7/5/3/1 ticks");
        trace_.log("RECOVERED", "0x80035260 loads ZFEMOCAP.BIN; original frontend model/art packs are local");
        trace_.log("RECOVERED", "state 0x24 FUN_8005A538 loads Z1PORT.IDX/BIG and Z1COOL.IDX/BIG for View Player");
        trace_.log("PLAYER-LAYOUT", "FUN_8003F7C8 reloads gfx state 0x24 / ZSET8 LIVE: Bkge-h, brta-d/brba-d, ba41=(40,18), team *Z=(296,35), cros=(336,204), o18a=(356,198)");
        trace_.log("PLAYER-ANIM", "FUN_8003186C + FUN_80034A5C: ba41 uses two 128-page GPU pieces with discrete four-corner jumble; scanline wave disabled");
        trace_.log("PLAYER-POSITION", "descriptor case 0x13: roster slots 0..4 use original starting C/PF/SF/SG/PG strings at 0x80024C98..0x80024CC8; bench uses database position");
        trace_.log("COOL-FACT", "view-card help descriptor 0x800B22F0 uses ZFONT control glyph 0x94 play / 0x93 stop; input 0x800 routes FUN_80059F30 -> FUN_80059E14");
        roster_database_.load(options_.asset_root / "database" / "roster.n97db");
        std::string roster_base_hex;
        for(auto byte:roster_database_.baseIdentity()) {
            constexpr char digits[]="0123456789abcdef";
            roster_base_hex+=digits[byte>>4]; roster_base_hex+=digits[byte&15];
        }
        trace_.log("ROSTER-BASE","canonical-v1 SHA256="+roster_base_hex+
            "; immutable defaults=1070 bytes shared with drafts; differs-from-original="+
            std::to_string(roster_database_.differsFromOriginal())+"; before local overrides");
        loadRosterSave();
        if (options_.create_player_capture_dir.empty()) {
            const auto created_status=created_player_store_.load(options_.created_players_path,
                                                                 created_players_);
            trace_.log("CREATE-LOAD", std::string(
                created_status==nba97::CreatedPlayerLoadStatus::NewStore ? "new store" :
                created_status==nba97::CreatedPlayerLoadStatus::RecoveredBackup ? "recovered backup" : "loaded")+
                " generation="+std::to_string(created_player_store_.generation())+
                " count="+std::to_string(nba97_created_count(&created_players_))+
                " path="+created_player_store_.path().string());
        } else {
            trace_.log("CREATE-LOAD", "verification capture uses an isolated empty catalogue; persistent store untouched");
        }
        trace_.log("ROSTER-DB", "external private pack version=" +
            std::to_string(roster_database_.version()) + " teams=" +
            std::to_string(roster_database_.teams().size()) + " players=" +
            std::to_string(roster_database_.players().size()));
        trace_.log("ROSTER-INDEX", "FUN_8005FE14 boundary=0x1ED; assigned=" +
            std::to_string(roster_database_.assignedPlayerCount()) + " free-agents=" +
            std::to_string(roster_database_.freeAgentCount()) + " hidden/unlisted=" +
            std::to_string(roster_database_.unlistedPlayerCount()) + "; all references validated");
        trace_.log("ROSTER-SLOT-COPY", "FUN_80057864 copied 29x15 team slots plus the original 100-slot free-agent tail; FUN_80054CBC membership/count refresh represented; FUN_8005DB34 derived-rating refresh marked dirty");
        trace_.log("ROSTER-SLOTS", "FUN_8005770C resolves 29 teams x 15 signed slots; -1 -> null; special stat-index-zero players route through the private 5x5 FUN_8005768C fallback table");
        menu_.reset();
        const auto profile_status = profile_store_.load(options_.profiles_path);
        active_user_profiles_ = static_cast<int>(profile_store_.profiles().size());
        menu_.setActiveUserProfiles(active_user_profiles_);
        const bool restored = settings_.load(options_.settings_path);
        trace_.log("SETTINGS", std::string(restored ? "restored " : "defaults; save target ") +
                               options_.settings_path.string());
        trace_.log("RECOVERED", "Rules state=1 control 0x80098194; Options state=2 control 0x80098258");
        trace_.log("RECOVERED", "Rosters state=9 FUN_80057CE4; Users state=19 FUN_8005CF78; Card state=11 FUN_80053F4C");
        trace_.log("PROFILE-LOAD", std::string(
            profile_status == nba97::ProfileLoadStatus::NewStore ? "new store" :
            profile_status == nba97::ProfileLoadStatus::RecoveredBackup ? "recovered backup" : "loaded") +
            " generation=" + std::to_string(profile_store_.generation()) + " path=" +
            options_.profiles_path.string());
        trace_.log("PROFILE-SCAN", std::to_string(active_user_profiles_) +
            "/20 named records; FUN_8005CD88 stride=0x6C name=+0x5D; Users " +
            (active_user_profiles_ == 0 ? "disabled" : "enabled"));
        trace_.log("RECOVERED", "0x8003E698 arcade; 0x8003E714 simulation; 0x8003E620 restores custom snapshot");
        menu_frame_ = makeFrame(nba97::renderGameSetupMenu(
            menu_, title_source_, menu_font_, menu_sprites_, menu_cards_, 0));
    }

    int captureRelease() {
        const auto dir=std::filesystem::weakly_canonical(options_.release_capture_dir);
        const auto root=std::filesystem::weakly_canonical(".local/verification");
        const auto relative=dir.lexically_relative(root);
        if(relative.empty() || relative=="." || *relative.begin()==".." || std::filesystem::exists(dir))
            throw std::runtime_error("Release verification requires a fresh directory under .local/verification");
        std::filesystem::create_directories(dir);
        const auto original=roster_database_.slotTable();
        roster_store_=std::make_unique<nba97::RosterSaveStore>(dir/"rosters.n97rst");roster_store_->load(roster_database_);
        auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
        const auto generation=roster_store_->accepted().generation;
        const auto empty=roster_database_.freeAgentCount();
        auto settle=[&]{for(int i=0;i<50&&isRelease();++i){menu_elapsed_ms_+=17;
            nba97_trade_frame(&trade_screen_,0);nba97_reorder_child_input_ready(&reorder_child_,0);
            if(trade_choice_address_)tradeChoiceEvent(nba97_reset_tick(&trade_choice_,0));else stepReorderHelp(0,true);
            if(!isRelease())break;nba97_frontend_palette_tick(&trade_palette_,compare_backgrounds_->bank(),33);
            advanceComparePalette();if(compare_refresh_.remaining)advanceCompareRefresh();
            if(compare_repeat_.post_frames)--compare_repeat_.post_frames;}
            frontend_transition_active_=false;};
        auto capture=[&](const char* name,bool advance=true){if(advance)settle();updatePlayerPhoto(true);
            writePpm(isRelease()?renderTrade():renderBottomMenu(),dir/(std::string(name)+".ppm"));
            trace_.log("RELEASE-CHECKPOINT",name);};
        auto enter=[&]{frontend_page_=nba97::FrontendPage::Rosters;bottom_menu_.open(frontend_page_);
            bottom_menu_.setReleaseAvailable(nba97_release_available(original.data(),0,0)!=0);
            bottom_menu_.setSelected(2);rebuildMenuFrame();completeRecoveredBottomSelection();
            require(isRelease()&&frontend_transition_active_,"Release card did not route through native transition");
            require(transition_source_.width==512&&transition_frame_.width==512,"transition frame missing");};
        enter();capture("entry");
        unsigned released_cases=0,empty_cases=0;
        for(int team=0;team<29;++team)for(unsigned slot=0;slot<15;++slot) {
            Nba97TradeScreen test{};
            require(nba97_release_begin(&test,original.data(),int16_t(team),0,nullptr,uint8_t(slot),uint8_t((std::min)(slot,9u)))!=0,"real Release matrix entry");
            const bool occupied=test.selected[0]!=UINT16_MAX;
            const auto data=tradeData();const auto event=nba97_trade_input(&test,0x800,&data);
            require(event==(occupied?NBA97_TRADE_SWAPPED:NBA97_TRADE_IDLE),"real Release matrix outcome");
            nba97::RosterSlots proposal;std::copy_n(test.working,535,proposal.begin());
            (void)roster_database_.prepareSlotTable(proposal);
            if(occupied)++released_cases;else ++empty_cases;
        }
        trace_.log("RELEASE-MATRIX","PASS 435 real-data donor slots; released="+std::to_string(released_cases)+
            " empty="+std::to_string(empty_cases)+"; original preference pack; every population validated; saved=no");
        require(trade_screen_.frontend_state==17 && trade_screen_.team[1]==29 &&
            trade_screen_.cursor[1]==empty && trade_screen_.top[1]==(empty<4?0:(std::min)(empty-4,std::size_t(94))),"Release receiver positioning");
        require(trade_screen_.list_kind[0]==1&&trade_screen_.list_kind[1]==0&&!trade_screen_.input_callback[1],"Release constructor binding");
        handleTradeKey(VK_DOWN);capture("donor-row");
        require(trade_screen_.cursor[0]==1&&trade_screen_.cursor[1]==empty,"donor movement changed receiver");
        const auto team=trade_screen_.team[0];
        handleTradeKey(VK_RIGHT);capture("donor-team");
        require(trade_screen_.team[0]==(team+1)%29&&trade_screen_.team[1]==29,"team scan wrong side");
        handleTradeKey(VK_LEFT);settle();
        for(int i=0;i<20;++i)handleTradeKey(VK_DOWN);
        capture("donor-bottom");require(trade_screen_.cursor[0]==14&&trade_screen_.top[0]==9,"six row scroll bounds");
        for(int i=0;i<20;++i)handleTradeKey(VK_UP);settle();
        handleTradeKey('F');capture("help-growing",false);
        require(reorder_help_state_==17&&reorder_help_index_==0&&reorder_help_.rect.width==20&&reorder_help_.rect.height==10,"small Help opening");
        capture("help-open");
        require(reorder_help_.rect.x==121&&reorder_help_.rect.y==80&&
            reorder_help_.rect.width==270&&reorder_help_.rect.height==125&&nba97_help_text_visible(&reorder_help_),"original Release modal rectangle");
        handleTradeKey(VK_RETURN);capture("help-return");
        require(isRelease()&&reorder_help_.phase==NBA97_HELP_CLOSED,"Help dismissal incorrectly left Release");
        auto dismiss=[&]{handleTradeKey(VK_RETURN);settle();};
        handleTradeKey('D');capture("view");
        require(reorder_child_.state==0x24&&roster_viewer_.selectedPlayer(viewerDatabase())->id==original[team*15],"Release View donor identity");
        handleTradeKey('F');capture("view-help");dismiss();
        handleTradeKey('K');capture("view-browsed");
        require(roster_viewer_.teamIndex()!=std::size_t(team),"View team browse");
        handleTradeKey(VK_RETURN);capture("view-return");
        require(!reorder_child_.state&&!trade_choice_address_&&trade_screen_.team[0]==team,"Release View must not adopt");
        handleTradeKey('S');capture("compare");
        require(reorder_compare_.team[0]==team&&reorder_compare_.team[1]==team&&
            reorder_compare_.player[0]==original[team*15]&&reorder_compare_.player[1]==original[team*15],"Release Compare repeats donor, not empty pool");
        handleTradeKey('C');settle();handleTradeKey('K');capture("compare-browsed");
        require(reorder_compare_.team[1]!=team,"Compare right team browse");
        handleTradeKey('F');capture("compare-help");dismiss();
        handleTradeKey(VK_RETURN);capture("compare-return");
        require(!reorder_child_.state&&!trade_choice_address_&&trade_screen_.team[0]==team,"Compare must not adopt");
        for(int i=0;i<5;++i)handleTradeKey(VK_DOWN);
        const auto outgoing=trade_screen_.selected[0];
        handleTradeKey('C');capture("release-start",false);capture("released");
        require(trade_screen_.counts[29]==empty+1&&trade_screen_.working[435+empty]==outgoing&&
            trade_screen_.cursor[1]==empty+1&&roster_database_.slotTable()==original,"release isolated draft");
        handleTradeKey('X');capture("discard-prompt");require(trade_choice_address_==0x800af4f8,"Release discard prompt");
        handleTradeKey('C');capture("discard-kept");require(isRelease()&&!trade_choice_address_&&nba97_trade_dirty(&trade_screen_),"default keep edit");
        handleTradeKey('X');settle();handleTradeKey(VK_UP);settle();handleTradeKey('C');capture("discard-return");
        require(frontend_page_==nba97::FrontendPage::Rosters&&roster_database_.slotTable()==original,"Release discard changed live roster");
        require(roster_database_.slotTable()==original && roster_store_->accepted().generation==generation &&
            !std::filesystem::exists(dir/"rosters.n97rst"),"discarded screen wrote a roster save");
        enter();capture("reentry");
        require(trade_screen_.cursor[0]==0&&trade_screen_.top[0]==0&&trade_screen_.cursor[1]==empty,"menu reentry positions");
        handleTradeKey('X');capture("cancel-return");
        require(frontend_page_==nba97::FrontendPage::Rosters&&nba97_trade_result(&trade_screen_)==-1,"clean cancel result");
        enter();settle();for(int i=0;i<5;++i)handleTradeKey(VK_DOWN);handleTradeKey('C');settle();
        const auto draft=tradeDraft()->slotTable();bool injected=false;
        reorder_save_hooks_={[](nba97::RosterSaveStage stage,void* p){if(stage==nba97::RosterSaveStage::PartialWrite){
            *static_cast<bool*>(p)=true;throw std::runtime_error("injected Release save failure");}},&injected};
        handleTradeKey(VK_RETURN);capture("save-failed");
        require(injected&&isRelease()&&roster_database_.slotTable()==original&&tradeDraft()->slotTable()==draft,"failed Release save corrupted draft/live");
        dismiss();reorder_save_hooks_={};handleTradeKey(VK_RETURN);capture("accept-return");
        require(frontend_page_==nba97::FrontendPage::Rosters&&roster_database_.slotTable()==draft,"Release accepted publish");
        auto restart=roster_database_.prepareSlotTable(original);nba97::RosterSaveStore reopened(dir/"rosters.n97rst");
        reopened.load(restart);require(restart.slotTable()==draft&&restart.rosterOwner(outgoing)==29,"Release restart ownership");
        enter();capture("restart");
        for(int i=0;i<5;++i)handleTradeKey(VK_DOWN);
        while(trade_screen_.counts[team]>8){handleTradeKey('C');settle();}
        handleTradeKey('C');capture("minimum-refused");
        require(reorder_notice_&&trade_screen_.notice.message_address==0x800aeb54,"eight-player minimum original notice");dismiss();
        const auto retained=tradeDraft()->slotTable();handleTradeKey('D');capture("quirk-view");
        handleTradeKey(VK_RETURN);capture("quirk-return");
        require(nba97_trade_dirty(&trade_screen_)&&!nba97_trade_undo_dirty(&trade_screen_),"Release shared constructor checkpoint");
        handleTradeKey('X');settle();require(frontend_page_==nba97::FrontendPage::Rosters&&!trade_choice_address_&&
            roster_database_.slotTable()==retained,"Release pre-child changes must survive Cancel");
        auto retained_restart=roster_database_.prepareSlotTable(original);nba97::RosterSaveStore retained_store(dir/"rosters.n97rst");
        retained_store.load(retained_restart);require(retained_restart.slotTable()==retained,"Release checkpoint restart mismatch");
        enter();capture("retained-restart");handleTradeKey('X');settle();commitRosterReset();capture("reset-return");
        auto reset_restart=restart;nba97::RosterSaveStore reset_store(dir/"rosters.n97rst");reset_store.load(reset_restart);
        require(reset_restart.slotTable()==original&&roster_database_.slotTable()==original,"Release Reset persistence");
        trace_.log("RELEASE-HOST-VERIFY","PASS 30 checkpoints; release/refusals/Help/View/Compare/discard/save failure/restart/Reset; isolated save only; not original-reference equivalence");
        return 0;
    }

    int captureSign() {
        const auto dir=std::filesystem::weakly_canonical(options_.sign_capture_dir);
        const auto root=std::filesystem::weakly_canonical(".local/verification");
        const auto relative=dir.lexically_relative(root);
        if(relative.empty() || relative=="." || *relative.begin()==".." || std::filesystem::exists(dir))
            throw std::runtime_error("Sign verification requires a fresh directory under .local/verification");
        std::filesystem::create_directories(dir);
        const auto original=roster_database_.slotTable();
        roster_store_=std::make_unique<nba97::RosterSaveStore>(dir/"rosters.n97rst");roster_store_->load(roster_database_);
        frontend_page_=nba97::FrontendPage::SignFreeAgent;openTrade();
        auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
        auto settle=[&]{for(int i=0;i<50&&isSign();++i){menu_elapsed_ms_+=17;
            nba97_trade_frame(&trade_screen_,0);nba97_reorder_child_input_ready(&reorder_child_,0);
            if(trade_choice_address_)tradeChoiceEvent(nba97_reset_tick(&trade_choice_,0));else stepReorderHelp(0,true);
            if(!isSign())break;nba97_frontend_palette_tick(&trade_palette_,compare_backgrounds_->bank(),33);
            advanceComparePalette();if(compare_refresh_.remaining)advanceCompareRefresh();
            if(compare_repeat_.post_frames)--compare_repeat_.post_frames;}
            frontend_transition_active_=false;};
        auto capture=[&](const char* name){settle();updatePlayerPhoto(true);auto frame=renderTrade();
            if(reorder_child_.state==0x24&&roster_viewer_.teamIndex()==29&&!nba97_help_visible(&reorder_help_)) {
                const auto& strip=player_sprites_.at("xfrZ");unsigned checked=0;
                require(strip.width==39&&strip.height==156,"free-agent strip dimensions");
                for(unsigned y=0;y<strip.height;++y)for(unsigned x=0;x<strip.width;++x) {
                    auto from=(y*strip.width+x)*4,to=((35+y)*512+296+x)*4;
                    if(!strip.rgba[from+3])continue;
                    require(std::equal(strip.rgba.begin()+from,strip.rgba.begin()+from+3,frame.rgba.begin()+to),
                        "original xfrZ strip missing/shifted/replaced");++checked;
                }
                require(checked>1000,"free-agent strip opacity coverage");
                trace_.log("SIGN-STRIP-VERIFY","xfrZ 39x156 at296,35; original opaque pixels="+std::to_string(checked));
            }
            writePpm(frame,dir/(std::string(name)+".ppm"));
            trace_.log("SIGN-CHECKPOINT",name);};
        auto dismiss=[&]{handleTradeKey(VK_RETURN);settle();};
        capture("entry");
        require(trade_screen_.team[0]==29&&trade_screen_.frontend_state==14,"Sign entry identity");
        handleTradeKey('F');capture("help-first");require(reorder_help_state_==14&&reorder_help_index_==0,"Sign first Help routing");dismiss();
        handleTradeKey(VK_RIGHT);capture("destination-scan");handleTradeKey(VK_LEFT);settle();
        handleTradeKey('D');capture("view-free-agent");
        require(roster_viewer_.teamIndex()==29&&roster_viewer_.selectedPlayer(viewerDatabase())->id==original[435],"View free agent identity");
        handleTradeKey(VK_RIGHT);capture("view-next-free-agent");require(roster_viewer_.playerIndex()==1,"free agent cycle");
        handleTradeKey('F');capture("view-help");dismiss();handleTradeKey('X');settle();
        require(!reorder_child_.state&&trade_screen_.team[0]==29,"free-agent View return");
        require(trade_screen_.team[1]==3&&trade_screen_.phase==NBA97_TRADE_FIRST,"original Sign re-entry to Chicago");
        handleTradeKey(VK_LEFT);settle(); // Restore Charlotte for the next independent case.
        handleTradeKey('S');capture("compare-free-agent");
        require(reorder_compare_.team[0]==29&&reorder_compare_.player[0]==original[435],"Compare free agent");
        handleTradeKey('X');settle();
        require(trade_screen_.team[1]==3,"Compare re-entry normalizes receiver");
        handleTradeKey(VK_LEFT);settle();
        handleTradeKey('C');capture("second");handleTradeKey('F');capture("help-second");
        require(reorder_help_state_==14&&reorder_help_index_==1,"second Help routing");dismiss();
        handleTradeKey('D');settle();handleTradeKey('K');capture("second-view-browsed");
        require(roster_viewer_.teamIndex()!=trade_screen_.team[1],"Sign second View team browse");
        handleTradeKey(VK_RETURN);capture("second-view-return");
        require(!reorder_child_.state&&!trade_choice_address_&&trade_screen_.team[0]==29&&
            trade_screen_.team[1]==3&&trade_screen_.phase==NBA97_TRADE_SECOND,"Sign View original receiver/phase re-entry");
        handleTradeKey(VK_LEFT);settle();
        handleTradeKey('S');settle();handleTradeKey(VK_RIGHT);capture("second-compare-browsed");
        handleTradeKey(VK_RETURN);capture("second-compare-return");
        require(!reorder_child_.state&&!trade_choice_address_&&trade_screen_.cursor[0]==0&&
            trade_screen_.cursor[1]==0&&trade_screen_.team[1]==3&&trade_screen_.phase==NBA97_TRADE_SECOND,
            "Sign Compare original receiver/phase re-entry");
        handleTradeKey(VK_LEFT);settle();
        handleTradeKey('C');capture("occupied-refused");
        require(trade_screen_.notice.message_address==0x800aed88&&!trade_screen_.changes,"occupied refusal");dismiss();
        handleTradeKey('X');settle();require(trade_screen_.phase==NBA97_TRADE_FIRST,"cancel second");
        for(int i=0;i<99;++i)handleTradeKey(VK_DOWN);
        capture("empty-tail");handleTradeKey('C');capture("empty-source-refused");
        require(trade_screen_.notice.message_address==0x800aed20,"empty source notice");dismiss();
        for(int i=0;i<99;++i)handleTradeKey(VK_UP);
        handleTradeKey('C');
        const auto target=trade_screen_.team[1];const unsigned count=trade_screen_.counts[target];
        require(count<15,"fixture needs a team vacancy");
        for(unsigned i=0;i<14;++i)handleTradeKey(VK_DOWN);
        capture("vacancy-selected");const auto player=trade_screen_.selected[0];
        handleTradeKey('C');capture("signed");
        require(trade_screen_.changes==1&&trade_screen_.working[target*15+count]==player&&
            trade_screen_.counts[29]==roster_database_.freeAgentCount()-1&&roster_database_.slotTable()==original,"sign draft mutation");
        const auto draft=tradeDraft()->slotTable();
        bool injected=false;
        reorder_save_hooks_={[](nba97::RosterSaveStage stage,void* p){if(stage==nba97::RosterSaveStage::PartialWrite){*static_cast<bool*>(p)=true;throw std::runtime_error("injected Sign save failure");}},&injected};
        handleTradeKey(VK_RETURN);capture("save-failed");
        require(injected&&isSign()&&roster_database_.slotTable()==original&&tradeDraft()->slotTable()==draft,"failed Sign save changed state");
        dismiss();reorder_save_hooks_={};handleTradeKey(VK_RETURN);settle();
        require(frontend_page_==nba97::FrontendPage::Rosters&&roster_database_.slotTable()==draft,"Sign accept publish");
        auto restart=roster_database_.prepareSlotTable(original);nba97::RosterSaveStore reopened(dir/"rosters.n97rst");
        reopened.load(restart);require(restart.slotTable()==draft&&restart.rosterOwner(player)==target,"Sign restart ownership");
        frontend_page_=nba97::FrontendPage::SignFreeAgent;openTrade();capture("restart");
        require(trade_screen_.team[1]==3,"accepted Sign re-entry uses remembered left29, not receiver");
        handleTradeKey(VK_LEFT);settle();
        handleTradeKey('C');settle();
        for(unsigned i=0;i<14;++i)handleTradeKey(VK_DOWN);
        handleTradeKey('C');capture("second-signing");handleTradeKey('X');capture("discard");
        require(trade_choice_address_==0x800af4f8,"Sign discard prompt");
        handleTradeKey(VK_UP);settle();handleTradeKey('C');settle();
        require(frontend_page_==nba97::FrontendPage::Rosters&&roster_database_.slotTable()==draft,"Sign discard persisted draft");
        const auto expected=roster_database_.originalSlots();commitRosterReset();
        require(roster_database_.slotTable()==expected,"Reset failed to restore free agents");
        auto resetRestart=restart; nba97::RosterSaveStore resetStore(dir/"rosters.n97rst");resetStore.load(resetRestart);
        require(resetRestart.slotTable()==expected,"Reset restart mismatch");
        // Independent reproduction of the manually observed Sign/View/Cancel
        // quirk. Save retention must survive a fresh store, not just the UI.
        sign_team_=2;sign_cursors_={0,13};sign_tops_={0,9};
        frontend_page_=nba97::FrontendPage::SignFreeAgent;openTrade();
        handleTradeKey('C');handleTradeKey('C');capture("quirk-signed");
        const auto retained=tradeDraft()->slotTable();
        handleTradeKey('D');capture("quirk-view-alston");
        handleTradeKey(VK_RETURN);capture("quirk-view-return");
        require(trade_screen_.team[1]==3&&trade_screen_.cursor[1]==13&&trade_screen_.top[1]==9&&
            trade_screen_.phase==NBA97_TRADE_FIRST&&!trade_screen_.changes&&
            nba97_trade_dirty(&trade_screen_)&&!nba97_trade_undo_dirty(&trade_screen_),"Sign constructor undo/receiver quirk");
        handleTradeKey('X');settle();
        require(frontend_page_==nba97::FrontendPage::Rosters&&!trade_choice_address_&&
            roster_database_.slotTable()==retained,"Sign pre-View change not retained on Cancel");
        auto retainedRestart=roster_database_.prepareSlotTable(expected);
        nba97::RosterSaveStore retainedStore(dir/"rosters.n97rst");retainedStore.load(retainedRestart);
        require(retainedRestart.slotTable()==retained&&retainedRestart.rosterOwner(player)==2,"Sign quirk retained only in memory");
        frontend_page_=nba97::FrontendPage::SignFreeAgent;openTrade();capture("quirk-retained-restart");
        require(trade_screen_.team[1]==3&&trade_screen_.cursor[0]==0&&trade_screen_.cursor[1]==0&&
            trade_screen_.top[0]==0&&trade_screen_.top[1]==0,"Rosters menu must reset Sign row positions");
        handleTradeKey('X');settle();commitRosterReset();
        require(roster_database_.slotTable()==expected,"final isolated Reset");
        trace_.log("SIGN-HOST-VERIFY","PASS 26 checkpoints: original assets/Help/View/Compare/100-row tail/refusals/signing/save failure/restart/discard/Reset/retained-child-undo; isolated save only");
        return 0;
    }

    int captureTeamSelect() {
        const auto output=options_.team_select_capture_dir;
        if(std::filesystem::exists(output)) throw std::runtime_error("Team Select capture needs a fresh frame directory");
        std::filesystem::create_directories(output);
        const auto slots=roster_database_.slotTable();
        const auto identity=roster_database_.baseIdentity();
        const auto created=created_players_;
        settings_.adjustRule(3,1);const auto retained_rule=settings_.rule(3);
        frontend_page_=nba97::FrontendPage::GameSetup;frontend_transition_active_=false;menu_elapsed_ms_=0;
        std::ofstream states(output/"states.json");states<<"[\n";unsigned count=0;
        auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
        auto frame=[&](const char* name,const nba97::RosterDatabase::SlotTable* expected_roster=nullptr) {
            rebuildMenuFrame();
            PshImage pixels;pixels.width=static_cast<uint16_t>(menu_frame_.width);pixels.height=static_cast<uint16_t>(menu_frame_.height);
            pixels.rgba=menu_frame_.bgra;
            for(std::size_t i=0;i<pixels.rgba.size();i+=4) std::swap(pixels.rgba[i],pixels.rgba[i+2]);
            writePpm(pixels,output/(std::string(name)+".ppm"));
            unsigned text_visible=0,marker_visible=0;
            const auto& placement=user_setup_.placement();
            for(unsigned c=0;c<8;++c)
                if((placement.text_alive&(1u<<c)) && placement.text_x[c]<512) text_visible|=1u<<c;
            if(user_setup_.topology()<4)
                for(unsigned m=0;m<15;++m) if(placement.marker_x[m]<512) marker_visible|=1u<<m;
            if(count++) states<<",\n";
            states<<"{\"id\":\""<<name<<"\",\"page\":\""<<frontendPageName(frontend_page_)<<
                "\",\"home\":"<<team_select_.team[0]<<",\"away\":"<<team_select_.team[1]<<
                ",\"criterion\":"<<unsigned(team_select_.criterion)<<",\"side\":"<<unsigned(team_select_.side)<<
                ",\"help\":"<<int(team_select_help_.phase)<<",\"random_wait\":"<<unsigned(team_select_random_.wait)<<
                ",\"quarter\":"<<unsigned(menu_.setupChoice(0))<<",\"mode\":"<<unsigned(menu_.setupChoice(1))<<
                ",\"user_side\":"<<unsigned(user_setup_.state().side[0])<<
                ",\"user_profile\":"<<int(user_setup_.state().profile[0])<<
                ",\"assignment\":"<<unsigned(user_setup_.state().assignment[0])<<
                ",\"user_help\":"<<unsigned(user_setup_.help().phase)<<
                ",\"user_topology\":"<<user_setup_.topology()<<
                ",\"user_text_visible\":"<<text_visible<<",\"user_marker_visible\":"<<marker_visible<<
                ",\"editor\":"<<int(user_setup_.state().alphabet[0])<<
                ",\"cursor\":"<<unsigned(user_setup_.state().cursor[0])<<
                ",\"draft\":"<<std::quoted(user_setup_.state().draft[0])<<
                ",\"dialog\":"<<unsigned(user_setup_.dialogKind())<<
                ",\"dialog_phase\":"<<unsigned(user_setup_.dialogState().modal.phase)<<
                ",\"dialog_choice\":"<<unsigned(user_setup_.dialogState().choice)<<
                ",\"profile_count\":"<<profile_store_.profiles().size()<<
                ",\"profile_generation\":"<<profile_store_.generation()<<
                ",\"match_revision\":"<<match_session_.revision()<<
                ",\"poll_phase\":"<<unsigned(team_select_poll_.phase)<<
                ",\"poll_wait\":"<<team_select_poll_.post_remaining<<
                ",\"poll_repeat\":"<<team_select_poll_.pad.repeat_counter<<
                ",\"raw_mask\":"<<team_select_held_<<
                ",\"topology_countdown\":"<<user_setup_.topologyCountdown()<<
                ",\"user_prior_mask\":"<<user_setup_.priorMask()<<
                ",\"user_prior_controller\":"<<unsigned(user_setup_.priorController())<<"}";
            require(roster_database_.slotTable()==(expected_roster ? *expected_roster:slots) && roster_database_.baseIdentity()==identity,
                "Team Select mutated accepted roster/identity");
            require(settings_.rule(3)==retained_rule && !std::memcmp(&created,&created_players_,sizeof(created)),
                "Team Select lost settings/created catalogue");
        };
        auto ticks=[&](unsigned n) {for(unsigned i=0;i<n && frontend_page_==nba97::FrontendPage::TeamSelect;++i) {
            menu_elapsed_ms_=static_cast<uint32_t>(((team_select_tick_+1)*1001+29)/30);updateTeamSelect();
        }rebuildMenuFrame();};
        auto pollReady=[&] {
            releaseFrontendPadKeys();
            for(unsigned i=0;i<8 && team_select_poll_.phase==NBA97_TEAM_POST_WAIT;++i)ticks(1);
            if(team_select_poll_.phase==NBA97_TEAM_SETTLE || team_select_poll_.phase==NBA97_TEAM_POLL)ticks(1);
        };
        auto setupTicks=[&](unsigned n) {for(unsigned i=0;i<n && setup_start_pending_;++i) {
            menu_elapsed_ms_=static_cast<uint32_t>(((setup_start_tick_+1)*1001+29)/30);updateSetupStart();
        }rebuildMenuFrame();};
        auto key=[&](WPARAM k) {
            if(frontend_page_==nba97::FrontendPage::TeamSelect) {
                if(team_select_help_.phase==NBA97_HELP_CLOSED && !nba97_team_random_busy(&team_select_random_) && !team_select_exit_wait_)
                    pollReady();
                handleMenuKey(k);ticks(1);releaseFrontendPadKeys();
                if(team_select_exit_wait_)ticks(2);
            } else {handleMenuKey(k);releaseFrontendPadKeys();}
            if(setup_start_pending_)setupTicks(2);
            frontend_transition_active_=false;
        };
        frame("setup");key('D');require(menu_.setupChoice(0)==3,"quarter previous wrap");frame("setup-quarter-wrap");
        key('C');require(menu_.setupChoice(0)==0,"quarter next wrap");
        key(VK_RIGHT);key('C');key(VK_RETURN);
        require(frontend_page_==nba97::FrontendPage::GameSetup && menu_.setupChoice(1)==1,
            "season must not enter exhibition Team Select");frame("setup-mode-pending");
        key('D');key(VK_LEFT);
        handleMenuKey(VK_RETURN);setupTicks(3);
        const auto pending_focus=menu_.selection();const auto pending_row=menu_.row();
        const auto pending_mouse_x=last_mouse_x_,pending_mouse_y=last_mouse_y_;
        const std::array<int,4> pending_choices{menu_.setupChoice(0),menu_.setupChoice(1),menu_.setupChoice(2),menu_.setupChoice(3)};
        handleMessage(WM_MOUSEMOVE,0,MAKELPARAM(600,60));handleMessage(WM_LBUTTONDOWN,0,MAKELPARAM(600,60));
        activateMenuSelection();adjustSetupChoice(1); // Direct adapters must respect the same modal boundary.
        for(unsigned i=0;i<4;++i)require(menu_.setupChoice(i)==pending_choices[i],"Setup Start wait freezes all card choices");
        require(menu_.selection()==pending_focus && menu_.row()==pending_row &&
            last_mouse_x_==pending_mouse_x && last_mouse_y_==pending_mouse_y,"Setup Start wait ignores mouse hover/focus");
        require(frontend_page_==nba97::FrontendPage::GameSetup && setup_start_pending_ &&
            team_select_poll_.phase==NBA97_TEAM_EXIT_CHANGE,"Setup held Start stays behind source change barrier");frame("setup-held-start");
        releaseFrontendPadKeys();setupTicks(2);frontend_transition_active_=false;
        require(frontend_page_==nba97::FrontendPage::TeamSelect,"actual Setup Start route failed");
        require(team_select_.team[0]==3 && team_select_.team[1]==24,"retail first-boot defaults");frame("entry");
        uint16_t scores[5][29];team_select_assets_->ranks(roster_database_,scores);
        std::ofstream numbers(output/"rank_cache.json");numbers<<"{\"scores\":[";
        for(unsigned c=0;c<5;++c) {if(c)numbers<<',';numbers<<'[';for(unsigned t=0;t<29;++t) {if(t)numbers<<',';numbers<<scores[c][t];}numbers<<']';}
        numbers<<"],\"ranks\":[";
        for(unsigned c=0;c<5;++c) {if(c)numbers<<',';numbers<<'[';for(unsigned t=0;t<31;++t) {if(t)numbers<<',';numbers<<unsigned(team_select_ranks_.value[c][t]);}numbers<<']';}numbers<<"]}";
        // Exercise the C++ adapter with an accepted/reopened isolated save.
        // The original score fixture remains separate from these native outputs.
        auto modified_slots=slots;std::swap(modified_slots[3*15],modified_slots[3*15+8]);
        auto modified=roster_database_.prepareSlotTable(slots);
        nba97::RosterSaveStore isolated(output.parent_path()/"propagation.n97rst");isolated.load(modified);
        require(isolated.commit(modified,modified_slots).changed,"isolated roster commit");
        auto reopened=roster_database_.prepareSlotTable(slots);
        nba97::RosterSaveStore restart(output.parent_path()/"propagation.n97rst");restart.load(reopened);
        require(reopened.slotTable()==modified_slots && reopened.baseIdentity()==identity,"saved roster restart/identity");
        uint16_t modified_scores[5][29];const auto modified_ranks=team_select_assets_->ranks(reopened,modified_scores);
        require(std::memcmp(scores,modified_scores,sizeof(scores))!=0,"ratings adapter ignored accepted modified slots");
        std::ofstream modified_numbers(output/"modified_rank_cache.json");modified_numbers<<"{\"scores\":[";
        for(unsigned c=0;c<5;++c) {if(c)modified_numbers<<',';modified_numbers<<'[';for(unsigned t=0;t<29;++t) {if(t)modified_numbers<<',';modified_numbers<<modified_scores[c][t];}modified_numbers<<']';}
        modified_numbers<<"],\"ranks\":[";
        for(unsigned c=0;c<5;++c) {if(c)modified_numbers<<',';modified_numbers<<'[';for(unsigned t=0;t<31;++t) {if(t)modified_numbers<<',';modified_numbers<<unsigned(modified_ranks.value[c][t]);}modified_numbers<<']';}modified_numbers<<"]}";
        frontend_transition_active_=true;handleMenuKey(VK_LEFT);
        handleMessage(WM_KEYDOWN,VK_LEFT,static_cast<LPARAM>(1u<<30)); // Repeated message still updates raw state.
        require(team_select_.team[0]==3 && team_select_held_==8,"fade/repeated keydown must capture without dispatch");
        frame("left-before-poll");frontend_transition_active_=false;ticks(1);
        require(team_select_.team[0]==2 && team_select_.team[1]==24 && team_select_poll_.post_remaining==7,
            "first home Left sampled after presentation");frame("home-left");
        const auto before_wait=team_select_presentations_;ticks(7);
        require(team_select_.team[0]==2 && team_select_poll_.phase==NBA97_TEAM_SETTLE,"Left seven-presentation postwait");frame("left-post-wait");
        ticks(1);require(team_select_.team[0]==1 && team_select_poll_.pad.repeat_counter==2 &&
            team_select_presentations_-before_wait==8,"held Left repeats only after7+1");frame("left-held-repeat");
        releaseFrontendPadKeys();key(VK_RIGHT);key(VK_RIGHT);
        require(team_select_.team[0]==3,"home right ID scan");frame("home-right");
        key(VK_UP);require(team_select_.criterion==5,"criterion up wrap");frame("criterion-up-wrap");
        key(VK_DOWN);require(team_select_.criterion==0,"criterion down wrap");frame("criterion-down-wrap");
        key('C');require(team_select_.side==1,"Cross switches active side");frame("away-active");
        key(VK_RIGHT);require(team_select_.team[1]==25,"away scan");frame("away-right");
        key(VK_DOWN);key(VK_RIGHT);frame("away-scoring-next");
        ticks(20);frame("selector-gold");
        const auto before_help=team_select_;key('F');ticks(24);frame("help");
        require(team_select_help_.phase==NBA97_HELP_READY,"Help did not reach ready");
        key('C');ticks(24);require(team_select_help_.phase==NBA97_HELP_CLOSED,"Help did not close");
        require(before_help.team[0]==team_select_.team[0] && before_help.team[1]==team_select_.team[1] &&
            before_help.side==team_select_.side && before_help.criterion==team_select_.criterion && !team_select_.result,
            "Help changed selected team/state");frame("help-return");
        key('F');handleMenuKey('C');ticks(14);
        require(team_select_help_.phase==NBA97_HELP_SHRINKING,"changed held key must dismiss Help after growth");
        releaseFrontendPadKeys();ticks(24);require(team_select_help_.phase==NBA97_HELP_CLOSED,"early Help dismissal return barrier");
        frame("help-early-close");
        key('D');require(team_select_.team[0]==before_help.team[0] && team_select_.team[1]==before_help.team[1],"invalid Square changed teams");frame("invalid-square");
        pollReady();handleMenuKey('X');handleMenuKey(VK_RETURN);ticks(1);
        require(team_select_held_==0x2080 && !team_select_.result && team_select_poll_.post_remaining==5,
            "Start+R2 is an ignored whole action chord with caller5");frame("team-shoulder-chord");
        releaseFrontendPadKeys();pollReady();handleMenuKey('C');handleMenuKey(VK_SPACE);
        handleMessage(WM_KEYUP,'C',0);require(team_select_held_==0x800,"alias keyup cannot clear held Space");
        handleMessage(WM_KILLFOCUS,0,0);require(!team_select_held_,"focus loss clears raw keys");
        const auto saved=team_select_;pollReady();handleMenuKey(VK_RSHIFT);ticks(2);
        require(team_select_exit_wait_ && team_select_poll_.phase==NBA97_TEAM_EXIT_CHANGE &&
            frontend_page_==nba97::FrontendPage::TeamSelect,"held Select waits on initiating mask");frame("select-held-exit");
        releaseFrontendPadKeys();ticks(1);
        require(team_select_poll_.phase==NBA97_TEAM_EXIT_FINAL && frontend_page_==nba97::FrontendPage::TeamSelect,
            "Select change precedes separate cleanup");frame("select-cleanup");
        ticks(1);frontend_transition_active_=false;
        require(frontend_page_==nba97::FrontendPage::GameSetup,"Select did not return to Setup");frame("setup-return");
        key(VK_RETURN);require(team_select_.team[0]==saved.team[0] && team_select_.team[1]==saved.team[1] &&
            team_select_.side==saved.side && team_select_.criterion==saved.criterion,"reentry lost focus/teams");frame("reentry");
        handleMenuKey(VK_RETURN);
        require(!team_select_exit_wait_,"keydown cannot bypass initial Start poll");ticks(1);
        require(frontend_page_==nba97::FrontendPage::TeamSelect && team_select_exit_wait_,
            "Start must wait for changed input before state5");
        ticks(1);require(frontend_page_==nba97::FrontendPage::TeamSelect,"held Start bypassed exit barrier");frame("start-held-exit");
        releaseFrontendPadKeys();ticks(1);
        require(team_select_poll_.phase==NBA97_TEAM_EXIT_FINAL,"Start changed input beforecleanup");frame("start-cleanup");
        ticks(1);frontend_transition_active_=false;
        require(frontend_page_==nba97::FrontendPage::UserSetup,"state5 route did not enter original User Setup");frame("user-setup-entry");
        auto userTicks=[&](unsigned n) {for(unsigned i=0;i<n && frontend_page_==nba97::FrontendPage::UserSetup;++i) {
            menu_elapsed_ms_=static_cast<uint32_t>(((user_setup_tick_+1)*1001+29)/30);updateUserSetup();
        }rebuildMenuFrame();};
        auto userKey=[&](WPARAM k) {
            handleMenuKey(k);userTicks(2);releaseFrontendPadKeys();user_setup_.releaseKeys();userTicks(2);frontend_transition_active_=false;
        };
        userKey(VK_RIGHT);require(user_setup_.state().side[0]==2,"controller0 home assignment");frame("user-home");
        userKey(VK_LEFT);require(user_setup_.state().side[0]==1,"controller0 neutral assignment");frame("user-neutral");
        userKey(VK_LEFT);require(user_setup_.state().side[0]==0,"controller0 away assignment");frame("user-away");
        userKey(VK_LEFT);require(user_setup_.state().side[0]==0,"away endpoint clamp");frame("user-away-boundary");
        userKey(VK_UP);require(user_setup_.state().profile[0]==-1,"Start New profile cycle");frame("user-new-profile");
        userKey(VK_RETURN);require(user_setup_.state().result==0 && user_setup_.state().assignment[0]==0,
            "unresolved active Start New must refuse readiness");frame("user-readiness-refused");
        require(!match_session_.snapshot() && !match_session_.revision(),"refused readiness captured a match");
        userKey('C');require(user_setup_.state().profile[0]==0 && user_setup_.state().alphabet[0]==0 &&
            !std::strcmp(user_setup_.state().draft[0],"A") && profile_store_.profiles().empty(),
            "editor claims first empty slot without saving");frame("user-editor-new");
        userKey('V');userKey('D');userKey(VK_UP);userKey('C');userKey(VK_DOWN);
        require(!std::strcmp(user_setup_.state().draft[0],"BA") && user_setup_.state().cursor[0]==1,
            "Cross duplicates current character, Down changes appended character");frame("user-editor-append");
        userKey(VK_LEFT);userKey(VK_UP);userKey(VK_RIGHT);userKey('D');
        require(!std::strcmp(user_setup_.state().draft[0],"C") && user_setup_.state().cursor[0]==0,
            "cursor navigation and Square delete");frame("user-editor-delete-char");
        userKey('F');userTicks(20);
        require(user_setup_.helpIndex()==1 && user_setup_.help().phase==NBA97_HELP_READY,"editing Help descriptor");
        frame("user-editor-help");userKey('C');userTicks(20);
        require(user_setup_.help().phase==NBA97_HELP_CLOSED && !std::strcmp(user_setup_.state().draft[0],"C"),
            "editing Help retains draft");frame("user-editor-help-return");
        userKey(VK_DOWN); // C -> B, exact case preserved by the save adapter.
        auto fail_path=options_.profiles_path;fail_path+=L".tmp";
        require(std::filesystem::create_directory(fail_path),"fresh isolated temporary write-failure fixture");
        userKey(VK_RETURN);userTicks(20);
        require(user_setup_.dialogKind()==nba97::UserSetupDialog::SaveFailure &&
            profile_store_.profiles().empty() && !profile_store_.generation() && user_setup_.state().alphabet[0]>=0,
            "failed write must retain draft and refuse acceptance");frame("user-save-failure");
        userKey('C');userTicks(20);
        require(std::filesystem::remove(fail_path),"remove only the empty isolated failure fixture");
        user_setup_.setControllers(0,0x11);user_setup_.key(4,8,true);
        handleMenuKey(VK_RETURN);user_setup_tick_+=2;userTicks(1); // One due row pass after a host stall.
        require(user_setup_.state().side[4]==0 && user_setup_.placement().marker_x[1]==50 &&
            !user_setup_.hasPendingRowTail(),"successful inline save must finish later controller before presentation");
        trace_.log("USER-INLINE-SAVE-PASS","PASS: physical4 Left and placement completed in the same host update as physical0 save");
        releaseFrontendPadKeys();user_setup_.releaseKeys();user_setup_.setControllers(0,1);userTicks(2);
        require(profile_store_.profiles().size()==1 && profile_store_.atSlot(0)->name=="B" &&
            user_setup_.state().alphabet[0]==-1 && !user_setup_.state().assignment[0],
            "retry accepts durable name without accepting match");frame("user-save-new");
        const auto first_id=profile_store_.atSlot(0)->id;
        const auto first_created=profile_store_.atSlot(0)->created_unix_seconds;
        userKey('C');userKey(VK_UP);userKey(VK_RETURN);
        require(profile_store_.atSlot(0)->name=="C" && profile_store_.atSlot(0)->id==first_id &&
            profile_store_.atSlot(0)->created_unix_seconds==first_created,"existing rename preserves identity/time");
        frame("user-save-rename");
        userKey('C');userKey(VK_RETURN);
        require(profile_store_.generation()==2,"no-op name must not write");frame("user-save-noop");
        userKey(VK_DOWN);userKey('C');userKey(VK_UP);userKey(VK_UP);userKey(VK_RETURN);userTicks(20);
        require(user_setup_.dialogKind()==nba97::UserSetupDialog::Duplicate &&
            user_setup_.state().profile[0]==1 && profile_store_.generation()==2,
            "duplicate exact name opens original warning without saving");frame("user-name-duplicate");
        userKey('C');userTicks(20);userKey(VK_DOWN);userKey(VK_RETURN);
        require(profile_store_.atSlot(1)->name=="B" && profile_store_.generation()==3,"second profile accepted");
        frame("user-save-second");
        userKey('D');userTicks(20);
        require(user_setup_.dialogKind()==nba97::UserSetupDialog::Delete &&
            user_setup_.dialogState().choice==1,"Delete uses source preference cancel");
        frame("user-delete-cancel-choice");
        userKey(VK_RETURN);userTicks(8);userKey(VK_RSHIFT);userTicks(8);
        require(user_setup_.dialogKind()==nba97::UserSetupDialog::Delete && profile_store_.generation()==3,
            "Start/Select cannot choose or cancel delete");frame("user-delete-invalid");
        userKey('C');userTicks(30);
        require(user_setup_.dialogKind()==nba97::UserSetupDialog::None && profile_store_.atSlot(1) &&
            profile_store_.generation()==3,"cancel delete must not write");frame("user-delete-cancelled");
        userKey('D');userTicks(20);userKey(VK_UP);userTicks(8);
        require(user_setup_.dialogState().choice==0,"delete choice up");
        handleMenuKey('C');userTicks(1);
        require(user_setup_.dialogState().confirm_pending && profile_store_.atSlot(1),
            "delete Cross starts delayed confirmation");frame("user-delete-delay");
        userTicks(7);
        require(nba97_help_text_visible(&user_setup_.dialogState().modal) && profile_store_.generation()==3,
            "choice text and saved profile persist through seven delayed updates");
        userTicks(1);require(user_setup_.dialogState().modal.phase==NBA97_HELP_SHRINKING &&
            profile_store_.atSlot(1),"confirmation shrinks before deletion");
        userTicks(30);
        require(user_setup_.dialogState().modal.phase==NBA97_HELP_RETURN_BARRIER && profile_store_.atSlot(1),
            "held Cross keeps deletion behind return barrier");frame("user-delete-barrier");
        releaseFrontendPadKeys();user_setup_.releaseKeys();userTicks(2);
        require(!profile_store_.atSlot(1) && profile_store_.atSlot(0)->id==first_id &&
            profile_store_.generation()==4 && user_setup_.state().profile[0]==-2,"delete exact slot and retain other IDs");
        frame("user-delete-accepted");
        nba97::UserProfileStore restarted;restarted.load(options_.profiles_path);
        require(restarted.atSlot(0) && restarted.atSlot(0)->id==first_id && !restarted.atSlot(1),
            "accepted profile and deletion hole survive restart");
        userKey(VK_UP);userKey('C');userKey(VK_UP);handleMenuKey(VK_RSHIFT);userTicks(2);
        require(frontend_page_==nba97::FrontendPage::UserSetup && user_setup_.state().result==-1 &&
            user_setup_.priorController()==8 && user_setup_.priorMask()==0x100,
            "state5 Select waits on aggregate100 after clearing editor");frame("user-held-cancel");
        releaseFrontendPadKeys();user_setup_.releaseKeys();userTicks(1);frontend_transition_active_=false;
        require(frontend_page_==nba97::FrontendPage::TeamSelect && profile_store_.generation()==4,
            "Select abandons editing draft without saving");frame("user-editor-abandon");
        key(VK_RETURN);require(user_setup_.state().profile[0]==-2 && user_setup_.names().name[0][0]=='C',
            "reentry retains accepted profile only");
        userKey(VK_LEFT); // Abandoned pre-confirmation assignment was neutral.
        userKey(VK_RETURN);require(user_setup_.state().assignment[0]==2,"state5 acceptance maps away to2");frame("match-handoff-pending");
        require(match_session_.revision()==1 && match_session_.snapshot() &&
            match_session_.snapshot()->accepted_slots==slots && match_session_.snapshot()->request.teams[0]==team_select_.team[0] &&
            match_session_.snapshot()->rules==settings_.effectiveRules(),"actual result6 captures accepted roster/settings");
        std::ofstream(output/"match_snapshot.json")<<nba97::matchSnapshotReceipt(*match_session_.snapshot());
        userKey('F');userTicks(20);
        require(user_setup_.help().phase==NBA97_HELP_READY,"User Help ready");frame("user-help");
        userKey('C');userTicks(20);
        require(user_setup_.help().phase==NBA97_HELP_CLOSED && user_setup_.state().side[0]==0 &&
            user_setup_.state().profile[0]==-2,"User Help changed assignment/profile");frame("user-help-return");
        handleMenuKey(VK_RIGHT);handleMenuKey(VK_RETURN);userTicks(2);
        require(user_setup_.state().side[0]==0 && user_setup_.state().assignment[0]==2,"raw chord changed state");
        releaseFrontendPadKeys();user_setup_.releaseKeys();userTicks(2);frame("user-invalid-chord");
        for(WPARAM shoulder:{WPARAM('A'),WPARAM('Z'),WPARAM('S'),WPARAM('X')}) {
            handleMenuKey(shoulder);handleMenuKey(VK_RETURN);userTicks(2);
            require(!user_setup_.state().start_latch && user_setup_.state().assignment[0]==2,
                "Start plus any shoulder is not exact Start");
            releaseFrontendPadKeys();user_setup_.releaseKeys();userTicks(2);
        }
        frame("user-shoulder-chords");
        user_setup_.setControllers(3,0xff);userTicks(4);
        require(user_setup_.topology()==0 && user_setup_.topologyCountdown()==-1,"four topology observations retain old layout");frame("user-topology-3-pending");
        userTicks(1);require(user_setup_.topology()==3,"fifth topology observation adopts8rows");frame("user-eight-controllers");
        user_setup_.setControllers(2,0xf1);userTicks(4);
        require(user_setup_.topology()==3 && user_setup_.topologyCountdown()==-1,"second topology debounce");frame("user-topology-2-pending");
        userTicks(1);require(user_setup_.topology()==2,"fifth adopts port2tap");frame("user-port2-multitap");
        user_setup_.setControllers(0,1);userTicks(5);
        userKey(VK_RIGHT);userKey(VK_RIGHT);
        require(user_setup_.state().side[0]==2 && user_setup_.state().assignment[0]==2,"local draft must not publish");frame("user-unaccepted-home");
        userKey(VK_RSHIFT);
        require(frontend_page_==nba97::FrontendPage::TeamSelect && team_select_.team[0]==saved.team[0] &&
            team_select_.team[1]==saved.team[1] && team_select_.side==saved.side && team_select_.criterion==saved.criterion,
            "state5 cancellation lost Team Select state");frame("user-setup-return");
        key(VK_RETURN);require(frontend_page_==nba97::FrontendPage::UserSetup &&
            user_setup_.state().side[0]==0 && user_setup_.state().profile[0]==-2,
            "User Setup reentry lost committed assignment or retained cancelled profile");frame("user-reentry");
        userKey(VK_RSHIFT);require(frontend_page_==nba97::FrontendPage::TeamSelect,"User Setup second cancel");
        key('V');const auto random_begin=team_select_presentations_;ticks(66);
        require(team_select_random_.remaining==0 && team_select_random_.wait==12,"last candidate wait boundary");
        const auto last_random=team_select_;handleMenuKey(VK_RSHIFT);handleMenuKey('C');
        require(frontend_page_==nba97::FrontendPage::TeamSelect && team_select_.side==last_random.side,
            "random callback accepted input during final wait");frame("random-last-wait");
        updateFrontendPadKey(VK_RSHIFT,false);ticks(12);
        require(!nba97_team_random_busy(&team_select_random_) && team_select_poll_.post_remaining==5 &&
            team_select_.side==last_random.side,"random owner78 then caller5");frame("random-caller-wait");
        ticks(5);require(team_select_.side==last_random.side,"no raw sampling inside caller wait");
        ticks(1);require(team_select_.side==(last_random.side^1) && team_select_presentations_-random_begin==84,
            "Cross held in random dispatches at next poll84");frame("random-held-cross");
        releaseFrontendPadKeys();key('C');
        require(team_select_.side==last_random.side && team_select_.team[1]<29,"random regular-team domain");frame("random-complete");
        // Fresh host fixtures exercise rare modal routes without touching saves
        // outside the capture's already validated isolated run directory.
        frontend_page_=nba97::FrontendPage::UserSetup;user_setup_=nba97::UserSetupSession{};
        const auto userClock=static_cast<int32_t>(uint64_t(menu_elapsed_ms_)*120/1000);
        user_setup_.open({0,1,1,1,1,1,0,0},profile_store_.profiles(),userClock);
        user_setup_tick_=uint64_t(menu_elapsed_ms_)*30/1001;
        user_setup_.setControllers(1,1);userKey(VK_RIGHT);userTicks(20);
        require(user_setup_.dialogKind()==nba97::UserSetupDialog::Capacity &&
            user_setup_.state().side[0]==1,"sixth player opens capacity warning");frame("user-capacity-warning");
        userKey('C');userTicks(20);
        for(unsigned slot=1;slot<20;++slot)
            require(profile_store_.acceptExact(uint8_t(slot),0,"Fixture"+std::to_string(slot),true),"isolated full catalogue");
        user_setup_=nba97::UserSetupSession{};
        user_setup_.open({},profile_store_.profiles(),static_cast<int32_t>(uint64_t(menu_elapsed_ms_)*120/1000));
        user_setup_.configureEditor(user_setup_assets_->alphabet(),[&](const char* name){return menu_font_.textWidth(name);});
        userKey(VK_LEFT);userKey(VK_UP);userKey('C');userTicks(20);
        require(user_setup_.dialogKind()==nba97::UserSetupDialog::Full && user_setup_.state().profile[0]==-1 &&
            profile_store_.profiles().size()==20,"full catalogue cannot claim a draft slot");frame("user-full-warning");
        userKey('C');userTicks(20);
        userKey(VK_UP); // From unresolved FF to the first saved fixed slot.
        require(user_setup_.state().profile[0]==0,"full catalogue saved selector");
        userKey(VK_RETURN);
        require(match_session_.revision()==2 && match_session_.snapshot()->controls.profile_ids[0]==first_id &&
            match_session_.snapshot()->controls.provenance[0]==NBA97_CONTROLS_DEFAULT,
            "saved profile with disabled controls selects defaults");frame("match-profile-snapshot");
        const auto retained_maps=match_session_.liveControls();
        userKey(VK_DOWN);userKey(VK_DOWN);userKey(VK_RETURN);
        require(match_session_.revision()==3 && match_session_.snapshot()->controls.provenance[0]==NBA97_CONTROLS_RETAINED &&
            !std::memcmp(retained_maps.map,match_session_.liveControls().map,sizeof(retained_maps.map)),
            "FE handoff retains live controller maps");frame("match-no-profile-retains");
        userKey(VK_RSHIFT);
        require(match_session_.revision()==3,"cancel changed frozen match");frame("match-cancel-preserved");
        for(unsigned i=0;i<31 && team_select_.team[team_select_.side]!=29;++i)key(VK_RIGHT);
        require(team_select_.team[team_select_.side]==29,"special-team source navigation");
        key(VK_RETURN);userKey(VK_RETURN);
        require(match_session_.revision()==3,"unsupported special team replaced prior snapshot");frame("match-special-pending");
        userKey(VK_RSHIFT);key(VK_RIGHT);key(VK_RIGHT); // special29 ->30 -> next regular rank
        auto prior_store=std::move(roster_store_);
        roster_database_.swap(reopened);
        roster_store_=std::make_unique<nba97::RosterSaveStore>(output.parent_path()/"propagation.n97rst");
        roster_store_->load(roster_database_);
        key(VK_RETURN);userKey(VK_RETURN);
        require(match_session_.revision()==4 && match_session_.snapshot()->accepted_slots==modified_slots &&
            match_session_.snapshot()->roster_generation==1 &&
            !std::memcmp(&match_session_.snapshot()->ranks,&modified_ranks,sizeof(modified_ranks)),
            "host handoff must use accepted reopened roster and fresh ranks");
        frame("match-modified-roster",&modified_slots);
        std::ofstream(output/"match_modified_snapshot.json")<<nba97::matchSnapshotReceipt(*match_session_.snapshot());
        roster_database_.swap(reopened);roster_store_=std::move(prior_store);
        // Controlled source-clock placement fixtures, not physical input or
        // original-frame timing evidence. No profile/store writes are needed.
        user_setup_=nba97::UserSetupSession{};
        user_setup_.open({1,0,0,0,0,0,0,0},{},0,0x20);
        user_setup_.setControllers(0,0x11);user_setup_.primeEntryTopology();user_setup_.step(0);
        frame("user-placement-entry-hidden");
        user_setup_.step(7);frame("user-placement-first-rows");
        // Fixed original assets/palette/title, varying only retained targets.
        // This tests the native adapter, not original framebuffer equivalence.
        auto drawPlacement=[&](const Nba97UserPlacement& placement) {
            return nba97::renderUserSetup(user_setup_.state(),user_setup_.names(),0,placement,
                team_select_.team[0],team_select_.team[1],*user_setup_assets_,*team_select_assets_,
                team_select_sprites_,menu_font_,team_select_palette_,nullptr,nullptr,false);
        };
        auto hidden=user_setup_.placement();
        nba97_user_setup_placement_rebuild(&hidden,0);
        const auto empty_pixels=drawPlacement(hidden);
        auto label=hidden;label.text_x[0]=256;label.text_y[0]=88;
        auto marker=hidden;marker.marker_x[0]=318;marker.marker_y[0]=73;
        auto changedWithin=[&](const PshImage& image,int left,int top,int right,int bottom) {
            unsigned changed=0;
            for(int y=0;y<240;++y)for(int x=0;x<512;++x) {
                const auto at=std::size_t(y*512+x)*4;
                if(!std::equal(image.rgba.begin()+at,image.rgba.begin()+at+4,empty_pixels.rgba.begin()+at)) {
                    require(x>=left && x<right && y>=top && y<bottom,"retained placement drew outside original asset region");
                    ++changed;
                }
            }
            require(changed>0,"retained placement did not draw original asset");
        };
        changedWithin(drawPlacement(label),180,70,312,120);
        const auto& marker_sprite=team_select_sprites_.at(user_setup_assets_->layout()[18].tag);
        changedWithin(drawPlacement(marker),318,73,318+marker_sprite.width,73+marker_sprite.height);
        trace_.log("USER-PLACEMENT-PIXELS","PASS: original label and marker independently follow retained targets with fixed palette/title");
        user_setup_.setControllers(0,0x10);user_setup_.step(13);frame("user-disconnect-before-row");
        user_setup_.step(14);frame("user-disconnect-after-row");
        user_setup_.setControllers(0,0x11);user_setup_.step(20);frame("user-reconnect-before-row");
        user_setup_.step(21);frame("user-reconnect-after-row");
        user_setup_.key(0,4,true);user_setup_.step(28);user_setup_.releaseKeys();user_setup_.step(35);
        user_setup_.setControllers(3,0xff);
        for(unsigned i=0;i<5;++i)user_setup_.step(36);
        frame("user-rebuild-clock-closed");
        user_setup_.key(0,0x20,true);
        const auto help_action=user_setup_.step(42);
        require(help_action.size()==1 && help_action[0].event==NBA97_USER_HELP && user_setup_.hasPendingRowTail(),
            "rebuilt Help must suspend before current row placement");
        user_setup_.openHelp(user_setup_assets_->help().descriptor(5,0).rect,0);
        for(unsigned i=0;i<20;++i)user_setup_.tickHelp();
        frame("user-rebuild-help-before-tail");
        user_setup_.releaseKeys();user_setup_.tickHelp();user_setup_.key(0,0x800,true);
        for(unsigned i=0;i<30;++i)user_setup_.tickHelp();
        user_setup_.releaseKeys();user_setup_.tickHelp();
        require(user_setup_.help().phase==NBA97_HELP_CLOSED,"placement fixture Help return");
        user_setup_.setControllers(3,0xfe);user_setup_.step(1000);
        frame("user-rebuild-help-resumed");
        user_setup_.step(1001);frame("user-rebuild-next-pass");
        user_setup_=nba97::UserSetupSession{};user_setup_.open({}, {},0);
        user_setup_.configureEditor(user_setup_assets_->alphabet(),[&](const char* name){return menu_font_.textWidth(name);});
        user_setup_.setControllers(0,1);user_setup_.step(0);
        for(const auto input:std::array<std::pair<uint16_t,int32_t>,3>{{{4,7},{1,14},{0x800,21}}}) {
            user_setup_.key(0,input.first,true);user_setup_.step(input.second);user_setup_.releaseKeys();
        }
        user_setup_.setControllers(2,0xf1);
        for(unsigned i=0;i<5;++i)user_setup_.step(22);
        frame("user-editor-rebuild-hidden");
        user_setup_.step(28);frame("user-editor-rebuild-restored");
        states<<"\n]\n";
        trace_.log("TEAM-CAPTURE","PASS: "+std::to_string(count)+" original-asset native frames; host Team Select/User Setup/editor/modal/transaction/restart scenarios; match handoff pending; no real saves");
        return 0;
    }

    int captureTrade() {
        const auto dir=std::filesystem::weakly_canonical(options_.trade_capture_dir);
        const auto root=std::filesystem::weakly_canonical(".local/verification");
        const auto relative=dir.lexically_relative(root);
        if(relative.empty() || relative=="." || *relative.begin()==".." || std::filesystem::exists(dir))
            throw std::runtime_error("Trade verification needs a fresh directory inside .local/verification");
        std::filesystem::create_directories(dir);
        const auto original=roster_database_.slotTable();
        roster_store_=std::make_unique<nba97::RosterSaveStore>(dir/"rosters.n97rst");
        roster_store_->load(roster_database_);
        frontend_page_=nba97::FrontendPage::TradePlayers;
        trade_teams_={2,24};openTrade();
        auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
        // Every slot pair on29 adjacent-team pairs, with the real private
        // catalogue/preference table. Validate conservation without publication.
        std::array<unsigned,4> matrix_results{}; // swap, transfer, both empty, minimum
        for(int team=0;team<29;++team)for(unsigned a=0;a<15;++a)for(unsigned b=0;b<15;++b) {
            Nba97TradeScreen candidate{};const uint8_t cursors[]{uint8_t(a),uint8_t(b)};
            const uint8_t tops[]{uint8_t((std::min)(a,9u)),uint8_t((std::min)(b,9u))};
            require(nba97_trade_begin(&candidate,original.data(),int16_t(team),int16_t((team+1)%29),0,nullptr,cursors,tops)!=0,"private matrix entry");
            const auto before=candidate;
            const bool empty_left=before.selected[0]==UINT16_MAX,empty_right=before.selected[1]==UINT16_MAX;
            const bool both_empty=empty_left&&empty_right,transfer=empty_left!=empty_right;
            const int donor=empty_left?1:0;
            const bool minimum=transfer&&before.counts[before.team[donor]]==8;
            const auto expected=both_empty?NBA97_TRADE_IDLE:minimum?NBA97_TRADE_NOTICE:NBA97_TRADE_SWAPPED;
            auto data=tradeData();
            require(nba97_trade_input(&candidate,0x800,&data)==NBA97_TRADE_PICK,"private matrix first callback");
            auto event=nba97_trade_input(&candidate,0x800,&data);
            require(event==expected,"private matrix wrong swap/transfer/rejection outcome");
            ++matrix_results[both_empty?2:minimum?3:transfer?1:0];
            require(!std::memcmp(candidate.snapshot,original.data(),sizeof(candidate.snapshot)),"private matrix snapshot mutated");
            if(event!=NBA97_TRADE_SWAPPED) {
                require(!std::memcmp(candidate.working,before.working,sizeof(candidate.working))&&
                    !std::memcmp(candidate.counts,before.counts,sizeof(candidate.counts))&&candidate.changes==0&&
                    candidate.phase==NBA97_TRADE_SECOND,"private matrix rejection changed draft or phase");
                if(minimum)require(candidate.notice.notice==NBA97_ROSTER_NOTICE_MINIMUM&&
                    candidate.notice.subject==before.team[donor],"private matrix minimum notice subject");
            } else {
                require(candidate.changes==1&&candidate.phase==NBA97_TRADE_FIRST&&nba97_trade_dirty(&candidate),"private matrix successful trade not committed to draft");
                for(int p=0;p<2;++p) {
                    const int t=before.team[p];
                    const int delta=transfer?(p==donor?-1:1):0;
                    require(candidate.counts[t]==before.counts[t]+delta,"private matrix wrong team count");
                    std::array<uint16_t,15> expected_members{},actual_members{};
                    std::copy_n(before.working+t*15,15,expected_members.begin());
                    std::copy_n(candidate.working+t*15,15,actual_members.begin());
                    // Independent membership oracle; donor starter ordering is
                    // tested separately by the recovered compaction tests.
                    expected_members[before.cursor[p]]=before.selected[1-p];
                    std::sort(expected_members.begin(),expected_members.end());
                    std::sort(actual_members.begin(),actual_members.end());
                    require(expected_members==actual_members,"private matrix wrong player moved");
                }
            }
            for(int t=0;t<30;++t)if(t!=before.team[0]&&t!=before.team[1]) {
                require(candidate.counts[t]==before.counts[t]&&!std::memcmp(candidate.working+t*15,
                    before.working+t*15,(t==29?100:15)*sizeof(uint16_t)),"private matrix unrelated roster changed");
            }
            nba97::RosterSlots proposed;std::copy_n(candidate.working,535,proposed.begin());
            (void)roster_database_.prepareSlotTable(proposed);
        }
        trace_.log("TRADE-MATRIX","PASS 6525 real-data slot pairs; bounded adjacent-team sample, not all team pairs; population/contiguity/ownership validated; no publication");
        trace_.log("TRADE-MATRIX-OUTCOMES","swap="+std::to_string(matrix_results[0])+" transfer="+std::to_string(matrix_results[1])+
            " both-empty="+std::to_string(matrix_results[2])+" minimum="+std::to_string(matrix_results[3])+"; exact event, phase, identities, counts, snapshot and unrelated rosters checked");
        auto settle=[&] {
            for(int i=0;i<50 && isRosterEditor();++i) {
                menu_elapsed_ms_+=17;
                nba97_trade_frame(&trade_screen_,0);
                nba97_reorder_child_input_ready(&reorder_child_,0);
                if(trade_choice_address_)tradeChoiceEvent(nba97_reset_tick(&trade_choice_,0));
                else stepReorderHelp(0,true);
                if(!isRosterEditor())break;
                nba97_frontend_palette_tick(&trade_palette_,compare_backgrounds_->bank(),33);
                advanceComparePalette();
                if(compare_refresh_.remaining)advanceCompareRefresh();
                if(compare_repeat_.post_frames)--compare_repeat_.post_frames;
            }
            frontend_transition_active_=false;
        };
        auto capture=[&](const char* name){
            settle();updatePlayerPhoto(true);
            const auto recorded=nativeRecordState();
            require(recorded[1]==static_cast<int>(nba97::FrontendPage::TradePlayers)&&
                recorded[3]==trade_screen_.team[0]&&recorded[4]==trade_screen_.phase&&
                recorded[5]==reorder_child_.state&&recorded[6]==reorder_help_.phase,
                "Trade recording used another screen's controller state");
            for(unsigned side=0;side<2;++side)
                require(recorded[7+side]==trade_screen_.cursor[side]&&
                    recorded[9+side]==trade_screen_.top[side]&&recorded[11+side]==trade_screen_.selected[side],
                    "Trade recording lost parent cursor/viewport/player identity");
            std::array<uint8_t,1070> slot_bytes{};
            for(unsigned i=0;i<535;++i) {
                slot_bytes[2*i]=uint8_t(trade_screen_.working[i]);
                slot_bytes[2*i+1]=uint8_t(trade_screen_.working[i]>>8);
            }
            nba97::Sha256 expected_hash;expected_hash.update(slot_bytes.data(),slot_bytes.size());
            require(nativeRecordSlotsHash()==expected_hash.digest(),"Trade recording hashed a stale Re-order draft");
            writePpm(renderTrade(),dir/(std::string(name)+".ppm"));trace_.log("TRADE-CHECKPOINT",name);
        };
        auto help=[&](const char* name){handleTradeKey('F');capture(name);handleTradeKey(VK_RETURN);settle();};
        capture("entry");help("help-first");
        for(int i=0;i<14;++i)handleTradeKey(VK_DOWN);capture("empty-first");
        handleTradeKey('D');capture("empty-view-notice");handleTradeKey(VK_RETURN);settle();
        require(!reorder_child_.state,"empty row opened child");
        handleTradeKey('S');capture("empty-compare-notice");handleTradeKey(VK_RETURN);settle();
        require(!reorder_child_.state && !nba97_trade_dirty(&trade_screen_),"empty Compare changed roster");
        for(int i=0;i<14;++i)handleTradeKey(VK_UP);
        handleTradeKey('C');capture("second");help("help-second");
        handleTradeKey('X');capture("cancel-second");
        require(trade_screen_.phase==NBA97_TRADE_FIRST&&!trade_screen_.child&&
            trade_screen_.team[0]==2&&trade_screen_.team[1]==24&&trade_screen_.cursor[0]==0&&trade_screen_.cursor[1]==0&&
            !std::memcmp(trade_screen_.working,original.data(),sizeof(trade_screen_.working))&&roster_database_.slotTable()==original,
            "replacement cancellation changed selection or roster");
        trace_.log("TRADE-CANCEL-VERIFY","PASS second -> first; Charlotte/Seattle cursor0/0; full draft and accepted roster unchanged");
        handleTradeKey('C');settle();
        handleTradeKey(VK_RIGHT);capture("second-team");
        require(trade_screen_.team[1]==25,"second team scan failed");
        handleTradeKey(VK_LEFT);settle();require(trade_screen_.team[1]==24,"second team reverse scan failed");
        handleTradeKey('X');settle();
        handleTradeKey('D');capture("view");help("view-help");
        auto view_team=roster_viewer_.teamIndex();handleTradeKey('K');capture("view-team-scan");
        require(roster_viewer_.teamIndex()!=view_team,"View R1 not routed");handleTradeKey('J');settle();
        handleTradeKey('E');capture("view-layer");handleTradeKey('Q');settle();
        handleTradeKey(VK_RIGHT);capture("view-browsed");handleTradeKey(VK_RETURN);capture("view-keep");
        require(trade_choice_address_==0x800AEE88&&trade_choice_.choice==1,"View keep modal/default ignore missing");
        handleTradeKey('C');capture("view-ignore");
        require(!reorder_child_.state&&trade_screen_.team[0]==2&&trade_screen_.team[1]==24&&
            trade_screen_.cursor[0]==0&&nba97_trade_result(&trade_screen_)==0&&!nba97_trade_dirty(&trade_screen_),
            "View ignored proposal changed parent");
        handleTradeKey('S');capture("compare-initial");handleTradeKey('X');settle();
        require(!reorder_child_.state&&!trade_choice_address_&&trade_screen_.cursor[0]==0&&
            nba97_trade_result(&trade_screen_)==0,"unmodified Compare cancel failed");
        handleTradeKey('S');settle();handleTradeKey(VK_RIGHT);settle();handleTradeKey(VK_RETURN);capture("compare-initial-keep");
        require(trade_choice_address_==0x800AEEF6&&trade_choice_.choice==1,"Compare keep modal/default ignore missing");
        handleTradeKey(VK_UP);settle();handleTradeKey('C');capture("compare-initial-return");
        require(!reorder_child_.state&&trade_screen_.team[0]==2&&trade_screen_.team[1]==24&&
            trade_screen_.cursor[0]==1&&trade_screen_.top[0]==1&&trade_screen_.cursor[1]==0&&
            !nba97_trade_dirty(&trade_screen_)&&roster_database_.slotTable()==original,"Compare keep changed roster or wrong selection");
        handleTradeKey(VK_UP);settle(); // Restore Divac for the independent View keep path.
        handleTradeKey('D');settle();handleTradeKey(VK_RIGHT);settle();handleTradeKey(VK_RETURN);settle();
        require(trade_choice_address_==0x800AEE88&&trade_choice_.choice==1,"second View proposal missing");
        handleTradeKey(VK_UP);settle();handleTradeKey('C');settle();capture("view-return");
        require(!reorder_child_.state && trade_screen_.cursor[0]==1,"View adoption failed");
        handleTradeKey('S');capture("compare");help("compare-help");
        handleTradeKey('E');settle();require(reorder_compare_.layer==3,"Compare R2 not routed");capture("compare-layer");
        handleTradeKey('Q');settle();handleTradeKey('K');settle();require(reorder_compare_.team[0]==3,"Compare R1 not routed");
        handleTradeKey('J');settle();
        handleTradeKey(VK_RIGHT);settle();capture("compare-browsed");handleTradeKey(VK_RETURN);capture("compare-keep");
        require(trade_choice_address_==0x800AEEF6,"Compare keep modal missing");
        handleTradeKey(VK_UP);settle();handleTradeKey('C');settle();capture("compare-return");
        require(!reorder_child_.state && trade_screen_.cursor[0]==2,"Compare adoption failed");
        handleTradeKey(VK_UP);settle(); // Match original reference: Mason versus McIlvaine.
        handleTradeKey('C');handleTradeKey('C');capture("traded");
        require(nba97_trade_dirty(&trade_screen_) && roster_database_.slotTable()==original,"trade draft isolation failed");
        const auto traded_draft=tradeDraft()->slotTable();
        const auto traded_left=trade_screen_.selected[0],traded_right=trade_screen_.selected[1];
        handleTradeKey('X');capture("discard-after-trade");
        require(trade_choice_address_==0x800AF4F8&&trade_choice_.choice==1,
            "post-trade discard prompt/default don't exit missing");
        handleTradeKey('C');settle();
        require(!trade_choice_address_&&isRosterEditor()&&
            !std::memcmp(trade_screen_.working,traded_draft.data(),sizeof(trade_screen_.working))&&
            roster_database_.slotTable()==original,"declining discard changed draft or published it");
        handleTradeKey('D');capture("view-after-trade");
        require(viewerDatabase().slotTable()==traded_draft&&
            viewerDatabase().teams().at(roster_viewer_.teamIndex()).id==2&&
            viewerDatabase().slotTable()[2*15+roster_viewer_.playerIndex()]==traded_left,
            "View after trade used accepted/original ownership instead of draft");
        handleTradeKey('X');settle();
        handleTradeKey('S');capture("compare-after-trade");
        require(reorder_compare_.player[0]==traded_left&&reorder_compare_.player[1]==traded_right&&
            viewerDatabase().slotTable()==traded_draft,"Compare after trade used stale player identities");
        handleTradeKey('X');settle();
        require(!reorder_child_.state&&!trade_choice_address_&&
            !std::memcmp(trade_screen_.working,traded_draft.data(),sizeof(trade_screen_.working))&&
            roster_database_.slotTable()==original&&roster_store_->accepted().generation==0,
            "post-trade child round trip mutated or published draft");
        trace_.log("TRADE-DRAFT-CHILDREN","PASS View/Compare see traded ownership; child returns keep working slots; live roster/save generation unchanged");
        bool injected=false;
        reorder_save_hooks_={[](nba97::RosterSaveStage stage,void* p){if(stage==nba97::RosterSaveStage::PartialWrite){*static_cast<bool*>(p)=true;throw std::runtime_error("injected Trade save failure");}},&injected};
        const auto before=trade_screen_;handleTradeKey(VK_RETURN);
        require(injected && !std::memcmp(&before,&trade_screen_,sizeof(before)) && roster_database_.slotTable()==original,"failed save changed draft/live state");
        capture("save-failed");handleTradeKey(VK_RETURN);settle();reorder_save_hooks_={};
        handleTradeKey(VK_RETURN);settle();
        require(frontend_page_==nba97::FrontendPage::Rosters && roster_database_.slotTable()!=original,"Trade save did not publish");
        const auto accepted=roster_database_.slotTable();
        auto restart=roster_database_.prepareSlotTable(original);nba97::RosterSaveStore reopened(dir/"rosters.n97rst");
        reopened.load(restart);require(restart.slotTable()==accepted,"Trade restart lost changes");
        frontend_page_=nba97::FrontendPage::TradePlayers;openTrade();capture("restart");
        handleTradeKey('C');handleTradeKey('C');handleTradeKey('X');capture("discard");
        require(trade_choice_address_==0x800AF4F8,"discard modal missing");
        handleTradeKey(VK_UP);settle();handleTradeKey('C');settle();
        require(frontend_page_==nba97::FrontendPage::Rosters && roster_database_.slotTable()==accepted,"discard changed accepted save");
        frontend_page_=nba97::FrontendPage::TradePlayers;openTrade();
        // Use a different replacement so this edit doesn't merely undo the
        // first trade: the subsequent Reset must exercise a real commit.
        handleTradeKey('C');handleTradeKey(VK_DOWN);handleTradeKey('C');settle();
        const auto sync_draft=tradeDraft()->slotTable();
        const auto previous_generation=roster_store_->accepted().generation;
        bool sync_injected=false;
        reorder_save_hooks_={[](nba97::RosterSaveStage stage,void* p){
            if(stage==nba97::RosterSaveStage::BeforeDirectorySync){*static_cast<bool*>(p)=true;
                throw std::runtime_error("injected Trade post-replacement sync failure");}},&sync_injected};
        handleTradeKey(VK_RETURN);capture("save-sync-uncertain");reorder_save_hooks_={};
        require(sync_injected&&reorder_exit_after_notice_&&reorder_notice_&&
            nba97_trade_result(&trade_screen_)==1&&roster_database_.slotTable()==sync_draft&&
            roster_store_->accepted().generation==previous_generation+1,
            "postcommit sync warning failed to publish exactly once");
        auto sync_restart=roster_database_.prepareSlotTable(original);
        nba97::RosterSaveStore sync_reopened(dir/"rosters.n97rst");sync_reopened.load(sync_restart);
        require(sync_restart.slotTable()==sync_draft&&sync_reopened.accepted().generation==previous_generation+1,
            "postcommit sync warning disk/live state diverged");
        handleTradeKey(VK_RETURN);settle();
        require(frontend_page_==nba97::FrontendPage::Rosters&&!reorder_exit_after_notice_&&
            !reorder_notice_&&roster_store_->accepted().generation==previous_generation+1&&
            roster_database_.slotTable()==sync_draft,"closing committed warning retried or lost the trade");
        trace_.log("TRADE-SYNC-VERIFY","PASS post-replacement failure publishes once; fresh load matches; notice acknowledgement exits without retry");
        commitRosterReset();require(roster_database_.slotTable()==original && !rosterResetEligible(),"Reset did not restore defaults");
        // Intentionally preserve the original child-return undo bug. Separate
        // the constructor checkpoint from the native durable-save baseline.
        for(int scenario=0;scenario<4;++scenario) {
            frontend_page_=nba97::FrontendPage::TradePlayers;
            trade_teams_={2,24};trade_cursors_={0,0};trade_tops_={0,0};openTrade();
            const auto generation=roster_store_->accepted().generation;
            handleTradeKey(VK_DOWN); // Navigate to Mason; don't fake remembered cursors on reopen.
            handleTradeKey('C');handleTradeKey('C');settle();
            const auto retained=tradeDraft()->slotTable();
            require(retained!=original&&nba97_trade_undo_dirty(&trade_screen_),"quirk fixture trade missing");
            handleTradeKey('F');settle();handleTradeKey(VK_RETURN);settle();
            require(nba97_trade_undo_dirty(&trade_screen_),"Help must not renew the original constructor checkpoint");
            const bool second=scenario%2!=0;
            if(second){handleTradeKey('C');settle();}
            handleTradeKey(second?'S':'D');settle();handleTradeKey('X');settle();
            require(!reorder_child_.state&&!nba97_trade_undo_dirty(&trade_screen_)&&
                nba97_trade_dirty(&trade_screen_)&&trade_screen_.phase==(second?NBA97_TRADE_SECOND:NBA97_TRADE_FIRST)&&
                trade_screen_.changes==0&&roster_database_.slotTable()==original&&
                roster_store_->accepted().generation==generation,"child quirk rebased wrong state or autosaved");
            if(second){handleTradeKey('X');settle();require(isRosterEditor(),
                "cancel second pick must not exit or persist");}
            if(second) {
                handleTradeKey('C');handleTradeKey(VK_DOWN);handleTradeKey('C');settle();
                const auto later=tradeDraft()->slotTable();require(later!=retained,"later quirk edit missing");
                handleTradeKey('X');capture(("quirk-later-discard-"+std::to_string(scenario)).c_str());
                require(trade_choice_address_==0x800AF4F8,"later edits must request discard");
                handleTradeKey('C');settle();require(tradeDraft()->slotTable()==later,"declining quirk discard lost later edits");
                handleTradeKey('X');settle();handleTradeKey(VK_UP);settle();
            }
            bool fault=false;
            if(scenario==1)reorder_save_hooks_={[](nba97::RosterSaveStage stage,void* p){
                if(stage==nba97::RosterSaveStage::PartialWrite){*static_cast<bool*>(p)=true;
                    throw std::runtime_error("injected retained-checkpoint cancel save failure");}},&fault};
            if(scenario==2)reorder_save_hooks_={[](nba97::RosterSaveStage stage,void* p){
                if(stage==nba97::RosterSaveStage::BeforeDirectorySync){*static_cast<bool*>(p)=true;
                    throw std::runtime_error("injected retained-checkpoint cancel sync uncertainty");}},&fault};
            const auto pre_exit=trade_screen_;
            handleTradeKey(second?'C':'X');settle();
            if(scenario==1) {
                require(fault&&reorder_notice_&&!reorder_exit_after_notice_&&
                    !std::memcmp(trade_screen_.working,pre_exit.working,sizeof(pre_exit.working))&&
                    !std::memcmp(trade_screen_.undo,pre_exit.undo,sizeof(pre_exit.undo))&&
                    nba97_trade_result(&trade_screen_)==0&&roster_database_.slotTable()==original&&
                    roster_store_->accepted().generation==generation,"failed cancel save lost draft/checkpoint or published");
                capture("quirk-cancel-save-failed");
                handleTradeKey(VK_RETURN);settle();reorder_save_hooks_={};
                handleTradeKey('X');settle();handleTradeKey(VK_UP);settle();handleTradeKey('C');settle();
            } else if(scenario==2) {
                require(fault&&reorder_notice_&&reorder_exit_after_notice_&&
                    nba97_trade_result(&trade_screen_)==-1&&roster_database_.slotTable()==retained&&
                    roster_store_->accepted().generation==generation+1,"cancel sync warning must retain signed result and publish once");
                capture("quirk-cancel-sync-uncertain");reorder_save_hooks_={};handleTradeKey(VK_RETURN);settle();
            }
            require(frontend_page_==nba97::FrontendPage::Rosters&&roster_database_.slotTable()==retained&&
                nba97_trade_result(&trade_screen_)==-1&&roster_store_->accepted().generation==generation+1,
                "original quirk cancel failed to retain exactly the pre-child roster");
            auto disk=roster_database_.prepareSlotTable(original);nba97::RosterSaveStore reload(dir/"rosters.n97rst");reload.load(disk);
            require(disk.slotTable()==retained&&reload.accepted().generation==generation+1,"retained cancel restart mismatch");
            frontend_page_=nba97::FrontendPage::TradePlayers;openTrade();
            require(trade_screen_.cursor[0]==0&&trade_screen_.top[0]==0&&trade_screen_.cursor[1]==0,
                "cancel exit must not overwrite remembered entry cursors");
            capture(("quirk-retained-reopen-"+std::to_string(scenario)).c_str());
            if(scenario==0) {
                // Original reference continues from this retained roster:
                // Divac versus Mason, then last empty Charlotte slot receives Mason.
                handleTradeKey('S');capture("compare-retained");handleTradeKey('X');settle();
                require(!reorder_child_.state&&trade_screen_.cursor[0]==0&&trade_screen_.cursor[1]==0&&
                    !nba97_trade_dirty(&trade_screen_),"retained Compare return changed selection/roster");
                for(int i=0;i<14;++i)handleTradeKey(VK_DOWN);
                capture("transfer-receiver-empty");
                require(trade_screen_.selected[0]==UINT16_MAX&&trade_screen_.cursor[0]==14,
                    "reference transfer receiver must be the last empty slot");
                const auto transfer_before=trade_screen_;
                handleTradeKey('C');capture("transfer-second");
                require(trade_screen_.phase==NBA97_TRADE_SECOND,"empty receiver selection failed");
                handleTradeKey('C');capture("transfer-complete");
                require(trade_screen_.phase==NBA97_TRADE_FIRST&&
                    trade_screen_.counts[2]==transfer_before.counts[2]+1&&
                    trade_screen_.counts[24]==transfer_before.counts[24]-1&&
                    roster_database_.slotTable()==retained&&roster_store_->accepted().generation==generation+1,
                    "reference transfer failed counts/phase/draft isolation");
                for(int side=0;side<2;++side) {
                    const int team=side==0?2:24;
                    std::array<uint16_t,15> expected{},actual{};
                    std::copy_n(retained.data()+team*15,15,expected.begin());
                    std::copy_n(trade_screen_.working+team*15,15,actual.begin());
                    expected[side==0?14:0]=side==0?transfer_before.selected[1]:UINT16_MAX;
                    std::sort(expected.begin(),expected.end());std::sort(actual.begin(),actual.end());
                    require(expected==actual,"reference transfer moved the wrong player");
                }
                (void)tradeDraft();
                handleTradeKey('X');capture("transfer-discard");
                require(trade_choice_address_==0x800AF4F8,"reference transfer discard prompt missing");
                handleTradeKey(VK_UP);settle();handleTradeKey('C');settle();
                require(roster_database_.slotTable()==retained,"transfer discard lost retained pre-child roster");
                require(frontend_page_==nba97::FrontendPage::Rosters,"transfer discard did not exit to cards");
                frontend_page_=nba97::FrontendPage::TradePlayers;openTrade();
                require(!std::memcmp(trade_screen_.working,retained.data(),sizeof(trade_screen_.working))&&
                    trade_screen_.counts[2]==13&&trade_screen_.counts[24]==13&&
                    trade_screen_.cursor[0]==0&&trade_screen_.cursor[1]==0,
                    "transfer discard reopening changed retained roster/counts/cursors");
                capture("transfer-discard-reopened");handleTradeKey('X');settle();
                trace_.log("TRADE-TRANSFER-VERIFY","PASS Mason Seattle -> last empty Charlotte slot; counts+1/-1; exact membership; discard restores retained roster without save");
            } else {handleTradeKey('X');settle();}
            require(frontend_page_==nba97::FrontendPage::Rosters&&
                roster_store_->accepted().generation==generation+1,"unchanged reopening must not republish");
            commitRosterReset();require(roster_database_.slotTable()==original&&!rosterResetEligible(),"quirk Reset failed");
        }
        trace_.log("TRADE-QUIRK-VERIFY","PASS 4 host routes; Help does not rebase; View/Compare rebase without save; cancel keeps pre-child trades; later discard/decline; failure/retry and sync warning; restart/Reset");
        trace_.log("TRADE-RECORD-VERIFY","PASS all checkpoints: Trade parent team/phase/cursors/viewport/player IDs and 535-slot little-endian draft hash; v1 timeline right-team field absent");
        trace_.log("TRADE-HOST-VERIFY","PASS entry/second-team/cancel/Help/View/Compare/keep/trade/save-failure/retry/restart/discard/Reset; isolated save only");
        return 0;
    }

    bool isSign() const { return frontend_page_==nba97::FrontendPage::SignFreeAgent; }
    bool isRelease() const { return frontend_page_==nba97::FrontendPage::ReleasePlayers; }
    bool isRosterEditor() const { return isRelease() || isSign() || frontend_page_==nba97::FrontendPage::TradePlayers; }
    std::uint8_t editorState() const {return isRelease()?17:isSign()?14:13;}
    void openTrade() {
        trade_assets_=std::make_unique<nba97::TradeAssets>(options_.asset_root,editorState());
        reorder_labels_=std::make_unique<nba97::ReorderLabelPreview>(options_.asset_root);
        reorder_help_pack_=std::make_unique<nba97::FrontendHelpPack>(options_.asset_root/
            (isRelease()?"release/help.n97ui":isSign()?"sign/help.n97ui":"trade/help.n97ui"));
        reorder_help_={};reorder_notice_.reset();reorder_exit_after_notice_=false;
        trade_choice_={};trade_choice_address_=0;reorder_child_={};reorder_child_database_.reset();
        for(const char* tag:{isRelease()?"ba23":isSign()?"ba30":"ba38","ba02","frmr","110p","111p",
                            isRelease()?"help":"hel1","hel2"}) {
            auto im=load_png_image(options_.asset_root/"menu/ZSET4-decoded"/(std::string(tag)+".png"));
            for(std::size_t i=0;i<im.rgba.size();i+=4)
                if(!im.rgba[i]&&!im.rgba[i+1]&&!im.rgba[i+2])im.rgba[i+3]=0;
            roster_sprites_[tag]=std::move(im);
        }
        const auto table=roster_database_.slotTable();
        const int opened=isRelease()?nba97_release_begin(&trade_screen_,table.data(),release_team_,0,nullptr,0,0):
            isSign()?nba97_sign_begin(&trade_screen_,table.data(),sign_team_,0,nullptr,sign_cursors_.data(),sign_tops_.data()):
            nba97_trade_begin(&trade_screen_,table.data(),trade_teams_[0],trade_teams_[1],0,nullptr,trade_cursors_.data(),trade_tops_.data());
        if(!opened) throw std::runtime_error("invalid roster editor entry");
        std::size_t count=0;for(const auto& p:roster_database_.players())count=(std::max)(count,std::size_t(p.id)+1);
        trade_positions_.assign(count,99);trade_injuries_.assign(count,0);
        for(const auto& p:roster_database_.players())trade_positions_[p.id]=p.position;
        if(!compare_backgrounds_)compare_backgrounds_=std::make_unique<nba97::FrontendPaletteAssets>(
            options_.asset_root/"menu/ZSET4-team-backgrounds/indexed.n97pal");
        if(!nba97_frontend_palette_begin(&trade_palette_,compare_backgrounds_->bank(),33,trade_screen_.team[0],trade_screen_.team[1]))
            throw std::runtime_error("invalid Trade palettes");
        trade_portrait_ids_.fill(UINT16_MAX);trade_portraits_={};loadTradePortraits();
        trade_tick_=menu_elapsed_ms_/17;
        if(isRelease()) {
            trace_.log("RELEASE-ENTRY","8005721C -> 80056494; state17 kinds1/0 capacities15/100 right-base15; first callback57084, second NULL; single-stage; signed selector result");
            trace_.log("RELEASE-ASSETS","80093330[17] -> 80097104; ba23=(140,10); help=(235,217); Z2PORT 87x51; private state17 Help rect121,80,270,125; ZFONT0/1");
            trace_.log("RELEASE-KEYS","arrows=donor rows/team; F=Help; X/Esc=cancel/discard; Enter=accept; C/Space=release; D=View; S=Compare (both start on donor, source8005A074); edits remain a private draft until exit");
        } else {
        trace_.log(isSign()?"SIGN-ENTRY":"TRADE-ENTRY",isSign()?"80056F9C -> 80056494; state14 kinds0/1 capacity100/15 right-base100; callbacks56D6C/56E40; six visible rows; 535-slot isolated snapshot":
            "80056CD0 -> 80056494; graphics/controller state13; kinds1/1; 535-slot isolated snapshot; 30 rows/6 visible per side");
        trace_.log(isSign()?"SIGN-ASSETS":"TRADE-ASSETS",isSign()?"80096E84 ba30=(156,10); Z2PORT 87x51; state14 private Help/notice pack":"80096D24 ba38=(155,10); Z2PORT 87x51; private UI/preference/Help packs; independent indexed CLUT halves");
        trace_.log(isSign()?"SIGN-KEYS":"TRADE-KEYS",isSign()?
            "Up/Down=active rows; Left/Right=receiver team; C/Space=pick/sign into empty slot; X/Esc=cancel; Enter=accept/save from first stage; D=View; S=Compare; F=Help; normal mode0":
            "arrows=active list rows/teams; C/Space=pick/trade; X/Esc=cancel; Enter=accept/save; D=View; S=Compare; F=Help; normal frontend mode0/no injury context");
        }
        logTrade("constructed");
        if(!roster_load_error_.empty())showReorderSaveNotice("save unavailable","file needs attention","see CLI for details",false,0x800);
    }
    Nba97TradeData tradeData() const {
        return {trade_positions_.data(),trade_injuries_.data(),trade_assets_->preference(),trade_positions_.size(),0};
    }
    void loadTradePortraits() {
        for(unsigned p=0;p<2;++p) {
            auto id=trade_screen_.selected[p];if(id==trade_portrait_ids_[p]&&!trade_portraits_[p].rgba.empty())continue;
            char file[40]{};sprintf_s(file,"player_%03u.png",id==UINT16_MAX?0u:unsigned(id)+1);
            auto im=load_png_image(options_.asset_root/"menu/Z2PORT-decoded"/file);
            if(im.width!=87||im.height!=51)throw std::runtime_error("invalid Trade Z2PORT portrait");
            trade_portraits_[p]=std::move(im);trade_portrait_ids_[p]=id;
            trace_.log(isRelease()?"RELEASE-PORTRAIT":"TRADE-PORTRAIT","side="+std::to_string(p)+" player="+std::to_string(id)+" record="+file);
        }
    }
    void logTrade(const char* event) {
        const auto& s=trade_screen_;
        trace_.log(isRelease()?"RELEASE":isSign()?"SIGN":"TRADE",std::string(event)+" teams="+std::to_string(s.team[0])+"/"+std::to_string(s.team[1])+
            " phase="+std::to_string(s.phase)+" selected="+std::to_string(s.selected[0])+"/"+std::to_string(s.selected[1])+
            " cursor="+std::to_string(s.cursor[0])+"/"+std::to_string(s.cursor[1])+" top="+std::to_string(s.top[0])+"/"+std::to_string(s.top[1])+
            " counts="+std::to_string(s.counts[s.team[0]])+"/"+std::to_string(s.counts[s.team[1]])+
            " changes="+std::to_string(s.changes)+" save-dirty="+std::to_string(nba97_trade_dirty(&s))+
            " undo-dirty="+std::to_string(nba97_trade_undo_dirty(&s))+" child="+std::to_string(s.child)+
            " selector-result="+std::to_string(nba97_trade_result(&s))+" kinds="+std::to_string(s.list_kind[0])+"/"+std::to_string(s.list_kind[1]));
    }
    PshImage renderTrade() {
        prepareFrontendTitle();
        auto im=reorder_child_.state ? renderReorder() : nba97::renderTradeScreen(trade_screen_,roster_sprites_,menu_font_,
            trade_portraits_,trade_assets_->labels(trade_screen_,roster_database_),*compare_backgrounds_,trade_palette_,menu_elapsed_ms_,frontend_title_.corners());
        if(!reorder_child_.state && nba97_help_visible(&reorder_help_))
            nba97::FrontendHelpPack::draw(im,reorder_labels_->smallFont(),reorder_notice_?*reorder_notice_:
                reorder_help_pack_->descriptor(editorState(),trade_screen_.phase==NBA97_TRADE_SECOND),reorder_help_);
        if(trade_choice_address_)trade_assets_->drawChoice(im,trade_choice_address_,trade_choice_);
        return im;
    }
    static uint16_t tradeKey(WPARAM key) {
        return key=='J'||key==VK_OEM_4?0x200:key=='K'||key==VK_OEM_6?0x400:key=='Q'?0x1000:key=='E'?0x2000:
            key==VK_UP?1:key==VK_DOWN?2:key==VK_LEFT?8:key==VK_RIGHT?4:
            key=='C'||key==VK_SPACE?0x800:key=='X'||key==VK_ESCAPE||key==VK_BACK?0x100:
            key==VK_RETURN?0x80:key=='D'?0x10:key=='S'?0x40:
            key=='F'||key=='H'||key==VK_F1?0x20:0;
    }
    void syncTrade() {
        loadTradePortraits();
        for(unsigned p=0;p<2;++p)nba97_frontend_palette_request(&trade_palette_,p,trade_screen_.team[p],33);
    }
    void tradeChoiceEvent(int event) {
        if(event&NBA97_RESET_OPEN)playBottomMenuSound(12,"trade-confirm-open");
        if(event&NBA97_RESET_UP)playBottomMenuSound(3,"trade-confirm-up");
        if(event&NBA97_RESET_DOWN)playBottomMenuSound(4,"trade-confirm-down");
        if(event&NBA97_RESET_CHOSEN){playBottomMenuSound(6,"trade-confirm-choice");playBottomMenuSound(8,"trade-confirm-close");}
        if(event&NBA97_RESET_RETURN) {
            auto address=trade_choice_address_;trade_choice_address_=0;
            const bool yes=trade_choice_.choice==0;
            if(address==0x800AF4F8) {
                const auto before=trade_screen_;
                auto result=nba97_trade_discard_answer(&trade_screen_,yes,0x800);
                if(result==NBA97_TRADE_DISCARD)completeTradeExit(before,0x800);
                else logTrade("discard declined");
            } else completeTradeChild(yes);
        }
    }
    void openTradeChoice(uint32_t address,uint16_t held) {
        trade_choice_={};trade_choice_address_=address;
        tradeChoiceEvent(nba97_reset_open(&trade_choice_,trade_assets_->rect(address),held,0));
        trace_.log("TRADE-CONFIRM","descriptor="+std::to_string(address)+" default=ignore/cancel; original red modal; no mutation before choice");
    }
    void finishTrade(bool accepted) {
        if(nba97_trade_result(&trade_screen_)!=(accepted?1:-1))throw std::runtime_error("Trade selector exit contract mismatch");
        if(accepted)for(unsigned p=0;p<2;++p){
            if(isRelease()){release_team_=trade_screen_.team[0];}
            else if(isSign()){sign_team_=nba97_reorder_normalize_team(trade_screen_.team[0],trade_screen_.mode,trade_screen_.eligible[0]);sign_cursors_[p]=trade_screen_.cursor[p];sign_tops_[p]=trade_screen_.top[p];}
            else {trade_teams_[p]=trade_screen_.team[p];trade_cursors_[p]=trade_screen_.cursor[p];trade_tops_[p]=trade_screen_.top[p];}}
        logTrade(accepted?"accepted and durably published":nba97_trade_dirty(&trade_screen_)?
            "cancelled; pre-child checkpoint retained and durably published":"cancelled; entry roster unchanged");
        beginFrontendTransition(nba97::FrontendPage::Rosters,isRelease()?
            (accepted?"Release saved":"Release cancelled"):isSign()?
            (accepted?"Sign saved":"Sign cancelled"):(accepted?"Trade saved":"Trade cancelled"));
        // 57CE4 resets both cursor fields to-1;56494 maps them to row0 on
        // next menu entry. Direct child returns do NOT go through57CE4.
        if(trade_screen_.frontend_state==14){sign_cursors_={};sign_tops_={};}
        trade_assets_.reset();trade_portraits_={};reorder_help_pack_.reset();reorder_labels_.reset();
        reorder_help_={};reorder_notice_.reset();reorder_exit_after_notice_=false;
    }
    void completeTradeExit(const Nba97TradeScreen& before,uint16_t held) {
        const auto result=nba97_trade_result(&trade_screen_);
        if(result!=1&&result!=-1)throw std::runtime_error("Trade exit without signed selector result");
        const bool accepted=result==1;
        if(accepted||nba97_trade_dirty(&trade_screen_)) {
            nba97::RosterCommitResult saved;
            try {
                if(!roster_store_)throw std::runtime_error("roster save unavailable");
                if(std::memcmp(roster_database_.slotTable().data(),trade_screen_.snapshot,sizeof(trade_screen_.snapshot)))
                    throw std::runtime_error("Trade baseline conflict");
                nba97::RosterSlots proposed;std::copy_n(trade_screen_.working,535,proposed.begin());
                saved=roster_store_->commit(roster_database_,proposed,reorder_save_hooks_);
            }catch(const std::exception& e){
                trade_screen_=before;syncTrade();trace_.log(isRelease()?"RELEASE-SAVE-FAILED":isSign()?"SIGN-SAVE-FAILED":"TRADE-SAVE-FAILED",e.what());
                showReorderSaveNotice("save failed","draft kept - exit cancelled","see CLI then retry",false,held);
                rebuildMenuFrame();return;
            }
            trace_.log(isRelease()?"RELEASE-COMMIT":isSign()?"SIGN-COMMIT":"TRADE-COMMIT","generation="+std::to_string(saved.generation)+" bytes="+std::to_string(saved.bytes)+
                " sync="+std::to_string(saved.sync_completed)+" selector-result="+std::to_string(result)+
                (accepted?" reason=Start":" reason=retained-pre-child-checkpoint"));
            if(!saved.sync_completed){
                showReorderSaveNotice("saved - sync uncertain","do not retry this edit","see CLI for details",true,held);
                rebuildMenuFrame();return;
            }
        }
        playBottomMenuSound(accepted?9:10,accepted?"trade-accept":"trade-discard");finishTrade(accepted);
    }
    void updateTrade() {
        if(!isRosterEditor())return;
        const auto now=menu_elapsed_ms_/17;
        const auto first=now>120?(std::max)(trade_tick_,now-120):trade_tick_;
        for(auto n=first;n<now;++n) {
            uint16_t raw=0;for(int key:std::array<int,21>{VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,'C',VK_SPACE,'X',VK_ESCAPE,VK_RETURN,'D','S','F','H',VK_F1,VK_BACK,'J','K','Q','E',VK_OEM_4,VK_OEM_6})
                if(GetForegroundWindow()==window_ && !IsIconic(window_) && (GetAsyncKeyState(key)&0x8000))raw|=tradeKey(key);
            if(!reorder_notice_) {
                if(reorder_child_.state)nba97_reorder_child_input_ready(&reorder_child_,raw);
                else if(!trade_choice_address_) {
                    const auto release_wait=trade_screen_.release_scroll_remaining;
                    nba97_trade_frame(&trade_screen_,raw);
                    if(release_wait && !trade_screen_.release_scroll_remaining)
                        logTrade("80056FF4 nine-frame receiver scroll complete; donor stays active");
                }
            }
            nba97_frontend_palette_tick(&trade_palette_,compare_backgrounds_->bank(),33);
            advanceComparePalette();
            if(trade_choice_address_)tradeChoiceEvent(nba97_reset_tick(&trade_choice_,raw));
            else stepReorderHelp(raw,true);
            if(!isRosterEditor())break;
        }
        trade_tick_=now;
    }
    void handleTradeKey(WPARAM key) {
        const auto raw=tradeKey(key);if(!raw || cool_fact_flash_.remaining)return;
        if(player_notice_.phase!=NBA97_HELP_CLOSED){playerNoticeEvent(nba97_help_input(&player_notice_,raw));return;}
        if(trade_choice_address_){tradeChoiceEvent(nba97_reset_input(&trade_choice_,raw==0x80?0x800:raw));rebuildMenuFrame();return;}
        if(reorder_help_.phase!=NBA97_HELP_CLOSED){stepReorderHelp(raw,false);rebuildMenuFrame();return;}
        if(reorder_child_.state) {
            if(reorder_child_.waiting_input_change)return;
            if(reorder_child_.state==0x23)handleCompareKey(key);
            else if(raw==0x20)openReorderHelp();
            else if(raw==0x80||raw==0x100)requestTradeChildReturn(raw);
            else handleRosterViewKey(key=='C'?VK_SPACE:key=='D'?'S':key);
            rebuildMenuFrame();return;
        }
        if(trade_screen_.waiting || trade_screen_.release_scroll_remaining)return;
        if(raw==0x20){openReorderHelp();rebuildMenuFrame();return;}
        const auto before=trade_screen_;const auto data=tradeData();
        const auto event=nba97_trade_input(&trade_screen_,raw,&data);
        if(event==NBA97_TRADE_PENDING) {
            trace_.log("RELEASE-PENDING","raw="+std::to_string(raw)+
                " callback80057084 not implemented in entry slice; no phase change, mutation, modal or sound; View/Compare await Release-specific routing");
            rebuildMenuFrame();return;
        }
        if(event==NBA97_TRADE_SWAPPED) {
            try {(void)tradeDraft();} // Native invariant: preserve the entire base population.
            catch(const std::exception& e) {
                trade_screen_=before;trace_.log("TRADE-MUTATION-BLOCKED",std::string(e.what())+"; draft rolled back; live/save untouched");
                showReorderSaveNotice("trade rejected","draft kept unchanged","see CLI for details",false,raw);rebuildMenuFrame();return;
            }
            if(isRelease()) trace_.log("RELEASE-MUTATE","80057084 ->558E0 ->55AF8(2) ->56FF4; player="+
                std::to_string(before.selected[0])+" donor="+std::to_string(trade_screen_.team[0])+
                " donor_count="+std::to_string(trade_screen_.counts[trade_screen_.team[0]])+
                " free_count="+std::to_string(trade_screen_.counts[29])+
                " scroll_wait="+std::to_string(trade_screen_.release_scroll_remaining)+"; save=no (isolated draft)");
        }
        if(event==NBA97_TRADE_NOTICE) {
            const auto& d=trade_screen_.notice;std::string subject;
            if(d.notice==NBA97_ROSTER_NOTICE_INJURED)subject=roster_database_.player(d.subject)->displayName();
            else if(d.subject>=0)subject=std::string(roster_database_.team(d.subject)->city);
            reorder_notice_=d.message_address==0x800AFC22?trade_assets_->emptyNotice(raw==0x40):
                trade_assets_->notice(d.message_address,subject);
            nba97_help_open(&reorder_help_,reorder_notice_->rect,raw);playBottomMenuSound(5,"trade-warning");
            trace_.log(isRelease()?"RELEASE-REFUSE":isSign()?"SIGN-REFUSE":"TRADE-REJECT","source descriptor="+std::to_string(d.message_address)+" subject="+std::to_string(d.subject)+" draft unchanged");
        } else if(event==NBA97_TRADE_DISCARD_PROMPT)openTradeChoice(0x800AF4F8,raw);
        else if(event==NBA97_TRADE_VIEW || event==NBA97_TRADE_COMPARE)openTradeChild();
        else if(event==NBA97_TRADE_ACCEPT||event==NBA97_TRADE_DISCARD){completeTradeExit(before,raw);return;}
        else if(event==NBA97_TRADE_INVALID)throw std::runtime_error("Trade controller rejected invalid data");
        const auto cue=nba97_trade_event_sound(event,raw);
        trace_.log(isRelease()?"RELEASE-CUE":isSign()?"SIGN-CUE":"TRADE-CUE","event="+std::to_string(event)+" raw="+std::to_string(raw)+
            " cue="+std::to_string(cue)+" source=8003D930/80055314; zero=no selector cue");
        if(cue)playBottomMenuSound(cue,isRelease()?"release-selector":isSign()?"sign-selector":"trade-selector");
        syncTrade();logTrade(("event="+std::to_string(event)).c_str());rebuildMenuFrame();
    }

    std::unique_ptr<const nba97::RosterDatabase> tradeDraft() const {
        if(std::memcmp(roster_database_.slotTable().data(),trade_screen_.snapshot,sizeof(trade_screen_.snapshot)))
            throw std::runtime_error("Trade draft baseline changed");
        nba97::RosterSlots slots;std::copy_n(trade_screen_.working,535,slots.begin());
        return std::make_unique<const nba97::RosterDatabase>(roster_database_.prepareSlotTable(slots));
    }
    void openTradeChild() {
        // 56254 saves the left-team selector argument before child entry.
        // A later Sign menu re-entry therefore normalizes29 to Chicago too,
        // even when the parent is subsequently cancelled without adoption.
        if(isSign()) {
            sign_team_=nba97_reorder_normalize_team(trade_screen_.team[0],trade_screen_.mode,trade_screen_.eligible[0]);
            for(unsigned p=0;p<2;++p){sign_cursors_[p]=trade_screen_.cursor[p];sign_tops_[p]=trade_screen_.top[p];}
        }
        const auto result=nba97_trade_result(&trade_screen_);
        const uint8_t child=result==2?0x24:result==3?0x23:0;
        if(!child || child!=trade_screen_.child)throw std::runtime_error("Trade selector child contract mismatch");
        trace_.log("TRADE-ROUTE","56CD0 signed result="+std::to_string(result)+" -> frontend child="+std::to_string(child));
        reorder_child_={};reorder_child_.state=child;
        reorder_child_.parent_page=trade_screen_.phase==NBA97_TRADE_SECOND;
        reorder_child_.team=trade_screen_.team[reorder_child_.parent_page];
        for(unsigned p=0;p<2;++p){reorder_child_.cursor[p]=trade_screen_.cursor[p];reorder_child_.top[p]=trade_screen_.top[p];reorder_child_.player_id[p]=trade_screen_.selected[p];}
        reorder_child_.waiting_input_change=1;reorder_child_.held_mask=trade_screen_.child==0x24?0x10:0x40;
        if(trade_screen_.child==0x24){reorder_child_.player_id[0]=trade_screen_.selected[reorder_child_.parent_page];openReorderView();}
        else openReorderCompare();
        logTrade("child opened from isolated draft");
    }
    void requestTradeChildReturn(uint16_t mask) {
        trade_child_exit_=mask;
        for(unsigned p=0;p<2;++p){trade_child_teams_[p]=trade_screen_.team[p];trade_child_slots_[p]=trade_screen_.cursor[p];}
        if(reorder_child_.state==0x24) {
            auto p=reorder_child_.parent_page;
            trade_child_teams_[p]=static_cast<int16_t>(roster_viewer_.selectedTeam(viewerDatabase())->id);
            trade_child_slots_[p]=static_cast<uint8_t>(roster_viewer_.playerIndex());
        }else for(unsigned p=0;p<2;++p){trade_child_teams_[p]=reorder_compare_.team[p];trade_child_slots_[p]=reorder_compare_.slot[p];}
        playBottomMenuSound(mask==0x80?9:10,"trade-child-return");
        if(nba97_trade_child_proposal(&trade_screen_,mask,trade_child_teams_.data(),trade_child_slots_.data()))
            openTradeChoice(reorder_child_.state==0x24?0x800AEE88:0x800AEEF6,mask);
        else completeTradeChild(false);
    }
    void completeTradeChild(bool adopt) {
        if(!nba97_trade_return_child(&trade_screen_,trade_child_exit_,trade_child_teams_.data(),trade_child_slots_.data(),adopt))
            throw std::runtime_error("invalid Trade child return");
        if(reorder_saved_viewer_){roster_viewer_=*reorder_saved_viewer_;reorder_saved_viewer_.reset();}
        reorder_child_={};reorder_child_database_.reset();compare_assets_.reset();compare_portraits_={};
        reorder_compare_={};compare_refresh_={};compare_palette_={};nba97_compare_repeat_idle(&compare_repeat_);
        cool_fact_audio_.stop();player_photo_loader_.reset();roster_portrait_loaded_=false;
        syncTrade();logTrade(adopt?"child selection adopted after confirmation; no roster mutation":"child return; parent selection retained");
        trace_.log("TRADE-UNDO-CHECKPOINT","original quirk preserved: 56494 rebases undo on child re-entry; pre-child trades retained; save not written");
    }

    void openReorder() {
        // Assets remain private; missing records are errors, never substitutes
        // from Z1PORT or rendered screenshots of the original game.
        const auto root = options_.asset_root / "menu";
        validateMenuAsset(root / "Z2PORT.IDX", 3970);
        validateMenuAsset(root / "Z2PORT.BIG", 2344626);
        if (!std::filesystem::exists(options_.asset_root / "reorder/discard.n97ui"))
            throw std::runtime_error("run tools/extract_reorder_dialogs.py for Re-order confirmation");
        reorder_labels_ = std::make_unique<nba97::ReorderLabelPreview>(options_.asset_root);
        reorder_help_pack_ = std::make_unique<nba97::FrontendHelpPack>(options_.asset_root / "reorder/help.n97ui");
        reorder_help_ = {};
        reorder_notice_.reset(); reorder_exit_after_notice_=false;
        reorder_child_ = {};
        reorder_child_database_.reset();
        reorder_saved_viewer_.reset();
        for (const char* tag : {"ba22", "ba02", "frmr", "110p", "111p", "hel1", "hel2"}) {
            auto image = load_png_image(root / "ZSET4-decoded" / (std::string(tag)+".png"));
            for (std::size_t i = 0; i < image.rgba.size(); i += 4)
                if (!image.rgba[i] && !image.rgba[i+1] && !image.rgba[i+2]) image.rgba[i+3] = 0;
            roster_sprites_[tag] = std::move(image);
        }
        const auto table = roster_database_.slotTable();
        if (!nba97_reorder_screen_enter(&reorder_screen_, table.data(), reorder_team_, 0, nullptr,
                reorder_saved_cursor_.data(), reorder_saved_top_.data(), 0))
            throw std::runtime_error("invalid Re-order entry state");
        reorder_portrait_ids_.fill(UINT16_MAX);
        reorder_tick_ = menu_elapsed_ms_ / 17;
        reorder_modal_frame_ = 0;
        loadReorderPortraits();
        trace_.log("REORDER-ENTRY", "80056AEC -> 80056494 -> 800560BC; state=0x0C layout=0x0D; "
            "snapshot=535 slots; kinds=1/2; objects=30; visible=6; frame pump=native");
        trace_.log("REORDER-LAYOUT", "ZSET4 graphics=0x0C ba22=(156,10); Z2PORT 87x51 portraits=(54/386,22); "
            "frml/frmr=(30/368,15); heading=(256,70); rows=(60/270,112+16*n); arrows=6/10");
        trace_.log("REORDER-FOOTER", "3D5F0/3D65C -> 3186C object4 at (235,217); "
            "8009B230 tags=hel1/hel2; selected Help descriptor drives original ZSET4 graphic; not controller number");
        trace_.log("REORDER-KEYS", "arrows=rows/teams; C/Space=pick; X/Esc=cancel; Enter=accept; "
            "D=View; S=Compare; F/H/F1=original Help; Accept saves a local override; original assets never overwritten");
        logReorder("constructed");
        if(!roster_load_error_.empty())
            showReorderSaveNotice("save unavailable","file needs attention","see CLI for details",false,0x800);
        else if(roster_store_ && roster_store_->needsRepair())
            showReorderSaveNotice("backup recovered","Accept repairs the save","Cancel leaves files alone",false,0x800);
    }

    void loadRosterSave() {
        if(!options_.release_capture_dir.empty()) {
            trace_.log("ROSTER-SAVE","disabled for Release capture; active save untouched");
            return;
        }
        // Existing deterministic capture/self-test modes must never read/write
        // the active save. The dedicated save harness requires explicit paths.
        if(options_.verify_reorder_save.empty() && (options_.self_test ||
           !options_.rosters_menu_capture_dir.empty() || !options_.view_rosters_capture_dir.empty() ||
           !options_.reorder_capture_dir.empty() || !options_.trade_capture_dir.empty() || !options_.sign_capture_dir.empty())) {
            trace_.log("ROSTER-SAVE","disabled for deterministic self-test/capture; active save untouched");
            return;
        }
        try {
            auto store=std::make_unique<nba97::RosterSaveStore>(options_.roster_save_path);
            const auto origin=store->load(roster_database_);
            const char* source=origin==nba97::RosterLoadOrigin::Defaults ? "defaults" :
                origin==nba97::RosterLoadOrigin::Primary ? "primary" :
                origin==nba97::RosterLoadOrigin::RecoveredMissing ? "backup (primary missing)" : "backup (primary invalid)";
            roster_store_=std::move(store);
            trace_.log("ROSTER-LOAD",std::string(source)+"; generation="+std::to_string(roster_store_->accepted().generation)+
                "; default-different="+std::to_string(roster_database_.differsFromOriginal())+
                "; repair-needed="+std::to_string(roster_store_->needsRepair())+"; "+roster_store_->path().string());
        } catch(const std::exception& e) {
            roster_store_.reset(); roster_load_error_=e.what();
            trace_.log("ROSTER-SAVE-BLOCKED",roster_load_error_+
                "; using immutable defaults read-only; no fallback overwrite; fix save or choose --roster-save then restart");
        }
    }

    void showReorderSaveNotice(const char* title,const char* line1,const char* line2,bool committed,std::uint16_t held=0x80) {
        // Native-port message, not a recovered memory-card string. Reuse the
        // original style-zero geometry/lifecycle and local ZFONT1.
        reorder_notice_=nba97::FrontendHelpDescriptor{12,0,0,{116, 70,280,100},
            {{true,0,title},{true,0,line1},{true,0,line2},{true,0,"press a button to continue"}}};
        reorder_exit_after_notice_=committed;
        const auto before=reorder_help_;
        const auto event=nba97_help_open(&reorder_help_,reorder_notice_->rect,held);
        recordNativeHelp(1,held,event,before);
        reorderHelpEvent(event);
        trace_.log("ROSTER-SAVE-NOTICE",std::string(title)+"; native message; original green modal/ZFONT1; "+
            (committed ? "committed state; continue exits without replay" : "draft retained; notice input cannot accept/cancel editor"));
    }

    void finishReorderExit() {
        if(reorder_screen_.selection.accepted) {
            std::uint8_t active=0;
            nba97_reorder_screen_save(&reorder_screen_,reorder_saved_cursor_.data(),reorder_saved_top_.data(),&active);
            reorder_team_=reorder_screen_.team;
        }
        beginFrontendTransition(nba97::FrontendPage::Rosters,"Re-order result="+
            std::to_string(nba97_reorder_screen_result(&reorder_screen_))+"; release screen-owned resources");
        reorder_labels_.reset(); reorder_help_pack_.reset(); reorder_help_={}; reorder_portraits_={};
        reorder_notice_.reset(); reorder_exit_after_notice_=false;
    }

    void loadReorderPortraits() {
        for (int p = 0; p < 2; ++p) {
            const auto id = reorder_screen_.selection.selected_ids[p];
            if (id == reorder_portrait_ids_[p] && !reorder_portraits_[p].rgba.empty()) continue;
            // 80030D14/portrait resolver: physical zero is fallback, N+1 is player N.
            const unsigned record = id == UINT16_MAX ? 0u : static_cast<unsigned>(id)+1u;
            char name[32]{}; sprintf_s(name, "player_%03u.png", record);
            auto image = load_png_image(options_.asset_root / "menu/Z2PORT-decoded" / name);
            if (image.width != 87 || image.height != 51) throw std::runtime_error("wrong Z2PORT portrait dimensions");
            reorder_portraits_[p] = std::move(image);
            reorder_portrait_ids_[p] = id;
            trace_.log("REORDER-ASSET", "column=" + std::to_string(p) + " player=" +
                std::to_string(id) + " Z2PORT record=" + std::to_string(record) + " 87x51");
        }
    }

    void logReorder(const char* event) {
        const auto& s = reorder_screen_.selection;
        Nba97ReorderMarker markers[4]{};
        nba97_reorder_screen_markers(&reorder_screen_, markers);
        const char* help_tag=nba97_reorder_screen_help_tag(&reorder_screen_);
        trace_.log("REORDER", std::string(event) + " team=" + std::to_string(reorder_screen_.team) +
            " phase=" + nba97_reorder_phase_name(s.phase) + " cursor=" + std::to_string(s.cursor[0]) +
            "/" + std::to_string(s.cursor[1]) + " top=" + std::to_string(s.top[0]) + "/" +
            std::to_string(s.top[1]) + " markers=L(up/down):" +
            std::to_string(markers[0].visible) + "/" + std::to_string(markers[2].visible) +
            ",R(up/down):" + std::to_string(markers[1].visible) + "/" +
            std::to_string(markers[3].visible) + " footer=" + (help_tag?help_tag:"invalid") +
            " selected=" + std::to_string(s.selected_ids[0]) + "/" +
            std::to_string(s.selected_ids[1]) + " changes=" + std::to_string(s.changes) +
            " row-revision=" + std::to_string(s.row_revision) +
            " visible-redraws=" + std::to_string(s.visible_redraws) +
            " present-requests=" + std::to_string(s.presentation_requests) +
            " generation="+std::to_string(roster_store_ ? roster_store_->accepted().generation : 0)+
            " default-different="+std::to_string(roster_database_.differsFromOriginal())+
            " helpers=556B0/558E0/55AF8 (draft changes are not saved until Accept)");
    }

    void prepareFrontendTitle() {
        const char* tag=nullptr; int x=0,y=0;
        const nba97::MenuSpritePack* pack=&roster_sprites_;
        if(frontend_page_==nba97::FrontendPage::TeamSelect) {tag="ba08";x=160;y=10;pack=&team_select_sprites_;}
        else if(frontend_page_==nba97::FrontendPage::UserSetup) {tag="ba39";x=155;y=10;pack=&team_select_sprites_;}
        else if(frontend_page_==nba97::FrontendPage::ReorderRosters || isRosterEditor()) {
            if(reorder_child_.state==0x24) {tag="ba41";x=40;y=18;pack=&player_sprites_;}
            else if(reorder_child_.state==0x23) {tag="ba02";x=170;y=15;}
            else {const bool trade=isRosterEditor();tag=isRelease()?"ba23":isSign()?"ba30":trade?"ba38":"ba22";x=isRelease()?140:trade&&!isSign()?155:156;y=10;}
        } else if(frontend_page_==nba97::FrontendPage::ViewRosters) {
            if(roster_viewer_.mode()==nba97::RosterViewMode::PlayerCard) {tag="ba41";x=40;y=18;pack=&player_sprites_;}
            else {tag="ba35";x=142;y=10;}
        }
        if(!tag) {frontend_title_.leave();return;}
        const auto found=pack->find(tag);
        if(found==pack->end()) throw std::runtime_error("missing original frontend title sprite");
        if(frontend_title_.select(tag,x,y,found->second.width,found->second.height)) {
            frontend_title_tick_=std::uint64_t(menu_elapsed_ms_)*30/1000;
            frontend_title_painted_=false;
            trace_.log("TITLE-MOTION",std::string("layout tag=")+tag+" source=31F48/32BF0 single textured quad; "
                "phase="+std::to_string(frontend_title_.state().next)+" rng="+std::to_string(frontend_rng_)+
                "; native nominal30Hz presentation policy; original cadence/stream not measured; no128-pixel split");
        }
    }

    void presentFrontendTitle(bool direct=false) {
        const auto seed=frontend_rng_;
        const auto phase=frontend_title_.state().next;
        const auto changed=direct ? frontend_title_.presentDirect(frontend_rng_):frontend_title_.present(frontend_rng_);
        frontend_rng_draws_+=(phase==0 ? 8 : 0)+(direct ? 0:1);
        ++frontend_title_presents_;
        if(frontend_title_presents_<=8 || frontend_title_presents_%30==0 || native_record_) {
            std::string vertices;
            for(int i=0;i<8;++i) {if(i) vertices+=",";vertices+=std::to_string(frontend_title_.corners()[i]);}
            trace_.log("TITLE-PRESENT","n="+std::to_string(frontend_title_presents_)+" changed="+
                std::to_string(changed)+" phase="+std::to_string(phase)+" rng="+std::to_string(seed)+"->"+
                std::to_string(frontend_rng_)+" shared-draws="+std::to_string(frontend_rng_draws_)+" xy="+vertices+
                "; original RNG algorithm shared with Cool Facts; original runtime seed/history unverified");
        }
    }

    void updateFrontendTitle() {
        prepareFrontendTitle();
        if(frontend_page_==nba97::FrontendPage::TeamSelect ||
           frontend_page_==nba97::FrontendPage::UserSetup) return; // Screen-specific frame owners.
        const auto tick=std::uint64_t(menu_elapsed_ms_)*30/1000;
        if(frontend_title_.active() && frontend_title_painted_ && tick>frontend_title_tick_) {
            presentFrontendTitle();
            frontend_title_painted_=false; // No skipping unpainted motion after a stall.
        }
        frontend_title_tick_=tick;
    }

    PshImage renderReorder() {
        prepareFrontendTitle();
        auto image = reorder_child_.state == 0x24 ?
            nba97::renderRosterViewer(roster_viewer_, viewerDatabase(), menu_font_, player_sprites_,
                menu_elapsed_ms_, roster_portrait_loaded_ ? &roster_portrait_ : nullptr,
                roster_cool_facts_available_, &control_font_,
                menu_elapsed_ms_ < stat_flash_until_ms_ ? stat_flash_direction_ : 0, nba97_fact_flash_visible(&cool_fact_flash_),
                player_photo_loader_.state().city_enabled != 0, frontend_title_.corners()) :
            reorder_child_.state == 0x23 ?
            nba97::renderCompareScreen(compare_refresh_,viewerDatabase(),*compare_assets_,roster_sprites_,
                reorder_labels_->smallFont(),compare_portraits_,menu_elapsed_ms_,*compare_backgrounds_,compare_palette_,compare_arrows_,frontend_title_.corners()) :
            nba97::renderReorderScreen(reorder_screen_, roster_sprites_, menu_font_,
            reorder_portraits_, reorder_labels_->renderFeedback(reorder_screen_.selection,
                roster_database_, static_cast<std::uint16_t>(reorder_screen_.team),
                reorder_modal_frame_, reorder_discard_yes_), menu_elapsed_ms_,frontend_title_.corners());
        if (nba97_help_visible(&reorder_help_))
            reorder_help_pack_->draw(image, reorder_labels_->smallFont(),
                reorder_notice_ ? *reorder_notice_ : reorder_help_pack_->descriptor(reorder_help_state_, reorder_help_index_), reorder_help_);
        drawPlayerNotice(image);
        return image;
    }

    const nba97::RosterDatabase& viewerDatabase() const {
        return reorder_child_database_ ? *reorder_child_database_ : roster_database_;
    }

    void openReorderView() {
        const bool trade=isRosterEditor();
        const auto event = trade?NBA97_REORDER_REQUEST_VIEW:nba97_reorder_child_begin(&reorder_screen_, &reorder_child_, 0x10);
        if (event == NBA97_REORDER_REJECTED_EMPTY) {
            reorder_modal_frame_ = 0;
            playBottomMenuSound(5, "reorder-view-empty");
            logReorder("view-empty");
            return;
        }
        if (event != NBA97_REORDER_REQUEST_VIEW) return;
        auto draft = trade?tradeDraft():std::make_unique<const nba97::RosterDatabase>(roster_database_.draftView(reorder_screen_));
        nba97::RosterViewer child;
        const auto p = reorder_child_.parent_page;
        if (!child.openPlayerFromRoster(*draft, {reorder_child_.cursor[p], reorder_child_.top[p], reorder_child_.team},
                {0, false, static_cast<std::int16_t>(trade?editorState():0x0c), p},
                (isRelease()||isSign())?trade_assets_->freeAgentName():std::string{}))
            throw std::runtime_error("Re-order child identity could not be resolved from draft");
        if (child.selectedPlayer(*draft)->id != reorder_child_.player_id[0])
            throw std::runtime_error("Re-order draft/child selected identity mismatch");
        reorder_saved_viewer_ = roster_viewer_;
        reorder_child_database_ = std::move(draft);
        roster_viewer_ = child;
        cool_fact_selection_={};cool_fact_selection_.selected=-1;
        cool_fact_audio_.stop();
        stat_flash_until_ms_ = 0;
        loadSelectedPlayerCardAssets();
        trace_.log(trade?"TRADE-CHILD":"REORDER-CHILD", std::string(trade?"push state=0x24 from=0x0D via 8005A074/8005A538; page=":"push state=0x24 from=0x0C via 8005A074/8005A538; page=") +
            std::to_string(p) + " player=" + std::to_string(reorder_child_.player_id[0]) +
            " slot=" + std::to_string(reorder_child_.cursor[p]) + "; immutable player catalogue=shared; "
            "roster slots=1070 bytes; parent context=" + std::to_string(sizeof(reorder_child_)) +
            " bytes; draft publication=no; D/S=stop fact, C/Space=play, Enter/X/Esc=return, F=Help");
        if(trade)logTrade("view-parent-suspended");else logReorder("view-parent-suspended");
        logRosterViewFocus("draft child entry");
    }

    void returnReorderView(std::uint16_t mask) {
        // FUN_8003D930 exit: accepted Start=9, cancelled Circle=10.
        playBottomMenuSound(mask == 0x80 ? 9 : 10, "view-return");
        cool_fact_audio_.stop();
        roster_viewer_.returnToRoster();
        if (!nba97_reorder_child_return(&reorder_screen_, &reorder_child_, mask) || !reorder_saved_viewer_)
            throw std::runtime_error("invalid Re-order child return");
        roster_viewer_ = *reorder_saved_viewer_;
        reorder_saved_viewer_.reset();
        reorder_child_database_.reset();
        player_photo_loader_.reset();
        roster_portrait_loaded_ = false;
        stat_flash_until_ms_ = 0;
        trace_.log("REORDER-CHILD", "pop 0x24 -> 0x0C; browsed child selection discarded (writeback only for Trade parent13); "
            "speech stopped; parent draft/cursor/top retained; exit mask=" + std::to_string(mask) + "; saved=no");
        logReorder("view-parent-resumed");
    }

    void logCompare(const char* event) {
        const auto& s=reorder_compare_;
        trace_.log("COMPARE",std::string(event)+" active="+std::to_string(s.active_side)+
            " teams="+std::to_string(s.team[0])+"/"+std::to_string(s.team[1])+
            " slots="+std::to_string(s.slot[0])+"/"+std::to_string(s.slot[1])+
            " players="+std::to_string(s.player[0])+"/"+std::to_string(s.player[1])+
            " layer="+std::to_string(s.layer)+" top="+std::to_string(s.top)+
            "; draft read-only; visible=5; label-object=27 (not audio)");
    }

    void loadComparePortraits() {
        for(unsigned side=0;side<2;++side) {
            const auto id=reorder_compare_.player[side];
            if(compare_portrait_ids_[side]==id && !compare_portraits_[side].rgba.empty()) continue;
            char name[32]{}; sprintf_s(name,"player_%03u.png",static_cast<unsigned>(id)+1u);
            auto portrait=load_png_image(options_.asset_root/"menu/Z2PORT-decoded"/name);
            if(portrait.width!=87 || portrait.height!=51) throw std::runtime_error("wrong Compare portrait archive");
            compare_portraits_[side]=std::move(portrait); compare_portrait_ids_[side]=id;
            trace_.log("COMPARE-ASSET","side="+std::to_string(side)+" player="+std::to_string(id)+
                " Z2PORT record="+std::to_string(static_cast<unsigned>(id)+1)+" 87x51; no Z1PORT/cool-fact substitution");
        }
    }

    void openReorderCompare() {
        const bool trade=isRosterEditor();
        const auto event=trade?NBA97_REORDER_REQUEST_COMPARE:nba97_reorder_child_begin(&reorder_screen_,&reorder_child_,0x40);
        if(event==NBA97_REORDER_REJECTED_EMPTY) {
            reorder_modal_frame_=0; playBottomMenuSound(5,"reorder-compare-empty"); logReorder("compare-empty"); return;
        }
        if(event!=NBA97_REORDER_REQUEST_COMPARE) return;
        auto draft=trade?tradeDraft():std::make_unique<const nba97::RosterDatabase>(roster_database_.draftView(reorder_screen_));
        const auto table=draft->slotTable();
        //8005A17C..5A198: parent16/17 do NOT increment source side. Release
        //Compare starts BOTH identities on the donor, not its empty receiver.
        const int16_t compare_teams[2]={trade_screen_.team[0],isRelease()?trade_screen_.team[0]:trade_screen_.team[1]};
        const uint8_t compare_slots[2]={trade_screen_.cursor[0],isRelease()?trade_screen_.cursor[0]:trade_screen_.cursor[1]};
        if(!(trade?nba97_compare_begin_teams(&reorder_compare_,compare_teams,compare_slots,table.data()):
             nba97_compare_begin(&reorder_compare_,&reorder_child_,table.data()))) throw std::runtime_error("Compare draft identity mismatch");
        if(!nba97_compare_refresh_begin(&compare_refresh_,&reorder_compare_)) throw std::runtime_error("Compare refresh entry failed");
        compare_refresh_painted_=false;
        nba97_compare_repeat_idle(&compare_repeat_);
        compare_arrows_={};
        // The reduced scroll continuation is valid only for fully collapsed
        // incoming glyphs; fail explicitly instead of silently truncating a
        // future replacement font that requires the general clipping kernel.
        for(unsigned code=0;code<256;++code) {
            const auto* glyph=reorder_labels_->smallFont().glyph(static_cast<char>(code));
            if(glyph && glyph->height>14) throw std::runtime_error("Compare scroll requires original glyph height <=14");
        }
        for(auto& arrow:compare_arrows_) {
            std::fill_n(arrow.start,3,std::uint8_t{128});std::fill_n(arrow.rgb,3,std::uint8_t{128});
        }
        compare_assets_=std::make_unique<nba97::CompareAssets>(options_.asset_root/"reorder/compare.n97ui");
        if(!compare_backgrounds_) compare_backgrounds_=std::make_unique<nba97::FrontendPaletteAssets>(
            options_.asset_root/"menu/ZSET4-team-backgrounds/indexed.n97pal");
        if(!nba97_frontend_palette_begin(&compare_palette_,compare_backgrounds_->bank(),33,
            reorder_compare_.team[0],reorder_compare_.team[1])) throw std::runtime_error("Compare palette entry failed");
        reorder_child_database_=std::move(draft);
        cool_fact_audio_.stop(); compare_portrait_ids_.fill(UINT16_MAX); loadComparePortraits();
        trace_.log(trade?"TRADE-CHILD":"REORDER-CHILD",std::string(trade?"push state=0x23 from=0x0D via 8005A074/8005A880; parent-page=":"push state=0x23 from=0x0C via 8005A074/8005A880; parent-page=")+
            std::to_string(reorder_child_.parent_page)+"; state=12 bytes; shared player catalogue; no draft publication");
        trace_.log("COMPARE-LAYOUT","graphics 800978C4: ba02=(170,15), Z2PORT=(54/386,22); ZFONT1 selector1; "
            "labels x256, values x128/384; y82/92/102; layer116; five rows135+14*n; team names212; independent team palette halves");
        trace_.log("COMPARE-KEYS","C/Space=active side; Left/Right=player; Up/Down=both stat lists; J/K=team; Q/E=layer; F=Help; Enter/X/Esc=return; no cool facts");
        logCompare("entered");
        trace_.log("COMPARE-PALETTE","source=8002FE58/8002FF40/8002FF80; raw indexed pack=134356 bytes; "
            "state="+std::to_string(sizeof(compare_palette_))+" bytes; two independent halves; 160 dynamic/96 fixed colors; factors=0..16");
    }

    void advanceComparePalette() {
        if(reorder_child_.state!=0x23) return;
        for(unsigned i=0;i<compare_arrows_.size();++i) {
            auto& arrow=compare_arrows_[i];const auto before=arrow.flags;
            nba97_reorder_tint_tick(&arrow);
            if((before & 0xc0)!=(arrow.flags & 0xc0))
                trace_.log("COMPARE-ARROW","index="+std::to_string(i)+" phase="+std::to_string(arrow.flags & 0xc0)+
                    " rgb="+std::to_string(arrow.rgb[0])+","+std::to_string(arrow.rgb[1])+","+std::to_string(arrow.rgb[2])+
                    "; source=2AE5C; update while hidden/Help too");
        }
        const int changed=nba97_frontend_palette_tick(&compare_palette_,compare_backgrounds_->bank(),33);
        if(changed<0) throw std::runtime_error("invalid Compare palette update");
        for(unsigned side=0;side<2;++side) if(changed&(1<<side)) {
            const auto& half=compare_palette_.half[side];
            if(half.next_factor==1 || half.next_factor==9 || half.next_factor==17)
                trace_.log("COMPARE-PALETTE","side="+std::to_string(side)+" target="+std::to_string(half.target)+
                    " applied-factor="+std::to_string(half.next_factor-1)+
                    (half.next_factor==17 ? " settled" : " fading")+"; logical tick, original wall-clock parity unverified");
        }
    }

    void advanceCompareRefresh() {
        const bool scrolling=compare_refresh_.cue>=3;
        const auto scroll_cue=compare_refresh_.cue;
        const int cue=nba97_compare_refresh_presented(&compare_refresh_,&reorder_compare_);
        if(cue<0) throw std::runtime_error("Compare refresh continuation invalid");
        if(scrolling) {
            if(compare_refresh_.remaining==1) {
                //3A224 rebuilds the primary marker pair after group0's pump.
                for(unsigned index=4;index<6;++index) {
                    compare_arrows_[index]={};
                    std::fill_n(compare_arrows_[index].start,3,std::uint8_t{128});
                    std::fill_n(compare_arrows_[index].rgb,3,std::uint8_t{128});
                }
                const bool visible=scroll_cue==3 ? reorder_compare_.top!=0 :
                    reorder_compare_.top+5u<nba97_compare_stat_count(&reorder_compare_);
                if(visible) nba97_reorder_tint_flash(&compare_arrows_[scroll_cue+1]);
            }
            // Build the next presentation: group0 moves before group1.
            advanceComparePalette();
            trace_.log("COMPARE-SCROLL","completed-present="+std::to_string(2-compare_refresh_.remaining)+
                " visible-top="+std::to_string(nba97_compare_refresh_top(&compare_refresh_,0))+"/"+
                std::to_string(nba97_compare_refresh_top(&compare_refresh_,1))+" cue="+std::to_string(cue)+
                "; 3AB64 group0 then1; clip14/translate14 duration1; outgoing lifetime0");
            if(cue) playBottomMenuSound(cue,"compare-input");
            return;
        }
        if(compare_refresh_.remaining==1) {
            // 39574 builds the first frame BEFORE 310D8 notices the new ID.
            // Native local I/O is ready here; original CD completion is separate.
            loadComparePortraits();
            advanceComparePalette();
        }
        trace_.log("COMPARE-REFRESH","completed-present="+std::to_string(2-compare_refresh_.remaining)+
            " text="+std::to_string(compare_refresh_.text.player[0])+"/"+std::to_string(compare_refresh_.text.player[1])+
            " requested="+std::to_string(reorder_compare_.player[0])+"/"+std::to_string(reorder_compare_.player[1])+
            " cue="+std::to_string(cue)+(cue ? "; 59808 text refreshed, callback returns" : "; text retained, local portrait ready"));
        if(cue) {
            playBottomMenuSound(cue,"compare-input"); // Selector dispatch follows callback return.
            const unsigned index=reorder_compare_.active_side*2+(cue==1);
            const auto before=compare_arrows_[index];
            nba97_reorder_tint_flash(&compare_arrows_[index]);
            trace_.log("COMPARE-ARROW","trigger index="+std::to_string(index)+" cue="+std::to_string(cue)+
                " prior-phase="+std::to_string(before.flags & 0xc0)+" elapsed="+std::to_string(compare_arrows_[index].elapsed)+
                "; 3D534 ->2ADEC after callback/sound; colors from original ZFONT1 glyph");
        }
    }

    static std::uint16_t compareKeyMask(WPARAM key) {
        switch(key) {
        case VK_UP:return 1; case VK_DOWN:return 2;
        case VK_LEFT:return 8; case VK_RIGHT:return 4;
        case 'J':return 0x200; case 'K':return 0x400;
        case 'Q':return 0x1000; case 'E':return 0x2000;
        case 'C':case VK_SPACE:return 0x800;
        case 'F':case 'H':case VK_F1:return 0x20;
        case VK_RETURN:return 0x80;
        case VK_ESCAPE:case VK_BACK:case 'X':return 0x100;
        case 'D':return 0x10; case 'S':return 0x40;
        default:return 0;
        }
    }

    std::uint16_t sampleCompareInput() const {
        // Host controller0 only. Do not navigate a background/minimized app.
        if(GetForegroundWindow()!=window_ || IsIconic(window_)) return 0;
        std::uint16_t mask=0;
        for(unsigned key=0;key<256;++key)
            if(compareKeyMask(key) && (GetAsyncKeyState(key)&0x8000)) mask|=compareKeyMask(key);
        return mask; // Preserve chords; never prioritize Right over another button.
    }

    void advanceComparePostCycle(std::uint16_t held) {
        advanceComparePalette();
        const bool completed_cycle=compare_repeat_.previous_mask!=0;
        const auto ready=nba97_compare_repeat_presented(&compare_repeat_);
        if(ready<0) throw std::runtime_error("invalid Compare post-cycle pacing");
        if(!ready) return;
        if(completed_cycle && nba97_compare_callback_mask(compare_repeat_.previous_mask))
            trace_.log("COMPARE-CALLBACK","delay/poll complete; held="+std::to_string(held)+
                " mask="+std::to_string(compare_repeat_.previous_mask)+" counter="+std::to_string(compare_repeat_.counter));
        else if(completed_cycle && (compare_repeat_.previous_mask==1 || compare_repeat_.previous_mask==2))
            trace_.log("COMPARE-SCROLL","delay/poll complete; held="+std::to_string(held)+
                "; source=5A1EC/3D930; null top-Up callback polls only; dispatched geometry completes within delay3");
        else if(completed_cycle) trace_.log("COMPARE-PACING","post-delay and input-poll frame complete; held="+std::to_string(held)+
            " counter="+std::to_string(compare_repeat_.counter)+"; source=3AE4C/3E38C; selected-text wait=0 (2C244 mask=C7); held-repeat=enabled");
        if(held==1 || held==2 || held==4 || held==8 || nba97_compare_callback_mask(held)) { handleCompareInput(held); return; }
        nba97_compare_repeat_idle(&compare_repeat_);
        if(held) {
            // A direction chord is not a directional action. Keep polling if
            // either direction stays down, so releasing its partner resumes it.
            if(held!=1 && held!=2 && held!=0x20 && held!=0x80 && held!=0x100) { compare_repeat_.post_frames=1; return; }
            handleCompareInput(held);
        }
    }

    void handleCompareKey(WPARAM key) {
        handleCompareInput(compareKeyMask(key));
    }

    void handleCompareInput(std::uint16_t mask) {
        if(reorder_child_.waiting_input_change || compare_refresh_.remaining || compare_repeat_.post_frames) return;
        if(mask==0x20) { openReorderHelp(); return; }
        if(mask==0x80 || mask==0x100) {
            if(isRosterEditor()){requestTradeChildReturn(mask);return;}
            playBottomMenuSound(mask==0x80 ? 9 : 10,"compare-return");
            if(!nba97_reorder_child_return(&reorder_screen_,&reorder_child_,mask))
                throw std::runtime_error("Compare return context invalid");
            reorder_child_database_.reset(); compare_assets_.reset(); compare_portraits_={}; reorder_compare_={}; compare_palette_={}; compare_refresh_={};
            nba97_compare_repeat_idle(&compare_repeat_);
            trace_.log("REORDER-CHILD","pop 0x23 -> 0x0C; browsed identities discarded; parent draft/cursors/top retained; saved=no");
            logReorder("compare-parent-resumed"); return;
        }
        const auto table=viewerDatabase().slotTable();
        const auto before=reorder_compare_;
        const auto event=nba97_compare_refresh_input(&reorder_compare_,&compare_refresh_,table.data(),mask);
        if(event==NBA97_COMPARE_INVALID) throw std::runtime_error("Compare navigation rejected stale state");
        if(mask==4 || mask==8) {
            const int delay=nba97_compare_repeat_request(&compare_repeat_,mask);
            if(!delay) throw std::runtime_error("Compare repeat request rejected");
            compare_refresh_painted_=false; compare_refresh_tick_=menu_elapsed_ms_/17;
            trace_.log("COMPARE-PACING","mask="+std::to_string(mask)+" counter="+std::to_string(compare_repeat_.counter)+
                " post-delay="+std::to_string(delay)+" poll=1; normal counter-record-passes=2; counted after player callback, not Windows autorepeat");
        } else if(mask==1 || mask==2) {
            const int post=nba97_compare_scroll_request(&compare_repeat_,&before,mask);
            if(!post)
                throw std::runtime_error("Compare scroll wait rejected");
            compare_refresh_painted_=false;compare_refresh_tick_=menu_elapsed_ms_/17;
            trace_.log("COMPARE-SCROLL","mask="+std::to_string(mask)+" top="+std::to_string(reorder_compare_.top)+
                " callback-presents="+std::to_string(compare_refresh_.remaining)+
                " delay="+std::to_string(post-1)+" poll=1; dispatch="+(post==1?"null-up-5A1EC":"3AB64")+
                "; endpoint silent; normal two-pass input history");
        } else if(nba97_compare_callback_mask(mask)) {
            if(nba97_compare_callback_request(&compare_repeat_,mask)!=5)
                throw std::runtime_error("Compare callback wait rejected");
            compare_refresh_painted_=false;compare_refresh_tick_=menu_elapsed_ms_/17;
            trace_.log("COMPARE-CALLBACK","mask="+std::to_string(mask)+" event="+std::to_string(event)+
                " delay=5 poll=1; 3E388 after59F20; callback no-op still waits; input blocked until completed presents");
        } else if(mask) nba97_compare_repeat_idle(&compare_repeat_);
        for(unsigned side=0;side<2;++side) if(compare_palette_.half[side].target!=reorder_compare_.team[side]) {
            const auto prior=compare_palette_.half[side];
            if(!nba97_frontend_palette_request(&compare_palette_,side,reorder_compare_.team[side],33))
                throw std::runtime_error("Compare palette request failed");
            trace_.log("COMPARE-PALETTE","request side="+std::to_string(side)+" from-target="+
                std::to_string(prior.target)+" to="+std::to_string(reorder_compare_.team[side])+
                " snapshot=current; interrupted="+std::to_string(prior.next_factor<17));
        }
        if(event==NBA97_COMPARE_PLAYER) {
            compare_refresh_painted_=false; compare_refresh_tick_=menu_elapsed_ms_/17;
            advanceComparePalette(); // First 39574 iteration precedes the first pending frame.
            trace_.log("COMPARE-REFRESH","59928 requested player="+std::to_string(reorder_compare_.player[reorder_compare_.active_side])+
                " side="+std::to_string(reorder_compare_.active_side)+
                "; two completed presentations before 59808 text refresh and selector sound; input blocked; CD latency not emulated");
        } else if(event==NBA97_COMPARE_SCROLL) advanceComparePalette();
        else if(event!=NBA97_COMPARE_NO_CHANGE) loadComparePortraits();
        // Generic selector 3D930: up/down=3/4, left/right=2/1, callback=6.
        // Top-Up has no callback; bottom-Down clears the latch: neither sounds.
        if(event!=NBA97_COMPARE_NO_CHANGE && event!=NBA97_COMPARE_PLAYER && event!=NBA97_COMPARE_SCROLL)
            playBottomMenuSound(mask==1 ? 3 : mask==2 ? 4 : mask==8 ? 2 : mask==4 ? 1 : 6,"compare-input");
        logCompare(event==NBA97_COMPARE_NO_CHANGE ? "input-no-change" : "navigation");
    }

    void reorderHelpEvent(Nba97HelpEvent event) {
        if (event == NBA97_HELP_OPEN_SOUND) playBottomMenuSound(7, "reorder-help-open");
        else if (event == NBA97_HELP_CLOSE_SOUND) {
            playBottomMenuSound(8, "reorder-help-close");
            trace_.log("REORDER-HELP", "text removed; shrinking via 800309DC; sound=8; parent input blocked");
        } else if (event == NBA97_HELP_RETURNED) {
            if(reorder_notice_) {
                const bool finish=reorder_exit_after_notice_;
                reorder_notice_.reset(); reorder_exit_after_notice_=false;
                trace_.log("ROSTER-SAVE-NOTICE",finish ? "committed notice closed; returning to Rosters" : "notice closed; draft retained; retry/discard available");
                if(isRosterEditor()) {
                    nba97_trade_dismiss_notice(&trade_screen_,reorder_help_.held);
                    if(finish){
                        const bool accepted=nba97_trade_result(&trade_screen_)==1;
                        playBottomMenuSound(accepted?9:10,"trade-committed-notice-exit");finishTrade(accepted);
                    }
                } else if(finish) finishReorderExit();
                return;
            }
            if(isRosterEditor()){trade_screen_.latch=0;logTrade("Help returned");return;}
            reorder_screen_.selection.input_latch = 0;
            trace_.log("REORDER-HELP", "returned after input-change barrier; draft/cursors/scroll preserved; saved=no");
            logReorder("help-return");
        }
    }

    void stepReorderHelp(std::uint16_t raw,bool tick) {
        const auto before=reorder_help_;
        const auto event=tick?nba97_help_tick(&reorder_help_,raw):nba97_help_input(&reorder_help_,raw);
        recordNativeHelp(tick?3:2,raw,event,before);
        reorderHelpEvent(event);
    }

    void openReorderHelp() {
        const bool trade=isRosterEditor();
        reorder_help_state_ = reorder_child_.state ? reorder_child_.state : trade?editorState():12;
        reorder_help_index_ = reorder_child_.state ? 0 : trade?(trade_screen_.phase==NBA97_TRADE_SECOND):reorder_screen_.selection.descriptor_page;
        const auto& d = reorder_help_pack_->descriptor(reorder_help_state_, reorder_help_index_);
        const auto before=reorder_help_;
        const auto event = nba97_help_open(&reorder_help_, d.rect, 0x20);
        recordNativeHelp(1,0x20,event,before);
        reorderHelpEvent(event);
        char route[64]{};
        sprintf_s(route, "state=0x%02X descriptor=0x%08X", unsigned(reorder_help_state_), unsigned(d.address));
        trace_.log(isRelease()?"RELEASE-HELP":trade?"TRADE-HELP":"REORDER-HELP", "40FCC -> 40A1C index=" + std::to_string(reorder_help_index_) +
            " " + route + " rect=" + std::to_string(d.rect.x) + "," +
            std::to_string(d.rect.y) + "," + std::to_string(d.rect.width) + "," + std::to_string(d.rect.height) +
            " lines=" + std::to_string(d.lines.size()) + " sound=7; private Help/font pack; input barrier active");
        if(trade)logTrade("help-open");else logReorder("help-open");
    }

    void updateReorder() {
        if (frontend_page_ != nba97::FrontendPage::ReorderRosters) return;
        const auto tick = menu_elapsed_ms_ / 17;
        // Bound catch-up work after debugger pauses without freezing rendering.
        const auto first = tick > 120 ? (std::max)(reorder_tick_, tick-120) : reorder_tick_;
        const std::uint16_t held = (GetAsyncKeyState('X') & 0x8000) ||
            (GetAsyncKeyState(VK_ESCAPE) & 0x8000) ? 0x100 : 0;
        for (auto frame = first; frame < tick; ++frame) {
            const std::uint16_t raw = held ? held :
                ((GetAsyncKeyState('C') | GetAsyncKeyState(VK_SPACE)) & 0x8000) ? 0x800 :
                (GetAsyncKeyState(VK_RETURN) & 0x8000) ? 0x80 :
                (GetAsyncKeyState(VK_UP) & 0x8000) ? 1 :
                (GetAsyncKeyState(VK_DOWN) & 0x8000) ? 2 :
                (GetAsyncKeyState(VK_LEFT) & 0x8000) ? 8 :
                (GetAsyncKeyState(VK_RIGHT) & 0x8000) ? 4 :
                ((GetAsyncKeyState('F') | GetAsyncKeyState('H') | GetAsyncKeyState(VK_F1)) & 0x8000) ? 0x20 :
                (GetAsyncKeyState('D') & 0x8000) ? 0x10 : (GetAsyncKeyState('S') & 0x8000) ? 0x40 : 0;
            if(reorder_notice_) { /* Freeze editor byte-for-byte behind save notice. */ }
            else if (reorder_child_.state) nba97_reorder_child_input_ready(&reorder_child_, raw);
            else nba97_reorder_frame(&reorder_screen_.selection, raw);
            advanceComparePalette(); // Original frame pump also runs beneath Help.
            stepReorderHelp(raw,true);
            if(frontend_page_!=nba97::FrontendPage::ReorderRosters) break;
            if (reorder_modal_frame_ < 32) ++reorder_modal_frame_;
        }
        reorder_tick_ = tick;
    }

    void handleReorderKey(WPARAM key) {
        if(cool_fact_flash_.remaining) return; // 59E14 has not returned to the selector.
        if(player_notice_.phase!=NBA97_HELP_CLOSED) {
            playerNoticeEvent(nba97_help_input(&player_notice_,playerNoticeKeyMask(key)));
            return; // The dismissing key never reaches the child/parent.
        }
        auto& s = reorder_screen_.selection;
        if (reorder_help_.phase != NBA97_HELP_CLOSED) {
            const std::uint16_t raw = key == VK_ESCAPE || key == 'X' ? 0x100 :
                key == VK_RETURN ? 0x80 : key == VK_UP ? 1 : key == VK_DOWN ? 2 :
                key == VK_LEFT ? 8 : key == VK_RIGHT ? 4 :
                key == 'F' || key == 'H' || key == VK_F1 ? 0x20 :
                key == 'D' ? 0x10 : key == 'S' ? 0x40 : 0x800;
            stepReorderHelp(raw,false);
            return; // The dismissing key must never reach the editor.
        }
        if (s.waiting_input_change) return;
        if(reorder_child_.state==0x23) { handleCompareKey(key); rebuildMenuFrame(); return; }
        if (reorder_child_.state == 0x24) {
            if (reorder_child_.waiting_input_change) return;
            if (key == 'F' || key == 'H' || key == VK_F1) openReorderHelp();
            else if (key == VK_RETURN || key == VK_ESCAPE || key == VK_BACK || key == 'X')
                returnReorderView(key == VK_RETURN ? 0x80 : 0x100);
            else handleRosterViewKey(key == 'C' ? VK_SPACE : key == 'D' ? 'S' : key);
            rebuildMenuFrame();
            return;
        }
        if (s.modal) {
            if (reorder_modal_frame_ < 24) return;
            nba97_reorder_dismiss_modal(&s);
            // Sample the acknowledging press even if it fell between timers.
            s.held_mask = key == VK_ESCAPE || key == 'X' ? 0x100 :
                key == VK_RETURN ? 0x80 : key == VK_UP ? 1 : key == VK_DOWN ? 2 :
                key == VK_LEFT ? 8 : key == VK_RIGHT ? 4 : 0x800;
            playBottomMenuSound(8, "reorder-message-close");
            return;
        }
        const auto before_input=reorder_screen_;
        Nba97ReorderEvent event = NBA97_REORDER_NO_CHANGE;
        std::uint32_t sound = 0;
        if (s.phase == NBA97_REORDER_DISCARD_PROMPT) {
            if (key == VK_UP || key == VK_DOWN) {
                reorder_discard_yes_ = key == VK_UP;
                playBottomMenuSound(key == VK_UP ? 2 : 1, "reorder-confirm-choice");
                return;
            }
            if (key == 'X' || key == VK_ESCAPE) event = nba97_reorder_screen_input(&reorder_screen_, NBA97_REORDER_DISCARD_NO);
            else if (key == 'C' || key == VK_SPACE || key == VK_RETURN)
                event = nba97_reorder_screen_input(&reorder_screen_, reorder_discard_yes_ ? NBA97_REORDER_DISCARD_YES : NBA97_REORDER_DISCARD_NO);
        } else if (key == VK_LEFT || key == VK_RIGHT) {
            const bool changed = nba97_reorder_screen_scan(&reorder_screen_, key == VK_LEFT ? -1 : 1) != 0;
            if (changed) { sound = key == VK_LEFT ? 3 : 4; event = NBA97_REORDER_MOVED; }
            else trace_.log("REORDER-GATE", "team scan ignored while replacement is active or no eligible team");
        } else if (key == VK_UP || key == VK_DOWN) {
            event = nba97_reorder_screen_input(&reorder_screen_, key == VK_UP ? NBA97_REORDER_UP : NBA97_REORDER_DOWN);
            if (event == NBA97_REORDER_MOVED) sound = key == VK_UP ? 2 : 1;
        } else if (key == 'C' || key == VK_SPACE) {
            event = nba97_reorder_screen_input(&reorder_screen_, NBA97_REORDER_SELECT); sound = 6;
        } else if (key == 'X' || key == VK_ESCAPE) {
            event = nba97_reorder_screen_input(&reorder_screen_, NBA97_REORDER_CANCEL); sound = 8;
        } else if (key == VK_RETURN) {
            event = nba97_reorder_screen_input(&reorder_screen_, NBA97_REORDER_ACCEPT); sound = 6;
        } else if (key == 'F' || key == 'H' || key == VK_F1) {
            openReorderHelp();
            rebuildMenuFrame();
            return;
        } else if (key == 'D') {
            openReorderView();
            rebuildMenuFrame();
            return;
        } else if (key == 'S') {
            openReorderCompare();
            rebuildMenuFrame();
            return;
        }
        if (event == NBA97_REORDER_ASK_DISCARD || s.modal) {
            reorder_modal_frame_ = 0; reorder_discard_yes_ = false; sound = 5;
        }
        if (sound) playBottomMenuSound(sound, "reorder-input");
        logReorder(event==NBA97_REORDER_EXIT_ACCEPTED ? "accept-request (not yet saved)" : nba97_reorder_event_name(event));
        if (s.phase == NBA97_REORDER_CLOSED) {
            if (s.accepted) {
                nba97::RosterCommitResult saved;
                try {
                    if(!roster_store_) throw std::runtime_error("save store unavailable; inspect ROSTER-SAVE-BLOCKED and restart");
                    auto candidate=roster_database_;
                    if(!candidate.applyReorderScreen(reorder_screen_))
                        throw std::runtime_error("Re-order baseline/membership changed; refused publication");
                    saved=roster_store_->commit(roster_database_,candidate.slotTable(),reorder_save_hooks_);
                } catch(const std::exception& e) {
                    reorder_screen_=before_input;
                    trace_.log("REORDER-SAVE-FAILED",std::string(e.what())+"; editor phase/cursors/top/draft restored; accepted memory unchanged");
                    showReorderSaveNotice("save failed","draft kept - not accepted","see CLI then retry or cancel",false);
                    rebuildMenuFrame(); return;
                }
                trace_.log("REORDER-COMMIT",std::string(saved.changed ? "saved" : "no-op; no file rewritten")+
                    "; generation="+std::to_string(saved.generation)+"; bytes="+std::to_string(saved.bytes)+
                    "; sync-completed="+std::to_string(saved.sync_completed)+
                    "; default-different="+std::to_string(roster_database_.differsFromOriginal())+
                    "; "+roster_store_->path().string());
                if(!saved.sync_completed) {
                    showReorderSaveNotice("saved - sync uncertain","do not retry this edit","see CLI for details",true);
                    rebuildMenuFrame(); return;
                }
            } else trace_.log("REORDER-DISCARD", "entire entry snapshot preserved; no live database writes");
            finishReorderExit();
            return;
        }
        loadReorderPortraits();
        rebuildMenuFrame();
    }

    int verifyReorderSave() {
        const auto& scenario=options_.verify_reorder_save;
        if(scenario=="release-seed" || scenario=="release-probe") return verifyReleaseResetSetup();
        if(scenario.rfind("reset-",0)==0) return verifyRosterReset();
        if(scenario!="save" && scenario!="reload" && scenario!="cancel" && scenario!="failure" &&
           scenario!="blocked" && scenario!="repair" && scenario!="noop" && scenario!="postcommit" &&
           scenario!="replacement" && scenario!="failure-cancel" && scenario!="children")
            throw std::runtime_error("unknown Re-order save verification case");
        frontend_page_=nba97::FrontendPage::ReorderRosters; openReorder();
        const auto original_live=roster_database_.slotTable();
        const auto generation=roster_store_ ? roster_store_->accepted().generation : 0;
        const auto capture=[&](const char* name) {
            writePpm(renderReorder(),options_.reorder_save_capture_dir/(std::string(name)+".ppm"));
        };
        const auto tickNotice=[&] {
            for(int i=0;i<40 && reorder_help_.phase!=NBA97_HELP_CLOSED;++i)
                reorderHelpEvent(nba97_help_tick(&reorder_help_,0));
        };
        const auto dismiss=[&] {
            tickNotice(); handleReorderKey(VK_RETURN); tickNotice();
            if(reorder_help_.phase!=NBA97_HELP_CLOSED) throw std::runtime_error("save notice did not close");
        };
        if(reorder_notice_) {tickNotice();capture("load-notice");dismiss();}
        capture("entry");
        if(scenario=="replacement") {
            handleReorderKey('C');
            const auto selected=reorder_screen_;
            handleReorderKey(VK_RETURN);
            if(std::memcmp(&selected,&reorder_screen_,sizeof(selected)) || roster_store_->accepted().generation!=generation)
                throw std::runtime_error("Accept did not respect replacement-stage gate");
            handleReorderKey('X');nba97_reorder_frame(&reorder_screen_.selection,0);handleReorderKey('X');
        } else if(scenario=="reload") {
            if(!roster_store_ || roster_database_.slotTable()!=roster_store_->accepted().slots)
                throw std::runtime_error("reload failed to apply saved roster");
        } else {
            if(scenario!="noop" && scenario!="repair") {
                handleReorderKey('C'); handleReorderKey(VK_DOWN); handleReorderKey('C');
                if(reorder_screen_.selection.changes!=1) throw std::runtime_error("host keys did not stage swap");
            }
            capture("draft");
            if(scenario=="children") {
                for(auto key:{'D','S'}) {
                    handleReorderKey(key);nba97_reorder_child_input_ready(&reorder_child_,0);
                    handleReorderKey('F');tickNotice();capture(key=='D' ? "view-help" : "compare-help");
                    handleReorderKey(VK_RETURN);tickNotice();handleReorderKey(VK_RETURN);
                    if(reorder_child_.state) throw std::runtime_error("child did not return before Accept");
                    nba97_reorder_frame(&reorder_screen_.selection,0);
                }
                if(roster_database_.slotTable()!=original_live) throw std::runtime_error("child published draft");
            }
            const auto before=reorder_screen_;
            bool injected=false;
            struct Fault {bool* fired;nba97::RosterSaveStage at;};
            Fault fault{&injected,scenario=="postcommit" ? nba97::RosterSaveStage::PrimaryReplaced : nba97::RosterSaveStage::PartialWrite};
            if(scenario=="failure" || scenario=="failure-cancel" || scenario=="postcommit")
                reorder_save_hooks_={[](nba97::RosterSaveStage s,void* p) {
                    auto& f=*static_cast<Fault*>(p);if(s==f.at) {*f.fired=true;throw std::runtime_error("injected host save failure");}
                },&fault};
            if(scenario=="cancel") {
                handleReorderKey('X');
                if(reorder_screen_.selection.phase!=NBA97_REORDER_DISCARD_PROMPT) throw std::runtime_error("missing discard prompt");
                handleReorderKey(VK_UP);handleReorderKey(VK_RETURN);
                if(roster_database_.slotTable()!=original_live) throw std::runtime_error("discard published roster");
            } else {
                handleReorderKey(VK_RETURN);
                if(scenario=="failure" || scenario=="failure-cancel" || scenario=="blocked") {
                    if(!reorder_notice_ || std::memcmp(&before,&reorder_screen_,sizeof(before)) ||
                       roster_database_.slotTable()!=original_live)
                        throw std::runtime_error("failed Accept lost editor state or published roster");
                    if(scenario!="blocked" && !injected) throw std::runtime_error("host failure injection not reached");
                    tickNotice();capture("failure-notice");dismiss();capture("after-notice");
                    if(std::memcmp(&before,&reorder_screen_,sizeof(before))) throw std::runtime_error("notice dismissal changed draft");
                    reorder_save_hooks_={};
                    if(scenario=="failure") handleReorderKey(VK_RETURN);
                    if(scenario=="failure-cancel") {
                        handleReorderKey('X');handleReorderKey(VK_UP);handleReorderKey(VK_RETURN);
                        if(roster_database_.slotTable()!=original_live) throw std::runtime_error("failed save followed by cancel published draft");
                    }
                } else if(scenario=="postcommit") {
                    if(!injected || !reorder_notice_ || roster_store_->accepted().generation!=generation+1 ||
                       roster_database_.slotTable()==original_live)
                        throw std::runtime_error("postcommit warning retained old accepted state");
                    tickNotice();capture("committed-notice");dismiss();
                    if(roster_store_->accepted().generation!=generation+1) throw std::runtime_error("warning dismissal replayed commit");
                }
            }
            if(scenario!="blocked" && frontend_page_!=nba97::FrontendPage::Rosters)
                throw std::runtime_error("successful Accept/Discard failed to leave editor");
            if(frontend_page_==nba97::FrontendPage::Rosters) {
                frontend_transition_active_=false;
                writePpm(nba97::renderRecoveredBottomMenu(bottom_menu_,menu_font_,menu_sprites_,roster_sprites_,users_sprites_,roster_menu_cards_,menu_elapsed_ms_),
                    options_.reorder_save_capture_dir/"returned.ppm");
            }
        }
        const auto final=roster_database_.slotTable();
        trace_.log("REORDER-SAVE-VERIFY",scenario+" PASS; generation="+
            std::to_string(roster_store_ ? roster_store_->accepted().generation : 0)+
            "; default-different="+std::to_string(roster_database_.differsFromOriginal())+
            "; team="+std::to_string(reorder_screen_.team)+
            "; first="+std::to_string(final[reorder_screen_.team*15])+
            "; second="+std::to_string(final[reorder_screen_.team*15+1])+
            "; actual host key handlers; no original fidelity score");
        return 0;
    }

    // Small Release -> Reset fixture, deliberately separate from the long
    // editor/child-flow capture. No direct slot mutation or commit shortcut:
    // release uses the real C/Enter handlers; subsequent processes drive Reset.
    int verifyReleaseResetSetup() {
        const bool release=options_.verify_reorder_save=="release-seed";
        auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
        require(roster_store_!=nullptr,"Release/Reset requires isolated save store");
        const auto before=roster_database_.slotTable();
        const auto generation=roster_store_->accepted().generation;
        const auto original=roster_database_.originalSlots();
        const auto player=original[45]; // Chicago's original first slot, not a hardcoded player ID.
        require(player!=UINT16_MAX,"Release/Reset fixture has no original starter");
        if(release) require(before==original && generation==0,"Release seed must start from fresh defaults");
        beginFrontendTransition(nba97::FrontendPage::Rosters,"isolated Release/Reset setup");
        frontend_transition_active_=false;
        release_team_=3;
        bottom_menu_.setSelected(2);
        require(bottom_menu_.selected()==2,"Release setup card disabled");
        handleMenuKey(VK_SPACE); // Native card-menu binding; editor C remains Release.
        require(bottom_select_pending_,"Release setup missing card selection");
        bottom_select_pending_=false;completeRecoveredBottomSelection();
        require(isRelease(),"Release setup did not enter editor");
        auto settle=[&] {
            for(int i=0;i<50 && isRelease();++i) {
                menu_elapsed_ms_+=17;nba97_trade_frame(&trade_screen_,0);
                nba97_reorder_child_input_ready(&reorder_child_,0);
                if(trade_choice_address_) tradeChoiceEvent(nba97_reset_tick(&trade_choice_,0));
                else stepReorderHelp(0,true);
                if(!isRelease())break;
                nba97_frontend_palette_tick(&trade_palette_,compare_backgrounds_->bank(),33);
                advanceComparePalette();if(compare_refresh_.remaining)advanceCompareRefresh();
                if(compare_repeat_.post_frames)--compare_repeat_.post_frames;
            }
            frontend_transition_active_=false;
        };
        auto capture=[&](const char* name) {
            settle();updatePlayerPhoto(true);
            writePpm(isRelease()?renderTrade():renderBottomMenu(),
                options_.reorder_save_capture_dir/(std::string(name)+".ppm"));
            trace_.log("RESET-RELEASE-CHECKPOINT",name);
        };
        capture("entry");
        if(release) {
            const auto pool_count=roster_database_.freeAgentCount();
            const auto team_count=trade_screen_.counts[3];
            require(trade_screen_.selected[0]==player,"Release seed selected wrong starter");
            handleTradeKey('C');capture("released-draft");
            require(!reorder_notice_ && nba97_trade_dirty(&trade_screen_) &&
                trade_screen_.counts[3]==team_count-1 && trade_screen_.counts[29]==pool_count+1 &&
                trade_screen_.working[435+pool_count]==player && roster_database_.slotTable()==before &&
                roster_store_->accepted().generation==generation,"Release seed draft isolation/counts failed");
            handleTradeKey(VK_RETURN);capture("accepted");
            require(frontend_page_==nba97::FrontendPage::Rosters &&
                roster_database_.rosterOwner(player)==29 && roster_store_->accepted().generation==generation+1 &&
                bottom_menu_.enabled(3),"Release seed accept/ownership/Reset availability failed");
        } else {
            require(roster_database_.slotTable()==roster_store_->accepted().slots,"Release probe reload mismatch");
            handleTradeKey('X');capture("returned");
            require(frontend_page_==nba97::FrontendPage::Rosters && roster_database_.slotTable()==before &&
                roster_store_->accepted().generation==generation,"Release probe mutated accepted data");
        }
        trace_.log("RESET-RELEASE-CHECK","player="+std::to_string(player)+
            "; owner="+std::to_string(roster_database_.rosterOwner(player))+
            "; pool-count="+std::to_string(roster_database_.freeAgentCount())+
            "; reset-enabled="+std::to_string(bottom_menu_.enabled(3)));
        const auto final=roster_database_.slotTable();
        trace_.log("REORDER-SAVE-VERIFY",options_.verify_reorder_save+" PASS; generation="+
            std::to_string(roster_store_->accepted().generation)+
            "; default-different="+std::to_string(roster_database_.differsFromOriginal())+
            "; team=3; first="+std::to_string(final[45])+"; second="+std::to_string(final[46])+
            "; Release host handlers; isolated fixture; no original timing parity claim");
        return 0;
    }

    int verifyRosterReset() {
        const auto& scenario=options_.verify_reorder_save;
        if(scenario!="reset-cancel" && scenario!="reset-confirm" && scenario!="reset-failure" &&
           scenario!="reset-postcommit" && scenario!="reset-locked")
            throw std::runtime_error("unknown Reset verification case");
        beginFrontendTransition(nba97::FrontendPage::Rosters,"isolated Reset host verification");
        frontend_transition_active_=false;
        const auto before=roster_database_.slotTable();
        const auto generation=roster_store_ ? roster_store_->accepted().generation:0;
        const auto capture=[&](const char* name) {
            writePpm(renderBottomMenu(),options_.reorder_save_capture_dir/(std::string(name)+".ppm"));
        };
        capture("entry");
        if(scenario=="reset-locked") {
            if(bottom_menu_.enabled(3)) throw std::runtime_error("default Reset is enabled");
            bottom_menu_.setSelected(3);
            if(bottom_menu_.selected()==3 || rosterResetEligible()) throw std::runtime_error("locked Reset focus accepted");
        } else {
            if(!bottom_menu_.enabled(3)) throw std::runtime_error("changed roster Reset remains locked");
            bottom_menu_.setSelected(3);
            capture("focused");
            const auto sound=cursor_audio_.exportCursorSound(options_.asset_root/"menu/ZCURSOR.VH",
                options_.asset_root/"menu/ZCURSOR.VB",12,options_.reorder_save_capture_dir/"reset-confirm.wav");
            trace_.log("RESET-AUDIO-EXPORT","BNKl id12; "+std::to_string(sound.sample_rate)+"Hz; "+
                std::to_string(sound.rendered_sample_count)+" samples; private WAV; no original capture parity claimed");
            for(auto invalid_id:{0u,13u,128u,0xffffffffu}) {
                bool rejected=false;
                try {cursor_audio_.exportCursorSound(options_.asset_root/"menu/ZCURSOR.VH",
                    options_.asset_root/"menu/ZCURSOR.VB",invalid_id,options_.reorder_save_capture_dir/"invalid.wav");}
                catch(const std::exception&) {rejected=true;}
                if(!rejected) throw std::runtime_error("unpopulated/out-of-range BNKl ID accepted");
            }
            activateRecoveredBottomSelection();
            if(bottom_select_pending_) throw std::runtime_error("Reset flashed before confirmation");
            capture("opening");
            for(int i=0;i<32;++i) tickReset(0x800);
            if(reset_prompt_.modal.phase!=NBA97_HELP_WAIT_CHANGE || reset_prompt_.choice!=1)
                throw std::runtime_error("held opener bypassed default Cancel");
            capture("held-cancel");
            tickReset(0);
            if(scenario!="reset-cancel") {
                handleMenuKey(VK_UP);
                for(int i=0;i<8;++i) tickReset(0);
                capture("restore-selected");
            }
            bool injected=false;
            struct Fault {bool* fired;nba97::RosterSaveStage at;};
            Fault fault{&injected,scenario=="reset-postcommit" ? nba97::RosterSaveStage::PrimaryReplaced:nba97::RosterSaveStage::PartialWrite};
            if(scenario=="reset-failure" || scenario=="reset-postcommit")
                reorder_save_hooks_={[](nba97::RosterSaveStage stage,void* context) {
                    auto& f=*static_cast<Fault*>(context);
                    if(stage==f.at) {*f.fired=true;throw std::runtime_error("injected Reset save failure");}
                },&fault};
            handleMenuKey('C');
            capture("closing");
            for(int i=0;i<32;++i) tickReset(0x800);
            if(bottom_select_pending_ || roster_database_.slotTable()!=before)
                throw std::runtime_error("held confirm published Reset");
            tickReset(0);
            if(scenario=="reset-cancel") {
                if(bottom_select_pending_ || roster_database_.slotTable()!=before ||
                   roster_store_->accepted().generation!=generation || bottom_menu_.selected()!=3 ||
                   !bottom_menu_.enabled(3)) throw std::runtime_error("Cancel mutated roster or lost Reset focus/availability");
            } else {
                if(!bottom_select_pending_) throw std::runtime_error("confirmed Reset missing card flash");
                capture("flash");
                bottom_select_pending_=false;completeRecoveredBottomSelection();
                if(scenario=="reset-failure") {
                    if(!injected || !reset_notice_ || roster_database_.slotTable()!=before ||
                       roster_store_->accepted().generation!=generation || !bottom_menu_.enabled(3))
                        throw std::runtime_error("failed Reset lost accepted roster or eligibility");
                } else if(roster_database_.differsFromOriginal() || bottom_menu_.enabled(3) ||
                          roster_store_->accepted().generation!=generation+1 || bottom_menu_.selected()!=4)
                    throw std::runtime_error("Reset failed to persist defaults/relock/focus View Rosters");
                if(scenario=="reset-postcommit" && (!injected || !reset_notice_))
                    throw std::runtime_error("Reset committed-warning not shown");
                if(reset_notice_) {
                    for(int i=0;i<32;++i) tickReset(0);
                    capture("save-notice");handleMenuKey('C');
                    for(int i=0;i<32;++i) tickReset(0);
                    if(reset_notice_) throw std::runtime_error("Reset notice did not close");
                }
            }
            reorder_save_hooks_={};
            capture("returned");
        }
        const auto final=roster_database_.slotTable();
        trace_.log("REORDER-SAVE-VERIFY",scenario+" PASS; generation="+
            std::to_string(roster_store_ ? roster_store_->accepted().generation:0)+
            "; default-different="+std::to_string(roster_database_.differsFromOriginal())+
            "; team=3; first="+std::to_string(final[45])+"; second="+std::to_string(final[46])+
            "; selected="+std::to_string(bottom_menu_.selected())+
            "; actual Reset host handlers; no original fidelity score");
        return 0;
    }

    int captureReorder() {
        frontend_page_ = nba97::FrontendPage::ReorderRosters;
        openReorder();
        auto capture = [&](const char* name) {
            // Offline settled checkpoints wait for actual I/O, never a fake CD
            // duration. Interactive frames only use the non-blocking poll.
            if (reorder_child_.state == 0x24) updatePlayerPhoto(true);
            loadReorderPortraits();
            const auto frame = renderReorder();
            if (std::string(name)=="entry.ppm" || std::string(name)=="replacement-scrolled.ppm" ||
                std::string(name)=="swapped.ppm") {
                // Check actual composed pixels against source-font glyphs,
                // not just the marker helper's returned flags/coordinates.
                unsigned visible=0;
                for(int page=0;page<2;++page) for(int down=0;down<2;++down) {
                    const auto* glyph=menu_font_.glyph(static_cast<char>(down?0x8c:0x8b));
                    if(!glyph) throw std::runtime_error("missing original Re-order marker glyph");
                    const int x=page?256:46, y=(down?196:116)-glyph->center_y;
                    bool all_equal=true; unsigned checked=0;
                    for(int gy=0;gy<glyph->height;++gy) for(int gx=0;gx<glyph->width;++gx) {
                        const auto source=(gy*glyph->width+gx)*4;
                        if(!glyph->rgba[source+3]) continue;
                        const auto target=((y+gy)*frame.width+x+gx)*4;
                        if(x+gx<0 || x+gx>=frame.width || y+gy<0 || y+gy>=frame.height)
                            throw std::runtime_error("Re-order marker fixture outside frame");
                        all_equal &= std::equal(glyph->rgba.begin()+source,glyph->rgba.begin()+source+3,
                                               frame.rgba.begin()+target);
                        ++checked;
                    }
                    const auto top=reorder_screen_.selection.top[page];
                    const bool expected=down?top<9:top>0;
                    if(!checked || all_equal!=expected)
                        throw std::runtime_error("Re-order composed marker visibility/pixels differ from source");
                    visible+=expected?1u:0u;
                }
                trace_.log("REORDER-MARKER-VERIFY",std::string(name)+" four glyph footprints checked; visible="+
                    std::to_string(visible)+" source=3DD38/3A224 both pages persist");
            }
            writePpm(frame, options_.reorder_capture_dir / name);
            logReorder(name);
        };
        auto capture_help = [&](const char* prefix) {
            // Same modal/controller/compositor as the interactive host. Logical
            // ticks are deterministic here; no original timing parity claimed.
            openReorderHelp();
            const auto name = [&](const char* suffix) { return std::string(prefix) + suffix + ".ppm"; };
            capture(name("-start").c_str());
            for (int i=0;i<40;++i) reorderHelpEvent(nba97_help_tick(&reorder_help_, 0));
            capture(name("-open").c_str());
            reorderHelpEvent(nba97_help_input(&reorder_help_, 0x800));
            for (int i=0;i<5;++i) reorderHelpEvent(nba97_help_tick(&reorder_help_, 0x800));
            capture(name("-closing").c_str());
            for (int i=0;i<40;++i) reorderHelpEvent(nba97_help_tick(&reorder_help_, 0));
            capture(name("-returned").c_str());
        };
        auto capture_view = [&](const char* prefix) {
            const auto name = [&](const char* suffix) { return std::string(prefix)+suffix+".ppm"; };
            handleReorderKey('D'); // Real window handler, not a substitute test renderer.
            if (reorder_child_.state != 0x24) throw std::runtime_error("capture did not enter View child");
            writePpm(renderReorder(), options_.reorder_capture_dir / name("-photo-wait"));
            logReorder(name("-photo-wait").c_str());
            capture(name("-entered").c_str());
            nba97_reorder_child_input_ready(&reorder_child_,0);
            handleReorderKey(VK_UP); // Top endpoint: no sound/flash.
            handleReorderKey(VK_DOWN); // Team/layer must preserve this scroll.
            handleReorderKey(VK_RIGHT);
            writePpm(renderReorder(), options_.reorder_capture_dir / name("-photo-cycle-wait"));
            logReorder(name("-photo-cycle-wait").c_str());
            handleReorderKey('K');
            handleReorderKey('E');
            if(roster_viewer_.firstVisiblePlayerStat()!=1)
                throw std::runtime_error("View child team/layer lost stat scroll");
            capture(name("-browsed").c_str());
            handleReorderKey('F');
            for(int i=0;i<40;++i) reorderHelpEvent(nba97_help_tick(&reorder_help_,0));
            capture(name("-help").c_str());
            handleReorderKey(VK_SPACE);
            for(int i=0;i<40;++i) reorderHelpEvent(nba97_help_tick(&reorder_help_,0));
            if(std::string(prefix)=="view-first") {
                // Extent reset is NOT an Up press: no extra scroll sound/flash.
                const auto previous_flash=stat_flash_until_ms_;
                handleReorderKey('Q'); handleReorderKey('Q'); handleReorderKey('Q');
                if(roster_viewer_.firstVisiblePlayerStat()!=0 || stat_flash_until_ms_!=previous_flash)
                    throw std::runtime_error("View layer reset generated scroll feedback");
                handleReorderKey(VK_UP); // Endpoint remains silent.
                handleReorderKey(VK_DOWN); handleReorderKey(VK_UP);
                handleReorderKey(VK_LEFT); handleReorderKey(VK_RIGHT); handleReorderKey('J');
            }
            handleReorderKey(std::string(prefix)=="view-replacement" ? VK_ESCAPE : VK_RETURN);
            if(reorder_child_.state) throw std::runtime_error("capture did not return to editor");
            capture(name("-returned").c_str());
            nba97_reorder_frame(&reorder_screen_.selection,0); // Release Start; no extra action.
        };
        auto capture_compare = [&](const char* prefix) {
            const auto name=[&](const char* suffix) { return std::string(prefix)+suffix+".ppm"; };
            auto finish_callback=[&](const char* label,bool palette_frames=false) {
                if(compare_repeat_.post_frames!=6 || compare_refresh_.remaining)
                    throw std::runtime_error("Compare generic callback must wait5 plus poll1");
                const auto retained=reorder_compare_;const auto parent=reorder_screen_;
                const auto audio=cursor_audio_.info();
                for(unsigned phase=0;phase<6;++phase) {
                    const auto before=renderReorder();
                    for(auto key:{WPARAM(VK_LEFT),WPARAM(VK_RIGHT),WPARAM(VK_UP),WPARAM(VK_DOWN),
                        WPARAM('K'),WPARAM('J'),WPARAM('Q'),WPARAM('E'),WPARAM('F'),WPARAM(VK_SPACE),WPARAM(VK_RETURN),WPARAM('X')})
                        handleReorderKey(key);
                    if(std::memcmp(&retained,&reorder_compare_,sizeof(retained)) ||
                        std::memcmp(&parent,&reorder_screen_,sizeof(parent)) || renderReorder().rgba!=before.rgba ||
                        reorder_child_.state!=0x23 || cursor_audio_.info().record!=audio.record)
                        throw std::runtime_error("input/audio escaped generic Compare callback wait");
                    advanceComparePostCycle(0);
                    if(palette_frames && std::string(prefix)=="compare-first")
                        capture(("compare-palette-"+std::to_string(phase)+".ppm").c_str());
                }
                if(compare_repeat_.post_frames) throw std::runtime_error("Compare callback did not release after poll");
                trace_.log("COMPARE-CALLBACK-VERIFY",std::string(prefix)+" "+label+
                    " presents=6; injected keys blocked; draft unchanged");
            };
            // Previous child return releases its input through a real C frame,
            // which advances parent tint once. Compare against THIS entry
            // frame, not the earlier pre-View frame's animation phase.
            capture(name("-parent").c_str());
            handleReorderKey('S');
            if(reorder_child_.state!=0x23) throw std::runtime_error("capture did not enter Compare");
            capture(name("-entered").c_str());
            nba97_reorder_child_input_ready(&reorder_child_,0);
            handleReorderKey(VK_SPACE);
            capture(name("-side").c_str());
            finish_callback("side");
            handleReorderKey(VK_RIGHT);
            const auto pending=reorder_compare_;
            const auto frozen_parent=reorder_screen_;
            const auto before_audio=cursor_audio_.info();
            for(unsigned phase=0;phase<2;++phase) {
                const auto frozen=renderReorder();
                for(auto key:{WPARAM(VK_LEFT),WPARAM(VK_RIGHT),WPARAM(VK_UP),WPARAM(VK_DOWN),
                    WPARAM('K'),WPARAM('J'),WPARAM('Q'),WPARAM('E'),WPARAM('F'),WPARAM(VK_SPACE),WPARAM(VK_RETURN),WPARAM('X')})
                    handleReorderKey(key);
                if(std::memcmp(&pending,&reorder_compare_,sizeof(pending)) ||
                    std::memcmp(&frozen_parent,&reorder_screen_,sizeof(frozen_parent)) ||
                    renderReorder().rgba!=frozen.rgba || cursor_audio_.info().record!=before_audio.record ||
                    compare_refresh_.remaining!=2-phase || reorder_child_.state!=0x23)
                    throw std::runtime_error("Compare input/audio escaped two-present callback");
                if(std::string(prefix)=="compare-first") capture(("compare-refresh-"+std::to_string(phase)+".ppm").c_str());
                advanceCompareRefresh(); // The same continuation used after an interactive paint.
            }
            if(compare_refresh_.remaining || std::memcmp(&compare_refresh_.text,&pending,sizeof(pending)))
                throw std::runtime_error("Compare text did not commit after two presents");
            if(std::string(prefix)=="compare-first") capture("compare-refresh-2.ppm");
            unsigned post_frames=0;
            while(compare_repeat_.post_frames) {
                const auto frozen_post=renderReorder();
                for(auto key:{WPARAM('K'),WPARAM('F'),WPARAM(VK_RETURN),WPARAM(VK_LEFT),WPARAM(VK_SPACE)})
                    handleReorderKey(key);
                if(renderReorder().rgba!=frozen_post.rgba || reorder_child_.state!=0x23)
                    throw std::runtime_error("Compare post-delay accepted menu input");
                advanceComparePostCycle(0); ++post_frames;
            }
            if(post_frames!=8) throw std::runtime_error("initial Compare post-delay must be7 plus poll1");
            trace_.log("COMPARE-PACING-VERIFY",std::string(prefix)+" post-presents=8; extra input blocked; parent retained");
            handleReorderKey('K');
            finish_callback("team",true);
            for(unsigned factor=6;factor<=16;++factor) {
                advanceComparePalette();
                if(std::string(prefix)=="compare-first")
                    capture(("compare-palette-"+std::to_string(factor)+".ppm").c_str());
            }
            advanceComparePalette(); // Settled update must preserve the full frame.
            if(std::string(prefix)=="compare-first") capture("compare-palette-settled.ppm");
            auto finish_scroll=[&]() {
                while(compare_refresh_.remaining) { (void)renderReorder();advanceCompareRefresh(); }
                while(compare_repeat_.post_frames) { (void)renderReorder();advanceComparePostCycle(0); }
            };
            handleReorderKey('E');finish_callback("layer");handleReorderKey(VK_DOWN);finish_scroll();
            capture(name("-browsed").c_str());
            logCompare(name("-browsed").c_str());
            handleReorderKey('F');
            for(int i=0;i<40;++i) reorderHelpEvent(nba97_help_tick(&reorder_help_,0));
            capture(name("-help").c_str());
            handleReorderKey(VK_SPACE);
            for(int i=0;i<40;++i) reorderHelpEvent(nba97_help_tick(&reorder_help_,0));
            if(std::string(prefix)=="compare-first") {
                handleReorderKey('Q');finish_callback("layer-back-stats");
                handleReorderKey('Q');finish_callback("layer-back-ratings");
                capture("compare-ratings.ppm"); logCompare("compare-ratings.ppm");
                handleReorderKey('Q');
                finish_callback("layer-back-attributes");
                capture("compare-attributes.ppm"); logCompare("compare-attributes.ppm");
                for(int i=0;i<12;++i) { handleReorderKey(VK_DOWN);finish_scroll(); }
                capture("compare-attributes-bottom.ppm"); logCompare("compare-attributes-bottom.ppm");
                for(int i=0;i<30 && reorder_compare_.team[1]!=29;++i) {
                    handleReorderKey('J');finish_callback("team-back");
                }
                if(reorder_compare_.team[1]!=29) throw std::runtime_error("private Compare free-agent capture unavailable");
                for(unsigned factor=0;factor<=16;++factor) advanceComparePalette();
                capture("compare-free-agents.ppm"); logCompare("compare-free-agents.ppm");
            }
            handleReorderKey(VK_RETURN);
            if(reorder_child_.state) throw std::runtime_error("Compare capture failed to return");
            capture(name("-returned").c_str());
            nba97_reorder_frame(&reorder_screen_.selection,0);
        };
        for (int i=0;i<12;++i) nba97_reorder_frame(&reorder_screen_.selection,0);
        capture("entry.ppm");
        capture_help("help-first");
        capture_view("view-first");
        capture_compare("compare-first");
        nba97_reorder_screen_input(&reorder_screen_, NBA97_REORDER_SELECT);
        for (int i=0;i<7;++i) nba97_reorder_screen_input(&reorder_screen_, NBA97_REORDER_DOWN);
        for (int i=0;i<12;++i) nba97_reorder_frame(&reorder_screen_.selection,0);
        capture("replacement-scrolled.ppm");
        capture_help("help-replacement");
        capture_view("view-replacement");
        capture_compare("compare-replacement");
        nba97_reorder_screen_input(&reorder_screen_, NBA97_REORDER_SELECT);
        capture("swapped.ppm");
        capture_view("view-swapped");
        capture_compare("compare-swapped");
        nba97_reorder_screen_input(&reorder_screen_, NBA97_REORDER_CANCEL);
        reorder_modal_frame_ = 32;
        capture("discard-prompt.ppm");
        nba97_reorder_screen_input(&reorder_screen_, NBA97_REORDER_DISCARD_YES);
        // Find a real roster player whose five original IDX entries are empty.
        // Do not force the availability flag or invent an absent speech asset.
        std::ifstream fact_index_file(options_.asset_root/"menu/Z1COOL.IDX",std::ios::binary);
        const std::vector<std::uint8_t> fact_index((std::istreambuf_iterator<char>(fact_index_file)),{});
        const auto base_slots=roster_database_.slotTable();
        std::size_t missing_slot=435;
        for(std::size_t i=0;i<435;++i)
            if(base_slots[i]!=UINT16_MAX && !nba97::playerHasCoolFacts(fact_index,base_slots[i])) {missing_slot=i;break;}
        if(missing_slot==435) throw std::runtime_error("no original no-facts roster case found");
        reorder_team_=static_cast<std::int16_t>(missing_slot/15);
        for(unsigned page=0;page<2;++page) {
            reorder_saved_cursor_[page]=static_cast<std::int16_t>(missing_slot%15+page*15);
            reorder_saved_top_[page]=static_cast<std::int16_t>((missing_slot%15>5 ? missing_slot%15-5 : 0)+page*15);
        }
        openReorder();capture("no-facts-parent.ppm");
        handleReorderKey('D');nba97_reorder_child_input_ready(&reorder_child_,0);
        if(roster_cool_facts_available_) throw std::runtime_error("original absent fact incorrectly enabled");
        const auto frozen_parent=reorder_screen_;
        const auto frozen_player=roster_viewer_.selectedPlayer(viewerDatabase())->id;
        const auto frozen_stat=roster_viewer_.firstVisiblePlayerStat();
        capture("no-facts-player.ppm");
        handleReorderKey('C');capture("no-facts-opening.ppm");
        for(int i=0;i<13;++i) playerNoticeEvent(nba97_help_tick(&player_notice_,0x800));
        handleReorderKey('C'); // Held opener cannot close.
        if(player_notice_.phase!=NBA97_HELP_WAIT_CHANGE) throw std::runtime_error("notice consumed held opener");
        capture("no-facts-open.ppm");
        handleReorderKey(VK_DOWN); // Changed input dismisses, never scrolls player.
        for(int i=0;i<5;++i) playerNoticeEvent(nba97_help_tick(&player_notice_,2));
        capture("no-facts-closing.ppm");
        for(int i=0;i<8;++i) playerNoticeEvent(nba97_help_tick(&player_notice_,2));
        if(player_notice_.phase!=NBA97_HELP_RETURN_BARRIER) throw std::runtime_error("notice close barrier missing");
        handleReorderKey(VK_RETURN); // Changes closing mask; must not exit View.
        if(player_notice_.phase!=NBA97_HELP_CLOSED || reorder_child_.state!=0x24 ||
           roster_viewer_.selectedPlayer(viewerDatabase())->id!=frozen_player ||
           roster_viewer_.firstVisiblePlayerStat()!=frozen_stat ||
           std::memcmp(&frozen_parent,&reorder_screen_,sizeof(frozen_parent)))
            throw std::runtime_error("no-facts round trip changed child/parent state");
        capture("no-facts-returned.ppm");
        handleReorderKey(VK_RETURN);capture("no-facts-editor-return.ppm");
        if(reorder_child_.state || roster_database_.slotTable()!=base_slots)
            throw std::runtime_error("no-facts View return published state");
        trace_.log("PLAYER-NOTICE-VERIFY","original IDX absence; player="+std::to_string(frozen_player)+
            "; 13 grow/13 shrink ticks; source footer; key barriers; child/parent unchanged; no save");
        // Isolated runtime wiring check; never persist test volume settings.
        const auto saved_settings = settings_;
        settings_ = nba97::FrontendSettings{};
        settings_.adjustOption(3,-1); // 9 -> 8
        playBottomMenuSound(2,"sfx-setting-check");
        if(cursor_audio_.info().playback_volume!=96)
            throw std::runtime_error("host ignored live SFX setting");
        const auto audible = cursor_audio_.info();
        while(settings_.option(3)) settings_.adjustOption(3,-1);
        playBottomMenuSound(1,"sfx-setting-check");
        playRosterCursorSound(-1);
        if(cursor_audio_.info().record!=audible.record || cursor_audio_.info().playback_volume!=96 ||
           stat_flash_direction_!=-1 || stat_flash_until_ms_!=menu_elapsed_ms_+340)
            throw std::runtime_error("mute replaced audio or suppressed visual stat flash");
        settings_=saved_settings;
        trace_.log("SFX-SETTING-VERIFY", "setting8->96; setting0 skips playback; visual flash retained; settings restored without save");
        for(unsigned logical=0;logical<6;++logical) {
            const auto player_id=static_cast<std::uint16_t>(logical/5);
            const auto variant=logical%5;
            const auto filename="cool-fact-p"+std::to_string(player_id)+"-v"+std::to_string(variant)+".wav";
            const auto info=cool_fact_audio_.exportCoolFact(options_.asset_root/"menu/Z1COOL.IDX",
                options_.asset_root/"menu/Z1COOL.BIG",player_id,variant,options_.reorder_capture_dir/filename);
            if(info.record!=logical+1) throw std::runtime_error("Cool Fact reserved record regression");
            trace_.log("COOL-FACT-EXPORT", "player="+std::to_string(player_id)+" variant="+std::to_string(variant)+
                " logical="+std::to_string(logical)+" physical="+std::to_string(info.record)+
                " rate="+std::to_string(info.sample_rate)+" samples="+std::to_string(info.sample_count)+
                " -> "+filename+"; "+roster_database_.player(player_id)->displayName()+"; local decoded PCM, not original mixer capture");
        }
        const auto first_fact_slot=std::find(base_slots.begin(),base_slots.begin()+435,0);
        if(first_fact_slot==base_slots.begin()+435) throw std::runtime_error("five-fact roster fixture absent");
        const auto fact_slot=static_cast<unsigned>(first_fact_slot-base_slots.begin());
        reorder_team_=static_cast<std::int16_t>(fact_slot/15);
        for(unsigned page=0;page<2;++page) {
            reorder_saved_cursor_[page]=static_cast<std::int16_t>(fact_slot%15+page*15);
            reorder_saved_top_[page]=static_cast<std::int16_t>((fact_slot%15>5 ? fact_slot%15-5 : 0)+page*15);
        }
        openReorder();capture("fact-cycle-parent.ppm");
        handleReorderKey('D');nba97_reorder_child_input_ready(&reorder_child_,0);
        capture("fact-cycle-player.ppm");
        if(cool_fact_selection_.available_mask!=31) throw std::runtime_error("five-fact source fixture changed");
        const auto fact_parent=reorder_screen_;
        int previous=-1;
        for(unsigned cycle=0;cycle<2;++cycle) {
            unsigned played=0;
            for(unsigned i=0;i<5;++i) {
                constexpr unsigned speech_levels[]{9,8,4,0,9};
                while(settings_.option(2)!=speech_levels[i]) settings_.adjustOption(2,1);
                handleReorderKey('C');
                const int variant=cool_fact_selection_.selected;
                if(variant<0 || variant>=5 || (played&(1u<<variant)) ||
                   (i==0 && previous==variant) || cool_fact_selection_.flags[variant]!=1 ||
                   cool_fact_audio_.info().record!=static_cast<unsigned>(variant+1))
                    throw std::runtime_error("host Cool Fact cycle repeated or decoded wrong variant");
                if(cool_fact_audio_.info().playback_volume!=(std::min)(speech_levels[i]*15u,127u) ||
                   cool_fact_audio_.info().playback_suppressed || !cool_fact_audio_.isPlaying())
                    throw std::runtime_error("host ignored speech setting or suppressed zero-gain voice");
                for(unsigned frame=0;frame<8;++frame) {
                    const auto before=renderReorder();
                    const auto choice=cool_fact_selection_;
                    constexpr WPARAM blocked[]{VK_LEFT,VK_RIGHT,VK_UP,VK_DOWN,'Q','E','J','K','F','C','D',VK_RETURN,'X'};
                    for(auto key:blocked) handleReorderKey(key);
                    if(before.rgba!=renderReorder().rgba ||
                       std::memcmp(&choice,&cool_fact_selection_,sizeof(choice)) ||
                       cool_fact_flash_.remaining!=8-frame || reorder_child_.state!=0x24)
                        throw std::runtime_error("input escaped the Cool Fact callback/flash");
                    if(!cycle && !i) capture(("fact-flash-"+std::to_string(frame)+".ppm").c_str());
                    advanceCoolFactFlash();
                    if(cool_fact_selection_.flags[variant]!=(frame==7 ? 0 : 1))
                        throw std::runtime_error("Cool Fact consumed before eight completed presents");
                }
                played|=1u<<variant;previous=variant;
                handleReorderKey('D');
                if(cool_fact_audio_.isPlaying()) throw std::runtime_error("Square did not stop speech");
                const auto stop_cue=cursor_audio_.info();
                handleReorderKey('D'); // Already stopped: no second cue.
                if(cursor_audio_.info().record!=stop_cue.record)
                    throw std::runtime_error("idle Square generated a cue");
                if(std::memcmp(&fact_parent,&reorder_screen_,sizeof(fact_parent)))
                    throw std::runtime_error("Cool Fact mutated parent roster");
                trace_.log("COOL-FACT-CYCLE-VERIFY","cycle="+std::to_string(cycle)+" step="+std::to_string(i)+
                    " variant="+std::to_string(variant)+" played-mask="+std::to_string(played));
            }
            if(played!=31) throw std::runtime_error("host Cool Fact cycle missed a variant");
        }
        settings_=saved_settings; // No test settings are persisted.
        capture("fact-cycle-returned.ppm");
        handleReorderKey(VK_RETURN);capture("fact-cycle-editor-return.ppm");
        // Separate semantic block: original85 stills remain unchanged. These
        // exercise the actual host continuations, not only the pure C policy.
        trace_.log("COMPARE-HELD-VERIFY","begin");
        nba97_reorder_frame(&reorder_screen_.selection,0);
        handleReorderKey('S');
        if(reorder_child_.state!=0x23) throw std::runtime_error("held test failed to open Compare");
        nba97_reorder_child_input_ready(&reorder_child_,0);
        const auto held_parent=reorder_screen_;
        const auto held_table=viewerDatabase().slotTable();
        handleReorderKey(VK_RIGHT);
        while(compare_refresh_.remaining) advanceCompareRefresh();
        const auto arrow_base=renderReorder();
        const auto* arrow_glyph=reorder_labels_->smallFont().glyph(static_cast<char>(0x8a));
        if(!arrow_glyph) throw std::runtime_error("missing original arrow for private flash proof");
        const int arrow_x=135-reorder_labels_->smallFont().textWidth(std::string(1,static_cast<char>(0x8a)))/2;
        const int arrow_y=116-arrow_glyph->center_y;
        unsigned arrow_pixels=0;
        for(unsigned phase=0;phase<=21;++phase) {
            if(phase) {
                if(compare_repeat_.post_frames) advanceComparePostCycle(0);
                else advanceComparePalette();
            }
            const int gold[3]={120,102,0};
            const auto shown=renderReorder();
            auto expected=arrow_base;
            for(unsigned y=0;y<arrow_glyph->height;++y) for(unsigned x=0;x<arrow_glyph->width;++x) {
                const auto src=(y*arrow_glyph->width+x)*4;
                if(!arrow_glyph->rgba[src+3]) continue;
                const auto dst=((arrow_y+y)*512+arrow_x+x)*4;
                for(unsigned c=0;c<3;++c) {
                    const int modulation=phase<=4 ? 128+(gold[c]-128)*int(phase)/4 :
                        phase<=16 ? gold[c] : phase<=20 ? gold[c]+(128-gold[c])*int(phase-16)/4 : 128;
                    expected.rgba[dst+c]=static_cast<std::uint8_t>((std::min)(255,int(arrow_glyph->rgba[src+c])*modulation/128));
                }
                ++arrow_pixels;
            }
            if(shown.rgba!=expected.rgba || std::memcmp(&held_parent,&reorder_screen_,sizeof(held_parent)))
                throw std::runtime_error("Compare arrow flash changed wrong pixels/parent");
            capture(("compare-arrow-"+std::to_string(phase)+".ppm").c_str());
        }
        trace_.log("COMPARE-ARROW-VERIFY","phases=22 original-glyph-pixels="+std::to_string(arrow_pixels)+
            " outside-unchanged=yes; native-neutral-start=128; original allocator start not captured");
        const unsigned team=reorder_compare_.team[0], initial_slot=reorder_compare_.slot[0];
        unsigned team_count=0;
        while(team_count<15 && held_table[team*15+team_count]!=UINT16_MAX) ++team_count;
        if(!team_count) throw std::runtime_error("held test empty team");
        handleReorderKey(VK_RIGHT);
        unsigned held_presents=0;
        for(unsigned action=0;action<40;++action) {
            const unsigned expected_slot=(initial_slot+action+1)%team_count;
            if(reorder_compare_.slot[0]!=expected_slot || reorder_compare_.player[0]!=held_table[team*15+expected_slot] ||
               compare_repeat_.counter!=(std::min)(2+action*4,48u) || compare_refresh_.remaining!=2)
                throw std::runtime_error("held cycle identity/counter/callback mismatch");
            for(unsigned phase=0;phase<2;++phase) {
                (void)renderReorder(); advanceCompareRefresh(); ++held_presents;
            }
            const unsigned post=compare_repeat_.post_frames;
            for(unsigned frame=0;frame<post;++frame) {
                (void)renderReorder(); advanceComparePostCycle(action==39 ? 0 : 4); ++held_presents;
                if(frame+1<post && reorder_compare_.slot[0]!=expected_slot)
                    throw std::runtime_error("held action bypassed post/poll wait");
            }
            if(std::memcmp(&held_parent,&reorder_screen_,sizeof(held_parent)) || viewerDatabase().slotTable()!=held_table)
                throw std::runtime_error("held cycle modified editor/draft");
        }
        if(held_presents!=200 || compare_repeat_.post_frames || compare_refresh_.remaining)
            throw std::runtime_error("held cycle did not stop at release poll");
        auto finish_callback=[&](std::uint16_t held) {
            while(compare_refresh_.remaining) { (void)renderReorder();advanceCompareRefresh(); }
            const unsigned post=compare_repeat_.post_frames;
            for(unsigned frame=0;frame<post;++frame) { (void)renderReorder();advanceComparePostCycle(held); }
        };
        const auto reversal_slot=reorder_compare_.slot[0];
        handleReorderKey(VK_RIGHT); finish_callback(8);
        if(compare_repeat_.counter!=2 || reorder_compare_.slot[0]!=reversal_slot || compare_refresh_.remaining!=2)
            throw std::runtime_error("held reversal failed to reset acceleration");
        finish_callback(12); // Neither direction wins a contradictory chord.
        const auto chord_state=reorder_compare_;
        for(unsigned poll=0;poll<3;++poll) { (void)renderReorder();advanceComparePostCycle(12); }
        if(std::memcmp(&chord_state,&reorder_compare_,sizeof(chord_state)) || compare_refresh_.remaining)
            throw std::runtime_error("opposing direction chord cycled a player");
        advanceComparePostCycle(4); // Releasing Left resumes the still-held Right.
        if(compare_refresh_.remaining!=2 || compare_repeat_.counter!=2)
            throw std::runtime_error("chord release did not resume fresh held direction");
        finish_callback(0x20); // Help held at the poll is processed after callback.
        if(reorder_help_.phase==NBA97_HELP_CLOSED || reorder_child_.state!=0x23)
            throw std::runtime_error("post-cycle held Help was lost");
        for(unsigned frame=0;frame<40;++frame) reorderHelpEvent(nba97_help_tick(&reorder_help_,0));
        handleReorderKey(VK_SPACE);
        for(unsigned frame=0;frame<40;++frame) reorderHelpEvent(nba97_help_tick(&reorder_help_,0));
        if(reorder_help_.phase!=NBA97_HELP_CLOSED) throw std::runtime_error("held Help did not return");
        handleReorderKey(VK_LEFT); finish_callback(0x100);
        if(reorder_child_.state || viewerDatabase().slotTable()!=held_table)
            throw std::runtime_error("held Cancel failed to return without mutation");
        trace_.log("COMPARE-HELD-VERIFY","end actions=40 presentations=200; wrap/release/reversal/chord/help/cancel passed; draft unchanged; original timing comparison pending");
        trace_.log("COMPARE-GENERIC-VERIFY","begin");
        nba97_reorder_frame(&reorder_screen_.selection,0);
        handleReorderKey('S');nba97_reorder_child_input_ready(&reorder_child_,0);
        if(reorder_child_.state!=0x23) throw std::runtime_error("generic test failed to open Compare");
        auto generic_parent=reorder_screen_;
        const auto generic_table=viewerDatabase().slotTable();
        unsigned generic_waits=0;
        auto finish_generic=[&](std::uint16_t held) {
            const auto mask=compare_repeat_.previous_mask;
            if(compare_repeat_.post_frames!=6 || compare_refresh_.remaining)
                throw std::runtime_error("generic callback wrong presentation count");
            const auto retained=reorder_compare_;
            for(unsigned phase=0;phase<6;++phase) {
                const auto before=renderReorder();const auto audio=cursor_audio_.info();
                for(auto key:{WPARAM(VK_LEFT),WPARAM(VK_RIGHT),WPARAM(VK_UP),WPARAM(VK_DOWN),WPARAM('K'),WPARAM('J'),
                    WPARAM('Q'),WPARAM('E'),WPARAM('F'),WPARAM(VK_SPACE),WPARAM(VK_RETURN),WPARAM('X')})
                    handleReorderKey(key);
                if(std::memcmp(&retained,&reorder_compare_,sizeof(retained)) ||
                    std::memcmp(&generic_parent,&reorder_screen_,sizeof(generic_parent)) ||
                    cursor_audio_.info().record!=audio.record || renderReorder().rgba!=before.rgba || reorder_child_.state!=0x23)
                    throw std::runtime_error("generic callback accepted input before poll");
                advanceComparePostCycle(held);
                if(phase<5 && std::memcmp(&retained,&reorder_compare_,sizeof(retained)))
                    throw std::runtime_error("held callback repeated before sixth presentation");
            }
            ++generic_waits;
            trace_.log("COMPARE-GENERIC-VERIFY","wait mask="+std::to_string(mask)+" held="+std::to_string(held)+
                " presents=6; blocked-input checked each present");
        };
        for(auto mask:std::array<std::uint16_t,5>{0x200,0x400,0x1000,0x2000,0x800}) {
            handleCompareInput(mask);
            const auto once=reorder_compare_;
            finish_generic(mask);
            if(compare_repeat_.counter!=6 || compare_repeat_.post_frames!=6 ||
                !std::memcmp(&once,&reorder_compare_,sizeof(once)))
                throw std::runtime_error("held generic callback did not repeat at fixed delay");
            finish_generic(0);
        }
        for(auto mask:std::array<std::uint16_t,4>{0x10,0x40,0x404,0x600}) {
            const auto before=reorder_compare_;const auto audio=cursor_audio_.info();
            handleCompareInput(mask);
            if(std::memcmp(&before,&reorder_compare_,sizeof(before)) || cursor_audio_.info().record!=audio.record)
                throw std::runtime_error("unsupported generic callback changed state or sound");
            finish_generic(0);
        }
        handleCompareInput(0x400);finish_generic(0x20);
        if(reorder_help_.phase==NBA97_HELP_CLOSED) throw std::runtime_error("held Help lost after generic callback");
        for(unsigned frame=0;frame<40;++frame) reorderHelpEvent(nba97_help_tick(&reorder_help_,0));
        handleReorderKey(VK_SPACE);
        for(unsigned frame=0;frame<40;++frame) reorderHelpEvent(nba97_help_tick(&reorder_help_,0));
        if(reorder_help_.phase!=NBA97_HELP_CLOSED) throw std::runtime_error("generic callback Help did not return");
        generic_parent.selection.input_latch=0; // Help consumes its return input, not roster data.
        handleCompareInput(0x2000);finish_generic(0x100);
        auto expected_return=generic_parent;
        expected_return.selection.screen_result=0;
        expected_return.selection.child_ids[0]=expected_return.selection.child_ids[1]=UINT16_MAX;
        expected_return.selection.input_latch=0;
        expected_return.selection.waiting_input_change=1;
        expected_return.selection.held_mask=0x100;
        if(reorder_child_.state || std::memcmp(&expected_return,&reorder_screen_,sizeof(expected_return)) ||
            viewerDatabase().slotTable()!=generic_table || generic_waits!=16)
            throw std::runtime_error("generic callback round trips changed parent draft");
        trace_.log("COMPARE-GENERIC-VERIFY","end waits=16 presentations=96; five held routes, four silent noops, Help/Cancel; draft unchanged");
        trace_.log("COMPARE-SCROLL-VERIFY","begin");
        nba97_reorder_frame(&reorder_screen_.selection,0);
        handleReorderKey('S');nba97_reorder_child_input_ready(&reorder_child_,0);
        if(reorder_child_.state!=0x23) throw std::runtime_error("scroll test failed to open Compare");
        const auto scroll_table=viewerDatabase().slotTable();
        for(unsigned i=0;i<18;++i) advanceComparePalette();
        unsigned scroll_actions=0,scroll_presents=0,scroll_cues=0;
        auto scroll_step=[&](std::uint16_t mask,std::uint16_t held,const char* prefix) {
            if(!compare_repeat_.post_frames) handleCompareInput(mask);
            const auto requested=reorder_compare_;
            const auto callback_presents=compare_refresh_.remaining;
            const auto old_top=callback_presents ? int(requested.top)+(mask==1?1:-1) : requested.top;
            const unsigned post=mask==1 && !callback_presents && !requested.top ? 1 : 4;
            if(compare_repeat_.previous_mask!=mask || compare_repeat_.post_frames!=post)
                throw std::runtime_error("scroll host fixed wait missing");
            for(unsigned phase=0;phase<callback_presents;++phase) {
                if(prefix) capture((std::string(prefix)+"-"+std::to_string(phase)+".ppm").c_str());
                const auto image=renderReorder();const auto audio=cursor_audio_.info();
                for(auto key:std::array<WPARAM,12>{VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,'J','K','Q','E',VK_SPACE,'F',VK_RETURN,VK_ESCAPE})
                    handleCompareKey(key);
                if(std::memcmp(&requested,&reorder_compare_,sizeof(requested)) || renderReorder().rgba!=image.rgba ||
                    cursor_audio_.info().record!=audio.record || viewerDatabase().slotTable()!=scroll_table)
                    throw std::runtime_error("input escaped scroll presentation barrier");
                if(nba97_compare_refresh_top(&compare_refresh_,0)!=unsigned(phase?requested.top:old_top) ||
                    nba97_compare_refresh_top(&compare_refresh_,1)!=unsigned(old_top))
                    throw std::runtime_error("Compare group0/group1 scroll order differs");
                advanceCompareRefresh();++scroll_presents;
            }
            if(prefix) capture((std::string(prefix)+"-2.ppm").c_str());
            for(unsigned phase=0;phase<post;++phase) {
                const auto image=renderReorder();const auto audio=cursor_audio_.info();
                for(auto key:std::array<WPARAM,4>{VK_DOWN,'F','K',VK_RETURN}) handleCompareKey(key);
                if(std::memcmp(&requested,&reorder_compare_,sizeof(requested)) || renderReorder().rgba!=image.rgba ||
                    cursor_audio_.info().record!=audio.record)
                    throw std::runtime_error("input escaped scroll post-delay barrier");
                advanceComparePostCycle(phase+1==post?held:0);++scroll_presents;
            }
            ++scroll_actions;if(callback_presents) ++scroll_cues;
            trace_.log("COMPARE-SCROLL-VERIFY","action mask="+std::to_string(mask)+" top="+std::to_string(requested.top)+
                " callback="+std::to_string(callback_presents)+" post="+std::to_string(post)+" held="+std::to_string(held));
        };
        scroll_step(1,0,nullptr); // Top endpoint, no cue or callback frames.
        scroll_step(2,0,"compare-scroll-down");
        scroll_step(1,0,"compare-scroll-up");
        // Hold through the full normal-stat extent, beyond bottom, then reverse
        // through the full list and beyond top. Each accepted poll is bounded.
        for(unsigned i=0;i<21;++i) scroll_step(2,i==20?0:2,nullptr);
        for(unsigned i=0;i<21;++i) scroll_step(1,i==20?0:1,nullptr);
        if(scroll_actions!=45 || scroll_cues!=40 || scroll_presents!=251 || reorder_compare_.top ||
            compare_repeat_.post_frames || compare_refresh_.remaining || viewerDatabase().slotTable()!=scroll_table)
            throw std::runtime_error("Compare held scroll did not stop/reverse at endpoints");
        handleCompareInput(0x80);nba97_reorder_frame(&reorder_screen_.selection,0);
        trace_.log("COMPARE-SCROLL-VERIFY","end actions=45 cues=40 presentations=251; held/reversal/endpoints; draft unchanged");
        // Separate deterministic presentation sequence: no timer, input, audio,
        // palette or selection updates. Repainting alone must not consume RNG.
        prepareFrontendTitle();
        const auto title_parent=reorder_screen_;
        const auto title_table=viewerDatabase().slotTable();
        for(unsigned frame=0;frame<=8;++frame) {
            if(frame) presentFrontendTitle();
            const auto title_state=frontend_title_.state();
            const auto seed=frontend_rng_;
            const auto image=renderReorder();
            if(renderReorder().rgba!=image.rgba || seed!=frontend_rng_ ||
                std::memcmp(&title_state,&frontend_title_.state(),sizeof(title_state)) ||
                std::memcmp(&title_parent,&reorder_screen_,sizeof(title_parent)) ||
                title_table!=viewerDatabase().slotTable())
                throw std::runtime_error("title repaint mutated motion, parent or draft");
            writePpm(image,options_.reorder_capture_dir/("title-reorder-"+std::to_string(frame)+".ppm"));
            std::string vertices;
            for(int i=0;i<8;++i) {if(i) vertices+=",";vertices+=std::to_string(frontend_title_.corners()[i]);}
            trace_.log("TITLE-VERIFY","frame="+std::to_string(frame)+" rng="+std::to_string(seed)+
                " phase="+std::to_string(title_state.next)+" draws="+std::to_string(frontend_rng_draws_)+" xy="+vertices);
        }
        trace_.log("REORDER-CAPTURE", "129 screen/Help/View/Compare/notice/speech/palette/refresh/arrow/scroll/photo-wait/title frames; real key handlers; live database unchanged; original parity not asserted");
        return 0;
    }

    void loadMenuSprites(const std::filesystem::path& root) {
        static constexpr const char* tags[] = {
            "Bkge","Bkgf","Bkgg","Bkgh","help","ba09",
            "brte","brtf","brtg","brth","brle","brri",
            "brba","brbb","brbc","brbd","brbe","brbf","brbg","brbh",
            "ba13","ba14","ba25",
            "c00a","c01a","c02a","c03a","c04a",
            "c05a","c06a","c07a","c08a",
            "c09a","c10a","c11a","c12a",
            "c13a","c14a","c15a","c17a",
            "c00g","c01g","c02g","c03g","c04g","c05g",
            "o15a","o15b","o04a","o04b","o03a","o03b",
            "o06a","o06b","o05a","o05b","XXL1","XXR2"
        };
        for (const char* tag : tags) {
            const auto path = root / (std::string(tag) + ".png");
            if (!std::filesystem::exists(path))
                throw std::runtime_error("missing decoded ZSET1 sprite: " + path.string() +
                                         " (run scripts/decode_menu_assets.ps1)");
            PshImage image = load_png_image(path);
            // SHPM/PS1 indexed textures use palette index zero as transparent.
            // The generic PNG converter preserves its RGB value but emits an
            // opaque alpha channel, so restore the game's sprite semantics for
            // every overlay.  The Bkg* strips are the only opaque screen tiles.
            if (std::string_view(tag).rfind("Bkg", 0) != 0) {
                for (std::size_t at = 0; at < image.rgba.size(); at += 4)
                    if (image.rgba[at] == 0 && image.rgba[at + 1] == 0 &&
                        image.rgba[at + 2] == 0)
                        image.rgba[at + 3] = 0;
            }
            menu_sprites_.emplace(tag, std::move(image));
        }
        trace_.log("MENU-SPRITE", "ZSET1.PSP original first-screen pack decoded locally: " +
                                  std::to_string(menu_sprites_.size()) + " sprites");
        trace_.log("RECOVERED", "0x80030CDC selects ZSET1.PSP; 0x80031A88(0) loads screen 0");
    }

    void loadRecoveredBottomSprites(const std::filesystem::path& root,
                                    nba97::MenuSpritePack& destination,
                                    bool rosters) {
        static constexpr const char* common[] = {
            "Bkga","Bkgb","Bkgc","Bkgd","Bkge","Bkgf","Bkgg","Bkgh","help","tria",
            "brta","brtb","brtc","brtd","brba","brbb","brbc","brbd",
            "brte","brtf","brtg","brth","brle","brri",
            "brbe","brbf","brbg","brbh","frml","XXL1","XXR2"
        };
        const auto load_one = [&](const char* tag) {
            const auto path = root / (std::string(tag) + ".png");
            if (!std::filesystem::exists(path))
                throw std::runtime_error("missing decoded frontend sprite: " + path.string() +
                                         " (run scripts/extract_assetpacks.ps1)");
            PshImage image = load_png_image(path);
            if (std::string_view(tag).rfind("Bkg", 0) != 0) {
                for (std::size_t at = 0; at < image.rgba.size(); at += 4)
                    if (image.rgba[at] == 0 && image.rgba[at + 1] == 0 &&
                        image.rgba[at + 2] == 0)
                        image.rgba[at + 3] = 0;
            }
            destination.emplace(tag, std::move(image));
        };
        for (const char* tag : common) load_one(tag);
        if (rosters) {
            load_one("ba24");
            load_one("ba35");
            load_one("dflt");
            static constexpr const char* team_logos[] = {
                "atlR","bosR","chaR","chiR","cleR","dalR","denR","detR","golR","houR",
                "indR","lacR","lalR","miaR","milR","minR","nwjR","nwyR","orlR","phiR",
                "phoR","porR","sacR","sanR","seaR","torR","utaR","vanR","wasR"
            };
            for (const char* tag : team_logos) load_one(tag);
            for (int i = 0; i < 16; ++i) {
                char tag[8]{};
                sprintf_s(tag, "c%02dd", i);
                load_one(tag);
            }
            load_one("c06r");
            load_one("c14r");
        } else {
            load_one("ba37");
        }
        trace_.log("MENU-SPRITE", std::string(rosters ? "ZSET4.PSP Rosters" :
                                               "ZSET7.PSP Users") +
                                  " original pack decoded locally: " +
                                  std::to_string(destination.size()) + " sprites");
    }

    void loadPlayerCardTeamLogos(const std::filesystem::path& root) {
        static constexpr const char* tags[] = {
            "atlL","bosL","chaL","chiL","cleL","dalL","denL","detL","golL","houL",
            "indL","lacL","lalL","miaL","milL","minL","nwjL","nwyL","orlL","phiL",
            "phoL","porL","sacL","sanL","seaL","torL","utaL","vanL","wasL"
        };
        for (const char* tag : tags) {
            const auto path = root / (std::string(tag) + ".png");
            if (!std::filesystem::exists(path))
                throw std::runtime_error("missing decoded ZLOGOS sprite: " + path.string() +
                                         " (run scripts/decode_team_logos.ps1)");
            PshImage image = load_png_image(path);
            for (std::size_t at = 0; at < image.rgba.size(); at += 4)
                if (image.rgba[at] == 0 && image.rgba[at + 1] == 0 && image.rgba[at + 2] == 0)
                    image.rgba[at + 3] = 0;
            roster_sprites_.emplace(tag, std::move(image));
        }
        trace_.log("PLAYER-TEAM-LOGO",
            "ZLOGOS.PSH exact 44x48 player-card crests decoded locally; Chicago tag chi -> chiL");
    }

    void loadCreatePlayerSprites(const std::filesystem::path& root) {
        static constexpr const char* tags[] = {
            "Bkga","Bkgb","Bkgc","Bkgd","Bkge","Bkgf","Bkgg","Bkgh","help","ba05","ba06","ba07",
            "brta","brtb","brtc","brtd","brte","brtf","brtg","brth","brle","brri",
            "brba","brbb","brbc","brbd","brbe","brbf","brbg","brbh","XXL1","XXR2",
            "c00c","c01c","c02c","c03c","c04c","c05c","c00r","c04r"
        };
        for (const char* tag : tags) {
            const auto path = root / (std::string(tag) + ".png");
            if (!std::filesystem::exists(path))
                throw std::runtime_error("missing decoded ZSET5 Create Player sprite: " +
                    path.string() + " (run scripts/extract_assetpacks.ps1)");
            PshImage image = load_png_image(path);
            if (std::string_view(tag).rfind("Bkg", 0) != 0) {
                for (std::size_t at = 0; at < image.rgba.size(); at += 4)
                    if (image.rgba[at] == 0 && image.rgba[at + 1] == 0 &&
                        image.rgba[at + 2] == 0)
                        image.rgba[at + 3] = 0;
            }
            create_player_sprites_.emplace(tag, std::move(image));
        }
        trace_.log("MENU-SPRITE", "ZSET5.PSP Create Player screen 0x1F decoded locally: " +
            std::to_string(create_player_sprites_.size()) + " exact/runtime sprites");
    }

    void loadPlayerCardSprites(const std::filesystem::path& root) {
        static constexpr const char* tags[] = {
            "Bkge","Bkgf","Bkgg","Bkgh","help",
            "brta","brtb","brtc","brtd","brle","brri",
            "brba","brbb","brbc","brbd",
            "ba41","o18a","o18b","cros","shot","wait",
            "atlZ","bosZ","chaZ","chiZ","cleZ","dalZ","denZ","detZ","golZ","houZ",
            "indZ","lacZ","lalZ","miaZ","milZ","minZ","nwjZ","nwyZ","orlZ","phiZ",
            "phoZ","porZ","sacZ","sanZ","seaZ","torZ","utaZ","vanZ","wasZ","xfrZ"
        };
        for (const char* tag : tags) {
            const auto path = root / (std::string(tag) + ".png");
            if (!std::filesystem::exists(path))
                throw std::runtime_error("missing decoded ZSET8 View Player sprite: " +
                                         path.string() +
                                         " (run scripts/extract_assetpacks.ps1)");
            PshImage image = load_png_image(path);
            if (std::string_view(tag).rfind("Bkg", 0) != 0) {
                for (std::size_t at = 0; at < image.rgba.size(); at += 4)
                    if (image.rgba[at] == 0 && image.rgba[at + 1] == 0 &&
                        image.rgba[at + 2] == 0)
                        image.rgba[at + 3] = 0;
            }
            player_sprites_.emplace(tag, std::move(image));
        }
        trace_.log("MENU-SPRITE",
            "ZSET8.PSP View Player state 0x24 decoded locally: " +
            std::to_string(player_sprites_.size()) + " exact sprites");
    }

    void validateMenuAsset(const std::filesystem::path& path,
                           std::uintmax_t expected_size) {
        if (!std::filesystem::exists(path) || std::filesystem::file_size(path) != expected_size)
            throw std::runtime_error("missing or invalid private menu asset: " + path.string());
        trace_.log("MENU-ASSET", path.string() + " bytes=" + std::to_string(expected_size));
    }

    void updatePlayerPhoto(bool wait_for_capture = false) {
        do {
            auto result = player_photo_loader_.poll(wait_for_capture);
            if (result.event == nba97::PlayerPhotoLoader::Event::None) break;
            if (result.event == nba97::PlayerPhotoLoader::Event::Stale) {
                trace_.log("PLAYER-PHOTO", "discard stale completion record=" + std::to_string(result.record) +
                    "; newer selection or closed card; no stale photo publication");
            } else if (result.event == nba97::PlayerPhotoLoader::Event::Ready) {
                roster_portrait_ = std::move(result.image);
                roster_portrait_loaded_ = true;
                trace_.log("PLAYER-PHOTO", "ready record=" + std::to_string(result.record) +
                    " 180x156; 80030E78 photo=on city=on wait=covered; actual native decode, no simulated CD delay");
            } else {
                roster_portrait_loaded_ = false;
                trace_.log("PLAYER-PHOTO-ERROR", "record=" + std::to_string(result.record) + " " + result.error +
                    "; photo stays hidden; wait retained; navigation remains available");
            }
        } while (wait_for_capture && player_photo_loader_.state().pending);
    }

    void loadSelectedPlayerCardAssets(bool new_card = true) {
        const auto* player = roster_viewer_.selectedPlayer(viewerDatabase());
        if (new_card) {
            player_photo_loader_.reset();
            roster_portrait_loaded_ = false;
        }
        roster_cool_facts_available_ = false;
        if (!player) return;
        const auto root = options_.asset_root / "menu";
        // Z1PORT physical record zero is the original fallback.  Player n's
        // direct portrait is physical record n+1 (Longley id37 -> record38).
        const auto portrait_root = root / "Z1PORT-decoded";
        const auto portrait_path_for = [&portrait_root](std::uint32_t record) {
            wchar_t name[32]{};
            swprintf_s(name, L"player_%03u.png", record);
            return portrait_root / name;
        };
        auto portrait_path = portrait_path_for(static_cast<std::uint32_t>(player->id) + 1);
        if (!std::filesystem::exists(portrait_path)) portrait_path = portrait_path_for(0);
        const auto record = portrait_path == portrait_path_for(0) ? 0 : static_cast<int>(player->id) + 1;
        if (player_photo_loader_.request(record, portrait_path)) {
            roster_portrait_loaded_ = false;
            trace_.log("PLAYER-PHOTO", "request player=" + std::to_string(player->id) + " record=" +
                std::to_string(record) + " source=" + portrait_path.string() +
                "; 800310D8 photo=off wait=visible city=" +
                (player_photo_loader_.state().city_enabled ? "retained" : "hidden") + "; latest-request bounded queue");
        }

        std::ifstream input(root / "Z1COOL.IDX", std::ios::binary);
        std::vector<std::uint8_t> index((std::istreambuf_iterator<char>(input)), {});
        const nba97::CoolFactIndexView facts(index);
        std::uint8_t fact_mask=0;
        for(unsigned variant=0;variant<5;++variant)
            if(facts.lookup(player->id,variant).bytes) fact_mask|=static_cast<std::uint8_t>(1u<<variant);
        roster_cool_facts_available_=fact_mask!=0;
        nba97_fact_refresh(&cool_fact_selection_,fact_mask);
        resolveCoolFactChoice("refresh-593F0");
        trace_.log("PLAYER-CARD", "state=0x24 player=" + player->displayName() +
            " id=" + std::to_string(player->id) + " portrait-record=" +
            std::to_string(static_cast<unsigned>(player->id) + 1) + " 180x156 cool-facts=" +
            (roster_cool_facts_available_ ? "enabled" : "disabled"));
    }

    void loadTeamRosterBackgrounds(const std::filesystem::path& root) {
        nba97_semantic_trace_record(0x8002FE58u);
        static constexpr const char* teams[] = {
            "atl","bos","cha","chi","cle","dal","den","det","gol","hou",
            "ind","lac","lal","mia","mil","min","nwj","nwy","orl","phi",
            "pho","por","sac","san","sea","tor","uta","van","was","xea"};
        static constexpr const char* strips[] = {
            "Bkga", "Bkgb", "Bkgc", "Bkgd", "Bkge", "Bkgf", "Bkgg", "Bkgh"};
        for (const char* team : teams) {
            for (const char* strip : strips) {
                const auto path = root / team / (std::string(strip) + ".png");
                if (!std::filesystem::exists(path))
                    throw std::runtime_error("missing team-paletted roster background: " + path.string() +
                                             " (run scripts/extract_assetpacks.ps1)");
                auto image = load_png_image(path);
                if (image.width != 128 || image.height != 240)
                    throw std::runtime_error("invalid team roster background dimensions: " +
                                             path.string() + " (expected 128x240)");
                bool lower_half_has_art = false;
                for (int y = 120; y < image.height && !lower_half_has_art; ++y) {
                    for (int x = 0; x < image.width; ++x) {
                        const std::size_t at =
                            (static_cast<std::size_t>(y) * image.width + x) * 4;
                        if (image.rgba[at + 3] != 0 &&
                            (image.rgba[at] != 0 || image.rgba[at + 1] != 0 ||
                             image.rgba[at + 2] != 0)) {
                            lower_half_has_art = true;
                            break;
                        }
                    }
                }
                if (!lower_half_has_art)
                    throw std::runtime_error("team roster background has empty lower half: " +
                                             path.string() + " (re-run corrected decoder)");
                roster_sprites_.emplace(std::string(team) + strip, std::move(image));
            }
        }
        trace_.log("ROSTER-PALETTE",
            "29 team palettes plus original record29 xeaP for Compare free agents; Bkga-h loaded from local-only ZTMPAL output");
    }

    void loadMenuCards(const std::filesystem::path& root) {
        const bool deterministic_capture = !options_.rosters_menu_capture_dir.empty() ||
                                           !options_.create_player_capture_dir.empty() ||
                                           !options_.team_select_capture_dir.empty();
        const auto random_seed = !deterministic_capture
            ? static_cast<std::mt19937::result_type>(
                  std::chrono::high_resolution_clock::now().time_since_epoch().count() ^
                  static_cast<long long>(GetTickCount64()))
            : static_cast<std::mt19937::result_type>(0x57ce4u);
        std::mt19937 random(random_seed);
        std::uniform_int_distribution<int> card_index(0, 94);

        const auto load_card = [&](int index) {
            wchar_t name[32]{};
            swprintf_s(name, L"card_%02d.png", index);
            const auto path = root / name;
            if (!std::filesystem::exists(path))
                throw std::runtime_error("missing decoded ZCARD portrait: " + path.string() +
                                         " (run scripts/decode_card_assets.ps1)");
            auto image = load_png_image(path);
            if (image.width != 69 || image.height != 63)
                throw std::runtime_error("invalid decoded ZCARD portrait dimensions: " + path.string());
            for (std::size_t at = 0; at < image.rgba.size(); at += 4)
                if (image.rgba[at] == 0 && image.rgba[at + 1] == 0 &&
                    image.rgba[at + 2] == 0)
                    image.rgba[at + 3] = 0;
            return image;
        };
        const auto fill_random_pack = [&](auto& destination) {
            std::array<bool, 32> used_residue{};
            std::vector<int> selected;
            selected.reserve(destination.size());
            for (auto& image : destination) {
                int index = 0;
                do {
                    index = card_index(random);
                } while (used_residue[static_cast<std::size_t>(index & 31)]);
                used_residue[static_cast<std::size_t>(index & 31)] = true;
                image = load_card(index);
                selected.push_back(index);
            }
            return selected;
        };

        const auto setup_indices = fill_random_pack(menu_cards_);
        const auto roster_indices = fill_random_pack(roster_menu_cards_);
        const auto create_indices = fill_random_pack(create_player_cards_);
        const auto index_list = [](const auto& indices) {
            std::string result;
            for (const int index : indices) {
                if (!result.empty()) result += ',';
                result += std::to_string(index);
            }
            return result;
        };
        trace_.log("MENU-CARD", "0x80031A88 loaded 95 ZCARD.BIN images; setup PRNG picks=" +
            index_list(setup_indices) + "; Rosters PRNG picks=" + index_list(roster_indices) +
            "; Create Player PRNG picks=" + index_list(create_indices));
        trace_.log("RECOVERED", "0x80031F48 flags=0x20 replacement: per-screen PRNG with unique index&31 mask; 4 setup, 8 Rosters and 3 Create Player 69x63 SHPP composites");
        trace_.log("ROSTER-MENU", "FUN_80057CE4 state=9: 8 choices x 3 runtime objects (normal/selected/ZCARD); draw-order=back-row 0..3 then front-row 4..7");
        trace_.log("ROSTER-STACK", "FEONLY screen-9 table 0x80094ED4 records 16..39: back plates=(65,81)/(165,75)/(270,75)/(365,81), front plates=(50,116)/(150,110)/(255,110)/(345,116); pair dy=35; ZCARD origins individually authored");
        trace_.log("ROSTER-LOCK", "FUN_80057C48 Reset requires roster snapshot delta and no special-state override; FUN_80057A98 Injuries requires active context plus any non-zero value among 536 injury bytes; FUN_800399C4 state 0x80 routes locked c06/c14 indices through the red1 CLUT while preserving white labels and blocking focus");
        trace_.log("ROSTER-FLASH", "FUN_8003F240 toggles the selected object's normal/selected plate for 12 vblanks; the underlying plate and ZCARD composite remain present in both phases");
        trace_.log("ROSTER-SFX", "FUN_8003D930 + live no$psx: down/up/left/right map ZCURSOR ids 1/2/3/4; FUN_8003F240 select=id6 and toggles selected plate 12 vblanks");
    }

    int captureCreatePlayer() {
        const auto output = options_.create_player_capture_dir;
        std::filesystem::create_directories(output);
        const auto context = createPlayerContext();
        nba97_create_menu_open(&create_player_menu_, &created_players_, context);
        const auto capture = [&](const char* name, std::uint32_t elapsed = 0) {
            writePpm(nba97::renderCreatePlayerMenu(create_player_menu_, menu_font_,
                create_player_sprites_, create_player_cards_, elapsed), output / name);
        };
        capture("empty-new-selected.ppm");
        capture("empty-title-phase.ppm", 75);
        if (create_player_menu_.selected != 1 || create_player_menu_.enabled[0] ||
            !create_player_menu_.enabled[1] || create_player_menu_.enabled[2])
            throw std::runtime_error("Create Player empty-menu predicates failed");

        Nba97CreateEditor editor{};
        if (!nba97_create_editor_open_new(&editor, &created_players_))
            throw std::runtime_error("Create Player editor fixture failed");
        const auto capture_editor = [&](const char* name, std::uint32_t elapsed = 0,
                                        const Nba97CreateNameEditor* name_editor = nullptr) {
            writePpm(nba97::renderCreatePlayerEditor(editor, roster_database_, menu_font_,
                create_player_sprites_, elapsed, create_player_preview_.get(),name_editor,
                &control_font_), output / name);
        };
        capture_editor("editor-first-required.ppm");
        capture_editor("editor-selector-gold.ppm", 340);
        {
            const auto& descriptor=create_player_help_pack_->descriptor(0x22,0);
            Nba97HelpModal modal{};
            if(nba97_help_open(&modal,descriptor.rect,0x20)!=NBA97_HELP_OPEN_SOUND)
                throw std::runtime_error("Create Player editor Help fixture failed");
            for(int tick=0;tick<40 && !nba97_help_text_visible(&modal);++tick)
                nba97_help_tick(&modal,0);
            auto image=nba97::renderCreatePlayerEditor(editor,roster_database_,menu_font_,
                create_player_sprites_,0,create_player_preview_.get(),nullptr,&control_font_);
            create_player_help_pack_->draw(image,control_font_,descriptor,modal);
            writePpm(image,output/"editor-help-page-1.ppm");
        }
        Nba97CreateNameEditor name_editor{};
        if(nba97_create_name_begin(&editor,&name_editor)!=6)
            throw std::runtime_error("Create Player inline name fixture failed");
        capture_editor("editor-name-inline-open.ppm",0,&name_editor);
        nba97_create_name_input(&editor,&name_editor,NBA97_CREATE_NAME_NEXT_CHARACTER);
        capture_editor("editor-name-inline-cycle.ppm",0,&name_editor);
        {
            const auto editor_before=editor;
            const auto name_before=name_editor;
            const auto& descriptor=create_player_help_pack_->descriptor(0x22,1);
            Nba97HelpModal modal{};
            if(nba97_help_open(&modal,descriptor.rect,0x20)!=NBA97_HELP_OPEN_SOUND)
                throw std::runtime_error("Create Player name Help fixture failed");
            for(int tick=0;tick<40 && !nba97_help_text_visible(&modal);++tick)
                nba97_help_tick(&modal,0);
            auto image=nba97::renderCreatePlayerEditor(editor,roster_database_,menu_font_,
                create_player_sprites_,0,create_player_preview_.get(),&name_editor,
                &control_font_);
            create_player_help_pack_->draw(image,control_font_,descriptor,modal);
            writePpm(image,output/"editor-name-help-modal.ppm");
            if(std::memcmp(&editor,&editor_before,sizeof(editor))!=0 ||
               std::memcmp(&name_editor,&name_before,sizeof(name_editor))!=0)
                throw std::runtime_error("Create Player name Help changed inline transaction");
        }
        if(nba97_create_name_cancel(&editor,&name_editor)!=10||editor.first_name[0]!='\0')
            throw std::runtime_error("Create Player inline name cancellation did not restore 13 bytes");
        nba97_create_editor_append_letter(&editor, 'A');
        nba97_create_editor_move(&editor, 1);
        nba97_create_editor_append_letter(&editor, 'B');
        editor.selected_field = NBA97_CREATE_HAIR_STYLE;
        editor.previous_visible_first_field = editor.visible_first_field =
            NBA97_CREATE_HAIR_STYLE - 3;
        editor.scroll_ticks_remaining = 0;
        if(std::getenv("NBA97_CREATE_ORIGINAL_VRAM")) {
            /* The synchronized no$psx dump contains an uppercase A on the
               generated 100x30 jersey-surname page. */
            const auto saved_last=editor.last_name[0];
            editor.last_name[0]='A';
            capture_editor("editor-appearance-original-vram.ppm");
            editor.last_name[0]=saved_last;
        }
        editor.skin_tone = 1;
        editor.hair_style = 1;
        editor.hair_color = 1;
        editor.facial_hair = 1;
        /* Match the captured PS1 appearance reference (Boston, values=1)
           without changing the Atlanta full-body texture-audit fixture. */
        editor.team = 1;
        capture_editor("editor-appearance-layer.ppm");
        editor.height_inches=64;
        capture_editor("editor-appearance-height-64.ppm");
        editor.height_inches=90;
        capture_editor("editor-appearance-height-90.ppm");
        editor.height_inches=63;
        editor.team = 0;
        editor.selected_field = NBA97_CREATE_HAND;
        editor.previous_visible_first_field = editor.visible_first_field = 2;
        nba97_create_editor_move(&editor, 1);
        capture_editor("editor-layer-scroll-enter.ppm");
        /* Clip 0 is the retail one-frame appearance pose. Verify animation
           against clip 1 while the model is in the full-body field state. */
        capture_editor("editor-model-motion-phase.ppm", 330);
        for(int tick=0;tick<3;++tick) nba97_create_editor_tick(&editor);
        capture_editor("editor-layer-scroll-mid.ppm");
        for(int tick=0;tick<3;++tick) nba97_create_editor_tick(&editor);
        capture_editor("editor-layer-scroll-settled.ppm");
        editor.selected_field = NBA97_CREATE_DRIBBLING;
        editor.previous_visible_first_field = editor.visible_first_field =
            NBA97_CREATE_FIELD_GOALS + 12;
        editor.scroll_ticks_remaining = 0;
        capture_editor("editor-ratings-final.ppm");
        const auto capture_rating_help=[&](const char* file) {
            const auto page=nba97_create_editor_help_index(&editor,nullptr);
            const auto& descriptor=create_player_help_pack_->descriptor(0x22,page);
            Nba97HelpModal modal{};
            nba97_help_open(&modal,descriptor.rect,0x20);
            for(int tick=0;tick<40 && !nba97_help_text_visible(&modal);++tick)
                nba97_help_tick(&modal,0);
            auto frame=nba97::renderCreatePlayerEditor(editor,roster_database_,menu_font_,
                create_player_sprites_,340,create_player_preview_.get(),nullptr,&control_font_);
            create_player_help_pack_->draw(frame,control_font_,descriptor,modal);
            writePpm(frame,output/file);
        };
        capture_rating_help("editor-rating-help-page-4.ppm");
        if(nba97_create_editor_toggle_rating_group(&editor)!=6)
            throw std::runtime_error("rating-group Cross fixture failed");
        capture_editor("editor-rating-group-selected.ppm",340);
        capture_rating_help("editor-rating-help-page-5.ppm");
        if(!nba97_create_editor_adjust(&editor,-1) || editor.ratings[16]!=99)
            throw std::runtime_error("rating-group all-minimum wrap fixture failed");
        capture_editor("editor-rating-group-wrapped.ppm",340);
        if(nba97_create_editor_toggle_rating_group(&editor)!=6 ||
           editor.selected_field!=NBA97_CREATE_DRIBBLING)
            throw std::runtime_error("rating-group remembered return-row fixture failed");
        capture_editor("editor-rating-individual-return.ppm",340);
        nba97_create_editor_cancel(&editor.txn);

        Nba97CreateEditorTxn txn{};
        if (!nba97_create_editor_begin_new(&txn, &created_players_) ||
            !nba97_create_editor_accept(&txn, &created_players_))
            throw std::runtime_error("Create Player single-record fixture failed");
        std::snprintf(created_players_.metadata[0].first_name,
                      sizeof(created_players_.metadata[0].first_name),"A");
        std::snprintf(created_players_.metadata[0].last_name,
                      sizeof(created_players_.metadata[0].last_name),"B");
        created_players_.metadata[0].team=0;
        created_players_.metadata[0].roster_slot=5;
        nba97_create_menu_open(&create_player_menu_, &created_players_, context);
        create_player_menu_.selected = 0;
        capture("one-edit-selected.ppm");
        create_player_menu_.selected = 2;
        capture("one-delete-selected.ppm");

        Nba97CreatedPlayerPicker picker{};
        if(!nba97_created_picker_open(&picker,&created_players_,0x21))
            throw std::runtime_error("Create Player Delete picker fixture failed");
        const auto capture_delete=[&](const char* name,std::uint32_t address,
                                      const char* team) {
            Nba97ResetPrompt prompt{};
            if(!(nba97_reset_open(&prompt,create_player_delete_assets_->rect(address),0x800,1)&NBA97_RESET_OPEN))
                throw std::runtime_error("Create Player Delete modal fixture failed");
            for(int tick=0;tick<32 && !nba97_help_text_visible(&prompt.modal);++tick)
                nba97_reset_tick(&prompt,0);
            if(!nba97_help_text_visible(&prompt.modal) || prompt.choice!=0)
                throw std::runtime_error("Create Player Delete modal did not reach original ready phase");
            auto image=nba97::renderCreatedPlayerPicker(picker,created_players_,roster_database_,
                menu_font_,create_player_sprites_,0);
            create_player_delete_assets_->draw(image,address,team,prompt);
            writePpm(image,output/name);
        };
        capture_delete("delete-free-agent.ppm",0x800AF352,"free agents");
        capture_delete("delete-team-bench.ppm",0x800AF3D6,"Boston");
        capture_delete("delete-team-starter.ppm",0x800AF460,"Boston");

        while (nba97_created_count(&created_players_) < NBA97_CREATED_PLAYER_CAPACITY) {
            if (!nba97_create_editor_begin_new(&txn, &created_players_) ||
                !nba97_create_editor_accept(&txn, &created_players_))
                throw std::runtime_error("Create Player full-catalogue fixture failed");
        }
        nba97_create_menu_open(&create_player_menu_, &created_players_, context);
        capture("full-new-disabled.ppm");
        if (create_player_menu_.enabled[1] || create_player_menu_.selected != 0)
            throw std::runtime_error("Create Player full-menu predicates failed");
        trace_.log("CREATE-CAPTURE", "PASS: 27 deterministic 512x240 frames; manager empty/one/full predicates, exact screen-0x22 controller-icon prompts, inline name entry/Select cancel plus authored Help 1/5, 2/5, 4/5 and 5/5, rating-group Cross/return and all-at-limit wrap, recovered height-relative appearance framing, 20-vblank selector pulse, six-vblank bank scroll, articulated ZDOM/mocap phase, appearance/final-ratings layers, and all three exact FEONLY Delete descriptors; pixel-exact PS1 triangle coverage remains separately pending");
        verifyCreatePlayerEditAcceptance(output);
        return 0;
    }

    void verifyCreatePlayerEditAcceptance(const std::filesystem::path& output) {
        // Exercise the real host handlers, not just the C core. This store is
        // a capture-only fixture; never write the user's loaded catalogue.
        created_player_store_.load(output / "editor-acceptance-fixture.n97cpl",created_players_);
        nba97_created_catalog_init(&created_players_);
        Nba97CreateEditor fixture{};
        if(!nba97_create_editor_open_new(&fixture,&created_players_))
            throw std::runtime_error("Edit acceptance fixture could not start");
        std::strcpy(fixture.first_name,"Wrap"); std::strcpy(fixture.last_name,"Test");
        created_player_store_.acceptEditor(fixture,created_players_);
        const auto accepted_catalog=created_players_;
        const auto generation=created_player_store_.generation();
        frontend_page_=nba97::FrontendPage::CreatePlayers;
        const auto open_edit=[&] {
            if(!nba97_created_picker_open(&created_player_picker_,&created_players_,0x20))
                throw std::runtime_error("Edit picker fixture could not start");
            created_player_picker_active_=true;
            handleCreatePlayerKey('C');
            if(!create_player_editor_active_ || created_player_picker_active_)
                throw std::runtime_error("Cross did not enter the actual Edit host path");
        };
        open_edit();
        handleCreatePlayerKey(VK_RETURN);
        if(create_player_editor_active_ || created_player_store_.generation()!=generation)
            throw std::runtime_error("Unchanged Edit Start failed to exit without a write");
        open_edit();
        const auto colleges=createPlayerCollegeCount();
        if(colleges<2 || create_player_editor_.college_count!=colleges)
            throw std::runtime_error("Edit did not initialize the loaded College choices");
        create_player_editor_.selected_field=NBA97_CREATE_COLLEGE;
        handleCreatePlayerKey(VK_LEFT);
        if(create_player_editor_.college!=colleges-1)
            throw std::runtime_error("Actual Edit College Left did not wrap");
        handleCreatePlayerKey(VK_RIGHT);
        if(create_player_editor_.college!=0)
            throw std::runtime_error("Actual Edit College Right did not wrap");
        create_player_editor_.selected_field=NBA97_CREATE_JERSEY_NUMBER;
        handleCreatePlayerKey(VK_LEFT);
        if(create_player_editor_.jersey_number!=99)
            throw std::runtime_error("Actual Edit jersey Left did not wrap");
        handleCreatePlayerKey(VK_RIGHT);
        handleCreatePlayerKey(VK_RETURN);
        if(create_player_editor_active_ || created_player_store_.generation()!=generation ||
           std::memcmp(&created_players_,&accepted_catalog,sizeof(created_players_)))
            throw std::runtime_error("Reverted Edit did not exit as a no-op");
        trace_.log("CREATE-EDIT-REGRESSION","PASS: actual picker/Cross -> Edit, Start unchanged/reverted exit; College choices="+
            std::to_string(colleges)+" Left/Right wrap; jersey Left/Right wrap; catalogue and durable generation unchanged");
    }

    int captureRostersMenu() {
        const auto output = options_.rosters_menu_capture_dir;
        nba97::RecoveredBottomMenu menu;
        menu.open(nba97::FrontendPage::Rosters);
        menu.setRosterCapabilities(false, false);
        menu.setSelected(4);
        const auto render = [&](const std::filesystem::path& name,
                                bool selected_overlay_visible = true,
                                std::uint32_t elapsed_ms = 0) {
            writePpm(nba97::renderRecoveredBottomMenu(
                menu, menu_font_, menu_sprites_, roster_sprites_, users_sprites_,
                roster_menu_cards_, elapsed_ms, selected_overlay_visible), output / name);
        };
        render("rosters_initial.ppm");
        menu.setSelected(3);
        render("rosters_reset_locked_attempt.ppm");
        for (int phase = 0; phase < 12; ++phase) {
            char name[48]{};
            sprintf_s(name, "rosters_select_phase_%02d.ppm", phase);
            render(name, (phase & 1) != 0);
        }
        for (int phase = 0; phase < 4; ++phase) {
            char name[48]{};
            sprintf_s(name, "rosters_title_phase_%02d.ppm", phase);
            render(name, true, static_cast<std::uint32_t>(phase * 75));
        }
        menu.move(1, 0);
        render("rosters_reorder_focused.ppm");
        menu.setRosterCapabilities(true, true);
        menu.setSelected(3);
        render("rosters_reset_enabled.ppm");
        menu.setSelected(7);
        render("rosters_injuries_enabled.ppm");

        const auto audio_root = options_.asset_root / "menu";
        static constexpr std::array<const char*, 6> sound_names{
            "down", "up", "left", "right", "unused_05", "select_flash"};
        std::ofstream metadata(output / "capture.json", std::ios::trunc);
        if (!metadata) throw std::runtime_error("cannot write Rosters menu capture metadata");
        metadata << "{\n  \"schema_version\": 1,\n  \"function\": \"0x80057CE4\",\n"
                 << "  \"stack_y\": [81,75,75,81,116,110,110,116],\n"
                 << "  \"stack_x\": [65,165,270,365,50,150,255,345],\n"
                 << "  \"art_y\": [95,88,89,98,130,123,124,133],\n"
                 << "  \"art_x\": [77,171,282,379,62,156,267,359],\n"
                 << "  \"stack_pair_delta_y\": [35,35,35,35],\n"
                 << "  \"layout_table\": \"0x80094ED4 records 16..39\",\n"
                 << "  \"flash_vblanks\": 12,\n  \"sounds\": [\n";
        for (std::uint32_t sound_id = 1; sound_id <= 6; ++sound_id) {
            char wav_name[64]{};
            char raw_wav_name[64]{};
            sprintf_s(wav_name, "zcursor_%02u_%s.wav", sound_id,
                      sound_names[sound_id - 1]);
            sprintf_s(raw_wav_name, "zcursor_%02u_%s_raw.wav", sound_id,
                      sound_names[sound_id - 1]);
            const auto info = cursor_audio_.exportCursorSound(
                audio_root / "ZCURSOR.VH", audio_root / "ZCURSOR.VB", sound_id,
                output / wav_name);
            cursor_audio_.exportCursorSoundRaw(
                audio_root / "ZCURSOR.VH", audio_root / "ZCURSOR.VB", sound_id,
                output / raw_wav_name);
            metadata << "    {\"id\":" << sound_id << ",\"role\":\""
                     << sound_names[sound_id - 1] << "\",\"file\":\"" << wav_name
                     << "\",\"raw_file\":\"" << raw_wav_name
                     << "\",\"rate\":" << info.sample_rate << ",\"samples\":"
                     << info.rendered_sample_count << ",\"source_samples\":"
                     << info.sample_count << ",\"root_note\":" << info.root_note
                     << ",\"requested_note\":" << info.requested_note
                     << ",\"pitch_cents\":" << info.pitch_cents << "}"
                     << (sound_id == 6 ? "\n" : ",\n");
            trace_.log("ROSTER-SFX", "captured id=" + std::to_string(sound_id) +
                " role=" + sound_names[sound_id - 1] + " rate=" +
                std::to_string(info.sample_rate) + " samples=" +
                std::to_string(info.rendered_sample_count) + " source-samples=" +
                std::to_string(info.sample_count) + " root/request=" +
                std::to_string(info.root_note) + "/" +
                std::to_string(info.requested_note) + " pitch=" +
                std::to_string(info.pitch_cents) + " cents bytes=" +
                std::to_string(info.compressed_bytes));
        }
        const auto repeated_right = cursor_audio_.exportCursorSound(
            audio_root / "ZCURSOR.VH", audio_root / "ZCURSOR.VB", 4,
            output / "zcursor_04_right_repeat.wav");
        trace_.log("ROSTER-SFX-REPEAT", "three consecutive logical Right moves all route "
            "FUN_8003D930 cue id=4; deterministic repeat samples=" +
            std::to_string(repeated_right.rendered_sample_count) + " rate=" +
            std::to_string(repeated_right.sample_rate) + "Hz; runtime WinMM device reused");
        metadata << "  ],\n  \"availability\": {\"reset\":\"roster snapshot differs and no special-state override\","
                    "\"injuries\":\"active context plus one or more non-zero values among 536 injury bytes\"}\n}\n";
        trace_.log("ROSTER-CAPTURE", "deterministic stack, lock variants, 12 flash phases and six recovered WAVs -> " + output.string());
        return 0;
    }

    int captureViewRosters() {
        const auto output = options_.view_rosters_capture_dir;
        roster_viewer_ = nba97::RosterViewer{};
        roster_viewer_.open(roster_database_);
        writePpm(nba97::renderRosterViewer(
            roster_viewer_, roster_database_, menu_font_, roster_sprites_, 340),
            output / "team_atlanta_initial.ppm");

        std::uint32_t elapsed = 100;
        for (int team = 0; team < 3; ++team) {
            roster_viewer_.scanTeam(1, roster_database_, elapsed);
            elapsed += 340;
        }
        writePpm(nba97::renderRosterViewer(
            roster_viewer_, roster_database_, menu_font_, roster_sprites_, elapsed),
            output / "team_chicago_initial.ppm");
        roster_viewer_.toggleHelp();
        writePpm(nba97::renderRosterViewer(
            roster_viewer_, roster_database_, menu_font_, roster_sprites_, elapsed),
            output / "team_chicago_help.ppm");
        roster_viewer_.dismissHelp();
        for (int phase = 0; phase < 40; ++phase) {
            char name[48]{};
            sprintf_s(name, "team_chicago_phase_%02d.ppm", phase);
            writePpm(nba97::renderRosterViewer(
                roster_viewer_, roster_database_, menu_font_, roster_sprites_,
                static_cast<std::uint32_t>(phase * 17)), output / name);
        }

        for (int row = 0; row < 6; ++row)
            roster_viewer_.move(0, 1, roster_database_, elapsed + row * 34);
        writePpm(nba97::renderRosterViewer(
            roster_viewer_, roster_database_, menu_font_, roster_sprites_, elapsed + 340),
            output / "team_chicago_scrolled.ppm");

        nba97::RosterViewer dallas_viewer;
        dallas_viewer.open(roster_database_);
        std::uint32_t dallas_elapsed = 100;
        for (int team = 0; team < 5; ++team) {
            dallas_viewer.scanTeam(1, roster_database_, dallas_elapsed);
            dallas_elapsed += 340;
        }
        writePpm(nba97::renderRosterViewer(
            dallas_viewer, roster_database_, menu_font_, roster_sprites_, dallas_elapsed),
            output / "team_dallas_initial.ppm");

        roster_viewer_ = nba97::RosterViewer{};
        roster_viewer_.open(roster_database_);
        elapsed = 100;
        for (int team = 0; team < 3; ++team) {
            roster_viewer_.scanTeam(1, roster_database_, elapsed);
            elapsed += 340;
        }
        roster_viewer_.activate(roster_database_);
        loadSelectedPlayerCardAssets();
        updatePlayerPhoto(true);
        writePpm(nba97::renderRosterViewer(
            roster_viewer_, roster_database_, menu_font_, player_sprites_, 340,
            roster_portrait_loaded_ ? &roster_portrait_ : nullptr,
            roster_cool_facts_available_, &control_font_),
            output / "player_chicago_initial.ppm");
        roster_viewer_.toggleHelp();
        writePpm(nba97::renderRosterViewer(
            roster_viewer_, roster_database_, menu_font_, player_sprites_, 340,
            roster_portrait_loaded_ ? &roster_portrait_ : nullptr,
            roster_cool_facts_available_, &control_font_),
            output / "player_chicago_help.ppm");
        roster_viewer_.dismissHelp();
        roster_viewer_.cycleCategory(-1);
        writePpm(nba97::renderRosterViewer(
            roster_viewer_, roster_database_, menu_font_, player_sprites_, 340,
            roster_portrait_loaded_ ? &roster_portrait_ : nullptr,
            roster_cool_facts_available_, &control_font_),
            output / "player_chicago_layer_1.ppm");
        roster_viewer_.cycleCategory(1);
        for (int phase = 0; phase < 40; ++phase) {
            char name[48]{};
            sprintf_s(name, "player_chicago_phase_%02d.ppm", phase);
            writePpm(nba97::renderRosterViewer(
                roster_viewer_, roster_database_, menu_font_, player_sprites_,
                static_cast<std::uint32_t>(phase * 17),
                roster_portrait_loaded_ ? &roster_portrait_ : nullptr,
                roster_cool_facts_available_, &control_font_), output / name);
        }

        // The user's original no$psx reference was captured one row below the
        // recovered initial state: "games started" has scrolled out and the
        // upper marker is visible beside "games played". Capture that exact
        // scenario separately so visual verification never rewards changing
        // the correct initial list to fit a scrolled reference frame.
        roster_viewer_.move(0, 1, roster_database_, 680);
        writePpm(nba97::renderRosterViewer(
            roster_viewer_, roster_database_, menu_font_, player_sprites_, 680,
            roster_portrait_loaded_ ? &roster_portrait_ : nullptr,
            roster_cool_facts_available_, &control_font_),
            output / "player_chicago_scrolled.ppm");
        for (int phase = 0; phase < 40; ++phase) {
            char name[56]{};
            sprintf_s(name, "player_chicago_scrolled_phase_%02d.ppm", phase);
            writePpm(nba97::renderRosterViewer(
                roster_viewer_, roster_database_, menu_font_, player_sprites_,
                static_cast<std::uint32_t>(phase * 17),
                roster_portrait_loaded_ ? &roster_portrait_ : nullptr,
                roster_cool_facts_available_, &control_font_), output / name);
        }

        // Same identity/layer/top as the manually captured original Rodman
        // card. This is a compositor checkpoint, not a Re-order runtime trace.
        roster_viewer_ = nba97::RosterViewer{};
        roster_viewer_.open(roster_database_);
        for (int team=0; team<3; ++team)
            roster_viewer_.scanTeam(1, roster_database_, 100 + team*340);
        roster_viewer_.activate(roster_database_);
        roster_viewer_.move(1, 0, roster_database_, 1200);
        const auto* rodman = roster_viewer_.selectedPlayer(roster_database_);
        if (!rodman || rodman->last_name != "Rodman" || roster_viewer_.firstVisiblePlayerStat()!=0)
            throw std::runtime_error("Rodman photo checkpoint identity/top mismatch");
        loadSelectedPlayerCardAssets();
        writePpm(nba97::renderRosterViewer(roster_viewer_, roster_database_, menu_font_,
            player_sprites_, 0, nullptr, roster_cool_facts_available_, &control_font_),
            output / "player_chicago_rodman_wait.ppm");
        updatePlayerPhoto(true);
        if (!roster_portrait_loaded_) throw std::runtime_error("Rodman photo checkpoint failed to load");
        writePpm(nba97::renderRosterViewer(roster_viewer_, roster_database_, menu_font_,
            player_sprites_, 0, &roster_portrait_, roster_cool_facts_available_, &control_font_),
            output / "player_chicago_rodman_ready.ppm");

        std::ofstream metadata(output / "capture.json", std::ios::trunc);
        if (!metadata) throw std::runtime_error("cannot write View Rosters capture metadata");
        metadata << "{\n"
                 << "  \"schema_version\": 1,\n"
                 << "  \"width\": 512,\n"
                 << "  \"height\": 240,\n"
                 << "  \"team\": \"Chicago Bulls\",\n"
                 << "  \"visible_rows\": 6,\n"
                 << "  \"captures\": [\"team_atlanta_initial.ppm\", "
                    "\"team_chicago_initial.ppm\", \"team_chicago_help.ppm\", "
                    "\"team_chicago_scrolled.ppm\", \"team_dallas_initial.ppm\", "
                    "\"player_chicago_initial.ppm\", "
                    "\"player_chicago_help.ppm\", \"player_chicago_layer_1.ppm\", "
                    "\"player_chicago_scrolled.ppm\", \"player_chicago_rodman_wait.ppm\", "
                    "\"player_chicago_rodman_ready.ppm\"]\n"
                 << "}\n";
        trace_.log("VERIFY-CAPTURE", "deterministic View Rosters frames -> " + output.string());
        return 0;
    }

    int runSelfTest() {
        if (nba97_frontend_sfx_volume(0) != 0 ||
            nba97_frontend_sfx_volume(9) != 108 ||
            nba97_frontend_sfx_volume(11) != 127 ||
            nba97_frontend_music_volume(0) != 0 ||
            nba97_frontend_music_volume(8) != 120 ||
            nba97_frontend_music_volume(9) != 127)
            throw std::runtime_error("shared C frontend-volume recovery self-test failed");
        trace_.log("SELF-TEST", "shared C recovery boundary values validated for FUN_8002F124/FUN_8002F258");
        if (!flow_.update(options_.transition_ms, options_.transition_ms) ||
            flow_.screen() != nba97::BootScreen::LegalScreen)
            throw std::runtime_error("load -> legal self-test failed");
        if (!flow_.update(options_.transition_ms, options_.transition_ms) ||
            flow_.screen() != nba97::BootScreen::IntroVideo)
            throw std::runtime_error("legal -> intro-video self-test failed");
        if (!std::filesystem::exists(intro_movie_))
            throw std::runtime_error("missing decoded Z0ZTITLE.XA playback asset");
        if (!flow_.completeIntro() || flow_.screen() != nba97::BootScreen::TitleScreen)
            throw std::runtime_error("intro-video -> title self-test failed");
        if (!flow_.enterMainMenu() || flow_.screen() != nba97::BootScreen::MainMenu)
            throw std::runtime_error("title -> game-setup self-test failed");
        if (roster_database_.teams().size() != 29 || roster_database_.players().size() != 493 ||
            roster_database_.assignedPlayerCount() != 362 || roster_database_.freeAgentCount() != 67 ||
            roster_database_.unlistedPlayerCount() != 64 ||
            !roster_database_.player(0) || !roster_database_.team(28))
            throw std::runtime_error("external roster database validation self-test failed");
        std::size_t copied_team_slots = 0;
        for (const auto& team : roster_database_.teams()) {
            for (const auto id : team.roster) {
                if (id != UINT16_MAX) {
                    if (roster_database_.rosterOwner(id) != static_cast<std::int16_t>(team.id))
                        throw std::runtime_error("Rosters_CopySlotTable team membership self-test failed");
                    ++copied_team_slots;
                }
            }
        }
        std::size_t copied_free_agents = 0;
        bool free_agent_hole = false;
        for (const auto id : roster_database_.freeAgentSlots()) {
            if (id == UINT16_MAX) {
                free_agent_hole = true;
            } else {
                if (free_agent_hole || roster_database_.rosterOwner(id) != 29)
                    throw std::runtime_error("Rosters_CopySlotTable free-agent tail self-test failed");
                ++copied_free_agents;
            }
        }
        if (copied_team_slots != 362 || copied_free_agents != 67 ||
            roster_database_.rosterOwner(429) != -1 ||
            roster_database_.rosterOwner(492) != -1 ||
            !roster_database_.derivedTeamRatingsDirty())
            throw std::runtime_error("Rosters_CopySlotTable population self-test failed");
        trace_.log("SELF-TEST", "Rosters_CopySlotTable PASS: 435 team slots + 100 free-agent slots; membership=362 team/67 free/64 hidden; trailing 33 holes preserved");
        std::size_t resolved_slots = 0;
        std::size_t empty_slots = 0;
        for (std::int16_t team_id = 0; team_id < 29; ++team_id) {
            const auto slots = roster_database_.resolveTeamSlots(team_id);
            for (const auto* slot : slots) slot ? ++resolved_slots : ++empty_slots;
        }
        const auto invalid_low = roster_database_.resolveTeamSlots(-1);
        const auto invalid_high = roster_database_.resolveTeamSlots(29);
        if (resolved_slots != 362 || empty_slots != 73 ||
            std::any_of(invalid_low.begin(), invalid_low.end(), [](const auto* p) { return p; }) ||
            std::any_of(invalid_high.begin(), invalid_high.end(), [](const auto* p) { return p; }))
            throw std::runtime_error("Rosters_ResolveTeamSlots fixed-slot/bounds self-test failed");
        std::size_t special_fallbacks = 0;
        for (std::int16_t team_id = 0; team_id < 29; ++team_id) {
            const auto normal = roster_database_.resolveTeamSlots(team_id, false);
            const auto special = roster_database_.resolveTeamSlots(team_id, true);
            for (std::size_t slot = 0; slot < normal.size(); ++slot) {
                if (normal[slot] && normal[slot]->regular_stats_index == 0) {
                    if (!special[slot] || special[slot] == normal[slot])
                        throw std::runtime_error("Rosters_ResolveTeamSlots special fallback self-test failed");
                    ++special_fallbacks;
                }
            }
        }
        if (special_fallbacks == 0)
            throw std::runtime_error("Rosters_ResolveTeamSlots special branch was not exercised");
        trace_.log("SELF-TEST", "Rosters_ResolveTeamSlots PASS: 29x15 slots, 362 resolved, 73 null, both bounds rejected, all " +
            std::to_string(special_fallbacks) + " special-mode replacements exercised through private 5x5 table");
        const auto* montross = roster_database_.player(66);
        if (!montross || montross->last_name != "Montross" ||
            montross->jersey_number != 0xff || montross->jerseyNumberText() != "00")
            throw std::runtime_error("FEONLY jersey 00 sentinel formatting self-test failed");
        frontend_music_.start(options_.asset_root / "menu" / "ZTMENU1.CNK", 0);
        const auto music_info = frontend_music_.info();
        if (music_info.codec != 0x06 || music_info.channels != 2 ||
            music_info.sample_rate != 44100 || music_info.sample_count != 7421609 ||
            music_info.data_blocks != 2524)
            throw std::runtime_error("frontend SCHl/PSX-ADPCM decoder self-test failed");
        frontend_music_.stop();
        menu_.reset();
        menu_.setActiveUserProfiles(active_user_profiles_);
        const auto jiggle_a = nba97::renderGameSetupMenu(
            menu_, title_source_, menu_font_, menu_sprites_, menu_cards_, 0);
        const auto jiggle_b = nba97::renderGameSetupMenu(
            menu_, title_source_, menu_font_, menu_sprites_, menu_cards_, 137);
        if (jiggle_a.rgba == jiggle_b.rgba)
            throw std::runtime_error("game-setup title jiggle self-test failed");
        if (!menu_.moveHorizontal(1) || std::string(menu_.selectedLabel()) != "mode")
            throw std::runtime_error("game-setup option navigation self-test failed");
        if (!menu_.moveVertical(1) || std::string(menu_.selectedLabel()) != "rules" ||
            !menu_.moveHorizontal(1) || std::string(menu_.selectedLabel()) != "options")
            throw std::runtime_error("game-setup button navigation self-test failed");
        if (!menu_.moveHorizontal(1) || std::string(menu_.selectedLabel()) != "rosters" ||
            !menu_.moveHorizontal(1) || std::string(menu_.selectedLabel()) != "card")
            throw std::runtime_error("disabled Users skip self-test failed");
        nba97::RecoveredBottomMenu recovered_menu;
        if (recoveredMenuDirectionSound(1, 0) != 4 ||
            recoveredMenuDirectionSound(-1, 0) != 3 ||
            recoveredMenuDirectionSound(0, -1) != 2 ||
            recoveredMenuDirectionSound(0, 1) != 1)
            throw std::runtime_error("Rosters FUN_8003D930 direction sound mapping self-test failed");
        recovered_menu.open(nba97::FrontendPage::Rosters);
        const auto roster_a = nba97::renderRecoveredBottomMenu(
            recovered_menu, menu_font_, menu_sprites_, roster_sprites_, users_sprites_,
            roster_menu_cards_, 0);
        recovered_menu.move(1, 0);
        const auto roster_b = nba97::renderRecoveredBottomMenu(
            recovered_menu, menu_font_, menu_sprites_, roster_sprites_, users_sprites_,
            roster_menu_cards_, 0);
        if (roster_a.rgba == roster_b.rgba || recovered_menu.count() != 8)
            throw std::runtime_error("Rosters original-card navigation self-test failed");
        recovered_menu.setSelected(3);
        if (recovered_menu.selected() == 3 || recovered_menu.enabled(3) ||
            recovered_menu.enabled(7))
            throw std::runtime_error("Rosters Reset/Injuries recovered lock self-test failed");
        recovered_menu.setRosterCapabilities(true, true);
        recovered_menu.setSelected(3);
        if (recovered_menu.selected() != 3 || !recovered_menu.enabled(7))
            throw std::runtime_error("Rosters Reset/Injuries unlock self-test failed");
        const auto flash_on = nba97::renderRecoveredBottomMenu(
            recovered_menu, menu_font_, menu_sprites_, roster_sprites_, users_sprites_,
            roster_menu_cards_, 0, true);
        const auto flash_off = nba97::renderRecoveredBottomMenu(
            recovered_menu, menu_font_, menu_sprites_, roster_sprites_, users_sprites_,
            roster_menu_cards_, 0, false);
        if (flash_on.rgba == flash_off.rgba)
            throw std::runtime_error("Rosters recovered selection-overlay flash self-test failed");
        nba97::RosterViewer constructor_test;
        constructor_test.open(roster_database_);
        static constexpr std::array<int, 6> constructor_defaults{0, 15, 32, 32, 44, 44};
        for (std::size_t category = 0; category < constructor_defaults.size(); ++category) {
            if (constructor_test.displayIndexForCategory(category) != constructor_defaults[category])
                throw std::runtime_error("Rosters_ConstructViewer descriptor defaults self-test failed");
        }
        const auto& default_controller = constructor_test.constructionController();
        if (constructor_test.category() != 2 || constructor_test.displayIndex() != 32 ||
            constructor_test.constructionLayerLimit() != 3 ||
            constructor_test.constructionDescriptorCount() != 28 ||
            constructor_test.constructedDescriptorIndex() != 32 ||
            !constructor_test.constructionObjectFlagsAgree() ||
            constructor_test.constructionObjectStateFlag() ||
            !constructor_test.constructionControllerBound() ||
            constructor_test.constructionControllerPhase() != 0 ||
            default_controller.slot != 0x12 || default_controller.flags != 0x10004u ||
            default_controller.object_id != 0x11 || default_controller.value != 7)
            throw std::runtime_error("Rosters_ConstructViewer normal construction self-test failed");
        constructor_test.cycleDisplay(1);
        constructor_test.cycleCategory(1);
        constructor_test.cycleDisplay(1);
        constructor_test.open(roster_database_);
        if (constructor_test.category() != 2 || constructor_test.displayIndex() != 32 ||
            constructor_test.displayIndexForCategory(3) != 32)
            throw std::runtime_error("Rosters_ConstructViewer reopen reset self-test failed");
        constructor_test.construct(roster_database_, {2, true, true});
        if (constructor_test.category() != 5 || constructor_test.displayIndex() != 44 ||
            constructor_test.constructionLayerLimit() != 5 ||
            constructor_test.constructedDescriptorIndex() != 44 ||
            !constructor_test.constructionObjectFlagsAgree() ||
            !constructor_test.constructionObjectStateFlag())
            throw std::runtime_error("Rosters_ConstructViewer special construction self-test failed");
        constructor_test.construct(roster_database_, {2, false, false});
        if (constructor_test.category() != 2 || constructor_test.constructionLayerLimit() != 3)
            throw std::runtime_error("Rosters_ConstructViewer inactive-special self-test failed");
        trace_.log("SELF-TEST", "Rosters_ConstructViewer PASS: six descriptor defaults reset; normal layer=2/limit=3; active special layer=5/limit=5; paired object flags and controller 0x12 bound");
        nba97::RosterViewer run_viewer_test;
        run_viewer_test.runViewer(roster_database_, {{-1, 5, 29}, 2, true, 7});
        if (run_viewer_test.playerIndex() != 0 ||
            run_viewer_test.firstVisiblePlayer() != 0 ||
            run_viewer_test.teamIndex() != 7 || run_viewer_test.category() != 5 ||
            run_viewer_test.runInputState() != 0x10 ||
            run_viewer_test.runCancelSentinel() != 0x100 ||
            !run_viewer_test.runDrawCallbackBound())
            throw std::runtime_error("Rosters_RunViewer sentinel/special-team self-test failed");
        run_viewer_test.runViewer(roster_database_, {{2, 0, 29}, 1, true, 12});
        if (run_viewer_test.teamIndex() != 3 || run_viewer_test.category() != 4)
            throw std::runtime_error("Rosters_RunViewer default-team sentinel self-test failed");
        run_viewer_test.runViewer(roster_database_, {{2, 0, 1}, 0, false, 0});
        if (!run_viewer_test.move(0, 1, roster_database_) ||
            !run_viewer_test.move(1, 0, roster_database_))
            throw std::runtime_error("Rosters_RunViewer writeback setup failed");
        run_viewer_test.finishRun(2, 0);
        const auto accepted_state = run_viewer_test.savedRunState();
        if (accepted_state.player_index != 3 || accepted_state.team_index != 2 ||
            run_viewer_test.lastRunResult() != 2 || run_viewer_test.lastRunExitStatus() != 0)
            throw std::runtime_error("Rosters_RunViewer accepted writeback self-test failed");
        run_viewer_test.move(0, 1, roster_database_);
        run_viewer_test.move(1, 0, roster_database_);
        run_viewer_test.finishRun(3, 0x100);
        if (run_viewer_test.playerIndex() != 3 || run_viewer_test.teamIndex() != 2 ||
            run_viewer_test.lastRunResult() != 3 ||
            run_viewer_test.lastRunExitStatus() != 0x100)
            throw std::runtime_error("Rosters_RunViewer cancel suppression self-test failed");
        trace_.log("SELF-TEST", "Rosters_RunViewer PASS: -1 player repair, team 29 -> active/3 branches, state 0x10 callback boundary, accepted writeback, and cancel 0x100 suppression");
        nba97::RosterViewer stat_layer_test;
        stat_layer_test.open(roster_database_);
        if (!stat_layer_test.cycleCategory(1) || stat_layer_test.category() != 3) {
            throw std::runtime_error("Player_ChangeStatLayer forward setup failed");
        }
        const auto same_extent = stat_layer_test.lastStatLayerChange();
        if (same_extent.input_mask != 0x2000 || same_extent.layer_label_object != 0x1b ||
            same_extent.descriptor_table != 2 || same_extent.descriptor_last_index != 23 ||
            same_extent.descriptor_extent_changed || same_extent.primary_animation_reset ||
            same_extent.primary_refresh_count != 6 || !same_extent.secondary_layout ||
            same_extent.secondary_animation_reset || same_extent.secondary_refresh_count != 6 ||
            !same_extent.controller_page_zeroed_for_rebuild || !same_extent.layout_rebuilt) {
            throw std::runtime_error("Player_ChangeStatLayer same-extent side effects failed");
        }
        stat_layer_test.move(0, 1, roster_database_);
        if (!stat_layer_test.cycleCategory(1) || stat_layer_test.category() != 0 ||
            stat_layer_test.firstVisiblePlayerStat() != 0) {
            throw std::runtime_error("Player_ChangeStatLayer normal wrap/reset failed");
        }
        const auto changed_extent = stat_layer_test.lastStatLayerChange();
        if (changed_extent.descriptor_table != 0 ||
            changed_extent.descriptor_last_index != 13 ||
            !changed_extent.descriptor_extent_changed ||
            !changed_extent.primary_animation_reset ||
            !changed_extent.secondary_animation_reset) {
            throw std::runtime_error("Player_ChangeStatLayer changed-extent animation failed");
        }
        stat_layer_test.construct(roster_database_, {1, true, false, true});
        if (stat_layer_test.category() != 4 || !stat_layer_test.cycleCategory(-1) ||
            stat_layer_test.category() != 3 || !stat_layer_test.cycleCategory(1) ||
            stat_layer_test.category() != 0 ||
            !stat_layer_test.lastStatLayerChange().skipped_layer_four) {
            throw std::runtime_error("Player_ChangeStatLayer restricted layer-4 skip failed");
        }
        stat_layer_test.construct(roster_database_, {1, true, false, false});
        stat_layer_test.cycleCategory(-1);
        if (!stat_layer_test.cycleCategory(1) || stat_layer_test.category() != 4 ||
            stat_layer_test.lastStatLayerChange().skipped_layer_four) {
            throw std::runtime_error("Player_ChangeStatLayer available layer-4 branch failed");
        }
        trace_.log("SELF-TEST", "Player_ChangeStatLayer PASS: inclusive constructor limit wrap, restricted layer-4 skip, label object 0x1B (not audio), 14/17/24 descriptor extents, conditional dual-group animation, row refresh, page-zero rebuild, and extent-dependent stat-scroll reset");
        nba97::RosterViewer roster_viewer_test;
        roster_viewer_test.open(roster_database_);
        const auto view_a = nba97::renderRosterViewer(
            roster_viewer_test, roster_database_, menu_font_, roster_sprites_, 0);
        if (view_a.width != 512 || view_a.height != 240 ||
            roster_database_.version() != 5 ||
            roster_database_.teams()[0].roster.size() != 15 ||
            roster_viewer_test.category() != 2 || roster_viewer_test.displayIndex() != 32)
            throw std::runtime_error("View Rosters fixed 15-slot state 0x10 setup failed");
        const auto* roster_sample = roster_viewer_test.selectedPlayer(roster_database_);
        if (!roster_sample ||
            roster_database_.playerAttribute(*roster_sample, 7).empty() ||
            !roster_viewer_test.cycleDisplay(1) || roster_viewer_test.displayIndex() != 33)
            throw std::runtime_error("View Rosters roster-pack descriptor setup failed");
        const auto points_view = nba97::renderRosterViewer(
            roster_viewer_test, roster_database_, menu_font_, roster_sprites_, 0);
        if (points_view.rgba == view_a.rgba || !roster_viewer_test.cycleCategory(1) ||
            roster_viewer_test.category() != 3 || roster_viewer_test.displayIndex() != 32 ||
            !roster_viewer_test.cycleCategory(-1) || roster_viewer_test.displayIndex() != 33)
            throw std::runtime_error("View Rosters category/field memory self-test failed");
        nba97::RosterViewer palette_test;
        palette_test.open(roster_database_);
        if (!palette_test.move(1, 0, roster_database_, 100))
            throw std::runtime_error("View Rosters palette-transition setup failed");
        const auto palette_start = nba97::renderRosterViewer(
            palette_test, roster_database_, menu_font_, roster_sprites_, 100);
        const auto palette_mid = nba97::renderRosterViewer(
            palette_test, roster_database_, menu_font_, roster_sprites_, 236);
        const auto palette_end = nba97::renderRosterViewer(
            palette_test, roster_database_, menu_font_, roster_sprites_, 372);
        bool found_palette_interpolation = false;
        for (int y = 170; y < 185 && !found_palette_interpolation; ++y) {
            for (int x = 16; x < 496; ++x) {
                const std::size_t at = (static_cast<std::size_t>(y) * 512 + x) * 4;
                bool start_differs_from_end = false;
                bool mid_differs_from_start = false;
                bool mid_differs_from_end = false;
                for (int channel = 0; channel < 3; ++channel) {
                    start_differs_from_end |=
                        palette_start.rgba[at + channel] != palette_end.rgba[at + channel];
                    mid_differs_from_start |=
                        palette_mid.rgba[at + channel] != palette_start.rgba[at + channel];
                    mid_differs_from_end |=
                        palette_mid.rgba[at + channel] != palette_end.rgba[at + channel];
                }
                if (start_differs_from_end && mid_differs_from_start && mid_differs_from_end) {
                    found_palette_interpolation = true;
                    break;
                }
            }
        }
        if (!found_palette_interpolation)
            throw std::runtime_error("View Rosters 17-tick palette interpolation self-test failed");
        if (roster_viewer_test.move(0, -1, roster_database_))
            throw std::runtime_error("View Rosters incorrectly wrapped above the first player");
        for (int row = 0; row < 6; ++row) {
            if (!roster_viewer_test.move(0, 1, roster_database_))
                throw std::runtime_error("View Rosters six-row scroll setup failed");
        }
        if (roster_viewer_test.playerIndex() != 6 ||
            roster_viewer_test.firstVisiblePlayer() != 1 ||
            !roster_viewer_test.move(1, 0, roster_database_) ||
            roster_viewer_test.teamIndex() != 1 ||
            roster_viewer_test.playerIndex() != 6 ||
            roster_viewer_test.firstVisiblePlayer() != 1)
            throw std::runtime_error("View Rosters team/player navigation self-test failed");
        roster_viewer_test.activate(roster_database_);
        if (roster_viewer_test.mode() != nba97::RosterViewMode::PlayerCard)
            throw std::runtime_error(
                "View Rosters action 0x10 did not push player-card state 0x24");
        for (int row = 0; row < 18; ++row) {
            if (!roster_viewer_test.move(0, 1, roster_database_))
                throw std::runtime_error("View Player 24-row statistic scroll self-test failed");
        }
        if (roster_viewer_test.firstVisiblePlayerStat() != 18 ||
            roster_viewer_test.move(0, 1, roster_database_) ||
            !roster_viewer_test.move(0, -1, roster_database_) ||
            roster_viewer_test.firstVisiblePlayerStat() != 17)
            throw std::runtime_error("View Player statistic scroll boundaries self-test failed");
        nba97::RosterViewer wrap_test;
        wrap_test.open(roster_database_);
        wrap_test.activate(roster_database_);
        for (int row = 0; row < 3; ++row)
            wrap_test.move(0, 1, roster_database_);
        const auto* wrap_team = wrap_test.selectedTeam(roster_database_);
        std::size_t wrap_count = 0;
        while (wrap_team && wrap_count < wrap_team->roster.size() &&
               roster_database_.player(wrap_team->roster[wrap_count]))
            ++wrap_count;
        if (wrap_count < 2 || !wrap_test.move(-1, 0, roster_database_) ||
            wrap_test.playerIndex() != wrap_count - 1 ||
            wrap_test.firstVisiblePlayerStat() != 3)
            throw std::runtime_error("View Player FUN_80059928 wrapped cycling self-test failed");
        const auto previous_cycle = wrap_test.lastPlayerCycle();
        const auto* wrapped_player = wrap_test.selectedPlayer(roster_database_);
        if (previous_cycle.input_mask != 8 || !previous_cycle.wrapped ||
            previous_cycle.previous_slot != 0 ||
            previous_cycle.current_slot != wrap_count - 1 ||
            previous_cycle.roster_count != wrap_count || !wrapped_player ||
            previous_cycle.resolved_player_id != wrapped_player->id ||
            !previous_cycle.global_player_id_updated ||
            previous_cycle.transition_frames != 2 ||
            previous_cycle.header_refresh_start != 0x18 ||
            previous_cycle.header_refresh_count != 3 ||
            previous_cycle.visible_row_refresh_count != 6 ||
            !previous_cycle.cool_fact_stopped ||
            !previous_cycle.player_card_refreshed ||
            !previous_cycle.stat_scroll_preserved)
            throw std::runtime_error("Rosters_CyclePlayer previous/refresh side effects failed");
        if (!wrap_test.move(1, 0, roster_database_) || wrap_test.playerIndex() != 0 ||
            !wrap_test.lastPlayerCycle().wrapped ||
            wrap_test.lastPlayerCycle().input_mask != 4 ||
            wrap_test.firstVisiblePlayerStat() != 3)
            throw std::runtime_error("Rosters_CyclePlayer next wrap/preservation failed");
        if (!wrap_test.cyclePlayer(1, roster_database_, {1, false, false, 7, 5, 0x23}) ||
            wrap_test.lastPlayerCycle().header_refresh_start != 0x1e ||
            wrap_test.lastPlayerCycle().header_refresh_count != 3 ||
            wrap_test.lastPlayerCycle().visible_row_refresh_start != 7 ||
            wrap_test.lastPlayerCycle().visible_row_refresh_count != 5 ||
            wrap_test.lastPlayerCycle().cool_fact_stopped ||
            wrap_test.lastPlayerCycle().player_card_refreshed)
            throw std::runtime_error("Rosters_CyclePlayer mirrored-page refresh failed");
        const auto before_blocked = wrap_test.playerIndex();
        if (wrap_test.cyclePlayer(1, roster_database_, {0, true, true}) ||
            wrap_test.playerIndex() != before_blocked ||
            !wrap_test.lastPlayerCycle().blocked_special_roster ||
            !wrap_test.lastPlayerCycle().input_latch_cleared ||
            wrap_test.lastPlayerCycle().transition_frames != 0)
            throw std::runtime_error("Rosters_CyclePlayer special-roster lock failed");
        trace_.log("SELF-TEST", "Rosters_CyclePlayer PASS: input 8/forward masks, bidirectional count-based wrap, player-ID mirror, two transition frames, page 0/other descriptor groups, visible-row refresh, state-0x24 cool-fact stop/rebuild, special descriptor-29 lock, and stat-scroll preservation");
        nba97::RosterViewer player_card_run_test;
        player_card_run_test.open(roster_database_);
        for (int row = 0; row < 6; ++row)
            player_card_run_test.move(0, 1, roster_database_);
        if (!player_card_run_test.runPlayerCard(roster_database_))
            throw std::runtime_error("Player_RunCard normal entry failed");
        const auto normal_card = player_card_run_test.playerCardRunState();
        const auto* normal_card_player = player_card_run_test.selectedPlayer(roster_database_);
        if (!normal_card.portrait_archive_requested ||
            !normal_card.cool_fact_archive_requested ||
            std::string(normal_card.portrait_index) != "Z1PORT.IDX" ||
            std::string(normal_card.portrait_archive) != "Z1PORT.BIG" ||
            std::string(normal_card.cool_fact_index) != "Z1COOL.IDX" ||
            std::string(normal_card.cool_fact_archive) != "Z1COOL.BIG" ||
            normal_card.object_count != 0x1d || normal_card.visible_row_count != 6 ||
            normal_card.manager_byte_2 != 0x70 ||
            normal_card.manager_short_20 != 0x36 ||
            normal_card.manager_short_22 != 0xef ||
            normal_card.manager_mirror_flags != std::array<int, 2>{1, 1} ||
            !normal_card.number_descriptor_bound || !normal_card.position_descriptor_bound ||
            normal_card.layout_id != 0x24 || normal_card.current_stat_layer != 2 ||
            normal_card.stat_layer_limit != 3 || normal_card.descriptor_last_index != 0x17 ||
            normal_card.descriptor_end != 0x38 || normal_card.selected_team != 0 ||
            normal_card.selected_roster_slot != 6 || normal_card.selected_visible_row != 5 ||
            !normal_card_player || normal_card.selected_player_id != normal_card_player->id ||
            !normal_card.player_context_initialized ||
            !normal_card.cool_fact_choices_refreshed ||
            !normal_card.parent_roster_viewer_selected || normal_card.parent_active_page != 0 ||
            normal_card.column_starts != std::array<int, 4>{0, 0, 0, 0} ||
            normal_card.column_steps != std::array<int, 4>{2, 2, 2, 2} ||
            normal_card.previous_fact_choice != -1 || normal_card.previous_fact_record != -1 ||
            !normal_card.controller.bound || normal_card.controller.slot != 0x39 ||
            normal_card.controller.flags != 0x10000u ||
            normal_card.controller.object_id != 0x10 || normal_card.controller.value != 0x0d ||
            normal_card.frontend_state_during_loop != 0x11 || normal_card.input_state != -1 ||
            normal_card.draw_callback != 0x8005A280u ||
            normal_card.action_callback != 0x8005A3FCu ||
            !normal_card.input_loop_bound || normal_card.frontend_state_after_loop != -1 ||
            !normal_card.teardown_scheduled || normal_card.teardown_complete)
            throw std::runtime_error("Player_RunCard normal lifecycle self-test failed");
        player_card_run_test.returnToRoster();
        if (player_card_run_test.playerCardRunState().input_loop_bound ||
            !player_card_run_test.playerCardRunState().teardown_complete)
            throw std::runtime_error("Player_RunCard teardown self-test failed");
        if (!player_card_run_test.runPlayerCard(roster_database_, {1, false, 0x11, 1}) ||
            player_card_run_test.playerCardRunState().current_stat_layer != 4 ||
            player_card_run_test.playerCardRunState().stat_layer_limit != 4 ||
            player_card_run_test.playerCardRunState().parent_roster_viewer_selected ||
            player_card_run_test.playerCardRunState().parent_active_page != 1)
            throw std::runtime_error("Player_RunCard special-layer branch failed");
        if (!player_card_run_test.runPlayerCard(roster_database_, {1, true, 0x10, 0}) ||
            player_card_run_test.playerCardRunState().current_stat_layer != 5 ||
            player_card_run_test.playerCardRunState().stat_layer_limit != 5)
            throw std::runtime_error("Player_RunCard forced-layer-five branch failed");
        if (!player_card_run_test.runPlayerCard(roster_database_, {0, true, 0x10, 0}) ||
            player_card_run_test.playerCardRunState().current_stat_layer != 2 ||
            player_card_run_test.playerCardRunState().stat_layer_limit != 3)
            throw std::runtime_error("Player_RunCard inactive-force branch failed");
        trace_.log("SELF-TEST", "Player_RunCard PASS: four private archive requests, manager 29/6/0x70/0x36/0xEF, layout 0x24, normal/special/forced layers, selected context and cool-fact refresh, four 0/2 columns, -1 sentinels, controller 0x39 binding, frontend 0x11/state -1 callbacks, and teardown restoration");
        nba97::RecoveredAudioPlayer cool_fact_test;
        const auto cool_fact_info = cool_fact_test.inspectCoolFact(
            options_.asset_root / "menu" / "Z1COOL.IDX",
            options_.asset_root / "menu" / "Z1COOL.BIG", 0, 0);
        if (cool_fact_info.record != 1 || cool_fact_info.sample_rate != 16000 ||
            cool_fact_info.sample_count != 286001 || cool_fact_info.compressed_bytes != 163440 ||
            cool_fact_test.isPlaying())
            throw std::runtime_error("View Player Cool Fact archive/decode regression failed");
        trace_.log("COOL-FACT-TEST",
            "player=0 variant=0 logical=0 physical-record=1 decoded PSX-ADPCM samples=286001 rate=16000 without playback");
        roster_viewer_test.returnToRoster();
        if (roster_viewer_test.mode() != nba97::RosterViewMode::TeamRoster ||
            roster_viewer_test.playerIndex() != 6 ||
            roster_viewer_test.firstVisiblePlayer() != 1)
            throw std::runtime_error("View Player return did not preserve roster state");
        roster_viewer_test.commit();
        const auto committed_team = roster_viewer_test.teamIndex();
        const auto committed_player = roster_viewer_test.playerIndex();
        roster_viewer_test.move(1, 0, roster_database_);
        roster_viewer_test.move(0, 1, roster_database_);
        roster_viewer_test.cancel();
        if (roster_viewer_test.teamIndex() != committed_team ||
            roster_viewer_test.playerIndex() != committed_player)
            throw std::runtime_error("View Rosters transactional cancel self-test failed");
        recovered_menu.open(nba97::FrontendPage::Card);
        const auto card_a = nba97::renderRecoveredBottomMenu(
            recovered_menu, menu_font_, menu_sprites_, roster_sprites_, users_sprites_,
            roster_menu_cards_, 0);
        recovered_menu.move(1, 0);
        const auto card_b = nba97::renderRecoveredBottomMenu(
            recovered_menu, menu_font_, menu_sprites_, roster_sprites_, users_sprites_,
            roster_menu_cards_, 0);
        if (card_a.rgba == card_b.rgba || recovered_menu.count() != 3)
            throw std::runtime_error("Memory Card original-card navigation self-test failed");
        menu_.setActiveUserProfiles(1);
        menu_.reset();
        menu_.moveVertical(1);
        menu_.moveHorizontal(1);
        menu_.moveHorizontal(1);
        if (!menu_.moveHorizontal(1) || std::string(menu_.selectedLabel()) != "users")
            throw std::runtime_error("Users enabled-profile self-test failed");
        menu_.setActiveUserProfiles(active_user_profiles_);
        menu_.reset();
        if (!menu_.hover(457, 210) || std::string(menu_.selectedLabel()) != "card")
            throw std::runtime_error("game-setup mouse-hover self-test failed");
        nba97::FrontendSettings test_settings;
        nba97::SettingsMenu test_menu;
        test_menu.open(nba97::FrontendPage::Rules);
        if (!test_settings.adjustRule(0, 1) || test_settings.style() != 2)
            throw std::runtime_error("Rules Custom transition self-test failed");
        test_menu.open(nba97::FrontendPage::Options);
        if (!test_settings.adjustOption(0, 1))
            throw std::runtime_error("Options edit self-test failed");
        const auto test_path = options_.settings_path.parent_path() / "frontend_settings.selftest.ini";
        test_settings.save(test_path);
        nba97::FrontendSettings restored;
        if (!restored.load(test_path) || restored.style() != test_settings.style() ||
            restored.rule(0) != test_settings.rule(0) ||
            restored.option(0) != test_settings.option(0))
            throw std::runtime_error("settings relaunch persistence self-test failed");
        std::error_code cleanup_error;
        std::filesystem::remove(test_path, cleanup_error);
        const auto profile_test_path = options_.profiles_path.parent_path() / "user_profiles.selftest.n97sav";
        std::filesystem::remove(profile_test_path, cleanup_error);
        std::filesystem::remove(std::filesystem::path(profile_test_path.wstring() + L".bak"), cleanup_error);
        nba97::UserProfileStore profile_test;
        if (profile_test.load(profile_test_path) != nba97::ProfileLoadStatus::NewStore ||
            !profile_test.create("PLAYER ONE") || profile_test.create("player one") ||
            !profile_test.create("PLAYER TWO") || profile_test.profiles().size() != 2)
            throw std::runtime_error("profile create/duplicate validation self-test failed");
        nba97::UserProfileStore profile_restored;
        if (profile_restored.load(profile_test_path) != nba97::ProfileLoadStatus::Loaded ||
            profile_restored.profiles().size() != 2 ||
            profile_restored.profiles()[0].name != "PLAYER ONE" ||
            !profile_restored.rename(0, "CAPTAIN") || !profile_restored.erase(1))
            throw std::runtime_error("versioned profile persistence self-test failed");
        nba97::UserProfileStore profile_final;
        if (profile_final.load(profile_test_path) != nba97::ProfileLoadStatus::Loaded ||
            profile_final.profiles().size() != 1 || profile_final.profiles()[0].name != "CAPTAIN" ||
            profile_final.generation() != 4)
            throw std::runtime_error("profile generation/update self-test failed");
        std::filesystem::remove(profile_test_path, cleanup_error);
        std::filesystem::remove(std::filesystem::path(profile_test_path.wstring() + L".bak"), cleanup_error);
        writeSemanticTraceReport();
        writeRosterScenarioReport();
        trace_.log("SELF-TEST", "PASS: boot, menus/settings, View Rosters state 0x10, View Player 24-row scroll and FUN_80059928 wrap, versioned profiles, external roster database, all 2,524 frontend-music blocks, and 11 native semantic checkpoints validated");
        return 0;
    }

    void writeRosterScenarioReport() {
        struct ScenarioEvent {
            std::string name;
            bool changed;
            std::size_t team;
            std::size_t player;
            std::size_t first_visible;
            std::size_t first_stat;
            int category;
            int display;
            const char* mode;
            bool help_visible;
        };
        struct Scenario {
            std::string id;
            std::vector<ScenarioEvent> events;
            std::vector<std::uint32_t> sequence;
        };
        std::vector<Scenario> scenarios;
        const auto capture = [](const char* name, bool changed,
                                const nba97::RosterViewer& viewer) {
            return ScenarioEvent{
                name, changed, viewer.teamIndex(), viewer.playerIndex(),
                viewer.firstVisiblePlayer(), viewer.firstVisiblePlayerStat(),
                viewer.category(), viewer.displayIndex(),
                viewer.mode() == nba97::RosterViewMode::PlayerCard
                    ? "player_card" : "team_roster",
                viewer.helpVisible()};
        };
        const auto finish_sequence = []() {
            std::array<std::uint32_t, NBA97_SEMANTIC_TRACE_SEQUENCE_CAPACITY> values{};
            const std::size_t count =
                nba97_semantic_trace_copy(values.data(), values.size());
            return std::vector<std::uint32_t>(values.begin(), values.begin() + count);
        };

        const auto begin = [&scenarios](const char* id) -> Scenario& {
            scenarios.push_back(Scenario{id, {}, {}});
            nba97_semantic_trace_reset();
            return scenarios.back();
        };
        const auto finish = [&finish_sequence](Scenario& scenario) {
            scenario.sequence = finish_sequence();
        };

        Scenario& opening = begin("open_defaults");
        nba97::RosterViewer opening_viewer;
        opening_viewer.open(roster_database_);
        opening.events.push_back(capture("open", true, opening_viewer));
        finish(opening);

        Scenario& roster_scroll = begin("roster_scroll_boundaries");
        nba97::RosterViewer roster_scroll_viewer;
        roster_scroll_viewer.open(roster_database_);
        roster_scroll.events.push_back(capture(
            "up_at_top", roster_scroll_viewer.move(0, -1, roster_database_),
            roster_scroll_viewer));
        bool five_down = true;
        for (int row = 0; row < 5; ++row)
            five_down &= roster_scroll_viewer.move(0, 1, roster_database_);
        roster_scroll.events.push_back(capture("down_5_no_scroll", five_down,
                                                roster_scroll_viewer));
        roster_scroll.events.push_back(capture(
            "down_6_scrolls_window",
            roster_scroll_viewer.move(0, 1, roster_database_), roster_scroll_viewer));
        bool to_bottom = false;
        while (roster_scroll_viewer.move(0, 1, roster_database_))
            to_bottom = true;
        roster_scroll.events.push_back(capture("at_last_valid_player", to_bottom,
                                                roster_scroll_viewer));
        roster_scroll.events.push_back(capture(
            "down_at_bottom", roster_scroll_viewer.move(0, 1, roster_database_),
            roster_scroll_viewer));
        bool to_top = false;
        while (roster_scroll_viewer.move(0, -1, roster_database_))
            to_top = true;
        roster_scroll.events.push_back(capture("returned_to_top", to_top,
                                                roster_scroll_viewer));
        finish(roster_scroll);

        Scenario& team_wrap = begin("roster_team_wrap");
        nba97::RosterViewer team_wrap_viewer;
        team_wrap_viewer.open(roster_database_);
        for (int row = 0; row < 6; ++row)
            team_wrap_viewer.move(0, 1, roster_database_);
        team_wrap.events.push_back(capture(
            "previous_from_first", team_wrap_viewer.move(-1, 0, roster_database_),
            team_wrap_viewer));
        team_wrap.events.push_back(capture(
            "next_from_last", team_wrap_viewer.move(1, 0, roster_database_),
            team_wrap_viewer));
        finish(team_wrap);

        Scenario& transition = begin("player_enter_return");
        nba97::RosterViewer transition_viewer;
        transition_viewer.open(roster_database_);
        for (int row = 0; row < 6; ++row)
            transition_viewer.move(0, 1, roster_database_);
        transition_viewer.activate(roster_database_);
        transition.events.push_back(capture("enter_player", true, transition_viewer));
        transition_viewer.returnToRoster();
        transition.events.push_back(capture("return_preserves_row", true, transition_viewer));
        finish(transition);

        Scenario& navigation = begin("view_rosters_navigation");
        nba97::RosterViewer viewer;
        viewer.open(roster_database_);
        navigation.events.push_back(capture("open", true, viewer));
        bool six_down = true;
        for (int row = 0; row < 6; ++row)
            six_down &= viewer.move(0, 1, roster_database_);
        navigation.events.push_back(capture("down_6", six_down, viewer));
        navigation.events.push_back(capture(
            "team_next", viewer.move(1, 0, roster_database_), viewer));
        viewer.activate(roster_database_);
        navigation.events.push_back(capture("activate_player", true, viewer));
        navigation.events.push_back(capture(
            "stat_layer_next", viewer.cycleCategory(1), viewer));
        bool stats_down = true;
        for (int row = 0; row < 18; ++row)
            stats_down &= viewer.move(0, 1, roster_database_);
        navigation.events.push_back(capture("stats_down_18", stats_down, viewer));
        navigation.events.push_back(capture(
            "stats_down_boundary", viewer.move(0, 1, roster_database_), viewer));
        navigation.events.push_back(capture(
            "stats_up", viewer.move(0, -1, roster_database_), viewer));
        viewer.returnToRoster();
        navigation.events.push_back(capture("return_to_roster", true, viewer));
        viewer.commit();
        viewer.move(1, 0, roster_database_);
        viewer.move(0, 1, roster_database_);
        viewer.cancel();
        navigation.events.push_back(capture("cancel_restores_commit", true, viewer));
        finish(navigation);

        Scenario& wrap = begin("view_player_wrap");
        nba97::RosterViewer wrap_viewer;
        wrap_viewer.open(roster_database_);
        wrap.events.push_back(capture("open", true, wrap_viewer));
        wrap_viewer.activate(roster_database_);
        wrap.events.push_back(capture("activate_player", true, wrap_viewer));
        wrap.events.push_back(capture(
            "previous_from_first", wrap_viewer.move(-1, 0, roster_database_),
            wrap_viewer));
        wrap.events.push_back(capture(
            "next_from_last", wrap_viewer.move(1, 0, roster_database_),
            wrap_viewer));
        finish(wrap);

        Scenario& stat_scroll = begin("player_stat_scroll_boundaries");
        nba97::RosterViewer stat_scroll_viewer;
        stat_scroll_viewer.open(roster_database_);
        stat_scroll_viewer.activate(roster_database_);
        stat_scroll.events.push_back(capture(
            "up_at_top", stat_scroll_viewer.move(0, -1, roster_database_),
            stat_scroll_viewer));
        bool stats_to_bottom = true;
        for (int row = 0; row < 18; ++row)
            stats_to_bottom &= stat_scroll_viewer.move(0, 1, roster_database_);
        stat_scroll.events.push_back(capture("down_18", stats_to_bottom,
                                              stat_scroll_viewer));
        stat_scroll.events.push_back(capture(
            "down_at_bottom", stat_scroll_viewer.move(0, 1, roster_database_),
            stat_scroll_viewer));
        bool stats_to_top = true;
        for (int row = 0; row < 18; ++row)
            stats_to_top &= stat_scroll_viewer.move(0, -1, roster_database_);
        stat_scroll.events.push_back(capture("up_18", stats_to_top,
                                              stat_scroll_viewer));
        stat_scroll.events.push_back(capture(
            "up_at_top_again", stat_scroll_viewer.move(0, -1, roster_database_),
            stat_scroll_viewer));
        finish(stat_scroll);

        Scenario& layers = begin("player_stat_layer_wrap");
        nba97::RosterViewer layer_viewer;
        layer_viewer.open(roster_database_);
        layer_viewer.activate(roster_database_);
        layer_viewer.move(0, 1, roster_database_);
        layers.events.push_back(capture(
            "previous_to_layer_1", layer_viewer.cycleCategory(-1), layer_viewer));
        layers.events.push_back(capture(
            "previous_to_layer_0", layer_viewer.cycleCategory(-1), layer_viewer));
        layers.events.push_back(capture(
            "previous_wraps_to_layer_3", layer_viewer.cycleCategory(-1), layer_viewer));
        layers.events.push_back(capture(
            "next_wraps_to_layer_0", layer_viewer.cycleCategory(1), layer_viewer));
        finish(layers);

        Scenario& displays = begin("roster_display_wrap_memory");
        nba97::RosterViewer display_viewer;
        display_viewer.open(roster_database_);
        displays.events.push_back(capture(
            "previous_wraps_32_to_43", display_viewer.cycleDisplay(-1), display_viewer));
        displays.events.push_back(capture(
            "next_wraps_43_to_32", display_viewer.cycleDisplay(1), display_viewer));
        displays.events.push_back(capture(
            "next_to_33", display_viewer.cycleDisplay(1), display_viewer));
        display_viewer.cycleCategory(1);
        display_viewer.cycleDisplay(1);
        displays.events.push_back(capture("layer_3_has_independent_33", true,
                                          display_viewer));
        display_viewer.cycleCategory(-1);
        displays.events.push_back(capture("layer_2_restores_33", true,
                                          display_viewer));
        finish(displays);

        Scenario& scan = begin("player_team_scan_wrap");
        nba97::RosterViewer scan_viewer;
        scan_viewer.open(roster_database_);
        scan_viewer.activate(roster_database_);
        scan.events.push_back(capture(
            "previous_team_wrap", scan_viewer.scanTeam(-1, roster_database_), scan_viewer));
        scan.events.push_back(capture(
            "next_team_wrap", scan_viewer.scanTeam(1, roster_database_), scan_viewer));
        finish(scan);

        Scenario& help = begin("help_modal_both_modes");
        nba97::RosterViewer help_viewer;
        help_viewer.open(roster_database_);
        help_viewer.toggleHelp();
        help.events.push_back(capture("team_help_open", true, help_viewer));
        help_viewer.dismissHelp();
        help.events.push_back(capture("team_help_closed", true, help_viewer));
        help_viewer.activate(roster_database_);
        help_viewer.toggleHelp();
        help.events.push_back(capture("player_help_open", true, help_viewer));
        help_viewer.dismissHelp();
        help.events.push_back(capture("player_help_closed", true, help_viewer));
        finish(help);

        Scenario& transaction = begin("roster_commit_cancel");
        nba97::RosterViewer transaction_viewer;
        transaction_viewer.open(roster_database_);
        for (int row = 0; row < 6; ++row)
            transaction_viewer.move(0, 1, roster_database_);
        transaction_viewer.move(1, 0, roster_database_);
        transaction_viewer.commit();
        transaction.events.push_back(capture("committed", true, transaction_viewer));
        transaction_viewer.move(1, 0, roster_database_);
        transaction_viewer.move(0, 1, roster_database_);
        transaction.events.push_back(capture("changed_after_commit", true,
                                               transaction_viewer));
        transaction_viewer.cancel();
        transaction.events.push_back(capture("cancel_restores_commit", true,
                                               transaction_viewer));
        finish(transaction);

        std::filesystem::create_directories(
            options_.roster_scenario_report_path.parent_path());
        std::ofstream output(options_.roster_scenario_report_path, std::ios::trunc);
        if (!output) throw std::runtime_error("cannot write roster scenario report");
        output << "{\n  \"schema_version\": 1,\n  \"scope\": \"view_rosters\",\n"
               << "  \"scenarios\": [\n";
        for (std::size_t scenario_index = 0; scenario_index < scenarios.size();
             ++scenario_index) {
            const auto& scenario = scenarios[scenario_index];
            output << "    {\"id\": \"" << scenario.id << "\", \"events\": [\n";
            for (std::size_t event_index = 0; event_index < scenario.events.size();
                 ++event_index) {
                const auto& event = scenario.events[event_index];
                output << "      {\"name\": \"" << event.name
                       << "\", \"changed\": " << (event.changed ? "true" : "false")
                       << ", \"mode\": \"" << event.mode
                       << "\", \"team\": " << event.team
                       << ", \"player\": " << event.player
                       << ", \"first_visible\": " << event.first_visible
                       << ", \"first_stat\": " << event.first_stat
                       << ", \"category\": " << event.category
                       << ", \"display\": " << event.display
                       << ", \"help_visible\": "
                       << (event.help_visible ? "true" : "false") << "}"
                       << (event_index + 1 == scenario.events.size() ? "\n" : ",\n");
            }
            output << "    ], \"function_sequence\": [";
            for (std::size_t index = 0; index < scenario.sequence.size(); ++index) {
                char address[16]{};
                sprintf_s(address, "\"0x%08X\"", scenario.sequence[index]);
                output << (index == 0 ? "" : ", ") << address;
            }
            output << "]}" << (scenario_index + 1 == scenarios.size() ? "\n" : ",\n");
        }
        output << "  ]\n}\n";
        if (!output) throw std::runtime_error("failed writing roster scenario report");
        trace_.log("SEMANTIC", "native View Rosters scenario state trace written");
    }

    void writeSemanticTraceReport() {
        struct SemanticFunction {
            std::uint32_t address;
            const char* name;
        };
        static constexpr std::array<SemanticFunction, 11> functions{{
            {0x8002FE58u, "Frontend_PatchTeamPalette"},
            {0x8005770Cu, "Rosters_ResolveTeamSlots"},
            {0x80057864u, "Rosters_CopySlotTable"},
            {0x80057CE4u, "Frontend_RunRostersMenu"},
            {0x80059034u, "Rosters_DrawTeamSelector"},
            {0x800590B8u, "Rosters_ConstructViewer"},
            {0x800592C4u, "Rosters_RunViewer"},
            {0x80059610u, "Player_ChangeStatLayer"},
            {0x80059928u, "Rosters_CyclePlayer"},
            {0x8005A538u, "Player_RunCard"},
            {0x8005FE14u, "Rosters_ResolvePlayerId"},
        }};
        for (const auto& function : functions) {
            if (nba97_semantic_trace_count(function.address) == 0)
                throw std::runtime_error(std::string("missing native semantic checkpoint: ") +
                                         function.name);
        }
        std::filesystem::create_directories(options_.semantic_report_path.parent_path());
        std::ofstream output(options_.semantic_report_path, std::ios::trunc);
        if (!output) throw std::runtime_error("cannot write semantic trace report");
        output << "{\n  \"schema_version\": 1,\n"
               << "  \"scope\": \"view_rosters\",\n"
               << "  \"dropped_events\": " << nba97_semantic_trace_dropped() << ",\n"
               << "  \"captured_sequence_events\": " << nba97_semantic_trace_size()
               << ",\n  \"functions\": [\n";
        for (std::size_t index = 0; index < functions.size(); ++index) {
            const auto& function = functions[index];
            char address[16]{};
            sprintf_s(address, "0x%08X", function.address);
            output << "    {\"address\": \"" << address << "\", \"name\": \""
                   << function.name << "\", \"native_event_count\": "
                   << nba97_semantic_trace_count(function.address) << "}"
                   << (index + 1 == functions.size() ? "\n" : ",\n");
        }
        std::array<std::uint32_t, NBA97_SEMANTIC_TRACE_SEQUENCE_CAPACITY> sequence{};
        const std::size_t sequence_size =
            nba97_semantic_trace_copy(sequence.data(), sequence.size());
        output << "  ],\n  \"sequence\": [";
        for (std::size_t index = 0; index < sequence_size; ++index) {
            char address[16]{};
            sprintf_s(address, "\"0x%08X\"", sequence[index]);
            output << (index == 0 ? "" : ", ") << address;
        }
        output << "]\n}\n";
        if (!output) throw std::runtime_error("failed writing semantic trace report");
        trace_.log("SEMANTIC", "11/11 mapped View Rosters function checkpoints observed; original trace comparison remains unclaimed");
    }

    void registerWindowClass() {
        WNDCLASSW window_class{};
        window_class.lpfnWndProc = &BootApplication::windowProcedure;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        window_class.lpszClassName = windowClassName();
        if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("RegisterClassW failed");
    }

    void createMainWindow() {
        RECT bounds{0, 0, 1024, 480};
        AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE);
        window_ = CreateWindowExW(0, windowClassName(),
            L"NBA Live 97 - native C++ decompilation", WS_OVERLAPPEDWINDOW,
            80, 80, bounds.right - bounds.left, bounds.bottom - bounds.top,
            nullptr, nullptr, GetModuleHandleW(nullptr), this);
        if (!window_) throw std::runtime_error("CreateWindowExW failed");
    }

    static constexpr const wchar_t* windowClassName() noexcept {
        return L"NBA97DecompWindow";
    }

    static LRESULT CALLBACK windowProcedure(HWND window, UINT message,
                                             WPARAM wparam, LPARAM lparam) {
        auto* app = reinterpret_cast<BootApplication*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            app = static_cast<BootApplication*>(create->lpCreateParams);
            app->window_ = window;
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }
        return app ? app->handleMessage(message, wparam, lparam) :
                     DefWindowProcW(window, message, wparam, lparam);
    }

    LRESULT handleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
        if(native_record_ && (message==WM_KEYDOWN || message==WM_KEYUP ||
           message==WM_MOUSEMOVE || message==WM_LBUTTONDOWN || message==WM_LBUTTONUP || message==WM_KILLFOCUS)) {
            try { native_record_->input(nativeRecordTime(),message,wparam,lparam); }
            catch(const std::exception& e) { native_record_->invalidate(e.what());trace_.log("RECORD-ERROR",e.what());stopNativeRecording(); }
        }
        const auto raw_key=wparam;
        if (message == WM_KEYDOWN || message == WM_KEYUP)
            wparam = nba97::normalizeWin32Shift(static_cast<std::uint32_t>(wparam),
                                               static_cast<std::uintptr_t>(lparam));
        if(message==WM_KEYDOWN || message==WM_KEYUP) updateFrontendPadKey(wparam,message==WM_KEYDOWN);
        if(message==WM_KEYDOWN && frontend_page_==nba97::FrontendPage::CreatePlayers &&
           (static_cast<std::uintptr_t>(lparam)&(1u<<30))==0) {
            char input[160]{};
            sprintf_s(input,"vk=0x%02X scan=0x%02X normalized=0x%02X name-token=0x%03X editor=%u name=%u group=%u",
                unsigned(raw_key),unsigned((static_cast<std::uintptr_t>(lparam)>>16)&0xff),
                unsigned(wparam),unsigned(nba97::createPlayerNameKeyMask(unsigned(wparam))),
                unsigned(create_player_editor_active_),unsigned(create_player_name_editor_.active),
                unsigned(create_player_editor_.rating_group_active));
            trace_.log("CREATE-KEY",input);
        }
        switch (message) {
        case WM_KEYDOWN:
            if(wparam==VK_F9 && !options_.native_record_dir.empty()) {
                if((static_cast<std::uintptr_t>(lparam)&(1u<<30))==0) toggleNativeRecording();
                return 0;
            }
            if(cool_fact_flash_.remaining) return 0;
            // Ignore Windows autorepeat. Recovered screens that repeat input
            // drive it from their own presentation/poll cadence instead.
            if ((static_cast<std::uintptr_t>(lparam) & (1u << 30)) != 0) return 0;
            if(!trade_choice_address_ && reorder_child_.state==0x23 && reorder_help_.phase==NBA97_HELP_CLOSED &&
               compareKeyMask(wparam) && sampleCompareInput()!=compareKeyMask(wparam)) {
                // Preserve the whole sampled mask. A chord containing a
                // generic callback bit calls59F20 (usually a silent no-op),
                // rather than giving the newest key-down priority.
                if(!compare_refresh_.remaining && !compare_repeat_.post_frames) {
                    const auto held=sampleCompareInput();
                    if(nba97_compare_callback_mask(held)) handleCompareInput(held);
                    else { nba97_compare_repeat_idle(&compare_repeat_);compare_repeat_.post_frames=1; }
                    compare_refresh_painted_=false;compare_refresh_tick_=menu_elapsed_ms_/17;
                }
                return 0;
            }
            if(bottom_select_pending_) return 0; // includes Escape during confirmed Reset flash
            if(reset_prompt_.modal.phase!=NBA97_HELP_CLOSED || reset_notice_) {
                handleResetKey(wparam); return 0;
            }
            if (flow_.screen() == nba97::BootScreen::MainMenu &&
                frontend_page_ == nba97::FrontendPage::ViewRosters &&
                (wparam == VK_UP || wparam == VK_DOWN)) {
                held_roster_direction_ = wparam == VK_UP ? -1 : 1;
                held_roster_counter_ = 0;
                held_roster_ticks_since_repeat_ = 0;
            }
            if (wparam == VK_ESCAPE && flow_.screen() == nba97::BootScreen::MainMenu &&
                frontend_page_ != nba97::FrontendPage::GameSetup &&
                frontend_page_ != nba97::FrontendPage::ProfileSetup &&
                frontend_page_ != nba97::FrontendPage::ViewRosters &&
                frontend_page_ != nba97::FrontendPage::ReorderRosters &&
                frontend_page_ != nba97::FrontendPage::CreatePlayers &&
                frontend_page_ != nba97::FrontendPage::TeamSelect &&
                frontend_page_ != nba97::FrontendPage::UserSetup &&
                !isRosterEditor())
                beginFrontendTransition(nba97::FrontendPage::GameSetup, "back input");
            else if (wparam == VK_ESCAPE && flow_.screen() == nba97::BootScreen::MainMenu &&
                     frontend_page_ == nba97::FrontendPage::ProfileSetup)
                handleMenuKey(wparam);
            else if (wparam == VK_ESCAPE && flow_.screen() == nba97::BootScreen::MainMenu &&
                     (frontend_page_ == nba97::FrontendPage::ViewRosters ||
                      frontend_page_ == nba97::FrontendPage::TeamSelect ||
                      frontend_page_ == nba97::FrontendPage::UserSetup ||
                      frontend_page_ == nba97::FrontendPage::CreatePlayers ||
                      frontend_page_ == nba97::FrontendPage::ReorderRosters || isRosterEditor()))
                handleMenuKey(wparam);
            else if (wparam == VK_ESCAPE) DestroyWindow(window_);
            else if (wparam == VK_SPACE && intro_player_.isPlaying())
                finishIntro("skipped by player input", 0);
            else if (flow_.screen() == nba97::BootScreen::TitleScreen &&
                     (wparam == VK_SPACE || wparam == VK_RETURN))
                enterMainMenu("Start pressed");
            else if (flow_.screen() == nba97::BootScreen::MainMenu)
                handleMenuKey(wparam);
            else if (wparam == VK_SPACE)
                flow_.requestAdvance(options_.transition_ms);
            return 0;
        case WM_KEYUP:
            if ((wparam == VK_UP && held_roster_direction_ < 0) ||
                (wparam == VK_DOWN && held_roster_direction_ > 0)) {
                held_roster_direction_ = 0;
                held_roster_counter_ = 0;
                held_roster_ticks_since_repeat_ = 0;
            }
            return 0;
        case WM_KILLFOCUS:
            user_setup_.releaseKeys();releaseFrontendPadKeys();
            return 0;
        case WM_CHAR:
            if (flow_.screen() == nba97::BootScreen::MainMenu &&
                frontend_page_ == nba97::FrontendPage::ProfileSetup &&
                profile_menu_.editing() && wparam >= 32 && wparam < 127 &&
                profile_menu_.append(static_cast<char>(wparam))) {
                rebuildMenuFrame();
                InvalidateRect(window_, nullptr, FALSE);
            }
            return 0;
        case WM_MOUSEMOVE:
            if (flow_.screen() == nba97::BootScreen::MainMenu)
                handleMenuHover(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            return 0;
        case WM_LBUTTONDOWN:
            if(setup_start_pending_) return 0;
            if(frontend_page_==nba97::FrontendPage::TeamSelect ||
               frontend_page_==nba97::FrontendPage::UserSetup) return 0;
            if(cool_fact_flash_.remaining) return 0;
            if(player_notice_.phase!=NBA97_HELP_CLOSED) {
                playerNoticeEvent(nba97_help_input(&player_notice_,0x800));
                return 0;
            }
            if (flow_.screen() == nba97::BootScreen::MainMenu) {
                if (frontend_page_ == nba97::FrontendPage::GameSetup)
                    activateMenuSelection();
                else if (frontend_page_ == nba97::FrontendPage::Rules ||
                         frontend_page_ == nba97::FrontendPage::Options)
                    adjustSetting(GET_X_LPARAM(lparam) < 512 ? -1 : 1);
                else if (frontend_page_ == nba97::FrontendPage::ViewRosters) {
                    int psx_x = 0, psx_y = 0;
                    if (clientToPsx(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam),
                                    psx_x, psx_y) &&
                        psx_x >= 225 && psx_x < 310 && psx_y >= 210) {
                        roster_viewer_.toggleHelp();
                        trace_.log("HELP", std::string("internal 0x20 state=") +
                            (roster_viewer_.mode() == nba97::RosterViewMode::TeamRoster ?
                             "0x10 descriptor=0x800B146C" :
                             "0x24 descriptor=0x800B22F0"));
                        rebuildMenuFrame();
                        InvalidateRect(window_, nullptr, FALSE);
                    } else if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard &&
                               psx_x >= 330 && psx_x < 450 &&
                               psx_y >= 200 && psx_y < 230) {
                        playSelectedCoolFact();
                        rebuildMenuFrame();
                        InvalidateRect(window_, nullptr, FALSE);
                    } else if (roster_viewer_.mode() == nba97::RosterViewMode::TeamRoster &&
                               roster_viewer_.selectedPlayer(roster_database_)) {
                        roster_viewer_.activate(roster_database_);
                        cool_fact_selection_={};cool_fact_selection_.selected=-1;
                        loadSelectedPlayerCardAssets();
                        trace_.log("ROSTER-VIEW", "mouse/internal 0x10 -> result=2 -> nested state 0x24");
                        rebuildMenuFrame();
                        InvalidateRect(window_, nullptr, FALSE);
                    }
                }
                else if (frontend_page_ == nba97::FrontendPage::Rosters)
                    activateRecoveredBottomSelection();
                else
                    trace_.log("MENU-BLOCK", std::string(bottom_menu_.selectedLabel()) +
                                             " child flow not yet decompiled");
                return 0;
            }
            break;
        case WM_TIMER:
            update();
            return 0;
        case WM_PAINT:
            paint();
            return 0;
        case WM_ERASEBKGND:
            // Every non-video frame is presented as one complete back-buffer
            // blit. Suppress the default erase so Windows cannot expose a
            // black intermediate frame between roster animation ticks.
            return 1;
        case WM_SIZE:
        case WM_DISPLAYCHANGE:
            intro_player_.resize();
            return 0;
        case nba97::IntroPlayer::kGraphEventMessage:
            handleMovieEvent();
            return 0;
        case WM_DESTROY:
            stopNativeRecording();
            KillTimer(window_, kFrameTimer);
            intro_player_.stop();
            frontend_music_.stop();
            cursor_audio_.stop();
            cool_fact_audio_.stop();
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window_, message, wparam, lparam);
        }
        return DefWindowProcW(window_, message, wparam, lparam);
    }

    void update() {
        const DWORD now = GetTickCount();
        if (flow_.screen() == nba97::BootScreen::MainMenu) {
            const auto music_error=frontend_music_.error();
            if(!music_error.empty()) {
                trace_.log("MUSIC-ERROR",music_error+"; stopped only the music stream");
                frontend_music_.stop();
            }
            const auto underruns=frontend_music_.underruns();
            if(underruns>music_underruns_logged_) {
                trace_.log("MUSIC-UNDERRUN","queued PCM starved count="+std::to_string(underruns)+"; timing reference requires review");
                music_underruns_logged_=underruns;
            }
            menu_elapsed_ms_ += now - previous_tick_;
            if (create_player_editor_active_)
                nba97_create_editor_tick(&create_player_editor_);
            updatePlayerPhoto();
            if(cool_fact_flash_.remaining) {
                const auto tick=menu_elapsed_ms_/17;
                if(cool_fact_flash_painted_ && cool_fact_flash_tick_<tick) {
                    advanceCoolFactFlash();
                    cool_fact_flash_painted_=false;
                }
                cool_fact_flash_tick_=tick; // Never skip unseen phases after a stall.
                reorder_tick_=tick; // No queued editor input after the callback.
            } else if(reorder_child_.state==0x23 && (compare_refresh_.remaining || compare_repeat_.post_frames)) {
                const auto tick=menu_elapsed_ms_/17;
                if(compare_refresh_painted_ && compare_refresh_tick_<tick) {
                    if(compare_refresh_.remaining) advanceCompareRefresh();
                    else advanceComparePostCycle(sampleCompareInput());
                    compare_refresh_painted_=false;
                }
                compare_refresh_tick_=tick; // Never skip an unseen pending frame after a stall.
                reorder_tick_=tick; // Suspended selector: no accumulated input/frame catch-up.
            } else if(player_notice_.phase!=NBA97_HELP_CLOSED) {
                updatePlayerNotice();
                reorder_tick_=menu_elapsed_ms_/17; // No delayed editor input catch-up.
            } else {
                updateRosterHeldInput();
                updateReorder();
                updateTrade();
            }
            updateReset();
            updateCreatePlayerDelete();
            updateCreatePlayerHelp();
            updateSetupStart();
            updateTeamSelect();
            updateUserSetup();
            if (bottom_select_pending_ &&
                menu_elapsed_ms_ - bottom_select_flash_start_ms_ >= kBottomSelectFlashMs) {
                bottom_select_pending_ = false;
                trace_.log("ROSTER-CARD-SELECT", "FUN_8003F240 flash complete after 12 vblanks; "
                    "dispatch=" + std::string(bottom_menu_.selectedLabel()));
                completeRecoveredBottomSelection();
            }
            updateFrontendTitle();
            rebuildMenuFrame();
            if (frontend_transition_active_) {
                // Entry may load assets inside this update. Do not subtract a
                // newer transition timestamp from the stale frame-start tick.
                const auto elapsed = GetTickCount() - frontend_transition_tick_;
                transition_frame_ = blendFrames(transition_source_, menu_frame_, elapsed,
                                                kFrontendTransitionMs);
                if (elapsed >= kFrontendTransitionMs) {
                    frontend_transition_active_ = false;
                    trace_.log("TRANSITION-END", frontendPageName(frontend_page_) +
                                                 std::string(" visible"));
                }
            }
            InvalidateRect(window_, nullptr, FALSE);
        }
        if (flow_.update(now - previous_tick_, options_.transition_ms)) {
            if (flow_.screen() == nba97::BootScreen::LegalScreen) {
                trace_.log("TRANSITION", "FEONLY 0x80036684 -> original ZLEGAL.PSH");
            } else if (flow_.screen() == nba97::BootScreen::IntroVideo) {
                startIntro();
            }
            InvalidateRect(window_, nullptr, FALSE);
        }
        previous_tick_ = now;
    }

    void updateRosterHeldInput() {
        if (frontend_page_ != nba97::FrontendPage::ViewRosters ||
            frontend_transition_active_ || held_roster_direction_ == 0)
            return;
        held_roster_counter_ = (std::min)(48, held_roster_counter_ + 2);
        const int interval = held_roster_counter_ <= 15 ? 7 :
            held_roster_counter_ <= 27 ? 5 :
            held_roster_counter_ <= 37 ? 3 : 1;
        if (++held_roster_ticks_since_repeat_ < interval) return;
        held_roster_ticks_since_repeat_ = 0;
        handleRosterViewKey(held_roster_direction_ < 0 ? VK_UP : VK_DOWN);
    }

    [[nodiscard]] const Frame& currentFrame() const noexcept {
        if (flow_.screen() == nba97::BootScreen::LoadScreen) return load_frame_;
        if (flow_.screen() == nba97::BootScreen::LegalScreen) return legal_frame_;
        if (flow_.screen() == nba97::BootScreen::MainMenu)
            return frontend_transition_active_ ? transition_frame_ : menu_frame_;
        // IntroVideo is handled separately by paint(). The title frame is only
        // reachable after BootFlow::completeIntro() changes this to TitleScreen.
        return title_frame_;
    }

    void paint() {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window_, &paint);
        if (flow_.screen() == nba97::BootScreen::IntroVideo) {
            if (intro_player_.isPlaying()) {
                intro_player_.repaint(dc);
            } else {
                RECT client{};
                GetClientRect(window_, &client);
                FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            }
        } else {
            RECT client{};
            GetClientRect(window_, &client);
            const int client_width = client.right - client.left;
            const int client_height = client.bottom - client.top;
            if (client_width <= 0 || client_height <= 0) {
                EndPaint(window_, &paint);
                return;
            }
            const RECT presentation = psxPresentationRect(client);
            const Frame& frame = currentFrame();
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = frame.width;
            info.bmiHeader.biHeight = -frame.height;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            HDC back_dc = CreateCompatibleDC(dc);
            HBITMAP back_bitmap = back_dc
                ? CreateCompatibleBitmap(dc, client_width, client_height) : nullptr;
            if (back_dc && back_bitmap) {
                HGDIOBJ previous_bitmap = SelectObject(back_dc, back_bitmap);
                FillRect(back_dc, &client,
                         static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
                SetStretchBltMode(back_dc, COLORONCOLOR);
                StretchDIBits(back_dc, presentation.left, presentation.top,
                              presentation.right - presentation.left,
                              presentation.bottom - presentation.top, 0, 0,
                              frame.width, frame.height, frame.bgra.data(), &info,
                              DIB_RGB_COLORS, SRCCOPY);
                BitBlt(dc, 0, 0, client_width, client_height,
                       back_dc, 0, 0, SRCCOPY);
                SelectObject(back_dc, previous_bitmap);
            } else {
                // Allocation failure should not blank the frame. Paint directly
                // without first clearing underneath the presentation rectangle.
                SetStretchBltMode(dc, COLORONCOLOR);
                StretchDIBits(dc, presentation.left, presentation.top,
                              presentation.right - presentation.left,
                              presentation.bottom - presentation.top, 0, 0,
                              frame.width, frame.height, frame.bgra.data(), &info,
                              DIB_RGB_COLORS, SRCCOPY);
            }
            if (back_bitmap) DeleteObject(back_bitmap);
            if (back_dc) DeleteDC(back_dc);
        }
        EndPaint(window_, &paint);
        if(flow_.screen()==nba97::BootScreen::MainMenu && frontend_title_.active()) frontend_title_painted_=true;
        if(flow_.screen()==nba97::BootScreen::MainMenu && frontend_page_==nba97::FrontendPage::GameSetup) setup_start_painted_=true;
        if(native_record_) {
            try {
                if(flow_.screen()!=nba97::BootScreen::MainMenu) {
                    trace_.log("RECORD-STOP","left frontend; this recorder does not capture the video renderer");
                    stopNativeRecording();
                } else {
                    const auto& frame=currentFrame();
                    const auto state=nativeRecordState();
                    if(frame.width!=512 || frame.height!=240) throw std::runtime_error("recorded presentation is not 512x240");
                    native_record_->submit(frame.bgra,nativeRecordTime(),state);
                    if(!native_record_->accepting()) stopNativeRecording();
                }
            } catch(const std::exception& e) { native_record_->invalidate(e.what());trace_.log("RECORD-ERROR",e.what());stopNativeRecording(); }
        }
        if(cool_fact_flash_.remaining) cool_fact_flash_painted_=true;
        if(reorder_child_.state==0x23 && (compare_refresh_.remaining || compare_repeat_.post_frames)) compare_refresh_painted_=true;
    }

    std::uint64_t nativeRecordTime() const {
        const auto now=nba97::ProcessAudioCapture::qpc100ns();
        if(now<native_record_origin_) throw std::runtime_error("native capture QPC moved backwards");
        return (now-native_record_origin_)*100;
    }
    nba97::NativeFrameCapture::State nativeRecordState() const {
        if(isRosterEditor()) {
            const auto& s=trade_screen_;
            // The v1 timeline has one team field: Trade records the LEFT team.
            // Right team and both counts remain in TRADE CLI events. Child fields
            // here describe the suspended parent, not the child's browsed pair.
            return {{static_cast<int>(flow_.screen()),static_cast<int>(frontend_page_),menu_elapsed_ms_,
                s.team[0],s.phase,reorder_child_.state,reorder_help_.phase,
                s.cursor[0],s.cursor[1],s.top[0],s.top[1],s.selected[0],s.selected[1],
                cool_fact_selection_.selected,cool_fact_flash_.remaining,frontend_transition_active_}};
        }
        const auto& s=reorder_screen_.selection;
        return {{static_cast<int>(flow_.screen()),static_cast<int>(frontend_page_),menu_elapsed_ms_,
            reorder_screen_.team,s.phase,reorder_child_.state,reorder_help_.phase,
            s.cursor[0],s.cursor[1],s.top[0],s.top[1],s.selected_ids[0],s.selected_ids[1],
            cool_fact_selection_.selected,cool_fact_flash_.remaining,frontend_transition_active_}};
    }
    std::array<std::uint8_t,32> nativeRecordSlotsHash() const {
        // Hash the displayed editor's effective draft, never object padding.
        nba97::Sha256 hash;
        for(int i=0;i<NBA97_ROSTER_TABLE_SLOTS;++i) {
            const auto local=i-reorder_screen_.team*15;
            const auto id=isRosterEditor() ? trade_screen_.working[i] :
                local>=0 && local<15 ? reorder_screen_.selection.slots[local]:reorder_screen_.working[i];
            const std::uint8_t bytes[]{static_cast<std::uint8_t>(id),static_cast<std::uint8_t>(id>>8)};
            hash.update(bytes,2);
        }
        return hash.digest();
    }
    void recordNativeHelp(unsigned operation,std::uint16_t raw,Nba97HelpEvent result,const Nba97HelpModal& before) {
        if(!native_record_)return;
        try {
            const auto pack=[](const Nba97HelpModal& m)->nba97::NativeFrameCapture::HelpModal {
                return {{m.phase,m.rect.x,m.rect.y,m.rect.width,m.rect.height,
                    m.target.x,m.target.y,m.target.width,m.target.height,m.held}};
            };
            const auto previous=pack(before),current=pack(reorder_help_);
            if(operation && previous==current && result==NBA97_HELP_NO_EVENT)return;
            native_record_->helpEvent(nativeRecordTime(),operation,raw,result,reorder_notice_.has_value(),
                previous,current,nativeRecordState(),nativeRecordSlotsHash());
            if(previous[0]!=current[0])trace_.log("RECORD-HELP","phase="+std::to_string(previous[0])+"->"+std::to_string(current[0])+
                "; operation="+std::to_string(operation)+"; raw="+std::to_string(raw)+"; real QPC event; no added frame");
            if(!native_record_->accepting())stopNativeRecording();
        } catch(const std::exception& e) {
            if(native_record_){native_record_->invalidate(e.what());trace_.log("RECORD-ERROR",e.what());stopNativeRecording();}
        }
    }
    void toggleNativeRecording() {
        if(native_record_) {stopNativeRecording();return;}
        if(flow_.screen()!=nba97::BootScreen::MainMenu) {
            trace_.log("RECORD-WAIT","navigate to the frontend first, then press F9");return;
        }
        try {
            const auto private_root=std::filesystem::absolute(options_.asset_root).parent_path();
            if(private_root.filename()!=".local") throw std::runtime_error("recording requires asset packs under .local");
            native_record_origin_=nba97::ProcessAudioCapture::qpc100ns();
            native_record_=std::make_unique<nba97::NativeFrameCapture>(private_root,options_.native_record_dir,native_record_origin_,options_.native_record_limit);
            native_record_->audioResult(options_.native_record_audio,false,false);
            if(options_.native_record_audio) {
                native_audio_record_=std::make_unique<nba97::ProcessAudioCapture>(
                    native_record_->directory()/"audio",native_record_origin_);
                trace_.log("RECORD-AUDIO","started Windows process-tree mix; PCM16 stereo48000Hz; packet QPC/flags retained; OS conversion=yes; no mic/system fallback");
            }
            trace_.log("RECORD-START",native_record_->directory().string()+
                "; raw 512x240 RGB; every native presentation; shared QPC clock; eight queued frames; reference-ready=no");
            recordNativeHelp(0,0,NBA97_HELP_NO_EVENT,reorder_help_);
            InvalidateRect(window_,nullptr,FALSE);
        } catch(const std::exception& e) {
            trace_.log("RECORD-ERROR",e.what());
            if(native_record_) {native_record_->invalidate("requested capture startup failed");stopNativeRecording();}
        }
    }
    void stopNativeRecording() {
        if(!native_record_) return;
        try {
            if(native_audio_record_) {
                native_audio_record_->finish();
                native_record_->audioResult(true,true,native_audio_record_->complete());
                trace_.log("RECORD-AUDIO",native_audio_record_->complete()?"process mix written; original parity unverified":native_audio_record_->error());
            }
            native_record_->finish();
            trace_.log("RECORD-STOP","submitted="+std::to_string(native_record_->submitted())+
                "; video="+(native_record_->error().empty()?std::string("written"):native_record_->error())+
                "; audio="+(native_audio_record_?std::string("process-mix; inspect separate audio status"):std::string("absent"))+
                "; reference-ready=no");
        } catch(const std::exception& e) {trace_.log("RECORD-ERROR",e.what());}
        native_record_.reset();
        native_audio_record_.reset();
    }

    void startIntro() {
        if (!std::filesystem::exists(intro_movie_)) {
            trace_.log("MOVIE-ERROR", "missing local intro pack: " + intro_movie_.string());
            finishIntro("missing local movie", -1);
            return;
        }
        try {
            intro_player_.start(window_, intro_movie_);
            trace_.log("MOVIE", "0x8002DFB4(0) -> Z0ZTITLE.XA; in-process C++ graph, same HWND");
            for (const std::string& filter : intro_player_.filterNames())
                trace_.log("DECODER", filter);
            trace_.log("MOVIE-PLAY", "1168 frames / 77.9 s with synchronized XA stereo");
            trace_.log("MOVIE-AUDIO", intro_player_.audioDescription() +
                "; DirectShow duplicate muted; prior WinMM state isolated/restored");
            trace_.log("DISPLAY", "intro surface active; title frame remains gated until movie end/skip");
        } catch (const std::exception& error) {
            trace_.log("MOVIE-ERROR", error.what());
            finishIntro("decoder graph failed", -1);
        }
    }

    void handleMovieEvent() {
        using Result = nba97::IntroPlayer::EventResult;
        const Result result = intro_player_.handleGraphEvents();
        if (result == Result::Completed)
            finishIntro("completed", intro_player_.lastEventCode());
        else if (result == Result::UserAbort)
            finishIntro("decoder reported user abort", intro_player_.lastEventCode());
        else if (result == Result::Error)
            finishIntro("decoder reported error", intro_player_.lastEventCode());
    }

    void finishIntro(const char* reason, long event_code) {
        intro_player_.stop();
        if (!flow_.completeIntro()) return;
        trace_.log("MOVIE-END", std::string(reason) + "; graph-event=" +
                                std::to_string(event_code));
        trace_.log("TRANSITION", "0x80035984 -> title 0x8002EEF4 -> ZCPYRT97.PSH");
        if(window_) InvalidateRect(window_, nullptr, FALSE);
    }

    void enterMainMenu(const char* reason) {
        if (!flow_.enterMainMenu()) return;
        menu_.reset();
        frontend_page_ = nba97::FrontendPage::GameSetup;
        menu_elapsed_ms_ = 0;
        POINT cursor{};
        if (GetCursorPos(&cursor) && ScreenToClient(window_, &cursor)) {
            last_mouse_x_ = cursor.x;
            last_mouse_y_ = cursor.y;
        }
        menu_mouse_armed_ = false;
        rebuildMenuFrame();
        trace_.log("TRANSITION", std::string(reason) +
                                 "; title loop -> Game Setup frontend state");
        trace_.log("MENU", "quarter=3 min, mode=exhibition, style=arcade, level=rookie");
        trace_.log("MENU-FOCUS", "game option quarter selected; arrows/mouse hover enabled");
        try {
            const auto recovered_volume =
                nba97_frontend_music_volume(settings_.option(1));
            frontend_music_.start(options_.asset_root / "menu" / "ZTMENU1.CNK", recovered_volume);
            music_underruns_logged_=0;
            const auto& audio = frontend_music_.info();
            trace_.log("MUSIC-DECODER", frontend_music_.decoderName());
            trace_.log("MUSIC-STREAM", "ZTMENU1.CNK blocks=" + std::to_string(audio.data_blocks) +
                " rate=" + std::to_string(audio.sample_rate) + "Hz channels=" +
                std::to_string(audio.channels) + " samples=" + std::to_string(audio.sample_count));
            trace_.log("MUSIC-PLAY", "FUN_8002F258 recovered volume=" +
                std::to_string(recovered_volume) + "/127; music-only PCM gain; four 1024-frame buffers; no Windows session-volume writes");
        } catch (const std::exception& error) {
            trace_.log("MUSIC-ERROR", error.what());
        }
        InvalidateRect(window_, nullptr, FALSE);
    }

    bool openTeamSelect() {
        try {
            if(!roster_load_error_.empty()) throw std::runtime_error("accepted roster unavailable: "+roster_load_error_);
            const auto root=options_.asset_root/"team_select";
            if(!team_select_assets_) {
                auto assets=std::make_unique<nba97::TeamSelectAssets>(root);
                nba97::MenuSpritePack sprites;
                auto load=[&](const std::string& tag) {
                    if(!sprites.count(tag)) sprites.emplace(tag,load_png_image(root/(tag+".png")));
                };
                for(unsigned i=4;i<18;++i) load(assets->layout()[i].tag);
                for(unsigned id=0;id<31;++id) load(assets->team(id).logo);
                team_select_rng_=assets->initialRng();
                team_select_sprites_=std::move(sprites);team_select_assets_=std::move(assets);
            }
            team_select_ranks_=team_select_assets_->ranks(roster_database_);
            nba97_team_select_open(&team_select_,team_select_.team[0],team_select_.team[1],
                team_select_.remembered_regular[0],team_select_.remembered_regular[1]);
            nba97_team_select_restore_focus(&team_select_,team_select_focus_);
            for(auto& tint:team_select_tints_) {tint={};std::fill_n(tint.rgb,3,uint8_t(128));}
            nba97_reorder_tint_pulse(&team_select_tints_[team_select_focus_]);
            nba97_frontend_palette_begin(&team_select_palette_,team_select_assets_->backgrounds().bank(),33,
                team_select_.team[1],team_select_.team[0]);
            if(frontend_page_==nba97::FrontendPage::UserSetup) {
                team_select_poll_.pad.prior_mask=user_setup_.priorMask();
                team_select_poll_.pad.prior_controller=user_setup_.priorController();
            }
            nba97_team_poll_open(&team_select_poll_); // Shared history survives page opens.
            team_select_help_={};team_select_exit_wait_=false;
            team_select_random_={};team_select_tick_=uint64_t(menu_elapsed_ms_)*30/1001;
            trace_.log("TEAM-ENTRY","owner8004FCD8 state3 packZSET1/LOGO titleba08; home="+
                std::to_string(team_select_.team[0])+" away="+std::to_string(team_select_.team[1])+
                " focus="+std::to_string(team_select_focus_)+" roster-generation="+
                std::to_string(roster_store_ ? roster_store_->accepted().generation:0)+
                " modified="+std::to_string(roster_database_.differsFromOriginal())+
                "; current roster ranks8005DB34; settings/catalogue retained; native clock/seed history unverified");
            return true;
        } catch(const std::exception& error) {
            trace_.log("TEAM-ENTRY-REFUSED",error.what());return false;
        }
    }

    void teamSelectPalette() {
        for(unsigned side=0;side<2;++side)
            nba97_frontend_palette_request(&team_select_palette_,side,team_select_.team[side^1],33);
    }

    void releaseFrontendPadKeys() noexcept {
        team_select_keys_={};team_select_held_=0;user_setup_.key(0,0xffff,false);
    }
    void updateFrontendPadKey(WPARAM key,bool down=true) {
        // WM repeat only updates held state. Alias releases cannot clear another
        // held key for the same normalized button (for example C and Space).
        if(key<team_select_keys_.size()) team_select_keys_[key]=down;
        team_select_held_=0;
        for(unsigned k=0;k<team_select_keys_.size();++k)
            if(team_select_keys_[k]) team_select_held_|=nba97::userSetupKeyMask(k);
        if(frontend_page_==nba97::FrontendPage::UserSetup) {
            user_setup_.key(0,0xffff,false);user_setup_.key(0,team_select_held_,true);
        }
    }
    void updateSetupStart() {
        if(!setup_start_pending_ || frontend_page_!=nba97::FrontendPage::GameSetup || frontend_transition_active_) return;
        const auto now=uint64_t(menu_elapsed_ms_)*30/1001;
        if(setup_start_tick_>=now || (window_ && !setup_start_painted_)) return;
        setup_start_tick_=now;setup_start_painted_=false;
        const uint16_t pads[8]={team_select_held_};Nba97TeamSample sample{};
        if(nba97_team_poll_prepare(&team_select_poll_,0) &&
           nba97_team_poll_presented(&team_select_poll_,pads,&sample)==NBA97_TEAM_POLL_EXITED) {
            setup_start_pending_=false;
            beginFrontendTransition(nba97::FrontendPage::TeamSelect,
                "Start80; state0 exit3B194 changed input and cleanup -> state3/8004FCD8");
        }
    }
    void dispatchTeamSelect(const Nba97TeamSample& sample) {
            const auto token=sample.token;
            const auto before=team_select_;
            const auto old_focus=unsigned(before.side)*6+before.criterion;
            const auto event=nba97_team_select_input(&team_select_,&team_select_ranks_,token);
            const auto wait=nba97_team_poll_caller_wait(token,sample.delay);
            team_select_focus_=team_select_.side*6+team_select_.criterion;
            if(event==NBA97_SELECT_CONTINUE || event==NBA97_SELECT_RETURN) {
                team_select_exit_wait_=true;
                nba97_team_poll_exit(&team_select_poll_);
            }
            if(event==NBA97_SELECT_CONTINUE) {
                trace_.log("TEAM-HANDOFF","state3 result1 -> state5 owner80037010 after3B194 input-change barrier; home="+
                    std::to_string(team_select_.team[0])+" away="+std::to_string(team_select_.team[1])+
                    "; teams/focus retained; next screen is User Setup");
            }
            if(team_select_.sound) playBottomMenuSound(team_select_.sound,"team-selector");
            if(event==NBA97_SELECT_HELP) {
                nba97_help_open(&team_select_help_,team_select_assets_->help().descriptor(3,0).rect,token);
                nba97_help_tick(&team_select_help_,team_select_held_); // First growth precedes modal submission.
                playBottomMenuSound(7,"team-help-open");
            }
            if(event==NBA97_SELECT_RANDOM) {
                nba97_team_random_begin(&team_select_random_,&team_select_,team_select_rng_.data());
                trace_.log("TEAM-RANDOM","owner8004F934 candidate1; 78 presentations + caller5 + next-poll1; held input blocked");
            }
            if(event!=NBA97_SELECT_HELP && event!=NBA97_SELECT_RANDOM && !team_select_exit_wait_)
                nba97_team_poll_finish_callback(&team_select_poll_,wait);
            if(old_focus!=team_select_focus_) {
                nba97_reorder_tint_unpulse(&team_select_tints_[old_focus]);
                nba97_reorder_tint_pulse(&team_select_tints_[team_select_focus_]);
            }
            teamSelectPalette();
            if(event!=NBA97_SELECT_NONE || !team_select_poll_.pad.repeat_counter)
                trace_.log("TEAM-INPUT","owner8003AE4C/8003D930/8004F9D8 token="+std::to_string(token)+
                " controller="+std::to_string(sample.controller)+" repeat="+std::to_string(team_select_poll_.pad.repeat_counter)+
                " caller-wait="+std::to_string(wait)+
                " event="+std::to_string(event)+" home="+std::to_string(before.team[0])+"->"+
                std::to_string(team_select_.team[0])+" away="+std::to_string(before.team[1])+"->"+
                std::to_string(team_select_.team[1])+" focus="+std::to_string(old_focus)+"->"+
                std::to_string(team_select_focus_)+" sound="+std::to_string(team_select_.sound));
    }

    void updateTeamSelect() {
        if(frontend_page_!=nba97::FrontendPage::TeamSelect || frontend_transition_active_) return;
        const auto now=uint64_t(menu_elapsed_ms_)*30/1001;
        if(team_select_tick_<now && (!window_ || frontend_title_painted_)) {
            const bool help=team_select_help_.phase!=NBA97_HELP_CLOSED;
            const bool random=nba97_team_random_busy(&team_select_random_)!=0;
            // Native composition has no queued text movement. The C settle
            // gate is tested, but original text-list movement remains pending.
            if(!help && !random && !nba97_team_poll_prepare(&team_select_poll_,0)) return;
            team_select_tick_=now; // Stall stretches time; never skip unseen presentations.
            ++team_select_presentations_;
            prepareFrontendTitle();presentFrontendTitle();frontend_title_painted_=false;
            for(auto& tint:team_select_tints_) nba97_reorder_tint_tick(&tint);
            nba97_frontend_palette_tick(&team_select_palette_,team_select_assets_->backgrounds().bank(),33);
            if(help) {
                const auto event=nba97_help_tick(&team_select_help_,team_select_held_);
                if(event==NBA97_HELP_CLOSE_SOUND) {
                    team_select_poll_.pad.prior_mask=team_select_help_.held;
                    playBottomMenuSound(8,"team-help-close");
                }
                if(event==NBA97_HELP_RETURNED) nba97_team_poll_finish_callback(&team_select_poll_,0);
            } else if(random) {
                if(nba97_team_random_tick(&team_select_random_,&team_select_,team_select_rng_.data())) {
                    teamSelectPalette();
                    trace_.log("TEAM-RANDOM","owner8004F934/8007A538 accepted="+
                        std::to_string(12-team_select_random_.remaining)+" team="+
                        std::to_string(team_select_.team[team_select_.side])+
                        "; original runtime seed history pending");
                }
                if(!nba97_team_random_busy(&team_select_random_)) nba97_team_poll_finish_callback(&team_select_poll_,5);
            } else {
                const uint16_t pads[8]={team_select_held_};Nba97TeamSample sample{};
                const auto event=nba97_team_poll_presented(&team_select_poll_,pads,&sample);
                if(event==NBA97_TEAM_POLL_INPUT) dispatchTeamSelect(sample);
                if(event==NBA97_TEAM_POLL_EXITED) {
                    team_select_exit_wait_=false;
                    const auto target=team_select_.result==1 ? nba97::FrontendPage::UserSetup:nba97::FrontendPage::GameSetup;
                    beginFrontendTransition(target,"state3 exit;3B194 changed initiating input then39574(0,1) cleanup; teams/focus retained");
                    if(frontend_page_==nba97::FrontendPage::TeamSelect) {
                        team_select_.result=0;nba97_team_poll_open(&team_select_poll_);
                    }
                    return;
                }
            }
        }
    }

    bool openUserSetup() {
        try {
            if(!team_select_assets_) throw std::runtime_error("User Setup requires selected teams/assets");
            if(!user_setup_assets_) {
                const auto root=options_.asset_root/"user_setup";
                auto assets=std::make_unique<nba97::UserSetupAssets>(root);
                for(const auto* tag:{"hel1","hel2","ba39","cnt3","cnt2","cnt1"})
                    if(!team_select_sprites_.count(tag)) team_select_sprites_.emplace(tag,load_png_image(root/(std::string(tag)+".png")));
                user_setup_assets_=std::move(assets);
            }
            if(!match_session_.initialized()) {
                match_session_.initialize(nba97::loadMatchControlDefaults(options_.asset_root/"match_setup/controls.n97ctl"));
                trace_.log("MATCH-CONTROLS-INIT","28800 cold branch ->61674(1);8 default maps; once per fresh native process; warm overlay paths pending");
            }
            user_setup_.open(user_setup_assets_->initialAssignments(),profile_store_.profiles(),
                static_cast<int32_t>(uint64_t(menu_elapsed_ms_)*120/1000),
                team_select_poll_.pad.prior_mask,team_select_poll_.pad.prior_controller);
            user_setup_.configureEditor(user_setup_assets_->alphabet(),[this](const char* text){return menu_font_.textWidth(text);});
            user_setup_.setControllers(0,1); // Native keyboard is physical port1; no invented second-player keys.
            user_setup_.primeEntryTopology(); // First observation belongs to the first later input pass.
            user_setup_.key(0,team_select_held_,true); // Preserve the changed nonzero exit mask across pages.
            user_setup_tick_=uint64_t(menu_elapsed_ms_)*30/1001;user_setup_refusal_logged_=false;
            trace_.log("USER-ENTRY","owner80037010 state5 titleba39 ZSET1/Pal0; home="+
                std::to_string(team_select_.team[0])+" away="+std::to_string(team_select_.team[1])+
                "; 8 assignments, fixed20-slot exact-name profile editor; keyboard physical0; input clock120Hz; commit flush is native policy");
            return true;
        } catch(const std::exception& e) {trace_.log("USER-ENTRY-REFUSED",e.what());return false;}
    }

    void openUserDialog(nba97::UserSetupDialog kind,unsigned controller,const std::string& name={}) {
        user_dialog_name_=name;
        user_setup_.openDialog(kind,user_setup_assets_->dialogRect(kind),controller,user_setup_assets_->deletePreference());
        // First39574 calls30C0C/30784 before submitting the modal rectangle.
        user_setup_.tickDialog();
        playBottomMenuSound(kind==nba97::UserSetupDialog::Delete ? 12:5,"user-dialog-open");
        trace_.log("USER-DIALOG","owner80040A1C kind="+std::to_string(unsigned(kind))+
            " controller="+std::to_string(controller)+" prior-mask="+std::to_string(user_setup_.dialogState().modal.held));
    }
    void updateUserProfileCount() {
        active_user_profiles_=static_cast<int>(profile_store_.profiles().size());
        menu_.setActiveUserProfiles(active_user_profiles_);
    }
    void captureMatchSnapshot() {
        nba97::MatchRequest request;request.users=user_setup_.state();
        for(unsigned side=0;side<2;++side)request.teams[side]=team_select_.team[side];
        for(unsigned card=0;card<4;++card)request.setup[card]=menu_.setupChoice(card);
        const nba97::MatchSnapshot* snapshot=nullptr;
        try {
            snapshot=&match_session_.capture(request,{roster_database_,settings_,profile_store_.profiles(),
                created_players_,team_select_assets_->ratingAdjustments(),roster_store_ ? roster_store_->accepted().generation:0,
                profile_store_.generation(),created_player_store_.generation()});
        } catch(const std::exception& e) {trace_.log("MATCH-SNAPSHOT-PENDING",e.what());return;}
        // Publication succeeded. A diagnostic allocation failure cannot relabel
        // it as a refused snapshot with an unchanged revision/live map.
        try {
            trace_.log("MATCH-SNAPSHOT","revision="+std::to_string(match_session_.revision())+
                " source61674/63D58/655B0 subset; "+nba97::matchSnapshotReceipt(*snapshot));
        } catch(const std::exception& e) {trace_.log("MATCH-SNAPSHOT-LOG-FAILED",std::string("snapshot retained; ")+e.what());}
    }
    void updateUserSetup() {
        if(frontend_page_!=nba97::FrontendPage::UserSetup || frontend_transition_active_) return;
        const auto now=uint64_t(menu_elapsed_ms_)*30/1001;
        if(user_setup_tick_>=now || (window_ && !frontend_title_painted_)) return;
        user_setup_tick_=now;
        if(user_setup_.state().result==-1) {
            prepareFrontendTitle();presentFrontendTitle();frontend_title_painted_=false;
            nba97_frontend_palette_tick(&team_select_palette_,team_select_assets_->backgrounds().bank(),33);
            if(user_setup_.cancelReady()) beginFrontendTransition(nba97::FrontendPage::TeamSelect,
                "state5 Select100 result-1; dispatcher3FD10/3B194 aggregate input changed; accepted assignments retained");
            return;
        }
        const auto dialog=user_setup_.dialogKind();
        const bool modal=user_setup_.help().phase!=NBA97_HELP_CLOSED || dialog!=nba97::UserSetupDialog::None;
        bool resume_pass=!modal;
        if(dialog!=nba97::UserSetupDialog::None) {
            const auto event=user_setup_.tickDialog();
            if(event&NBA97_RESET_UP)playBottomMenuSound(3,"user-dialog-up");
            if(event&NBA97_RESET_DOWN)playBottomMenuSound(4,"user-dialog-down");
            if(event&NBA97_RESET_CHOSEN)playBottomMenuSound(6,"user-dialog-confirm");
            if(event&32)playBottomMenuSound(8,"user-dialog-close");
            if(event&NBA97_RESET_RETURN) {
                const auto controller=user_setup_.dialogController();
                const bool erase=dialog==nba97::UserSetupDialog::Delete && user_setup_.dialogState().choice==0;
                user_setup_.finishDialog();
                if(erase) {
                    try {
                        if(!user_setup_.deleteProfile(controller,profile_store_))throw std::runtime_error(profile_store_.lastError());
                        updateUserProfileCount();
                        trace_.log("USER-DELETE","original3573C choice0; fixed slot cleared; native profile generation="+std::to_string(profile_store_.generation()));
                    } catch(const std::exception& e) {
                        trace_.log("USER-SAVE-FAILED",std::string(e.what())+"; deletion not accepted");
                        openUserDialog(nba97::UserSetupDialog::SaveFailure,controller);
                    }
                }
                resume_pass=user_setup_.dialogKind()==nba97::UserSetupDialog::None;
            }
        } else if(modal) {
            const auto event=user_setup_.tickHelp();
            if(event==NBA97_HELP_CLOSE_SOUND) playBottomMenuSound(8,"user-help-close");
            resume_pass=event==NBA97_HELP_RETURNED;
        }
        // 3B194 returns to the suspended controller pass, then36898. Do not
        // insert an extra39574/RNG presentation between child and caller.
        if(resume_pass) {
            uint16_t aggregate=0;
            for(unsigned c=0;c<8;++c) if(user_setup_.connected()&(1u<<c)) aggregate|=user_setup_.raw(c);
            if(aggregate!=0x80) user_setup_refusal_logged_=false;
            do {
            for(const auto& action:user_setup_.step(static_cast<int32_t>(uint64_t(menu_elapsed_ms_)*120/1000))) {
                if(action.sound) playBottomMenuSound(action.sound,"user-setup");
                if(action.event==NBA97_USER_REFUSED && user_setup_refusal_logged_) continue;
                if(action.event==NBA97_USER_REFUSED) user_setup_refusal_logged_=true;
                const bool global=action.event==NBA97_USER_CONFIRMED || action.event==NBA97_USER_REFUSED;
                trace_.log("USER-INPUT","owner80037010/36B80/36CA0 token="+std::to_string(action.token)+
                    " event="+std::to_string(action.event)+(global ? " scope=global":
                    " controller="+std::to_string(action.controller)+
                    " side="+std::to_string(action.old_side)+"->"+std::to_string(action.new_side)+
                    " profile="+std::to_string(action.old_profile)+"->"+std::to_string(action.new_profile))+
                    " sound="+std::to_string(action.sound)+
                    (action.event==NBA97_USER_REFUSED ? " reason=active-editor-or-unresolved-Start-New":"")+
                    (action.event==NBA97_USER_CANCELLED ? " original-cancel-context-origin=8":""));
                if(action.event==NBA97_USER_HELP) {
                    const unsigned index=user_setup_.state().alphabet[action.controller]>=0 ? 1:0;
                    user_setup_.openHelp(user_setup_assets_->help().descriptor(5,static_cast<uint8_t>(index)).rect,index);
                    user_setup_.tickHelp(); // First modal presentation advances growth.
                    playBottomMenuSound(7,"user-help-open");
                } else if(action.event==NBA97_USER_SAVE_REQUEST) {
                    try {
                        if(!user_setup_.saveEditor(action.controller,profile_store_))throw std::runtime_error(profile_store_.lastError());
                        playBottomMenuSound(9,"user-name-accepted");updateUserProfileCount();
                        trace_.log("USER-SAVE","owner80037E90..80037F50; exact name accepted; native profile generation="+
                            std::to_string(profile_store_.generation())+"; Start latched; no match launch");
                    } catch(const std::exception& e) {
                        trace_.log("USER-SAVE-FAILED",std::string(e.what())+"; editor draft retained");
                        openUserDialog(nba97::UserSetupDialog::SaveFailure,action.controller);
                    }
                } else if(action.event==NBA97_USER_CANCELLED) {
                    trace_.log("USER-EXIT-WAIT","state5 Select100 result-1;3FD10/3B194 awaits changed aggregate; no further controller pass");
                    break;
                } else if(action.event==NBA97_USER_CONFIRMED) {
                    captureMatchSnapshot();
                    trace_.log("MATCH-HANDOFF-PENDING","state5 result6; assignments committed; bounded snapshot attempted; presentation/extension settings and gameplay initialization pending; no gameplay launched");
                    user_setup_.deferMatch();
                } else if(action.event==NBA97_USER_CAPACITY || action.event==NBA97_USER_PROFILE_FULL ||
                          action.event==NBA97_USER_NAME_DUPLICATE || action.event==NBA97_USER_DELETE_REQUEST) {
                    const auto kind=action.event==NBA97_USER_CAPACITY ? nba97::UserSetupDialog::Capacity:
                        action.event==NBA97_USER_PROFILE_FULL ? nba97::UserSetupDialog::Full:
                        action.event==NBA97_USER_NAME_DUPLICATE ? nba97::UserSetupDialog::Duplicate:nba97::UserSetupDialog::Delete;
                    std::string name;
                    if(kind==nba97::UserSetupDialog::Duplicate)name=user_setup_.state().draft[action.controller];
                    if(kind==nba97::UserSetupDialog::Delete)name=user_setup_.names().name[user_setup_.state().profile[action.controller]];
                    openUserDialog(kind,action.controller,name);
                }
            }
            // A successful durable save has no child modal. Complete that
            // same row and any remaining controllers before36898 presents.
            } while(user_setup_.hasPendingRowTail() && !user_setup_.state().result &&
                    user_setup_.help().phase==NBA97_HELP_CLOSED && user_setup_.dialogKind()==nba97::UserSetupDialog::None);
        }
        prepareFrontendTitle();
        user_setup_.tickPresentation();
        presentFrontendTitle(user_setup_.state().result!=-1 && user_setup_.help().phase==NBA97_HELP_CLOSED &&
            user_setup_.dialogKind()==nba97::UserSetupDialog::None); // 36898 direct; modal39574 predraw.
        frontend_title_painted_=false;
        nba97_frontend_palette_tick(&team_select_palette_,team_select_assets_->backgrounds().bank(),33);
    }

    void handleMenuKey(WPARAM key) {
        updateFrontendPadKey(key); // Preserve input during fades and message-dispatch waits.
        if(cool_fact_flash_.remaining) return;
        if(reset_prompt_.modal.phase!=NBA97_HELP_CLOSED || reset_notice_) {handleResetKey(key);return;}
        if (frontend_transition_active_) return;
        if (bottom_select_pending_) return;
        if(setup_start_pending_) return;
        if(frontend_page_==nba97::FrontendPage::TeamSelect || frontend_page_==nba97::FrontendPage::UserSetup) return;
        if(isRosterEditor()){handleTradeKey(key);return;}
        if (frontend_page_ == nba97::FrontendPage::ReorderRosters) {
            handleReorderKey(key);
            return;
        }
        if (frontend_page_ == nba97::FrontendPage::ProfileSetup) {
            handleProfileKey(key);
            return;
        }
        if (frontend_page_ == nba97::FrontendPage::ViewRosters) {
            handleRosterViewKey(key);
            return;
        }
        if (frontend_page_ == nba97::FrontendPage::CreatePlayers) {
            handleCreatePlayerKey(key);
            return;
        }
        if (frontend_page_ != nba97::FrontendPage::GameSetup &&
            frontend_page_ != nba97::FrontendPage::Rules &&
            frontend_page_ != nba97::FrontendPage::Options) {
            bool changed = false;
            std::uint32_t sound_id = 0;
            const char* direction = "none";
            if (key == VK_LEFT) {
                changed = bottom_menu_.move(-1, 0);
                sound_id = recoveredMenuDirectionSound(-1, 0); direction = "left";
            } else if (key == VK_RIGHT) {
                changed = bottom_menu_.move(1, 0);
                sound_id = recoveredMenuDirectionSound(1, 0); direction = "right";
            } else if (key == VK_UP) {
                changed = bottom_menu_.move(0, -1);
                sound_id = recoveredMenuDirectionSound(0, -1); direction = "up";
            } else if (key == VK_DOWN) {
                changed = bottom_menu_.move(0, 1);
                sound_id = recoveredMenuDirectionSound(0, 1); direction = "down";
            }
            else if (key == VK_BACK || key == VK_RSHIFT) {
                beginFrontendTransition(nba97::FrontendPage::GameSetup, "back input");
                return;
            } else if (key == VK_RETURN || key == VK_SPACE || key == 'C') {
                activateRecoveredBottomSelection();
                return;
            }
            if (changed) {
                playBottomMenuSound(sound_id, direction);
                trace_.log("ROSTER-CARD-FOCUS", std::string("direction=") + direction +
                    " index=" + std::to_string(bottom_menu_.selected()) +
                    " label=" + bottom_menu_.selectedLabel() + " enabled=yes");
                rebuildMenuFrame();
                InvalidateRect(window_, nullptr, FALSE);
            } else if (sound_id != 0) {
                trace_.log("ROSTER-CARD-BOUNDARY", std::string("direction=") + direction +
                    " stayed=" + bottom_menu_.selectedLabel() +
                    "; boundary or recovered availability predicate blocked target");
            }
            return;
        }
        if (frontend_page_ != nba97::FrontendPage::GameSetup) {
            bool changed = false;
            if (key == VK_UP) changed = settings_menu_.move(-1);
            else if (key == VK_DOWN) changed = settings_menu_.move(1);
            else if (key == VK_LEFT) { adjustSetting(-1); return; }
            else if (key == VK_RIGHT) { adjustSetting(1); return; }
            else if (key == VK_BACK) {
                beginFrontendTransition(nba97::FrontendPage::GameSetup, "back input");
                return;
            }
            if (changed) {
                trace_.log("SETTINGS-FOCUS", std::string(settings_menu_.selectedLabel()));
                rebuildMenuFrame();
                InvalidateRect(window_, nullptr, FALSE);
            }
            return;
        }
        bool changed = false;
        menu_mouse_armed_ = false;
        if (key == VK_LEFT) changed = menu_.moveHorizontal(-1);
        else if (key == VK_RIGHT) changed = menu_.moveHorizontal(1);
        else if (key == VK_UP) changed = menu_.moveVertical(-1);
        else if (key == VK_DOWN) changed = menu_.moveVertical(1);
        else if (key == VK_RETURN) {
            if(team_select_held_!=0x80) return; // Whole raw chord is not exact Start.
            if(menu_.setupChoice(1)!=0) {
                trace_.log("SETUP-ROUTE-PENDING","8003F7C8 mode="+std::to_string(menu_.setupChoice(1))+
                    "; season/playoff route not implemented; no exhibition fallback");return;
            }
            nba97_team_poll_setup_start(&team_select_poll_,0);setup_start_pending_=true;
            setup_start_tick_=uint64_t(menu_elapsed_ms_)*30/1001;
            playBottomMenuSound(9,"setup-selector"); // 3E484 precedes3B194.
            trace_.log("SETUP-EXIT-WAIT","exact Start80; state0 shared history then3B194 input-change/cleanup beforeTeamSelect");
            return;
        }
        else if (key == VK_SPACE || key == 'C') {
            activateMenuSelection();
            return;
        }
        else if (key == 'D' && menu_.row()==nba97::MenuRow::GameOptions) {
            adjustSetupChoice(-1);return;
        }
        if (changed) {
            trace_.log("MENU-HOVER", std::string(menu_.row() == nba97::MenuRow::GameOptions
                                      ? "option=" : "button=") + menu_.selectedLabel());
            rebuildMenuFrame();
            InvalidateRect(window_, nullptr, FALSE);
        }
    }

    Nba97CreateMenuContext createPlayerContext() const {
        Nba97CreateMenuContext context{};
        context.new_context_allowed = static_cast<uint8_t>(
            nba97_created_first_free(&created_players_) >= 0);
        return context;
    }

    static std::uint16_t createDeleteKeyMask(WPARAM key) {
        return key==VK_UP ? 1:key==VK_DOWN ? 2:
            key=='C' || key==VK_SPACE || key==VK_RETURN ? 0x800:
            key=='X' || key==VK_ESCAPE || key==VK_BACK ? 0x100:0;
    }

    void createDeleteEvent(int event) {
        if(event&NBA97_RESET_OPEN) {
            playBottomMenuSound(12,"create-delete-confirm-open");
            trace_.log("CREATE-DELETE-MODAL","FUN_80040A1C address=0x"+
                addressHex(create_player_delete_address_)+" context="+create_player_delete_context_+
                " default=delete player; exact retail descriptor from local FEONLY pack");
        }
        if(event&NBA97_RESET_UP) playBottomMenuSound(3,"create-delete-confirm-up");
        if(event&NBA97_RESET_DOWN) playBottomMenuSound(4,"create-delete-confirm-down");
        if(event&(NBA97_RESET_UP|NBA97_RESET_DOWN))
            trace_.log("CREATE-DELETE-FOCUS",create_player_delete_prompt_.choice ? "cancel" : "delete player");
        if(event&NBA97_RESET_CHOSEN) {
            playBottomMenuSound(6,"create-delete-confirm-choice");
            playBottomMenuSound(8,"create-delete-confirm-close");
            trace_.log("CREATE-DELETE-CHOICE",create_player_delete_prompt_.choice ?
                "cancel chosen; wait for original shrink/return barrier" :
                "delete chosen; durable mutation deferred until original shrink/return barrier");
        }
        if(!(event&NBA97_RESET_RETURN)) return;
        create_player_delete_active_=false;
        if(create_player_delete_prompt_.choice) {
            trace_.log("CREATE-DELETE","cancelled; catalogue and durable generation unchanged");
            return;
        }
        auto candidate=created_players_;
        if(!nba97_created_delete(&candidate,create_player_delete_slot_)) {
            trace_.log("CREATE-DELETE-FAILED","selected record vanished; accepted catalogue retained");
            return;
        }
        try {
            if(!created_player_store_.save(candidate))
                throw std::runtime_error("Delete produced no durable change");
        } catch(const std::exception& error) {
            trace_.log("CREATE-DELETE-FAILED",std::string(error.what())+
                "; accepted catalogue retained and picker reopened");
            return;
        }
        created_players_=candidate;
        nba97_create_menu_open(&create_player_menu_,&created_players_,createPlayerContext());
        created_player_picker_active_=nba97_created_picker_open(&created_player_picker_,
            &created_players_,0x21)!=0;
        trace_.log("CREATE-DELETE","0x8004E768 committed slot="+
            std::to_string(create_player_delete_slot_)+" generation="+
            std::to_string(created_player_store_.generation())+" remaining="+
            std::to_string(nba97_created_count(&created_players_))+
            (created_player_picker_active_ ? "; Delete picker loop reconstructed" :
             "; last record removed, returned to manager"));
    }

    void openCreatePlayerDelete(int16_t slot) {
        if(slot<0 || slot>=NBA97_CREATED_PLAYER_CAPACITY) return;
        const auto& metadata=created_players_.metadata[slot];
        constexpr std::uint32_t free_agent=0x800AF352, bench=0x800AF3D6, starter=0x800AF460;
        create_player_delete_address_=metadata.team==29 ? free_agent :
            (metadata.roster_slot<5 ? starter : bench);
        create_player_delete_context_=metadata.team==29 ? "free-agent-pool" :
            (metadata.roster_slot<5 ? "starter" : "team-non-starter");
        create_player_delete_team_.clear();
        if(metadata.team<29) {
            if(const auto* team=roster_database_.team(metadata.team))
                create_player_delete_team_=std::string(team->city);
        }
        if(create_player_delete_team_.empty()) create_player_delete_team_="team";
        create_player_delete_slot_=slot;
        create_player_delete_tick_=menu_elapsed_ms_/17;
        create_player_delete_active_=true;
        createDeleteEvent(nba97_reset_open(&create_player_delete_prompt_,
            create_player_delete_assets_->rect(create_player_delete_address_),0x800,1));
    }

    void updateCreatePlayerDelete() {
        if(frontend_page_!=nba97::FrontendPage::CreatePlayers || !create_player_delete_active_) return;
        const auto tick=menu_elapsed_ms_/17;
        const auto first=tick>120 ? (std::max)(create_player_delete_tick_,tick-120):create_player_delete_tick_;
        std::uint16_t raw=0;
        for(auto key:{VK_UP,VK_DOWN,VK_RETURN,VK_SPACE,VK_ESCAPE,VK_BACK,int('C'),int('X')})
            if(GetAsyncKeyState(key)&0x8000) raw|=createDeleteKeyMask(key);
        for(auto frame=first;frame<tick;++frame)
            createDeleteEvent(nba97_reset_tick(&create_player_delete_prompt_,raw));
        create_player_delete_tick_=tick;
    }

    void createPlayerHelpEvent(Nba97HelpEvent event) {
        if(event==NBA97_HELP_OPEN_SOUND) playBottomMenuSound(7,"create-name-help-open");
        else if(event==NBA97_HELP_CLOSE_SOUND) playBottomMenuSound(8,"create-name-help-close");
        else if(event==NBA97_HELP_RETURNED)
            trace_.log("CREATE-HELP","FUN_80040A1C return barrier cleared; editor/name transaction resumed unchanged");
    }

    std::uint8_t createPlayerHelpIndex() const {
        return nba97_create_editor_help_index(&create_player_editor_,
                                              &create_player_name_editor_);
    }

    void openCreatePlayerHelp() {
        create_player_help_index_=createPlayerHelpIndex();
        const auto& descriptor=create_player_help_pack_->descriptor(0x22,create_player_help_index_);
        createPlayerHelpEvent(nba97_help_open(&create_player_help_,descriptor.rect,0x20));
        char route[160]{};
        sprintf_s(route,"FUN_80040FCC -> FUN_80040A1C state=0x22 index=%u descriptor=0x%08X; authored Help %u/5 opened; draft retained",
            unsigned(create_player_help_index_),unsigned(descriptor.address),
            unsigned(create_player_help_index_+1));
        trace_.log("CREATE-HELP",route);
    }

    void updateCreatePlayerHelp() {
        if(create_player_help_.phase==NBA97_HELP_CLOSED) return;
        std::uint16_t raw=0;
        for(auto key:{VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_RETURN,VK_SPACE,VK_ESCAPE,
                      VK_BACK,VK_RSHIFT,VK_F1,int('C'),int('D'),int('F'),int('H'),
                      int('S'),int('V'),int('X')})
            if(GetAsyncKeyState(key)&0x8000) raw|=createPlayerHelpKeyToken(key);
        createPlayerHelpEvent(nba97_help_tick(&create_player_help_,raw));
    }

    static std::uint16_t createPlayerNameKeyMask(WPARAM key) {
        return nba97::createPlayerNameKeyMask(static_cast<std::uint32_t>(key));
    }

    static std::uint16_t createPlayerHelpKeyToken(WPARAM key) {
        const auto callback_mask=createPlayerNameKeyMask(key);
        if(callback_mask) return callback_mask;
        /* Help only compares held tokens for a change; these host-only tokens
           let R1/R2 dismiss the modal without claiming a retail callback
           bit that has not been recovered for this editor. */
        if(key=='S') return 0x200;
        if(key=='X') return 0x400;
        return 0;
    }

    std::uint16_t createPlayerCollegeCount() const {
        std::vector<std::string> schools;
        schools.reserve(roster_database_.players().size());
        for (const auto& player : roster_database_.players())
            if (!player.school_name.empty() && player.school_name != "n/a")
                schools.push_back(player.school_name);
        std::sort(schools.begin(), schools.end());
        schools.erase(std::unique(schools.begin(), schools.end()), schools.end());
        return static_cast<std::uint16_t>(schools.size() + 1u); // zero is n/a
    }

    void handleCreatePlayerKey(WPARAM key) {
        if(create_player_delete_active_) {
            const auto mask=createDeleteKeyMask(key);
            if(mask==0x100)
                trace_.log("CREATE-DELETE-MODAL","Circle/Cancel ignored: recovered FUN_80040A1C only chooses with Cross");
            else createDeleteEvent(nba97_reset_input(&create_player_delete_prompt_,mask));
            rebuildMenuFrame(); if(window_)InvalidateRect(window_,nullptr,FALSE); return;
        }
        if(create_player_help_.phase!=NBA97_HELP_CLOSED) {
            createPlayerHelpEvent(nba97_help_input(&create_player_help_,
                createPlayerHelpKeyToken(key)));
            rebuildMenuFrame(); if(window_)InvalidateRect(window_,nullptr,FALSE); return;
        }
        if (created_player_picker_active_) {
            if (key == VK_RSHIFT || key == VK_ESCAPE || key == VK_BACK) {
                created_player_picker_active_=false;
                trace_.log("CREATE-PICK", "cancelled state="+
                    std::to_string(created_player_picker_.frontend_state)+
                    "; manager catalogue unchanged");
            } else if (key == VK_UP || key == VK_DOWN) {
                if(nba97_created_picker_move(&created_player_picker_,key==VK_UP?-1:1)) {
                    trace_.log("CREATE-PICK", "FUN_8004E184 cursor="+
                        std::to_string(created_player_picker_.cursor)+" top="+
                        std::to_string(created_player_picker_.top)+" slot="+
                        std::to_string(nba97_created_picker_slot(&created_player_picker_)));
                    playBottomMenuSound(recoveredMenuDirectionSound(0,key==VK_UP?-1:1),"create-player-picker");
                } else trace_.log("CREATE-BOUNDARY", "created-player picker hard stop");
            } else if (key == VK_RETURN || key == VK_SPACE || key == 'C') {
                const auto slot=nba97_created_picker_slot(&created_player_picker_);
                if(created_player_picker_.frontend_state==0x20 &&
                   nba97_create_editor_open_edit(&create_player_editor_,&created_players_,slot)) {
                    nba97_create_editor_set_college_count(&create_player_editor_,
                        createPlayerCollegeCount());
                    created_player_picker_active_=false; create_player_editor_active_=true;
                    trace_.log("CREATE-EDIT", "state 0x20 selected slot="+std::to_string(slot)+
                        "; FUN_8004D514 copied the original 44-byte draft plus decoded port metadata");
                    playBottomMenuSound(6,"create-edit");
                } else if(created_player_picker_.frontend_state==0x21) {
                    openCreatePlayerDelete(slot);
                }
            }
            rebuildMenuFrame(); if(window_)InvalidateRect(window_,nullptr,FALSE); return;
        }
        if (create_player_editor_active_) {
            if (create_player_name_editor_.active) {
                int sound = 0;
                const char* action = nullptr;
                const auto retail_name_input=[&](Nba97CreateNameCommand command) {
                    const auto before_editor=create_player_editor_;
                    const auto before_name=create_player_name_editor_;
                    const auto result=nba97_create_name_input(&create_player_editor_,
                        &create_player_name_editor_,command);
                    const auto& value=create_player_name_editor_.field==NBA97_CREATE_FIRST_NAME?
                        create_player_editor_.first_name:create_player_editor_.last_name;
                    const int retail_limit=menu_font_.textWidth("Weatherspoon")-6;
                    const int rejected_width=menu_font_.textWidth(value);
                    if(result&&rejected_width>retail_limit) {
                        create_player_editor_=before_editor;
                        create_player_name_editor_=before_name;
                        trace_.log("CREATE-NAME-WIDTH",
                            "FUN_8004C488 rejected edit: rendered width="+
                            std::to_string(rejected_width)+" limit="+
                            std::to_string(retail_limit)+
                            " (Weatherspoon minus six pixels); prior bytes/cursor restored");
                    }
                    return result;
                };
                if (key == VK_UP) {
                    sound = retail_name_input(NBA97_CREATE_NAME_NEXT_CHARACTER);
                    action = "next-character";
                } else if (key == VK_DOWN) {
                    sound = retail_name_input(NBA97_CREATE_NAME_PREVIOUS_CHARACTER);
                    action = "previous-character";
                } else if (key == VK_LEFT) {
                    sound = retail_name_input(NBA97_CREATE_NAME_CURSOR_LEFT);
                    action = "cursor-left";
                } else if (key == VK_RIGHT) {
                    sound = retail_name_input(NBA97_CREATE_NAME_CURSOR_RIGHT);
                    action = "cursor-right";
                } else if (key == 'C' || key == VK_SPACE) {
                    sound = retail_name_input(NBA97_CREATE_NAME_ADD);
                    action = "Cross/add";
                } else if (key == 'D') {
                    sound = retail_name_input(NBA97_CREATE_NAME_DELETE);
                    action = "Square/delete";
                } else if (key == 'V' || key == VK_BACK) {
                    sound = retail_name_input(NBA97_CREATE_NAME_BACKSPACE);
                    action = key == 'V' ? "Circle/backspace" : "host-Backspace convenience";
                } else if (key == VK_RETURN) {
                    sound = nba97_create_name_accept(&create_player_editor_,
                        &create_player_name_editor_);
                    action = "Start/accept";
                } else if (key == VK_RSHIFT || key == VK_ESCAPE) {
                    sound = nba97_create_name_cancel(&create_player_editor_,
                        &create_player_name_editor_);
                    action = key==VK_RSHIFT ? "Select/cancel-restore" :
                        "host-Escape/cancel-restore";
                } else if (key == 'F' || key == 'H' || key == VK_F1) {
                    openCreatePlayerHelp();
                    rebuildMenuFrame(); if(window_)InvalidateRect(window_,nullptr,FALSE);
                    return;
                } else if (key >= 'A' && key <= 'Z' &&
                           (GetKeyState(VK_CONTROL) & 0x8000)) {
                    const auto before_editor=create_player_editor_;
                    const auto before_name=create_player_name_editor_;
                    sound = nba97_create_name_set_character(&create_player_editor_,
                        &create_player_name_editor_, static_cast<char>(key));
                    const auto& value=create_player_name_editor_.field==NBA97_CREATE_FIRST_NAME?
                        create_player_editor_.first_name:create_player_editor_.last_name;
                    if(sound && menu_font_.textWidth(value)>menu_font_.textWidth("Weatherspoon")-6) {
                        create_player_editor_=before_editor;
                        create_player_name_editor_=before_name;
                        trace_.log("CREATE-NAME-WIDTH",
                            "Ctrl+letter host convenience rejected by retail pixel-width bound");
                    }
                    action = "Ctrl+letter host convenience";
                }
                if (action != nullptr) {
                    const auto& name = create_player_name_editor_.field ==
                        NBA97_CREATE_FIRST_NAME ? create_player_editor_.first_name :
                        create_player_editor_.last_name;
                    trace_.log(sound ? "CREATE-NAME-INPUT" : "CREATE-NAME-BOUNDARY",
                        std::string(action)+" field="+
                        nba97_create_field_name(create_player_name_editor_.field)+
                        " cursor="+std::to_string(create_player_name_editor_.cursor)+
                        " length="+std::to_string(create_player_name_editor_.length)+
                        " value=\""+name+"\""+
                        (sound ? " sound="+std::to_string(sound) : " blocked"));
                    if (sound) playBottomMenuSound(static_cast<std::uint32_t>(sound),
                                                   "create-name-inline");
                    rebuildMenuFrame();
                    if(window_) InvalidateRect(window_,nullptr,FALSE);
                }
                return;
            }
            bool changed = false;
            const auto previous = create_player_editor_.selected_field;
            char prior_value[64]{};
            nba97_create_editor_value(&create_player_editor_,prior_value,sizeof(prior_value));
            if (key == VK_UP)
                changed = nba97_create_editor_move(&create_player_editor_, -1) != 0;
            else if (key == VK_DOWN)
                changed = nba97_create_editor_move(&create_player_editor_, 1) != 0;
            else if (key == VK_LEFT)
                changed = nba97_create_editor_adjust(&create_player_editor_, -1) != 0;
            else if (key == VK_RIGHT)
                changed = nba97_create_editor_adjust(&create_player_editor_, 1) != 0;
            else if ((key == 'C' || key == VK_SPACE) &&
                     (create_player_editor_.selected_field == NBA97_CREATE_FIRST_NAME ||
                      create_player_editor_.selected_field == NBA97_CREATE_LAST_NAME)) {
                const auto sound=nba97_create_name_begin(&create_player_editor_,
                    &create_player_name_editor_);
                trace_.log("CREATE-NAME-OPEN",
                    "FUN_8004C488 inline editor on screen 0x22 field="+
                    std::string(nba97_create_field_name(create_player_name_editor_.field))+
                    " cursor=0 alphabet=56 max=12 sound="+std::to_string(sound)+
                    "; Start accepts, Select restores all 13 bytes");
                if(sound) playBottomMenuSound(static_cast<std::uint32_t>(sound),
                                               "create-name-open");
                rebuildMenuFrame(); if(window_)InvalidateRect(window_,nullptr,FALSE); return;
            }
            else if ((key == 'C' || key == VK_SPACE) &&
                     create_player_editor_.selected_field >= NBA97_CREATE_FIELD_GOALS) {
                const auto sound=nba97_create_editor_toggle_rating_group(&create_player_editor_);
                trace_.log("CREATE-RATING-FOCUS",std::string(create_player_editor_.rating_group_active ?
                    "FUN_8004B400 Cross -> group" : "FUN_8004B4F8 Cross -> remembered individual")+
                    " return-field="+std::to_string(create_player_editor_.selected_field)+
                    " help-page="+std::to_string(createPlayerHelpIndex()+1)+" sound=6");
                if(sound) playBottomMenuSound(sound,"create-rating-focus");
                rebuildMenuFrame(); if(window_)InvalidateRect(window_,nullptr,FALSE); return;
            }
            else if (key == 'F' || key == 'H' || key == VK_F1) {
                openCreatePlayerHelp();
                rebuildMenuFrame(); if(window_)InvalidateRect(window_,nullptr,FALSE); return;
            }
            else if (key == VK_RSHIFT || key == VK_ESCAPE) {
                nba97_create_editor_cancel(&create_player_editor_.txn);
                create_player_editor_active_ = false;
                trace_.log("CREATE-CANCEL",
                    "editor transaction discarded; created-player catalogue unchanged");
                nba97_create_menu_open(&create_player_menu_, &created_players_,
                                       createPlayerContext());
                rebuildMenuFrame();
                InvalidateRect(window_, nullptr, FALSE);
                return;
            } else if (key == VK_RETURN || key == VK_SPACE) {
                nba97::CreatedPlayerAcceptStatus acceptance;
                try {
                    acceptance=created_player_store_.acceptEditor(create_player_editor_,created_players_);
                } catch (const std::exception& error) {
                    trace_.log("CREATE-SAVE", std::string("durable commit failed; live catalogue and editor retained: ")+error.what());
                    return;
                }
                if (acceptance==nba97::CreatedPlayerAcceptStatus::Invalid) {
                    trace_.log("CREATE-VALIDATE",
                        "START blocked: original requires non-empty first and last names");
                    return;
                }
                create_player_editor_active_ = false;
                nba97_create_menu_open(&create_player_menu_, &created_players_,
                                       createPlayerContext());
                trace_.log("CREATE-SAVE", "0x8004D328 committed slot=" +
                    std::to_string(create_player_editor_.txn.slot) + " id=" +
                    std::to_string(NBA97_CREATED_PLAYER_FIRST_ID +
                                   create_player_editor_.txn.slot) + " created=" +
                    std::to_string(nba97_created_count(&created_players_)) + " free=" +
                    std::to_string(NBA97_CREATED_PLAYER_CAPACITY -
                                   nba97_created_count(&created_players_)) +
                    "; generation="+std::to_string(created_player_store_.generation())+
                    (acceptance==nba97::CreatedPlayerAcceptStatus::Written ?
                     "; atomic local save; immediate return to manager, no confirmation dialog" :
                     "; unchanged edit accepted; no disk write or generation bump; return to manager"));
                playBottomMenuSound(6, "create-save");
                rebuildMenuFrame();
                InvalidateRect(window_, nullptr, FALSE);
                return;
            }
            if (changed) {
                char value[64]{};
                nba97_create_editor_value(&create_player_editor_, value, sizeof(value));
                const auto field = create_player_editor_.selected_field;
                if(create_player_editor_.rating_group_active) {
                    const unsigned first=1+((field-NBA97_CREATE_FIELD_GOALS)/4)*4;
                    const auto* values=create_player_editor_.ratings+first;
                    trace_.log("CREATE-RATING-GROUP", "FUN_8004B710 -> "+
                        std::string(key==VK_LEFT?"8004B600":"8004B688")+
                        " displayed="+std::to_string(values[0])+","+std::to_string(values[1])+","+
                        std::to_string(values[2])+","+std::to_string(values[3])+
                        "; four-member clamp/all-at-limit wrap; Help 5/5");
                }
                trace_.log(previous == field ? "CREATE-ADJUST" : "CREATE-FIELD",
                    "field=" + std::to_string(field) + " name=" +
                    nba97_create_field_name(field) + " value=\"" + value + "\"" +
                    (previous==field ? std::string(" prior=\"")+prior_value+"\"; direction="+
                        (key==VK_LEFT?"left":"right")+"; endpoint policy="+
                        (create_player_editor_.rating_group_active?"group clamp/all-limit wrap":"wrap") : "")+
                    (field >= NBA97_CREATE_SKIN_TONE &&
                     field <= NBA97_CREATE_FACIAL_HAIR
                        ? "; close-up camera + localized head/texture rebuild required"
                        : field == NBA97_CREATE_TEAM
                            ? "; 0x8004DCF4 team uniform rebuild required"
                            : field == NBA97_CREATE_HEIGHT
                                ? "; 0x80067F50 live model scale rebuild required"
                                : field >= NBA97_CREATE_SHOOTING_RANGE
                                    ? "; full-body preview camera"
                                    : ""));
                if(previous!=field && create_player_editor_.scroll_ticks_remaining)
                    trace_.log("CREATE-SCROLL", "selector bank "+
                        std::to_string(create_player_editor_.previous_visible_first_field)+" -> "+
                        std::to_string(create_player_editor_.visible_first_field)+
                        "; recovered six-vblank list transition armed");
                if(field==NBA97_CREATE_FIRST_NAME||field==NBA97_CREATE_LAST_NAME) {
                    const auto& name=field==NBA97_CREATE_FIRST_NAME ?
                        create_player_editor_.first_name:create_player_editor_.last_name;
                    trace_.log("CREATE-NAME", "retail 0x0D-byte buffer length="+
                        std::to_string(std::strlen(name))+"/12");
                }
                playBottomMenuSound(previous == field
                    ? recoveredMenuDirectionSound(key == VK_LEFT ? -1 : 1, 0)
                    : recoveredMenuDirectionSound(0, key == VK_UP ? -1 : 1),
                    previous == field ? "create-adjust" : "create-field");
                rebuildMenuFrame();
                InvalidateRect(window_, nullptr, FALSE);
            } else if (key == VK_DOWN || key == VK_UP || key == VK_LEFT ||
                       key == VK_RIGHT) {
                trace_.log("CREATE-BOUNDARY", "field=" +
                    std::to_string(create_player_editor_.selected_field) + " name=" +
                    nba97_create_field_name(create_player_editor_.selected_field) +
                    "; required-name gate or recovered value/navigation boundary");
            }
            return;
        }
        bool changed = false;
        const char* direction = nullptr;
        std::uint32_t sound_id = 0;
        if (key == VK_LEFT) {
            changed = nba97_create_menu_move(&create_player_menu_, -1) != 0;
            direction = "left";
            sound_id = recoveredMenuDirectionSound(-1, 0);
        } else if (key == VK_RIGHT) {
            changed = nba97_create_menu_move(&create_player_menu_, 1) != 0;
            direction = "right";
            sound_id = recoveredMenuDirectionSound(1, 0);
        } else if (key == VK_RSHIFT || key == VK_ESCAPE || key == VK_BACK) {
            beginFrontendTransition(nba97::FrontendPage::Rosters,
                "Create Player manager back input");
            return;
        } else if (key == VK_RETURN || key == VK_SPACE || key == 'C') {
            if ((create_player_menu_.selected == 0 && create_player_menu_.enabled[0]) ||
                (create_player_menu_.selected == 2 && create_player_menu_.enabled[2])) {
                const auto state=static_cast<uint8_t>(create_player_menu_.selected==0?0x20:0x21);
                if(!nba97_created_picker_open(&created_player_picker_,&created_players_,state)) {
                    trace_.log("CREATE-PICK", "blocked: no occupied created-player records"); return;
                }
                created_player_picker_active_=true;
                trace_.log("CREATE-PICK", "FUN_8004E184 opened state="+std::to_string(state)+
                    " count="+std::to_string(created_player_picker_.count)+
                    " visible="+std::to_string(created_player_picker_.visible_count)+
                    "; hard boundaries and independent cursor/top restored");
                playBottomMenuSound(6,"create-player-picker");
                rebuildMenuFrame(); if(window_)InvalidateRect(window_,nullptr,FALSE); return;
            }
            if (create_player_menu_.selected == 1 && create_player_menu_.enabled[1]) {
                if (!nba97_create_editor_open_new(&create_player_editor_,
                                                  &created_players_)) {
                    trace_.log("CREATE-NEW", "blocked: all 40 original slots occupied");
                    return;
                }
                nba97_create_editor_set_college_count(&create_player_editor_,
                    createPlayerCollegeCount());
                create_player_editor_active_ = true;
                trace_.log("CREATE-NEW", "0x8004D514 opened isolated 44-byte draft slot=" +
                    std::to_string(create_player_editor_.txn.slot) +
                    "; defaults height=5'3\" weight=200 lbs shooting-range=8 ft ratings=50; first/last required");
                playBottomMenuSound(6, "create-new");
                rebuildMenuFrame();
                InvalidateRect(window_, nullptr, FALSE);
                return;
            }
            char status[96]{};
            nba97_create_menu_status(&create_player_menu_, status, sizeof(status));
            trace_.log("CREATE-SELECT", "screen 0x1F card=" +
                std::to_string(create_player_menu_.selected) + " status=\"" + status +
                "\"; Edit/Delete child remains a separate bounded slice");
            playBottomMenuSound(6, "create-select");
            return;
        }
        if (changed) {
            playBottomMenuSound(sound_id, direction);
            char status[96]{};
            nba97_create_menu_status(&create_player_menu_, status, sizeof(status));
            trace_.log("CREATE-FOCUS", std::string("direction=") + direction +
                " card=" + std::to_string(create_player_menu_.selected) +
                " created=" + std::to_string(create_player_menu_.created_count) +
                " status=\"" + status + "\"");
            rebuildMenuFrame();
            InvalidateRect(window_, nullptr, FALSE);
        } else if (direction != nullptr) {
            trace_.log("CREATE-BOUNDARY", std::string(direction) +
                " blocked by edge or disabled Edit/Delete card");
        }
    }

    void activateRecoveredBottomSelection() {
        if(reset_prompt_.modal.phase!=NBA97_HELP_CLOSED || reset_notice_) return;
        if (bottom_select_pending_) return;
        if (!bottom_menu_.enabled(bottom_menu_.selected())) {
            const int selected = bottom_menu_.selected();
            trace_.log("ROSTER-CARD-LOCK", std::string(bottom_menu_.selectedLabel()) +
                (selected == 3
                    ? " disabled: FUN_80057C48 requires roster != original defaults"
                    : selected == 7
                        ? " disabled: FUN_80057A98 requires at least one non-zero injury record"
                        : " disabled by recovered object predicate"));
            return;
        }
        if(frontend_page_==nba97::FrontendPage::Rosters && bottom_menu_.selected()==3 &&
           !reset_commit_pending_) {
            if(!reset_assets_) reset_assets_=std::make_unique<nba97::RosterResetAssets>(options_.asset_root);
            reset_tick_=menu_elapsed_ms_/17;
            resetEvent(nba97_reset_open(&reset_prompt_,reset_assets_->rect(),0x800,0));
            trace_.log("ROSTER-RESET","80057960 -> 80040A1C descriptor=800AEDD2; default=cancel; C/Space choose, Up/Down focus; no save before confirmation");
            return;
        }
        bottom_select_pending_ = true;
        bottom_select_flash_start_ms_ = menu_elapsed_ms_;
        bottom_select_index_ = bottom_menu_.selected();
        playBottomMenuSound(6, "select");
        trace_.log("ROSTER-CARD-SELECT", "FUN_8003F240 index=" +
            std::to_string(bottom_select_index_) + " label=" + bottom_menu_.selectedLabel() +
            "; FUN_8002F124 sound=6; selected overlay toggles 12 vblanks (~204 ms)");
        rebuildMenuFrame();
        if(window_) InvalidateRect(window_, nullptr, FALSE);
    }

    void completeRecoveredBottomSelection() {
        if (frontend_page_ == nba97::FrontendPage::Rosters && bottom_menu_.selected() == 6) {
            beginFrontendTransition(nba97::FrontendPage::CreatePlayers,
                "Create Players selected; 80057CE4 -> 8004DAE8 screen 0x1F");
            return;
        }
        if(frontend_page_==nba97::FrontendPage::Rosters && bottom_menu_.selected()==2) {
            if(!bottom_menu_.enabled(2))return;
            beginFrontendTransition(nba97::FrontendPage::ReleasePlayers,"Release selected; 80057CE4 -> state17 -> 8005721C");return;
        }
        if(frontend_page_==nba97::FrontendPage::Rosters && bottom_menu_.selected()==1) {
            beginFrontendTransition(nba97::FrontendPage::SignFreeAgent,"Sign selected; 80057CE4 -> state14 -> 80056F9C");return;
        }
        if(frontend_page_==nba97::FrontendPage::Rosters && bottom_menu_.selected()==0) {
            beginFrontendTransition(nba97::FrontendPage::TradePlayers,"Trade selected; 80057CE4 -> state13 -> 80056CD0");return;
        }
        if(frontend_page_==nba97::FrontendPage::Rosters && bottom_menu_.selected()==3 && reset_commit_pending_) {
            reset_commit_pending_=false;
            commitRosterReset();
            return;
        }
        if (frontend_page_ == nba97::FrontendPage::Rosters && bottom_menu_.selected() == 5) {
            beginFrontendTransition(nba97::FrontendPage::ReorderRosters,
                "Re-order selected; 80057CE4 -> state 0x0C -> 80056AEC");
            return;
        }
        if (frontend_page_ == nba97::FrontendPage::Rosters && bottom_menu_.selected() == 4) {
            beginFrontendTransition(nba97::FrontendPage::ViewRosters,
                "view rosters selected; FUN_80057CE4 return=6 pushes state 0x10 FUN_800592C4");
            return;
        }
        trace_.log("MENU-BLOCK", std::string(bottom_menu_.selectedLabel()) +
                                 " child flow not yet decompiled");
    }

    bool rosterResetEligible() const {
        const auto slots=roster_database_.slotTable();
        // Only normal frontend context exists in the port. Future season/
        // special contexts must supply their real +2FC4/+2FC3 bytes here.
        return nba97_reset_enabled(slots.data(),roster_database_.originalSlots().data(),0,0)!=0;
    }

    void showResetNotice(const char* title,const char* detail) {
        reset_notice_=nba97::FrontendHelpDescriptor{9,0,0,{116,70,280,100},
            {{true,0,title},{true,0,detail},{true,0,"see CLI for details"},{true,0,"press a button to continue"}}};
        resetNoticeEvent(nba97_help_open(&reset_notice_modal_,reset_notice_->rect,0x800));
    }

    void resetNoticeEvent(Nba97HelpEvent event) {
        if(event==NBA97_HELP_OPEN_SOUND) playBottomMenuSound(7,"reset-notice-open");
        else if(event==NBA97_HELP_CLOSE_SOUND) playBottomMenuSound(8,"reset-notice-close");
        else if(event==NBA97_HELP_RETURNED) {
            reset_notice_.reset();
            trace_.log("ROSTER-RESET-NOTICE","closed after input-change barrier; no automatic retry");
        }
    }

    void commitRosterReset() {
        nba97::RosterCommitResult result{};
        try {
            if(!roster_store_) throw std::runtime_error("roster save unavailable: "+roster_load_error_);
            if(!rosterResetEligible()) throw std::runtime_error("Reset eligibility changed before commit");
            result=roster_store_->commit(roster_database_,roster_database_.originalSlots(),reorder_save_hooks_);
        } catch(const std::exception& e) {
            trace_.log("ROSTER-RESET-FAILED",std::string(e.what())+"; accepted roster retained; no success claim; reopen Reset to retry");
            showResetNotice("reset not saved","accepted rosters retained");
            rebuildMenuFrame();
            return;
        }
        // After this commit boundary, a later UI/reporting failure must never
        // be described as an uncommitted Reset or trigger a retry.
        bottom_menu_.setRosterCapabilities(rosterResetEligible(),false);
        trace_.log("ROSTER-RESET-COMMIT","80057864 normal roster intent; 29x15+100 original slots; derived owners/counts rebuilt; generation="+
            std::to_string(roster_store_->accepted().generation)+"; bytes="+std::to_string(result.bytes)+
            "; sync="+std::to_string(result.sync_completed)+"; private base/profiles/settings unchanged; Reset locked again");
        if(!result.sync_completed) showResetNotice("reset saved - sync uncertain","do not retry this reset");
        rebuildMenuFrame();
    }

    void resetEvent(int event) {
        if(event&NBA97_RESET_OPEN) playBottomMenuSound(12,"reset-confirm-open");
        if(event&NBA97_RESET_UP) playBottomMenuSound(3,"reset-confirm-up");
        if(event&NBA97_RESET_DOWN) playBottomMenuSound(4,"reset-confirm-down");
        if(event&(NBA97_RESET_UP|NBA97_RESET_DOWN))
            trace_.log("ROSTER-RESET-FOCUS",reset_prompt_.choice ? "cancel" : "restore defaults");
        if(event&NBA97_RESET_CHOSEN) {
            playBottomMenuSound(6,"reset-confirm-choice");
            playBottomMenuSound(8,"reset-confirm-close");
            trace_.log("ROSTER-RESET-CHOICE",reset_prompt_.choice ? "cancel; no mutation" : "restore requested; wait for close/flash before durable commit");
        }
        if(event&NBA97_RESET_RETURN) {
            if(reset_prompt_.choice==0) {
                reset_commit_pending_=true;
                activateRecoveredBottomSelection(); // original 3F324 flash is AFTER confirmation
            } else trace_.log("ROSTER-RESET","cancelled; accepted roster/save generation untouched");
        }
    }

    static std::uint16_t resetKeyMask(WPARAM key) {
        return key==VK_UP ? 1:key==VK_DOWN ? 2:key=='C' || key==VK_SPACE || key==VK_RETURN ? 0x800:
            key=='X' || key==VK_ESCAPE || key==VK_BACK ? 0x100:key==VK_LEFT ? 8:key==VK_RIGHT ? 4:0;
    }
    void handleResetKey(WPARAM key) {
        const auto mask=resetKeyMask(key);
        if(reset_notice_) resetNoticeEvent(nba97_help_input(&reset_notice_modal_,mask));
        else resetEvent(nba97_reset_input(&reset_prompt_,mask));
    }
    void tickReset(std::uint16_t raw) {
        if(reset_notice_) resetNoticeEvent(nba97_help_tick(&reset_notice_modal_,raw));
        else resetEvent(nba97_reset_tick(&reset_prompt_,raw));
    }
    void updateReset() {
        if(frontend_page_!=nba97::FrontendPage::Rosters) return;
        const auto tick=menu_elapsed_ms_/17;
        const auto first=tick>120 ? (std::max)(reset_tick_,tick-120):reset_tick_;
        std::uint16_t raw=0;
        for(auto key:{VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_RETURN,VK_SPACE,VK_ESCAPE,VK_BACK,int('C'),int('X')})
            if(GetAsyncKeyState(key)&0x8000) raw|=resetKeyMask(key);
        for(auto i=first;i<tick;++i) tickReset(raw);
        reset_tick_=tick;
    }
    PshImage renderBottomMenu() {
        const bool visible=!bottom_select_pending_ ||
            (((menu_elapsed_ms_-bottom_select_flash_start_ms_)/kPsxVblankMs)&1u)!=0u;
        auto im=nba97::renderRecoveredBottomMenu(bottom_menu_,menu_font_,menu_sprites_,roster_sprites_,users_sprites_,
            roster_menu_cards_,menu_elapsed_ms_,visible);
        if(frontend_page_==nba97::FrontendPage::Rosters && reset_assets_) {
            reset_assets_->draw(im,reset_prompt_,menu_elapsed_ms_/17);
            if(reset_notice_) nba97::FrontendHelpPack::draw(im,reset_assets_->font(),*reset_notice_,reset_notice_modal_);
        }
        return im;
    }

    void playBottomMenuSound(std::uint32_t sound_id, const char* role) {
        try {
            const auto root = options_.asset_root / "menu";
            const auto info = cursor_audio_.playCursorSound(
                root / "ZCURSOR.VH", root / "ZCURSOR.VB", sound_id, settings_.option(3));
            const auto effective_percent =
                (info.program_volume * info.tone_volume * info.playback_volume * 100u) /
                (127u * 127u * 127u);
            trace_.log("ROSTER-CARD-SFX", std::string("role=") + role +
                " FUN_8002F124 id=" + std::to_string(sound_id) +
                " rate=" + std::to_string(info.sample_rate) + "Hz samples=" +
                std::to_string(info.rendered_sample_count) + " source-samples=" +
                std::to_string(info.sample_count) + " root/request=" +
                std::to_string(info.root_note) + "/" +
                std::to_string(info.requested_note) + " pitch=" +
                std::to_string(info.pitch_cents) + "c effective-gain=" +
                std::to_string(effective_percent) + "% setting=" +
                std::to_string(settings_.option(3)) + " playback=" +
                (info.playback_suppressed ? "suppressed" : "submitted"));
        } catch (const std::exception& error) {
            trace_.log("AUDIO-ERROR", std::string("rosters ZCURSOR decode/play failed: ") +
                error.what());
        }
    }

    void logRosterViewFocus(const char* reason) {
        const auto* team = roster_viewer_.selectedTeam(viewerDatabase());
        const auto* player = roster_viewer_.selectedPlayer(viewerDatabase());
        trace_.log(roster_viewer_.mode() == nba97::RosterViewMode::TeamRoster
                       ? "ROSTER-FOCUS" : "PLAYER-CARD",
            std::string(reason) + "; team=" +
            (team ? team->displayName() : "<none>") +
            " player=" + (player ? player->displayName() : "<none>") +
            " category=" + std::to_string(roster_viewer_.category()) +
            " display=" + std::to_string(roster_viewer_.displayIndex()) +
            " list-window=" + std::to_string(roster_viewer_.firstVisiblePlayer()) + ".." +
            std::to_string(roster_viewer_.firstVisiblePlayer() + 5) +
            " stat-window=" + std::to_string(roster_viewer_.firstVisiblePlayerStat()) + ".." +
            std::to_string(roster_viewer_.firstVisiblePlayerStat() + 5) +
            (player ? " id=" + std::to_string(player->id) + " number=" +
                player->jerseyNumberText() + " position=" +
                nba97::positionName(player->position) : ""));
    }

    void logPlayerStatLayer(const char* control) {
        const auto& change = roster_viewer_.lastStatLayerChange();
        trace_.log("PLAYER-STAT-LAYER",
            std::string(control) + " FUN_80059610 mask=0x" +
            (change.input_mask == 0x1000 ? "1000" : "2000") +
            " layer=" + std::to_string(change.previous_layer) + "->" +
            std::to_string(change.current_layer) +
            (change.skipped_layer_four ? " (restricted layer 4 skipped)" : "") +
            " layer-label-object=0x1B descriptors=" +
            std::to_string(change.descriptor_last_index + 1) +
            " table=" + std::to_string(change.descriptor_table) +
            " extent-change=" + (change.descriptor_extent_changed ? "yes" : "no") +
            " animation=primary:" +
            (change.primary_animation_reset ? "reset" : "retained") +
            "/secondary:" +
            (change.secondary_animation_reset ? "reset" : "retained") +
            " refreshed=" + std::to_string(change.primary_refresh_count) + "+" +
            std::to_string(change.secondary_refresh_count) +
            " controller-page=rebuild/restore stat-scroll=0");
    }

    void playRosterCursorSound(int direction) {
        try {
            const auto root = options_.asset_root / "menu";
            const std::uint32_t sound_id = direction < 0 ? 3u : 4u;
            const auto info = cursor_audio_.playCursorSound(
                root / "ZCURSOR.VH", root / "ZCURSOR.VB", sound_id, settings_.option(3));
            const auto effective_percent =
                (info.program_volume * info.tone_volume * info.playback_volume * 100u) /
                (127u * 127u * 127u);
            stat_flash_direction_ = direction;
            stat_flash_until_ms_ = menu_elapsed_ms_ + 340;
            trace_.log("PLAYER-STAT-FLASH", "FUN_8002AB88 gold transition=20 ticks; "
                "FUN_8002F124 sound=" + std::to_string(sound_id) +
                " ZCURSOR.VH/VB rate=" + std::to_string(info.sample_rate) +
                " samples=" + std::to_string(info.rendered_sample_count) +
                " source-samples=" + std::to_string(info.sample_count) +
                " root/request=" + std::to_string(info.root_note) + "/" +
                std::to_string(info.requested_note) + " pitch=" +
                std::to_string(info.pitch_cents) + "c" +
                " gain=" + std::to_string(info.program_volume) + "/127*" +
                std::to_string(info.tone_volume) + "/127*" +
                std::to_string(info.playback_volume) + "/127 (" +
                std::to_string(effective_percent) + "%) setting=" +
                std::to_string(settings_.option(3)) + " playback=" +
                (info.playback_suppressed ? "suppressed" : "submitted"));
        } catch (const std::exception& error) {
            trace_.log("AUDIO-ERROR", std::string("ZCURSOR decode/play failed: ") + error.what());
        }
    }

    static std::uint16_t playerNoticeKeyMask(WPARAM key) {
        switch(key) {
        case VK_UP: return 1; case VK_DOWN: return 2;
        case VK_LEFT: return 8; case VK_RIGHT: return 4;
        case 'C': case VK_SPACE: case VK_LBUTTON: return 0x800;
        case VK_RETURN: return 0x80;
        case 'X': case VK_ESCAPE: case VK_BACK: return 0x100;
        case 'F': case 'H': case VK_F1: return 0x20;
        case 'D': return 0x10; case 'S': return 0x40;
        case 'J': case VK_OEM_4: return 0x200;
        case 'K': case VK_OEM_6: return 0x400;
        case 'Q': return 0x1000; case 'E': return 0x2000;
        default: return 0;
        }
    }
    void playerNoticeEvent(Nba97HelpEvent event) {
        if(event==NBA97_HELP_OPEN_SOUND) playBottomMenuSound(5,"player-no-facts-open");
        else if(event==NBA97_HELP_CLOSE_SOUND) {
            playBottomMenuSound(8,"player-no-facts-close");
            trace_.log("PLAYER-NOTICE","text removed; shrink; child input blocked");
        } else if(event==NBA97_HELP_RETURNED) {
            held_roster_direction_=0;
            trace_.log("PLAYER-NOTICE","returned after input-change barrier; player/team/stat and parent draft retained");
        }
    }
    void updatePlayerNotice() {
        std::uint16_t raw=0;
        constexpr int keys[]{VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,'C',VK_SPACE,VK_RETURN,'X',VK_ESCAPE,VK_BACK,
                            'F','H',VK_F1,'D','S','J',VK_OEM_4,'K',VK_OEM_6,'Q','E',VK_LBUTTON};
        for(int key:keys)
            if(GetAsyncKeyState(key)&0x8000) raw|=playerNoticeKeyMask(key);
        const auto tick=menu_elapsed_ms_/17;
        const auto first=tick>120 ? (std::max)(player_notice_tick_,tick-120) : player_notice_tick_;
        for(auto frame=first;frame<tick && player_notice_.phase!=NBA97_HELP_CLOSED;++frame)
            playerNoticeEvent(nba97_help_tick(&player_notice_,raw));
        player_notice_tick_=tick;
    }
    void drawPlayerNotice(PshImage& image) {
        if(nba97_help_visible(&player_notice_))
            nba97::FrontendHelpPack::draw(image,control_font_,*player_notice_descriptor_,player_notice_);
    }
    void resolveCoolFactChoice(const char* reason) {
        unsigned draws=0;
        while(cool_fact_selection_.draw_mode && draws<1024) {
            nba97_fact_offer_random(&cool_fact_selection_,nba97_frontend_random(&frontend_rng_));
            ++draws; ++frontend_rng_draws_;
        }
        if(cool_fact_selection_.draw_mode) throw std::runtime_error("native Cool Fact RNG draw guard exhausted");
        std::string flags;
        for(auto value:cool_fact_selection_.flags) {if(!flags.empty())flags+=",";flags+=std::to_string(value);}
        trace_.log("COOL-FACT-CHOICE",std::string(reason)+" selected="+std::to_string(cool_fact_selection_.selected)+
            " flags=["+flags+"] draws="+std::to_string(draws)+
            "; original selection predicates/generator; shared native title/Cool Fact RNG (original history not matched)");
    }
    void startCoolFactFlash() {
        if(nba97_fact_flash_begin(&cool_fact_flash_)!=NBA97_FACT_READY)
            throw std::runtime_error("Cool Fact flash already active");
        held_roster_direction_=0;
        cool_fact_flash_tick_=menu_elapsed_ms_/17;
        cool_fact_flash_painted_=false;
        trace_.log("COOL-FACT-FLASH","59EBC object21/o18b on; o18a retained; eight presents; input callback held; native17ms cadence, original timing unverified");
    }
    void advanceCoolFactFlash() {
        const auto event=nba97_fact_flash_presented(&cool_fact_flash_,&cool_fact_selection_);
        if(event==NBA97_FACT_INVALID) throw std::runtime_error("invalid Cool Fact flash context");
        trace_.log("COOL-FACT-FLASH","completed-present="+std::to_string(8-cool_fact_flash_.remaining)+
            " overlay="+std::to_string(nba97_fact_flash_visible(&cool_fact_flash_))+
            " consumed="+std::to_string(event==NBA97_FACT_READY));
        if(event==NBA97_FACT_READY) resolveCoolFactChoice("consumed-after-eight-presents-59E14");
    }
    void playSelectedCoolFact(std::uint16_t invoking_mask=0x800) {
        if(cool_fact_flash_.remaining) return;
        const auto* player = roster_viewer_.selectedPlayer(viewerDatabase());
        if (!player) return;
        try {
            const auto preparation=nba97_fact_prepare(&cool_fact_selection_);
            if(preparation==NBA97_FACT_INVALID) throw std::runtime_error("invalid Cool Fact selection context");
            if(preparation==NBA97_FACT_DRAW) resolveCoolFactChoice("prepare-59D18");
            if(cool_fact_selection_.selected==-1) {
                if(!player_notice_descriptor_)
                    player_notice_descriptor_=nba97::loadPlayerNotice(options_.asset_root/"player/no-facts.n97ui");
                player_notice_tick_=menu_elapsed_ms_/17;
                playerNoticeEvent(nba97_help_open(&player_notice_,player_notice_descriptor_->rect,invoking_mask));
                trace_.log("PLAYER-NOTICE","59E14 -> 40A1C descriptor=0x800AFE06; player="+
                    std::to_string(player->id)+"; rect=136,90,240,64 style=1; original pack/ZFONT1; no speech request");
                return;
            }
            const auto variant=static_cast<std::uint32_t>(cool_fact_selection_.selected);
            if(cool_fact_selection_.flags[variant]==-1) {
                // 593F0's count-based refresh may choose a hole in sparse data.
                // 31630 then returns without a clip; 59E14 still gives feedback.
                playBottomMenuSound(6,"cool-fact-select");
                startCoolFactFlash();
                trace_.log("COOL-FACT-EMPTY","original sparse refresh selected absent variant="+std::to_string(variant)+
                    "; no substituted clip; normal eight-present overlay/consumption lifecycle");
                return;
            }
            const auto root = options_.asset_root / "menu";
            auto prepared = cool_fact_audio_.prepareCoolFact(
                root / "Z1COOL.IDX", root / "Z1COOL.BIG", player->id,variant);
            trace_.log("COOL-FACT-PREPARE","31630/314A0 record="+std::to_string(prepared.info.record)+
                " decoded-bytes="+std::to_string(prepared.pcm.size()*sizeof(std::int16_t))+"; no device submission");
            cool_fact_audio_.stop(); // 314A0 stops the previous voice before cue6/start.
            playBottomMenuSound(6,"cool-fact-select");
            const auto info=cool_fact_audio_.startCoolFact(std::move(prepared),settings_.option(2));
            trace_.log("COOL-FACT-START","31770 record="+std::to_string(info.record)+
                " speech-setting="+std::to_string(settings_.option(2))+
                " gain="+std::to_string(info.playback_volume)+"/127 playback=submitted"+
                (info.playback_volume ? std::string{} : "; silent voice retains duration/stop lifecycle"));
            startCoolFactFlash();
            trace_.log("COOL-FACT-AUDIO", "FUN_80059E14 -> FUN_80031630 -> FUN_80031770; "
                "player=" + player->displayName() + " record=" +
                std::to_string(info.record) + " " + info.source +
                " codec=PSX-ADPCM mono rate=" + std::to_string(info.sample_rate) +
                " samples=" + std::to_string(info.sample_count) + " duration-ms=" +
                std::to_string(info.sample_count * 1000u / info.sample_rate));
        } catch (const std::exception& error) {
            trace_.log("AUDIO-ERROR", std::string("Cool Fact decode/play/notice failed: ") + error.what());
        }
    }

    void handleRosterViewKey(WPARAM key) {
        if(cool_fact_flash_.remaining) return;
        if(player_notice_.phase!=NBA97_HELP_CLOSED) {
            playerNoticeEvent(nba97_help_input(&player_notice_,playerNoticeKeyMask(key)));
            rebuildMenuFrame();
            return;
        }
        const auto& database = viewerDatabase();
        if (roster_viewer_.helpVisible()) {
            if (key == 'H' || key == VK_F1 || key == VK_ESCAPE || key == VK_BACK ||
                key == VK_RETURN || key == VK_SPACE) {
                roster_viewer_.dismissHelp();
                trace_.log("HELP", "modal dismissed; restored active frontend state");
                rebuildMenuFrame();
                InvalidateRect(window_, nullptr, FALSE);
            }
            return;
        }
        if (key == 'H' || key == VK_F1) {
            roster_viewer_.toggleHelp();
            trace_.log("HELP", std::string("internal 0x20 FUN_80040FCC state=") +
                (roster_viewer_.mode() == nba97::RosterViewMode::TeamRoster ?
                 "0x10 descriptor=0x800B146C" : "0x24 descriptor=0x800B22F0"));
            rebuildMenuFrame();
            InvalidateRect(window_, nullptr, FALSE);
            return;
        }
        if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard &&
            (key == VK_ESCAPE || key == VK_BACK)) {
            roster_viewer_.returnToRoster();
            player_photo_loader_.reset();
            roster_portrait_loaded_ = false;
            trace_.log("PLAYER-CARD", "state 0x24 popped -> state 0x10; team/row/top preserved");
            rebuildMenuFrame();
            InvalidateRect(window_, nullptr, FALSE);
            return;
        }
        bool changed = false;
        const std::size_t previous_team = roster_viewer_.teamIndex();
        const std::size_t previous_player = roster_viewer_.playerIndex();
        const std::size_t previous_stat = roster_viewer_.firstVisiblePlayerStat();
        const auto scan_player_team = [&](int direction, const char* control) {
            const bool moved = roster_viewer_.scanTeam(direction, database, menu_elapsed_ms_);
            trace_.log("PLAYER-TEAM-SCAN", std::string(control) + " FUN_80059ABC team=" +
                std::to_string(previous_team) + "->" + std::to_string(roster_viewer_.teamIndex()) +
                " slot=" + std::to_string(previous_player) + "->" + std::to_string(roster_viewer_.playerIndex()) +
                " stat-top=" + std::to_string(previous_stat) + "->" +
                std::to_string(roster_viewer_.firstVisiblePlayerStat()) +
                (moved ? " retained/backtracked; original runtime comparison pending" : " unchanged/guarded"));
            return moved;
        };
        if (key == VK_LEFT)
            changed = roster_viewer_.move(-1, 0, database, menu_elapsed_ms_);
        else if (key == VK_RIGHT)
            changed = roster_viewer_.move(1, 0, database, menu_elapsed_ms_);
        else if (key == VK_UP)
            changed = roster_viewer_.move(0, -1, database, menu_elapsed_ms_);
        else if (key == VK_DOWN)
            changed = roster_viewer_.move(0, 1, database, menu_elapsed_ms_);
        else if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard &&
                 (key == VK_OEM_4 || key == 'J')) {
            changed = scan_player_team(-1, "L1");
        } else if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard &&
                   (key == VK_OEM_6 || key == 'K')) {
            changed = scan_player_team(1, "R1");
        }
        else if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard && key == 'Q') {
            changed = roster_viewer_.cycleCategory(-1);
            logPlayerStatLayer("L2/previous");
        } else if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard && key == 'E') {
            changed = roster_viewer_.cycleCategory(1);
            logPlayerStatLayer("R2/next");
        } else if (key == 'Z') {
            changed = roster_viewer_.cycleDisplay(-1);
            trace_.log("ROSTER-DISPLAY", "R2/internal 0x0200: previous field");
        } else if (key == 'C') {
            changed = roster_viewer_.cycleDisplay(1);
            trace_.log("ROSTER-DISPLAY", "L1/internal 0x0400: next field");
        }
        else if (key == VK_RETURN || key == VK_SPACE) {
            if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard) {
                playSelectedCoolFact(playerNoticeKeyMask(key));
                rebuildMenuFrame();
                InvalidateRect(window_, nullptr, FALSE);
                return;
            }
            if (roster_viewer_.selectedPlayer(database)) {
                roster_viewer_.activate(database);
                cool_fact_selection_={};cool_fact_selection_.selected=-1;
                loadSelectedPlayerCardAssets();
                const auto& run = roster_viewer_.playerCardRunState();
                trace_.log("ROSTER-VIEW",
                    "internal 0x10 -> result=2 -> FUN_8005A538 layout=0x24; "
                    "archives=" + std::string(run.portrait_index) + "/" +
                    run.portrait_archive + "+" + run.cool_fact_index + "/" +
                    run.cool_fact_archive + " manager=" +
                    std::to_string(run.object_count) + " objects/" +
                    std::to_string(run.visible_row_count) + " rows layer=" +
                    std::to_string(run.current_stat_layer) + "/" +
                    std::to_string(run.stat_layer_limit) + " team/slot/player=" +
                    std::to_string(run.selected_team) + "/" +
                    std::to_string(run.selected_roster_slot) + "/" +
                    std::to_string(run.selected_player_id) +
                    " controller=0x39 flags=0x10000 object=0x10 value=0x0D "
                    "frontend=0x11 input=-1 draw=0x8005A280 action=0x8005A3FC");
                rebuildMenuFrame();
                InvalidateRect(window_, nullptr, FALSE);
                return;
            }
            trace_.log("ROSTER-MESSAGE",
                key == VK_RETURN ? "view player: blank roster slot" :
                                   "compare players: blank roster slot");
        } else if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard && key == 'S') {
            const bool stopped=cool_fact_audio_.isPlaying();
            cool_fact_audio_.stop();
            const auto cue=nba97_frontend_fact_stop_sound(stopped,1);
            if(cue) playBottomMenuSound(cue,"cool-fact-stop");
            trace_.log("COOL-FACT-STOP","Square/internal 0x10 -> 59DB8(1); stopped="+
                std::to_string(stopped)+" cue="+std::to_string(cue)+"; idle stop is silent");
            rebuildMenuFrame();
            InvalidateRect(window_, nullptr, FALSE);
            return;
        } else if (key == VK_ESCAPE || key == VK_BACK) {
            roster_viewer_.cancel();
            trace_.log("ROSTER-CANCEL",
                "input 0x100: restored entry +0x70E/+0x712/+0x716 snapshot");
            beginFrontendTransition(nba97::FrontendPage::Rosters,
                                    "View Rosters back input");
            return;
        }
        if (changed) {
            const bool player_cycle_input =
                roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard &&
                (key == VK_LEFT || key == VK_RIGHT);
            if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard &&
                (player_cycle_input || roster_viewer_.playerIndex() != previous_player ||
                 roster_viewer_.teamIndex() != previous_team)) {
                cool_fact_audio_.stop();
                loadSelectedPlayerCardAssets(false);
            }
            if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard) {
                // 3D930 selects the sound by the INPUT route, not by any
                // incidental index reset inside the callback. In particular
                // 59610 extent changes must not sound/flash like scrolling up.
                if ((key == VK_UP || key == VK_DOWN) &&
                    roster_viewer_.firstVisiblePlayerStat() != previous_stat)
                    playRosterCursorSound(key == VK_DOWN ? 1 : -1);
                else if (player_cycle_input)
                    playBottomMenuSound(key == VK_LEFT ? 2 : 1, "view-input");
                else if (key == 'J' || key == 'K' || key == VK_OEM_4 || key == VK_OEM_6 ||
                         key == 'Q' || key == 'E')
                    playBottomMenuSound(6, "view-input");
            }
            if (player_cycle_input) {
                const auto& cycle = roster_viewer_.lastPlayerCycle();
                trace_.log("PLAYER-CYCLE", "FUN_80059928 mask=" +
                    std::to_string(cycle.input_mask) + " slot=" +
                    std::to_string(cycle.previous_slot) + "->" +
                    std::to_string(cycle.current_slot) + "/" +
                    std::to_string(cycle.roster_count) +
                    (cycle.wrapped ? " wrapped" : " advanced") +
                    " player-id=" + std::to_string(cycle.resolved_player_id) +
                    " transition-frames=" + std::to_string(cycle.transition_frames) +
                    " header-refresh=" + std::to_string(cycle.header_refresh_start) +
                    "+" + std::to_string(cycle.header_refresh_count) +
                    " rows=" + std::to_string(cycle.visible_row_refresh_start) +
                    "+" + std::to_string(cycle.visible_row_refresh_count) +
                    " cool-fact=stop/rebuild stat-scroll=preserved@" +
                    std::to_string(roster_viewer_.firstVisiblePlayerStat()));
            }
            if (roster_viewer_.mode() == nba97::RosterViewMode::TeamRoster &&
                roster_viewer_.teamIndex() != previous_team) {
                const auto* from = previous_team < database.teams().size()
                    ? &database.teams()[previous_team] : nullptr;
                const auto* to = roster_viewer_.selectedTeam(database);
                trace_.log("ROSTER-PALETTE", "FUN_8003F7B0 target " +
                    (from ? from->displayName() : "<none>") + " -> " +
                    (to ? to->displayName() : "<none>") +
                    "; FUN_8002FF80 transition ticks=0..16");
            }
            logRosterViewFocus(key == VK_RETURN || key == VK_SPACE ? "player view" : "navigation");
            rebuildMenuFrame();
            InvalidateRect(window_, nullptr, FALSE);
        }
    }

    void handleProfileKey(WPARAM key) {
        if (profile_menu_.editing()) {
            if (key == VK_BACK && profile_menu_.backspace()) {
                trace_.log("PROFILE-EDIT", "backspace; length=" +
                    std::to_string(profile_menu_.draft().size()));
            } else if (key == VK_RETURN) {
                if (profile_menu_.commit(profile_store_)) {
                    active_user_profiles_ = static_cast<int>(profile_store_.profiles().size());
                    menu_.setActiveUserProfiles(active_user_profiles_);
                    trace_.log("PROFILE-SAVE", profile_store_.profiles()[profile_menu_.selected()].name +
                        " generation=" + std::to_string(profile_store_.generation()) +
                        "; atomic CRC32 container updated");
                } else {
                    trace_.log("PROFILE-REJECT", profile_menu_.message());
                }
            } else if (key == VK_ESCAPE) {
                profile_menu_.cancel();
                trace_.log("PROFILE-EDIT", "name edit cancelled");
            }
        } else if (key == VK_UP || key == VK_DOWN) {
            if (profile_menu_.move(key == VK_UP ? -1 : 1, profile_store_.profiles().size()))
                trace_.log("PROFILE-FOCUS", profile_menu_.selected() == profile_store_.profiles().size()
                    ? "Start New" : profile_store_.profiles()[profile_menu_.selected()].name);
        } else if (key == VK_RETURN || key == VK_SPACE) {
            profile_menu_.beginEdit(profile_store_);
            trace_.log("PROFILE-EDIT", profile_menu_.selected() < profile_store_.profiles().size()
                ? "edit existing name" : "Start New; recovered editor max=13");
        } else if (key == VK_DELETE) {
            const std::string name = profile_menu_.selected() < profile_store_.profiles().size()
                ? profile_store_.profiles()[profile_menu_.selected()].name : std::string{};
            if (profile_menu_.requestDelete(profile_store_)) {
                active_user_profiles_ = static_cast<int>(profile_store_.profiles().size());
                menu_.setActiveUserProfiles(active_user_profiles_);
                trace_.log("PROFILE-DELETE", name + "; FUN_80036D48 zero-record semantic; generation=" +
                    std::to_string(profile_store_.generation()));
            } else if (!profile_menu_.message().empty()) {
                trace_.log("PROFILE-CONFIRM", profile_menu_.message());
            }
        } else if (key == VK_ESCAPE || key == VK_BACK) {
            beginFrontendTransition(nba97::FrontendPage::GameSetup, "user setup back input");
            return;
        }
        rebuildMenuFrame();
        InvalidateRect(window_, nullptr, FALSE);
    }

    void handleMenuHover(int client_x, int client_y) {
        if(setup_start_pending_) return;
        if(frontend_page_==nba97::FrontendPage::TeamSelect ||
           frontend_page_==nba97::FrontendPage::UserSetup) return;
        if(cool_fact_flash_.remaining) return;
        if(player_notice_.phase!=NBA97_HELP_CLOSED) return;
        if(reset_prompt_.modal.phase!=NBA97_HELP_CLOSED || reset_notice_) return;
        if (frontend_page_ == nba97::FrontendPage::ReorderRosters || isRosterEditor()) return;
        if (bottom_select_pending_) return;
        const bool genuinely_moved = last_mouse_x_ < 0 ||
            std::abs(client_x - last_mouse_x_) > 1 ||
            std::abs(client_y - last_mouse_y_) > 1;
        last_mouse_x_ = client_x;
        last_mouse_y_ = client_y;
        if (!menu_mouse_armed_) {
            if (!genuinely_moved) return;
            menu_mouse_armed_ = true;
        }
        RECT client{};
        GetClientRect(window_, &client);
        const RECT presentation = psxPresentationRect(client);
        const int presentation_width = presentation.right - presentation.left;
        const int presentation_height = presentation.bottom - presentation.top;
        if (presentation_width <= 0 || presentation_height <= 0 ||
            client_x < presentation.left || client_x >= presentation.right ||
            client_y < presentation.top || client_y >= presentation.bottom)
            return;
        const int psx_x = (client_x - presentation.left) * kPsxWidth / presentation_width;
        const int psx_y = (client_y - presentation.top) * kPsxHeight / presentation_height;
        if (frontend_page_ == nba97::FrontendPage::GameSetup) {
            if (!menu_.hover(psx_x, psx_y)) return;
            trace_.log("MENU-HOVER", std::string(menu_.row() == nba97::MenuRow::GameOptions
                                      ? "option=" : "button=") + menu_.selectedLabel());
        } else if (frontend_page_ == nba97::FrontendPage::Rules ||
                   frontend_page_ == nba97::FrontendPage::Options) {
            if (!settings_menu_.hover(psx_x, psx_y)) return;
            trace_.log("SETTINGS-FOCUS", settings_menu_.selectedLabel());
        } else if (frontend_page_ == nba97::FrontendPage::ViewRosters) {
            if (!roster_viewer_.hover(psx_x, psx_y, roster_database_)) return;
            logRosterViewFocus("mouse hover");
        } else {
            if (!bottom_menu_.hover(psx_x, psx_y)) return;
            trace_.log("SUBMENU-FOCUS", bottom_menu_.selectedLabel());
        }
        rebuildMenuFrame();
        InvalidateRect(window_, nullptr, FALSE);
    }

    bool clientToPsx(int client_x, int client_y, int& psx_x, int& psx_y) const {
        RECT client{};
        GetClientRect(window_, &client);
        const RECT presentation = psxPresentationRect(client);
        const int width = presentation.right - presentation.left;
        const int height = presentation.bottom - presentation.top;
        if (width <= 0 || height <= 0 || client_x < presentation.left ||
            client_x >= presentation.right || client_y < presentation.top ||
            client_y >= presentation.bottom)
            return false;
        psx_x = (client_x - presentation.left) * kPsxWidth / width;
        psx_y = (client_y - presentation.top) * kPsxHeight / height;
        return true;
    }

    void rebuildMenuFrame() {
        menu_.syncStyle(settings_.style());
        if(frontend_page_==nba97::FrontendPage::TeamSelect) {
            prepareFrontendTitle();
            auto image=nba97::renderTeamSelect(team_select_,team_select_ranks_,*team_select_assets_,
                team_select_sprites_,menu_font_,control_font_,team_select_palette_,team_select_tints_,frontend_title_.corners());
            if(nba97_help_visible(&team_select_help_)) team_select_assets_->help().draw(image,control_font_,
                team_select_assets_->help().descriptor(3,0),team_select_help_);
            menu_frame_=makeFrame(image);
        }
        else if(frontend_page_==nba97::FrontendPage::UserSetup) {
            prepareFrontendTitle();
            auto image=nba97::renderUserSetup(user_setup_.state(),user_setup_.names(),user_setup_.topology(),
                user_setup_.placement(),team_select_.team[0],team_select_.team[1],*user_setup_assets_,
                *team_select_assets_,team_select_sprites_,menu_font_,team_select_palette_,frontend_title_.corners(),
                &user_setup_.editorTints(),user_setup_.help().phase!=NBA97_HELP_CLOSED && user_setup_.helpIndex()==1);
            if(nba97_help_visible(&user_setup_.help())) user_setup_assets_->help().draw(image,control_font_,
                user_setup_assets_->help().descriptor(5,static_cast<uint8_t>(user_setup_.helpIndex())),user_setup_.help());
            if(user_setup_.dialogKind()!=nba97::UserSetupDialog::None)
                user_setup_assets_->drawDialog(image,control_font_,user_setup_.dialogKind(),user_setup_.dialogState(),user_dialog_name_);
            menu_frame_=makeFrame(image);
        }
        else if(isRosterEditor())menu_frame_=makeFrame(renderTrade());
        else if (frontend_page_ == nba97::FrontendPage::ReorderRosters)
            menu_frame_ = makeFrame(renderReorder());
        else if (frontend_page_ == nba97::FrontendPage::GameSetup)
            menu_frame_ = makeFrame(nba97::renderGameSetupMenu(
                menu_, title_source_, menu_font_, menu_sprites_, menu_cards_, menu_elapsed_ms_));
        else if (frontend_page_ == nba97::FrontendPage::ProfileSetup)
            menu_frame_ = makeFrame(nba97::renderUserProfileSetup(
                profile_menu_, profile_store_, menu_font_, menu_sprites_, menu_elapsed_ms_));
        else if (frontend_page_ == nba97::FrontendPage::CreatePlayers) {
            auto image=created_player_picker_active_
                ? nba97::renderCreatedPlayerPicker(created_player_picker_,created_players_,
                    roster_database_,menu_font_,create_player_sprites_,menu_elapsed_ms_)
                : create_player_editor_active_
                    ? nba97::renderCreatePlayerEditor(create_player_editor_, roster_database_,
                        menu_font_, create_player_sprites_, menu_elapsed_ms_,
                        create_player_preview_.get(), &create_player_name_editor_,
                        &control_font_)
                    : nba97::renderCreatePlayerMenu(create_player_menu_, menu_font_,
                        create_player_sprites_, create_player_cards_, menu_elapsed_ms_);
            if(create_player_delete_active_)
                create_player_delete_assets_->draw(image,create_player_delete_address_,
                    create_player_delete_team_,create_player_delete_prompt_);
            if(nba97_help_visible(&create_player_help_)) {
                create_player_help_pack_->draw(image,control_font_,
                    create_player_help_pack_->descriptor(0x22,create_player_help_index_),
                    create_player_help_);
            }
            menu_frame_=makeFrame(image);
        }
        else if (frontend_page_ == nba97::FrontendPage::ViewRosters) {
            prepareFrontendTitle();
            auto image = nba97::renderRosterViewer(
                roster_viewer_, roster_database_, menu_font_,
                roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard
                    ? player_sprites_ : roster_sprites_,
                menu_elapsed_ms_,
                roster_portrait_loaded_ ? &roster_portrait_ : nullptr,
                roster_cool_facts_available_, &control_font_,
                menu_elapsed_ms_ < stat_flash_until_ms_ ? stat_flash_direction_ : 0,
                nba97_fact_flash_visible(&cool_fact_flash_),
                player_photo_loader_.state().city_enabled != 0, frontend_title_.corners());
            drawPlayerNotice(image);
            menu_frame_=makeFrame(image);
        }
        else if (frontend_page_ == nba97::FrontendPage::Rules ||
                 frontend_page_ == nba97::FrontendPage::Options)
            menu_frame_ = makeFrame(nba97::renderSettingsMenu(
                settings_menu_, settings_, menu_font_, menu_sprites_, menu_elapsed_ms_));
        else
        {
            menu_frame_ = makeFrame(renderBottomMenu());
        }
    }

    static std::string frontendPageName(nba97::FrontendPage page) {
        if (page == nba97::FrontendPage::UserSetup) return "User Setup";
        if (page == nba97::FrontendPage::TeamSelect) return "Team Select";
        if (page == nba97::FrontendPage::Rules) return "Rules";
        if (page == nba97::FrontendPage::ProfileSetup) return "User Setup";
        if (page == nba97::FrontendPage::Options) return "Options";
        if (page == nba97::FrontendPage::Rosters) return "Rosters";
        if (page == nba97::FrontendPage::ViewRosters) return "View Rosters";
        if (page == nba97::FrontendPage::ReorderRosters) return "Re-order Rosters";
        if (page == nba97::FrontendPage::TradePlayers) return "Trade Players";
        if (page == nba97::FrontendPage::SignFreeAgent) return "Sign Free Agent";
        if (page == nba97::FrontendPage::ReleasePlayers) return "Release Players";
        if (page == nba97::FrontendPage::CreatePlayers) return "Create Player";
        if (page == nba97::FrontendPage::Users) return "Users";
        if (page == nba97::FrontendPage::Card) return "Memory Card";
        return "Game Setup";
    }

    void activateMenuSelection() {
        if(setup_start_pending_) return;
        if (menu_.row() == nba97::MenuRow::FrontendButtons) {
            static constexpr std::array<nba97::FrontendPage, 5> pages{
                nba97::FrontendPage::Rules, nba97::FrontendPage::Options,
                nba97::FrontendPage::Rosters, nba97::FrontendPage::Users,
                nba97::FrontendPage::Card};
            if (!menu_.buttonEnabled(menu_.selection())) {
                trace_.log("MENU-DISABLED", "users requires at least one active profile (FUN_8005CD88 -> object 0x2B flags 0x06)");
                return;
            }
            beginFrontendTransition(pages[static_cast<std::size_t>(menu_.selection())],
                                    std::string(menu_.selectedLabel()) + " selected");
            return;
        }
        adjustSetupChoice(1);
    }

    void adjustSetupChoice(int direction) {
        if(setup_start_pending_) return;
        menu_.syncStyle(settings_.style());
        const auto card=static_cast<unsigned>(menu_.selection());
        const auto before=menu_.setupChoice(card);
        if(!menu_.adjustSetupChoice(direction)) return;
        if(card==2) {settings_.adjustRule(14,direction);menu_.syncStyle(settings_.style());}
        playBottomMenuSound(6,"setup-choice");
        trace_.log("SETUP-CHOICE","8003EC1C/8003F43C card="+std::to_string(card)+" value="+
            std::to_string(before)+"->"+std::to_string(menu_.setupChoice(card))+"; quarter/mode/level session-local; style shares Rules");
        rebuildMenuFrame();if(window_) InvalidateRect(window_,nullptr,FALSE);
    }

    void beginFrontendTransition(nba97::FrontendPage target, const std::string& reason) {
        if (frontend_transition_active_ || setup_start_pending_ || target == frontend_page_) return;
        if(target==nba97::FrontendPage::TeamSelect && !openTeamSelect()) return;
        if(target==nba97::FrontendPage::UserSetup && !openUserSetup()) return;
        transition_source_ = menu_frame_;
        const auto previous_page = frontend_page_;
        frontend_page_ = target;
        if(target==nba97::FrontendPage::TeamSelect || target==nba97::FrontendPage::UserSetup) {}
        else if(target==nba97::FrontendPage::TradePlayers || target==nba97::FrontendPage::SignFreeAgent || target==nba97::FrontendPage::ReleasePlayers)openTrade();
        else if (target == nba97::FrontendPage::ReorderRosters) openReorder();
        else if (target == nba97::FrontendPage::ProfileSetup)
            profile_menu_.open(profile_store_.profiles().size());
        else if (target == nba97::FrontendPage::CreatePlayers) {
            const auto context = createPlayerContext();
            nba97_create_menu_open(&create_player_menu_, &created_players_, context);
            char status[96]{};
            nba97_create_menu_status(&create_player_menu_, status, sizeof(status));
            trace_.log("CREATE-ENTRY", "8004DAE8 objects=3 order=Edit/New/Delete; count=" +
                std::to_string(create_player_menu_.created_count) + " focus=" +
                std::to_string(create_player_menu_.selected) + " status=\"" + status + "\"");
            trace_.log("CREATE-CAPACITY", "8004AEBC scans 40 records x 68 bytes; IDs begin at 493; New additionally requires an open 535-slot roster/free-agent destination");
        }
        else if (target == nba97::FrontendPage::ViewRosters) {
            roster_viewer_.open(roster_database_);
            logRosterViewFocus("FUN_800592C4 restored selection");
        }
        else if (target == nba97::FrontendPage::Rules || target == nba97::FrontendPage::Options)
            settings_menu_.open(target);
        else if (target != nba97::FrontendPage::GameSetup) {
            bottom_menu_.open(target);
            if (target == nba97::FrontendPage::Rosters) {
                bottom_menu_.setRosterCapabilities(rosterResetEligible(), false);
                const int vacancies=nba97_sign_available(roster_database_.slotTable().data(),0,0,nullptr);
                bottom_menu_.setSignAvailable(vacancies!=0);
                const int release_available=nba97_release_available(roster_database_.slotTable().data(),0,0);
                bottom_menu_.setReleaseAvailable(release_available!=0);
                trace_.log("RELEASE-AVAILABILITY","80057B6C enabled="+std::to_string(release_available)+
                    " free="+std::to_string(roster_database_.freeAgentCount())+
                    " last-slot="+std::to_string(roster_database_.slotTable()[534])+
                    " mode0 restriction0; last-slot sentinel gate, not team minimum; single-stage release callback80057084");
                trace_.log("SIGN-AVAILABILITY","80057B00 vacancies="+std::to_string(vacancies)+" free="+std::to_string(roster_database_.freeAgentCount())+" mode0 restriction0");
                trace_.log("ROSTER-CARD-STATE", "Reset eligible="+
                    std::to_string(rosterResetEligible())+"; 535-slot default comparison; normal frontend context "
                    "without the special-state override; Injuries locked until an active context "
                    "has a non-zero entry among 536 injury bytes; authored red plates remain visible");
            }
            if (target == nba97::FrontendPage::Rosters &&
                previous_page == nba97::FrontendPage::ViewRosters)
                bottom_menu_.setSelected(4);
            if (target == nba97::FrontendPage::Rosters && previous_page == nba97::FrontendPage::ReorderRosters)
                bottom_menu_.setSelected(5);
            if (target == nba97::FrontendPage::Rosters && previous_page == nba97::FrontendPage::CreatePlayers)
                bottom_menu_.setSelected(6);
        }
        rebuildMenuFrame();
        frontend_transition_tick_ = GetTickCount();
        frontend_transition_active_ = true;
        transition_frame_ = transition_source_;
        trace_.log("TRANSITION", reason + "; recovered FE state=" +
            std::to_string(target == nba97::FrontendPage::TeamSelect ? 3 :
                           target == nba97::FrontendPage::UserSetup ? 5 :
                           target == nba97::FrontendPage::ProfileSetup ? -1 :
                           target == nba97::FrontendPage::ViewRosters ? 0x10 :
                           target == nba97::FrontendPage::ReorderRosters ? 0x0C :
                           target == nba97::FrontendPage::TradePlayers ? 0x0D :
                           target == nba97::FrontendPage::SignFreeAgent ? 0x0E :
                           target == nba97::FrontendPage::ReleasePlayers ? 0x11 :
                           target == nba97::FrontendPage::CreatePlayers ? 0x1F :
                           target == nba97::FrontendPage::Rules ? 1 :
                           target == nba97::FrontendPage::Options ? 2 :
                           target == nba97::FrontendPage::Rosters ? 9 :
                           target == nba97::FrontendPage::Users ? 19 :
                           target == nba97::FrontendPage::Card ? 11 : 0) +
            " -> " + frontendPageName(target) + " (original recovered pack/layout)");
        // Headless host regressions use this same transition handler. NULL
        // would invalidate every desktop window, not merely skip repaint.
        if(window_) InvalidateRect(window_, nullptr, FALSE);
    }

    void adjustSetting(int direction) {
        const int index = settings_menu_.selected();
        const bool changed = frontend_page_ == nba97::FrontendPage::Rules
            ? settings_.adjustRule(index, direction)
            : settings_.adjustOption(index, direction);
        if (!changed) return;
        settings_.save(options_.settings_path);
        const std::string value = frontend_page_ == nba97::FrontendPage::Rules
            ? settings_.ruleValue(index) : settings_.optionValue(index);
        trace_.log("SETTINGS-SAVE", std::string(settings_menu_.selectedLabel()) +
                                    "=" + value + " -> " + options_.settings_path.string());
        if (frontend_page_ == nba97::FrontendPage::Options && index == 1) {
            const auto recovered_volume =
                nba97_frontend_music_volume(settings_.option(1));
            frontend_music_.setRecoveredVolume(recovered_volume);
            trace_.log("MUSIC-VOLUME", "FUN_8002F258 value=" +
                std::to_string(recovered_volume) + "/127 applied to future music PCM only; queued audio <=4096 sample-frames; SFX/speech unaffected");
        }
        rebuildMenuFrame();
        InvalidateRect(window_, nullptr, FALSE);
    }

    Options options_;
    Trace trace_;
    std::unique_ptr<nba97::NativeFrameCapture> native_record_;
    std::unique_ptr<nba97::ProcessAudioCapture> native_audio_record_;
    std::uint64_t native_record_origin_=0;
    ComApartment com_apartment_;
    nba97::BootFlow flow_;
    nba97::IntroPlayer intro_player_;
    nba97::FrontendMusicPlayer frontend_music_;
    unsigned music_underruns_logged_=0;
    nba97::RecoveredAudioPlayer cursor_audio_;
    nba97::RecoveredAudioPlayer cool_fact_audio_;
    Nba97CoolFacts cool_fact_selection_{{0,0,0,0,0},-1,0,0,-1};
    // Port stream only. Rejection predicates match source, but global original
    // RNG consumption across other menus/gameplay is not yet reconstructed.
    std::uint16_t frontend_rng_=0; // Recovered zero-seed fallback; original startup history is not claimed.
    std::uint64_t frontend_rng_draws_=0;
    nba97::FrontendTitlePresentation frontend_title_;
    std::uint64_t frontend_title_tick_=0, frontend_title_presents_=0;
    bool frontend_title_painted_=false;
    Nba97FactFlash cool_fact_flash_{};
    std::uint32_t cool_fact_flash_tick_=0;
    bool cool_fact_flash_painted_=false;
    std::optional<nba97::FrontendHelpDescriptor> player_notice_descriptor_;
    Nba97HelpModal player_notice_{};
    std::uint32_t player_notice_tick_=0;
    nba97::MainMenu menu_;
    nba97::FrontendSettings settings_;
    nba97::SettingsMenu settings_menu_;
    nba97::RecoveredBottomMenu bottom_menu_;
    nba97::RosterViewer roster_viewer_;
    nba97::RosterDatabase roster_database_;
    std::unique_ptr<nba97::RosterSaveStore> roster_store_;
    std::string roster_load_error_;
    nba97::RosterSaveHooks reorder_save_hooks_{};
    std::optional<nba97::FrontendHelpDescriptor> reorder_notice_;
    bool reorder_exit_after_notice_=false;
    Nba97ReorderScreen reorder_screen_{};
    std::unique_ptr<nba97::TeamSelectAssets> team_select_assets_;
    nba97::MenuSpritePack team_select_sprites_;
    Nba97TeamSelect team_select_{{3,24},{3,24},0,0,0,0};
    Nba97TeamRanks team_select_ranks_{};
    Nba97FrontendPalette team_select_palette_{};
    Nba97HelpModal team_select_help_{};
    std::array<Nba97ReorderTint,12> team_select_tints_{};
    std::array<uint32_t,6> team_select_rng_{};
    unsigned team_select_focus_=0;
    Nba97TeamRandom team_select_random_{};
    uint64_t team_select_tick_=0;
    bool setup_start_pending_=false;
    bool setup_start_painted_=false;
    uint64_t setup_start_tick_=0;
    uint64_t team_select_presentations_=0;
    Nba97TeamPoll team_select_poll_{};
    std::array<bool,256> team_select_keys_{};
    uint16_t team_select_held_=0;
    bool team_select_exit_wait_=false;
    std::unique_ptr<nba97::UserSetupAssets> user_setup_assets_;
    nba97::UserSetupSession user_setup_;
    uint64_t user_setup_tick_=0;
    bool user_setup_refusal_logged_=false;
    std::string user_dialog_name_;
    std::unique_ptr<nba97::ReorderLabelPreview> reorder_labels_;
    std::unique_ptr<nba97::FrontendHelpPack> reorder_help_pack_;
    Nba97HelpModal reorder_help_{};
    std::uint8_t reorder_help_index_ = 0;
    std::uint8_t reorder_help_state_ = 12;
    Nba97TradeScreen trade_screen_{};
    std::unique_ptr<nba97::TradeAssets> trade_assets_;
    std::array<int16_t,2> trade_teams_{2,24},trade_child_teams_{};
    std::array<uint8_t,2> trade_cursors_{},trade_tops_{},trade_child_slots_{};
    int16_t sign_team_=2;
    int16_t release_team_=2; // Native remembered donor; original default context not asserted.
    std::array<uint8_t,2> sign_cursors_{},sign_tops_{};
    std::vector<uint8_t> trade_positions_,trade_injuries_;
    std::array<PshImage,2> trade_portraits_{};
    std::array<uint16_t,2> trade_portrait_ids_{};
    Nba97FrontendPalette trade_palette_{};
    Nba97ResetPrompt trade_choice_{};
    uint32_t trade_choice_address_=0,trade_tick_=0;
    uint16_t trade_child_exit_=0;
    Nba97ReorderChild reorder_child_{};
    std::unique_ptr<const nba97::RosterDatabase> reorder_child_database_;
    std::optional<nba97::RosterViewer> reorder_saved_viewer_;
    Nba97Compare reorder_compare_{};
    std::unique_ptr<nba97::CompareAssets> compare_assets_;
    std::unique_ptr<nba97::FrontendPaletteAssets> compare_backgrounds_;
    Nba97FrontendPalette compare_palette_{};
    Nba97CompareRefresh compare_refresh_{};
    Nba97CompareRepeat compare_repeat_{};
    std::array<Nba97ReorderTint,6> compare_arrows_{};
    bool compare_refresh_painted_=false;
    std::uint32_t compare_refresh_tick_=0;
    std::array<PshImage,2> compare_portraits_;
    std::array<std::uint16_t,2> compare_portrait_ids_{UINT16_MAX,UINT16_MAX};
    std::array<PshImage, 2> reorder_portraits_;
    std::array<std::uint16_t, 2> reorder_portrait_ids_{UINT16_MAX, UINT16_MAX};
    std::array<std::int16_t, 2> reorder_saved_cursor_{-1,-1}, reorder_saved_top_{-1,-1};
    std::int16_t reorder_team_ = 29;
    std::uint32_t reorder_tick_ = 0;
    int reorder_modal_frame_ = 0;
    bool reorder_discard_yes_ = false;
    nba97::UserProfileStore profile_store_;
    nba97::MatchSession match_session_;
    nba97::CreatedPlayerStore created_player_store_;
    std::unique_ptr<nba97::CreatePlayerDeleteAssets> create_player_delete_assets_;
    std::unique_ptr<nba97::FrontendHelpPack> create_player_help_pack_;
    Nba97HelpModal create_player_help_{};
    std::uint8_t create_player_help_index_=0;
    nba97::UserProfileMenu profile_menu_;
    nba97::FrontendPage frontend_page_ = nba97::FrontendPage::GameSetup;
    nba97::PshFont menu_font_;
    nba97::PshFont control_font_;
    nba97::MenuSpritePack menu_sprites_;
    nba97::MenuSpritePack roster_sprites_;
    nba97::MenuSpritePack player_sprites_;
    nba97::MenuSpritePack users_sprites_;
    nba97::MenuSpritePack create_player_sprites_;
    std::unique_ptr<nba97::CreatePlayerPreview> create_player_preview_;
    nba97::MenuCardPack menu_cards_;
    nba97::RosterCardPack roster_menu_cards_;
    nba97::CreatePlayerCardPack create_player_cards_;
    Nba97CreatedPlayerCatalog created_players_{};
    Nba97CreateMenu create_player_menu_{};
    Nba97CreateEditor create_player_editor_{};
    Nba97CreateNameEditor create_player_name_editor_{};
    bool create_player_editor_active_ = false;
    Nba97CreatedPlayerPicker created_player_picker_{};
    bool created_player_picker_active_ = false;
    Nba97ResetPrompt create_player_delete_prompt_{};
    bool create_player_delete_active_=false;
    std::int16_t create_player_delete_slot_=-1;
    std::uint32_t create_player_delete_address_=0;
    std::uint32_t create_player_delete_tick_=0;
    std::string create_player_delete_team_,create_player_delete_context_;
    PshImage roster_portrait_;
    bool roster_portrait_loaded_ = false;
    nba97::PlayerPhotoLoader player_photo_loader_{decodePlayerPhotoOnWorker};
    bool roster_cool_facts_available_ = false;
    std::uint32_t stat_flash_until_ms_ = 0;
    int stat_flash_direction_ = 0;
    PshImage title_source_;
    Frame load_frame_;
    Frame legal_frame_;
    Frame title_frame_;
    Frame menu_frame_;
    Frame transition_source_;
    Frame transition_frame_;
    std::filesystem::path intro_movie_;
    HWND window_ = nullptr;
    DWORD previous_tick_ = GetTickCount();
    std::uint32_t menu_elapsed_ms_ = 0;
    int last_mouse_x_ = -1;
    int last_mouse_y_ = -1;
    bool menu_mouse_armed_ = false;
    bool bottom_select_pending_ = false;
    std::unique_ptr<nba97::RosterResetAssets> reset_assets_;
    Nba97ResetPrompt reset_prompt_{};
    std::uint32_t reset_tick_=0;
    bool reset_commit_pending_=false;
    std::optional<nba97::FrontendHelpDescriptor> reset_notice_;
    Nba97HelpModal reset_notice_modal_{};
    std::uint32_t bottom_select_flash_start_ms_ = 0;
    int bottom_select_index_ = -1;
    static constexpr std::uint32_t kPsxVblankMs = 17;
    static constexpr std::uint32_t kBottomSelectFlashMs = 12 * kPsxVblankMs;
    int active_user_profiles_ = 0;
    int held_roster_direction_ = 0;
    int held_roster_counter_ = 0;
    int held_roster_ticks_since_repeat_ = 0;
    static constexpr std::uint32_t kFrontendTransitionMs = 180;
    DWORD frontend_transition_tick_ = 0;
    bool frontend_transition_active_ = false;
};
} // namespace

int main(int argc, char** argv) {
    try {
        BootApplication application(parseOptions(argc, argv));
        return application.run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NBA Live 97 C++ app error: %s\n", error.what());
        return 2;
    }
}
