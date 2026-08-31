#include "recovered/game_player_bindings.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned checks=0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
using Bytes=std::array<uint8_t,24>;
static Nba97GamePlayerBindingsInput fixture(const Bytes& byte9) {
    Nba97GamePlayerBindingsInput in{};in.player_byte9=byte9.data();in.player_count=byte9.size();
    for(unsigned side=0;side<2;++side) for(unsigned i=0;i<12;++i) {
        in.lineup[side][i]=static_cast<uint16_t>(i);
        in.player_reference[side][i]=static_cast<uint16_t>(side*12+i);
    }
    for(unsigned i=0;i<10;++i) {
        in.entity_table[i]=static_cast<uint8_t>(i);in.entity[i].binding_index=i;
        in.entity[i].side_byte=static_cast<uint8_t>(i<5?0:5);
        in.entity[i].opponent_slot=static_cast<uint16_t>((i+5)%10);
    }
    return in;
}
static Nba97GamePlayerBindingsEffects run(const Nba97GamePlayerBindingsInput& in,
        Nba97GamePlayerBindingsResult expected=NBA97_PLAYER_BINDINGS_READY) {
    struct Guarded { uint64_t prefix; Nba97GamePlayerBindingsEffects effect; uint64_t suffix; } out{};
    out.prefix=0x123456789abcdef0;out.suffix=0xfedcba9876543210;
    std::memset(&out.effect,0xa5,sizeof(out.effect));const auto before=in;
    CHECK(nba97_game_player_bindings(&out.effect,&in)==expected);
    CHECK(out.prefix==0x123456789abcdef0 && out.suffix==0xfedcba9876543210);
    CHECK(std::memcmp(&before,&in,sizeof(in))==0);return out.effect;
}
static void refuse(const Nba97GamePlayerBindingsInput& in,Nba97GamePlayerBindingsResult why) {
    Nba97GamePlayerBindingsEffects out;std::memset(&out,0x73,sizeof(out));const auto before=out;
    CHECK(nba97_game_player_bindings(&out,&in)==why);
    CHECK(std::memcmp(&out,&before,sizeof(out))==0);
}
static void ordinary() {
    Bytes bytes;bytes.fill(78);const auto in=fixture(bytes);const auto out=run(in);
    CHECK(out.visited_entities==10 && out.trap_table_slot==255 && out.tail_count==4);
    CHECK(out.first_6459c_fallback_byte==0x84);
    for(unsigned i=0;i<4;++i) {
        CHECK(out.tail[i].owner==(i<2?NBA97_BINDING_6459C:NBA97_BINDING_644FC));
        CHECK(out.tail[i].side_word==(i%2?5:0));
    }
    for(unsigned side=0;side<2;++side) for(unsigned i=0;i<12;++i) CHECK(out.inverse_lineup[side][i]==i);
    for(unsigned i=0;i<10;++i) {
        const auto& e=out.entity[i];CHECK(e.written==63);
        CHECK(out.status_reference[i]==(i<5?i:i+7));
        CHECK(out.player_reference[i]==(i<5?i:i+7));
        CHECK(e.status_reference==out.status_reference[i] && e.player_reference==out.player_reference[i]);
        CHECK(e.word38==i%5 && e.opponent_cc==(i+5)%10);
        CHECK(e.scale_c6==256 && e.inverse_c8==256);
    }
}
static void rawArithmeticAndTrapPrefix() {
    Bytes bytes;bytes.fill(78);
    for(unsigned height=0;height<256;++height) for(unsigned at=0;at<10;++at) {
        bytes.fill(78);bytes[at<5?at:at+7]=static_cast<uint8_t>(height);
        auto in=fixture(bytes);in.entity[at].side_byte=static_cast<uint8_t>(255-height);
        const auto out=run(in,height?NBA97_PLAYER_BINDINGS_READY:NBA97_PLAYER_BINDINGS_DIVIDE_TRAP);
        const auto& e=out.entity[at];
        CHECK(e.word38==static_cast<uint16_t>(at-(255-height)));
        CHECK(e.scale_c6==height*256/78);
        if(height) {CHECK(e.inverse_c8==19968/height && out.tail_count==4);}
        else {
            CHECK(out.trap_table_slot==at && out.visited_entities==10-at && out.tail_count==0);
            CHECK((e.written&31)==15); //+C8 absent; +CC may have been written by a prior opponent visit.
            for(unsigned i=0;i<10;++i) {
                CHECK((out.entity[i].written&31)==(i>at?31:i==at?15:0));
                const bool cc_written=((i+5)%10)>at;
                CHECK(bool(out.entity[i].written&NBA97_BINDING_OPPONENTCC)==cc_written);
                if(cc_written) CHECK(out.entity[i].opponent_cc==(i+5)%10);
            }
            // Both inverse maps finish before the first entity may trap.
            for(unsigned side=0;side<2;++side) for(unsigned i=0;i<12;++i) CHECK(out.inverse_lineup[side][i]==i);
        }
    }
    // Independent fixed ratio values around divisions and byte extremes.
    const unsigned raw[][3]={{1,3,19968},{77,252,259},{78,256,256},{79,259,252},{255,836,78}};
    for(const auto& r:raw) {
        bytes.fill(static_cast<uint8_t>(r[0]));const auto out=run(fixture(bytes));
        CHECK(out.entity[0].scale_c6==r[1] && out.entity[0].inverse_c8==r[2]);
    }
}
static void layoutsAndAliases() {
    Bytes bytes;bytes.fill(78);auto in=fixture(bytes);
    const uint16_t home[12]={11,7,3,4,0,5,0xffff,0x8000,0xfffe,11,3,11};
    const uint16_t expected[12]={4,0xffff,0xffff,10,3,5,0xffff,1,0xffff,0xffff,0xffff,11};
    std::memcpy(in.lineup[0],home,sizeof(home));auto out=run(in);
    CHECK(std::memcmp(out.inverse_lineup[0],expected,sizeof(expected))==0);
    CHECK(out.status_reference[0]==11 && out.status_reference[1]==7);
    CHECK(out.player_reference[0]==11 && out.player_reference[1]==7);
    // Record aliases do not merge distinct status slots.
    in.player_reference[0][11]=2;in.player_reference[0][7]=2;out=run(in);
    CHECK(out.player_reference[0]==2 && out.player_reference[1]==2);
    CHECK(out.status_reference[0]==11 && out.status_reference[1]==7);
    // Entry identity, actual word+0 and source table position are distinct.
    in=fixture(bytes);
    for(unsigned i=0;i<10;++i) {in.entity_table[i]=static_cast<uint8_t>(9-i);in.entity[i].binding_index=(i+3)%10;}
    out=run(in);
    for(unsigned i=0;i<10;++i) {
        const unsigned binding=(i+3)%10;
        CHECK(out.entity[i].player_reference==out.player_reference[binding]);
        CHECK(out.entity[i].word38==static_cast<uint16_t>(binding-(i<5?0:5)));
    }
    // All table entries can alias one entity. The other nine are untouched;
    // source does not demand a permutation or infer physical entity identity.
    in=fixture(bytes);for(auto& e:in.entity_table)e=3;in.entity[3].opponent_slot=7;
    out=run(in);CHECK(out.visited_entities==10 && out.entity[3].opponent_cc==3);
    for(unsigned i=0;i<10;++i) CHECK(out.entity[i].written==(i==3?63:0));
    // Repeated opponent destinations receive the last traversal write (slot0).
    in=fixture(bytes);for(auto& e:in.entity)e.opponent_slot=9;out=run(in);
    CHECK(out.entity[9].opponent_cc==0);
    for(unsigned i=0;i<9;++i) CHECK(!(out.entity[i].written&NBA97_BINDING_OPPONENTCC));
}
static void guardsAndOverlap() {
    Bytes bytes;bytes.fill(78);auto in=fixture(bytes);Nba97GamePlayerBindingsEffects out{};
    CHECK(nba97_game_player_bindings(nullptr,&in)==NBA97_PLAYER_BINDINGS_ARGUMENT);
    CHECK(nba97_game_player_bindings(&out,nullptr)==NBA97_PLAYER_BINDINGS_ARGUMENT);
    in.player_byte9=nullptr;refuse(in,NBA97_PLAYER_BINDINGS_ARGUMENT);
    for(unsigned slot:{12u,32767u,32768u,65535u}) {
        in=fixture(bytes);in.lineup[0][4]=static_cast<uint16_t>(slot);refuse(in,NBA97_PLAYER_BINDINGS_LINEUP);
    }
    for(unsigned slot:{12u,32767u}) {
        in=fixture(bytes);in.lineup[1][11]=static_cast<uint16_t>(slot);refuse(in,NBA97_PLAYER_BINDINGS_LINEUP);
    }
    in=fixture(bytes);in.player_reference[1][4]=24;refuse(in,NBA97_PLAYER_BINDINGS_PLAYER_REFERENCE);
    in=fixture(bytes);in.player_count=0;refuse(in,NBA97_PLAYER_BINDINGS_PLAYER_REFERENCE);
    in=fixture(bytes);in.entity_table[0]=10;refuse(in,NBA97_PLAYER_BINDINGS_ENTITY_REFERENCE);
    for(uint32_t id:{10u,65535u,0x80000000u,0xffffffffu}) {
        in=fixture(bytes);in.entity[0].binding_index=id;refuse(in,NBA97_PLAYER_BINDINGS_ENTITY_INDEX);
    }
    for(unsigned slot:{10u,32768u,65535u}) {
        in=fixture(bytes);in.entity[0].opponent_slot=static_cast<uint16_t>(slot);refuse(in,NBA97_PLAYER_BINDINGS_OPPONENT_REFERENCE);
    }
    // Unreferenced bench player pointers are not consumed. Neither are an
    // invalid opponent or later entity once the original earlier DIV traps.
    in=fixture(bytes);in.player_reference[0][11]=65535;run(in);
    bytes[16]=0;in.entity[9].opponent_slot=65535;in.entity[0].binding_index=0xffffffffu;
    const auto trapped=run(in,NBA97_PLAYER_BINDINGS_DIVIDE_TRAP);CHECK(trapped.trap_table_slot==9);
    bytes.fill(78);in=fixture(bytes);const auto expected=run(in);
    alignas(Nba97GamePlayerBindingsInput) std::array<uint8_t,1024> storage{};
    for(unsigned at:{0u,64u,256u,384u}) {
        storage.fill(0x97);std::memcpy(storage.data()+256,&in,sizeof(in));
        auto wanted=storage;std::memcpy(wanted.data()+at,&expected,sizeof(expected));
        CHECK(nba97_game_player_bindings(reinterpret_cast<Nba97GamePlayerBindingsEffects*>(storage.data()+at),
              reinterpret_cast<const Nba97GamePlayerBindingsInput*>(storage.data()+256))==NBA97_PLAYER_BINDINGS_READY);
        CHECK(storage==wanted);
    }
    // The bytes pool may overlap output: every consumed byte is read before publication.
    storage.fill(78);in.player_byte9=storage.data()+32;
    CHECK(nba97_game_player_bindings(reinterpret_cast<Nba97GamePlayerBindingsEffects*>(storage.data()),&in)==NBA97_PLAYER_BINDINGS_READY);
    CHECK(std::memcmp(storage.data(),&expected,sizeof(expected))==0);
}
int main() {
    ordinary();rawArithmeticAndTrapPrefix();layoutsAndAliases();guardsAndOverlap();
    std::printf("PLAYER BINDINGS PASS: %u checks; direct646A8 effects, aliases, ratios and source trap prefix; tail helpers not executed\n",checks);
}
