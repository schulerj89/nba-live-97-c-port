#include "roster_reset_assets.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>
namespace nba97 {
namespace {
std::vector<std::uint8_t> readReset(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in) throw std::runtime_error("missing private reorder/reset.n97ui; run extract_roster_reset.py");
    const auto size=in.tellg();
    if(size<10 || size>2048) throw std::runtime_error("Reset pack size out of bounds");
    std::vector<std::uint8_t> b(static_cast<std::size_t>(size));in.seekg(0);
    if(!in.read(reinterpret_cast<char*>(b.data()),size)) throw std::runtime_error("truncated Reset pack");
    return b;
}
}
RosterResetAssets::RosterResetAssets(const std::filesystem::path& root)
    : RosterResetAssets(readReset(root/"reorder/reset.n97ui")) {
    font_=load_psh_font(root/"fonts/ZFONT1.PSH",10,1);
    for(const auto& line:lines_) for(char c:line)
        if(c!=' ' && !font_.glyph(c)) throw std::runtime_error("Reset requires original ZFONT1 glyph");
}
RosterResetAssets::RosterResetAssets(const std::vector<std::uint8_t>& b) {
    const std::uint8_t expected[]{121,0,75,0,14,1,110,1,5,2};
    if(b.size()<10 || b.size()>2048 || !std::equal(std::begin(expected),std::end(expected),b.begin()))
        throw std::runtime_error("unsupported Reset descriptor");
    rect_={121,75,270,110};
    std::size_t pos=10;
    for(unsigned i=0;i<7;++i) {
        if(pos>=b.size() || b[pos++]!=1) throw std::runtime_error("Reset line alignment invalid");
        std::string line;
        while(pos<b.size() && b[pos]) {
            if(b[pos]<32 || b[pos]>126 || line.size()>=120) throw std::runtime_error("Reset line invalid");
            line+=char(b[pos++]);
        }
        if(pos>=b.size()) throw std::runtime_error("Reset line unterminated");
        ++pos;lines_.push_back(std::move(line));
    }
    if(pos!=b.size()) throw std::runtime_error("Reset pack trailing bytes");
}
void RosterResetAssets::draw(PshImage& im,const Nba97ResetPrompt& p,std::uint32_t ticks) const {
    (void)ticks; // Animation belongs to the C logical-tick state, not wall time.
    if(!nba97_help_visible(&p.modal)) return;
    const auto r=p.modal.rect;
    if(im.width!=512 || im.height!=240 || im.rgba.size()!=512*240*4 ||
       r.x<0 || r.y<0 || r.width<1 || r.height<1 || r.x+r.width>512 || r.y+r.height>240)
        throw std::runtime_error("Reset render bounds invalid");
    for(int y=0;y<r.height;++y) for(int x=0;x<r.width;++x) {
        const int t=x*256/r.width+y*256/r.height,mix=std::min(t,512-t);
        const auto at=((r.y+y)*512+r.x+x)*4;
        im.rgba[at]=std::uint8_t(20+80*mix/256); // 30430 style1: red20..100
        im.rgba[at+1]=im.rgba[at+2]=std::uint8_t(10-10*mix/256);im.rgba[at+3]=255;
    }
    if(!nba97_help_text_visible(&p.modal)) return;
    // 40A1C: choices start at y+10, +12 each body line, +6 after last
    // body line. Unselected choice adds +4; no invented arrow icon.
    int y=rect_.y+10;
    for(unsigned i=0;i<lines_.size();++i) {
        int x=256-font_.textWidth(lines_[i])/2;
        for(char ch:lines_[i]) {
            if(ch==' ') {x+=font_.spaceWidth();continue;}
            const auto* glyph=font_.glyph(ch);
            if(!glyph) throw std::runtime_error("Reset render missing glyph");
            for(int yy=0;yy<glyph->height;++yy) for(int xx=0;xx<glyph->width;++xx) {
                const int px=x+xx,py=y-glyph->center_y+yy;
                const auto source=(yy*glyph->width+xx)*4;
                if(!glyph->rgba[source+3] || px<0 || px>=512 || py<0 || py>=240) continue;
                const auto at=(py*512+px)*4;
                for(unsigned c=0;c<3;++c) {
                    const unsigned factor=i>=5 ? p.tint[i-5].rgb[c]:128;
                    im.rgba[at+c]=std::uint8_t(std::min(255u,glyph->rgba[source+c]*factor/128));
                }
                im.rgba[at+3]=255;
            }
            x+=std::max(0,int(glyph->width)-font_.kerning());
        }
        y+=12;
        if(i==4) y+=6;
        else if(i>4 && i!=5u+p.initial_choice) y+=4;
    }
}
}
