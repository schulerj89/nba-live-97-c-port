#include "zdomf_model.hpp"
#include "zdomf_mocap.hpp"
#include "zdomf_projection.hpp"
#include "zdomf_runtime.hpp"
#include "zdomf_transform.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
struct Rgb { std::uint8_t r=0,g=0,b=0; };
struct Point { double x=0,y=0; };
struct DrawFace { std::array<Point,3> p{}; double depth=0; std::uint8_t part=0; };
void pixel(std::vector<Rgb>& image, int x, int y, Rgb color) {
    if (x>=0 && y>=0 && x<512 && y<240) image[std::size_t(y)*512+x]=color;
}
void line(std::vector<Rgb>& image,int x0,int y0,int x1,int y1,Rgb color) {
    const int dx=std::abs(x1-x0),sx=x0<x1?1:-1;
    const int dy=-std::abs(y1-y0),sy=y0<y1?1:-1;
    int error=dx+dy;
    for(;;) {
        pixel(image,x0,y0,color);
        if(x0==x1&&y0==y1)break;
        const int twice=2*error;
        if(twice>=dy){error+=dy;x0+=sx;}
        if(twice<=dx){error+=dx;y0+=sy;}
    }
}
double edge(Point a,Point b,Point p) {
    return (p.x-a.x)*(b.y-a.y)-(p.y-a.y)*(b.x-a.x);
}
void triangle(std::vector<Rgb>& image,const DrawFace& face,Rgb color) {
    const double area=edge(face.p[0],face.p[1],face.p[2]);
    if(std::abs(area)<0.01)return;
    const int min_x=std::max(0,int(std::floor(std::min({face.p[0].x,face.p[1].x,face.p[2].x}))));
    const int max_x=std::min(511,int(std::ceil(std::max({face.p[0].x,face.p[1].x,face.p[2].x}))));
    const int min_y=std::max(0,int(std::floor(std::min({face.p[0].y,face.p[1].y,face.p[2].y}))));
    const int max_y=std::min(239,int(std::ceil(std::max({face.p[0].y,face.p[1].y,face.p[2].y}))));
    for(int y=min_y;y<=max_y;++y)for(int x=min_x;x<=max_x;++x) {
        const Point p{double(x)+0.5,double(y)+0.5};
        const double a=edge(face.p[1],face.p[2],p)/area;
        const double b=edge(face.p[2],face.p[0],p)/area;
        const double c=1.0-a-b;
        if(a>=0&&b>=0&&c>=0)pixel(image,x,y,color);
    }
}
nba97::ZdomfVec3 narrow(nba97::ZdomfWorldVec3 value) {
    return {static_cast<std::int16_t>(value.x),static_cast<std::int16_t>(value.y),
            static_cast<std::int16_t>(value.z)};
}
void write_ppm(const std::filesystem::path& path,const std::vector<Rgb>& image) {
    std::ofstream output(path,std::ios::binary);
    if(!output)throw std::runtime_error("cannot create runtime smoke frame");
    output<<"P6\n512 240\n255\n";
    output.write(reinterpret_cast<const char*>(image.data()),
                 static_cast<std::streamsize>(image.size()*sizeof(Rgb)));
}
std::vector<std::uint8_t> bytes(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("missing runtime smoke asset: "+path.string());
    return {(std::istreambuf_iterator<char>(input)),{}};
}
}

int main(int argc,char** argv) {
    try {
        if(argc!=4)throw std::runtime_error(
            "usage: runtime-smoke <model-root> <ZFEMOCAP.BIN> <output.ppm>");
        const std::filesystem::path root=argv[1],mocap_path=argv[2],output=argv[3];
        auto model=nba97::load_zdomf_model(root/"ZDOMFATL.BIN");
        const auto trig=bytes(root/"ZDOMTRIG.BIN");
        const auto transforms=nba97::load_zdomf_base_transforms(
            root/"ZDEFLIST.BIN",root/"ZDOMTRIG.BIN");
        const auto projection=nba97::load_create_player_projection(root/"ZDOMTRIG.BIN");
        const auto mocap=nba97::load_zdomf_mocap(mocap_path);
        for(std::size_t part=0;part<model.pivots.size();++part)
            model.pivots[part]=nba97::apply_zdomf_transform(
                transforms.parts[part],model.pivots[part]);
        for(auto& face:model.primary_faces)for(auto& corner:face.corners)
            corner.position=nba97::apply_zdomf_transform(
                transforms.parts[corner.part],corner.position);

        const auto pose=nba97::sample_zdomf_mocap(mocap,1,0);
        const auto runtime=nba97::build_zdomf_runtime_pose(
            model.pivots,trig,pose,{75,{0,0,0},0,0});
        std::array<int,4> bounds{{512,240,-1,-1}};
        std::size_t saturated=0,visible=0;
        std::vector<Rgb> image(512*240,{5,8,16});
        std::vector<std::array<nba97::ZdomfWorldVec3,3>> world_faces;
        std::vector<std::uint8_t> face_parts;
        std::array<double,4> view_bounds{{1e9,1e9,-1e9,-1e9}};
        for(const auto& face:model.primary_faces) {
            std::array<nba97::ZdomfWorldVec3,3> world{};
            for(std::size_t corner=0;corner<3;++corner) {
                const auto& source=face.corners[corner];
                world[corner]=nba97::apply_zdomf_runtime_pose(
                    runtime,source.part,source.position);
                const auto projected=nba97::project_zdomf_vertex(projection,narrow(world[corner]));
                if(projected.flags!=nba97::ZdomfProjectionNone)++saturated;
                if(projected.x>=0&&projected.x<512&&projected.y>=0&&projected.y<240)++visible;
                bounds[0]=std::min(bounds[0],int(projected.x));
                bounds[1]=std::min(bounds[1],int(projected.y));
                bounds[2]=std::max(bounds[2],int(projected.x));
                bounds[3]=std::max(bounds[3],int(projected.y));
                pixel(image,projected.x,projected.y,{180,220,255});
                const double vx=world[corner].x*0.88+world[corner].z*0.48;
                const double vy=world[corner].y;
                view_bounds[0]=std::min(view_bounds[0],vx);
                view_bounds[1]=std::min(view_bounds[1],vy);
                view_bounds[2]=std::max(view_bounds[2],vx);
                view_bounds[3]=std::max(view_bounds[3],vy);
            }
            world_faces.push_back(world);
            face_parts.push_back(face.corners[2].part);
        }
        if(runtime.hierarchy.root_count!=3||runtime.hierarchy.max_depth!=5||
           visible<700||saturated!=0||mocap.clips[1].physical_frames!=18||
           mocap.clips[1].logical_ticks!=36) {
            throw std::runtime_error("runtime pipeline invariants failed");
        }

        // Filled, auto-fit 3D diagnostic. Exact RTPS pixels remain at right;
        // this view proves decoded faces form a renderable posed surface.
        const double width=std::max(1.0,view_bounds[2]-view_bounds[0]);
        const double height=std::max(1.0,view_bounds[3]-view_bounds[1]);
        const double scale=std::min(205.0/width,205.0/height);
        const double ox=18.0+(205.0-width*scale)/2.0-view_bounds[0]*scale;
        const double oy=222.0-(205.0-height*scale)/2.0+view_bounds[1]*scale;
        std::vector<DrawFace> draw;
        for(std::size_t index=0;index<world_faces.size();++index) {
            DrawFace face{};face.part=face_parts[index];
            for(std::size_t corner=0;corner<3;++corner) {
                const auto& world=world_faces[index][corner];
                const double vx=world.x*0.88+world.z*0.48;
                face.p[corner]={ox+vx*scale,oy-world.y*scale};
                face.depth+=(-world.x*0.48+world.z*0.88)/3.0;
            }
            draw.push_back(face);
        }
        std::sort(draw.begin(),draw.end(),[](const auto& a,const auto& b){return a.depth<b.depth;});
        const std::array<Rgb,6> palette{{
            {220,174,92},{78,142,216},{222,222,226},
            {181,105,72},{102,184,132},{187,100,180}}};
        for(const auto& face:draw)triangle(image,face,palette[face.part%palette.size()]);
        for(const auto& node:runtime.hierarchy.parts) {
            const auto a=node.joint_origin,b=node.joint_end;
            const double fixed=runtime.scale_16_16/65536.0;
            const double ax=ox+(a.x*0.88+a.z*0.48)*fixed*scale;
            const double ay=oy-(a.y*fixed+runtime.root_translation.y)*scale;
            const double bx=ox+(b.x*0.88+b.z*0.48)*fixed*scale;
            const double by=oy-(b.y*fixed+runtime.root_translation.y)*scale;
            line(image,int(ax),int(ay),int(bx),int(by),{255,230,60});
        }
        write_ppm(output,image);
        std::cout<<"ZDOMF RUNTIME SMOKE: PASS clips=6 clip1=18keys/36ticks joints=20"
                 <<" visible="<<visible<<" saturated="<<saturated
                 <<" rtps="<<(bounds[2]-bounds[0]+1)<<'x'<<(bounds[3]-bounds[1]+1)
                 <<" solid_faces="<<draw.size()<<" frame="<<output.string()<<'\n';
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"ZDOMF RUNTIME SMOKE: FAIL - "<<error.what()<<'\n';
        return 1;
    }
}
