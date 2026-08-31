#include "game_body_resources.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace nba97 {
namespace {
using Ref=Nba97GameBodyReference;
Ref addOffset(Ref ref,std::uint32_t offset){if(ref.known)ref.offset+=offset;return ref;}
bool canonical(Ref ref){return ref.known<=1 && (ref.known || (!ref.allocation&&!ref.offset));}
Nba97GamePeriodValue countWord(const Nba97GameBodyBuffer& buffer,unsigned offset,int& code) {
    if(!buffer.bytes || offset>buffer.size || 4>buffer.size-offset){code=NBA97_BODY_BOUNDS;return {};}
    if(!buffer.address_mod4_known){code=NBA97_BODY_ALIGNMENT_UNKNOWN;return {};}
    if((buffer.address_mod4+offset)&3u){code=NBA97_BODY_ALIGNMENT_TRAP;return {};}
    bool unknown=false;
    if(buffer.known)for(unsigned i=0;i<4;++i){
        if(buffer.known[offset+i]>1){code=NBA97_BODY_ARGUMENT;return {};}
        unknown|=buffer.known[offset+i]==0;
    }
    if(unknown){code=NBA97_BODY_UNKNOWN;return {};}
    std::uint32_t value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(buffer.bytes[offset+i])<<(i*8);
    code=NBA97_BODY_OK;return {value,1};
}
}
GameBodyResources& GameBodyResources::operator=(const GameBodyResources& source) {
    if(this!=&source){GameBodyResources candidate(source);*this=std::move(candidate);}
    return *this;
}
void GameBodyResources::add(unsigned id,GameBodyBytes input) {
    if(id>=Count || input.bytes.empty() || input.bytes.size()>UINT32_MAX ||
        input.originalAddressMod4 < -1 || input.originalAddressMod4>3)
        throw std::invalid_argument("body allocation extent/alignment metadata");
    // Every allocation has a knownness array so pointer sidecar stores cannot
    // leave their unused raw zero bytes exposed as known source data.
    if(input.known.empty())input.known.assign(input.bytes.size(),1);
    if(input.known.size()!=input.bytes.size())throw std::invalid_argument("body byte knownness extent");
    size_[id]=input.bytes.size();
    const auto leading=input.originalAddressMod4<0?0u:static_cast<unsigned>(input.originalAddressMod4);
    cells_[id].resize((input.bytes.size()+leading+3)/4);
    allocation_[id]=memory_.add(std::move(input.bytes),std::move(input.known),input.originalAddressMod4);
    if(!allocation_[id])throw std::invalid_argument("body allocation metadata");
}
Nba97GameBodyBuffer GameBodyResources::buffer(unsigned id) {
    if(id>=Count)throw std::out_of_range("body allocation identity");
    Nba97GameImageMemory described{};
    if(!memory_.describe(memory_.buffer(allocation_[id],0,size_[id]),described))
        throw std::invalid_argument("body allocation ownership");
    return {described.data,described.known,described.size,cells_[id].data(),cells_[id].size(),
        described.address_mod4,described.address_mod4_known};
}
Nba97GameBodyBuffer GameBodyResources::span(Ref ref,std::size_t size) {
    if(!canonical(ref)||!ref.known)throw std::invalid_argument("unknown or malformed body reference");
    auto view=buffer(ref.allocation);
    if(ref.offset>view.size || size>view.size-ref.offset)throw std::out_of_range("body reference span");
    return view;
}
Ref GameBodyResources::referenceAt(Ref slot) {
    auto view=span(slot,4);
    if(!view.address_mod4_known || ((view.address_mod4+slot.offset)&3u))
        throw std::invalid_argument("body reference alignment");
    const auto index=(std::uint64_t(slot.offset)+view.address_mod4)/4;
    if(index>=view.cell_count)throw std::out_of_range("body reference cell");
    const auto& cell=view.cells[index];
    if(cell.is_reference!=1 || !canonical(cell.reference))
        throw std::invalid_argument("serialized body word is not a native reference");
    for(unsigned i=0;i<4;++i)if(view.known[slot.offset+i]>1)
        throw std::invalid_argument("body reference metadata");
    return cell.reference;
}
Nba97GameRenderBuffer GameBodyResources::knownBuffer(Ref ref,std::size_t size) {
    auto view=span(ref,size);
    if(!view.address_mod4_known)throw std::invalid_argument("body byte-view alignment provenance");
    for(std::size_t i=0;i<size;++i){
        if(view.known[ref.offset+i]!=1)throw std::invalid_argument("body byte view is not fully known");
        const auto cell=(std::uint64_t(ref.offset)+i+view.address_mod4)/4;
        if(cell>=view.cell_count || view.cells[cell].is_reference ||
            !canonical(view.cells[cell].reference) || view.cells[cell].reference.known)
            throw std::invalid_argument("body byte view overlaps native reference metadata");
    }
    return {view.bytes+ref.offset,size};
}
Ref GameBodyResources::context(unsigned player) {
    if(player>=10)throw std::out_of_range("physical body player");
    return {Contexts,player*ContextStride,1};
}
Ref GameBodyResources::partHeader(unsigned player,unsigned part) {
    if(part>=20)throw std::out_of_range("body part");
    return referenceAt(addOffset(context(player),part*0x94u+0xb0u));
}
Ref GameBodyResources::partPivot(unsigned player,unsigned part) {
    if(part>=20)throw std::out_of_range("body part");
    return referenceAt(addOffset(context(player),part*0x94u+0xacu));
}
Ref GameBodyResources::descriptor(unsigned player){return referenceAt(addOffset(context(player),0xbc4));}
Ref GameBodyResources::alternateHeaders(unsigned player){return referenceAt(addOffset(context(player),0xbc8));}

GameBodyResourceResult prepareGameBodyResources(GameBodyBytes contexts,GameBodyBytes home,
    GameBodyBytes away,std::size_t journalCapacity) {
    GameBodyResourceResult result;
    try {
        auto candidate=std::unique_ptr<GameBodyResources>(new GameBodyResources);
        candidate->add(GameBodyResources::Contexts,std::move(contexts));
        candidate->add(GameBodyResources::Home,std::move(home));
        candidate->add(GameBodyResources::Away,std::move(away));
        for(unsigned id=GameBodyResources::RootsA;id<=GameBodyResources::RootsB;++id)
            candidate->add(id,{std::vector<std::uint8_t>(320),std::vector<std::uint8_t>(320),0});
        std::array<Nba97GameBodyBuffer,GameBodyResources::Count> buffers{};
        for(unsigned i=0;i<buffers.size();++i)buffers[i]=candidate->buffer(i);
        result.journal.resize(journalCapacity);std::size_t used=0;
        for(unsigned side=0;side<2;++side){
            const auto body=side?GameBodyResources::Away:GameBodyResources::Home;
            int read=NBA97_BODY_OK;
            const auto countA=countWord(buffers[body],0,read);
            if(read!=NBA97_BODY_OK){result.result=read;result.journal.resize(used);result.detail="body countA source input unavailable";return result;}
            const auto countB=countWord(buffers[body],4,read);
            if(read!=NBA97_BODY_OK){result.result=read;result.journal.resize(used);result.detail="body countB source input unavailable";return result;}
            Nba97GameBodyGeometryInput input{buffers.data(),buffers.size(),GameBodyResources::context(side*5),
                {body,8,1},{GameBodyResources::RootsA,0,1},{GameBodyResources::RootsB,0,1},countA,countB,{side*5,1}};
            result.result=nba97_game_body_geometry(&input,result.journal.empty()?nullptr:result.journal.data()+used,
                result.journal.size()-used,&result.side[side]);
            used+=result.side[side].writes;
            if(result.result!=NBA97_BODY_OK){result.journal.resize(used);result.detail="50768 stopped at a required owned source input";return result;}
            ++result.sidesCompleted;
        }
        result.journal.resize(used);result.resource=std::move(candidate);
    }catch(const std::exception& error){result.result=NBA97_BODY_ARGUMENT;result.detail=error.what();}
    return result;
}
}
