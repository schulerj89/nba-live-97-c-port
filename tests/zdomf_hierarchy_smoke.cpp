#include "zdomf_hierarchy.hpp"
#include "zdomf_model.hpp"
#include "zdomf_projection.hpp"
#include "zdomf_transform.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
struct Rgb { std::uint8_t r=0,g=0,b=0; };
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
nba97::ZdomfVec3 narrow(nba97::ZdomfWorldVec3 value) {
    return {static_cast<std::int16_t>(value.x),
            static_cast<std::int16_t>(value.y),
            static_cast<std::int16_t>(value.z)};
}
void write_ppm(const std::filesystem::path& path,const std::vector<Rgb>& image) {
    std::ofstream output(path,std::ios::binary);
    if(!output)throw std::runtime_error("cannot create hierarchy smoke frame");
    output<<"P6\n512 240\n255\n";
    output.write(reinterpret_cast<const char*>(image.data()),
                 static_cast<std::streamsize>(image.size()*sizeof(Rgb)));
}
}

int main(int argc,char** argv) {
    try {
        if(argc!=3)throw std::runtime_error("usage: hierarchy-smoke <model-root> <output.ppm>");
        const std::filesystem::path root=argv[1],output=argv[2];
        auto model=nba97::load_zdomf_model(root/"ZDOMFATL.BIN");
        const auto transforms=nba97::load_zdomf_base_transforms(
            root/"ZDEFLIST.BIN",root/"ZDOMTRIG.BIN");
        const auto projection=nba97::load_create_player_projection(root/"ZDOMTRIG.BIN");
        for(std::size_t part=0;part<model.pivots.size();++part)
            model.pivots[part]=nba97::apply_zdomf_transform(
                transforms.parts[part],model.pivots[part]);
        for(auto& face:model.primary_faces)for(auto& corner:face.corners)
            corner.position=nba97::apply_zdomf_transform(
                transforms.parts[corner.part],corner.position);
        const auto hierarchy=nba97::build_zdomf_hierarchy(model.pivots);

        std::array<int,4> bounds{{512,240,-1,-1}};
        std::array<int,4> flat_bounds{{512,240,-1,-1}};
        std::size_t saturated=0,visible=0;
        std::vector<Rgb> image(512*240);
        std::vector<std::array<int,4>> skeleton;
        std::vector<std::array<int,2>> points;
        for(const auto& node:hierarchy.parts) {
            const auto a=nba97::project_zdomf_vertex(projection,narrow(node.joint_origin));
            const auto b=nba97::project_zdomf_vertex(projection,narrow(node.joint_end));
            line(image,a.x,a.y,b.x,b.y,{255,190,0});
            skeleton.push_back({a.x,a.y,b.x,b.y});
        }
        for(const auto& face:model.primary_faces)for(const auto& corner:face.corners) {
            const auto flat=nba97::project_zdomf_vertex(projection,corner.position);
            flat_bounds[0]=std::min(flat_bounds[0],int(flat.x));
            flat_bounds[1]=std::min(flat_bounds[1],int(flat.y));
            flat_bounds[2]=std::max(flat_bounds[2],int(flat.x));
            flat_bounds[3]=std::max(flat_bounds[3],int(flat.y));
            const auto world=nba97::apply_zdomf_hierarchy(
                hierarchy,corner.part,corner.position);
            const auto projected=nba97::project_zdomf_vertex(projection,narrow(world));
            if(projected.flags!=nba97::ZdomfProjectionNone)++saturated;
            if(projected.x>=0&&projected.x<512&&projected.y>=0&&projected.y<240)++visible;
            bounds[0]=std::min(bounds[0],int(projected.x));
            bounds[1]=std::min(bounds[1],int(projected.y));
            bounds[2]=std::max(bounds[2],int(projected.x));
            bounds[3]=std::max(bounds[3],int(projected.y));
            for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)
                pixel(image,projected.x+x,projected.y+y,{220,235,255});
            points.push_back({projected.x,projected.y});
        }
        const int width=bounds[2]-bounds[0]+1,height=bounds[3]-bounds[1]+1;
        const int flat_width=flat_bounds[2]-flat_bounds[0]+1;
        const int flat_height=flat_bounds[3]-flat_bounds[1]+1;
        if(hierarchy.root_count!=3||hierarchy.max_depth!=5||visible<700||saturated!=0||
           (width<=flat_width&&height<=flat_height))
            throw std::runtime_error("hierarchy did not expand a valid on-screen model");
        // Human-readable diagnostic inset. The exact 1x GTE result remains at
        // the right; this 6x copy makes joint continuity visually reviewable
        // without changing any measured acceptance bounds.
        constexpr int zoom=6,ox=20,oy=30;
        for(const auto& edge:skeleton)
            line(image,ox+(edge[0]-bounds[0])*zoom,oy+(edge[1]-bounds[1])*zoom,
                 ox+(edge[2]-bounds[0])*zoom,oy+(edge[3]-bounds[1])*zoom,{255,190,0});
        for(const auto& point:points)for(int y=-2;y<=2;++y)for(int x=-2;x<=2;++x)
            pixel(image,ox+(point[0]-bounds[0])*zoom+x,
                  oy+(point[1]-bounds[1])*zoom+y,{220,235,255});
        write_ppm(output,image);
        std::cout<<"ZDOMF HIERARCHY SMOKE: PASS roots="<<hierarchy.root_count
                 <<" depth="<<hierarchy.max_depth<<" visible="<<visible
                 <<" saturated="<<saturated<<" flat="<<flat_width<<'x'<<flat_height
                 <<" assembled="<<width<<'x'<<height<<" frame="<<output.string()<<'\n';
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"ZDOMF HIERARCHY SMOKE: FAIL - "<<error.what()<<'\n';
        return 1;
    }
}
