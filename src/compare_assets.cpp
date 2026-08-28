#include "compare_assets.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace nba97 {
CompareAssets::CompareAssets(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in || in.tellg()<8 || in.tellg()>8192) throw std::runtime_error("missing/oversized private Compare pack; run extract_compare_assets.py");
    std::vector<unsigned char> b(static_cast<std::size_t>(in.tellg()));
    in.seekg(0); in.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()));
    if(!in || !std::equal(b.begin(),b.begin()+8,std::array<unsigned char,8>{'N','9','7','C',1,0,13,0}.begin()))
        throw std::runtime_error("invalid Compare pack header");
    std::size_t at=8;
    auto half=[&]() -> unsigned { if(at+2>b.size()) throw std::runtime_error("truncated Compare record");
        const unsigned n=b[at]|(b[at+1]<<8); at+=2; return n; };
    auto string=[&]() { const auto n=half();
        if(!n || n>128 || n>b.size()-at) throw std::runtime_error("invalid Compare string size");
        std::string s(b.begin()+at,b.begin()+at+n); at+=n;
        for(unsigned char c:s) if(c<32 || c>126) throw std::runtime_error("unsupported Compare text control");
        return s; };
    for(auto& s:texts_) s=string();
    const std::array<unsigned,3> counts{14,17,24};
    static constexpr std::array<int,14> attributes{58,59,60,53,54,56,57,55,64,61,62,63,65,66};
    static constexpr std::array<int,24> stats{22,23,6,32,8,34,0,43,1,44,2,45,15,16,17,37,18,38,19,39,20,40,21,25};
    for(unsigned family=0;family<3;++family) {
        if(half()!=counts[family]) throw std::runtime_error("wrong Compare descriptor extent");
        for(unsigned i=0;i<counts[family];++i) {
            const auto raw=half(); const int field=raw==65535 ? -1 : static_cast<int>(raw);
            const int expected=family==0 ? attributes[i] : family==1 ? (i ? static_cast<int>(i)+13 : -1) : stats[i];
            if(field!=expected) throw std::runtime_error("unsupported Compare field mapping");
            fields_[family].push_back({static_cast<std::int16_t>(field),string()});
        }
    }
    if(at!=b.size()) throw std::runtime_error("trailing Compare pack data");
}
const std::string& CompareAssets::label(unsigned layer,unsigned row) const {
    if(layer>3) throw std::runtime_error("unsupported Compare layer");
    return fields_.at(layer<2 ? layer : 2).at(row).label;
}
std::string CompareAssets::value(const RosterDatabase& db,const PlayerRecord& player,unsigned layer,unsigned row) const {
    (void)label(layer,row); // Bounds and supported normal-context layer check.
    const int field=fields_[layer<2 ? layer : 2][row].id;
    if(layer==0) return db.playerAttribute(player,row+1); // Validated original attribute ordering above.
    if(layer==1) return std::to_string(field==-1 ? player.overallRating() : player.ratings.at(field-14));
    return player.stats(layer==2 ? PlayerStatPeriod::Season1995_96 : PlayerStatPeriod::Playoffs1995_96).format(static_cast<std::int16_t>(field));
}
}
