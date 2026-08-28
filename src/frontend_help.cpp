#include "frontend_help.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace nba97 {
namespace {
constexpr std::size_t kLimit = 16384;
std::vector<std::uint8_t> readPack(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("missing private Help pack; run tools/extract_frontend_help.py");
    const auto size = in.tellg();
    if (size < 8 || size > static_cast<std::streamoff>(kLimit))
        throw std::runtime_error("Help pack exceeds size bounds");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    in.seekg(0);
    if (!in.read(reinterpret_cast<char*>(bytes.data()), size)) throw std::runtime_error("truncated Help pack");
    return bytes;
}
int encodedWidth(const PshFont& font, const std::string& text) {
    int width = 0;
    for (std::size_t i=0; i<text.size(); ++i) {
        if (static_cast<unsigned char>(text[i]) == 0x1f) {
            if (i+1 == text.size()) throw std::runtime_error("truncated Help spacing command");
            width += static_cast<unsigned char>(text[++i]);
        }
        else {
            if (text[i] != ' ' && !font.glyph(text[i])) throw std::runtime_error("missing original Help glyph");
            width += font.textWidth(text.substr(i,1));
        }
    }
    return width;
}
}

FrontendHelpPack::FrontendHelpPack(const std::filesystem::path& file) : FrontendHelpPack(readPack(file)) {}
FrontendHelpPack::FrontendHelpPack(const std::vector<std::uint8_t>& b) {
    auto fail = [] { throw std::runtime_error("invalid/unsupported private Help pack"); };
    if (b.size() < 8 || b.size() > kLimit || !std::equal(b.begin(),b.begin()+4,"N97H")) fail();
    auto half = [&](std::size_t at) { return unsigned(b[at]) | (unsigned(b[at+1])<<8); };
    auto word = [&](std::size_t at) { return std::uint32_t(half(at)) | (std::uint32_t(half(at+2))<<16); };
    if (half(4) != 1 || half(6) != 4) fail();
    std::size_t at=8;
    for (unsigned record=0;record<4;++record) {
        if (b.size()-at < 18) fail();
        FrontendHelpDescriptor d{};
        d.state=b[at]; d.index=b[at+1];
        const auto size=half(at+2); d.address=word(at+4); at+=8;
        if (size < 10 || size > b.size()-at) fail();
        const auto end=at+size;
        const unsigned x=half(at), y=half(at+2), w=half(at+4), h=b[at+6], lines=b[at+8];
        if (x>246 || y>110 || w<20 || x+w>512 || h<10 || y+h>240 ||
            b[at+7] || b[at+9] || !lines || lines>16 || y+10+16*(lines-1)>y+h) fail();
        if (!(((d.state==12 || d.state==13 || d.state==14) && d.index<2) || ((d.state==35 || d.state==36) && d.index==0))) fail();
        for (const auto& prior : descriptors_) if (prior.state==d.state && prior.index==d.index) fail();
        d.rect={static_cast<std::int16_t>(x),static_cast<std::int16_t>(y),
                static_cast<std::int16_t>(w),static_cast<std::int16_t>(h)};
        at+=10;
        for (unsigned row=0;row<lines;++row) {
            if (at>=end || b[at]>1) fail();
            FrontendHelpLine line{b[at++]!=0,0,{}};
            if (!line.centered) {
                if (at>=end || b[at]>=w) fail();
                line.offset=b[at++];
            }
            while (at<end && b[at]) {
                const auto ch=b[at++];
                line.encoded+=static_cast<char>(ch);
                if (ch==0x1f) {
                    if (at>=end || !b[at]) fail();
                    line.encoded+=static_cast<char>(b[at++]);
                } else if (ch<32) fail();
                if (line.encoded.size()>256) fail();
            }
            if (at>=end) fail();
            ++at; d.lines.push_back(std::move(line));
        }
        if (at!=end) fail();
        descriptors_.push_back(std::move(d));
    }
    if (at!=b.size()) fail();
}

const FrontendHelpDescriptor& FrontendHelpPack::descriptor(std::uint8_t state, std::uint8_t index) const {
    for (const auto& d : descriptors_) if (d.state==state && d.index==index) return d;
    throw std::runtime_error("unavailable original Help descriptor");
}

void FrontendHelpPack::draw(PshImage& image, const PshFont& font,
        const FrontendHelpDescriptor& d, const Nba97HelpModal& m) {
    if (!nba97_help_visible(&m)) return;
    if (image.width!=512 || image.height!=240 || image.rgba.size()!=512*240*4 ||
        m.rect.x<0 || m.rect.y<0 || m.rect.width<1 || m.rect.height<1 ||
        m.rect.x+m.rect.width>512 || m.rect.y+m.rect.height>240)
        throw std::runtime_error("invalid Help render bounds");
    if(d.style>1) throw std::runtime_error("unsupported modal style");
    // 30430 G4: style0 green Help; style1 red no-choice warning.
    // Native interpolation; exact PSX rasterization remains reference-unverified.
    const auto r=m.rect;
    for (int y=0;y<r.height;++y) for(int x=0;x<r.width;++x) {
        const int t=x*256/r.width+y*256/r.height;
        const int mix=std::min(t,512-t);
        const auto at=((r.y+y)*512+r.x+x)*4;
        if(d.style==1) {
            image.rgba[at]=static_cast<std::uint8_t>(20+80*mix/256);
            image.rgba[at+1]=image.rgba[at+2]=static_cast<std::uint8_t>(10-10*mix/256);
        } else {
            image.rgba[at]=image.rgba[at+2]=static_cast<std::uint8_t>(10-10*mix/256);
            image.rgba[at+1]=static_cast<std::uint8_t>(20+130*mix/256);
        }
        image.rgba[at+3]=255;
    }
    if (!nba97_help_text_visible(&m)) return;
    int y=d.rect.y+(d.style ? 15 : 10); // 40B5C no-choice warning offset.
    for (const auto& line : d.lines) {
        y+=line.extra_before;
        int x=line.centered ? 256-encodedWidth(font,line.encoded)/2 : d.rect.x+line.offset;
        // Validate every glyph even for left-aligned rows. 2C6B0's 1F byte
        // advances the cursor by its following unsigned byte; it isn't text.
        (void)encodedWidth(font,line.encoded);
        for (std::size_t i=0;i<line.encoded.size();++i) {
            if (static_cast<unsigned char>(line.encoded[i])==0x1f) {
                x+=static_cast<unsigned char>(line.encoded[++i]); continue;
            }
            const auto glyph=line.encoded.substr(i,1);
            const int width=font.textWidth(glyph);
            draw_psh_text_centered(image,font,glyph,x+width/2,y);
            x+=width;
        }
        y+=d.style ? 12 : 16; // 40CA4/40CB8: only Help adds extra4.
    }
}
}
