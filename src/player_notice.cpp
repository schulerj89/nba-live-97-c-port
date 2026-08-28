#include "player_notice.hpp"
#include "cool_fact_index.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace nba97 {
FrontendHelpDescriptor parsePlayerNotice(const std::vector<std::uint8_t>& b) {
    constexpr std::uint8_t header[]{136,0,90,0,240,0,64,1,2,0};
    if(b.size()<10 || b.size()>1024 || !std::equal(std::begin(header),std::end(header),b.begin()))
        throw std::runtime_error("invalid private player/no-facts.n97ui descriptor");
    FrontendHelpDescriptor d{};
    d.state=36;d.address=0x800AFE06;d.rect={136,90,240,64};d.style=1;
    std::size_t at=10;
    // Two descriptor lines plus 40E20's shared continuation prompt at2502C.
    for(unsigned i=0;i<3;++i) {
        if(at>=b.size() || b[at++]!=1) throw std::runtime_error("invalid no-facts alignment");
        FrontendHelpLine line{true,0,{}};
        if(i==2) line.extra_before=6;
        while(at<b.size() && b[at]) {
            if(b[at]<32 || b[at]>126 || line.encoded.size()>=120)
                throw std::runtime_error("invalid no-facts text");
            line.encoded+=char(b[at++]);
        }
        if(at>=b.size()) throw std::runtime_error("unterminated no-facts text");
        ++at;d.lines.push_back(std::move(line));
    }
    if(at!=b.size()) throw std::runtime_error("trailing no-facts data");
    return d;
}
FrontendHelpDescriptor loadPlayerNotice(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input) throw std::runtime_error("missing private player/no-facts.n97ui; run extract_player_notice.py");
    const auto size=input.tellg();
    if(size<10 || size>1024) throw std::runtime_error("no-facts pack exceeds bounds");
    std::vector<std::uint8_t> b(static_cast<std::size_t>(size));input.seekg(0);
    if(!input.read(reinterpret_cast<char*>(b.data()),size)) throw std::runtime_error("truncated no-facts pack");
    return parsePlayerNotice(b);
}
bool playerHasCoolFacts(const std::vector<std::uint8_t>& b,std::uint16_t player) {
    const CoolFactIndexView index(b);
    for(unsigned v=0;v<5;++v) {
        if(index.lookup(player,v).bytes) return true;
    }
    return false;
}
}
