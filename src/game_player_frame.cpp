#include "game_player_frame.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
namespace nba97 {
namespace {
using Ref=Nba97GameBodyReference;
bool valid(Ref r){return r.known<=1&&(r.known||(!r.allocation&&!r.offset));}
uint32_t mask(unsigned n){return n==4?UINT32_MAX:(1u<<(8*n))-1;}
}
int GamePlayerFrame::validateAddresses() const {
    if(!buffers||!addresses||address_count!=buffer_count||buffer_count>UINT32_MAX)return NBA97_BODY_ARGUMENT;
    // Mapping metadata is a prerequisite of this original-address interface;
    // memory bytes/cells/knownness are validated only when actually reached.
    for(std::size_t i=0;i<buffer_count;++i){const auto a=addresses[i];
        if(a.known>1||(!a.known&&a.word))return NBA97_BODY_ARGUMENT;
        if(!a.known)continue;
        if(buffers[i].size>UINT32_MAX||uint64_t(a.word)+buffers[i].size>0x100000000ull)return NBA97_BODY_ARGUMENT;
        // Private ABI stack/code aliases cannot be represented by these owners.
        if(a.word<0x1f800400u&&uint64_t(a.word)+buffers[i].size>0x1f800040u)return NBA97_BODY_ARGUMENT;
        for(std::size_t j=0;j<i;++j)if(addresses[j].known&&uint64_t(a.word)<uint64_t(addresses[j].word)+buffers[j].size&&uint64_t(addresses[j].word)<uint64_t(a.word)+buffers[i].size)return NBA97_BODY_ARGUMENT;
    }
    return NBA97_BODY_OK;
}
int GamePlayerFrame::locate(uint32_t a,unsigned width,Ref& out) const {
    out={};for(std::size_t i=0;i<buffer_count;++i)if(addresses[i].known&&a>=addresses[i].word){
        const uint64_t off=uint64_t(a)-addresses[i].word;
        if(off<buffers[i].size&&width<=buffers[i].size-off){out={static_cast<uint32_t>(i),static_cast<uint32_t>(off),1};return NBA97_BODY_OK;}}
    // Preserve the final52914 one-past context reference without allowing a
    // dereference there. Prefer an adjacent allocation's actual start above.
    if(width==0)for(std::size_t i=0;i<buffer_count;++i)if(addresses[i].known&&uint64_t(addresses[i].word)+buffers[i].size==a){
        out={static_cast<uint32_t>(i),static_cast<uint32_t>(buffers[i].size),1};return NBA97_BODY_OK;}
    return NBA97_BODY_BOUNDS;
}
Ref GamePlayerFrame::slot(uint32_t a) const {Ref r{};if(locate(a,0,r)!=NBA97_BODY_OK)r={UINT32_MAX,0,1};return r;}
int GamePlayerFrame::encoded(Ref r,uint32_t& word) const {
    if(!valid(r))return NBA97_BODY_ARGUMENT;
    if(!r.known)return NBA97_BODY_UNKNOWN;
    if(r.allocation>=buffer_count)return NBA97_BODY_BOUNDS;
    if(!addresses[r.allocation].known)return NBA97_BODY_ADDRESS_REQUIRED;
    word=addresses[r.allocation].word+r.offset;return NBA97_BODY_OK;
}
int GamePlayerFrame::access(uint32_t pc,uint32_t address,unsigned width,unsigned kind,Nba97PlayerFrameValue& value){
    (void)pc;Ref ref;int status=locate(address,width,ref);if(status!=NBA97_BODY_OK)return status;
    auto& b=buffers[ref.allocation];if(!b.bytes||!b.cells)return NBA97_BODY_BOUNDS;
    if(b.address_mod4_known>1||b.address_mod4>3)return NBA97_BODY_ARGUMENT;
    if(!b.address_mod4_known)return NBA97_BODY_ALIGNMENT_UNKNOWN;
    if((addresses[ref.allocation].word&3)!=b.address_mod4)return NBA97_BODY_ARGUMENT;
    const std::size_t cell_index=(uint64_t(ref.offset)+b.address_mod4)/4;
    if(cell_index>=b.cell_count)return NBA97_BODY_BOUNDS;
    auto& c=b.cells[cell_index];
    if(c.is_reference>1||!valid(c.reference)||(!c.is_reference&&c.reference.known))return NBA97_BODY_ARGUMENT;
    for(unsigned i=0;i<width;++i)if(b.known&&b.known[ref.offset+i]>1)return NBA97_BODY_ARGUMENT;
    if(kind==NBA97_FRAME_READ){
        value={};
        if(c.is_reference){
            if(width==4){value.is_reference=1;value.reference=c.reference;
                if(!c.reference.known)return NBA97_BODY_OK;}
            uint32_t word=0;status=encoded(c.reference,word);if(status!=NBA97_BODY_OK)return status;
            value.word=(word>>((address&3)*8))&mask(width);value.known_mask=static_cast<uint8_t>((1u<<width)-1);}
        else for(unsigned i=0;i<width;++i)if(!b.known||b.known[ref.offset+i]){value.word|=uint32_t(b.bytes[ref.offset+i])<<(i*8);value.known_mask|=static_cast<uint8_t>(1u<<i);}
        return NBA97_BODY_OK;
    }
    if(kind!=NBA97_FRAME_WRITE&&kind!=NBA97_FRAME_WRITE_POINTER)return NBA97_BODY_ARGUMENT;
    if(c.is_reference&&width!=4)return NBA97_BODY_ADDRESS_REQUIRED;
    if(!b.known&&!value.is_reference&&value.known_mask!=((1u<<width)-1))return NBA97_BODY_UNKNOWN;
    c={};
    for(unsigned i=0;i<width;++i){const bool known=(value.known_mask&(1u<<i))!=0;
        if(known)b.bytes[ref.offset+i]=static_cast<uint8_t>(value.word>>(8*i));
        if(b.known)b.known[ref.offset+i]=static_cast<uint8_t>(known);}
    if(value.is_reference){c.is_reference=1;c.reference=value.reference;}
    else if(kind==NBA97_FRAME_WRITE_POINTER){if(width!=4||value.known_mask!=15)return NBA97_BODY_ARGUMENT;
        Ref target; if(locate(value.word,0,target)==NBA97_BODY_OK){c.is_reference=1;c.reference=target;}}
    return NBA97_BODY_OK;
}
int GamePlayerFrame::math(const Nba97PlayerMathRequest& q,Nba97GamePeriodValue& out){
    if(q.kind==NBA97_FRAME_PROJECT_ONE){auto p=q;p.kind=NBA97_ROOT_PROJECT;return geometry.root.apply(p,out);}
    if(q.kind>=NBA97_FRAME_IR0){auto p=q;
        if(q.kind==NBA97_FRAME_IR0)p.kind=NBA97_ROOT_IR0;
        else if(q.kind==NBA97_FRAME_FLAGS)p.kind=NBA97_ROOT_FLAGS;
        else if(q.kind==NBA97_FRAME_DEPTH)p.kind=NBA97_ROOT_DEPTH;
        else return NBA97_BODY_ARGUMENT;
        return geometry.root.apply(p,out);}
    return geometry.apply(q,out);
}
int GamePlayerFrame::child(uint32_t entry){
    last_child=entry;last_child_writes.clear();
    try{last_child_writes.resize(child_store_budget);}catch(const std::bad_alloc&){return NBA97_BODY_JOURNAL_LIMIT;}catch(const std::length_error&){return NBA97_BODY_JOURNAL_LIMIT;}
    int status=NBA97_BODY_ARGUMENT;std::size_t written=0;
    if(entry==0x8005200c){Nba97GamePlayerRootInput in{};in.buffers=buffers;in.buffer_count=buffer_count;
        in.context_f0ed4=slot(0x800f0ed4);in.index_1029b0=slot(0x801029b0);in.scales_105f48=slot(0x80105f48);in.camera_f9fd8=slot(0x800f9fd8);
        in.preset_26384=slot(0x80026384);in.trig_b3254=slot(0x800b3254);in.world_fb480=slot(0x800fb480);in.primary_103fd8=slot(0x80103fd8);
        in.alternate_10b2b8=slot(0x8010b2b8);in.ground_102f8c=slot(0x80102f8c);in.screen_fea94=slot(0x800fea94);in.depth_106038=slot(0x80106038);
        in.math=GamePlayerRootGeometry::callback;in.math_user=&geometry.root;
        status=nba97_game_player_root(&in,last_child_writes.data(),last_child_writes.size(),&root_progress);written=root_progress.writes;
    }else if(entry==0x80055368){Nba97GamePlayerGeometryInput in{};in.buffers=buffers;in.buffer_count=buffer_count;
        in.context_f0ed4=slot(0x800f0ed4);in.root_10292c=slot(0x8010292c);in.work_f1c4c=slot(0x800f1c4c);in.work_f9cf8=slot(0x800f9cf8);in.work_f9c54=slot(0x800f9c54);in.work_f9d00=slot(0x800f9d00);
        in.foot_f9d04=slot(0x800f9d04);in.foot_fea38=slot(0x800fea38);in.hand_f0fb4=slot(0x800f0fb4);in.hand_fc62c=slot(0x800fc62c);in.angle_103edc=slot(0x80103edc);in.trig_b3254=slot(0x800b3254);
        in.math=GamePlayerGeometry::callback;in.math_user=&geometry.root.vector;
        status=nba97_game_player_geometry(&in,last_child_writes.data(),last_child_writes.size(),&part_progress);written=part_progress.writes;
    }else if(entry==0x800525ac){Nba97GamePlayerProjectionInput in{};in.buffers=buffers;in.buffer_count=buffer_count;in.addresses=addresses;in.address_count=address_count;
        in.context_f0ed4=slot(0x800f0ed4);in.bank_1ede8=slot(0x8001ede8);in.ordering_102924=slot(0x80102924);in.mask_1f80000c=slot(0x1f80000c);in.index_1029b0=slot(0x801029b0);in.suppress_dcf10=slot(0x800dcf10);
        in.math=GamePlayerProjectionGeometry::callback;in.math_user=&geometry;
        status=nba97_game_player_projection(&in,last_child_writes.data(),last_child_writes.size(),&projection_progress);written=projection_progress.writes;
    }
    last_child_writes.resize(written);return status;
}
int GamePlayerFrame::accessCallback(void* u,uint32_t pc,uint32_t a,unsigned w,unsigned kind,Nba97PlayerFrameValue* v){if(!u||!v)return NBA97_BODY_ARGUMENT;return static_cast<GamePlayerFrame*>(u)->access(pc,a,w,kind,*v);}
int GamePlayerFrame::childCallback(void* u,uint32_t,uint32_t entry){return u?static_cast<GamePlayerFrame*>(u)->child(entry):NBA97_BODY_ARGUMENT;}
int GamePlayerFrame::mathCallback(void* u,const Nba97PlayerMathRequest* q,Nba97GamePeriodValue* v){if(!u||!q||!v)return NBA97_BODY_ARGUMENT;return static_cast<GamePlayerFrame*>(u)->math(*q,*v);}
Nba97PlayerFrameContext GamePlayerFrame::context(std::size_t budget){return {accessCallback,mathCallback,childCallback,this,budget};}
int GamePlayerFrame::bindContext(std::size_t budget,Nba97PlayerFrameContext& out){out={};int status=validateAddresses();if(status==NBA97_BODY_OK)out=context(budget);return status;}
void GamePlayerFrame::resetProgress(){last_child=0;last_child_writes.clear();part_progress={};root_progress={};projection_progress={};}
int GamePlayerFrame::run(std::size_t b,Nba97PlayerFrameProgress& p){p={};resetProgress();int s=validateAddresses();if(s!=NBA97_BODY_OK)return s;auto c=context(b);return nba97_game_player_frame(&c,&p);}
int GamePlayerFrame::shadow(std::size_t b,Nba97PlayerFrameProgress& p){p={};resetProgress();int s=validateAddresses();if(s!=NBA97_BODY_OK)return s;auto c=context(b);return nba97_game_player_shadow(&c,&p);}
int GamePlayerFrame::indicator(std::size_t b,Nba97PlayerFrameProgress& p){p={};resetProgress();int s=validateAddresses();if(s!=NBA97_BODY_OK)return s;auto c=context(b);return nba97_game_player_indicator(&c,&p);}
int GamePlayerFrame::copy40(uint32_t a,uint32_t d,std::size_t b,Nba97PlayerFrameProgress& p){p={};resetProgress();int s=validateAddresses();if(s!=NBA97_BODY_OK)return s;auto c=context(b);return nba97_game_player_copy40(&c,a,d,&p);}
}
