#pragma once
#include "psh_font.hpp"
#include "recovered/frontend_help.h"

namespace nba97 {
struct FrontendHelpLine { bool centered; std::uint8_t offset; std::string encoded; std::uint8_t extra_before = 0; };
struct FrontendHelpDescriptor {
    std::uint8_t state, index;
    std::uint32_t address;
    Nba97HelpRect rect;
    std::vector<FrontendHelpLine> lines;
    std::uint8_t style = 0; // 0=Help, 1=no-choice warning (40A1C).
    std::uint8_t text_top = 0; // zero uses source style default; choices use10.
};
// Bounded private descriptor data, shared by parent and child Help. No copied
// game strings, invented icons, keyboard captions, or embedded screenshots.
class FrontendHelpPack {
public:
    explicit FrontendHelpPack(const std::filesystem::path& file);
    explicit FrontendHelpPack(const std::vector<std::uint8_t>& bytes);
    const FrontendHelpDescriptor& descriptor(std::uint8_t state, std::uint8_t index) const;
    static void draw(PshImage&, const PshFont&, const FrontendHelpDescriptor&, const Nba97HelpModal&);
private:
    std::vector<FrontendHelpDescriptor> descriptors_;
};
}
