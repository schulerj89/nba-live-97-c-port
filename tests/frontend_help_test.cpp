#include "frontend_help.hpp"
#include "recovered/reorder_screen.h"
#include <array>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void pass(const char* name) { std::cout << "HELP PASS " << name << '\n'; }
std::vector<std::uint8_t> fixture() {
    std::vector<std::uint8_t> b{'N','9','7','H',1,0,4,0};
    auto half=[&](unsigned v) { b.push_back(v&255); b.push_back((v>>8)&255); };
    const unsigned states[]={12,12,35,36};
    for (int i=0;i<4;++i) {
        b.push_back(static_cast<std::uint8_t>(states[i])); b.push_back(i==1 ? 1 : 0);
        half(17); half(0); half(0x8000); // Synthetic descriptor address, no asset text.
        half(121); half(70); half(270); b.insert(b.end(),{140,0,1,0});
        b.insert(b.end(),{0,21,'A',0x1f,18,'B',0});
    }
    return b;
}
void packTests() {
    const auto bytes=fixture();
    nba97::FrontendHelpPack pack(bytes);
    const auto& d=pack.descriptor(12,1);
    check(d.rect.width==270 && d.lines.size()==1 && !d.lines[0].centered &&
          d.lines[0].offset==21 && d.lines[0].encoded==std::string("A\x1f\x12" "B"), "encoded line preservation");
    pass("pack_routes_and_inline_spacing");
    auto sign=bytes;sign[8]=14;sign[33]=14;
    nba97::FrontendHelpPack signPack(sign);
    check(signPack.descriptor(14,0).rect.width==270&&signPack.descriptor(14,1).rect.height==140,
        "Sign Help routes accepted");
    for (std::size_t n=0;n<bytes.size();++n) {
        bool rejected=false;
        try { nba97::FrontendHelpPack truncated(std::vector<std::uint8_t>(bytes.begin(),bytes.begin()+n)); }
        catch(const std::runtime_error&) { rejected=true; }
        check(rejected,"truncated pack accepted");
    }
    pass("every_truncation_rejected");
    auto bad=[&](std::vector<std::uint8_t> b) {
        bool rejected=false;
        try { nba97::FrontendHelpPack invalid(b); } catch(const std::runtime_error&) { rejected=true; }
        check(rejected,"malformed pack accepted");
    };
    auto b=bytes; b[4]=2; bad(b);
    auto release=bytes;release.erase(release.begin()+33,release.begin()+58);release[6]=3;release[8]=17;
    nba97::FrontendHelpPack releasePack(release);
    check(releasePack.descriptor(17,0).rect.width==270,"Release single-stage Help accepted");
    b=release;b[9]=1;bad(b);
    b=release;b[8]=14;bad(b);
    b=bytes; b[6]=5; bad(b);
    b=bytes; b.push_back(0); bad(b);
    b=bytes; b.resize(16385); bad(b);
    b=bytes; b[16]=255; b[17]=255; bad(b);
    b=bytes; b[23]=1; bad(b); // Warning style is not a Help descriptor.
    b=bytes; b[25]=1; bad(b); // Choices unsupported in this bounded specialization.
    b=bytes; b[26]=2; bad(b);
    b=bytes; b[29]=0x1e; bad(b);
    b=bytes; b[31]=0x1f; bad(b); // No spacing operand before terminator.
    b=bytes; b[34]=0; bad(b); // Duplicate state/index.
    pass("pack_bounds_version_duplicate_and_control_rejection");
    Nba97HelpModal m{};
    check(nba97_help_open(&m,d.rect,0x20)==NBA97_HELP_OPEN_SOUND,"start background");
    PshImage image; image.width=512; image.height=240; image.rgba.assign(512*240*4,0);
    pack.draw(image,nba97::PshFont{},d,m);
    std::size_t count=0;
    for (std::size_t i=3;i<image.rgba.size();i+=4) if(image.rgba[i]) ++count;
    check(count==200 && image.rgba[(110*512+246)*4+1]==20,"initial modal bounds/colors");
    pass("small_green_modal_not_fullscreen");
}

void stateTests() {
    const std::array<Nba97HelpRect,4> targets{{{121,70,270,140},{121,85,270,110},
                                           {130,75,250,130},{130,60,250,140}}};
    const int counts[]={17,14,15,17};
    for (std::size_t i=0;i<targets.size();++i) {
        Nba97HelpModal m{};
        check(nba97_help_open(&m,targets[i],0x20)==NBA97_HELP_OPEN_SOUND,"open sound");
        check(nba97_help_open(&m,targets[i],0x20)==NBA97_HELP_NO_EVENT,"reopen while active");
        check(nba97_help_input(&m,0x800)==NBA97_HELP_NO_EVENT,"input during growth");
        int frames=0;
        while(m.phase==NBA97_HELP_GROWING) {
            nba97_help_tick(&m,0x20); ++frames;
            check(frames<40,"growth stuck");
        }
        check(frames==counts[i] && nba97_help_text_visible(&m),"growth clamping/frame count");
        for (int j=0;j<20;++j) check(nba97_help_tick(&m,0x20)==NBA97_HELP_NO_EVENT,"held opener dismissed");
        check(m.phase==NBA97_HELP_WAIT_CHANGE,"held opener phase");
        check(nba97_help_input(&m,0)==NBA97_HELP_NO_EVENT && m.phase==NBA97_HELP_READY,"release must arm not dismiss");
        check(nba97_help_input(&m,0x800)==NBA97_HELP_CLOSE_SOUND && !nba97_help_text_visible(&m),"fresh press close/text removed");
        frames=0;
        while(m.phase==NBA97_HELP_SHRINKING) { nba97_help_tick(&m,0x800); ++frames; }
        check(frames==counts[i] && !nba97_help_visible(&m),"shrink timing/end bounds");
        for (int j=0;j<20;++j) check(nba97_help_tick(&m,0x800)==NBA97_HELP_NO_EVENT,"closing input leaked");
        check(m.phase==NBA97_HELP_RETURN_BARRIER,"closing barrier phase");
        check(nba97_help_tick(&m,0)==NBA97_HELP_RETURNED && m.phase==NBA97_HELP_CLOSED,"return after input changed");
    }
    pass("four_original_geometries_growth_shrink_counts");
    pass("held_opener_release_and_close_barrier");
    Nba97HelpModal m{};
    nba97_help_open(&m,targets[0],0x20);
    for(int i=0;i<17;++i) nba97_help_tick(&m,0x20);
    check(nba97_help_input(&m,0x10)==NBA97_HELP_CLOSE_SOUND,"change directly to another button must dismiss");
    for(int i=0;i<17;++i) nba97_help_tick(&m,0x10);
    check(nba97_help_input(&m,0x100)==NBA97_HELP_RETURNED,"return uses mask change, not mandatory release");
    pass("original_input_change_not_timeout");
    m={};
    check(nba97_help_open(&m,{250,70,20,20},0x20)==NBA97_HELP_NO_EVENT && m.phase==NBA97_HELP_CLOSED,"invalid geometry");
    check(nba97_help_open(&m,targets[0],0)==NBA97_HELP_NO_EVENT,"invalid invoking mask");
    pass("invalid_open_leaves_state_untouched");
}

void roundTrips() {
    std::array<std::uint16_t,NBA97_ROSTER_TABLE_SLOTS> table;
    for(std::size_t i=0;i<table.size();++i) table[i]=static_cast<std::uint16_t>(i);
    Nba97ReorderScreen s{};
    check(nba97_reorder_screen_enter(&s,table.data(),0,0,nullptr,nullptr,nullptr,0)!=0,"parent entry");
    // Dirty both team 0 and team 1 before opening either Help page. This catches
    // accidentally re-entering the screen/accepting/resetting its whole draft.
    auto swap=[&] {
        nba97_reorder_screen_input(&s,NBA97_REORDER_SELECT);
        nba97_reorder_screen_input(&s,NBA97_REORDER_DOWN);
        nba97_reorder_screen_input(&s,NBA97_REORDER_SELECT);
    };
    swap(); nba97_reorder_screen_scan(&s,1); swap();
    for(int stage=0;stage<2;++stage) {
        if(stage) {
            nba97_reorder_screen_input(&s,NBA97_REORDER_SELECT);
            for(int i=0;i<7;++i) nba97_reorder_screen_input(&s,NBA97_REORDER_DOWN);
        }
        check(s.selection.descriptor_page==stage,"Help index follows descriptor page");
        const auto before=s;
        Nba97HelpModal m{};
        nba97_help_open(&m,stage ? Nba97HelpRect{121,85,270,110} : Nba97HelpRect{121,70,270,140},0x20);
        for(int i=0;i<40;++i) nba97_help_tick(&m,0);
        nba97_help_input(&m,0x100);
        for(int i=0;i<40;++i) nba97_help_tick(&m,0);
        check(m.phase==NBA97_HELP_CLOSED && std::memcmp(&s,&before,sizeof(s))==0,"Help changed parent/draft");
        check(std::equal(table.begin(),table.end(),s.snapshot) && s.selection.changes==2,"lost multi-team edit baseline");
    }
    pass("both_stages_dirty_multi_team_draft_preserved");
}
}
int main(int argc,char** argv) {
    try {
        packTests(); stateTests(); roundTrips();
        if(argc==2) {
            const std::filesystem::path root=argv[1];
            nba97::FrontendHelpPack pack(root/"reorder/help.n97ui");
            const auto font=nba97::load_psh_font(root/"fonts/ZFONT1.PSH",10,1);
            for(const auto route : std::array<std::array<std::uint8_t,2>,4>{{{{12,0}},{{12,1}},{{35,0}},{{36,0}}}}) {
                const auto& d=pack.descriptor(route[0],route[1]);
                Nba97HelpModal m{}; nba97_help_open(&m,d.rect,0x20);
                for(int i=0;i<40;++i) nba97_help_tick(&m,0);
                PshImage image; image.width=512; image.height=240; image.rgba.assign(512*240*4,0);
                pack.draw(image,font,d,m); // Fails on missing original control or text glyphs.
            }
            pass("private_four_descriptors_real_font_and_icons");
        }
        return 0;
    } catch(const std::exception& e) { std::cerr<<"HELP FAIL "<<e.what()<<'\n'; return 1; }
}
