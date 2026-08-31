#include "recovered/game_page_offset.h"
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
unsigned checks=0,failures=0;
void check(bool value){++checks;if(!value){++failures;std::fprintf(stderr,"failed check %u\n",checks);}}

struct Fixture {
    std::vector<std::uint8_t> values;
    std::size_t at=0;
    unsigned calls=0;
    unsigned refuse_at=0;
    int refusal=-77;
    std::uint8_t known=1;
    bool invalid_known=false;
    static int read(void *user,const Nba97GamePageOffsetReadEvent *event,
        Nba97GamePageOffsetByte *out)
    {
        auto& fixture=*static_cast<Fixture*>(user);
        ++fixture.calls;
        check(event->pc==0x800993e0u);
        check(event->address==0x800c55c0u);
        check(event->width==1);
        if(fixture.refuse_at==fixture.calls)return fixture.refusal;
        out->value=fixture.values.at(fixture.at++);
        out->known=fixture.invalid_known?2:fixture.known;
        return NBA97_GAME_PAGE_OFFSET_OK;
    }
    Nba97GamePageOffsetContext context(){return {read,this};}
};

std::uint32_t expected(bool alternate,std::uint32_t mode,std::uint32_t abr,
    std::uint32_t x,std::uint32_t y)
{
    if(alternate)return ((mode&3u)<<9)|((abr&3u)<<7)|
        ((y&0x300u)>>3)|((x&0x3ffu)>>6);
    return ((mode&3u)<<7)|((abr&3u)<<5)|((y&0x100u)>>4)|
        ((x&0x3ffu)>>6)|((y&0x200u)<<2);
}

void successful(std::vector<std::uint8_t> sequence,bool alternate,
    std::uint32_t mode,std::uint32_t abr,std::uint32_t x,std::uint32_t y)
{
    Fixture fixture{sequence};
    auto context=fixture.context();
    Nba97GamePageOffsetProgress progress{};
    std::uint32_t result=0xfeedfaceu;
    check(nba97_game_page_offset(&context,mode,abr,x,y,&result,&progress)==1);
    check(result==expected(alternate,mode,abr,x,y));
    check(progress.completed==1);
    check(progress.reads==sequence.size());
    check(fixture.calls==sequence.size());
    check(progress.stopped_pc==0&&progress.stopped_address==0);
}
}

int main()
{
    successful({1},true,2,0,0x200,0x100);
    successful({0,2},true,2,0,0x200,0x100);
    successful({2,0},false,2,0,0x200,0x100);
    successful({2,2},true,2,0,0x200,0x100);
    successful({255,1},false,7,6,0xffffffffu,0xffffffffu);

    const std::uint32_t coordinates[]={0,1,63,64,255,256,511,512,767,768,
        1023,1024,0x7fffffffu,0x80000000u,0xffffffffu};
    for(std::uint32_t mode=0;mode<8;++mode)for(std::uint32_t abr=0;abr<8;++abr)
        for(std::uint32_t x:coordinates)for(std::uint32_t y:coordinates){
            successful({1},true,mode,abr,x,y);
            successful({0,0},false,mode,abr,x,y);
        }

    {
        Fixture fixture{{0,2}};fixture.refuse_at=1;auto context=fixture.context();
        Nba97GamePageOffsetProgress progress{};std::uint32_t result=0x12345678u;
        check(nba97_game_page_offset(&context,2,0,0x200,0x100,&result,&progress)==-77);
        check(result==0x12345678u&&progress.reads==0&&!progress.completed);
        check(progress.stopped_pc==0x800993e0u&&progress.stopped_address==0x800c55c0u);
    }
    {
        Fixture fixture{{0,2}};fixture.refuse_at=2;auto context=fixture.context();
        Nba97GamePageOffsetProgress progress{};std::uint32_t result=0x12345678u;
        check(nba97_game_page_offset(&context,2,0,0x200,0x100,&result,&progress)==-77);
        check(result==0x12345678u&&progress.reads==1&&!progress.completed);
    }
    for(unsigned invalid=0;invalid<2;++invalid){
        Fixture fixture{{0}};fixture.known=0;fixture.invalid_known=invalid!=0;
        auto context=fixture.context();Nba97GamePageOffsetProgress progress{};
        std::uint32_t result=9;
        check(nba97_game_page_offset(&context,0,0,0,0,&result,&progress)==
            (invalid?NBA97_GAME_PAGE_OFFSET_ARGUMENT:NBA97_GAME_PAGE_OFFSET_UNKNOWN));
        check(result==9&&progress.reads==1&&!progress.completed);
    }
    {
        Nba97GamePageOffsetContext context{};Nba97GamePageOffsetProgress progress{};
        std::uint32_t result=7;
        check(nba97_game_page_offset(&context,0,0,0,0,&result,&progress)==
            NBA97_GAME_PAGE_OFFSET_READ_REQUIRED);
        check(result==7&&!progress.completed&&progress.reads==0);
        check(nba97_game_page_offset(nullptr,0,0,0,0,&result,&progress)==
            NBA97_GAME_PAGE_OFFSET_ARGUMENT);
        check(nba97_game_page_offset(&context,0,0,0,0,nullptr,&progress)==
            NBA97_GAME_PAGE_OFFSET_ARGUMENT);
        check(nba97_game_page_offset(&context,0,0,0,0,&result,nullptr)==
            NBA97_GAME_PAGE_OFFSET_ARGUMENT);
    }
    std::printf("%u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
