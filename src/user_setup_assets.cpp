#include "user_setup_assets.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace nba97 {
UserSetupAssets::UserSetupAssets(const std::filesystem::path& root):help_(root/"help.n97ui") {
    std::ifstream f(root/"ui.n97users",std::ios::binary|std::ios::ate);
    if(!f || f.tellg()<120 || f.tellg()>8192) throw std::runtime_error("missing/bounded User Setup pack; run tools/extract_user_setup.py");
    std::vector<uint8_t> b(static_cast<size_t>(f.tellg()));
    f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()));
    if(!f || !std::equal(b.begin(),b.begin()+4,"N97U")) throw std::runtime_error("invalid User Setup pack");
    size_t at=4;
    auto word=[&]() {if(at+2>b.size()) throw std::runtime_error("truncated User Setup word");
        uint16_t v=b[at]|(uint16_t(b[at+1])<<8);at+=2;return v;};
    auto text=[&](size_t count) {
        if(!count || count>127 || at+count>b.size()) throw std::runtime_error("invalid User Setup text");
        std::string s(b.begin()+at,b.begin()+at+count);at+=count;
        if(std::any_of(s.begin(),s.end(),[](unsigned char c){return c<32 || c>126;})) throw std::runtime_error("unsupported User Setup text");
        return s;
    };
    if(word()!=1 || word()!=35 || word()!=8 || word()!=68) throw std::runtime_error("unsupported User Setup version/count");
    const auto alphabet=text(68);std::copy(alphabet.begin(),alphabet.end(),alphabet_.begin());
    for(auto& color:colors_) {const uint32_t lo=word();color=lo|(uint32_t(word())<<16);}
    for(auto& v:assignments_) {if(at>=b.size() || b[at]>2) throw std::runtime_error("invalid initial assignment");v=b[at++];}
    player_format_=text(word());new_label_=text(word());
    const auto marker=player_format_.find("%d");
    if(marker==std::string::npos || player_format_.find('%',marker+2)!=std::string::npos ||
       player_format_.find('%')!=marker) throw std::runtime_error("unsupported player-label format");
    for(auto& item:layout_) {
        item.x=static_cast<int16_t>(word());item.y=static_cast<int16_t>(word());
        item.z=static_cast<int16_t>(word());item.flags=static_cast<int16_t>(word());item.tag=text(4);
        if(item.x<0 || item.x>511 || item.y<0 || item.y>239 || item.z<1 || item.z>17 || (item.flags&~9))
            throw std::runtime_error("invalid User Setup layout");
    }
    if(at!=b.size()) throw std::runtime_error("trailing User Setup data");
    (void)help_.descriptor(5,0);(void)help_.descriptor(5,1);
    loadDialogs(root/"dialogs.n97ui");
}
std::string UserSetupAssets::playerLabel(unsigned row) const {
    if(row>=8) throw std::runtime_error("invalid physical-controller visual row");
    auto text=player_format_;text.replace(text.find("%d"),2,std::to_string(row+1));return text;
}
void UserSetupAssets::loadDialogs(const std::filesystem::path& path) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f || f.tellg()<8 || f.tellg()>4096) throw std::runtime_error("missing/bounded User Setup dialogs; run extractor");
    std::vector<uint8_t> b(static_cast<size_t>(f.tellg()));f.seekg(0);
    f.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()));
    if(!f || !std::equal(b.begin(),b.begin()+4,"N97M") || b[4]!=1 || b[5] || b[6]!=4 || b[7])
        throw std::runtime_error("unsupported User Setup dialog pack");
    size_t at=8;
    auto byte=[&]() {if(at>=b.size()) throw std::runtime_error("truncated dialog pack");return b[at++];};
    auto half=[&]() {const unsigned low=byte();return low|(unsigned(byte())<<8);};
    const uint32_t addresses[]{0x800b008e,0x800b002c,0x800affac,0x800aff4c};
    for(unsigned i=0;i<4;++i) {
        const auto low=half();const uint32_t address=low|(uint32_t(half())<<16);
        const auto size=half();const size_t end=at+size;
        if(address!=addresses[i] || size<10 || end>b.size()) throw std::runtime_error("invalid dialog extent/address");
        auto& d=dialogs_[i];d.body.state=5;d.body.index=uint8_t(i);d.body.address=address;
        d.body.rect.x=static_cast<int16_t>(half());d.body.rect.y=static_cast<int16_t>(half());
        d.body.rect.width=static_cast<int16_t>(half());d.body.rect.height=byte();d.body.style=byte();
        const auto lines=byte(),choices=byte();
        if(d.body.style!=1 || lines!=3 || choices!=(i==3?2:0) || d.body.rect.x<0 ||
           d.body.rect.x>246 || d.body.rect.y<0 || d.body.rect.y>110 ||
           d.body.rect.width<20 || d.body.rect.x+d.body.rect.width>512 ||
           d.body.rect.height<10 || d.body.rect.y+d.body.rect.height>240)
            throw std::runtime_error("invalid dialog structure");
        for(unsigned row=0;row<unsigned(lines)+unsigned(choices);++row) {
            if(byte()!=1) throw std::runtime_error("invalid dialog alignment");
            std::string text;
            while(at<end && b[at]) {
                const auto c=byte();if(c<32 || c>126 || text.size()>=120) throw std::runtime_error("invalid dialog text");
                text+=char(c);
            }
            if(at>=end || byte()!=0) throw std::runtime_error("unterminated dialog text");
            for(size_t p=0;(p=text.find('%',p))!=std::string::npos;p+=2)
                if(p+1>=text.size() || text[p+1]!='s') throw std::runtime_error("unsupported dialog format");
            if(row<3)d.body.lines.push_back({true,0,text});
            else d.choices[row-3]=text;
        }
        if(at!=end) throw std::runtime_error("trailing dialog record");
        if(i==3)d.body.text_top=10;
    }
    const auto count=half();
    if(!count || count>127 || at+count+1!=b.size()) throw std::runtime_error("invalid continuation string");
    while(at+1<b.size()) {
        const auto c=byte();if(c<32 || c>126) throw std::runtime_error("invalid continuation glyph");
        continuation_+=char(c);
    }
    delete_preference_=byte();
}
Nba97HelpRect UserSetupAssets::dialogRect(UserSetupDialog kind) const {
    if(kind==UserSetupDialog::SaveFailure) return {111,85,290,100};
    const auto i=static_cast<unsigned>(kind);
    if(i<1 || i>4) throw std::runtime_error("invalid User Setup dialog");
    return dialogs_[i-1].body.rect;
}
void UserSetupAssets::drawDialog(PshImage& image,const PshFont& font,UserSetupDialog kind,
                                 const Nba97ResetPrompt& state,const std::string& name) const {
    if(name.size()>13) throw std::runtime_error("unbounded modal substitution");
    auto substitute=[&](std::string s) {
        size_t at=0;while((at=s.find("%s",at))!=std::string::npos) {s.replace(at,2,name);at+=name.size();}
        return s;
    };
    if(kind==UserSetupDialog::SaveFailure) {
        FrontendHelpDescriptor native{5,0,0,dialogRect(kind),
            {{true,0,"native save failed"},{true,0,"draft kept - not accepted"},{true,0,"see CLI then retry or cancel"},
             {true,0,continuation_,6}},1};
        FrontendHelpPack::draw(image,font,native,state.modal);return;
    }
    const auto& source=dialogs_.at(static_cast<unsigned>(kind)-1);auto body=source.body;
    for(auto& line:body.lines)line.encoded=substitute(line.encoded);
    if(kind!=UserSetupDialog::Delete) body.lines.push_back({true,0,continuation_,6});
    FrontendHelpPack::draw(image,font,body,state.modal);
    if(kind==UserSetupDialog::Delete && nba97_help_text_visible(&state.modal)) {
        draw_psh_text_centered(image,font,substitute(source.choices[0]),256,140,state.tint[0].rgb);
        draw_psh_text_centered(image,font,substitute(source.choices[1]),256,state.initial_choice ? 156:152,state.tint[1].rgb);
    }
}
}
