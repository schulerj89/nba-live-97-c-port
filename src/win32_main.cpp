#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <objbase.h>

#include "boot_flow.hpp"
#include "frontend_music.hpp"
#include "recovered_audio.hpp"
#include "recovered/frontend_audio.h"
#include "recovered/semantic_trace.h"
#include "intro_player.hpp"
#include "main_menu.hpp"
#include "png_image.hpp"
#include "psh_image.hpp"
#include "psh_font.hpp"
#include "roster_database.hpp"
#include "user_profiles.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

struct Options {
    std::filesystem::path asset_root = ".local/assetpacks";
    std::filesystem::path trace_path = ".local/logs/boot_decomp_trace.log";
    std::filesystem::path settings_path = ".local/config/frontend_settings.ini";
    std::filesystem::path profiles_path = ".local/saves/user_profiles.n97sav";
    std::filesystem::path view_rosters_capture_dir;
    std::filesystem::path semantic_report_path =
        ".local/reports/view_rosters_semantic_trace.json";
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
        else if (arg == "--capture-view-rosters" && i + 1 < argc)
            options.view_rosters_capture_dir = argv[++i];
        else if (arg == "--semantic-report" && i + 1 < argc)
            options.semantic_report_path = argv[++i];
    }
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
        if (options_.self_test) return runSelfTest();
        if (!options_.view_rosters_capture_dir.empty())
            return captureViewRosters();
        registerWindowClass();
        createMainWindow();
        ShowWindow(window_, SW_SHOWNORMAL);
        SetForegroundWindow(window_);
        UpdateWindow(window_);
        if (!SetTimer(window_, kFrameTimer, kFrameIntervalMs, nullptr))
            throw std::runtime_error("SetTimer failed");
        trace_.log("DISPLAY", "native C++ Win32 app visible at (80,80); SPACE advances, ESC exits");

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
        validateMenuAsset(menu_root / "ZSET7.PSP", 323444);
        validateMenuAsset(menu_root / "ZSET8.PSP", 297432);
        loadMenuSprites(menu_root / "ZSET1-decoded");
        loadRecoveredBottomSprites(menu_root / "ZSET4-decoded", roster_sprites_, true);
        loadTeamRosterBackgrounds(menu_root / "ZSET4-team-backgrounds");
        loadRecoveredBottomSprites(menu_root / "ZSET7-decoded", users_sprites_, false);
        loadPlayerCardSprites(menu_root / "ZSET8-decoded");
        loadMenuCards(menu_root / "ZCARD-decoded");
        trace_.log("RECOVERED", "0x8002F258 selects ZTMENU1.CNK frontend audio");
        trace_.log("RECOVERED", "0x8002FDA4 loads 33 ZTMPAL.PSH palettes; 0x80030308 loads ZBPAL.PSH");
        trace_.log("ROSTER-PALETTE", "FUN_8002FE58 patches ZSET4 Bkg colors 0..159 from ZTMPAL.PSH and preserves local colors 160..255");
        trace_.log("ROSTER-LAYOUT", "state 0x10 / ZSET4: Bkga-d x=0/128/256/384, ba35=(142,10), frml=(30,15), dynamic team logo=(40,16), team arrows=ZFONT0 0x8D/0x8A at (157/381,66), scroll arrows=0x8B/0x8C at (48,108/168), help=(235,217)");
        trace_.log("ROSTER-ANIM", "FUN_8002FF80 team crossfade=17 ticks; FUN_8002AB88 selected-row neutral-to-gold pulse=20 ticks");
        trace_.log("ROSTER-FIELDS", "recomp descriptor tables: categories=6 displays=56; Select/L2 category, R2/L1 field; six-row repeat=7/5/3/1 ticks");
        trace_.log("RECOVERED", "0x80035260 loads ZFEMOCAP.BIN; original frontend model/art packs are local");
        trace_.log("RECOVERED", "state 0x24 FUN_8005A538 loads Z1PORT.IDX/BIG and Z1COOL.IDX/BIG for View Player");
        trace_.log("PLAYER-LAYOUT", "FUN_8003F7C8 reloads gfx state 0x24 / ZSET8 LIVE: Bkge-h, brta-d/brba-d, ba41=(40,18), team *Z=(296,35), cros=(336,204), o18a=(356,198)");
        trace_.log("PLAYER-ANIM", "FUN_8003186C + FUN_80034A5C: ba41 uses two 128-page GPU pieces with discrete four-corner jumble; scanline wave disabled");
        trace_.log("PLAYER-POSITION", "descriptor case 0x13: roster slots 0..4 use original starting C/PF/SF/SG/PG strings at 0x80024C98..0x80024CC8; bench uses database position");
        trace_.log("COOL-FACT", "view-card help descriptor 0x800B22F0 uses ZFONT control glyph 0x94 play / 0x93 stop; input 0x800 routes FUN_80059F30 -> FUN_80059E14");
        roster_database_.load(options_.asset_root / "database" / "roster.n97db");
        trace_.log("ROSTER-DB", "external private pack version=" +
            std::to_string(roster_database_.version()) + " teams=" +
            std::to_string(roster_database_.teams().size()) + " players=" +
            std::to_string(roster_database_.players().size()));
        trace_.log("ROSTER-INDEX", "FUN_8005FE14 boundary=0x1ED; assigned=" +
            std::to_string(roster_database_.assignedPlayerCount()) + " free-agents=" +
            std::to_string(roster_database_.freeAgentCount()) + "; all references validated");
        trace_.log("RECOVERED", "FUN_80057864 copies DAT_800C0CAC roster slots; FUN_8005770C resolves 29 teams x 15 slots");
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

    void loadPlayerCardSprites(const std::filesystem::path& root) {
        static constexpr const char* tags[] = {
            "Bkge","Bkgf","Bkgg","Bkgh","help",
            "brta","brtb","brtc","brtd","brle","brri",
            "brba","brbb","brbc","brbd",
            "ba41","o18a","o18b","cros","shot","wait",
            "atlZ","bosZ","chaZ","chiZ","cleZ","dalZ","denZ","detZ","golZ","houZ",
            "indZ","lacZ","lalZ","miaZ","milZ","minZ","nwjZ","nwyZ","orlZ","phiZ",
            "phoZ","porZ","sacZ","sanZ","seaZ","torZ","utaZ","vanZ","wasZ"
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

    void loadSelectedPlayerCardAssets() {
        const auto* player = roster_viewer_.selectedPlayer(roster_database_);
        roster_portrait_loaded_ = false;
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
        roster_portrait_ = load_png_image(portrait_path);
        if (roster_portrait_.width != 180 || roster_portrait_.height != 156)
            throw std::runtime_error("invalid Z1PORT View Player portrait dimensions: " +
                                     portrait_path.string());
        roster_portrait_loaded_ = true;

        std::ifstream input(root / "Z1COOL.IDX", std::ios::binary);
        std::vector<std::uint8_t> index((std::istreambuf_iterator<char>(input)), {});
        const auto read_u32 = [&index](std::size_t at) -> std::uint32_t {
            if (at + 4 > index.size()) return 0;
            return static_cast<std::uint32_t>(index[at]) |
                   (static_cast<std::uint32_t>(index[at + 1]) << 8) |
                   (static_cast<std::uint32_t>(index[at + 2]) << 16) |
                   (static_cast<std::uint32_t>(index[at + 3]) << 24);
        };
        const std::uint32_t count = read_u32(0);
        for (std::uint32_t variant = 0; variant < 5; ++variant) {
            const std::uint32_t record = static_cast<std::uint32_t>(player->id) * 5 + variant;
            if (record < count && read_u32(4 + static_cast<std::size_t>(record) * 8) != 0) {
                roster_cool_facts_available_ = true;
                break;
            }
        }
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
            "pho","por","sac","san","sea","tor","uta","van","was"};
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
            "29 runtime-patched ZSET4 team backgrounds Bkga-h loaded from local-only ZTMPAL output");
    }

    void loadMenuCards(const std::filesystem::path& root) {
        std::mt19937 random(static_cast<std::mt19937::result_type>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count() ^
            static_cast<long long>(GetTickCount64())));
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
        const auto index_list = [](const auto& indices) {
            std::string result;
            for (const int index : indices) {
                if (!result.empty()) result += ',';
                result += std::to_string(index);
            }
            return result;
        };
        trace_.log("MENU-CARD", "0x80031A88 loaded 95 ZCARD.BIN images; setup PRNG picks=" +
            index_list(setup_indices) + "; Rosters PRNG picks=" + index_list(roster_indices));
        trace_.log("RECOVERED", "0x80031F48 flags=0x20 replacement: per-screen PRNG with unique index&31 mask; 4 setup plus 8 Rosters 69x63 SHPP composites");
        trace_.log("ROSTER-MENU", "FUN_80057CE4 state=9: 8 choices x 3 runtime objects (normal/selected/ZCARD); native plates at x=49/154/259/364 y=56/122; art offset=(12,23)");
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

        roster_viewer_ = nba97::RosterViewer{};
        roster_viewer_.open(roster_database_);
        elapsed = 100;
        for (int team = 0; team < 3; ++team) {
            roster_viewer_.scanTeam(1, roster_database_, elapsed);
            elapsed += 340;
        }
        roster_viewer_.activate(roster_database_);
        loadSelectedPlayerCardAssets();
        writePpm(nba97::renderRosterViewer(
            roster_viewer_, roster_database_, menu_font_, player_sprites_, 340,
            roster_portrait_loaded_ ? &roster_portrait_ : nullptr,
            roster_cool_facts_available_, &control_font_),
            output / "player_chicago_initial.ppm");
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

        std::ofstream metadata(output / "capture.json", std::ios::trunc);
        if (!metadata) throw std::runtime_error("cannot write View Rosters capture metadata");
        metadata << "{\n"
                 << "  \"schema_version\": 1,\n"
                 << "  \"width\": 512,\n"
                 << "  \"height\": 240,\n"
                 << "  \"team\": \"Chicago Bulls\",\n"
                 << "  \"visible_rows\": 6,\n"
                 << "  \"captures\": [\"team_atlanta_initial.ppm\", "
                    "\"team_chicago_initial.ppm\", \"team_chicago_scrolled.ppm\", "
                    "\"player_chicago_initial.ppm\", "
                    "\"player_chicago_scrolled.ppm\"]\n"
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
            roster_database_.assignedPlayerCount() != 362 || roster_database_.freeAgentCount() != 131 ||
            !roster_database_.player(0) || !roster_database_.team(28))
            throw std::runtime_error("external roster database validation self-test failed");
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
        nba97::RosterViewer roster_viewer_test;
        roster_viewer_test.open(roster_database_);
        const auto view_a = nba97::renderRosterViewer(
            roster_viewer_test, roster_database_, menu_font_, roster_sprites_, 0);
        if (view_a.width != 512 || view_a.height != 240 ||
            roster_database_.version() != 3 ||
            roster_database_.teams()[0].roster.size() != 15 ||
            roster_viewer_test.category() != 2 || roster_viewer_test.displayIndex() != 32)
            throw std::runtime_error("View Rosters fixed 15-slot state 0x10 setup failed");
        const auto* roster_sample = roster_viewer_test.selectedPlayer(roster_database_);
        if (!roster_sample ||
            roster_database_.playerAttribute(*roster_sample, 7).empty() ||
            !roster_viewer_test.cycleDisplay(1) || roster_viewer_test.displayIndex() != 33)
            throw std::runtime_error("View Rosters v3 descriptor setup failed");
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
        const auto* wrap_team = wrap_test.selectedTeam(roster_database_);
        std::size_t wrap_count = 0;
        while (wrap_team && wrap_count < wrap_team->roster.size() &&
               roster_database_.player(wrap_team->roster[wrap_count]))
            ++wrap_count;
        if (wrap_count < 2 || !wrap_test.move(-1, 0, roster_database_) ||
            wrap_test.playerIndex() != wrap_count - 1 ||
            !wrap_test.move(1, 0, roster_database_) || wrap_test.playerIndex() != 0)
            throw std::runtime_error("View Player FUN_80059928 wrapped cycling self-test failed");
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
        trace_.log("SELF-TEST", "PASS: boot, menus/settings, View Rosters state 0x10, View Player 24-row scroll and FUN_80059928 wrap, versioned profiles, external roster database, all 2,524 frontend-music blocks, and 11 native semantic checkpoints validated");
        return 0;
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
            {0x80059610u, "Rosters_ScanTeams"},
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
        switch (message) {
        case WM_KEYDOWN:
            // WM_KEYDOWN repeats while a physical/D-pad key is held. The PS1
            // frontend advances once per press, so ignore repeat messages
            // (bit 30 reports that the key was already down).
            if ((static_cast<std::uintptr_t>(lparam) & (1u << 30)) != 0) return 0;
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
                frontend_page_ != nba97::FrontendPage::ViewRosters)
                beginFrontendTransition(nba97::FrontendPage::GameSetup, "back input");
            else if (wparam == VK_ESCAPE && flow_.screen() == nba97::BootScreen::MainMenu &&
                     frontend_page_ == nba97::FrontendPage::ProfileSetup)
                handleMenuKey(wparam);
            else if (wparam == VK_ESCAPE && flow_.screen() == nba97::BootScreen::MainMenu &&
                     frontend_page_ == nba97::FrontendPage::ViewRosters)
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
            menu_elapsed_ms_ += now - previous_tick_;
            updateRosterHeldInput();
            rebuildMenuFrame();
            if (frontend_transition_active_) {
                const auto elapsed = now - frontend_transition_tick_;
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
        InvalidateRect(window_, nullptr, FALSE);
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
            const auto& audio = frontend_music_.info();
            trace_.log("MUSIC-DECODER", frontend_music_.decoderName());
            trace_.log("MUSIC-STREAM", "ZTMENU1.CNK blocks=" + std::to_string(audio.data_blocks) +
                " rate=" + std::to_string(audio.sample_rate) + "Hz channels=" +
                std::to_string(audio.channels) + " samples=" + std::to_string(audio.sample_count));
            trace_.log("MUSIC-PLAY", "FUN_8002F258 recovered volume=" +
                std::to_string(recovered_volume) + "/127; looped in-process");
        } catch (const std::exception& error) {
            trace_.log("MUSIC-ERROR", error.what());
        }
        InvalidateRect(window_, nullptr, FALSE);
    }

    void handleMenuKey(WPARAM key) {
        if (frontend_transition_active_) return;
        if (frontend_page_ == nba97::FrontendPage::ProfileSetup) {
            handleProfileKey(key);
            return;
        }
        if (frontend_page_ == nba97::FrontendPage::ViewRosters) {
            handleRosterViewKey(key);
            return;
        }
        if (frontend_page_ != nba97::FrontendPage::GameSetup &&
            frontend_page_ != nba97::FrontendPage::Rules &&
            frontend_page_ != nba97::FrontendPage::Options) {
            bool changed = false;
            if (key == VK_LEFT) changed = bottom_menu_.move(-1, 0);
            else if (key == VK_RIGHT) changed = bottom_menu_.move(1, 0);
            else if (key == VK_UP) changed = bottom_menu_.move(0, -1);
            else if (key == VK_DOWN) changed = bottom_menu_.move(0, 1);
            else if (key == VK_BACK) {
                beginFrontendTransition(nba97::FrontendPage::GameSetup, "back input");
                return;
            } else if (key == VK_RETURN || key == VK_SPACE) {
                activateRecoveredBottomSelection();
                return;
            }
            if (changed) {
                trace_.log("SUBMENU-FOCUS", bottom_menu_.selectedLabel());
                rebuildMenuFrame();
                InvalidateRect(window_, nullptr, FALSE);
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
        else if (key == VK_RETURN || key == VK_SPACE) {
            activateMenuSelection();
            return;
        }
        if (changed) {
            trace_.log("MENU-HOVER", std::string(menu_.row() == nba97::MenuRow::GameOptions
                                      ? "option=" : "button=") + menu_.selectedLabel());
            rebuildMenuFrame();
            InvalidateRect(window_, nullptr, FALSE);
        }
    }

    void activateRecoveredBottomSelection() {
        if (frontend_page_ == nba97::FrontendPage::Rosters && bottom_menu_.selected() == 4) {
            beginFrontendTransition(nba97::FrontendPage::ViewRosters,
                "view rosters selected; FUN_80057CE4 return=6 pushes state 0x10 FUN_800592C4");
            return;
        }
        trace_.log("MENU-BLOCK", std::string(bottom_menu_.selectedLabel()) +
                                 " child flow not yet decompiled");
    }

    void logRosterViewFocus(const char* reason) {
        const auto* team = roster_viewer_.selectedTeam(roster_database_);
        const auto* player = roster_viewer_.selectedPlayer(roster_database_);
        trace_.log(roster_viewer_.mode() == nba97::RosterViewMode::TeamRoster
                       ? "ROSTER-FOCUS" : "PLAYER-CARD",
            std::string(reason) + "; team=" +
            (team ? team->city + " " + team->nickname : "<none>") +
            " player=" + (player ? player->displayName() : "<none>") +
            " category=" + std::to_string(roster_viewer_.category()) +
            " display=" + std::to_string(roster_viewer_.displayIndex()) +
            " list-window=" + std::to_string(roster_viewer_.firstVisiblePlayer()) + ".." +
            std::to_string(roster_viewer_.firstVisiblePlayer() + 5) +
            " stat-window=" + std::to_string(roster_viewer_.firstVisiblePlayerStat()) + ".." +
            std::to_string(roster_viewer_.firstVisiblePlayerStat() + 5) +
            (player ? " id=" + std::to_string(player->id) + " number=" +
                std::to_string(player->jersey_number) + " position=" +
                nba97::positionName(player->position) : ""));
    }

    void playRosterCursorSound(int direction) {
        try {
            const auto root = options_.asset_root / "menu";
            const std::uint32_t sound_id = direction < 0 ? 3u : 4u;
            const auto info = cursor_audio_.playCursorSound(
                root / "ZCURSOR.VH", root / "ZCURSOR.VB", sound_id);
            const auto effective_percent =
                (info.program_volume * info.tone_volume * info.playback_volume * 100u) /
                (127u * 127u * 127u);
            stat_flash_direction_ = direction;
            stat_flash_until_ms_ = menu_elapsed_ms_ + 340;
            trace_.log("PLAYER-STAT-FLASH", "FUN_8002AB88 gold transition=20 ticks; "
                "FUN_8002F124 sound=" + std::to_string(sound_id) +
                " ZCURSOR.VH/VB rate=" + std::to_string(info.sample_rate) +
                " samples=" + std::to_string(info.sample_count) +
                " gain=" + std::to_string(info.program_volume) + "/127*" +
                std::to_string(info.tone_volume) + "/127*" +
                std::to_string(info.playback_volume) + "/127 (" +
                std::to_string(effective_percent) + "%)");
        } catch (const std::exception& error) {
            trace_.log("AUDIO-ERROR", std::string("ZCURSOR decode/play failed: ") + error.what());
        }
    }

    void playSelectedCoolFact() {
        const auto* player = roster_viewer_.selectedPlayer(roster_database_);
        if (!player || !roster_cool_facts_available_) {
            trace_.log("COOL-FACT", "input 0x800 -> original no-cool-facts modal DAT_800AFE06");
            return;
        }
        try {
            const auto root = options_.asset_root / "menu";
            const auto info = cool_fact_audio_.playCoolFact(
                root / "Z1COOL.IDX", root / "Z1COOL.BIG", player->id);
            trace_.log("COOL-FACT-AUDIO", "FUN_80059E14 -> FUN_80031630 -> FUN_80031770; "
                "player=" + player->displayName() + " record=" +
                std::to_string(info.record) + " " + info.source +
                " codec=PSX-ADPCM mono rate=" + std::to_string(info.sample_rate) +
                " samples=" + std::to_string(info.sample_count) + " duration-ms=" +
                std::to_string(info.sample_count * 1000u / info.sample_rate));
        } catch (const std::exception& error) {
            trace_.log("AUDIO-ERROR", std::string("Cool Fact decode/play failed: ") + error.what());
        }
    }

    void handleRosterViewKey(WPARAM key) {
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
            trace_.log("PLAYER-CARD", "state 0x24 popped -> state 0x10; team/row/top preserved");
            rebuildMenuFrame();
            InvalidateRect(window_, nullptr, FALSE);
            return;
        }
        bool changed = false;
        const std::size_t previous_team = roster_viewer_.teamIndex();
        const std::size_t previous_player = roster_viewer_.playerIndex();
        const std::size_t previous_stat = roster_viewer_.firstVisiblePlayerStat();
        if (key == VK_LEFT)
            changed = roster_viewer_.move(-1, 0, roster_database_, menu_elapsed_ms_);
        else if (key == VK_RIGHT)
            changed = roster_viewer_.move(1, 0, roster_database_, menu_elapsed_ms_);
        else if (key == VK_UP)
            changed = roster_viewer_.move(0, -1, roster_database_, menu_elapsed_ms_);
        else if (key == VK_DOWN)
            changed = roster_viewer_.move(0, 1, roster_database_, menu_elapsed_ms_);
        else if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard &&
                 (key == VK_OEM_4 || key == 'J')) {
            changed = roster_viewer_.scanTeam(-1, roster_database_, menu_elapsed_ms_);
            trace_.log("PLAYER-TEAM-SCAN", "L1: previous team through FUN_80059610");
        } else if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard &&
                   (key == VK_OEM_6 || key == 'K')) {
            changed = roster_viewer_.scanTeam(1, roster_database_, menu_elapsed_ms_);
            trace_.log("PLAYER-TEAM-SCAN", "R1: next team through FUN_80059610");
        }
        else if (key == 'Q') {
            changed = roster_viewer_.cycleCategory(-1);
            trace_.log("PLAYER-STAT-LAYER", "L2/internal 0x1000: previous recovered descriptor layer");
        } else if (key == 'E') {
            changed = roster_viewer_.cycleCategory(1);
            trace_.log("PLAYER-STAT-LAYER", "R2/internal 0x2000: next recovered descriptor layer");
        } else if (key == 'Z') {
            changed = roster_viewer_.cycleDisplay(-1);
            trace_.log("ROSTER-DISPLAY", "R2/internal 0x0200: previous field");
        } else if (key == 'C') {
            changed = roster_viewer_.cycleDisplay(1);
            trace_.log("ROSTER-DISPLAY", "L1/internal 0x0400: next field");
        }
        else if (key == VK_RETURN || key == VK_SPACE) {
            if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard) {
                playSelectedCoolFact();
                rebuildMenuFrame();
                InvalidateRect(window_, nullptr, FALSE);
                return;
            }
            if (roster_viewer_.selectedPlayer(roster_database_)) {
                roster_viewer_.activate(roster_database_);
                loadSelectedPlayerCardAssets();
                trace_.log("ROSTER-VIEW",
                    "internal 0x10 -> result=2 -> nested state 0x24 FUN_8005A538");
                rebuildMenuFrame();
                InvalidateRect(window_, nullptr, FALSE);
                return;
            }
            trace_.log("ROSTER-MESSAGE",
                key == VK_RETURN ? "view player: blank roster slot" :
                                   "compare players: blank roster slot");
        } else if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard && key == 'S') {
            cool_fact_audio_.stop();
            trace_.log("COOL-FACT-STOP", "Square/internal 0x10 -> FUN_80059DB8(1); speech stopped");
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
            if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard &&
                (roster_viewer_.playerIndex() != previous_player ||
                 roster_viewer_.teamIndex() != previous_team)) {
                cool_fact_audio_.stop();
                loadSelectedPlayerCardAssets();
            }
            if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard &&
                roster_viewer_.firstVisiblePlayerStat() != previous_stat)
                playRosterCursorSound(roster_viewer_.firstVisiblePlayerStat() > previous_stat ? 1 : -1);
            if (roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard &&
                roster_viewer_.playerIndex() != previous_player)
                trace_.log("PLAYER-WRAP", "FUN_80059928 player slot " +
                    std::to_string(previous_player) + " -> " +
                    std::to_string(roster_viewer_.playerIndex()) +
                    " with bidirectional roster wrap");
            if (roster_viewer_.teamIndex() != previous_team) {
                const auto* from = previous_team < roster_database_.teams().size()
                    ? &roster_database_.teams()[previous_team] : nullptr;
                const auto* to = roster_viewer_.selectedTeam(roster_database_);
                trace_.log("ROSTER-PALETTE", "FUN_8003F7B0 target " +
                    (from ? from->city + " " + from->nickname : "<none>") + " -> " +
                    (to ? to->city + " " + to->nickname : "<none>") +
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
        if (frontend_page_ == nba97::FrontendPage::GameSetup)
            menu_frame_ = makeFrame(nba97::renderGameSetupMenu(
                menu_, title_source_, menu_font_, menu_sprites_, menu_cards_, menu_elapsed_ms_));
        else if (frontend_page_ == nba97::FrontendPage::ProfileSetup)
            menu_frame_ = makeFrame(nba97::renderUserProfileSetup(
                profile_menu_, profile_store_, menu_font_, menu_sprites_, menu_elapsed_ms_));
        else if (frontend_page_ == nba97::FrontendPage::ViewRosters)
            menu_frame_ = makeFrame(nba97::renderRosterViewer(
                roster_viewer_, roster_database_, menu_font_,
                roster_viewer_.mode() == nba97::RosterViewMode::PlayerCard
                    ? player_sprites_ : roster_sprites_,
                menu_elapsed_ms_,
                roster_portrait_loaded_ ? &roster_portrait_ : nullptr,
                roster_cool_facts_available_, &control_font_,
                menu_elapsed_ms_ < stat_flash_until_ms_ ? stat_flash_direction_ : 0,
                cool_fact_audio_.isPlaying()));
        else if (frontend_page_ == nba97::FrontendPage::Rules ||
                 frontend_page_ == nba97::FrontendPage::Options)
            menu_frame_ = makeFrame(nba97::renderSettingsMenu(
                settings_menu_, settings_, menu_font_, menu_sprites_, menu_elapsed_ms_));
        else
            menu_frame_ = makeFrame(nba97::renderRecoveredBottomMenu(
                bottom_menu_, menu_font_, menu_sprites_, roster_sprites_, users_sprites_,
                roster_menu_cards_, menu_elapsed_ms_));
    }

    static std::string frontendPageName(nba97::FrontendPage page) {
        if (page == nba97::FrontendPage::Rules) return "Rules";
        if (page == nba97::FrontendPage::ProfileSetup) return "User Setup";
        if (page == nba97::FrontendPage::Options) return "Options";
        if (page == nba97::FrontendPage::Rosters) return "Rosters";
        if (page == nba97::FrontendPage::ViewRosters) return "View Rosters";
        if (page == nba97::FrontendPage::Users) return "Users";
        if (page == nba97::FrontendPage::Card) return "Memory Card";
        return "Game Setup";
    }

    void activateMenuSelection() {
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
        beginFrontendTransition(nba97::FrontendPage::ProfileSetup,
            std::string(menu_.selectedLabel()) +
            " accepted; FUN_80037010 recovered user assignment/profile stage");
    }

    void beginFrontendTransition(nba97::FrontendPage target, const std::string& reason) {
        if (frontend_transition_active_ || target == frontend_page_) return;
        transition_source_ = menu_frame_;
        const auto previous_page = frontend_page_;
        frontend_page_ = target;
        if (target == nba97::FrontendPage::ProfileSetup)
            profile_menu_.open(profile_store_.profiles().size());
        else if (target == nba97::FrontendPage::ViewRosters) {
            roster_viewer_.open(roster_database_);
            logRosterViewFocus("FUN_800592C4 restored selection");
        }
        else if (target == nba97::FrontendPage::Rules || target == nba97::FrontendPage::Options)
            settings_menu_.open(target);
        else if (target != nba97::FrontendPage::GameSetup) {
            bottom_menu_.open(target);
            if (target == nba97::FrontendPage::Rosters &&
                previous_page == nba97::FrontendPage::ViewRosters)
                bottom_menu_.setSelected(4);
        }
        rebuildMenuFrame();
        frontend_transition_tick_ = GetTickCount();
        frontend_transition_active_ = true;
        transition_frame_ = transition_source_;
        trace_.log("TRANSITION", reason + "; recovered FE state=" +
            std::to_string(target == nba97::FrontendPage::ProfileSetup ? 0x37010 :
                           target == nba97::FrontendPage::ViewRosters ? 0x10 :
                           target == nba97::FrontendPage::Rules ? 1 :
                           target == nba97::FrontendPage::Options ? 2 :
                           target == nba97::FrontendPage::Rosters ? 9 :
                           target == nba97::FrontendPage::Users ? 19 :
                           target == nba97::FrontendPage::Card ? 11 : 0) +
            " -> " + frontendPageName(target) + " (original recovered pack/layout)");
        InvalidateRect(window_, nullptr, FALSE);
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
                std::to_string(recovered_volume) + "/127 applied live");
        }
        rebuildMenuFrame();
        InvalidateRect(window_, nullptr, FALSE);
    }

    Options options_;
    Trace trace_;
    ComApartment com_apartment_;
    nba97::BootFlow flow_;
    nba97::IntroPlayer intro_player_;
    nba97::FrontendMusicPlayer frontend_music_;
    nba97::RecoveredAudioPlayer cursor_audio_;
    nba97::RecoveredAudioPlayer cool_fact_audio_;
    nba97::MainMenu menu_;
    nba97::FrontendSettings settings_;
    nba97::SettingsMenu settings_menu_;
    nba97::RecoveredBottomMenu bottom_menu_;
    nba97::RosterViewer roster_viewer_;
    nba97::RosterDatabase roster_database_;
    nba97::UserProfileStore profile_store_;
    nba97::UserProfileMenu profile_menu_;
    nba97::FrontendPage frontend_page_ = nba97::FrontendPage::GameSetup;
    nba97::PshFont menu_font_;
    nba97::PshFont control_font_;
    nba97::MenuSpritePack menu_sprites_;
    nba97::MenuSpritePack roster_sprites_;
    nba97::MenuSpritePack player_sprites_;
    nba97::MenuSpritePack users_sprites_;
    nba97::MenuCardPack menu_cards_;
    nba97::RosterCardPack roster_menu_cards_;
    PshImage roster_portrait_;
    bool roster_portrait_loaded_ = false;
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
