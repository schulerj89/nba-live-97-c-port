#include "match_controls.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>

static void check(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
static void tests() {
    Nba97MatchControls initial{};Nba97ProfileControls profiles{};
    std::array<uint8_t,59> defaults{};
    for(unsigned b=0;b<59;++b)defaults[b]=uint8_t(255-b);
    for(unsigned c=0;c<8;++c) {
        for(unsigned b=0;b<36;++b)initial.stats[c][b]=uint8_t(1+c*36+b);
        for(unsigned b=0;b<59;++b)initial.map[c][b]=uint8_t(31+c*59+b);
    }
    for(unsigned p=0;p<20;++p)
        for(unsigned b=0;b<59;++b)profiles.map[p][b]=uint8_t(p*13+b*7);
    // Exercise every signed byte and every validity byte. Each call checks all
    // slots with distinct prior maps/statistics; force accepts any selector.
    unsigned cases=0;
    for(int selector=-128;selector<=127;++selector)for(unsigned valid=0;valid<=255;++valid)
        for(int force:{0,1,-1}) {
            std::array<int8_t,8> selectors;selectors.fill(int8_t(selector));
            std::fill_n(profiles.valid,20,uint8_t(valid));
            auto state=initial;std::array<uint8_t,8> origin;origin.fill(0xab);
            const bool ok=nba97_match_controls_finalize(&state,selectors.data(),&profiles,
                                                        defaults.data(),force,origin.data())!=0;
            if(!force && selector>=20) {
                check(!ok && !std::memcmp(&state,&initial,sizeof(state)),"invalid selector mutated live state");
                check(std::all_of(origin.begin(),origin.end(),[](auto n){return n==0xab;}),"invalid selector changed receipt");
            } else {
                check(ok,"valid selector refused");
                for(unsigned c=0;c<8;++c) {
                    const uint8_t* expected=force || (selector>=0 && !valid) ? defaults.data():
                        selector<0 ? initial.map[c]:profiles.map[selector];
                    check(!std::memcmp(state.map[c],expected,59),"wrong effective controls");
                    check(std::all_of(state.stats[c],state.stats[c]+36,[](auto n){return !n;}),"stats prefix not cleared");
                    check(origin[c]==(force || (selector>=0 && !valid) ? NBA97_CONTROLS_DEFAULT:
                        selector<0 ? NBA97_CONTROLS_RETAINED:NBA97_CONTROLS_PROFILE),"wrong map provenance");
                }
            }
            ++cases;
        }
    // Mixed selectors prove per-controller decisions and retained live lifetime.
    std::array<int8_t,8> late_bad{{-2,-1,0,1,2,3,4,20}};
    auto guarded=initial;std::array<uint8_t,8> guard_origin{};guard_origin.fill(99);
    check(!nba97_match_controls_finalize(&guarded,late_bad.data(),&profiles,defaults.data(),0,guard_origin.data()) &&
          !std::memcmp(&guarded,&initial,sizeof(initial)) && guard_origin[0]==99,
          "last-slot validation must precede every mutation");
    check(!nba97_match_controls_finalize(&guarded,late_bad.data(),nullptr,defaults.data(),1,guard_origin.data()) &&
          !std::memcmp(&guarded,&initial,sizeof(initial)),"missing input must not mutate live state");
    const std::array<int8_t,8> selectors{{-2,-1,1,7,19,-128,0,2}};
    std::vector<nba97::UserProfile> saved(3);
    for(unsigned i=0;i<3;++i) {
        auto& p=saved[i];p.slot=uint8_t(i==0?1:i==1?7:19);p.id=100+i;
        p.controls_valid=uint8_t(i==0?0:i==1?128:255);
        for(unsigned b=0;b<59;++b)p.controls[b]=uint8_t(i*17+b*3);
    }
    auto first=nba97::finalizeMatchControls(initial,selectors,saved,defaults);
    check(first.profile_ids==std::array<uint64_t,8>{{0,0,100,101,102,0,0,0}},"stable-ID receipt");
    check(first.provenance==std::array<uint8_t,8>{{0,0,1,2,2,0,1,1}},"mixed and cleared-slot provenance");
    const auto frozen=first;
    saved[1].controls.fill(55);saved[1].controls_valid=0;
    auto negative=selectors;negative.fill(-2);
    auto second=nba97::finalizeMatchControls(first.controls,negative,saved,defaults);
    check(!std::memcmp(&first.controls,&second.controls,sizeof(first.controls)),"FE lost previous live maps");
    check(!std::memcmp(&frozen.controls,&first.controls,sizeof(first.controls)),"result borrowed mutable profile maps");
    auto bootstrap=nba97::finalizeMatchControls(first.controls,selectors,saved,defaults,true);
    for(const auto& map:bootstrap.controls.map)check(!std::memcmp(map,defaults.data(),59),"cold defaults");
    auto duplicate=saved;duplicate.push_back(saved[0]);
    bool rejected=false;try {nba97::finalizeMatchControls(initial,selectors,duplicate,defaults);}
    catch(const std::runtime_error&) {rejected=true;}
    check(rejected,"duplicate fixed slot/ID accepted");
    saved[0].slot=255;rejected=false;
    try {nba97::finalizeMatchControls(initial,selectors,saved,defaults);}
    catch(const std::runtime_error&) {rejected=true;}
    check(rejected,"unmapped legacy vector accepted");
    std::printf("MATCH CONTROLS PASS: %u signed-selector/validity/force cases; fixed slots, cleared records, IDs and owned live-map retention\n",cases);
}
int main() {try {tests();return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
