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
std::int32_t shift12(std::int64_t value) {
    if(value>=0)return static_cast<std::int32_t>(value/4096);
    return static_cast<std::int32_t>(-(((-value)+4095)/4096));
}
nba97::ZdomfTransform multiply_rotation(const nba97::ZdomfTransform& a,
                                        const nba97::ZdomfTransform& b) {
    nba97::ZdomfTransform out{};
    for(std::size_t row=0;row<3;++row)for(std::size_t column=0;column<3;++column) {
        std::int64_t sum=0;
        for(std::size_t inner=0;inner<3;++inner)
            sum+=std::int64_t(a.rotation[row][inner])*b.rotation[inner][column];
        out.rotation[row][column]=static_cast<std::int16_t>(shift12(sum));
    }
    return out;
}
nba97::ZdomfWorldVec3 rotate_trace(const nba97::ZdomfTransform& matrix,
                                   const nba97::ZdomfVec3& vertex) {
    const std::array<std::int32_t,3> input{{vertex.x,vertex.y,vertex.z}};
    std::array<std::int32_t,3> output{};
    for(std::size_t row=0;row<3;++row) {
        std::int64_t sum=0;
        for(std::size_t column=0;column<3;++column)
            sum+=std::int64_t(matrix.rotation[row][column])*input[column];
        output[row]=shift12(sum);
    }
    return {output[0],output[1],output[2]};
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
        const auto trace_raw=model.primary_faces.at(0).corners.at(0);
        const auto trig=bytes(root/"ZDOMTRIG.BIN");
        const auto transforms=nba97::load_zdomf_base_transforms(
            root/"ZDEFLIST.BIN",root/"ZDOMTRIG.BIN");
        const auto& trace_part9=transforms.parts[9];
        std::cout<<"TRACE part9 ZDEFLIST angles="
                 <<transforms.angles[9].x<<'/'<<transforms.angles[9].y<<'/'
                 <<transforms.angles[9].z<<" matrix=";
        for(std::size_t row=0;row<3;++row) {
            if(row)std::cout<<';';
            std::cout<<trace_part9.rotation[row][0]<<'/'
                     <<trace_part9.rotation[row][1]<<'/'
                     <<trace_part9.rotation[row][2];
        }
        std::cout<<" translation="<<trace_part9.translation[0]<<'/'
                 <<trace_part9.translation[1]<<'/'
                 <<trace_part9.translation[2]<<'\n';
        const auto trace_part9_vertex=nba97::apply_zdomf_transform(
            trace_part9,{-40,108,-5});
        if(trace_part9_vertex.x!=106 || trace_part9_vertex.y!=20 ||
           trace_part9_vertex.z!=40)
            throw std::runtime_error("part9 original matrix-layout probe mismatch");
        std::cout<<"TRACE part9 retail-layout probe raw=-40/108/-5 result="
                 <<trace_part9_vertex.x<<'/'<<trace_part9_vertex.y<<'/'
                 <<trace_part9_vertex.z<<" expected=106/20/40 PASS\n";
        const auto projection=nba97::load_create_player_projection(root/"ZDOMTRIG.BIN");
        const auto mocap=nba97::load_zdomf_mocap(mocap_path);
        std::array<int,6> raw_bounds{{32767,32767,32767,-32768,-32768,-32768}};
        for(const auto& face:model.primary_faces)for(const auto& corner:face.corners) {
            raw_bounds[0]=std::min(raw_bounds[0],int(corner.position.x));
            raw_bounds[1]=std::min(raw_bounds[1],int(corner.position.y));
            raw_bounds[2]=std::min(raw_bounds[2],int(corner.position.z));
            raw_bounds[3]=std::max(raw_bounds[3],int(corner.position.x));
            raw_bounds[4]=std::max(raw_bounds[4],int(corner.position.y));
            raw_bounds[5]=std::max(raw_bounds[5],int(corner.position.z));
        }
        for(std::size_t part=0;part<model.pivots.size();++part)
            model.pivots[part]=nba97::apply_zdomf_transform(
                transforms.parts[part],model.pivots[part]);
        for(auto& face:model.primary_faces)for(auto& corner:face.corners)
            corner.position=nba97::apply_zdomf_transform(
                transforms.parts[corner.part],corner.position);
        const auto trace_base=model.primary_faces.at(0).corners.at(0);
        std::array<int,6> base_bounds{{32767,32767,32767,-32768,-32768,-32768}};
        for(const auto& face:model.primary_faces)for(const auto& corner:face.corners) {
            base_bounds[0]=std::min(base_bounds[0],int(corner.position.x));
            base_bounds[1]=std::min(base_bounds[1],int(corner.position.y));
            base_bounds[2]=std::min(base_bounds[2],int(corner.position.z));
            base_bounds[3]=std::max(base_bounds[3],int(corner.position.x));
            base_bounds[4]=std::max(base_bounds[4],int(corner.position.y));
            base_bounds[5]=std::max(base_bounds[5],int(corner.position.z));
        }

        // FUN_80035260 explicitly calls FUN_80034CC8(context,1) before
        // FUN_800351F4, selecting the 18-key Create Player clip.
        const auto pose=nba97::sample_zdomf_mocap(mocap,1,0);
        // FUN_800351F4 stores {0x2000,0x5000,0}; FUN_800696C4 shifts them
        // and maps context +8/+10/+C to render X/Y/Z respectively.
        nba97::ZdomfRuntimeConfig trace_config{75,{256,0,640},0,0};
        trace_config.apply_frontend_view=true;
        const auto runtime=nba97::build_zdomf_runtime_pose(
            model.pivots,trig,pose,trace_config);
        auto original_view=nba97::make_zdomf_rotation(trig,{0x5dc,0,0});
        for(auto& value:original_view.rotation[0])
            value=static_cast<std::int16_t>((static_cast<std::int32_t>(value)*16)/10);
        auto original_scaled_root=nba97::make_zdomf_rotation(trig,{0,0,0});
        for(auto& row:original_scaled_root.rotation)for(auto& value:row)
            value=static_cast<std::int16_t>(
                (static_cast<std::int32_t>(value)*runtime.scale_16_16)/65536);
        const auto original_view_root=multiply_rotation(original_view,original_scaled_root);
        const auto original_part_matrix=multiply_rotation(
            original_view_root,runtime.part_matrices[trace_base.part]);
        std::array<nba97::ZdomfTransform,20> original_part_matrices{};
        std::array<nba97::ZdomfWorldVec3,20> original_origins{};
        std::array<nba97::ZdomfWorldVec3,20> original_endpoints{};
        const auto root16=narrow(runtime.root_translation);
        auto original_root_translation=rotate_trace(original_view,root16);
        original_root_translation.x+=projection.camera.translation[0];
        original_root_translation.y+=projection.camera.translation[1];
        original_root_translation.z+=projection.camera.translation[2];
        const auto& original_parents=nba97::zdomf_parent_table();
        for(std::size_t part=0;part<original_part_matrices.size();++part) {
            original_part_matrices[part]=multiply_rotation(
                original_view_root,runtime.part_matrices[part]);
            const auto parent=original_parents[part];
            original_origins[part]=parent<0?original_root_translation:
                original_endpoints[static_cast<std::size_t>(parent)];
            const auto offset=rotate_trace(original_part_matrices[part],model.pivots[part]);
            original_endpoints[part]={original_origins[part].x+offset.x,
                                      original_origins[part].y+offset.y,
                                      original_origins[part].z+offset.z};
        }
        std::cout<<"TRACE corrected composite="
                 <<original_part_matrix.rotation[0][0]<<'/'
                 <<original_part_matrix.rotation[0][1]<<'/'
                 <<original_part_matrix.rotation[0][2]<<';'
                 <<original_part_matrix.rotation[1][0]<<'/'
                 <<original_part_matrix.rotation[1][1]<<'/'
                 <<original_part_matrix.rotation[1][2]<<';'
                 <<original_part_matrix.rotation[2][0]<<'/'
                 <<original_part_matrix.rotation[2][1]<<'/'
                 <<original_part_matrix.rotation[2][2]<<'\n';
        if(original_part_matrix.rotation[0][0]!=572 ||
           original_part_matrix.rotation[1][0]!=-2195 ||
           original_part_matrix.rotation[2][0]!=1895)
            throw std::runtime_error("polygon trace composite matrix changed");
        const auto original_rotated=rotate_trace(original_part_matrix,trace_base.position);
        auto port_staged=rotate_trace(runtime.part_matrices[trace_base.part],trace_base.position);
        port_staged.x=static_cast<std::int32_t>(
            std::int64_t(port_staged.x)*runtime.scale_16_16/65536);
        port_staged.y=static_cast<std::int32_t>(
            std::int64_t(port_staged.y)*runtime.scale_16_16/65536);
        port_staged.z=static_cast<std::int32_t>(
            std::int64_t(port_staged.z)*runtime.scale_16_16/65536);
        const nba97::ZdomfVec3 staged16{
            static_cast<std::int16_t>(port_staged.x),
            static_cast<std::int16_t>(port_staged.y),
            static_cast<std::int16_t>(port_staged.z)};
        port_staged=rotate_trace(runtime.frontend_view_transform,staged16);
        std::cout<<"TRACE corrected rotation original="<<original_rotated.x<<'/'
                 <<original_rotated.y<<'/'<<original_rotated.z<<" port="
                 <<port_staged.x<<'/'<<port_staged.y<<'/'<<port_staged.z<<'\n';
        if(original_rotated.x!=40||original_rotated.y!=-5||original_rotated.z!=1||
           port_staged.x!=38||port_staged.y!=-4||port_staged.z!=1)
            throw std::runtime_error("polygon trace transform order changed");
        const auto trace_world=nba97::apply_zdomf_runtime_pose(
            runtime,trace_base.part,trace_base.position);
        std::array<int,4> bounds{{512,240,-1,-1}};
        std::size_t saturated=0,visible=0;
        std::vector<Rgb> image(512*240,{5,8,16});
        std::vector<std::array<nba97::ZdomfWorldVec3,3>> world_faces;
        std::vector<std::uint8_t> face_parts;
        std::array<double,4> view_bounds{{1e9,1e9,-1e9,-1e9}};
        std::array<int,6> world_bounds{{32767,32767,32767,-32768,-32768,-32768}};
        std::uint64_t largest_edge_squared=0;
        std::size_t largest_edge_face=0;
        std::array<std::uint8_t,3> largest_edge_parts{};
        std::array<nba97::ZdomfVec3,3> largest_edge_source{};
        std::array<nba97::ZdomfWorldVec3,3> largest_edge_world{};
        std::array<nba97::ZdomfWorldVec3,3> largest_edge_original{};
        std::array<nba97::ZdomfProjectedVertex,3> largest_edge_projected{};
        std::size_t face_index=0;
        for(const auto& face:model.primary_faces) {
            std::array<nba97::ZdomfWorldVec3,3> world{};
            for(std::size_t corner=0;corner<3;++corner) {
                const auto& source=face.corners[corner];
                world[corner]=nba97::apply_zdomf_runtime_pose(
                    runtime,source.part,source.position);
                world_bounds[0]=std::min(world_bounds[0],int(world[corner].x));
                world_bounds[1]=std::min(world_bounds[1],int(world[corner].y));
                world_bounds[2]=std::min(world_bounds[2],int(world[corner].z));
                world_bounds[3]=std::max(world_bounds[3],int(world[corner].x));
                world_bounds[4]=std::max(world_bounds[4],int(world[corner].y));
                world_bounds[5]=std::max(world_bounds[5],int(world[corner].z));
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
            for(std::size_t a=0;a<3;++a)for(std::size_t b=a+1;b<3;++b) {
                const auto dx=std::int64_t(world[a].x)-world[b].x;
                const auto dy=std::int64_t(world[a].y)-world[b].y;
                const auto dz=std::int64_t(world[a].z)-world[b].z;
                const auto squared=static_cast<std::uint64_t>(dx*dx+dy*dy+dz*dz);
                if(squared>largest_edge_squared) {
                    largest_edge_squared=squared;
                    largest_edge_face=face_index;
                    largest_edge_world=world;
                    for(std::size_t corner=0;corner<3;++corner)
                        largest_edge_parts[corner]=face.corners[corner].part;
                    for(std::size_t corner=0;corner<3;++corner)
                        largest_edge_source[corner]=face.corners[corner].position;
                    for(std::size_t corner=0;corner<3;++corner) {
                        const auto part=face.corners[corner].part;
                        const auto rotated=rotate_trace(
                            original_part_matrices[part],face.corners[corner].position);
                        largest_edge_original[corner]={
                            rotated.x+original_origins[part].x,
                            rotated.y+original_origins[part].y,
                            rotated.z+original_origins[part].z};
                    }
                    auto perspective_only=projection;
                    perspective_only.camera.rotation={{{4096,0,0},{0,4096,0},{0,0,4096}}};
                    perspective_only.camera.translation={0,0,0};
                    for(std::size_t corner=0;corner<3;++corner)
                        largest_edge_projected[corner]=nba97::project_zdomf_vertex(
                            perspective_only,narrow(largest_edge_original[corner]));
                }
            }
            ++face_index;
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
        const auto largest_edge_autofit=draw.at(largest_edge_face).p;
        std::sort(draw.begin(),draw.end(),[](const auto& a,const auto& b){return a.depth<b.depth;});
        const std::array<Rgb,6> palette{{
            {220,174,92},{78,142,216},{222,222,226},
            {181,105,72},{102,184,132},{187,100,180}}};
        for(const auto& face:draw)triangle(image,face,palette[face.part%palette.size()]);
        auto perspective_only=projection;
        perspective_only.camera.rotation={{{4096,0,0},{0,4096,0},{0,0,4096}}};
        perspective_only.camera.translation={0,0,0};
        std::vector<DrawFace> original_perspective;
        original_perspective.reserve(model.primary_faces.size());
        for(const auto& face:model.primary_faces) {
            DrawFace projected_face{};
            projected_face.part=face.corners[2].part;
            for(std::size_t corner=0;corner<3;++corner) {
                const auto part=face.corners[corner].part;
                const auto rotated=rotate_trace(
                    original_part_matrices[part],face.corners[corner].position);
                const nba97::ZdomfWorldVec3 camera{
                    rotated.x+original_origins[part].x,
                    rotated.y+original_origins[part].y,
                    rotated.z+original_origins[part].z};
                const auto projected=nba97::project_zdomf_vertex(
                    perspective_only,narrow(camera));
                projected_face.p[corner]={double(projected.x),double(projected.y)};
                projected_face.depth+=camera.z/3.0;
            }
            original_perspective.push_back(projected_face);
        }
        std::sort(original_perspective.begin(),original_perspective.end(),
                  [](const auto& a,const auto& b){return a.depth>b.depth;});
        for(const auto& face:original_perspective)
            triangle(image,face,palette[face.part%palette.size()]);
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
        std::cout << "ZDOMF LAYOUT: transformed=0x" << std::hex
                  << model.layout.transformed_vertex_offset << "..0x"
                  << model.layout.transformed_vertex_end << std::dec << '\n';
        std::cout << "ZDOMF PART COUNTS:";
        for (std::size_t part = 0; part < model.part_triangle_counts.size(); ++part)
            std::cout << ' ' << part << '=' << model.part_triangle_counts[part];
        std::cout << '\n';
        std::cout<<"ZDOMF RUNTIME SMOKE: PASS clips=6 create=clip1/18keys/36ticks joints=20"
                 <<" trace=f0c0(part="<<int(trace_raw.part)<<",raw="
                 <<trace_raw.position.x<<'/'<<trace_raw.position.y<<'/'<<trace_raw.position.z
                 <<",base="<<trace_base.position.x<<'/'<<trace_base.position.y<<'/'<<trace_base.position.z
                 <<",origin="<<runtime.part_origins[trace_base.part].x<<'/'
                 <<runtime.part_origins[trace_base.part].y<<'/'
                 <<runtime.part_origins[trace_base.part].z
                 <<",angles="<<pose.joints[trace_base.part].angles.x<<'/'
                 <<pose.joints[trace_base.part].angles.y<<'/'
                 <<pose.joints[trace_base.part].angles.z
                 <<",m="<<runtime.part_matrices[trace_base.part].rotation[0][0]<<'/'
                 <<runtime.part_matrices[trace_base.part].rotation[0][1]<<'/'
                 <<runtime.part_matrices[trace_base.part].rotation[0][2]<<';'
                 <<runtime.part_matrices[trace_base.part].rotation[1][0]<<'/'
                 <<runtime.part_matrices[trace_base.part].rotation[1][1]<<'/'
                 <<runtime.part_matrices[trace_base.part].rotation[1][2]<<';'
                 <<runtime.part_matrices[trace_base.part].rotation[2][0]<<'/'
                 <<runtime.part_matrices[trace_base.part].rotation[2][1]<<'/'
                 <<runtime.part_matrices[trace_base.part].rotation[2][2]
                 <<",viewM00="<<runtime.frontend_view_transform.rotation[0][0]
                 <<",originalComposite="
                 <<original_part_matrix.rotation[0][0]<<'/'
                 <<original_part_matrix.rotation[0][1]<<'/'
                 <<original_part_matrix.rotation[0][2]<<';'
                 <<original_part_matrix.rotation[1][0]<<'/'
                 <<original_part_matrix.rotation[1][1]<<'/'
                 <<original_part_matrix.rotation[1][2]<<';'
                 <<original_part_matrix.rotation[2][0]<<'/'
                 <<original_part_matrix.rotation[2][1]<<'/'
                 <<original_part_matrix.rotation[2][2]
                 <<",rotationXYZ(original="<<original_rotated.x<<'/'
                 <<original_rotated.y<<'/'<<original_rotated.z
                 <<",portStaged="<<port_staged.x<<'/'<<port_staged.y<<'/'
                 <<port_staged.z<<')'
                 <<",camera="<<trace_world.x<<'/'<<trace_world.y<<'/'<<trace_world.z<<')'
                 <<" rawXYZ="<<raw_bounds[0]<<'/'<<raw_bounds[1]<<'/'<<raw_bounds[2]
                 <<".."<<raw_bounds[3]<<'/'<<raw_bounds[4]<<'/'<<raw_bounds[5]
                 <<" baseXYZ="<<base_bounds[0]<<'/'<<base_bounds[1]<<'/'<<base_bounds[2]
                 <<".."<<base_bounds[3]<<'/'<<base_bounds[4]<<'/'<<base_bounds[5]
                 <<" worldXYZ="<<world_bounds[0]<<'/'<<world_bounds[1]<<'/'<<world_bounds[2]
                 <<".."<<world_bounds[3]<<'/'<<world_bounds[4]<<'/'<<world_bounds[5]
                 <<" outlier=f"<<largest_edge_face<<" parts="
                 <<int(largest_edge_parts[0])<<'/'<<int(largest_edge_parts[1])<<'/'
                 <<int(largest_edge_parts[2])<<" xyz="
                 <<" source="
                 <<largest_edge_source[0].x<<'/'<<largest_edge_source[0].y<<'/'<<largest_edge_source[0].z<<';'
                 <<largest_edge_source[1].x<<'/'<<largest_edge_source[1].y<<'/'<<largest_edge_source[1].z<<';'
                 <<largest_edge_source[2].x<<'/'<<largest_edge_source[2].y<<'/'<<largest_edge_source[2].z
                 <<" world="
                 <<largest_edge_world[0].x<<'/'<<largest_edge_world[0].y<<'/'<<largest_edge_world[0].z<<';'
                 <<largest_edge_world[1].x<<'/'<<largest_edge_world[1].y<<'/'<<largest_edge_world[1].z<<';'
                 <<largest_edge_world[2].x<<'/'<<largest_edge_world[2].y<<'/'<<largest_edge_world[2].z
                 <<" original="
                 <<largest_edge_original[0].x<<'/'<<largest_edge_original[0].y<<'/'<<largest_edge_original[0].z<<';'
                 <<largest_edge_original[1].x<<'/'<<largest_edge_original[1].y<<'/'<<largest_edge_original[1].z<<';'
                 <<largest_edge_original[2].x<<'/'<<largest_edge_original[2].y<<'/'<<largest_edge_original[2].z
                 <<" projected="
                 <<largest_edge_projected[0].x<<'/'<<largest_edge_projected[0].y<<';'
                 <<largest_edge_projected[1].x<<'/'<<largest_edge_projected[1].y<<';'
                 <<largest_edge_projected[2].x<<'/'<<largest_edge_projected[2].y
                 <<" autoFit="
                 <<int(largest_edge_autofit[0].x)<<'/'<<int(largest_edge_autofit[0].y)<<';'
                 <<int(largest_edge_autofit[1].x)<<'/'<<int(largest_edge_autofit[1].y)<<';'
                 <<int(largest_edge_autofit[2].x)<<'/'<<int(largest_edge_autofit[2].y)
                 <<" edge2="<<largest_edge_squared
                 <<" visible="<<visible<<" saturated="<<saturated
                 <<" rtps="<<(bounds[2]-bounds[0]+1)<<'x'<<(bounds[3]-bounds[1]+1)
                 <<" solid_faces="<<draw.size()<<" frame="<<output.string()<<'\n';
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"ZDOMF RUNTIME SMOKE: FAIL - "<<error.what()<<'\n';
        return 1;
    }
}
