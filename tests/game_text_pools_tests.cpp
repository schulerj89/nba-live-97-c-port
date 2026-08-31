#include "recovered/game_text_pools.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace {
unsigned checks=0;
void check(bool ok){++checks;if(!ok){std::fprintf(stderr,"text pools check %u failed\n",checks);std::exit(1);}}
constexpr uint32_t Globals=0x800b2000u,Control=0x801029c0u,Descriptor=0x80130000u,Arena=0x80140000u;
struct Fixture {
    std::array<std::vector<uint8_t>,4> data,known;std::array<Nba97GameTextRegion,4> regions{};
    Nba97GameTextPoolArguments args{16,500,5,200,256,0x80024958u,400,400};
    Nba97GameTextPoolContext context{};Nba97GameTextPoolProgress out{};
    std::vector<Nba97GameTextPoolEvent> journal=std::vector<Nba97GameTextPoolEvent>(100000);
    unsigned mode=0,calls=0;uint32_t style=Arena;
    Fixture(){const uint32_t bases[]={Globals,Control,Descriptor,Arena};const size_t sizes[]={0x4000,16,16,0x30000};
        for(unsigned i=0;i<4;++i){data[i].resize(sizes[i],0xcd);known[i].resize(sizes[i],static_cast<uint8_t>(i<2));
            regions[i]={bases[i],data[i].data(),known[i].data(),sizes[i]};}
        context={{regions.data(),regions.size()},allocate,this};put(Control,0);
    }
    void put(uint32_t address,uint32_t value){for(auto& r:regions)if(address>=r.base&&address-r.base+4<=r.size){auto off=address-r.base;
        for(unsigned i=0;i<4;++i){r.data[off+i]=static_cast<uint8_t>(value>>(i*8));r.known[off+i]=1;}return;}check(false);}
    uint32_t get(uint32_t address){for(auto& r:regions)if(address>=r.base&&address-r.base+4<=r.size){uint32_t v=0;auto off=address-r.base;
        for(unsigned i=0;i<4;++i)v|=uint32_t(r.data[off+i])<<(i*8);return v;}check(false);return 0;}
    static int allocate(void* user,const Nba97GameTextMemory*,const Nba97GameTextPoolEvent* event,Nba97GameTextPoolValue* returned){
        auto& f=*static_cast<Fixture*>(user);++f.calls;check(event->kind==NBA97_TEXT_POOL_ALLOCATE_9027C&&event->pc==0x80090234u);
        check(event->argument[0]==f.args.name&&event->argument[2]==0x20&&event->argument[3]==1);
        // Explicit allocator fixture boundary. This is not a production9027C.
        if(f.mode==1)return 0;
        f.put(Descriptor,f.mode==4?0:f.style);
        if(f.mode==5)f.put(Control,Descriptor);
        if(f.mode==6)f.put(Control,Control+8);
        *returned={f.mode==3?0:Descriptor,static_cast<uint8_t>(f.mode!=2)};return 1;
    }
    int run(size_t capacity=100000){return nba97_game_text_pools(&context,&args,journal.data(),capacity,&out);}
};
void ordinary(){Fixture f;check(f.run()==NBA97_TEXT_COMPLETE);check(f.calls==1&&f.out.completed&&f.out.requested_size==90360);
    check(f.out.events==2163&&f.out.stores==2162&&f.out.callbacks_completed==1&&f.out.return_v0==0);
    check(f.get(Arena+0x18)==Arena+0x58&&f.get(Arena+8)==Arena+0x58+64000);
    check(f.get(Arena+0x10)==Arena+0x58+64000+10000&&f.get(Arena+0xc)==Arena+0x58+64000+10000+12800);
    check(f.get(Arena+0x14)==Arena+0x58+64000+10000+12800+2560);
    check(f.get(Arena+0x1c)==Arena+89960&&f.get(0x800b2048)==Arena);
    check(f.get(Arena)==0xcdcdcdcdu&&f.known[3][0]==0&&f.known[3][0x42]==0&&f.known[3][0x58]==0);
    check(f.data[3][0x52]==16&&f.data[3][0x53]==0&&f.data[3][0x20]==0x90&&f.data[3][0x21]==1);
    for(unsigned i=0;i<1280;++i){const auto off=f.get(Arena+0xc)-Arena+i*2;check(f.data[3][off]==255&&f.data[3][off+1]==255&&f.known[3][off]==1);}
    for(unsigned i=0;i<256;++i){auto off=f.get(Arena+0x14)-Arena+i*2;check(f.data[3][off]==255&&f.data[3][off+1]==255);}
    for(unsigned i=0;i<200;++i){auto off=f.get(Arena+0x10)-Arena+i*64;check(f.data[3][off+0x12]==255&&f.known[3][off+0x12]==1&&f.known[3][off]==0);}
    for(unsigned i=0;i<400;++i){auto off=f.get(Arena+0x1c)-Arena+i;check(f.data[3][off]==0&&f.known[3][off]==1);}
    size_t published=0;for(size_t i=0;i<f.out.events;++i)if(f.journal[i].pc==0x8002e3d4u)published=i;
    check(published>0&&f.journal[published+1].pc==0x8002e3dcu&&f.journal[published+2].pc==0x8002e3f4u);
    Fixture interrupted;check(interrupted.run(published+1)==NBA97_TEXT_LIMIT);
    check(interrupted.get(0x800b2048)==Arena&&!interrupted.out.completed&&interrupted.known[3][0x20]==0);
}
void failures(){
    {Fixture f;f.context.io=nullptr;check(f.run()==NBA97_TEXT_IO_REFUSED);check(f.out.events==1&&!f.journal[0].completed&&f.out.stores==0);}
    {Fixture f;f.put(Control,Control+4);f.mode=1;check(f.run()==NBA97_TEXT_IO_REFUSED);check(f.out.events==2&&f.out.stores==1&&f.get(Control+4)==1);}
    {Fixture f;f.put(Control,Control+4);f.mode=2;check(f.run()==NBA97_TEXT_UNKNOWN);check(f.out.events==3&&f.get(Control+4)==0&&f.out.stopped_pc==0x80090170u);}
    {Fixture f;f.mode=3;check(f.run()==NBA97_TEXT_RESOURCE);check(f.out.stopped_pc==0x80090170u&&f.out.stopped_address==0);}
    {Fixture f;f.mode=4;check(f.run()==NBA97_TEXT_RESOURCE);check(f.out.stopped_pc==0x8002e2b4u&&f.out.stopped_address==0x18);}
    {Fixture f;f.put(Control,Control+4);f.mode=5;check(f.run()==NBA97_TEXT_RESOURCE);check(f.get(Control+4)==1&&f.get(Descriptor)==0);check(f.out.stopped_pc==0x8002e2b4u);}
    {Fixture f;f.put(Control,Control+4);f.mode=6;check(f.run()==NBA97_TEXT_COMPLETE);check(f.get(Control+4)==1&&f.get(Control+8)==0);}
    {Fixture f;f.known[1][0]=0;f.known[1][3]=2;check(f.run()==NBA97_TEXT_ARGUMENT);check(!f.out.events&&!f.calls);}
    {Fixture f;f.known[3][0x24]=0;f.known[3][0x25]=2;check(f.run()==NBA97_TEXT_ARGUMENT);check(f.out.stopped_pc==0x8002e324u&&f.out.stores==19&&f.known[3][0x24]==0);}
    {Fixture f;f.style=Arena+1;check(f.run()==NBA97_TEXT_ALIGNMENT_TRAP);check(f.out.stopped_pc==0x8002e2b4u&&f.out.stores==0);}
    for(size_t cap=0;cap<2163;cap+=71){Fixture f;check(f.run(cap)==NBA97_TEXT_LIMIT);check(f.out.events==cap&&!f.out.completed);}
}
void quirks(){
    {Fixture f;f.args={0x1ff,0xffff,0x100,0x100,0xffff,0x80024958u,0xdeadbeefu,0xffff};check(f.run()==NBA97_TEXT_COMPLETE);
        check(f.out.requested_size==0xffffffa1u&&f.out.return_v0==0xffffffffu&&f.data[3][0x52]==255);}
    {Fixture f;f.args={16,0,0,0,0,0x80024958u,0xdeadbeefu,32};f.style=0x800b202cu;check(f.run()==NBA97_TEXT_COMPLETE);
        // B2048 aliasesstyle+1C: publication changes the live bitmapbase, then
        // byte28 clears that pointer's lowbyte and subsequent reads move again.
        check(f.get(0x800b2048)==0x800b2000u);check(f.data[0][0x1d]==0&&f.data[0][0x2c]==0);}
    {Fixture a,b;b.args.unused_argument6=0xffffffffu;check(a.run()==1&&b.run()==1);check(a.data==b.data&&a.known==b.known);}
}
}
int main(){ordinary();failures();quirks();std::printf("game_text_pools: %u checks passed\n",checks);}
