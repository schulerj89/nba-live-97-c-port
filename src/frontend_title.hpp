#pragma once
#include "psh_image.hpp"
#include "recovered/frontend_title.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

namespace nba97 {
// State changes belong to presentation dispatch, never to pixel drawing.
class FrontendTitlePresentation {
public:
    bool select(const std::string& tag, int x, int y, int width, int height) {
        if(tag==tag_) return false;
        if(width<=0 || height<=0 || width>255 || height>255 ||
           x < -32768 || y < -32768 || x > 32767-width || y > 32767-height)
            throw std::runtime_error("invalid original title extent");
        const auto phase=state_.next;
        const int16_t base[2][8]={{static_cast<int16_t>(x),static_cast<int16_t>(y),
            static_cast<int16_t>(x+width),static_cast<int16_t>(y),
            static_cast<int16_t>(x),static_cast<int16_t>(y+height),
            static_cast<int16_t>(x+width),static_cast<int16_t>(y+height)}, {}};
        nba97_title_init(&state_,base,1);
        state_.next=phase; // Original phase800FDBD8 survives layout construction.
        tag_=tag;
        return true;
    }
    void leave() {tag_.clear();} // Keep alternation; next entry rebuilds baseline.
    bool active() const {return !tag_.empty();}
    const std::string& tag() const {return tag_;}
    const int16_t* corners() const {return state_.current[0];}
    const Nba97TitleMotion& state() const {return state_;}
    int present(uint16_t& rng, bool suppressed=false) {
        return nba97_title_selector_step(&state_,&rng,suppressed);
    }
    int presentDirect(uint16_t& rng) {return nba97_title_step(&state_,&rng);}
private:
    Nba97TitleMotion state_{};
    std::string tag_;
};

// Native affine textured quad. 33DD4 writes one FT4's UVs; its40-byte copies
// are double buffers, not a split at texel128. This is not a PS1 GPU emulator:
// fixed-point raster precision/blending parity remains a separate comparison.
inline void drawFrontendTitle(PshImage& dst, const PshImage& texture, const int16_t* xy) {
    if(!xy || !texture.width || !texture.height ||
       texture.rgba.size()!=std::size_t(texture.width)*texture.height*4 ||
       dst.rgba.size()!=std::size_t(dst.width)*dst.height*4)
        throw std::runtime_error("invalid title quad image");
    const double u[4]={0.,double(texture.width-1),0.,double(texture.width-1)};
    const double v[4]={0.,0.,double(texture.height-1),double(texture.height-1)};
    const int triangles[2][3]={{0,1,2},{1,3,2}};
    for(const auto& t:triangles) {
        const int a=t[0],b=t[1],c=t[2];
        const double denominator=double(xy[2*b+1]-xy[2*c+1])*(xy[2*a]-xy[2*c])+
            double(xy[2*c]-xy[2*b])*(xy[2*a+1]-xy[2*c+1]);
        if(denominator==0) continue;
        const int left=(std::max)(0,(std::min)({int(xy[2*a]),int(xy[2*b]),int(xy[2*c])}));
        const int right=(std::min)(int(dst.width),(std::max)({int(xy[2*a]),int(xy[2*b]),int(xy[2*c])}));
        const int top=(std::max)(0,(std::min)({int(xy[2*a+1]),int(xy[2*b+1]),int(xy[2*c+1])}));
        const int bottom=(std::min)(int(dst.height),(std::max)({int(xy[2*a+1]),int(xy[2*b+1]),int(xy[2*c+1])}));
        for(int y=top;y<bottom;++y) for(int x=left;x<right;++x) {
            const double wa=((xy[2*b+1]-xy[2*c+1])*(x+.5-xy[2*c])+
                (xy[2*c]-xy[2*b])*(y+.5-xy[2*c+1]))/denominator;
            const double wb=((xy[2*c+1]-xy[2*a+1])*(x+.5-xy[2*c])+
                (xy[2*a]-xy[2*c])*(y+.5-xy[2*c+1]))/denominator;
            const double wc=1-wa-wb;
            if(wa < -1e-9 || wb < -1e-9 || wc < -1e-9) continue;
            if(a==1 && wb<=1e-9) continue; // Shared diagonal belongs to first triangle.
            const int sx=std::clamp(int(std::lround(wa*u[a]+wb*u[b]+wc*u[c])),0,int(texture.width)-1);
            const int sy=std::clamp(int(std::lround(wa*v[a]+wb*v[b]+wc*v[c])),0,int(texture.height)-1);
            const auto from=(std::size_t(sy)*texture.width+sx)*4;
            const auto to=(std::size_t(y)*dst.width+x)*4;
            const unsigned alpha=texture.rgba[from+3];
            if(!alpha) continue;
            for(int channel=0;channel<3;++channel)
                dst.rgba[to+channel]=static_cast<uint8_t>((texture.rgba[from+channel]*alpha+
                    dst.rgba[to+channel]*(255-alpha))/255);
            dst.rgba[to+3]=255;
        }
    }
}
} // namespace nba97
