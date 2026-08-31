#include "match_input_edges.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace nba97;
unsigned checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(std::to_string(checks)+": "+why);}
MatchRuntimeState fixture() {
    MatchRuntimeState s;
    for(auto& c:s.controller)c.clearFromSource();
    for(unsigned i=0;i<11;++i){s.entity_table[i]={static_cast<std::uint8_t>(i),1};s.entity[i].record.clearFromSource();}
    return s;
}
bool sameRecords(const MatchRuntimeState& a,const MatchRuntimeState& b) {
    for(unsigned i=0;i<8;++i)if(a.controller[i].bytes!=b.controller[i].bytes||a.controller[i].known!=b.controller[i].known)return false;
    for(unsigned i=0;i<11;++i)if(a.entity[i].record.bytes!=b.entity[i].record.bytes||a.entity[i].record.known!=b.entity[i].record.known)return false;
    return true;
}
void edges() {
    auto s=fixture();s.controller[3].put(0x30,2,0x8000);
    // Neutral direction tolerates unknown mode3C and all camera globals.
    s.controller[3].known[0x3c]=0;
    auto result=updateMatchRuntimeInputEdges(s,{3,1},0x12340000u,{});
    check(result.published&&result.result==NBA97_INPUT_OK,"full32 neutral edge update");
    check(result.receipt.edge_mask.known==1&&result.receipt.edge_mask.word==0,"signextended previous mask participates in full32 AND");
    check(s.controller[3].read(0x32,2).word==0x8000&&s.controller[3].read(0x2a,2).word==1024,"stored changed half and neutral direction");
    check(s.controller[3].known[0x3c]==0,"unknown unconsumed mode preserved");
    unsigned writes2a=0;for(unsigned i=0;i<result.receipt.count;++i){const auto& e=result.receipt.event[i];
        if(e.kind==2&&e.field==NBA97_INPUT_CONTROL_2A){check(e.value.word==(writes2a?1024u:8u),"two2A stores in source order");++writes2a;}}
    check(writes2a==2,"both2A stores retained");
    s=fixture();result=updateMatchRuntimeInputEdges(s,{2,1},0x12340000u,{});
    check(result.published&&result.receipt.edge_mask.word==0x12340000u,"return does not narrow to stored16");
    check(s.controller[2].read(0x34,2).word==0,"low16 edge store remains separate");
    s.controller[2].put(0x26,2,7);s.entity_table[7]={10,1};
    result=updateMatchRuntimeInputEdges(s,{2,1},0x400,{});
    check(result.published&&s.entity[10].record.read(0xe4,2).word==10,"held400 follows aliased table into ball record");
    check(s.entity[7].record.read(0xe4,2).word==0,"selected table index is not a physical entity shortcut");
}
void directions() {
    MatchInputCamera camera{{7,1},{5,1},{1,1}};
    auto s=fixture();s.controller[0].put(0x3c,1,0);
    auto result=updateMatchRuntimeInputEdges(s,{0,1},15,camera);
    check(result.published,"contradictory directions supported");
    check(s.controller[0].read(0x38,4).word==5,"bit8 beats4 andbit1 beats2");
    check(s.controller[0].read(0x2a,2).word==3*128,"camera5 and flip transform exact direction");
    s=fixture();s.controller[0].put(0x3c,1,1);
    result=updateMatchRuntimeInputEdges(s,{0,1},15,{{7,1},{},{}});
    check(result.published&&s.controller[0].read(0x2a,2).word==4*128,"nonzero controller mode ignores camera mode and flip");
    // Sweep all low16 held masks against the separate direction-priority and
    // edge-bit formula; the original owner has its own MIPS comparison suite.
    s=fixture();s.controller[0].put(0x3c,1,1);
    unsigned previous=0;
    for(unsigned mask=0;mask<65536;++mask){
        s.controller[0].put(0x26,2,0xffff);
        result=updateMatchRuntimeInputEdges(s,{0,1},mask,{{0,1},{},{}});
        check(result.published,"all16bit masks execute");
        const auto sign=previous&0x8000u?previous|0xffff0000u:previous;
        check(result.receipt.edge_mask.word==((sign^mask)&mask),"independent edge oracle");
        previous=mask;
    }
}
void refusal() {
    auto s=fixture();s.controller[1].put(0x30,2,0x1234);const auto before=s;
    auto result=updateMatchRuntimeInputEdges(s,{1,1},1,{});
    check(!result.published&&result.result==NBA97_INPUT_UNRESOLVED&&sameRecords(s,before),"unknown camera atomic refusal");
    check(result.receipt.count>=6&&result.receipt.event[0].value.word==1,"source prefix still available");
    s=fixture();s.controller[1].put(0x26,2,11);const auto badIndex=s;
    result=updateMatchRuntimeInputEdges(s,{1,1},0x400,{});
    check(!result.published&&result.result==NBA97_INPUT_REFERENCE&&sameRecords(s,badIndex),"unowned selected slot not clamped");
    s=fixture();s.controller[1].put(0x26,2,2);s.entity[2].record.known[0xe4]=2;const auto badWrite=s;
    result=updateMatchRuntimeInputEdges(s,{1,1},0x400,{});
    check(!result.published&&result.result==NBA97_INPUT_ARGUMENT&&sameRecords(s,badWrite),"write-only entity metadata cannot be silently repaired");
    s=fixture();s.controller[0].known[0x30]=0;s.controller[0].known[0x31]=2;const auto malformed=s;
    result=updateMatchRuntimeInputEdges(s,{0,1},0,{});
    check(!result.published&&result.result==NBA97_INPUT_ARGUMENT&&sameRecords(s,malformed),"later malformed previous-mask byte not hidden by unknown first");
    result=updateMatchRuntimeInputEdges(s,{0,0},0,{});
    check(result.result==NBA97_INPUT_UNRESOLVED,"unknown actualcontrollerreference staysunknown");
    result=updateMatchRuntimeInputEdges(s,{8,1},0,{});
    check(result.result==NBA97_INPUT_REFERENCE,"controller table extent enforced");
}
}
int main(){try{edges();directions();refusal();std::cout<<checks<<" native match input-edge checks passed\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
