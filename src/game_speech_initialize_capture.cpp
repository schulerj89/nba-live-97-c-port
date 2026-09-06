#include "game_speech_initialize_capture.h"
#include <array>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 {
namespace {
struct Fixture {
    const Nba97GameTextMemory* memory;
    std::array<unsigned,3> attempts{};
    std::vector<std::uint32_t> destinations,identifiers,categories;
    unsigned copies=0,conversions=0,releases=0;
    std::uint32_t allocated_size=0,released_pointer=0;
    std::uint8_t* locate(std::uint32_t a,unsigned width,bool mark=false) const {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            if(r.known)for(unsigned i=0;i<width;++i){if(mark)r.known[a-r.base+i]=1;else if(r.known[a-r.base+i]!=1)throw std::runtime_error("speech fixture unknown data");}
            return r.data+a-r.base;}
        throw std::runtime_error("speech fixture mapping missing");
    }
    void put(std::uint32_t a,std::uint32_t v,unsigned width=4) {auto* p=locate(a,width,true);for(unsigned i=0;i<width;++i)p[i]=std::uint8_t(v>>(8*i));}
    std::uint32_t get(std::uint32_t a,unsigned width=4) const {const auto* p=locate(a,width);std::uint32_t v=0;for(unsigned i=0;i<width;++i)v|=std::uint32_t(p[i])<<(8*i);return v;}
    static int load(void* user,const Nba97GameTextMemory*,const Nba97GameResourceLoaderEvent* e,Nba97GameResourceLoaderValue* value) {
        auto& f=*static_cast<Fixture*>(user);unsigned slot=3;
        for(unsigned i=0;i<3;++i)if(e->argument[0]==std::array<std::uint32_t,3>{0x80027b28u,0x80027b34u,0x80027b78u}[i])slot=i;
        if(slot==3 || e->entry!=0x800941c8u || e->argument[1]!=(slot==2?0x20u:0u))return 0;
        *value={++f.attempts[slot]==1?0u:0x80130000u+(e->argument[0]&0xffffu),1};return 1;
    }
    static int child(void* user,const Nba97GameTextMemory*,const Nba97GameSpeechInitializeEvent* e,Nba97GameSpeechInitializeRegisters* r) {
        auto& f=*static_cast<Fixture*>(user);const auto a0=r->gpr[4].word,a1=r->gpr[5].word,a2=r->gpr[6].word,a3=r->gpr[7].word;
        r->gpr[2]={e->entry,15};
        if(e->entry==0x8007fc08u) {
            if(a3<0x80102fe0u || a3>=0x80103490u || (a3-0x80102fe0u)%12u)return 0;
            const auto index=(a3-0x80102fe0u)/12u,length=(index%4u+1u)*4u;
            f.destinations.push_back(a3);f.identifiers.push_back(a2);f.categories.push_back(a1);
            f.put(a3,index%3u?0u:0x80150000u+index*16u);f.put(a3+4u,length);f.put(a3+8u,(a1<<24)^a2);
            for(unsigned j=0;j<length;++j)f.put(0x80150000u+index*16u+j,(index*7u+j)&255u,1);
        } else if(e->entry==0x80090160u) {
            if(a0!=0x80027ba4u || a2!=0)return 0;
            f.allocated_size=a1;r->gpr[2]={0x80160000u,15};
        } else if(e->entry==0x8009cb0cu) {
            // Explicit synthetic copy service, not a second recovered owner.
            const auto* source=f.locate(a1,a2);std::vector<std::uint8_t> bytes(source,source+a2);
            auto* destination=f.locate(a0,a2,true);for(unsigned j=0;j<a2;++j)destination[j]=bytes[j];++f.copies;
        } else if(e->entry==0x800ae54cu) {++f.conversions;r->gpr[2]={0xa5000000u|(a0-0x80160000u),15};}
        else if(e->entry==0x80090698u) {++f.releases;f.released_pointer=a0;}
        return 1;
    }
};
}
bool GameSpeechInitializeCapture::dispatch(const Nba97GameTextMemory* memory,
    const Nba97GameMatchInitializeEvent* event,Nba97GameMatchInitializeRegisters* registers) {
    if(!memory || !event || !registers || !receipt.empty())return false;
    Fixture f{memory};f.put(0x80015018u,1);
    Nba97GameSpeechInitializeContext context{};context.memory=*memory;context.operation_budget=1400;
    if(nba97_game_speech_initialize_registers_from_match_initialize(event,registers,&context.registers)!=NBA97_TEXT_COMPLETE)return false;
    context.io=Fixture::child;context.user=&f;
    Nba97GameSpeechInitializeDependencies dependencies{32,Fixture::load,&f};
    Nba97GameSpeechInitializeProgress p{};Nba97GameSpeechInitializeAdapterProgress adapter{};
    const auto result=nba97_game_speech_initialize_with_recovered_dependencies(&context,&dependencies,&p,&adapter);
    *registers=p.registers;
    if(result!=NBA97_TEXT_COMPLETE || !p.completed || adapter.resource_loader_invocations!=3 ||
       f.destinations.size()!=100 || f.copies!=34 || f.conversions!=34 || f.releases!=1 || f.released_pointer!=0x80137b78u ||
       f.get(0x800fe9c8u)!=0x80137b28u || f.get(0x800febdcu)!=0x80137b34u || f.get(0x800feabcu)!=0x80160000u)
        throw std::runtime_error("speech initializer CPU fixture drifted");
    unsigned total=0;std::vector<std::uint32_t> packed;
    for(unsigned i=0;i<100;++i){const auto record=0x80102fe0u+i*12u,length=(i%4u+1u)*4u;
        if(i%3u){if(f.get(record)!=0)throw std::runtime_error("speech null record drifted");continue;}
        if(f.get(record)!=0x80160000u+total || f.get(record+4u)!=(0xa5000000u|total))throw std::runtime_error("speech packed record drifted");
        for(unsigned j=0;j<length;++j)if(f.get(0x80160000u+total+j,1)!=((i*7u+j)&255u))throw std::runtime_error("speech packed bytes drifted");
        packed.push_back(f.get(record));total+=length;
    }
    for(unsigned i=100;i<110;++i)if(f.get(0x80102fe4u+i*12u)!=0xffffffffu)throw std::runtime_error("speech sentinel drifted");
    if(total!=f.allocated_size || p.allocation_size.word!=total)throw std::runtime_error("speech allocation sum drifted");
    std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x8007FD40\",\"inclusive_end\":\"0x800800F7\",\"bytes\":952,\"instructions\":238,"
       "\"classification\":\"no direct visual effect\",\"scope\":\"natural retained match initializer with recovered retry loaders and explicit synthetic speech lookup/copy/conversion/services; no audible or gameplay claim\","
       "\"completed\":true,\"call_pc\":"<<event->pc<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"calls\":"<<p.callbacks_completed<<
       ",\"languages\":["<<p.first_language.word<<','<<p.second_language.word<<"],\"aux_pointers\":["<<f.get(0x800fe9c8u)<<','<<f.get(0x800febdcu)<<"],\"lookups\":"<<f.destinations.size()<<
       ",\"copies\":"<<f.copies<<",\"conversions\":"<<f.conversions<<",\"allocation_size\":"<<total<<",\"allocation_pointer\":"<<f.get(0x800feabcu)<<",\"released_pointer\":"<<f.released_pointer<<
       ",\"sentinels\":10,\"restored_ra\":"<<p.restored_register[0].word<<",\"frame_stack_pointer\":"<<p.frame_stack_pointer<<",\"packed_pointers\":[";
    for(std::size_t i=0;i<packed.size();++i){if(i)o<<',';o<<packed[i];}
    o<<"],\"lookup_destinations\":[";for(std::size_t i=0;i<f.destinations.size();++i){if(i)o<<',';o<<f.destinations[i];}
    o<<"],\"loaders\":[";for(unsigned i=0;i<3;++i){if(i)o<<',';const auto& l=adapter.resource_loader[i];o<<"{\"operations\":"<<l.operations<<",\"attempts\":"<<l.load_attempts<<",\"null_results\":"<<l.null_results<<'}';}
    o<<"]}";receipt=o.str();return true;
}
}
