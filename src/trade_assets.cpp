#include "trade_assets.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>
namespace nba97 {
namespace {
std::vector<std::uint8_t> read(const std::filesystem::path& p) {
    std::ifstream in(p,std::ios::binary|std::ios::ate);
    if(!in || in.tellg()<8 || in.tellg()>16384) throw std::runtime_error("missing/invalid private Trade UI pack; run extract_trade_assets.py");
    std::vector<std::uint8_t> b(static_cast<std::size_t>(in.tellg()));in.seekg(0);
    if(!in.read(reinterpret_cast<char*>(b.data()),b.size())) throw std::runtime_error("truncated Trade UI pack");
    return b;
}
void text(PshImage& im,const PshFont& font,const std::string& value,int x,int y,const std::uint8_t* tint=nullptr) {
    for(char c:value) {
        if(c==' ') {x+=font.spaceWidth();continue;}
        const auto* g=font.glyph(c);if(!g) throw std::runtime_error("missing original Trade glyph");
        for(int yy=0;yy<g->height;++yy) for(int xx=0;xx<g->width;++xx) {
            int px=x+xx,py=y-g->center_y+yy;const auto src=(yy*g->width+xx)*4;
            if(px<0 || px>=512 || py<0 || py>=240 || !g->rgba[src+3]) continue;
            auto at=(py*512+px)*4;
            for(int k=0;k<3;++k) im.rgba[at+k]=std::uint8_t(std::min(255u,unsigned(g->rgba[src+k])*(tint?tint[k]:128)/128));
            im.rgba[at+3]=255;
        }
        x+=std::max(0,int(g->width)-font.kerning());
    }
}
std::string substitute(std::string value,const std::string& subject) {
    const auto p=value.find("%s");if(p!=std::string::npos) value.replace(p,2,subject);return value;
}
std::string header(std::string value,const std::string& team,int empty) {
    for(auto p=value.find("%c");p!=std::string::npos;p=value.find("%c")) value.erase(p,2);
    value=substitute(value,team);auto p=value.find("%d");
    if(p!=std::string::npos) value.replace(p,2,std::to_string(empty));return value;
}
}
TradeAssets::TradeAssets(const std::filesystem::path& root,bool sign):TradeAssets(read(root/(sign?"sign/ui.n97trade":"trade/ui.n97trade"))) {
    if(sign_!=sign) throw std::runtime_error("roster editor UI pack belongs to the wrong screen");
    font_=load_psh_font(root/"fonts/ZFONT0.PSH",10,1);
    small_=load_psh_font(root/"fonts/ZFONT1.PSH",10,1);
}
TradeAssets::TradeAssets(const std::vector<std::uint8_t>& b) {
    auto fail=[] {throw std::runtime_error("invalid Trade UI pack");};
    if(b.size()<8 || b.size()>16384 || !std::equal(b.begin(),b.begin()+4,"N97T")) fail();
    auto half=[&](std::size_t i){return unsigned(b[i])|(unsigned(b[i+1])<<8);};
    auto word=[&](std::size_t i){return std::uint32_t(half(i))|(std::uint32_t(half(i+2))<<16);};
    if(half(4)!=1 || (half(6)!=21 && half(6)!=25)) fail();
    sign_=half(6)==25;
    std::size_t at=8;bool have_preferences=false;
    for(unsigned r=0;r<half(6);++r) {
        if(at+8>b.size()) fail();auto addr=word(at),size=word(at+4);at+=8;
        if(!size || size>b.size()-at) fail();const auto end=at+size;
        auto string=[&]() {std::string s;while(at<end && b[at]) {
            if(b[at]<32 || b[at]>126 || s.size()>=256) fail();s+=char(b[at++]);}
            if(at>=end) fail();++at;return s;};
        if(addr==0x800265AC) {
            if(size!=25 || have_preferences) fail();have_preferences=true;
            for(auto& p:preference_) {p=b[at++];if(p>4) fail();}
        } else if(addr<0x800a0000) {
            if(text_.count(addr)) fail();text_[addr]=string();
        } else {
            if(size<10 || dialogs_.count(addr)) fail();
            const unsigned x=half(at),y=half(at+2),w=half(at+4),h=b[at+6],n=b[at+8],choices=b[at+9];
            if(x>246 || y>110 || w<20 || x+w>512 || h<10 || y+h>240 || b[at+7]!=1 ||
               !n || n>8 || (choices!=0 && choices!=2)) fail();
            Dialog d{{int16_t(x),int16_t(y),int16_t(w),int16_t(h)},{},{}};at+=10;
            for(unsigned i=0;i<n+choices;++i) {if(at>=end || b[at++]!=1) fail();(i<n?d.body:d.choices).push_back(string());}
            dialogs_[addr]=std::move(d);
        }
        if(at!=end) fail();
    }
    if(at!=b.size() || !have_preferences || text_.size()!=(sign_?15:14) || dialogs_.size()!=(sign_?9:6)) fail();
    if(sign_ && (!text_.count(0x8009D83A) || !dialogs_.count(0x800AED20) ||
        !dialogs_.count(0x800AEC72) || !dialogs_.count(0x800AED88))) fail();
    for(auto a:{0x8002655Cu,0x80026574u,0x80026588u,0x8002659Cu,0x80026508u,0x8002650Cu,
                0x80026510u,0x80026514u,0x80026518u,0x8002651Cu,0x8002502Cu,0x80024E60u,
                0x800264ECu,0x800264F8u}) if(!text_.count(a)) fail();
    for(auto a:{0x800AEBB2u,0x800AECBEu,0x800AFC22u,0x800AF4F8u,0x800AEE88u,0x800AEEF6u}) if(!dialogs_.count(a)) fail();
}
FrontendHelpDescriptor TradeAssets::notice(std::uint32_t address,const std::string& subject) const {
    const auto& d=dialogs_.at(address);FrontendHelpDescriptor result{};
    if(!d.choices.empty()) throw std::runtime_error("choice descriptor used as warning");
    result.state=sign_?14:13;result.address=address;result.rect=d.rect;result.style=1;
    for(const auto& line:d.body) {
        if(subject.empty() && line.find("%s")!=std::string::npos)
            throw std::runtime_error("original Trade notice requires a subject");
        result.lines.push_back({true,0,substitute(line,subject)});
    }
    result.lines.push_back({true,0,text_.at(0x8002502c),6});return result;
}
FrontendHelpDescriptor TradeAssets::emptyNotice(bool compare) const {
    // 54B94 passes an action name, not a player or team name, to40A1C.
    return notice(0x800AFC22,text_.at(compare?0x800264F8:0x800264EC));
}
Nba97HelpRect TradeAssets::rect(std::uint32_t address) const {return dialogs_.at(address).rect;}
void TradeAssets::drawChoice(PshImage& im,std::uint32_t address,const Nba97ResetPrompt& p) const {
    const auto& d=dialogs_.at(address);
    if(d.choices.size()!=2) throw std::runtime_error("Trade choice descriptor missing choices");
    auto m=p.modal;const bool visible=nba97_help_text_visible(&m)!=0;
    m.phase=nba97_help_visible(&m)?NBA97_HELP_GROWING:NBA97_HELP_CLOSED;
    FrontendHelpDescriptor bg{};bg.rect=d.rect;bg.style=1;
    FrontendHelpPack::draw(im,small_,bg,m);
    if(!visible) return;
    int y=d.rect.y+10;
    for(const auto& line:d.body) {text(im,small_,line,256-small_.textWidth(line)/2,y);y+=12;}
    y+=6;
    for(unsigned i=0;i<2;++i) {
        text(im,small_,d.choices[i],256-small_.textWidth(d.choices[i])/2,y,p.tint[i].rgb);
        y+=12;if(i!=p.initial_choice) y+=4;
    }
}
PshImage TradeAssets::labels(const Nba97TradeScreen& s,const RosterDatabase& db) const {
    PshImage im;im.width=512;im.height=240;im.rgba.assign(512*240*4,0);
    const std::uint8_t gold[]{120,102,0};
    for(int p=0;p<2;++p) {
        const auto* team=db.team(s.team[p]);
        const bool free=s.team[p]==29;
        if(!team && !(free && sign_)) throw std::runtime_error("editor team missing");
        const int empty=(free?100:15)-s.counts[s.team[p]];
        const auto name=free?text_.at(0x8009D83A):std::string(team->city);
        const auto value=header(text_.at(p?(empty?0x80026588:0x8002659c):(empty?0x8002655c:0x80026574)),name,empty);
        text(im,small_,value,p?462-small_.textWidth(value):50,85,(p==(s.phase==NBA97_TRADE_SECOND))?gold:nullptr);
        for(int row=0;row<6;++row) {
            const unsigned slot=s.top[p]+row;const auto id=s.working[s.team[p]*15+slot];
            const int x=p?270:60,y=112+row*16;const auto* tint=s.tint[p][slot].rgb;
            // 3CD74..3CD90 selects original empty label and x offset45.
            if(id==UINT16_MAX) {text(im,font_,header(text_.at(0x80024E60),"",0),x+45,y,tint);continue;}
            const auto* player=db.player(id);if(!player) throw std::runtime_error("Trade row player missing");
            const auto pos=text_.at(0x80026508+4*std::min<unsigned>(player->position,5));
            const auto num=player->jerseyNumberText();
            text(im,font_,pos,x+28-font_.textWidth(pos),y,tint);
            text(im,font_,num,x+58-font_.textWidth(num),y,tint);
            text(im,font_,player->last_name,x+66,y,tint);
        }
    }
    return im;
}
}
