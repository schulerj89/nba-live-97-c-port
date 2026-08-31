#include "recovered/game_player_attributes.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static unsigned checks;
#define CHECK(x) do { ++checks;if(!(x)){std::fprintf(stderr,"line%d: %s\n",__LINE__,#x);std::exit(1);} } while(0)
using Players=std::array<Nba97GameAttributePlayer,24>;
static Nba97GamePlayerAttributesInput fixture(const Players& players) {
    Nba97GamePlayerAttributesInput in{};in.players=players.data();in.player_count=players.size();
    in.first_known=in.divisor_known=in.flag_known=1;in.divisor64=10;
    for(unsigned i=0;i<11;++i)in.entity[i]={i,static_cast<uint16_t>(i),1,1};
    return in;
}
static Players players() {Players p{};for(auto& x:p)x={78,50,50,50,50,50,50};return p;}
static void normal_and_aliases() {
    auto p=players();auto in=fixture(p);Nba97GamePlayerAttributesEffects out{};
    CHECK(nba97_game_player_attributes(&out,&in)==NBA97_ATTRIBUTES_COMPLETE);
    CHECK(out.visited_entities==10 && out.stopped_entity==255 && out.height_written==1023 && !out.tail_count);
    for(unsigned i=0;i<10;++i) {
        CHECK(out.height165f48[i]==48672 && out.entity[i].written==63);
        const uint16_t expected[]={400,68,56,0,0,162};CHECK(std::memcmp(out.entity[i].field,expected,sizeof(expected))==0);
    }
    CHECK(out.entity[10].written==0);
    // Source consumes raw word00, including wrapped high bits; it is not a generated ID.
    for(unsigned i=0;i<11;++i)in.entity[i].word00=0x80000000;
    p[9].byte09=255;CHECK(nba97_game_player_attributes(&out,&in)==1);
    CHECK(out.height_written==1 && out.height165f48[0]==159120);
    in.first_entity=1;p[10].byte09=1;CHECK(nba97_game_player_attributes(&out,&in)==1);
    CHECK(out.entity[0].written==0 && out.entity[10].written==63 && out.height165f48[0]==624);
}
static void traps_and_signed_arithmetic() {
    for(uint8_t rating: {uint8_t(23),uint8_t(24),uint8_t(25)})for(unsigned entity=0;entity<10;++entity) {
        auto p=players();p[entity].byte1e=rating;auto in=fixture(p);Nba97GamePlayerAttributesEffects out{};
        CHECK(nba97_game_player_attributes(&out,&in)==NBA97_ATTRIBUTES_RATING_DIVIDE_TRAP);
        CHECK(out.stopped_entity==entity && out.visited_entities==entity+1 && !out.tail_count);
        CHECK(out.entity[entity].written==1 && out.entity[entity].field[0]==400);
        if(entity)CHECK(out.entity[entity-1].written==63);
    }
    auto p=players();auto in=fixture(p);Nba97GamePlayerAttributesEffects out{};in.divisor64=0;
    CHECK(nba97_game_player_attributes(&out,&in)==NBA97_ATTRIBUTES_RATE_DIVIDE_TRAP);
    CHECK(out.height_written==1 && out.entity[0].written==1 && out.visited_entities==1);
    in.divisor64=0xffffffff;p[0].byte1e=22;p[0].byte1b=100;p[0].byte14=255;p[0].byte1c=0;p[0].byte15=255;
    CHECK(nba97_game_player_attributes(&out,&in)==1);
    CHECK(out.entity[0].field[NBA97_ATTRIBUTE_44]==19656);
    CHECK(out.entity[0].field[NBA97_ATTRIBUTE_3C]==43); // Arithmetic shift(-1,1)=-1, not C truncating/2.
    CHECK(out.entity[0].field[NBA97_ATTRIBUTE_3E]==uint16_t(-46));
    CHECK(out.entity[0].field[NBA97_ATTRIBUTE_40]==uint16_t(-15));
    CHECK(out.entity[0].field[NBA97_ATTRIBUTE_42]==65);
}
static void guards_and_tails() {
    auto p=players();auto in=fixture(p);Nba97GamePlayerAttributesEffects out{};
    in.flag21498=0x8000;CHECK(nba97_game_player_attributes(&out,&in)==NBA97_ATTRIBUTES_TAILS_REQUIRED);
    CHECK(out.tail_count==3 && out.tail[0]==NBA97_ATTRIBUTE_4D9EC && out.tail[1]==NBA97_ATTRIBUTE_35A44 && out.tail[2]==NBA97_ATTRIBUTE_38A18);
    in.flag21498=0;in.flag_known=0;CHECK(nba97_game_player_attributes(&out,&in)==NBA97_ATTRIBUTES_UNRESOLVED);
    CHECK(out.visited_entities==10 && out.entity[9].written==63 && !out.tail_count);
    in=fixture(p);in.divisor64=0;in.divisor_known=0;CHECK(nba97_game_player_attributes(&out,&in)==NBA97_ATTRIBUTES_UNRESOLVED);
    CHECK(out.entity[0].written==1 && out.height_written==1);
    p[0].byte1e=24;CHECK(nba97_game_player_attributes(&out,&in)==NBA97_ATTRIBUTES_RATING_DIVIDE_TRAP); // Trap precedes unknown-rate read.
    p=players();in=fixture(p);in.entity[4].player_known=0;in.entity[4].player_reference=0;
    CHECK(nba97_game_player_attributes(&out,&in)==NBA97_ATTRIBUTES_UNRESOLVED);
    CHECK(out.visited_entities==4 && out.entity[3].written==63 && !out.entity[4].written);
    in=fixture(p);in.entity[4].word00=11;CHECK(nba97_game_player_attributes(&out,&in)==NBA97_ATTRIBUTES_REFERENCE);
    CHECK(out.height_written==15 && out.visited_entities==4);
    in=fixture(p);in.first_entity=2;CHECK(nba97_game_player_attributes(&out,&in)==NBA97_ATTRIBUTES_REFERENCE);
    CHECK(out.visited_entities==9 && out.stopped_entity==11 && out.entity[10].written==63);
    in=fixture(p);in.first_known=0;CHECK(nba97_game_player_attributes(&out,&in)==NBA97_ATTRIBUTES_UNRESOLVED);
    CHECK(!out.visited_entities && !out.height_written);
    std::memset(&out,0x73,sizeof(out));const auto before=out;in.first_known=2;
    CHECK(nba97_game_player_attributes(&out,&in)==NBA97_ATTRIBUTES_ARGUMENT);
    CHECK(std::memcmp(&out,&before,sizeof(out))==0);
}
int main(){normal_and_aliases();traps_and_signed_arithmetic();guards_and_tails();std::printf("game_player_attributes: %u checks passed\n",checks);}
