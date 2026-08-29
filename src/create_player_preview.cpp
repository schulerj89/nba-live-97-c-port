#include "create_player_preview.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
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
struct RasterFace {
    std::array<Point3,3> world{};
    std::array<Point,3> screen{};
    std::array<Point,3> uv{};
    std::uint8_t part=0;
    double depth=0,light=1;
};
double edge(Point a,Point b,Point p) {
    return (p.x-a.x)*(b.y-a.y)-(p.y-a.y)*(b.x-a.x);
}
bool uniform_part(std::uint8_t part) {
    return part==1||part==5||part==8||part==11;
}
bool skin_part(std::uint8_t part) {
    return part==2||part==6||part==9||
        (part>=13&&part<=15)||(part>=17&&part<=19);
}
void triangle(PshImage& output,const RasterFace& face,const PshImage& jersey,
              const PshImage& shorts,const PshImage& shorts_alt,
              const std::array<std::uint8_t,3>& skin) {
    const double area=edge(face.screen[0],face.screen[1],face.screen[2]);
    if(std::abs(area)<0.01)return;
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
        std::array<std::uint8_t,3> color{{38,38,44}};
        if(skin_part(face.part)) color=skin;
        else if(uniform_part(face.part)) {
            const int packet_u=std::clamp(static_cast<int>(a*face.uv[0].x+b*face.uv[1].x+c*face.uv[2].x),0,255);
            const int packet_v=std::clamp(static_cast<int>(a*face.uv[0].y+b*face.uv[1].y+c*face.uv[2].y),0,255);
            const PshImage* texture=&jersey;
            int tx=std::clamp(packet_u,0,int(jersey.width)-1);
            int ty=std::clamp(packet_v,0,int(jersey.height)-1);
            if(packet_v>=118&&packet_v<198) {
                texture=packet_u<120?&shorts:&shorts_alt;
                tx=std::clamp(packet_u-(packet_u<120?0:120),0,int(texture->width)-1);
                ty=std::clamp(packet_v-118,0,int(texture->height)-1);
            }
            const auto at=(static_cast<std::size_t>(ty)*texture->width+tx)*4;
            if(texture->rgba[at+3])color={{texture->rgba[at],texture->rgba[at+1],texture->rgba[at+2]}};
            else color={{185,185,190}};
        }
        pixel(output,x,y,static_cast<std::uint8_t>(color[0]*face.light),
              static_cast<std::uint8_t>(color[1]*face.light),
              static_cast<std::uint8_t>(color[2]*face.light));
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
    for(std::size_t part=0;part<model_.pivots.size();++part)
        model_.pivots[part]=apply_zdomf_transform(base_transforms_.parts[part],model_.pivots[part]);
    for(auto& face:model_.primary_faces)for(auto& corner:face.corners)
        corner.position=apply_zdomf_transform(base_transforms_.parts[corner.part],corner.position);
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
    if(mocap_.clips[1].physical_frames!=18||mocap_.clips[1].logical_ticks!=36)
        throw std::runtime_error("ZFEMOCAP Create Player clip is not 18 keys / 36 ticks");
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
    for(const auto& path:team_paths) {
        team_jerseys_.push_back(decode_team_texture(path,3));
        team_shorts_.push_back(decode_team_texture(path,8));
        team_shorts_alt_.push_back(decode_team_texture(path,9));
    }
    if(team_jerseys_.size()!=29||team_shorts_.size()!=29||team_shorts_alt_.size()!=29)
        throw std::runtime_error("expected 29 NBA jersey and shorts texture pairs");
}

void CreatePlayerPreview::draw(PshImage& image,const Nba97CreateEditor& editor,
                               std::uint32_t elapsed_ms) const {
    const auto& clip=mocap_.clips[1];
    // FUN_80035260 explicitly selects clip 1 through FUN_80034CC8 before
    // FUN_800351F4 initializes playback.
    const auto pose=sample_zdomf_mocap(mocap_,1,(elapsed_ms/33u)%clip.logical_ticks);
    const auto runtime=build_zdomf_runtime_pose(
        model_.pivots,packed_trig_,pose,{editor.height_inches,{0,0,0},0,0});
    const bool appearance_closeup=editor.selected_field>=NBA97_CREATE_SKIN_TONE&&
        editor.selected_field<=NBA97_CREATE_FACIAL_HAIR;
    const double width_scale=std::clamp(1.0+(static_cast<double>(editor.weight_pounds)-200.0)/500.0,
                                        0.82,1.25);
    const std::array<std::array<std::uint8_t,3>,8> skin{{
        {{244,194,142}},{{222,165,113}},{{198,133,82}},{{171,105,65}},
        {{143,84,54}},{{116,67,45}},{{91,52,39}},{{70,42,34}}}};
    const auto skin_color=skin[std::min<std::size_t>(editor.skin_tone,skin.size()-1)];
    std::vector<RasterFace> raster;
    raster.reserve(model_.primary_faces.size());
    std::array<double,4> view_bounds{{1e9,1e9,-1e9,-1e9}};
    for(const auto& face:model_.primary_faces) {
        // Preserve the previous material heuristic for this isolated test.
        // Only transform ownership changes: FUN_800687BC retains a distinct
        // part reference for every corner, including 94 cross-part faces.
        RasterFace out{};out.part=face.corners[2].part;
        for(std::size_t corner=0;corner<3;++corner) {
            const auto part=face.corners[corner].part;
            const auto& vertex=face.corners[corner].position;
            const auto assembled=apply_zdomf_runtime_pose(runtime,part,vertex);
            out.world[corner]={double(assembled.x),double(assembled.y),double(assembled.z)};
            out.uv[corner]={double(face.uv[corner][0]),double(face.uv[corner][1])};
            // This screen-space fit remains port-owned and isolated. The exact
            // RTPS path is exercised separately by the runtime smoke test while
            // FUN_80066090's second matrix/group buffers are still translated.
            const double yaw_x=(out.world[corner].x*0.94+out.world[corner].z*0.34)*width_scale;
            const double yaw_z=-out.world[corner].x*0.34+out.world[corner].z*0.94;
            view_bounds[0]=std::min(view_bounds[0],yaw_x);
            view_bounds[1]=std::min(view_bounds[1],out.world[corner].y);
            view_bounds[2]=std::max(view_bounds[2],yaw_x);
            view_bounds[3]=std::max(view_bounds[3],out.world[corner].y);
            out.depth+=yaw_z/3.0;
        }
        const Point3 a{out.world[1].x-out.world[0].x,out.world[1].y-out.world[0].y,out.world[1].z-out.world[0].z};
        const Point3 b{out.world[2].x-out.world[0].x,out.world[2].y-out.world[0].y,out.world[2].z-out.world[0].z};
        const double nz=a.x*b.y-a.y*b.x;
        out.light=std::clamp(0.70+std::abs(nz)/12000.0,0.70,1.0);
        raster.push_back(out);
    }
    const double source_width=std::max(1.0,view_bounds[2]-view_bounds[0]);
    const double source_height=std::max(1.0,view_bounds[3]-view_bounds[1]);
    const double target_width=appearance_closeup?180.0:140.0;
    const double target_height=appearance_closeup?190.0:155.0;
    const double scale=std::min(target_width/source_width,target_height/source_height);
    const double center_x=408.0;
    const double center_y=128.0;
    const double source_center_x=(view_bounds[0]+view_bounds[2])*0.5;
    const double source_center_y=(view_bounds[1]+view_bounds[3])*0.5;
    for(auto& face:raster)for(std::size_t corner=0;corner<3;++corner) {
        const double yaw_x=(face.world[corner].x*0.94+face.world[corner].z*0.34)*width_scale;
        face.screen[corner]={center_x+(yaw_x-source_center_x)*scale,
                             center_y-(face.world[corner].y-source_center_y)*scale};
    }
    std::sort(raster.begin(),raster.end(),[](const auto& a,const auto& b){return a.depth<b.depth;});
    const auto team=std::min<std::size_t>(editor.team,team_jerseys_.size()-1);
    for(const auto& face:raster)
        triangle(image,face,team_jerseys_[team],team_shorts_[team],team_shorts_alt_[team],skin_color);
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
        " secondary-triangles="+std::to_string(model_.secondary_face_count)+layout.str()+" team-models="+
        std::to_string(team_family_count_)+" uniforms="+
        std::to_string(team_jerseys_.size())+"xZDOMS-jersey/shorts mocap-clips=6 create=clip1/18keys/36ticks base-transform-sets="+
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
