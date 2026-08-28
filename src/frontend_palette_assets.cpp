#include "frontend_palette_assets.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace nba97 {
namespace {
constexpr std::size_t pack_size=16+33*324+4*(128*240+192);
constexpr const char* tags[]={"atlP","bosP","chaP","chiP","cleP","dalP","denP","detP",
    "golP","houP","indP","lacP","lalP","miaP","milP","minP","nwjP","nwyP","orlP",
    "phiP","phoP","porP","sacP","sanP","seaP","torP","utaP","vanP","wasP",
    "xeaP","xweP","zc1P","zc2P"};
unsigned half(const std::vector<std::uint8_t>& b,std::size_t at) {
    return b.at(at)|(unsigned(b.at(at+1))<<8);
}
}
FrontendPaletteAssets::FrontendPaletteAssets(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in || in.tellg()!=static_cast<std::streamoff>(pack_size))
        throw std::runtime_error("missing/wrong-size private indexed palette pack; run decode_team_backgrounds.py");
    std::vector<std::uint8_t> b(pack_size);
    in.seekg(0); in.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()));
    if(!in) throw std::runtime_error("cannot read indexed palette pack");
    parse(b);
}
FrontendPaletteAssets::FrontendPaletteAssets(const std::vector<std::uint8_t>& b) { parse(b); }
void FrontendPaletteAssets::parse(const std::vector<std::uint8_t>& b) {
    if(b.size()!=pack_size || !std::equal(b.begin(),b.begin()+4,"N97P") ||
        half(b,4)!=1 || half(b,6)!=4 || half(b,8)!=33 ||
        half(b,10)!=128 || half(b,12)!=240 || half(b,14)!=0)
        throw std::runtime_error("invalid indexed palette header");
    std::size_t at=16;
    for(unsigned t=0;t<count;++t) {
        if(!std::equal(b.begin()+at,b.begin()+at+4,tags[t]))
            throw std::runtime_error("indexed palette team order differs from source");
        at+=4;
        for(unsigned i=0;i<160;++i,at+=2) bank_[t*160+i]=static_cast<std::uint16_t>(half(b,at));
    }
    for(unsigned strip=0;strip<4;++strip) {
        std::copy_n(b.begin()+at,128*240,indices_.begin()+strip*128*240); at+=128*240;
        for(unsigned i=0;i<96;++i,at+=2) local_[strip*96+i]=static_cast<std::uint16_t>(half(b,at));
    }
}
void FrontendPaletteAssets::draw(PshImage& image,const Nba97FrontendPalette& state) const {
    if(image.width!=512 || image.height!=240 || image.rgba.size()!=512*240*4 ||
        !state.initialized || state.half[0].target>=count || state.half[1].target>=count)
        throw std::runtime_error("invalid indexed background render state");
    // Match the existing opaque background rasterizer's RGB expansion. Zero
    // CLUT words are black here; foreground STP blending is a separate concern.
    for(unsigned strip=0;strip<4;++strip) for(unsigned y=0;y<240;++y) for(unsigned x=0;x<128;++x) {
        const auto index=indices_[strip*128*240+y*128+x];
        const auto word=index<160?state.half[strip/2].current[index]:local_[strip*96+index-160];
        const auto dst=(y*512+strip*128+x)*4;
        for(unsigned c=0;c<3;++c) {
            const auto v=(word>>(c*5))&31;
            image.rgba[dst+c]=static_cast<std::uint8_t>((v<<3)|(v>>2));
        }
        image.rgba[dst+3]=255;
    }
}
}
