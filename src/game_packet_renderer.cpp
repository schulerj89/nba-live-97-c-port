#include "game_packet_renderer.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace nba97 {
namespace {
using Result=GamePacketResult;
int signed11(std::uint32_t v){v&=2047;return v<1024?int(v):int(v)-2048;}
int clamp(int v,int upper){return std::max(0,std::min(upper,v));}
std::array<int,3> rgb(std::uint32_t word){return {int(word&255),int((word>>8)&255),int((word>>16)&255)};}
std::int64_t edge(int ax,int ay,int bx,int by,int x,int y){return std::int64_t(bx-ax)*(y-ay)-std::int64_t(by-ay)*(x-ax);}
bool included(int ax,int ay,int bx,int by){return by<ay||(by==ay&&bx>ax);}
// The sum is halved once. Halving two odd operands separately loses a bit.
int average(int a,int b){return (a+b)>>1;}
}
GamePacketRenderer::GamePacketRenderer(GameVramWords& vram):vram_(vram),written_(GameVramWords::Width*GameVramWords::Height){}
void GamePacketRenderer::begin(GameDrawProgress& p){p={};progress_=&p;used_=needed_=0;std::fill(written_.begin(),written_.end(),std::uint8_t{0});}
Result GamePacketRenderer::ready()const{
    if((state.known&63)!=63||!state.display.known)return Result::UnknownState;
    if(state.display.field>1)return Result::Argument;
    // This backend targets retail1MiB VRAM. Enhanced/2MiB modes must not wrap
    // into the other texture bank and accidentally look successful.
    if(state.mode&0x800)return Result::UnsupportedMode;
    if(state.display.enabled&&(state.display.x<0||state.display.y<0||state.display.x>=1024||state.display.y>=512||
       state.display.width<1||state.display.height<1||state.display.width>1024-state.display.x||
       state.display.height>512-state.display.y))return Result::Argument;
    return Result::Complete;
}
bool GamePacketRenderer::clipped(int x,int y)const{
    if(x<0||y<0||x>=1024||y>=512)return true;
    if(x<int(state.top_left&1023)||y<int((state.top_left>>10)&511)||
       x>int(state.bottom_right&1023)||y>int((state.bottom_right>>10)&511))return true;
    if(state.display.interlaced&&unsigned(y&1)==state.display.field)return true;
    const auto& d=state.display;
    return d.enabled&&!(state.mode&1024)&&x>=d.x&&y>=d.y&&x<d.x+d.width&&y<d.y+d.height;
}
Result GamePacketRenderer::sample(int u,int v,std::uint16_t clut,std::uint16_t& texel){
    const unsigned mx=(state.window&31)*8,my=((state.window>>5)&31)*8;
    const unsigned ox=((state.window>>10)&31)*8,oy=((state.window>>15)&31)*8;
    unsigned tu=(unsigned(u)&255&~mx)|(ox&mx),tv=(unsigned(v)&255&~my)|(oy&my);
    const unsigned depth=(state.mode>>7)&3;
    const unsigned x=(((state.mode&15)*64)+(depth==0?tu/4:depth==1?tu/2:tu))&1023;
    const unsigned y=(((state.mode&16)?256:0)+tv)&511;
    if(written_[y*1024+x])return Result::TextureFeedbackUnsupported;
    if(!vram_.word(x,y,texel))return Result::UnknownVram;
    if(depth<2){
        const unsigned index=depth==0?(texel>>((tu&3)*4))&15:(texel>>((tu&1)*8))&255;
        const unsigned cx=(((clut&63)*16)+index)&1023,cy=(clut>>6)&511;
        if(written_[cy*1024+cx])return Result::TextureFeedbackUnsupported;
        if(!vram_.word(cx,cy,texel))return Result::UnknownVram;
    }
    return Result::Complete;
}
Result GamePacketRenderer::pixel(int x,int y,const std::array<int,3>& color,int u,int v,
    std::uint16_t clut,bool textured,bool raw,bool transparent,bool dither){
    if(clipped(x,y))return Result::Complete;
    auto& p=*progress_;p.stopped_x=x;p.stopped_y=y;
    if(p.candidates==pixel_budget)return Result::PixelLimit;
    ++p.candidates;
    std::uint16_t background=0,texel=0;bool background_known=false;
    if(state.mask&2){
        if(!vram_.word(unsigned(x),unsigned(y),background))return Result::UnknownVram;
        background_known=true;if(background&0x8000){++p.masked;return Result::Complete;}
    }
    if(textured){
        const auto result=sample(u,v,clut,texel);if(result!=Result::Complete)return result;
        if(!texel){++p.transparent;return Result::Complete;}
    }
    const bool blend=transparent&&(!textured||(texel&0x8000));
    if(blend&&!background_known&&!vram_.word(unsigned(x),unsigned(y),background))return Result::UnknownVram;
    static constexpr int matrix[4][4]={{-4,0,-3,1},{2,-2,3,-1},{-3,1,-4,0},{3,-1,2,-2}};
    const int delta=dither&&((state.mode&512)!=0)?matrix[y&3][x&3]:0;
    unsigned output=(state.mask&1)?0x8000u:(textured?texel&0x8000u:0);
    for(unsigned channel=0;channel<3;++channel){
        const int source=(texel>>(channel*5))&31;
        int front=textured?(raw?source:clamp(((source*color[channel])>>4)+delta,255)>>3):clamp(color[channel]+delta,255)>>3;
        if(blend){
            const int back=(background>>(channel*5))&31;
            switch((state.mode>>5)&3){
            case 0:front=average(back,front);break;
            case 1:front=std::min(31,back+front);break;
            case 2:front=std::max(0,back-front);break;
            default:front=std::min(31,back+(front>>2));break;
            }
        }
        output|=unsigned(front)<<(channel*5);
    }
    vram_.drawWord(unsigned(x),unsigned(y),std::uint16_t(output));written_[std::size_t(y)*1024+x]=1;++p.pixels;
    return Result::Complete;
}
Result GamePacketRenderer::triangle(Vertex a,Vertex b,Vertex c,std::uint16_t clut,bool textured,bool raw,bool transparent,bool gouraud){
    ++progress_->triangles;
    const int minx=std::min({a.x,b.x,c.x}),maxx=std::max({a.x,b.x,c.x});
    const int miny=std::min({a.y,b.y,c.y}),maxy=std::max({a.y,b.y,c.y});
    if(maxx-minx>1023||maxy-miny>511)return Result::Complete;
    // Attributes start at the source-selected leftmost vertex with a half
    // unit bias. Quantize each planar gradient to twelve fractional bits;
    // dividing an exact barycentric value per pixel loses this precision rule.
    const Vertex anchor=b.x<=a.x?(c.x<=b.x?c:b):(c.x<a.x?c:a);
    auto area=edge(a.x,a.y,b.x,b.y,c.x,c.y);if(!area)return Result::Complete;
    if(area<0){std::swap(b,c);area=-area;}
    std::array<std::int64_t,5> gx{},gy{},origin{};
    const auto attribute=[](const Vertex& v,unsigned i){return i<3?v.color[i]:i==3?v.u:v.v;};
    for(unsigned i=0;i<5;++i){
        if(i<3?!gouraud:!textured)continue;
        const auto da=std::int64_t(attribute(b,i)-attribute(a,i)),db=std::int64_t(attribute(c,i)-attribute(a,i));
        gx[i]=(da*(c.y-a.y)-db*(b.y-a.y))*4096/area;
        gy[i]=((b.x-a.x)*db-(c.x-a.x)*da)*4096/area;
        origin[i]=std::int64_t(attribute(anchor,i))*4096+2048;
    }
    const auto interpolated=[&](unsigned i,int x,int y){
        const auto fixed=origin[i]+gx[i]*(x-anchor.x)+gy[i]*(y-anchor.y);
        return int((std::uint64_t(fixed)>>12)&255);
    };
    const int x0=std::max({0,minx,int(state.top_left&1023)}),x1=std::min({1023,maxx,int(state.bottom_right&1023)});
    const int y0=std::max({0,miny,int((state.top_left>>10)&511)}),y1=std::min({511,maxy,int((state.bottom_right>>10)&511)});
    for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
        const auto wa=edge(b.x,b.y,c.x,c.y,x,y),wb=edge(c.x,c.y,a.x,a.y,x,y),wc=edge(a.x,a.y,b.x,b.y,x,y);
        if(wa<0||wb<0||wc<0||(!wa&&!included(b.x,b.y,c.x,c.y))||
           (!wb&&!included(c.x,c.y,a.x,a.y))||(!wc&&!included(a.x,a.y,b.x,b.y)))continue;
        std::array<int,3> color=a.color;
        if(gouraud)for(unsigned ch=0;ch<3;++ch)color[ch]=interpolated(ch,x,y);
        const int u=textured?interpolated(3,x,y):0,v=textured?interpolated(4,x,y):0;
        const auto result=pixel(x,y,color,u,v,clut,textured,raw,transparent,gouraud||(textured&&!raw));
        if(result!=Result::Complete)return result;
    }
    return Result::Complete;
}
Result GamePacketRenderer::line(Vertex a,Vertex b,bool transparent){
    ++progress_->lines;
    const int width=std::abs(b.x-a.x),height=std::abs(b.y-a.y),steps=std::max(width,height);
    if(width>1023||height>511)return Result::Complete;
    // The original line convention orders endpoints by X (including vertical
    // ties). Coordinates retain32fractional bits; slopes round away from zero.
    // Tiny start biases resolve half-pixel ties differently on descending Y.
    if(steps&&a.x>=b.x)std::swap(a,b);
    constexpr std::int64_t unit=std::int64_t{1}<<32;
    const auto slope=[&](int delta){return steps?(std::int64_t(delta)*unit+(delta>0?steps-1:delta<0?1-steps:0))/steps:0;};
    const auto sx=slope(b.x-a.x),sy=slope(b.y-a.y);
    const auto start_x=std::int64_t(a.x)*unit+unit/2-1024;
    const auto start_y=std::int64_t(a.y)*unit+unit/2-(sy<0?1024:0);
    std::array<int,3> color_step{};
    for(unsigned ch=0;ch<3;++ch)color_step[ch]=steps?(b.color[ch]-a.color[ch])*4096/steps:0;
    for(int i=0;i<=steps;++i){
        const int x=signed11(std::uint32_t(std::uint64_t(start_x+sx*i)>>32));
        const int y=signed11(std::uint32_t(std::uint64_t(start_y+sy*i)>>32));
        std::array<int,3> color{};
        for(unsigned ch=0;ch<3;++ch)color[ch]=int((std::uint32_t(a.color[ch]*4096+2048+color_step[ch]*i)>>12)&255);
        const auto result=pixel(x,y,color,0,0,0,false,false,transparent,true);
        if(result!=Result::Complete)return result;
    }
    return Result::Complete;
}
Result GamePacketRenderer::rectangle(Vertex a,int w,int h,std::uint16_t clut,bool textured,bool raw,bool transparent){
    ++progress_->rectangles;
    const int sx=(state.mode&4096)?-1:1,sy=(state.mode&8192)?-1:1;
    for(int y=std::max(0,a.y);y<std::min(512,a.y+h);++y)
        for(int x=std::max(0,a.x);x<std::min(1024,a.x+w);++x){
            const auto result=pixel(x,y,a.color,a.u+(x-a.x)*sx,a.v+(y-a.y)*sy,clut,textured,raw,transparent,false);
            if(result!=Result::Complete)return result;
        }
    return Result::Complete;
}
Result GamePacketRenderer::feed(std::uint32_t word,std::uint8_t known_mask){
    if(known_mask&0xf0)return Result::Argument;
    if(!used_&&!(known_mask&8))return Result::PacketUnavailable;
    const unsigned code=used_?pending_[0]>>24:word>>24;
    unsigned required=15;
    if(!used_){
        if(code==0||code==1)required=8;
        else if(code==0xe1)required=11;
        else if(code==0xe6)required=9;
        else if(code>=0x20&&code<=0x7f&&(code<0x40||code>=0x60)&&(code&5)==5)required=8;
    }else if(code>=0x20&&code<=0x3f){
        const bool textured=(code&4)!=0,gouraud=(code&16)!=0;
        const unsigned vertices=(code&8)?4:3;unsigned cursor=1;
        for(unsigned i=0;i<vertices;++i){
            if(gouraud&&i){if(used_==cursor)required=textured&&(code&1)?0:7;++cursor;}
            ++cursor; // Every XY byte contains consumed coordinate bits.
            if(textured){
                // UV0's high half is CLUT; UV1's is the texture page. Only
                // UV2/UV3 have entirely ignored high halves in the source GPU.
                if(i>=2&&used_==cursor)required=3;
                ++cursor;
            }
        }
    }else if(code>=0x50&&code<=0x57&&used_==2)required=7;
    if((known_mask&required)!=required)return Result::PacketUnavailable;
    if(!used_){
        if(code==0||code==1||(code>=0xe1&&code<=0xe6))needed_=1;
        else if(code==2)needed_=3;
        else if(code>=0x20&&code<=0x3f){const unsigned n=(code&8)?4:3;needed_=1+n*((code&4)?2:1)+((code&16)?n-1:0);}
        else if(code>=0x40&&code<=0x5f){if(code&8)return Result::UnsupportedCommand;needed_=(code&16)?4:3;}
        else if(code>=0x60&&code<=0x7f)needed_=2+((code&4)?1:0)+((code&24)?0:1);
        else return Result::UnsupportedCommand;
    }
    // Only discarded bytes can be absent here. Canonicalize the local command
    // copy; the caller's retained bytes and byte knowledge remain untouched.
    std::uint32_t mask=0;for(unsigned i=0;i<4;++i)if(known_mask&(1u<<i))mask|=0xffu<<(i*8);
    pending_[used_++]=word&mask;++progress_->words;
    if(used_<needed_)return Result::Complete;
    used_=0;const auto result=execute();if(result==Result::Complete)++progress_->commands;return result;
}
Result GamePacketRenderer::execute(){
    const auto code=pending_[0]>>24;const auto word=pending_[0];
    if(code==0)return Result::Complete;
    if(code==1){std::fill(written_.begin(),written_.end(),std::uint8_t{0});return Result::Complete;}
    if(code>=0xe1&&code<=0xe6){
        switch(code){
        case 0xe1:state.mode=word&0x3fff;break;case 0xe2:state.window=word&0xfffff;break;
        case 0xe3:state.top_left=word&0x7ffff;break;case 0xe4:state.bottom_right=word&0x7ffff;break;
        case 0xe5:state.offset=word&0x3fffff;break;default:state.mask=word&3;break;
        }
        state.known|=1u<<(code-0xe1);return Result::Complete;
    }
    if(code==2){
        const auto color=rgb(word);const unsigned value=unsigned(color[0]>>3)|(unsigned(color[1]>>3)<<5)|(unsigned(color[2]>>3)<<10);
        const unsigned left=pending_[1]&0x3f0,top=(pending_[1]>>16)&511;
        const unsigned width=((pending_[2]&1023)+15)&~15u,height=(pending_[2]>>16)&511;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
            progress_->stopped_x=int((left+x)&1023);progress_->stopped_y=int((top+y)&511);
            if(progress_->candidates==pixel_budget)return Result::PixelLimit;
            ++progress_->candidates;vram_.drawWord((left+x)&1023,(top+y)&511,std::uint16_t(value));
            written_[((top+y)&511)*1024+((left+x)&1023)]=1;++progress_->pixels;
        }
        return Result::Complete;
    }
    const bool textured=(code&4)!=0,raw=(code&1)!=0,transparent=(code&2)!=0,gouraud=(code&16)!=0;
    const auto base_color=rgb(word);
    std::array<Vertex,4> vertices{};std::uint16_t clut=0;unsigned cursor=1;
    if(code<0x40){
        const unsigned count=(code&8)?4:3;
        for(unsigned i=0;i<count;++i){
            vertices[i].color=(gouraud&&i)?rgb(pending_[cursor++]):base_color;
            const auto xy=pending_[cursor++];vertices[i].x=signed11(xy);vertices[i].y=signed11(xy>>16);
            if(textured){
                const auto uv=pending_[cursor++];vertices[i].u=int(uv&255);vertices[i].v=int((uv>>8)&255);
                if(i==0)clut=std::uint16_t(uv>>16);
                if(i==1){state.mode=(state.mode&~0x9ffu)|((uv>>16)&0x9ffu);}
            }
        }
        const auto result=ready();if(result!=Result::Complete)return result;
        for(auto& v:vertices){v.x+=signed11(state.offset);v.y+=signed11(state.offset>>11);}
        const auto first=triangle(vertices[0],vertices[1],vertices[2],clut,textured,raw,transparent,gouraud);
        if(first!=Result::Complete||count==3)return first;
        return triangle(vertices[1],vertices[2],vertices[3],clut,textured,raw,transparent,gouraud);
    }
    const auto result=ready();if(result!=Result::Complete)return result;
    vertices[0].color=base_color;
    const auto xy=pending_[cursor++];vertices[0].x=signed11(xy)+signed11(state.offset);vertices[0].y=signed11(xy>>16)+signed11(state.offset>>11);
    if(code<0x60){
        vertices[1].color=gouraud?rgb(pending_[cursor++]):base_color;
        const auto end=pending_[cursor];vertices[1].x=signed11(end)+signed11(state.offset);vertices[1].y=signed11(end>>16)+signed11(state.offset>>11);
        return line(vertices[0],vertices[1],transparent);
    }
    if(textured){const auto uv=pending_[cursor++];vertices[0].u=int(uv&255);vertices[0].v=int((uv>>8)&255);clut=std::uint16_t(uv>>16);}
    const unsigned size=(code>>3)&3;
    const int w=size?(size==1?1:size==2?8:16):int(pending_[cursor]&1023);
    const int h=size?(size==1?1:size==2?8:16):int((pending_[cursor]>>16)&511);
    return rectangle(vertices[0],w,h,clut,textured,raw,transparent);
}
Result GamePacketRenderer::drawWords(const std::uint32_t* words,std::size_t count,GameDrawProgress& p){
    begin(p);if(!words&&count)return Result::Argument;
    for(std::size_t i=0;i<count;++i){const auto result=feed(words[i]);if(result!=Result::Complete)return result;}
    if(used_)return Result::IncompleteCommand;
    p.completed=true;p.stopped_x=p.stopped_y=-1;return Result::Complete;
}
Result GamePacketRenderer::drawOrderingTable(GamePacketRead read,void* user,std::uint32_t first,std::size_t link_budget,GameDrawProgress& p){
    struct Reader {GamePacketRead read;void* user;};Reader reader{read,user};
    if(!read){begin(p);return Result::Argument;}
    const auto known=[](void* context,std::uint32_t address,GamePacketWord& word){
        auto& r=*static_cast<Reader*>(context);const auto result=r.read(r.user,address,word.word);
        word.known_mask=15;return result;
    };
    return drawKnownOrderingTable(known,&reader,first,link_budget,p);
}
Result GamePacketRenderer::drawKnownWords(const GamePacketWord* words,std::size_t count,GameDrawProgress& p){
    begin(p);if(!words&&count)return Result::Argument;
    for(std::size_t i=0;i<count;++i){const auto result=feed(words[i].word,words[i].known_mask);if(result!=Result::Complete)return result;}
    if(used_)return Result::IncompleteCommand;
    p.completed=true;p.stopped_x=p.stopped_y=-1;return Result::Complete;
}
Result GamePacketRenderer::drawKnownOrderingTable(GamePacketKnownRead read,void* user,std::uint32_t first,std::size_t link_budget,GameDrawProgress& p){
    begin(p);if(!read||first>0xffffff)return Result::Argument;
    std::uint32_t address=first;
    while(!(address&0x800000)){
        p.stopped_link=address;if(address&3)return Result::LinkAlignment;
        if(p.links==link_budget)return Result::LinkLimit;
        GamePacketWord tag{};auto result=read(user,address,tag);if(result!=Result::Complete)return result;
        if(tag.known_mask&0xf0)return Result::Argument;
        if(tag.known_mask!=15)return Result::PacketUnavailable;
        ++p.links;const unsigned count=tag.word>>24;
        for(unsigned i=0;i<count;++i){GamePacketWord word{};result=read(user,address+4+i*4,word);if(result!=Result::Complete)return result;
            result=feed(word.word,word.known_mask);if(result!=Result::Complete)return result;}
        address=tag.word&0xffffff;
    }
    if(used_)return Result::IncompleteCommand;
    p.completed=true;p.stopped_link=0;p.stopped_x=p.stopped_y=-1;return Result::Complete;
}
}
