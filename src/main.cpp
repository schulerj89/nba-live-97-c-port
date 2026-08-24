#include "boot_flow.hpp"
#include "psh_image.hpp"
#include "psh_font.hpp"

#include <SDL.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
using nba97::BootFlow;
using nba97::BootScreen;
constexpr int kPsxWidth = 512;
constexpr int kPsxHeight = 240;

struct Options {
    std::filesystem::path asset_root = ".local/assetpacks";
    std::filesystem::path trace_path = ".local/logs/boot_decomp_trace.log";
    std::uint32_t transition_ms = 3000;
    bool self_test = false;
    std::filesystem::path dump_dir;
};

class Trace {
public:
    explicit Trace(const std::filesystem::path& path) {
        std::filesystem::create_directories(path.parent_path());
        file_ = std::fopen(path.string().c_str(), "w");
    }
    ~Trace() { if (file_) std::fclose(file_); }
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

Options parse_options(int argc, char** argv) {
    Options options;
    if (const char* root = std::getenv("NBA97_ASSET_ROOT")) options.asset_root = root;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--self-test") options.self_test = true;
        else if (arg == "--asset-root" && i + 1 < argc) options.asset_root = argv[++i];
        else if (arg == "--transition-ms" && i + 1 < argc)
            options.transition_ms = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        else if (arg == "--trace" && i + 1 < argc) options.trace_path = argv[++i];
        else if (arg == "--dump-decoded" && i + 1 < argc) options.dump_dir = argv[++i];
    }
    return options;
}

void dump_ppm(const PshImage& image, const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot create " + path.string());
    out << "P6\n" << image.width << ' ' << image.height << "\n255\n";
    for (std::size_t i = 0; i < image.rgba.size(); i += 4) {
        const char rgb[] = {static_cast<char>(image.rgba[i]),
                            static_cast<char>(image.rgba[i + 1]),
                            static_cast<char>(image.rgba[i + 2])};
        out.write(rgb, 3);
    }
}

SDL_Texture* make_texture(SDL_Renderer* renderer, const PshImage& image) {
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STATIC,
                                             image.width, image.height);
    if (!texture) return nullptr;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    if (SDL_UpdateTexture(texture, nullptr, image.rgba.data(), image.width * 4) != 0) {
        SDL_DestroyTexture(texture);
        return nullptr;
    }
    return texture;
}

void validate_fullscreen(const PshImage& image, const char* name) {
    if (image.width != kPsxWidth || image.height != kPsxHeight) {
        throw std::runtime_error(std::string(name) + " must be 512x240");
    }
}
} // namespace

int main(int argc, char** argv) {
    const Options options = parse_options(argc, argv);
    Trace trace(options.trace_path);
    try {
        trace.log("BOOT", "PS-X entry 0x801E3508 (Ghidra + recomp agreement)");
        trace.log("RECOVERED", "0x801E1A68 loads cdrom:ZLOADSCR.PSH -> 0x80170000");
        trace.log("RECOVERED", "0x801E1A68 loads cdrom:ZLOADING.PSH -> 0x80013800");
        trace.log("OVERLAY", "FEONLY entry 0x8007B79C");
        trace.log("RECOVERED", "0x80035984 calls legal 0x80036684 before title 0x8002EEF4");
        trace.log("RECOVERED", "0x80036684 loads ZLEGAL.PSH via 0x80028BAC");
        trace.log("RECOVERED", "0x8002D768 loads ZCPYRT97.PSH via 0x80028BAC");

        const PshImage load_screen = load_psh(options.asset_root / "boot" / "ZLOADSCR.PSH");
        const PshImage loading_strip = load_psh(options.asset_root / "boot" / "ZLOADING.PSH");
        const PshImage legal_screen = load_psh(options.asset_root / "frontend" / "ZLEGAL.PSH");
        PshImage title_screen = load_psh(options.asset_root / "frontend" / "ZCPYRT97.PSH");
        const nba97::PshFont title_font = nba97::load_psh_font(
            options.asset_root / "fonts" / "ZFONT0.PSH", 10, 1);
        nba97::draw_psh_text_centered(title_screen, title_font, "press start", 0x100, 0x1e);
        validate_fullscreen(load_screen, "ZLOADSCR.PSH");
        validate_fullscreen(legal_screen, "ZLEGAL.PSH");
        validate_fullscreen(title_screen, "ZCPYRT97.PSH");
        trace.log("ASSET", describe_psh(load_screen));
        trace.log("ASSET", describe_psh(loading_strip));
        trace.log("ASSET", describe_psh(legal_screen));
        trace.log("ASSET", describe_psh(title_screen));
        if (!options.dump_dir.empty()) {
            dump_ppm(load_screen, options.dump_dir / "ZLOADSCR.ppm");
            dump_ppm(loading_strip, options.dump_dir / "ZLOADING.ppm");
            dump_ppm(legal_screen, options.dump_dir / "ZLEGAL.ppm");
            dump_ppm(title_screen, options.dump_dir / "ZCPYRT97.ppm");
            trace.log("DUMP", "decoded QA frames written under " + options.dump_dir.string());
        }

        if (options.self_test) {
            BootFlow flow;
            flow.reset();
            if (!flow.update(options.transition_ms, options.transition_ms) ||
                flow.screen() != BootScreen::LegalScreen) {
                throw std::runtime_error("load -> legal transition did not complete");
            }
            trace.log("TRANSITION", "load screen -> 0x80036684 ZLEGAL.PSH");
            if (!flow.update(options.transition_ms, options.transition_ms) ||
                flow.screen() != BootScreen::IntroVideo) {
                throw std::runtime_error("legal -> intro-video transition did not complete");
            }
            (void)flow.completeIntro();
            trace.log("TRANSITION", "0x80035984 -> 0x8002EEF4 -> ZCPYRT97.PSH");
            trace.log("SELF-TEST", "PASS: original PSH bytes decoded; no BMP/capture input");
            return 0;
        }

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
            throw std::runtime_error(std::string("SDL_Init: ") + SDL_GetError());
        SDL_Window* window = SDL_CreateWindow("NBA Live 97 - decompiled boot slice",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 480, SDL_WINDOW_SHOWN);
        SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : nullptr;
        if (!renderer && window) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!window || !renderer) throw std::runtime_error(std::string("SDL: ") + SDL_GetError());

        SDL_Texture* load_texture = make_texture(renderer, load_screen);
        SDL_Texture* strip_texture = make_texture(renderer, loading_strip);
        SDL_Texture* legal_texture = make_texture(renderer, legal_screen);
        SDL_Texture* title_texture = make_texture(renderer, title_screen);
        if (!load_texture || !strip_texture || !legal_texture || !title_texture)
            throw std::runtime_error(std::string("texture creation: ") + SDL_GetError());

        BootFlow flow;
        flow.reset();
        trace.log("DISPLAY", "original ZLOADSCR.PSH visible; SPACE advances, ESC exits");
        bool running = true;
        std::uint32_t previous = SDL_GetTicks();
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN &&
                    event.key.keysym.sym == SDLK_ESCAPE)) running = false;
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE)
                    flow.requestAdvance(options.transition_ms);
            }
            const std::uint32_t now = SDL_GetTicks();
            if (flow.update(now - previous, options.transition_ms)) {
                if (flow.screen() == BootScreen::LegalScreen)
                    trace.log("TRANSITION", "FEONLY 0x80036684 -> original ZLEGAL.PSH");
                else if (flow.screen() == BootScreen::IntroVideo) {
                    trace.log("MOVIE", "Z0ZTITLE.XA playback is implemented by native Win32 target");
                    (void)flow.completeIntro();
                } else
                    trace.log("TRANSITION", "0x80035984 -> title 0x8002EEF4 -> ZCPYRT97.PSH");
            }
            previous = now;

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            if (flow.screen() == BootScreen::LoadScreen) {
                SDL_RenderCopy(renderer, load_texture, nullptr, nullptr);
                // The recovered boot routine draws ZLOADSCR before it loads the
                // separate strip. FEONLY retains the strip for later load states;
                // it is deliberately not duplicated over this already-complete frame.
            } else if (flow.screen() == BootScreen::LegalScreen) {
                SDL_RenderCopy(renderer, legal_texture, nullptr, nullptr);
            } else {
                SDL_RenderCopy(renderer, title_texture, nullptr, nullptr);
            }
            SDL_RenderPresent(renderer);
        }
        SDL_DestroyTexture(title_texture);
        SDL_DestroyTexture(legal_texture);
        SDL_DestroyTexture(strip_texture);
        SDL_DestroyTexture(load_texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    } catch (const std::exception& error) {
        trace.log("ERROR", error.what());
        std::fprintf(stderr, "\nRun scripts/extract_assetpacks.ps1 with your private disc image.\n");
        SDL_Quit();
        return 2;
    }
}
