#include "create_player_preview.hpp"
#include "create_player_motion.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
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
struct Point { double x=0,y=0; };
struct Point3 { double x=0,y=0,z=0; };
std::uint8_t expand5(std::uint16_t value) {
    return static_cast<std::uint8_t>((value<<3)|(value>>2));
}
PshImage decode_team_texture(const std::filesystem::path& path,std::uint32_t record) {
    const auto data=bytes(path);
    if(data.size()<32 || std::string(data.begin(),data.begin()+4)!="SHPP" || record>=u32(data,8))
        throw std::runtime_error("invalid Create Player team SHPP: "+path.string());
    const auto image_at=static_cast<std::size_t>(u32(data,20+record*8));
    const auto next_image_at=record+1<u32(data,8)?
        static_cast<std::size_t>(u32(data,20+(record+1)*8)):data.size()-12;
    const auto format=u32(data,image_at);
    const auto width=u16(data,image_at+4),height=u16(data,image_at+6);
    if((format&0xff)!=0x41||!width||!height||next_image_at<=image_at+544)
        throw std::runtime_error("unexpected Create Player team texture format");
    const auto pixel_at=image_at+16;
    const auto palette_at=next_image_at-512;
    const auto palette_descriptor_at=palette_at-16;
    const auto stored_width=(palette_descriptor_at-pixel_at)/height;
    if(stored_width<width) throw std::runtime_error("team texture row stride is truncated");
    if(palette_at+512>data.size()) throw std::runtime_error("truncated team texture palette");
    PshImage image;image.width=width;image.height=height;
    image.tag.assign(reinterpret_cast<const char*>(data.data()+16+record*8),4);
    image.rgba.resize(static_cast<std::size_t>(width)*height*4);
    for(std::size_t index=0;index<static_cast<std::size_t>(width)*height;++index) {
        const auto x=index%width,y=index/width;
        const auto palette_index=data[pixel_at+y*stored_width+x];
        const auto color=u16(data,palette_at+palette_index*2);
        const auto out=index*4;
        image.rgba[out]=expand5(color&31);image.rgba[out+1]=expand5((color>>5)&31);
        image.rgba[out+2]=expand5((color>>10)&31);image.rgba[out+3]=color?255:0;
    }
    return image;
}
PshImage decode_texture4(const std::filesystem::path& path,std::uint32_t record) {
    const auto data=bytes(path);
    if(data.size()<32 || std::string(data.begin(),data.begin()+4)!="SHPP" || record>=u32(data,8))
        throw std::runtime_error("invalid Create Player 4-bpp SHPP: "+path.string());
    const auto image_at=static_cast<std::size_t>(u32(data,20+record*8));
    const auto next_image_at=record+1<u32(data,8)?
        static_cast<std::size_t>(u32(data,20+(record+1)*8)):data.size()-12;
    const auto width=u16(data,image_at+4),height=u16(data,image_at+6);
    if((u32(data,image_at)&0xff)!=0x40||!width||!height||next_image_at<=image_at+64)
        throw std::runtime_error("unexpected Create Player 4-bpp texture format");
    const auto pixel_at=image_at+16;
    const auto palette_at=next_image_at-32;
    const auto palette_descriptor_at=palette_at-16;
    const auto stored_width=(palette_descriptor_at-pixel_at)/height;
    if(stored_width*2<width||palette_at+32>data.size())
        throw std::runtime_error("truncated Create Player 4-bpp texture");
    PshImage image;image.width=width;image.height=height;
    image.tag.assign(reinterpret_cast<const char*>(data.data()+16+record*8),4);
    image.rgba.resize(static_cast<std::size_t>(width)*height*4);
    for(std::size_t y=0;y<height;++y)for(std::size_t x=0;x<width;++x) {
        const auto packed=data[pixel_at+y*stored_width+x/2];
        const auto palette_index=x&1?packed>>4:packed&15;
        const auto color=u16(data,palette_at+palette_index*2);
        const auto out=(y*width+x)*4;
        image.rgba[out]=expand5(color&31);image.rgba[out+1]=expand5((color>>5)&31);
        image.rgba[out+2]=expand5((color>>10)&31);image.rgba[out+3]=color?255:0;
    }
    return image;
}
struct IndexedTexture {
    int width=0,height=0,bits_per_pixel=8;
    int vram_x_words=0,vram_y=0;
    std::vector<std::uint8_t> indices;
    std::vector<std::uint16_t> palette;
};
IndexedTexture decode_indexed_texture(const std::filesystem::path& path,
                                      std::uint32_t record,int bits_per_pixel,
                                      std::size_t trailing_bytes=0) {
    const auto data=bytes(path);
    if(data.size()<32||std::string(data.begin(),data.begin()+4)!="SHPP"||record>=u32(data,8))
        throw std::runtime_error("invalid indexed Create Player SHPP: "+path.string());
    const auto image_at=static_cast<std::size_t>(u32(data,20+record*8));
    const auto next_image_at=record+1<u32(data,8)?
        static_cast<std::size_t>(u32(data,20+(record+1)*8)):data.size()-12;
    const auto width=u16(data,image_at+4),height=u16(data,image_at+6);
    const auto palette_colors=bits_per_pixel==4?16u:256u;
    if(next_image_at<palette_colors*2+trailing_bytes)
        throw std::runtime_error("truncated indexed Create Player texture record");
    const auto palette_at=next_image_at-trailing_bytes-palette_colors*2;
    const auto palette_descriptor_at=palette_at-16;
    const auto pixel_at=image_at+16;
    const auto stored_width=(palette_descriptor_at-pixel_at)/height;
    if(!width||!height||palette_at+palette_colors*2>data.size()||
       (bits_per_pixel==8?stored_width<width:stored_width*2<width))
        throw std::runtime_error("truncated indexed Create Player texture");
    if(bits_per_pixel==4&&trailing_bytes) {
        const auto packed_row_bytes=((static_cast<std::size_t>(width)+3u)/4u)*2u;
        if(stored_width!=packed_row_bytes)
            throw std::runtime_error("unexpected aligned 4-bpp Create Player row stride");
    }
    IndexedTexture result;result.width=width;result.height=height;
    result.bits_per_pixel=bits_per_pixel;
    result.vram_x_words=u16(data,image_at+12);
    result.vram_y=u16(data,image_at+14);
    result.indices.resize(static_cast<std::size_t>(width)*height);
    for(std::size_t y=0;y<height;++y)for(std::size_t x=0;x<width;++x) {
        const auto packed=data[pixel_at+y*stored_width+(bits_per_pixel==4?x/2:x)];
        result.indices[y*width+x]=bits_per_pixel==4?(x&1?packed>>4:packed&15):packed;
    }
    result.palette.resize(palette_colors);
    for(std::size_t i=0;i<palette_colors;++i)result.palette[i]=u16(data,palette_at+i*2);
    return result;
}
void normalize_runtime_clut(std::vector<std::uint16_t>& palette) {
    for(auto& color:palette)color=color?static_cast<std::uint16_t>(color|0x8000u):0x9084u;
}
struct RasterFace {
    std::array<Point3,3> world{};
    std::array<Point,3> screen{};
    std::array<Point,3> uv{};
    std::uint8_t part=0;
    std::uint16_t clut=0;
    std::uint16_t tpage=0;
    std::uint32_t ordering_key=0;
    std::size_t submission_index=0;
    std::size_t submission_order=0;
    std::array<std::uint8_t,3> modulation{{128,128,128}};
};
struct TextureSampleCounts {
    std::size_t opaque=0,transparent=0,missing=0;
    void add(Ps1TextureSample sample) {
        if(sample==Ps1TextureSample::Opaque)++opaque;
        else if(sample==Ps1TextureSample::Transparent)++transparent;
        else ++missing;
    }
};
struct TextureRepresentative {
    bool present=false;
    int screen_x=0,screen_y=0,u=0,v=0;
    Ps1TextureSample sample=Ps1TextureSample::Missing;
    Ps1TextureTrace trace{};
    std::array<std::uint8_t,3> color{};
};
struct FaceTextureAudit {
    TextureSampleCounts samples{};
    TextureRepresentative first_sample{};
    TextureRepresentative first_transparent{};
    TextureRepresentative first_missing{};
    std::size_t solid_visible=0,textured_visible=0;
};
struct RasterTextureAudit {
    static constexpr std::size_t no_upload=std::numeric_limits<std::size_t>::max();
    static constexpr std::size_t jersey_upload=no_upload-1;
    static constexpr std::size_t name_upload=no_upload-2;
    int width=0,height=0;
    std::vector<FaceTextureAudit> faces;
    std::vector<int> solid_owner,textured_owner;
    TextureSampleCounts total;
    std::map<std::uint16_t,TextureSampleCounts> by_clut;
    std::map<std::uint16_t,TextureSampleCounts> by_tpage;
    std::map<std::size_t,TextureSampleCounts> by_upload;
};
struct OriginalVramAudit {
    std::vector<std::uint8_t> words;
    std::size_t samples=0;
    std::size_t word_mismatches=0;
    std::size_t index_mismatches=0;
    std::size_t palette_mismatches=0;
    bool first_present=false;
    std::size_t first_face=0;
    int first_x=0,first_y=0;
    std::uint16_t first_current_word=0,first_original_word=0;
    std::uint8_t first_current_index=0,first_original_index=0;
    std::uint16_t first_current_palette=0,first_original_palette=0;
};
double edge(Point a,Point b,Point p) {
    return (p.x-a.x)*(b.y-a.y)-(p.y-a.y)*(b.x-a.x);
}
void triangle(PshImage& output,const RasterFace& face,
              const Ps1VramTextureAtlas& textures,
              const Ps1VramTextureAtlas& jersey_textures,
              const Ps1VramTextureAtlas& name_textures,
              std::uint16_t palette_variant,
              std::uint16_t texture_variant,
              const std::array<std::uint8_t,3>& skin,
    RasterTextureAudit* audit,OriginalVramAudit* original_vram) {
    const double area=edge(face.screen[0],face.screen[1],face.screen[2]);
    // FUN_80065740 runs GTE NCLIP before addPrim and submits only MAC0 <= 0.
    // edge() has the opposite sign convention, so a non-positive host area
    // is either a retail-culled backface or a zero-area GPU no-op. A live
    // close-up OT contains exactly the 106/251 faces accepted by this test.
    if(area<=0.0)return;
    const int min_x=std::max(0,static_cast<int>(std::floor(std::min({face.screen[0].x,face.screen[1].x,face.screen[2].x}))));
    const int max_x=std::min(output.width-1,static_cast<int>(std::ceil(std::max({face.screen[0].x,face.screen[1].x,face.screen[2].x}))));
    const int min_y=std::max(0,static_cast<int>(std::floor(std::min({face.screen[0].y,face.screen[1].y,face.screen[2].y}))));
    const int max_y=std::min(output.height-1,static_cast<int>(std::ceil(std::max({face.screen[0].y,face.screen[1].y,face.screen[2].y}))));
    for(int y=min_y;y<=max_y;++y)for(int x=min_x;x<=max_x;++x) {
        const Point p{double(x)+0.5,double(y)+0.5};
        const double a=edge(face.screen[1],face.screen[2],p)/area;
        const double b=edge(face.screen[2],face.screen[0],p)/area;
        const double c=1.0-a-b;
        if(a<0||b<0||c<0)continue;
        const auto pixel_index=static_cast<std::size_t>(y)*output.width+x;
        if(audit)audit->solid_owner[pixel_index]=static_cast<int>(face.submission_index);
        std::array<std::uint8_t,3> color{{38,38,44}};
        const int packet_u=std::clamp(static_cast<int>(a*face.uv[0].x+b*face.uv[1].x+c*face.uv[2].x),0,255);
        const int packet_v=std::clamp(static_cast<int>(a*face.uv[0].y+b*face.uv[1].y+c*face.uv[2].y),0,255);
        Ps1TextureTrace trace{};
        const auto sample_texture=[&](Ps1TextureTrace* sample_trace) {
            auto result=textures.sampleDetailed(
                face.clut,face.tpage,packet_u,packet_v,palette_variant,
                texture_variant,color,sample_trace);
            if(result==Ps1TextureSample::Missing) {
                result=jersey_textures.sampleDetailed(
                    face.clut,face.tpage,packet_u,packet_v,palette_variant,
                    texture_variant,color,sample_trace);
                if(result!=Ps1TextureSample::Missing&&sample_trace)
                    sample_trace->upload_index=RasterTextureAudit::jersey_upload;
            }
            if(result==Ps1TextureSample::Missing) {
                result=name_textures.sampleDetailed(
                    face.clut,face.tpage,packet_u,packet_v,palette_variant,
                    texture_variant,color,sample_trace);
                if(result!=Ps1TextureSample::Missing&&sample_trace)
                    sample_trace->upload_index=RasterTextureAudit::name_upload;
            }
            return result;
        };
        auto sample=sample_texture(audit?&trace:nullptr);
        if(original_vram) {
            if(!audit)sample=sample_texture(&trace);
            const auto word_at=(static_cast<std::size_t>(trace.word_y)*1024+
                                trace.word_x)*2;
            if(word_at+2>original_vram->words.size())
                throw std::runtime_error("original no$psx VRAM texture address is outside 1 MiB");
            const auto original_word=static_cast<std::uint16_t>(
                original_vram->words[word_at]|(original_vram->words[word_at+1]<<8));
            const auto index_mask=trace.packet_bits_per_pixel==4?0x0fu:0xffu;
            const auto original_index=static_cast<std::uint8_t>(
                (original_word>>(trace.texel_in_word*trace.packet_bits_per_pixel))&index_mask);
            const auto clut_x=(face.clut&0x3fu)*16u;
            const auto clut_y=face.clut>>6;
            const auto palette_at=(static_cast<std::size_t>(clut_y)*1024+
                                   clut_x+original_index)*2;
            if(palette_at+2>original_vram->words.size())
                throw std::runtime_error("original no$psx VRAM CLUT address is outside 1 MiB");
            const auto original_palette=static_cast<std::uint16_t>(
                original_vram->words[palette_at]|(original_vram->words[palette_at+1]<<8));
            ++original_vram->samples;
            const bool word_mismatch=trace.vram_word!=original_word;
            const bool index_mismatch=trace.palette_index!=original_index;
            const bool palette_mismatch=trace.palette_value!=original_palette;
            original_vram->word_mismatches+=word_mismatch;
            original_vram->index_mismatches+=index_mismatch;
            original_vram->palette_mismatches+=palette_mismatch;
            if(!original_vram->first_present&&
               (word_mismatch||index_mismatch||palette_mismatch)) {
                original_vram->first_present=true;
                original_vram->first_face=face.submission_index;
                original_vram->first_x=x;original_vram->first_y=y;
                original_vram->first_current_word=trace.vram_word;
                original_vram->first_original_word=original_word;
                original_vram->first_current_index=trace.palette_index;
                original_vram->first_original_index=original_index;
                original_vram->first_current_palette=trace.palette_value;
                original_vram->first_original_palette=original_palette;
            }
        }
        if(audit) {
            auto& face_audit=audit->faces[face.submission_index];
            face_audit.samples.add(sample);
            audit->total.add(sample);
            audit->by_clut[face.clut].add(sample);
            audit->by_tpage[face.tpage].add(sample);
            audit->by_upload[trace.upload_index].add(sample);
            if(!face_audit.first_sample.present) {
                face_audit.first_sample={true,x,y,packet_u,packet_v,sample,trace,color};
            }
            if(sample==Ps1TextureSample::Transparent&&
               !face_audit.first_transparent.present) {
                face_audit.first_transparent={true,x,y,packet_u,packet_v,sample,trace};
            }
            if(sample==Ps1TextureSample::Missing&&
               !face_audit.first_missing.present) {
                face_audit.first_missing={true,x,y,packet_u,packet_v,sample,trace};
            }
        }
        if(sample==Ps1TextureSample::Transparent)continue;
        if(audit)audit->textured_owner[pixel_index]=static_cast<int>(face.submission_index);
        if(sample==Ps1TextureSample::Missing)color=skin;
        // POLY_FT3 raw-texture mode is off. The PS1 GPU modulates each
        // texel by the packet RGB with 0x80 representing 1.0 intensity.
        pixel(output,x,y,
              static_cast<std::uint8_t>(std::min(255,(int(color[0])*face.modulation[0])>>7)),
              static_cast<std::uint8_t>(std::min(255,(int(color[1])*face.modulation[1])>>7)),
              static_cast<std::uint8_t>(std::min(255,(int(color[2])*face.modulation[2])>>7)));
    }
}
std::string upload_name(std::size_t upload,std::uint16_t texture_variant) {
    if(upload==RasterTextureAudit::no_upload)return "none";
    if(upload==RasterTextureAudit::jersey_upload)return "jersey-number";
    if(upload==RasterTextureAudit::name_upload)return "jersey-name";
    constexpr std::array<unsigned,5> team_records{{2,6,7,4,0}};
    if(upload==0)return "shared-dthr-r1";
    if(upload==1)return "shared-dthl-r4";
    constexpr std::size_t team_base=2;
    if(upload>=team_base&&upload<team_base+team_records.size())
        return "team-r"+std::to_string(team_records[upload-team_base]);
    constexpr std::size_t shoe=team_base+team_records.size();
    if(upload==shoe)return "shoe-r6";
    if(upload==shoe+1+texture_variant)return "head-selected";
    if(upload>shoe)return "head-variant-"+std::to_string(upload-shoe-1);
    return "upload-"+std::to_string(upload);
}
void print_counts(const TextureSampleCounts& counts) {
    std::cout<<" opaque="<<counts.opaque<<" transparent="<<counts.transparent
             <<" missing="<<counts.missing;
}
void print_texture_audit(RasterTextureAudit& audit,
                         const std::vector<RasterFace>& raster,
                         std::uint16_t palette_variant,
                         std::uint16_t texture_variant,
                         const std::array<std::uint8_t,3>& skin) {
    for(const auto owner:audit.solid_owner)
        if(owner>=0)++audit.faces[static_cast<std::size_t>(owner)].solid_visible;
    for(const auto owner:audit.textured_owner)
        if(owner>=0)++audit.faces[static_cast<std::size_t>(owner)].textured_visible;
    std::vector<const RasterFace*> by_index(audit.faces.size(),nullptr);
    for(const auto& face:raster)by_index[face.submission_index]=&face;
    std::cout<<"[CREATE-TEXTURE-AUDIT] fixture=editor-appearance-layer"
             <<" palette-variant="<<palette_variant
             <<" texture-variant="<<texture_variant
             <<" faces="<<audit.faces.size()<<'\n';
    std::cout<<"[CREATE-TEXTURE-TOTAL]";
    print_counts(audit.total);std::cout<<'\n';
    for(std::size_t index=0;index<audit.faces.size();++index) {
        const auto& face=*by_index[index];
        const auto& result=audit.faces[index];
        std::cout<<"[CREATE-TEXTURE-FACE] face="<<index
                 <<" sxy="<<face.screen[0].x<<'/'<<face.screen[0].y<<','
                 <<face.screen[1].x<<'/'<<face.screen[1].y<<','
                 <<face.screen[2].x<<'/'<<face.screen[2].y
                 <<" uv="<<face.uv[0].x<<'/'<<face.uv[0].y<<','
                 <<face.uv[1].x<<'/'<<face.uv[1].y<<','
                 <<face.uv[2].x<<'/'<<face.uv[2].y
                 <<" clut=0x"<<std::hex<<face.clut<<" tpage=0x"<<face.tpage
                 <<std::dec<<" modulation="<<int(face.modulation[0])<<'/'
                 <<int(face.modulation[1])<<'/'<<int(face.modulation[2]);
        print_counts(result.samples);
        std::cout<<" solid-visible="<<result.solid_visible
                 <<" textured-visible="<<result.textured_visible<<'\n';
    }
    for(const auto& entry:audit.by_clut) {
        std::cout<<"[CREATE-TEXTURE-CLUT] clut=0x"<<std::hex<<entry.first<<std::dec;
        print_counts(entry.second);std::cout<<'\n';
    }
    for(const auto& entry:audit.by_tpage) {
        std::cout<<"[CREATE-TEXTURE-TPAGE] tpage=0x"<<std::hex<<entry.first<<std::dec;
        print_counts(entry.second);std::cout<<'\n';
    }
    for(const auto& entry:audit.by_upload) {
        std::cout<<"[CREATE-TEXTURE-UPLOAD] upload="<<upload_name(entry.first,texture_variant);
        if(entry.first!=RasterTextureAudit::no_upload)std::cout<<" index="<<entry.first;
        print_counts(entry.second);std::cout<<'\n';
    }
    std::size_t disappearing=0;
    for(std::size_t index=0;index<audit.faces.size();++index) {
        const auto& result=audit.faces[index];
        if(!result.solid_visible||result.textured_visible)continue;
        ++disappearing;
        std::cout<<"[CREATE-TEXTURE-DISAPPEARING] face="<<index
                 <<" solid-visible="<<result.solid_visible;
        print_counts(result.samples);std::cout<<'\n';
        const auto& representative=result.first_transparent;
        if(!representative.present)continue;
        const auto& face=*by_index[index];
        const auto& trace=representative.trace;
        std::cout<<"[CREATE-TEXTURE-TRACE] face="<<index
                 <<" screen="<<representative.screen_x<<'/'<<representative.screen_y
                 <<" sxy="<<face.screen[0].x<<'/'<<face.screen[0].y<<','
                 <<face.screen[1].x<<'/'<<face.screen[1].y<<','
                 <<face.screen[2].x<<'/'<<face.screen[2].y
                 <<" sample-uv="<<representative.u<<'/'<<representative.v
                 <<" face-uv="<<face.uv[0].x<<'/'<<face.uv[0].y<<','
                 <<face.uv[1].x<<'/'<<face.uv[1].y<<','
                 <<face.uv[2].x<<'/'<<face.uv[2].y
                 <<" clut=0x"<<std::hex<<face.clut<<" tpage=0x"<<face.tpage
                 <<std::dec<<" modulation="<<int(face.modulation[0])<<'/'
                 <<int(face.modulation[1])<<'/'<<int(face.modulation[2])
                 <<" word="<<trace.word_x<<'/'<<trace.word_y
                 <<" texel-in-word="<<trace.texel_in_word
                 <<" selected-upload="<<upload_name(trace.upload_index,texture_variant)
                 <<" upload-bpp="<<trace.upload_bits_per_pixel
                 <<" upload-xy="<<trace.upload_x<<'/'<<trace.upload_y
                 <<" palette-index="<<unsigned(trace.palette_index)
                 <<" palette-value=0x"<<std::hex<<trace.palette_value<<std::dec
                 <<" result=transparent\n";
    }
    std::cout<<"[CREATE-TEXTURE-AUDIT] disappearing-visible-faces="
             <<disappearing<<'\n';
    if(audit.faces.size()>1&&audit.faces[1].first_sample.present) {
        constexpr std::size_t traced_face=1;
        const auto& representative=audit.faces[traced_face].first_sample;
        const auto& face=*by_index[traced_face];
        const auto& trace=representative.trace;
        std::cout<<"[CREATE-TEXTURE-SAMPLE-TRACE] face="<<traced_face
                 <<" screen="<<representative.screen_x<<'/'<<representative.screen_y
                 <<" sample-uv="<<representative.u<<'/'<<representative.v
                 <<" clut=0x"<<std::hex<<face.clut<<" tpage=0x"<<face.tpage
                 <<std::dec<<" modulation="<<int(face.modulation[0])<<'/'
                 <<int(face.modulation[1])<<'/'<<int(face.modulation[2])
                 <<" word="<<trace.word_x<<'/'<<trace.word_y
                 <<" texel-in-word="<<trace.texel_in_word
                 <<" selected-upload="<<upload_name(trace.upload_index,texture_variant)
                 <<" upload-bpp="<<trace.upload_bits_per_pixel
                 <<" upload-xy="<<trace.upload_x<<'/'<<trace.upload_y
                 <<" palette-index="<<unsigned(trace.palette_index)
                 <<" palette-value=0x"<<std::hex<<trace.palette_value<<std::dec
                 <<" texel-rgb="<<unsigned(representative.color[0])<<'/'
                 <<unsigned(representative.color[1])<<'/'
                 <<unsigned(representative.color[2])
                 <<" final-rgb="
                 <<((int(representative.color[0])*face.modulation[0])>>7)<<'/'
                 <<((int(representative.color[1])*face.modulation[1])>>7)<<'/'
                 <<((int(representative.color[2])*face.modulation[2])>>7)<<'\n';
    }
    for(std::size_t index=0;index<audit.faces.size();++index) {
        const auto& result=audit.faces[index];
        if(!result.solid_visible||!result.first_missing.present)continue;
        const auto& representative=result.first_missing;
        const auto& face=*by_index[index];
        const auto& trace=representative.trace;
        std::cout<<"[CREATE-TEXTURE-MISSING-TRACE] face="<<index
                 <<" screen="<<representative.screen_x<<'/'<<representative.screen_y
                 <<" sample-uv="<<representative.u<<'/'<<representative.v
                 <<" clut=0x"<<std::hex<<face.clut<<" tpage=0x"<<face.tpage
                 <<std::dec<<" modulation="<<int(face.modulation[0])<<'/'
                 <<int(face.modulation[1])<<'/'<<int(face.modulation[2])
                 <<" word="<<trace.word_x<<'/'<<trace.word_y
                 <<" selected-upload=none palette-index=unavailable"
                 <<" fallback-rgb="<<((int(skin[0])*face.modulation[0])>>7)<<'/'
                 <<((int(skin[1])*face.modulation[1])>>7)<<'/'
                 <<((int(skin[2])*face.modulation[2])>>7)
                 <<" result=missing\n";
        break;
    }
}
}

CreatePlayerPreview::CreatePlayerPreview(const std::filesystem::path& asset_root) {
    const auto model_root=asset_root/"create_player"/"model";
    model_=load_zdomf_model(model_root/"ZDOMFATL.BIN");
    if(model_.primary_faces.size()!=251 || model_.secondary_face_count!=38 ||
       model_.mixed_part_face_count!=94)
        throw std::runtime_error("unexpected ZDOMF geometry header");
    base_transforms_=load_zdomf_base_transforms(
        model_root/"ZDEFLIST.BIN",model_root/"ZDOMTRIG.BIN");
    packed_trig_=bytes(model_root/"ZDOMTRIG.BIN");
    projection_=load_create_player_projection(model_root/"ZDOMTRIG.BIN");
    // Isolated native transcription of the one-set frontend path through
    // FUN_80062F4C and FUN_800631B0. Preserve all later motion/projection and
    // material approximations so this stage can be evaluated independently.
    const auto apply_base_transforms=[&](ZdomfModel& model) {
        for(std::size_t part=0;part<model.pivots.size();++part)
            model.pivots[part]=apply_zdomf_transform(
                base_transforms_.parts[part],model.pivots[part]);
        for(auto& face:model.primary_faces)for(auto& corner:face.corners)
            corner.position=apply_zdomf_transform(
                base_transforms_.parts[corner.part],corner.position);
        for(std::size_t part=0;part<model.part_triangles.size();++part)
            for(auto& triangle:model.part_triangles[part])for(auto& vertex:triangle)
                vertex=apply_zdomf_transform(base_transforms_.parts[part],vertex);
        for(auto& faces:model.part_faces)for(auto& face:faces)
            for(auto& corner:face.corners)
                corner.position=apply_zdomf_transform(
                    base_transforms_.parts[corner.part],corner.position);
    };
    apply_base_transforms(model_);
    hierarchy_=build_zdomf_hierarchy(model_.pivots);
    projection_flat_bounds_={{32767,32767,65535,-32768,-32768,0}};
    projection_bounds_={{32767,32767,65535,-32768,-32768,0}};
    for(const auto& face:model_.primary_faces)for(const auto& corner:face.corners) {
        const auto flat=project_zdomf_vertex(projection_,corner.position);
        projection_flat_bounds_[0]=std::min<std::int32_t>(projection_flat_bounds_[0],flat.x);
        projection_flat_bounds_[1]=std::min<std::int32_t>(projection_flat_bounds_[1],flat.y);
        projection_flat_bounds_[2]=std::min<std::int32_t>(projection_flat_bounds_[2],flat.depth);
        projection_flat_bounds_[3]=std::max<std::int32_t>(projection_flat_bounds_[3],flat.x);
        projection_flat_bounds_[4]=std::max<std::int32_t>(projection_flat_bounds_[4],flat.y);
        projection_flat_bounds_[5]=std::max<std::int32_t>(projection_flat_bounds_[5],flat.depth);
        const auto assembled=apply_zdomf_hierarchy(hierarchy_,corner.part,corner.position);
        const ZdomfVec3 assembled16{
            static_cast<std::int16_t>(assembled.x),
            static_cast<std::int16_t>(assembled.y),
            static_cast<std::int16_t>(assembled.z)};
        const auto projected=project_zdomf_vertex(projection_,assembled16);
        projection_bounds_[0]=std::min<std::int32_t>(projection_bounds_[0],projected.x);
        projection_bounds_[1]=std::min<std::int32_t>(projection_bounds_[1],projected.y);
        projection_bounds_[2]=std::min<std::int32_t>(projection_bounds_[2],projected.depth);
        projection_bounds_[3]=std::max<std::int32_t>(projection_bounds_[3],projected.x);
        projection_bounds_[4]=std::max<std::int32_t>(projection_bounds_[4],projected.y);
        projection_bounds_[5]=std::max<std::int32_t>(projection_bounds_[5],projected.depth);
        if(projected.flags!=ZdomfProjectionNone)++projection_saturated_vertices_;
    }

    mocap_=load_zdomf_mocap(asset_root/"menu"/"ZFEMOCAP.BIN");
    if(mocap_.clips[1].physical_frames!=18||mocap_.clips[1].logical_ticks!=36||
       mocap_.clips[1].timing_code!=0x28)
        throw std::runtime_error(
            "ZFEMOCAP Create Player clip is not 18 keys / 36 ticks / timing 0x28");
    for(const auto& entry:std::filesystem::directory_iterator(model_root))
        if(entry.path().filename().string().rfind("ZDOMF",0)==0 && entry.path().extension()==".BIN")
            ++team_family_count_;
    if(team_family_count_<29) throw std::runtime_error("Create Player team model family is incomplete");
    std::vector<std::filesystem::path> team_paths;
    for(const auto& entry:std::filesystem::directory_iterator(model_root)) {
        const auto name=entry.path().filename().string();
        if(name.rfind("ZDOMS",0)!=0||entry.path().extension()!=".BIN"||
           name=="ZDOMSALE.BIN"||name=="ZDOMSALW.BIN"||name=="ZDOMSFRE.BIN") continue;
        team_paths.push_back(entry.path());
    }
    std::sort(team_paths.begin(),team_paths.end());
    team_models_.reserve(team_paths.size());
    for(const auto& texture_path:team_paths) {
        const auto texture_name=texture_path.filename().string();
        const auto model_path=model_root/("ZDOMF"+texture_name.substr(5));
        auto team_model=load_zdomf_model(model_path);
        if(team_model.primary_faces.size()!=model_.primary_faces.size() ||
           team_model.secondary_face_count!=model_.secondary_face_count ||
           team_model.mixed_part_face_count!=model_.mixed_part_face_count)
            throw std::runtime_error("Create Player team model layout differs: "+
                                     model_path.string());
        apply_base_transforms(team_model);
        team_models_.push_back(std::move(team_model));
    }
    const auto dither_right=decode_indexed_texture(
        asset_root/"menu"/"ZFEPLAYR.ART",1,8);
    const auto dither_left=decode_indexed_texture(
        asset_root/"menu"/"ZFEPLAYR.ART",4,8);
    const auto number_base=decode_indexed_texture(
        asset_root/"menu"/"ZFEPLAYR.ART",0,4);
    if(dither_right.width!=35||dither_right.height!=114||
       dither_right.vram_x_words!=832||dither_right.vram_y!=374||
       dither_left.width!=77||dither_left.height!=94||
       dither_left.vram_x_words!=850||dither_left.vram_y!=374)
        throw std::runtime_error("Create Player shared dither surfaces do not match retail VRAM layout");
    const auto shoe=decode_indexed_texture(asset_root/"menu"/"ZFEPLAYR.ART",6,4);
    auto shoe_runtime_palette=shoe.palette;
    if(shoe_runtime_palette.size()!=16)
        throw std::runtime_error("Create Player SHOE palette is not 16 colors");
    // Record 6 retains its authored graded 16-color CLUT at (544,502).
    // The black/transparent replacement belongs to record 5's generated
    // last-name surface at the adjacent (512,502) CLUT.
    const auto hair_path=model_root/"ZDOMHAIR.BIN";
    const auto skin_path=model_root/"ZDOMPSKN.BIN";
    // Decode the private SHPP records once. FUN_800626D0 reuses these same
    // resident pointers; reopening the pack for every team/palette variant
    // made native startup needlessly quadratic.
    std::array<IndexedTexture,77> hair_textures{};
    for(std::uint32_t record=0;record<hair_textures.size();++record)
        hair_textures[record]=decode_indexed_texture(hair_path,record,8);
    std::array<IndexedTexture,18> skin_textures{};
    for(std::uint32_t record=0;record<skin_textures.size();++record)
        skin_textures[record]=decode_indexed_texture(skin_path,record,8);
    for(std::uint32_t letter=0;letter<name_letter_indices_.size();++letter) {
        // ZDOMLTRS stores 24 bytes of glyph-control data after the real CLUT.
        // Treating the record end as the palette end makes the calculated row
        // stride one byte too wide and scrambles every row after the first.
        const auto glyph=decode_indexed_texture(
            model_root/"ZDOMLTRS.BIN",letter,4,24);
        if(glyph.height!=23||glyph.width>10)
            throw std::runtime_error("unexpected Create Player jersey-name glyph");
        name_letter_indices_[letter]=glyph.indices;
        name_letter_widths_[letter]=static_cast<std::uint8_t>(glyph.width);
    }
    // FUN_800626D0's exact SHPP record maps. The apparently reversed record
    // order is intentional: the Hair Style control drives the 0xBF scalp
    // family at record/context +0x27, while Facial Hair drives the 0xAF face
    // family at +0x0C.
    // The AF facial family consumes palette A, while the BF scalp family
    // consumes palette B. These selectors look counter-intuitive beside the
    // record order, but are the literal FUN_8006240C pointer tables.
    constexpr std::array<std::uint32_t,3> hair_palette_a{{38,40,39}};
    constexpr std::array<std::uint32_t,3> hair_palette_b{{35,37,36}};
    constexpr std::array<std::uint32_t,18> hair_skin_palette_a{{
        59,60,61,62,63,64,71,72,73,74,75,76,65,66,67,68,69,70}};
    constexpr std::array<std::uint32_t,18> hair_skin_palette_b{{
        41,42,43,44,45,46,53,54,55,56,57,58,47,48,49,50,51,52}};
    const auto facial_a0=hair_textures[hair_palette_a[0]].palette.at(0xa2);
    const auto facial_a1=hair_textures[hair_palette_a[1]].palette.at(0xa2);
    const auto scalp_b0=hair_textures[hair_palette_b[0]].palette.at(0xb1);
    const auto scalp_b1=hair_textures[hair_palette_b[1]].palette.at(0xb1);
    if(facial_a0==facial_a1||scalp_b0==scalp_b1)
        throw std::runtime_error(
            "FUN_800626D0 Hair Color proof samples did not recolor both overlay bands");
    std::cout<<"[CREATE-HAIR-PALETTE] shared-control=Hair Color"
             <<" facial-A2="<<facial_a0<<"->"<<facial_a1
             <<" scalp-B1="<<scalp_b0<<"->"<<scalp_b1
             <<" indices=stable style/facial masks independent of color\n";
    constexpr std::size_t head_variant_count=8*9*13;
    std::array<IndexedTexture,head_variant_count> head_textures{};
    for(std::uint8_t skin=0;skin<8;++skin)
        for(std::uint8_t facial=0;facial<=8;++facial)
            for(std::uint8_t hair=0;hair<=12;++hair) {
                auto head=hair_textures[skin&1];
                if(facial) {
                    const auto& overlay=hair_textures[
                        nba97_create_facial_hair_record(facial)];
                    for(std::size_t pixel=0;pixel<head.indices.size();++pixel)
                        if(overlay.indices[pixel]!=0xaf)head.indices[pixel]=overlay.indices[pixel];
                }
                if(hair) {
                    const auto& overlay=hair_textures[
                        nba97_create_hair_style_record(hair)];
                    for(std::size_t pixel=0;pixel<head.indices.size();++pixel)
                        if(overlay.indices[pixel]!=0xbf)head.indices[pixel]=overlay.indices[pixel];
                }
                head_textures[(skin*9+facial)*13+hair]=std::move(head);
            }
    for(const auto& path:team_paths) {
        Ps1VramTextureAtlas atlas;
        // FUN_80067F74 uploads every ZFEPLAYR record before the per-team
        // switch.  dthr/dthl are the shared 8-bpp body index surfaces; their
        // source CLUTs are intentionally ignored in favor of packet row 501.
        atlas.upload8Indexed(dither_right.width,dither_right.height,
            dither_right.indices,dither_right.vram_x_words,dither_right.vram_y);
        atlas.upload8Indexed(dither_left.width,dither_left.height,
            dither_left.indices,dither_left.vram_x_words,dither_left.vram_y);
        for(const auto record:std::array<std::uint32_t,5>{{2,6,7,4,0}}) {
            const auto texture=decode_indexed_texture(path,record,8);
            constexpr std::array<std::array<int,2>,5> origins{{
                {{832,256}},{{949,374}},{{889,374}},{{832,488}},{{886,468}}}};
            const auto slot=record==2?0:record==6?1:record==7?2:record==4?3:4;
            atlas.upload8Indexed(texture.width,texture.height,texture.indices,
                                 origins[slot][0],origins[slot][1]);
        }
        atlas.upload4Indexed(shoe.width,shoe.height,shoe.indices,922,454);
        // FUN_8006785C/FUN_800626D0 rebuilds this 47x50 indexed head surface
        // and uploads it independently of the five team texture rectangles.
        for(std::uint16_t variant=0;variant<head_variant_count;++variant) {
            const auto& head=head_textures[variant];
            atlas.upload8Indexed(head.width,head.height,head.indices,898,454,variant);
        }
        for(std::uint8_t variant=0;variant<8;++variant) {
            auto team_palette=decode_indexed_texture(path,2,8).palette;
            const auto team_patch=decode_indexed_texture(path,10+variant,8).palette;
            std::copy(team_patch.begin()+208,team_patch.end(),team_palette.begin()+208);
            normalize_runtime_clut(team_palette);
            for(std::uint8_t hair_color=0;hair_color<3;++hair_color)
                for(std::uint8_t decorated=0;decorated<2;++decorated) {
                    const auto palette_variant=static_cast<std::uint16_t>(
                        (variant*3+hair_color)*2+decorated);
                    atlas.uploadClut(team_palette,512,500,palette_variant);
                }

            auto body_palette=hair_textures[23+variant].palette;
            const auto& skin_patch=skin_textures[6+(variant>>1)].palette;
            std::copy(skin_patch.begin()+208,skin_patch.end(),body_palette.begin()+208);
            for(std::uint8_t hair_color=0;hair_color<3;++hair_color)
                for(std::uint8_t decorated=0;decorated<2;++decorated) {
                    auto palette=body_palette;
                    if(decorated) {
                        const auto group=static_cast<std::size_t>(hair_color)*6+(variant>>1);
                        const auto& patch_a=hair_textures[hair_palette_a[hair_color]].palette;
                        const auto& patch_as=hair_textures[hair_skin_palette_a[group]].palette;
                        const auto& patch_b=hair_textures[hair_palette_b[hair_color]].palette;
                        const auto& patch_bs=hair_textures[hair_skin_palette_b[group]].palette;
                        std::copy_n(patch_a.begin()+0xa0,13,palette.begin()+0xa0);
                        std::copy_n(patch_as.begin()+0xad,3,palette.begin()+0xad);
                        std::copy_n(patch_b.begin()+0xb0,13,palette.begin()+0xb0);
                        std::copy_n(patch_bs.begin()+0xbd,3,palette.begin()+0xbd);
                    }
                    normalize_runtime_clut(palette);
                    if(path==team_paths.front()&&variant==0&&decorated) {
                        constexpr std::array<std::uint16_t,3> expected_a2{{
                            0x8422,0x8d0a,0x94c8}};
                        constexpr std::array<std::uint16_t,3> expected_b2{{
                            0x8821,0x8ce9,0x8844}};
                        if(palette.at(0xa2)!=expected_a2[hair_color]||
                           palette.at(0xb2)!=expected_b2[hair_color])
                            throw std::runtime_error(
                                "FUN_800626D0 shared Hair Color CLUT fixture mismatch");
                        std::cout<<"[CREATE-HAIR-CLUT] color="<<int(hair_color)
                                 <<" facial-A2=0x"<<std::hex<<palette.at(0xa2)
                                 <<" scalp-B2=0x"<<palette.at(0xb2)<<std::dec
                                 <<" texture-variant=independent\n";
                    }
                    const auto palette_variant=static_cast<std::uint16_t>(
                        (variant*3+hair_color)*2+decorated);
                    atlas.uploadClut(std::move(palette),512,501,palette_variant);
                    atlas.uploadClut(shoe_runtime_palette,544,502,palette_variant);
                }
        }
        team_texture_uploads_.push_back(std::move(atlas));

        // FUN_80064724 composes the one- or two-digit jersey number from the
        // team's ten ZDOME 4-bpp glyphs into a 64x64 page at (960,256), then
        // uploads record 0's CLUT at (528,502). For a single digit, blank
        // 32-pixel tiles are placed at both sides and the glyph is centered.
        const auto suffix=path.filename().string().substr(5);
        const auto number_path=model_root/("ZDOME"+suffix);
        std::array<IndexedTexture,10> digits{};
        for(std::uint32_t digit=0;digit<digits.size();++digit) {
            digits[digit]=decode_indexed_texture(number_path,digit,4);
            if(digits[digit].width>32||digits[digit].height!=64)
                throw std::runtime_error("unexpected Create Player jersey-number glyph");
        }
        const auto transparent=std::find(
            digits[0].palette.begin(),digits[0].palette.end(),0);
        if(transparent==digits[0].palette.end())
            throw std::runtime_error("Create Player jersey-number CLUT has no transparent entry");
        const auto transparent_index=static_cast<std::uint8_t>(
            std::distance(digits[0].palette.begin(),transparent));
        std::vector<Ps1VramTextureAtlas> number_atlases;
        number_atlases.reserve(100);
        for(std::uint16_t number=0;number<100;++number) {
            auto surface=number_base.indices;
            const auto blank=[&](int origin_x) {
                for(int y=0;y<64;++y)
                    std::fill_n(surface.begin()+y*64+origin_x,
                                digits[0].width,transparent_index);
            };
            const auto place_digit=[&](std::uint8_t digit,int origin_x) {
                for(int y=0;y<64;++y)
                    std::copy_n(digits[digit].indices.begin()+
                                y*digits[digit].width,digits[digit].width,
                                surface.begin()+y*64+origin_x);
            };
            if(number<10) {
                blank(0);blank(32);
                place_digit(static_cast<std::uint8_t>(number),16);
            }
            else {
                place_digit(static_cast<std::uint8_t>(number/10),0);
                place_digit(static_cast<std::uint8_t>(number%10),32);
            }
            Ps1VramTextureAtlas number_atlas;
            number_atlas.upload4Indexed(64,64,std::move(surface),960,256);
            number_atlas.uploadClut(digits[0].palette,528,502,0xffff);
            number_atlases.push_back(std::move(number_atlas));
        }
        jersey_texture_uploads_.push_back(std::move(number_atlases));
    }
    if(team_texture_uploads_.size()!=29)
        throw std::runtime_error("expected 29 NBA team texture upload sets");
    if(team_models_.size()!=team_texture_uploads_.size())
        throw std::runtime_error("Create Player team model/texture families are misaligned");
    if(jersey_texture_uploads_.size()!=team_texture_uploads_.size())
        throw std::runtime_error("Create Player jersey-number texture family is incomplete");
}

void CreatePlayerPreview::draw(PshImage& image,const Nba97CreateEditor& editor,
                               std::uint32_t elapsed_ms) const {
    const auto team=std::min<std::size_t>(editor.team,team_models_.size()-1);
    const auto& model=team_models_[team];
    const auto jersey_number=std::min<std::size_t>(editor.jersey_number,99);
    const auto& jersey_textures=jersey_texture_uploads_[team][jersey_number];
    const bool appearance_field=
        editor.selected_field>=NBA97_CREATE_SKIN_TONE&&
        editor.selected_field<=NBA97_CREATE_FACIAL_HAIR;
    std::vector<std::uint8_t> name_surface(100u*30u,15);
    const auto letter_index=[](unsigned char value) {
        if(value>='a'&&value<='z')return int(value-'a');
        if(value>='A'&&value<='Z')return int(value-'A');
        return 26;
    };
    std::size_t name_length=0;
    int name_width=0;
    for(;name_length<sizeof(editor.last_name)&&editor.last_name[name_length];++name_length) {
        const auto letter=letter_index(
            static_cast<unsigned char>(editor.last_name[name_length]));
        name_width+=letter<26?name_letter_widths_[letter]+2:6;
    }
    const auto name_spacing=name_width<91?2:1;
    if(name_width>=91)name_width=(name_width+1)-static_cast<int>(name_length);
    int name_x=50-(name_width>>1);
    for(std::size_t character=0;character<name_length;++character) {
        const auto letter=letter_index(
            static_cast<unsigned char>(editor.last_name[character]));
        if(letter<26) {
            const auto width=name_letter_widths_[letter];
            const auto& glyph=name_letter_indices_[letter];
            for(int y=0;y<23;++y)for(int x=0;x<width;++x) {
                const auto target_x=name_x+x;
                if(target_x>=0&&target_x<100)
                    name_surface[static_cast<std::size_t>(y)*100+target_x]=
                        glyph[static_cast<std::size_t>(y)*width+x];
            }
            name_x+=width+name_spacing;
        } else name_x+=6;
    }
    std::vector<std::uint16_t> name_palette(16,0x8000);
    name_palette[15]=0;
    if(appearance_field&&team==0&&editor.skin_tone==0&&editor.hair_style==0&&
       editor.hair_color==0&&editor.facial_hair==0) {
        if(const auto path=std::getenv("NBA97_CREATE_ORIGINAL_VRAM")) {
            const auto original=bytes(path);
            if(original.size()!=1024u*512u*2u)
                throw std::runtime_error(
                    "NBA97_CREATE_ORIGINAL_VRAM must be a 1 MiB raw VRAM dump");
            std::size_t index_mismatches=0,palette_mismatches=0;
            for(std::size_t y=0;y<30;++y)for(std::size_t x=0;x<100;++x) {
                const auto word=u16(original,((320u+y)*1024u+960u+x/4u)*2u);
                const auto expected=static_cast<std::uint8_t>(
                    (word>>((x&3u)*4u))&0x0fu);
                if(name_surface[y*100u+x]!=expected)++index_mismatches;
            }
            for(std::size_t index=0;index<name_palette.size();++index) {
                const auto expected=u16(
                    original,(502u*1024u+512u+index)*2u);
                if(name_palette[index]!=expected)++palette_mismatches;
            }
            std::cout<<"[CREATE-ORIGINAL-NAME-VRAM] samples=3000"
                     <<" index-mismatches="<<index_mismatches
                     <<" palette-mismatches="<<palette_mismatches<<'\n';
            if(index_mismatches||palette_mismatches)
                throw std::runtime_error(
                    "Create Player surname page differs from synchronized no$psx capture");
        }
    }
    Ps1VramTextureAtlas name_textures;
    name_textures.upload4Indexed(100,30,std::move(name_surface),960,320);
    name_textures.uploadClut(std::move(name_palette),512,502,0xffff);
    const std::size_t clip_index=appearance_field?0u:1u;
    const auto& clip=mocap_.clips[clip_index];
    // FUN_80035260 initializes clip 1. The appearance callbacks at B380/B358
    // then write context+0x4E=0 for Skin Tone through Facial Hair; B3A8/B3D4
    // restore clip 1 when the selector leaves those four rows.
    // FUN_80039574 presents once per two NTSC vblanks. Each presentation
    // advances FUN_80034DC0's clip-1 accumulator by 0x300 against a 0x280
    // logical-tick threshold, and FUN_800355A0 advances yaw by 8/1024 turn.
    // The elapsed-time origin is the synchronized retail audit phase
    // (tick 7, accumulator 0x80, yaw 808), not a frozen pose.
    const auto motion=create_player_full_body_motion(elapsed_ms);
    const auto pose=sample_zdomf_mocap(
        mocap_,clip_index,
        appearance_field?0u:motion.logical_tick%clip.logical_ticks);
    ZdomfRuntimeConfig runtime_config{};
    runtime_config.height_value=editor.height_inches;
    // FUN_800355A0 takes 13 frames to move the model context from its
    // full-body state {256,0,640} to the settled appearance state below,
    // while locking context+0xA8 to 0x3F0. A live no$psx capture after only
    // forcing context+0x4E=0 confirms raw +8/+C/+10 =
    // 4240/33792/-2496, or {132,-78,1056} after the runtime's >>5 boundary.
    // The Y component is not constant: FUN_800355A0 subtracts
    // 0xC0 + ((recordHeight << 8) / 0x18) on each of those 13 presentations.
    // Keep that recovered fixed-point relationship so taller players retain
    // the same retail head framing instead of growing out of the close-up.
    runtime_config.root_position=appearance_field?
        ZdomfWorldVec3{132,nba97_create_appearance_root_y(editor.height_inches),1056}:
        ZdomfWorldVec3{256,0,640};
    runtime_config.root_yaw=appearance_field?0x3f0:
        static_cast<std::int16_t>(motion.root_yaw);
    runtime_config.apply_frontend_view=true;
    runtime_config.frontend_angles={2051,191,0};
    // FUN_80031F48 installs DAT_800ED55C/55E/560 = {0x1C0,0xC0,0x500}.
    // Applying that recovered frontend translation after the rotated context
    // root reproduces both synchronized anchors exactly: {544,9,594} at
    // clip-1/tick-7 and {162,104,231} in the settled clip-0 close-up. Unlike
    // the former frozen anchor, it also preserves clip 1's changing root lift,
    // which supplies the retail jog's vertical hop.
    runtime_config.frontend_translation={448,192,1280};
    const auto runtime=build_zdomf_runtime_pose(
        model.pivots,packed_trig_,pose,runtime_config);
    if(appearance_field&&elapsed_ms==0) {
        std::cerr<<"[CREATE-CAMERA] field="<<int(editor.selected_field)
                 <<" clip="<<clip_index
                 <<" root="<<runtime_config.root_position.x<<'/'
                 <<runtime_config.root_position.y<<'/'
                 <<runtime_config.root_position.z
                 <<" yaw="<<runtime_config.root_yaw
                 <<" record-root="<<runtime.record_root_translation.x<<'/'
                 <<runtime.record_root_translation.y<<'/'
                 <<runtime.record_root_translation.z<<'\n';
    }
    const std::array<std::array<std::uint8_t,3>,8> skin{{
        {{244,194,142}},{{222,165,113}},{{198,133,82}},{{171,105,65}},
        {{143,84,54}},{{116,67,45}},{{91,52,39}},{{70,42,34}}}};
    const auto skin_color=skin[std::min<std::size_t>(editor.skin_tone,skin.size()-1)];
    std::size_t part_face_count=0;
    for(const auto& faces:model.part_faces)part_face_count+=faces.size();
    std::vector<RasterFace> raster;
    raster.reserve(model.primary_faces.size()+part_face_count);
    ZdomfProjectionConfig runtime_projection{};
    runtime_projection.camera.rotation={{{4096,0,0},{0,4096,0},{0,0,4096}}};
    runtime_projection.camera.translation={0,0,0};
    runtime_projection.draw_offset_x=0;
    const auto append_face=[&](const ZdomfFace& face,std::size_t audit_index,
                               std::size_t submission_order,bool part_stream) {
        // FUN_800687BC retains a distinct
        // part reference for every corner, including 94 cross-part faces.
        RasterFace out{};out.part=face.corners[2].part;
        out.clut=face.clut;out.tpage=face.tpage;
        out.modulation=face.modulation;
        for(std::size_t corner=0;corner<3;++corner) {
            const auto part=face.corners[corner].part;
            const auto& vertex=face.corners[corner].position;
            const auto assembled=apply_zdomf_runtime_record_pose(runtime,part,vertex);
            out.world[corner]={double(assembled.x),double(assembled.y),double(assembled.z)};
            out.uv[corner]={double(face.uv[corner][0]),double(face.uv[corner][1])};
            const ZdomfVec3 camera_vertex{
                static_cast<std::int16_t>(assembled.x),
                static_cast<std::int16_t>(assembled.y),
                static_cast<std::int16_t>(assembled.z)};
            const auto projected=project_zdomf_vertex(runtime_projection,camera_vertex);
            out.screen[corner]={double(projected.x),double(projected.y)};
        }
        // FUN_800687BC stores a depth-pointer per descriptor. FUN_80065740
        // orders each assembled face through descriptor 0's source-triangle
        // AVSZ3 value, not the average of the assembled face's mixed corners.
        const auto& order_corner=face.corners[0];
        const auto& order_triangle=model.part_triangles[order_corner.part]
            [order_corner.triangle_index];
        std::int64_t source_depth=0;
        for(const auto& vertex:order_triangle) {
            const auto assembled=apply_zdomf_runtime_record_pose(
                runtime,order_corner.part,vertex);
            const auto projected=project_zdomf_vertex(runtime_projection,{
                static_cast<std::int16_t>(assembled.x),
                static_cast<std::int16_t>(assembled.y),
                static_cast<std::int16_t>(assembled.z)});
            source_depth+=projected.depth;
        }
        const auto otz=std::clamp<std::int64_t>((source_depth*0x155)>>12,0,0xffff);
        auto ordering_bucket=static_cast<std::uint32_t>(otz)&0xfffu;
        // FUN_80065564 sends the first six torso triangles eight buckets
        // nearer than the normal FUN_80065388 source stream.
        if(part_stream&&order_corner.part==9&&order_corner.triangle_index<6)
            ordering_bucket=(ordering_bucket-8u)&0xfffu;
        out.ordering_key=ordering_bucket<<2;
        out.submission_index=audit_index;
        out.submission_order=submission_order;
        raster.push_back(out);
    };
    std::size_t source_index=0;
    for(const auto& faces:model.part_faces)for(const auto& face:faces) {
        append_face(face,model.primary_faces.size()+source_index,
                    source_index,true);
        ++source_index;
    }
    for(std::size_t face_index=0;face_index<model.primary_faces.size();++face_index)
        append_face(model.primary_faces[face_index],face_index,
                    part_face_count+face_index,false);
    // The second 38-face descriptor set belongs to FUN_800632D4's conditional
    // alternate leg pass. It uses six separately projected packet streams and
    // must not be submitted by the normal Create Player path (FUN_80069A08).
    // Match the PS1 ordering-table submission recovered by the runtime
    // differential: farther primitives are emitted first so nearer faces
    // cover them.  The reversed comparator made back surfaces overwrite the
    // visible body even though every transformed vertex was already exact.
    std::sort(raster.begin(),raster.end(),[](const auto& a,const auto& b){
        if(a.ordering_key!=b.ordering_key)return a.ordering_key>b.ordering_key;
        // addPrim prepends to a bucket, so later source packets draw first
        // when two faces share the same quantized AVSZ3 ordering key.
        return a.submission_order>b.submission_order;
    });
    const auto skin_variant=static_cast<std::uint16_t>(
        std::min<std::size_t>(editor.skin_tone,7));
    const auto hair_variant=static_cast<std::uint16_t>(
        std::min<std::size_t>(editor.hair_style,12));
    const auto facial_variant=static_cast<std::uint16_t>(
        std::min<std::size_t>(editor.facial_hair,8));
    const auto hair_color_variant=static_cast<std::uint16_t>(
        std::min<std::size_t>(editor.hair_color,2));
    const auto decorated=static_cast<std::uint16_t>(hair_variant||facial_variant);
    const auto palette_variant=static_cast<std::uint16_t>(
        (skin_variant*3+hair_color_variant)*2+decorated);
    const auto texture_variant=static_cast<std::uint16_t>(
        (skin_variant*9+facial_variant)*13+hair_variant);
    const bool run_texture_audit=!texture_audit_logged_&&!appearance_field&&
        elapsed_ms==0&&team==0&&
        skin_variant==1&&hair_variant==1&&hair_color_variant==1&&facial_variant==1;
    RasterTextureAudit texture_audit{};
    if(run_texture_audit) {
        texture_audit.width=image.width;texture_audit.height=image.height;
        texture_audit.faces.resize(model.primary_faces.size()+part_face_count);
        texture_audit.solid_owner.assign(static_cast<std::size_t>(image.width)*image.height,-1);
        texture_audit.textured_owner.assign(static_cast<std::size_t>(image.width)*image.height,-1);
    }
    OriginalVramAudit original_vram{};
    OriginalVramAudit* original_vram_ptr=nullptr;
    if(appearance_field&&team==0&&skin_variant==0&&hair_variant==0&&
       hair_color_variant==0&&facial_variant==0) {
        if(const auto path=std::getenv("NBA97_CREATE_ORIGINAL_VRAM")) {
            original_vram.words=bytes(path);
            if(original_vram.words.size()!=1024u*512u*2u)
                throw std::runtime_error("NBA97_CREATE_ORIGINAL_VRAM must be a 1 MiB raw VRAM dump");
            original_vram_ptr=&original_vram;
        }
    }
    for(const auto& face:raster)
        triangle(image,face,team_texture_uploads_[team],jersey_textures,name_textures,palette_variant,
                  texture_variant,skin_color,run_texture_audit?&texture_audit:nullptr,
                  original_vram_ptr);
    if(original_vram_ptr) {
        std::cout<<"[CREATE-ORIGINAL-VRAM] samples="<<original_vram.samples
                 <<" word-mismatches="<<original_vram.word_mismatches
                 <<" index-mismatches="<<original_vram.index_mismatches
                 <<" palette-mismatches="<<original_vram.palette_mismatches<<'\n';
        if(original_vram.first_present)
            std::cout<<"[CREATE-ORIGINAL-VRAM-FIRST] face="<<original_vram.first_face
                     <<" screen="<<original_vram.first_x<<'/'<<original_vram.first_y
                     <<" word=current:0x"<<std::hex<<original_vram.first_current_word
                     <<" original:0x"<<original_vram.first_original_word
                     <<" index=current:0x"<<unsigned(original_vram.first_current_index)
                     <<" original:0x"<<unsigned(original_vram.first_original_index)
                     <<" palette=current:0x"<<original_vram.first_current_palette
                     <<" original:0x"<<original_vram.first_original_palette
                     <<std::dec<<'\n';
        if(original_vram.samples!=19139 || original_vram.word_mismatches ||
           original_vram.index_mismatches || original_vram.palette_mismatches)
            throw std::runtime_error(
                "Create Player sampled VRAM differs from synchronized no$psx capture");
    }
    if(run_texture_audit) {
        print_texture_audit(texture_audit,raster,palette_variant,texture_variant,skin_color);
        texture_audit_logged_=true;
    }
}

std::string CreatePlayerPreview::description() const {
    std::ostringstream layout;
    layout<<std::hex<<std::uppercase
          <<" packets=0x"<<model_.layout.primary_packet_a_offset
          <<"/0x"<<model_.layout.primary_packet_b_offset
          <<" vertices=0x"<<model_.layout.transformed_vertex_offset
          <<"..0x"<<model_.layout.transformed_vertex_end;
    return "ZDOMF relinked parts=20 surface-triangles="+std::to_string(model_.primary_faces.size())+
        " mixed-part="+std::to_string(model_.mixed_part_face_count)+
        " secondary-alt-triangles="+std::to_string(model_.secondary_faces.size())+layout.str()+" team-models="+
        std::to_string(team_family_count_)+" uniforms="+
        std::to_string(team_texture_uploads_.size())+"xpaired-ZDOMF/ZDOMS shared-body=dthr/dthl mocap-clips=6 create=clip0-closeup/clip1-fullbody base-transform-sets="+
        std::to_string(base_transforms_.available_sets)+
        " hierarchy=3roots/depth"+std::to_string(hierarchy_.max_depth)+
        " projection=RTPS(H="+std::to_string(projection_.projection_distance)+
        ",OF=256/120,draw=128/0,flat="+
        std::to_string(projection_flat_bounds_[0])+"/"+std::to_string(projection_flat_bounds_[1])+"/"+
        std::to_string(projection_flat_bounds_[2])+".."+std::to_string(projection_flat_bounds_[3])+"/"+
        std::to_string(projection_flat_bounds_[4])+"/"+std::to_string(projection_flat_bounds_[5])+
        ",assembled="+
        std::to_string(projection_bounds_[0])+"/"+std::to_string(projection_bounds_[1])+"/"+
        std::to_string(projection_bounds_[2])+".."+std::to_string(projection_bounds_[3])+"/"+
        std::to_string(projection_bounds_[4])+"/"+std::to_string(projection_bounds_[5])+
        ",saturated="+std::to_string(projection_saturated_vertices_)+")";
}
} // namespace nba97
