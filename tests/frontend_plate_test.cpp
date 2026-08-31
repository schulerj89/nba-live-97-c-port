#include "frontend_plate.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
unsigned checks = 0;
void check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
PshImage image(unsigned width, unsigned height, std::array<uint8_t,4> color) {
    PshImage result;
    result.width = static_cast<uint16_t>(width);
    result.height = static_cast<uint16_t>(height);
    result.rgba.resize(width*height*4);
    for (std::size_t i=0; i<result.rgba.size(); ++i) result.rgba[i]=color[i%4];
    return result;
}
std::array<uint8_t,4> pixel(const PshImage& source, unsigned x, unsigned y) {
    const auto at=(std::size_t(y)*source.width+x)*4;
    return {source.rgba[at],source.rgba[at+1],source.rgba[at+2],source.rgba[at+3]};
}
void set(PshImage& target, unsigned x, unsigned y, std::array<uint8_t,4> value) {
    const auto at=(std::size_t(y)*target.width+x)*4;
    for(unsigned c=0;c<4;++c)target.rgba[at+c]=value[c];
}
PshImage coordinates(unsigned width=103, unsigned height=60) {
    auto result=image(width,height,{0,0,0,255});
    for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x)
        set(result,x,y,{uint8_t(x),uint8_t(y),uint8_t(17+x+y),255});
    return result;
}
template<class Action>
void rejectsUnchanged(PshImage& destination, Action action) {
    const auto before=destination.rgba;
    bool caught=false;
    try {action();} catch(const std::runtime_error&) {caught=true;}
    check(caught,"invalid plate input was accepted");
    check(destination.rgba==before,"plate refusal partially painted destination");
}

void tests() {
    constexpr std::array<uint8_t,4> background{7,11,13,255};
    for(int side=0;side<2;++side) {
        auto destination=image(140,80,background);
        auto source=coordinates();
        auto frame=image(120,62,{30,40,50,255});
        const int x=12,y=3,frame_x=x-(side?2:10),frame_y=y-1;
        // Known texels may remain visible through a transparent frame. The
        // unavailable edge must be covered, not every pixel in the frame.
        set(frame,unsigned(20+x-frame_x),unsigned(20+y-frame_y),{0,0,0,0});
        nba97::drawFrontendPlate(destination,source,x,y,side,frame,frame_x,frame_y);
        for(const auto point: {std::array<unsigned,2>{20,20},{53,30},{80,40}})
            check(pixel(destination,x+point[0],y+point[1])==pixel(source,point[0],point[1]),
                  "fixed plate UV must equal local XY, without rectangular warping");
        check(pixel(destination,x+(side?0:100),y+(side?0:10))==background,
              "fixed shape painted outside its polygon");
        const unsigned edge_y=side?30:59;
        check(pixel(destination,x+103,y+edge_y)==background,
              "occluded unavailable edge was painted from invented padding");

        auto alpha_source=source;
        set(alpha_source,20,20,{91,92,93,0});
        set(alpha_source,80,40,{0,0,0,255});
        destination=image(140,80,background);
        nba97::drawFrontendPlate(destination,alpha_source,x,y,side,frame,frame_x,frame_y);
        check(pixel(destination,x+20,y+20)==background,"transparent source texel changed destination");
        check(pixel(destination,x+80,y+40)==std::array<uint8_t,4>{0,0,0,255},
              "opaque black source texel was RGB-keyed away");

        // A late unavailable sample must refuse before earlier visible
        // source texels are written. Partial alpha is not full occlusion.
        for(uint8_t alpha: {uint8_t(0),uint8_t(254)}) {
            auto broken=frame;
            set(broken,unsigned(103+x-frame_x),unsigned(edge_y+y-frame_y),{1,2,3,alpha});
            rejectsUnchanged(destination,[&]{nba97::drawFrontendPlate(destination,source,x,y,side,
                                                                       broken,frame_x,frame_y);});
        }
        rejectsUnchanged(destination,[&]{nba97::drawFrontendPlate(destination,source,x,y,side,frame,300,300);});

        // A genuinely present edge texel needs no foreground opacity.
        const auto full_source=coordinates(106);
        const auto clear_frame=image(120,62,{0,0,0,0});
        destination=image(140,80,background);
        nba97::drawFrontendPlate(destination,full_source,x,y,side,clear_frame,frame_x,frame_y);
        check(pixel(destination,x+103,y+edge_y)==pixel(full_source,103,edge_y),
              "available edge texel incorrectly required foreground coverage");
    }

    auto destination=image(140,80,background),source=coordinates(),frame=image(120,62,{1,2,3,255});
    for(int side: {-1,2})
        rejectsUnchanged(destination,[&]{nba97::drawFrontendPlate(destination,source,12,3,side,frame,10,2);});
    auto short_source=source;short_source.rgba.pop_back();
    rejectsUnchanged(destination,[&]{nba97::drawFrontendPlate(destination,short_source,12,3,1,frame,10,2);});
    auto short_frame=frame;short_frame.rgba.pop_back();
    rejectsUnchanged(destination,[&]{nba97::drawFrontendPlate(destination,source,12,3,1,short_frame,10,2);});
    auto short_destination=destination;short_destination.rgba.pop_back();
    rejectsUnchanged(short_destination,[&]{nba97::drawFrontendPlate(short_destination,source,12,3,1,frame,10,2);});
    auto zero=source;zero.width=0;zero.rgba.clear();
    rejectsUnchanged(destination,[&]{nba97::drawFrontendPlate(destination,zero,12,3,1,frame,10,2);});
    rejectsUnchanged(destination,[&]{nba97::drawFrontendPlate(destination,destination,12,3,1,frame,10,2);});
    rejectsUnchanged(destination,[&]{nba97::drawFrontendPlate(destination,source,12,3,1,destination,10,2);});

    // Destination clipping is independent of texture availability. Extreme
    // signed origins must not wrap into the image or into the frame lookup.
    auto clipped=image(6,60,background);
    nba97::drawFrontendPlate(clipped,source,-100,0,1,frame,-102,-1);
    check(pixel(clipped,0,30)==pixel(source,100,30),"negative origin lost visible identity samples");
    check(pixel(clipped,3,30)==background,"clipped unavailable edge used a clamped source texel");
    const auto before=clipped.rgba;
    const auto clear_frame=image(1,1,{0,0,0,0});
    for(int origin: {(std::numeric_limits<int>::min)(),(std::numeric_limits<int>::max)()})
        nba97::drawFrontendPlate(clipped,source,origin,origin,1,clear_frame,0,0);
    check(clipped.rgba==before,"fully clipped extreme origin changed destination");
}
} // namespace

int main() {
    try {tests();std::cout<<"FRONTEND PLATE PASS: "<<checks
        <<" identity UV, shape, occlusion, refusal and clipping assertions; native coverage only\n";return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
