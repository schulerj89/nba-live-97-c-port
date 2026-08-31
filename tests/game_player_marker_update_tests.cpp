#include "recovered/game_player_marker_update.h"
#include "game_render_backend.hpp"
#include "game_player_marker_update.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace {
unsigned checks;void check(bool v){++checks;if(!v){std::fprintf(stderr,"marker update check %u failed\n",checks);std::abort();}}
constexpr uint32_t A=0x80120000,T=0x80122000,S=0x80123000,CTRL=0x80124000,PHYS=0x80125000,P=0x800eba50,F=0x80109b90,PACK=0x800d8f14;
struct Fixture {
    std::vector<uint8_t> bytes=std::vector<uint8_t>(0x200000),known=std::vector<uint8_t>(0x200000,1);
    Nba97PlayerMarkerContext context{access,io,this,10000};Nba97PlayerMarkerProgress progress{};
    nba97::GameRenderBackend backend;std::vector<Nba97PlayerMarkerCall> calls;
    unsigned transfers=0,refuse=0;bool mutate=false;
    void put(uint32_t a,uint32_t v,unsigned n=4){for(unsigned i=0;i<n;++i){bytes[a-0x80000000+i]=static_cast<uint8_t>(v>>(8*i));known[a-0x80000000+i]=1;}}
    uint32_t get(uint32_t a,unsigned n=4)const{uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(bytes[a-0x80000000+i])<<(8*i);return v;}
    Fixture(){put(0x800fac20,0xffffffff);put(0x800fdb58,1);put(0x800fc650,T);put(T,A);put(A+0x1c,S);put(S+0x20,24u<<10,2);
        put(0x800fc634,CTRL);put(0x800fc654,PHYS);put(0x800fc65c,CTRL+4);put(CTRL+4,A);put(0x800fc63c,CTRL+8);put(0x800fc660,CTRL+10);
        put(0x800b72d4,8);put(0x800b72a0,10,1);put(0x800b72a8,20,1);put(0x800b72b0,30,1);
        for(uint32_t p:{P,F}){put(p,0x23);put(p+4,16,2);put(p+6,1,2);}
        backend.unmaskedTransfersKnown=true;backend.sdkTransferLimitsKnown=true;backend.sdkTransferWidth=1024;backend.sdkTransferHeight=512;
        backend.memory.add(std::vector<uint8_t>(0x200000),{},0);
    }
    static int access(void* p,uint32_t,uint32_t a,unsigned n,unsigned kind,Nba97PlayerFrameValue* v){
        auto& f=*static_cast<Fixture*>(p);if(a<0x80000000||uint64_t(a)+n>0x80200000)return NBA97_BODY_BOUNDS;
        for(unsigned i=0;i<n;++i)if(f.known[a-0x80000000+i]>1)return NBA97_BODY_ARGUMENT;
        if(kind)f.put(a,v->word,n);else for(unsigned i=0;i<n;++i)if(f.known[a-0x80000000+i]){v->word|=uint32_t(f.bytes[a-0x80000000+i])<<(8*i);v->known_mask|=static_cast<uint8_t>(1u<<i);}
        return 1;
    }
    static int transfer(void* p,const Nba97GameImageTransfer* e){
        auto& f=*static_cast<Fixture*>(p);auto view=f.backend.memory.buffer(1,0,f.bytes.size());
        for(std::size_t i=0;i<f.bytes.size();++i)view.data[i]=f.bytes[i];
        Nba97GameImageMemory memory{};check(f.backend.memory.describe(view,memory));auto q=*e;q.source.memory=&memory;
        int result=nba97::GameRenderBackend::transferIo(&f.backend,&q);if(result==1)++f.transfers;return result;
    }
    static int io(void* p,const Nba97PlayerMarkerCall* q,Nba97GamePeriodValue* result){
        auto& f=*static_cast<Fixture*>(p);f.calls.push_back(*q);if(f.refuse==f.calls.size())return NBA97_BODY_BOUNDS;
        if(q->entry==0x800946b8){Nba97GameImageMemory memory{f.bytes.data(),f.known.data(),f.bytes.size(),0,1};Nba97GameImageUploadState state{f.get(0x800d7b14),1};Nba97GameImageUploadProgress progress{};
            Nba97GameImagePlacement placement{static_cast<int32_t>(q->args[1]),static_cast<int32_t>(q->args[2]),static_cast<int32_t>(q->args[3]),static_cast<int32_t>(q->args[4])};
            int rc=nba97_game_image_upload(&state,{&memory,int64_t(q->args[0])-0x80000000},placement,100,transfer,&f,&progress);
            f.put(0x800d7b14,state.pending_d7b14);if(rc!=1)return NBA97_BODY_BOUNDS;
        }else if(q->entry==0x800994f4){if(f.mutate){f.put(0x801029b0,1);f.put(0x8001ede8,1);f.put(0x800c55c0,2,1);}}
        else return NBA97_BODY_ARGUMENT;
        *result={0,1};return 1;
    }
    int run(){return nba97_game_player_marker_update(&context,&progress);}
};
}
int main(){
    for(uint32_t gate:{0u,1u,0x7fffffffu,0x80000000u,0xfffffffeu}){
        Fixture f;f.put(0x800fac20,gate);f.known[0xfdb58]=0;check(f.run()==1&&f.progress.stores==1&&f.calls.empty());check(f.get(0x800fac20)==gate+1);
    }
    {
        Fixture f;check(f.run()==1&&f.progress.completed&&f.progress.packets==1);check(f.get(0x800fed1c)==P);
        check(f.get(P+0x10,2)==1&&f.get(P+0x14,2)==24u*1057&&f.get(P+0x16,2)==24u*1057);
        check(f.get(PACK+4,1)==10&&f.get(PACK+5,1)==20&&f.get(PACK+6,1)==30);
        check(f.get(PACK+12,1)==128&&f.get(PACK+20,1)==159&&f.get(PACK+29,1)==191);
        check(f.transfers==1&&f.calls.size()==2&&f.calls[0].pc==0x80050e4c&&f.calls[1].pc==0x80050e54);
        uint16_t pixel=0;check(f.backend.vram.word(512,226,pixel)&&pixel==1);check(f.backend.vram.word(514,226,pixel)&&pixel==24u*1057);
        check(f.get(P+12,2)==512&&f.get(P+14,2)==226&&f.get(0x800d7b14)==1);
    }
    for(unsigned route=0;route<4;++route){
        Fixture f;if(route==0)f.put(0x800fdb58,0);else if(route==1){f.put(0x800fdb90,0x82,2);f.put(0x800fe88e,1,2);f.put(0x800d7a70,32);}else {f.put(A+4,0xffff,2);f.put(PHYS,9);if(route==2)f.put(PHYS+0xce,0,1);else {f.put(PHYS+0xce,1,1);f.put(CTRL+8,0,2);}}
        check(f.run()==1&&f.get(0x800fed1c)==F);check(f.get(PACK+12,1)==32&&f.get(PACK+20,1)==63);check(f.transfers==1);
    }
    {
        Fixture f;f.put(CTRL,1,2);f.put(A+0xde,3,1);f.put(0x800b72d4,0);check(f.run()==1);
        check(f.get(P+0x14,2)==0&&f.get(P+0x16,2)==1057); // unselected zero clamp
        Fixture c;c.put(A+4,0xffff,2);c.put(PHYS+0xde,3,1);c.put(0x800b72d4,0);check(c.run()==1);
        check(c.get(P+0x14,2)==0&&c.get(P+0x16,2)==0); // computer route deliberately has no zero clamp
    }
    {
        Fixture f;f.refuse=1;check(f.run()==NBA97_BODY_BOUNDS&&f.progress.stopped_pc==0x80050e4c);
        check(f.get(P+0x14,2)==24u*1057&&f.get(PACK+0x16,2)==11&&f.transfers==0);
        Fixture sync;sync.refuse=2;check(sync.run()==NBA97_BODY_BOUNDS&&sync.progress.stopped_pc==0x80050e54&&sync.transfers==1);check(sync.get(P+14,2)==226);
        Fixture missing;missing.context.io=nullptr;check(missing.run()==NBA97_MARKER_IO_REQUIRED&&missing.progress.stopped_pc==0x80050e4c);
    }
    {
        Fixture f;f.mutate=true;check(f.run()==1);const uint32_t later=PACK+120;
        check(f.get(later+0xe,2)==((227u<<6)|32)&&f.get(later+12,1)==128&&f.get(PACK+12,1)==0);
        check(f.get(P+14,2)==226); // upload captured prior index; later writes see the new live index
    }
    {
        Fixture f;nba97::GamePlayerMarkerUpdate owner;owner.memory.access=Fixture::access;owner.memory.user=&f;
        owner.io=Fixture::io;owner.user=&f;check(owner.run(10000,f.progress)==1&&f.transfers==1);
        check(f.calls[0].args[0]==P&&f.calls[0].args[1]==0&&f.calls[0].args[2]==0&&f.calls[0].args[3]==512&&f.calls[0].args[4]==226);
        owner.memory.access=nullptr;check(owner.run(10000,f.progress)==NBA97_BODY_ARGUMENT&&!f.progress.completed);
        Fixture no_io;owner.memory.access=Fixture::access;owner.memory.user=&no_io;owner.io=nullptr;
        check(owner.run(10000,no_io.progress)==NBA97_MARKER_IO_REQUIRED&&no_io.progress.stopped_pc==0x80050e4c);
        no_io.put(0x800fac20,0);check(owner.run(10000,no_io.progress)==1); // unvisited IO not required
    }
    std::printf("game_player_marker_update: %u checks passed\n",checks);
}
