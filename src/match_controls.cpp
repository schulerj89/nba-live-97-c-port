#include "match_controls.hpp"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace nba97 {
MatchControlResult finalizeMatchControls(const Nba97MatchControls& live,
        const std::array<int8_t,8>& selectors,const std::vector<UserProfile>& profiles,
        const std::array<uint8_t,59>& defaults,bool force) {
    if(profiles.size()>20) throw std::runtime_error("match controls: profile capacity exceeded");
    Nba97ProfileControls source{};
    std::array<uint64_t,20> ids{};std::unordered_set<uint64_t> seen;
    for(const auto& p:profiles) {
        if(p.slot>=20 || !p.id || ids[p.slot] || !seen.insert(p.id).second)
            throw std::runtime_error("match controls: invalid profile ID or fixed slot");
        ids[p.slot]=p.id;source.valid[p.slot]=p.controls_valid;
        std::copy(p.controls.begin(),p.controls.end(),source.map[p.slot]);
    }
    MatchControlResult result;result.controls=live;
    if(!nba97_match_controls_finalize(&result.controls,selectors.data(),&source,
                                     defaults.data(),force,result.provenance.data()))
        throw std::runtime_error("match controls: selector outside bounded profile table");
    for(unsigned c=0;c<8;++c)
        if(selectors[c]>=0 && selectors[c]<20)result.profile_ids[c]=ids[unsigned(selectors[c])];
    return result;
}
}
