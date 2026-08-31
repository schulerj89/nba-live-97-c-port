#include "recovered/game_controller_selection.h"
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

static void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
static Nba97GameSelectionInput initial() {
    Nba97GameSelectionInput in{};
    for(unsigned i=0;i<8;++i) {
        in.controller_table[i]=uint8_t(i);
        in.controller[i].team_base=-1;
        in.controller[i].selected={uint16_t(0xdead+i),1};
    }
    for(unsigned i=0;i<11;++i) {in.entity_table[i]=uint8_t(i);in.entity[i].claim=-1;}
    in.ball=10;in.incoming_s6={7,1};return in;
}
static Nba97GameSelectionEffects execute(const Nba97GameSelectionInput& in) {
    Nba97GameSelectionEffects out{};const auto before=in;
    check(nba97_game_controller_selection(&out,&in)==NBA97_SELECTION_OK,"valid selection failed");
    check(!std::memcmp(&before,&in,sizeof(in)),"input modified");return out;
}
static void refused(const Nba97GameSelectionInput& in,int status) {
    Nba97GameSelectionEffects out;std::memset(&out,0xa5,sizeof(out));const auto before=out;
    check(nba97_game_controller_selection(&out,&in)==status,"wrong refusal status");
    check(!std::memcmp(&before,&out,sizeof(out)),"failure published partial effects");
}

static void tests() {
    check(nba97_game_selection_distance(0,0)==0,"zero distance");
    check(nba97_game_selection_distance(-7,10)==12,"close-axis integer distance");
    check(nba97_game_selection_distance(3,-20)==20,"far-axis integer distance");
    check(nba97_game_selection_distance(4,20)==21,"quarter contribution");
    check(nba97_game_selection_distance(10,20)==23,"inclusive twice-min branch");
    check(nba97_game_selection_distance(std::numeric_limits<int32_t>::min(),0)==268435456,
          "source INT_MIN negation/shift quirk repaired");
    auto in=initial();in.controller[0].team_base=0;in.controller[1].team_base=0;
    auto out=execute(in);
    check(out.selected[0].word==4 && out.selected[1].word==3,"ties must choose later free entity");
    check(out.claim[4]==0 && out.claim[3]==1 && out.write_count==2,"claim ordering");
    check(out.writes[0].controller_record==0 && out.writes[0].entity_record==4 &&
          out.writes[1].entity_record==3,"ordered effect log");

    in=initial();in.controller[0].team_base=32767;
    out=execute(in);check(out.selected[0].word==9,"all positive team bases choose away");
    in.controller[0].team_base=-32768;in.ball=255;in.entity_table[0]=255;
    out=execute(in);check(out.write_count==0,"negative team ignored");

    in=initial();in.controller[0].team_base=0;in.entity[2].claim=0;
    in.ball=255;in.incoming_s6={0,0};
    out=execute(in);
    check(out.selected[0].word==0xdead && !out.selected_written[0],"existing claim repaired stale selection");
    in.controller[0].selected={0,0};out=execute(in);
    check(!out.selected[0].known,"existing claim invented unknown selection");

    in=initial();in.controller[0].team_base=0;
    for(unsigned i=0;i<5;++i) in.entity[i].claim=99;
    in.ball=255;out=execute(in);
    check(out.selected[0].word==7 && out.claim[7]==0,"no-candidate incoming s6 bug repaired");
    check(out.scratch_s6.word==7,"scratch s6 not retained");
    in.incoming_s6={0xc0000007u,1};out=execute(in);
    check(out.selected[0].word==7 && out.claim[7]==0 && out.writes[0].raw_s6==0xc0000007u,
          "source pointer-index SLL wrapping lost");
    in.incoming_s6={0,0};refused(in,NBA97_SELECTION_UNRESOLVED);
    in.incoming_s6={11,1};refused(in,NBA97_SELECTION_OUTSIDE_STORAGE);

    in=initial();in.controller[0].team_base=0;
    for(unsigned i=0;i<5;++i) in.entity[i].x=801*256;
    out=execute(in);check(out.selected[0].word==7,"distance ceiling repaired");
    in.entity[1].x=800*256;out=execute(in);
    check(out.selected[0].word==1,"inclusive distance800 refused");
    in.entity[1].x=0;in.entity[2].x=1;out=execute(in);
    check(out.selected[0].word==1,"negative difference must arithmetic-shift before distance");
    in.entity[1].x=std::numeric_limits<int32_t>::max();
    in.entity[10].x=std::numeric_limits<int32_t>::min();out=execute(in);
    check(out.selected[0].word==1,"coordinate subtraction must wrap before shift");

    in=initial();
    for(auto& c:in.controller) c.team_base=0;
    out=execute(in);
    check(out.write_count==8 && out.selected[5].word==0 && out.selected[7].word==0 &&
          out.claim[0]==7,"later no-candidate searches must overwrite stale candidate claim");
    for(auto& reference:in.controller_table) reference=0;
    out=execute(in);
    check(out.selected[0].word==0 && out.selected[1].word==0xdeae && out.writes[7].controller_record==0,
          "controller table alias lost");
    in=initial();in.controller[0].team_base=0;in.entity_table[0]=5;
    out=execute(in);
    check(out.selected[0].word==4 && out.claim[4]==0 && out.claim[9]==-1,
          "contiguous search and indexed write incorrectly flattened");

    unsigned tail_cases=0;
    for(int state=-32768;state<=32767;++state) {
        in=initial();in.tail_state=int16_t(state);in.tail_entity=10;
        out=execute(in);
        const bool changed=state!=0 && state<9;
        check(out.tail_state==(changed ? 1:state),"signed tail state result");
        check(out.tail_state_written==uint8_t(changed),"tail write ownership");
        check(out.call_7a36c==uint8_t(changed && state>=2),"tail callback boundary");
        in.entity[10].claim=0;out=execute(in);
        check(out.tail_state==state && !out.tail_state_written && !out.call_7a36c,"claimed tail changed");
        ++tail_cases;
    }
    in=initial();in.tail_entity=-1;in.tail_state=0;execute(in);
    in.tail_state=9;refused(in,NBA97_SELECTION_OUTSIDE_STORAGE);
    in.tail_entity=3;in.entity_table[3]=255;refused(in,NBA97_SELECTION_UNRESOLVED);
    // Late tail lookup must not publish the selection already computed.
    in.controller[0].team_base=0;refused(in,NBA97_SELECTION_UNRESOLVED);
    in=initial();in.controller_table[7]=255;refused(in,NBA97_SELECTION_UNRESOLVED);
    in=initial();in.controller[0].team_base=0;in.entity_table[0]=7;
    refused(in,NBA97_SELECTION_OUTSIDE_STORAGE);
    // Existing claim at the first record stops the source before an unsafe walk.
    in.entity[7].claim=0;execute(in);
    in=initial();in.incoming_s6={1,0};refused(in,NBA97_SELECTION_INVALID);
    in=initial();in.controller[7].selected.known=2;refused(in,NBA97_SELECTION_INVALID);
    in=initial();in.entity_table[7]=11;refused(in,NBA97_SELECTION_INVALID);
    check(nba97_game_controller_selection(nullptr,&in)==NBA97_SELECTION_INVALID,"null out");
    Nba97GameSelectionEffects guard{};const auto before=guard;
    check(nba97_game_controller_selection(&guard,nullptr)==NBA97_SELECTION_INVALID &&
          !std::memcmp(&guard,&before,sizeof(guard)),"null input changed output");

    in=initial();in.controller[0].team_base=0;const auto expected=execute(in);
    alignas(Nba97GameSelectionInput) unsigned char storage[2048]{};
    for(unsigned input_offset=0;input_offset<=512;input_offset+=256)
        for(unsigned output_offset=0;output_offset<=512;output_offset+=256) {
            std::memcpy(storage+input_offset,&in,sizeof(in));
            auto* src=reinterpret_cast<Nba97GameSelectionInput*>(storage+input_offset);
            auto* dst=reinterpret_cast<Nba97GameSelectionEffects*>(storage+output_offset);
            check(nba97_game_controller_selection(dst,src)==NBA97_SELECTION_OK,"overlap refused");
            check(!std::memcmp(dst,&expected,sizeof(expected)),"overlap changed effects");
        }
    std::printf("GAME CONTROLLER SELECTION PASS: %u signed tail cases with free/claimed variants; source bugs, references, distance, guards and overlap\n",tail_cases);
}
int main() {try {tests();return 0;}catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}}
