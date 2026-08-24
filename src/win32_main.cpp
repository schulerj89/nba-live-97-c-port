#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <objbase.h>

#include "boot_flow.hpp"
#include "frontend_music.hpp"
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
        validateMenuAsset(menu_root / "ZCARD.BIN", 474240);
        validateMenuAsset(menu_root / "ZTMENU1.CNK", 8522396);
        validateMenuAsset(menu_root / "ZSET1.PSP", 342448);
        validateMenuAsset(menu_root / "ZSET4.PSP", 332084);
        validateMenuAsset(menu_root / "ZSET7.PSP", 323444);
        loadMenuSprites(menu_root / "ZSET1-decoded");
        loadRecoveredBottomSprites(menu_root / "ZSET4-decoded", roster_sprites_, true);
        loadRecoveredBottomSprites(menu_root / "ZSET7-decoded", users_sprites_, false);
        loadMenuCards(menu_root / "ZCARD-decoded");
        trace_.log("RECOVERED", "0x8002F258 selects ZTMENU1.CNK frontend audio");
        trace_.log("RECOVERED", "0x8002FDA4 loads 33 ZTMPAL.PSH palettes; 0x80030308 loads ZBPAL.PSH");
        trace_.log("RECOVERED", "0x80035260 loads ZFEMOCAP.BIN; original frontend model/art packs are local");
        roster_database_.load(options_.asset_root / "database" / "roster.n97db");
        trace_.log("ROSTER-DB", "external private pack version=" +
            std::to_string(roster_database_.version()) + " teams=" +
            std::to_string(roster_database_.teams().size()) + " players=" +
            std::to_string(roster_database_.players().size()));
        trace_.log("ROSTER-INDEX", "FUN_8005FE14 boundary=0x1ED; assigned=" +
            std::to_string(roster_database_.assignedPlayerCount()) + " free-agents=" +
            std::to_string(roster_database_.freeAgentCount()) + "; all references validated");
        trace_.log("RECOVERED", "FUN_8005770C resolves 15 slots/team; 0x80023AB0 stride=0x68 across 29 teams");
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
            "Bkge","Bkgf","Bkgg","Bkgh","help",
            "brte","brtf","brtg","brth","brle","brri",
            "brbe","brbf","brbg","brbh","XXL1","XXR2"
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

    void validateMenuAsset(const std::filesystem::path& path,
                           std::uintmax_t expected_size) {
        if (!std::filesystem::exists(path) || std::filesystem::file_size(path) != expected_size)
            throw std::runtime_error("missing or invalid private menu asset: " + path.string());
        trace_.log("MENU-ASSET", path.string() + " bytes=" + std::to_string(expected_size));
    }

    void loadMenuCards(const std::filesystem::path& root) {
        for (std::size_t index = 0; index < menu_cards_.size(); ++index) {
            wchar_t name[32]{};
            swprintf_s(name, L"card_%02zu.png", index);
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
            menu_cards_[index] = std::move(image);
        }
        trace_.log("MENU-CARD", "0x80031A88 loaded ZCARD.BIN; deterministic private cards=0,1,2,3");
        trace_.log("RECOVERED", "0x80031F48 maps four flags=0x20 blk1 slots to unique 69x63 SHPP portraits");
    }

    int runSelfTest() {
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
            roster_database_.assignedPlayerCount() != 348 || roster_database_.freeAgentCount() != 145 ||
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
            recovered_menu, menu_font_, menu_sprites_, roster_sprites_, users_sprites_, 0);
        recovered_menu.move(1, 0);
        const auto roster_b = nba97::renderRecoveredBottomMenu(
            recovered_menu, menu_font_, menu_sprites_, roster_sprites_, users_sprites_, 0);
        if (roster_a.rgba == roster_b.rgba || recovered_menu.count() != 8)
            throw std::runtime_error("Rosters original-card navigation self-test failed");
        nba97::RosterViewer roster_viewer_test;
        roster_viewer_test.open(roster_database_);
        const auto view_a = nba97::renderRosterViewer(
            roster_viewer_test, roster_database_, menu_font_, roster_sprites_, 0);
        if (!roster_viewer_test.move(1, 0, roster_database_) ||
            roster_viewer_test.teamIndex() != 1 ||
            !roster_viewer_test.move(0, 1, roster_database_) ||
            roster_viewer_test.playerIndex() != 1)
            throw std::runtime_error("View Rosters team/player navigation self-test failed");
        roster_viewer_test.activate(roster_database_);
        const auto view_b = nba97::renderRosterViewer(
            roster_viewer_test, roster_database_, menu_font_, roster_sprites_, 137);
        if (roster_viewer_test.mode() != nba97::RosterViewMode::PlayerCard ||
            view_a.rgba == view_b.rgba || !roster_viewer_test.back())
            throw std::runtime_error("View Rosters player-card self-test failed");
        roster_viewer_test.activate(roster_database_);
        if (!roster_viewer_test.move(0, 1, roster_database_) ||
            roster_viewer_test.detailPage() != 1 ||
            nba97::renderRosterViewer(roster_viewer_test, roster_database_, menu_font_,
                                      roster_sprites_, 0).rgba == view_b.rgba)
            throw std::runtime_error("View Rosters all-ratings page self-test failed");
        recovered_menu.open(nba97::FrontendPage::Card);
        const auto card_a = nba97::renderRecoveredBottomMenu(
            recovered_menu, menu_font_, menu_sprites_, roster_sprites_, users_sprites_, 0);
        recovered_menu.move(1, 0);
        const auto card_b = nba97::renderRecoveredBottomMenu(
            recovered_menu, menu_font_, menu_sprites_, roster_sprites_, users_sprites_, 0);
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
        trace_.log("SELF-TEST", "PASS: boot, menus/settings, View Rosters/player cards, versioned profiles, external roster database, and all 2,524 frontend-music blocks validated");
        return 0;
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
                    roster_viewer_.activate(roster_database_);
                    logRosterViewFocus("mouse activation");
                    rebuildMenuFrame();
                    InvalidateRect(window_, nullptr, FALSE);
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
            const Frame& frame = currentFrame();
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = frame.width;
            info.bmiHeader.biHeight = -frame.height;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            SetStretchBltMode(dc, COLORONCOLOR);
            StretchDIBits(dc, 0, 0, client.right, client.bottom, 0, 0,
                          frame.width, frame.height, frame.bgra.data(), &info,
                          DIB_RGB_COLORS, SRCCOPY);
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
            const auto recovered_volume = static_cast<std::uint8_t>((std::min)(127,
                static_cast<int>(settings_.option(1)) * 15));
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
            (player ? " id=" + std::to_string(player->id) + " number=" +
                std::to_string(player->jersey_number) + " position=" +
                nba97::positionName(player->position) : ""));
    }

    void handleRosterViewKey(WPARAM key) {
        bool changed = false;
        if (key == VK_LEFT) changed = roster_viewer_.move(-1, 0, roster_database_);
        else if (key == VK_RIGHT) changed = roster_viewer_.move(1, 0, roster_database_);
        else if (key == VK_UP) changed = roster_viewer_.move(0, -1, roster_database_);
        else if (key == VK_DOWN) changed = roster_viewer_.move(0, 1, roster_database_);
        else if (key == VK_RETURN || key == VK_SPACE) {
            roster_viewer_.activate(roster_database_);
            changed = true;
        } else if (key == VK_ESCAPE || key == VK_BACK) {
            if (roster_viewer_.back()) {
                trace_.log("ROSTER-BACK", "player card -> team roster");
                changed = true;
            } else {
                beginFrontendTransition(nba97::FrontendPage::Rosters,
                                        "View Rosters back input");
                return;
            }
        }
        if (changed) {
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
        if (client.right <= 0 || client.bottom <= 0) return;
        const int psx_x = client_x * kPsxWidth / client.right;
        const int psx_y = client_y * kPsxHeight / client.bottom;
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

    void rebuildMenuFrame() {
        if (frontend_page_ == nba97::FrontendPage::GameSetup)
            menu_frame_ = makeFrame(nba97::renderGameSetupMenu(
                menu_, title_source_, menu_font_, menu_sprites_, menu_cards_, menu_elapsed_ms_));
        else if (frontend_page_ == nba97::FrontendPage::ProfileSetup)
            menu_frame_ = makeFrame(nba97::renderUserProfileSetup(
                profile_menu_, profile_store_, menu_font_, menu_sprites_, menu_elapsed_ms_));
        else if (frontend_page_ == nba97::FrontendPage::ViewRosters)
            menu_frame_ = makeFrame(nba97::renderRosterViewer(
                roster_viewer_, roster_database_, menu_font_, roster_sprites_, menu_elapsed_ms_));
        else if (frontend_page_ == nba97::FrontendPage::Rules ||
                 frontend_page_ == nba97::FrontendPage::Options)
            menu_frame_ = makeFrame(nba97::renderSettingsMenu(
                settings_menu_, settings_, menu_font_, menu_sprites_, menu_elapsed_ms_));
        else
            menu_frame_ = makeFrame(nba97::renderRecoveredBottomMenu(
                bottom_menu_, menu_font_, menu_sprites_, roster_sprites_, users_sprites_,
                menu_elapsed_ms_));
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
            const auto recovered_volume = static_cast<std::uint8_t>((std::min)(127,
                static_cast<int>(settings_.option(1)) * 15));
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
    nba97::MenuSpritePack menu_sprites_;
    nba97::MenuSpritePack roster_sprites_;
    nba97::MenuSpritePack users_sprites_;
    nba97::MenuCardPack menu_cards_;
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
