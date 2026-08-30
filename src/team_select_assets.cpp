#include "team_select_assets.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace nba97 {
TeamSelectAssets::TeamSelectAssets(const std::filesystem::path& root)
    : backgrounds_(root/"indexed.n97pal"), help_(root/"help.n97ui") {
    std::ifstream input(root/"ui.n97select",std::ios::binary|std::ios::ate);
    if(!input || input.tellg()<12 || input.tellg()>16384)
        throw std::runtime_error("missing/bounded Team Select pack; run tools/extract_team_select.py");
    std::vector<uint8_t> b(static_cast<size_t>(input.tellg()));
    input.seekg(0);input.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()));
    if(!input || !std::equal(b.begin(),b.begin()+4,"N97S"))
        throw std::runtime_error("invalid Team Select pack");
    size_t at=4;
    auto word=[&]() { if(at+2>b.size()) throw std::runtime_error("truncated Team Select pack");
        uint16_t v=b[at]|(uint16_t(b[at+1])<<8);at+=2;return v; };
    auto text=[&](size_t length) {
        if(!length || length>127 || at+length>b.size()) throw std::runtime_error("invalid Team Select string");
        std::string s(b.begin()+at,b.begin()+at+length);at+=length;
        if(std::any_of(s.begin(),s.end(),[](unsigned char c){return c<32 || c>126;}))
            throw std::runtime_error("unsupported Team Select string");
        return s;
    };
    if(word()!=1 || word()!=31 || word()!=18 || word()!=5)
        throw std::runtime_error("unsupported Team Select pack version/counts");
    for(auto& value:rng_) {uint32_t low=word();value=low|(uint32_t(word())<<16);}
    for(auto& adjustment:adjustments_) adjustment=static_cast<int16_t>(word());
    heading_=text(word());
    for(auto& label:criteria_) label=text(word());
    for(auto& team:teams_) {team.city=text(word());team.nickname=text(word());}
    for(auto& team:teams_) {team.logo=text(word());if(team.logo.size()!=4) throw std::runtime_error("invalid logo tag");}
    for(auto& item:layout_) {
        item.x=static_cast<int16_t>(word());item.y=static_cast<int16_t>(word());
        item.z=static_cast<int16_t>(word());item.flags=static_cast<int16_t>(word());item.tag=text(4);
        if(item.x<0 || item.x>511 || item.y<0 || item.y>239 || item.z<1 || item.z>17 || (item.flags&~1))
            throw std::runtime_error("invalid Team Select layout");
    }
    if(at!=b.size()) throw std::runtime_error("trailing Team Select data");
    (void)help_.descriptor(3,0);
}

Nba97TeamRanks TeamSelectAssets::ranks(const RosterDatabase& database, uint16_t (*scores)[29]) const {
    std::array<Nba97TeamRatingInput,29> input{};
    for(unsigned team=0;team<29;++team) {
        const auto players=database.resolveTeamSlots(static_cast<int16_t>(team));
        bool empty=false;
        for(unsigned slot=0;slot<15;++slot) {
            if(!players[slot]) {empty=true;continue;}
            if(empty) throw std::runtime_error("Team Select requires resolved contiguous current rosters");
            ++input[team].count;
            std::copy(players[slot]->ratings.begin(),players[slot]->ratings.end(),input[team].ratings[slot]);
        }
    }
    uint16_t local[5][29];Nba97TeamRanks result{};
    if(!nba97_team_ratings(input.data(),adjustments_.data(),scores ? scores:local,&result))
        throw std::runtime_error("Team Select ranking requires 8..15 resolved current players per regular team");
    return result;
}
}
