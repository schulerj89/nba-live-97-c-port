#include "create_player_preview.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace nba97 {
namespace {
std::vector<std::uint8_t> bytes(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    if(!input) throw std::runtime_error("missing Create Player model asset: "+path.string());
    return {(std::istreambuf_iterator<char>(input)),{}};
}
std::uint16_t u16(const std::vector<std::uint8_t>& b,std::size_t at) {
    if(at+2>b.size()) throw std::runtime_error("truncated Create Player model u16");
    return static_cast<std::uint16_t>(b[at]|(b[at+1]<<8));
}
std::uint32_t u32(const std::vector<std::uint8_t>& b,std::size_t at) {
    if(at+4>b.size()) throw std::runtime_error("truncated Create Player model u32");
    return std::uint32_t(b[at])|(std::uint32_t(b[at+1])<<8)|
        (std::uint32_t(b[at+2])<<16)|(std::uint32_t(b[at+3])<<24);
}
std::int16_t s16(const std::vector<std::uint8_t>& b,std::size_t at) {
    return static_cast<std::int16_t>(u16(b,at));
}
void pixel(PshImage& image,int x,int y,std::uint8_t r,std::uint8_t g,std::uint8_t b) {
    if(x<0||y<0||x>=image.width||y>=image.height)return;
    const auto at=(static_cast<std::size_t>(y)*image.width+x)*4;
    image.rgba[at]=r;image.rgba[at+1]=g;image.rgba[at+2]=b;image.rgba[at+3]=255;
}
void circle(PshImage& image,int cx,int cy,int radius,std::uint8_t r,std::uint8_t g,std::uint8_t b) {
    for(int y=-radius;y<=radius;++y)for(int x=-radius;x<=radius;++x)
        if(x*x+y*y<=radius*radius)pixel(image,cx+x,cy+y,r,g,b);
}
void line(PshImage& image,int x0,int y0,int x1,int y1,int width,
          std::uint8_t r,std::uint8_t g,std::uint8_t b) {
    const int dx=std::abs(x1-x0),sx=x0<x1?1:-1,dy=-std::abs(y1-y0),sy=y0<y1?1:-1;
    int error=dx+dy;
    for(;;){circle(image,x0,y0,width,r,g,b);if(x0==x1&&y0==y1)break;
        const int twice=2*error;if(twice>=dy){error+=dy;x0+=sx;}if(twice<=dx){error+=dx;y0+=sy;}}
}
struct Point { double x=0,y=0; };
Point rotate(Point value,double radians) {
    const double c=std::cos(radians),s=std::sin(radians);
    return {value.x*c-value.y*s,value.x*s+value.y*c};
}
std::uint8_t expand5(std::uint16_t value) {
    return static_cast<std::uint8_t>((value<<3)|(value>>2));
}
PshImage decode_first_team_texture(const std::filesystem::path& path) {
    const auto data=bytes(path);
    if(data.size()<0x4A0 || std::string(data.begin(),data.begin()+4)!="SHPP" || u32(data,8)!=10)
        throw std::runtime_error("invalid Create Player team SHPP: "+path.string());
    const auto image_at=static_cast<std::size_t>(u32(data,0x14));
    const auto next_image_at=static_cast<std::size_t>(u32(data,0x1c));
    const auto format=u32(data,image_at);
    const auto width=u16(data,image_at+4),height=u16(data,image_at+6);
    if((format&0xff)!=0x40||width<30||width>32||height!=64||next_image_at<=image_at+64)
        throw std::runtime_error("unexpected Create Player team texture format");
    const auto pixel_at=image_at+16;
    const auto palette_at=next_image_at-32;
    const auto palette_descriptor_at=palette_at-16;
    const auto packed=palette_descriptor_at-pixel_at;
    const auto stored_width=packed*2/height;
    if(stored_width<width) throw std::runtime_error("team texture row stride is truncated");
    if(palette_at+32>data.size()) throw std::runtime_error("truncated team texture palette");
    PshImage image;image.width=width;image.height=height;image.tag="chr0";
    image.rgba.resize(static_cast<std::size_t>(width)*height*4);
    for(std::size_t index=0;index<static_cast<std::size_t>(width)*height;++index) {
        const auto x=index%width,y=index/width;
        const auto packed_byte=data[pixel_at+y*(stored_width/2)+x/2];
        const auto palette_index=static_cast<std::uint8_t>((x&1)?packed_byte>>4:packed_byte&15);
        const auto color=u16(data,palette_at+palette_index*2);
        const auto out=index*4;
        image.rgba[out]=expand5(color&31);image.rgba[out+1]=expand5((color>>5)&31);
        image.rgba[out+2]=expand5((color>>10)&31);image.rgba[out+3]=color?255:0;
    }
    return image;
}
void textured_jersey(PshImage& output,const PshImage& texture,int cx,int top,int height) {
    for(int y=0;y<height;++y) {
        const int half=12+(y*5)/height;
        const int source_y=std::min<int>(texture.height-1,y*texture.height/height);
        for(int x=-half;x<=half;++x) {
            const int source_x=std::clamp((x+half)*texture.width/(half*2+1),0,int(texture.width)-1);
            const auto at=(static_cast<std::size_t>(source_y)*texture.width+source_x)*4;
            if(texture.rgba[at+3])pixel(output,cx+x,top+y,texture.rgba[at],texture.rgba[at+1],texture.rgba[at+2]);
        }
    }
}
}

CreatePlayerPreview::CreatePlayerPreview(const std::filesystem::path& asset_root) {
    const auto model_root=asset_root/"create_player"/"model";
    const auto model=bytes(model_root/"ZDOMFATL.BIN");
    if(model.size()!=45148 || model.size()<0xCA4)
        throw std::runtime_error("ZDOMFATL.BIN does not match the retail model extent");
    /* FUN_800687BC sets DAT_800EFF74=base+0xBCC; FUN_80069098 wires the
       following twenty eight-byte translation records into body parts. */
    for(std::size_t index=0;index<bones_.size();++index) {
        const auto at=0xBCC+index*8;
        bones_[index]={s16(model,at),s16(model,at+2),s16(model,at+4)};
    }
    vertex_count_=u32(model,0xC6C); face_count_=u32(model,0xC70);
    if(vertex_count_!=251 || face_count_!=38)
        throw std::runtime_error("unexpected ZDOMF geometry header");

    const auto mocap=bytes(asset_root/"menu"/"ZFEMOCAP.BIN");
    if(mocap.size()!=22188 || u32(mocap,0)!=0x5670 || u32(mocap,4)!=0x5688 ||
       u32(mocap,0x5670)!=8 || u32(mocap,0x5688)!=116)
        throw std::runtime_error("ZFEMOCAP directory does not contain recovered 8/116-frame sets");
    /* The 116-frame idle set begins at 0x790. Its compressed records retain
       signed XYZ values followed by the retail ABCD/DCBA sentinel. Preserve
       those values for deterministic articulated motion while the polygon
       packet decoder is translated separately. */
    for(std::size_t at=0x79C;at+8<=0xC64;at+=2) {
        const auto marker=u16(mocap,at+6);
        if(marker==0xABCD||marker==0xDCBA) {
            motion_samples_.push_back({s16(mocap,at),s16(mocap,at+2),s16(mocap,at+4)});
            at+=6;
        }
    }
    if(motion_samples_.size()<116)
        throw std::runtime_error("ZFEMOCAP idle stream has too few signed motion records");
    for(const auto& entry:std::filesystem::directory_iterator(model_root))
        if(entry.path().filename().string().rfind("ZDOMF",0)==0 && entry.path().extension()==".BIN")
            ++team_family_count_;
    if(team_family_count_<29) throw std::runtime_error("Create Player team model family is incomplete");
    std::vector<std::filesystem::path> team_paths;
    for(const auto& entry:std::filesystem::directory_iterator(model_root)) {
        const auto name=entry.path().filename().string();
        if(name.rfind("ZDOME",0)!=0||entry.path().extension()!=".BIN"||
           name=="ZDOMEALE.BIN"||name=="ZDOMEALW.BIN"||name=="ZDOMEFRE.BIN") continue;
        team_paths.push_back(entry.path());
    }
    std::sort(team_paths.begin(),team_paths.end());
    for(const auto& path:team_paths) team_textures_.push_back(decode_first_team_texture(path));
    if(team_textures_.size()!=29) throw std::runtime_error("expected 29 NBA team uniform textures");
}

void CreatePlayerPreview::draw(PshImage& image,const Nba97CreateEditor& editor,
                               std::uint32_t elapsed_ms) const {
    const auto& sample=motion_samples_[(elapsed_ms/17u)%116u];
    const double idle=static_cast<double>(sample.x)/4096.0*0.10;
    const double counter=static_cast<double>(sample.y)/4096.0*0.055;
    const double scale=0.19*(static_cast<double>(editor.height_inches)/75.0);
    const Point root{390.0,116.0+static_cast<double>(sample.z)/4096.0*1.5};
    const auto point=[&](Point from,const Vec3& vector,double angle) {
        auto delta=rotate({vector.x*scale,-vector.y*scale},angle);
        return Point{from.x+delta.x,from.y+delta.y};
    };
    const auto chain=[&](int first,Point origin,double angle,std::uint8_t r,
                         std::uint8_t g,std::uint8_t b,int width) {
        Point at=origin;
        for(int index=0;index<4;++index) {
            const Point next=point(at,bones_[first+index],angle*(index+1)/4.0);
            line(image,static_cast<int>(at.x),static_cast<int>(at.y),
                 static_cast<int>(next.x),static_cast<int>(next.y),width,r,g,b);
            at=next;
        }
        return at;
    };
    const std::array<std::array<std::uint8_t,3>,8> skin{{
        {{244,194,142}},{{222,165,113}},{{198,133,82}},{{171,105,65}},
        {{143,84,54}},{{116,67,45}},{{91,52,39}},{{70,42,34}}}};
    const auto skin_color=skin[std::min<std::size_t>(editor.skin_tone,skin.size()-1)];
    chain(0,root,idle,skin_color[0],skin_color[1],skin_color[2],4);
    chain(4,root,-idle,skin_color[0],skin_color[1],skin_color[2],4);
    Point chest=root;
    for(int index=8;index<11;++index) {
        const Point next=point(chest,bones_[index],counter);
        line(image,static_cast<int>(chest.x),static_cast<int>(chest.y),
             static_cast<int>(next.x),static_cast<int>(next.y),8,
             190,190,190);
        chest=next;
    }
    if(editor.team<team_textures_.size())
        textured_jersey(image,team_textures_[editor.team],static_cast<int>(root.x),
                        static_cast<int>(chest.y+5),38);
    chain(12,chest,idle+counter,skin_color[0],skin_color[1],skin_color[2],4);
    chain(16,chest,-idle-counter,skin_color[0],skin_color[1],skin_color[2],4);
    circle(image,static_cast<int>(chest.x),static_cast<int>(chest.y-9),9,
           skin_color[0],skin_color[1],skin_color[2]);
}

std::string CreatePlayerPreview::description() const {
    return "ZDOMF parts=20 vertices="+std::to_string(vertex_count_)+
        " faces="+std::to_string(face_count_)+" team-models="+
        std::to_string(team_family_count_)+" uniforms="+
        std::to_string(team_textures_.size())+"x10-SHPP mocap-idle=116 frames samples="+
        std::to_string(motion_samples_.size());
}
} // namespace nba97
