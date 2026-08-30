#include "recovered/user_setup.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line%d: %s\n",__LINE__,#x);std::exit(1);}}while(0)

static Nba97UserSetup fresh() {
    Nba97UserSetup s{};const uint8_t a[8]={};const int8_t p[8]={-2,-2,-2,-2,-2,-2,-2,-2};
    CHECK(nba97_user_setup_open(&s,a,p));return s;
}
static void editor_tests() {
    // Synthetic alphabet/measurement isolate control semantics from private art.
    const char letters[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._!@?";
    static_assert(sizeof(letters)==69);
    auto width=[](void* context,const char* text) {
        return context ? *static_cast<int*>(context):int(std::strlen(text))*9;
    };
    Nba97UserNames names{};auto s=fresh();s.side[0]=0;s.profile[0]=-1;
    CHECK(nba97_user_setup_edit_begin(&s,0,&names)==NBA97_USER_EDITOR_UPDATE);
    CHECK(s.profile[0]==0 && !std::strcmp(s.draft[0],"A") && s.sound==6 && !s.existing[0]);
    auto input=[&](uint16_t token,void* context=nullptr) {
        return nba97_user_setup_edit_input(&s,0,token,&names,letters,width,context);
    };
    CHECK(input(0x40)==NBA97_USER_NONE && s.cursor[0]==0);
    for(unsigned i=1;i<=68;++i) {
        CHECK(input(1)==NBA97_USER_EDITOR_UPDATE && s.draft[0][0]==letters[i%68] && s.sound==3);
    }
    CHECK(input(2)==NBA97_USER_EDITOR_UPDATE && s.draft[0][0]=='?' && s.sound==4);
    CHECK(input(0x10)==NBA97_USER_EDITOR_UPDATE && !std::strcmp(s.draft[0],"A"));
    input(1);input(0x800);input(2);
    CHECK(!std::strcmp(s.draft[0],"BA") && s.cursor[0]==1);
    input(8);input(1);input(4);input(0x10);
    CHECK(!std::strcmp(s.draft[0],"C") && s.cursor[0]==0);
    input(0x800);input(0x40);
    CHECK(!std::strcmp(s.draft[0],"C") && s.cursor[0]==0);
    CHECK(input(0x20)==NBA97_USER_HELP);
    std::strcpy(names.name[19],"C");
    CHECK(input(0x80)==NBA97_USER_NAME_DUPLICATE && s.alphabet[0]>=0);
    std::strcpy(names.name[19],"c"); // Equality is case-sensitive.
    CHECK(input(0x80)==NBA97_USER_SAVE_REQUEST && s.alphabet[0]>=0);
    CHECK(nba97_user_setup_edit_accept(&s,0)==NBA97_USER_SAVED && s.alphabet[0]==-1 && s.start_latch==1);
    std::strcpy(names.name[0],"Legacy Name");s.profile[0]=0;
    CHECK(nba97_user_setup_edit_begin(&s,0,&names)==NBA97_USER_EDITOR_UPDATE && s.existing[0]);
    CHECK(!std::strcmp(s.draft[0],"Legacy Name"));
    int measured=96;std::strcpy(s.draft[0],"A");s.cursor[0]=0;
    input(0x800,&measured);CHECK(!std::strcmp(s.draft[0],"A") && !s.sound);
    measured=95;input(0x800,&measured);CHECK(!std::strcmp(s.draft[0],"AA") && s.cursor[0]==1);
    std::strcpy(s.draft[0],"AAAAAAAAAAAAA");s.cursor[0]=0;
    input(0x800);CHECK(s.cursor[0]==0 && std::strlen(s.draft[0])==13);
    measured=105;input(1,&measured);CHECK(std::strlen(s.draft[0])==13);
    measured=106;input(1,&measured);CHECK(!std::strcmp(s.draft[0],"C"));
    // No unsupported token edits; source cursor repair/unused byte14 copy are
    // intentionally outside the bounded portable domain.
    for(uint16_t token:{uint16_t(0),uint16_t(0x200),uint16_t(0x880),uint16_t(0x1000)}) {
        auto before=s;input(token);CHECK(!std::strcmp(s.draft[0],before.draft[0]) && s.cursor[0]==before.cursor[0]);
    }
    s.draft[0][0]=0;CHECK(input(0x800)==NBA97_USER_NONE);
    std::strcpy(s.draft[0],"A");s.cursor[0]=1;CHECK(input(1)==NBA97_USER_NONE);
    for(unsigned i=0;i<20;++i) std::snprintf(names.name[i],14,"Profile%u",i);
    s=fresh();s.profile[0]=-1;s.side[0]=0;
    CHECK(nba97_user_setup_edit_begin(&s,0,&names)==NBA97_USER_PROFILE_FULL && s.profile[0]==-1);
    names.name[7][0]=0;s.profile[1]=7;s.side[1]=0;
    CHECK(nba97_user_setup_edit_begin(&s,0,&names)==NBA97_USER_PROFILE_FULL);
    s.side[1]=1;
    CHECK(nba97_user_setup_edit_begin(&s,0,&names)==NBA97_USER_EDITOR_UPDATE && s.profile[0]==7);
    s.profile[1]=8; // Delete's successor excludes even neutral claims.
    CHECK(nba97_user_setup_deleted(&s,0,&names)==NBA97_USER_DELETED && s.profile[0]==9);
}
int main() {
    editor_tests();
    Nba97UserNames names{};std::strcpy(names.name[0],"Alpha");std::strcpy(names.name[3],"Beta");
    std::strcpy(names.name[19],"Gamma");
    const uint8_t masks[4]={0x11,0x1f,0xf1,0xff};
    for(unsigned topology=0;topology<4;++topology) {
        CHECK(nba97_user_setup_topology_mask(topology)==masks[topology]);
        CHECK(nba97_user_setup_physical(topology,nba97_user_setup_row_count(topology))==-1);
    }
    CHECK(nba97_user_setup_physical(0,1)==4);
    CHECK(nba97_user_setup_topology_mask(4)==0);
    // Exhaust every legal assignment vector through roundtrip confirmation.
    // Capacity is an input-move gate, not an invented constructor restriction.
    for(unsigned encoded=0;encoded<6561;++encoded) {
        auto s=fresh();unsigned n=encoded;uint8_t a[8];int8_t p[8];std::fill_n(p,8,int8_t(-2));
        for(auto& v:a) {v=static_cast<uint8_t>(n%3);n/=3;}
        CHECK(nba97_user_setup_open(&s,a,p));
        uint16_t raw[8]={0x80};CHECK(nba97_user_setup_global(&s,raw,1)==NBA97_USER_CONFIRMED);
        CHECK(!std::memcmp(a,s.assignment,8) && s.result==6 && s.sound==9);
    }
    auto s=fresh();auto before=s;uint8_t bad[8]={3};int8_t no_profile[8];std::fill_n(no_profile,8,int8_t(-2));
    CHECK(!nba97_user_setup_open(&s,bad,no_profile));CHECK(!std::memcmp(&s,&before,sizeof(s)));
    CHECK(nba97_user_setup_input(&s,0,8,&names)==NBA97_USER_SIDE);
    CHECK(s.side[0]==0 && s.sound==2);
    CHECK(nba97_user_setup_input(&s,0,8,&names)==NBA97_USER_NONE && s.side[0]==0);
    CHECK(nba97_user_setup_input(&s,0,4,&names)==NBA97_USER_SIDE && s.side[0]==1 && s.hide_marker[0]);
    CHECK(nba97_user_setup_input(&s,0,0,&names)==NBA97_USER_NONE && !s.hide_marker[0]);
    CHECK(nba97_user_setup_input(&s,0,4,&names)==NBA97_USER_SIDE && s.side[0]==2);
    CHECK(nba97_user_setup_input(&s,0,4,&names)==NBA97_USER_NONE);
    // Neutral -> team capacity5; no underflow/wrap and no assignment publication.
    s=fresh();for(unsigned i=1;i<=5;++i) s.side[i]=2;
    before=s;CHECK(nba97_user_setup_input(&s,0,4,&names)==NBA97_USER_CAPACITY);
    CHECK(!std::memcmp(s.side,before.side,8) && !std::memcmp(s.assignment,before.assignment,8));
    // Profile ring skips empty and active claims, includes both sentinels.
    s=fresh();s.side[0]=2;s.side[1]=0;s.profile[1]=3;
    const int up[]={-1,0,19,-2},down[]={19,0,-1,-2};
    for(int p:up) {CHECK(nba97_user_setup_input(&s,0,1,&names)==NBA97_USER_PROFILE);CHECK(s.profile[0]==p);}
    for(int p:down) {CHECK(nba97_user_setup_input(&s,0,2,&names)==NBA97_USER_PROFILE);CHECK(s.profile[0]==p);}
    s.profile[0]=0;s.side[1]=1; // Neutral claim does not exclude profile cycling.
    nba97_user_setup_input(&s,0,1,&names);CHECK(s.profile[0]==3);
    s.side[0]=1;nba97_user_setup_input(&s,0,4,&names); // Joining DOES inspect neutral claims.
    CHECK(s.profile[0]==-2);
    // Readiness matrix: active-new and any editor block; neutral-new does not.
    for(unsigned controller=0;controller<8;++controller) {
        uint16_t raw[8]={0x80};
        s=fresh();s.profile[controller]=-1;
        CHECK(nba97_user_setup_global(&s,raw,1)==NBA97_USER_CONFIRMED);
        s=fresh();s.profile[controller]=-1;s.side[controller]=0;
        CHECK(nba97_user_setup_global(&s,raw,1)==NBA97_USER_REFUSED && !s.result);
        s=fresh();s.alphabet[controller]=0;
        CHECK(nba97_user_setup_global(&s,raw,1)==NBA97_USER_REFUSED);
    }
    uint16_t raw[8]={0x80,4};s=fresh();
    CHECK(nba97_user_setup_global(&s,raw,3)==NBA97_USER_NONE); // Combined Start+Right is not exact80.
    s.start_latch=1;raw[1]=0;CHECK(nba97_user_setup_global(&s,raw,1)==NBA97_USER_NONE);
    raw[0]=0;CHECK(nba97_user_setup_global(&s,raw,1)==NBA97_USER_NONE && !s.start_latch);
    raw[0]=0x80;CHECK(nba97_user_setup_global(&s,raw,1)==NBA97_USER_CONFIRMED);
    // Select discards local sides; clears selectors/editor but retains accepted assignments.
    s=fresh();s.side[0]=2;s.profile[0]=0;s.alphabet[0]=3;s.cursor[0]=5;
    raw[0]=0x100;CHECK(nba97_user_setup_global(&s,raw,1)==NBA97_USER_CANCELLED);
    CHECK(s.result==-1 && s.assignment[0]==0 && s.profile[0]==-2 && s.alphabet[0]==-1 && !s.cursor[0] && s.cancel_origin==8);
    s=fresh();s.side[4]=2;s.profile[4]=-1;raw[0]=0x80;
    CHECK(nba97_user_setup_global(&s,raw,1)==NBA97_USER_REFUSED); // Before timed disconnection.
    CHECK(nba97_user_setup_connections(&s,1,0));CHECK(s.side[4]==1 && s.profile[4]==-2);
    CHECK(nba97_user_setup_global(&s,raw,1)==NBA97_USER_CONFIRMED);
    // Pending operations remain requests: no fake edits/deletion or FF bypass.
    s=fresh();s.side[0]=2;s.profile[0]=-1;
    CHECK(nba97_user_setup_input(&s,0,0x800,&names)==NBA97_USER_EDIT_REQUEST && s.profile[0]==-1);
    s.profile[0]=0;CHECK(nba97_user_setup_input(&s,0,0x10,&names)==NBA97_USER_DELETE_REQUEST);
    CHECK(names.name[0][0]=='A' && s.profile[0]==0);
    Nba97UserRepeat repeat{};
    CHECK(nba97_user_setup_repeat(&repeat,8,100)==8);
    CHECK(!nba97_user_setup_repeat(&repeat,8,159));
    CHECK(nba97_user_setup_repeat(&repeat,8,160)==8);
    CHECK(!nba97_user_setup_repeat(&repeat,8,171));
    CHECK(nba97_user_setup_repeat(&repeat,8,172)==8);
    CHECK(nba97_user_setup_repeat(&repeat,4,173)==4);
    CHECK(nba97_user_setup_repeat(&repeat,4,173)==4); // Non-increasing original clock quirk.
    CHECK(!nba97_user_setup_repeat(&repeat,0,174));
    CHECK(repeat.remaining==11 && !repeat.last);
    CHECK(nba97_user_setup_repeat(&repeat,4,175)==4);
    CHECK(!nba97_user_setup_repeat(&repeat,0,1000) && repeat.remaining==-1);
    repeat={0,INT32_MIN,0};
    CHECK(!nba97_user_setup_repeat(&repeat,0,INT32_MAX) && repeat.remaining==1);
    std::puts("USER SETUP PASS:6561 assignment roundtrips; topology/capacity/readiness/repeat; editor alphabet, width boundaries, cursor, duplicate, accept, full and delete successor");
}
