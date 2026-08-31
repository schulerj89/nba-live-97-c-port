#include "game_court_frame_compose.hpp"
#include <cstdint>

namespace nba97 {
namespace {
std::uint32_t bits(unsigned width){return width==4?UINT32_MAX:(1u<<(width*8))-1u;}
unsigned knowledge(unsigned width){return (1u<<width)-1u;}
bool canonical(Nba97GamePeriodValue value){return value.known<=1&&(value.known||!value.word);}
bool pointerStore(std::uint32_t pc){
    // These are the five original cursor publications. Packet tag and ordering-
    // table writes are scalar words even when their low24 bits encode a link.
    return pc==0x8004acdcu||pc==0x8004ace4u||pc==0x8004ade4u||
           pc==0x8004af0cu||pc==0x8004b0ecu;
}
int courtStatus(int status,int& bridge){
    if(status==NBA97_BODY_OK)return NBA97_COURT_COMPLETE;
    if(status==NBA97_BODY_UNKNOWN)return NBA97_COURT_UNKNOWN;
    if(status==NBA97_BODY_ARGUMENT)return NBA97_COURT_ARGUMENT;
    bridge=status;return NBA97_COURT_RESOURCE;
}
}

int GameCourtFrameCompose::access(void* user,std::uint32_t pc,std::uint32_t address,
    unsigned width,int write,Nba97CourtValue* value){
    if(!user||!value||width<1||width>4)return NBA97_COURT_ARGUMENT;
    auto& owner=*static_cast<GameCourtFrameCompose*>(user);
    if(!owner.memory.access)return NBA97_COURT_ARGUMENT;
    if(value->known>1||(!value->known&&value->word)||(value->word&~bits(width)))
        return NBA97_COURT_ARGUMENT;
    Nba97PlayerFrameValue frame{};
    if(write){
        if(!value->known)return NBA97_COURT_ARGUMENT;
        frame.word=value->word;frame.known_mask=static_cast<std::uint8_t>(knowledge(width));
    }
    const unsigned kind=!write?NBA97_FRAME_READ:
        (width==4&&pointerStore(pc)?NBA97_FRAME_WRITE_POINTER:NBA97_FRAME_WRITE);
    const int status=owner.memory.access(owner.memory.user,pc,address,width,kind,&frame);
    if(status!=NBA97_BODY_OK)return courtStatus(status,owner.bridge_status_);
    if(frame.is_reference>1||frame.reference.known>1||
       (!frame.reference.known&&(frame.reference.allocation||frame.reference.offset))||
       (!frame.is_reference&&frame.reference.known)||
       (frame.is_reference&&(width!=4||
        (frame.reference.known?frame.known_mask!=knowledge(width):(frame.known_mask||frame.word))))||
       (frame.known_mask&~knowledge(width))||(frame.word&~bits(width)))
        return NBA97_COURT_ARGUMENT;
    for(unsigned i=0;i<width;++i)
        if(!(frame.known_mask&(1u<<i))&&(frame.word&(255u<<(8*i))))return NBA97_COURT_ARGUMENT;
    if(!write){
        if(frame.known_mask==knowledge(width))*value={frame.word,1};
        else *value={0,0};
    }
    return NBA97_COURT_COMPLETE;
}

int GameCourtFrameCompose::math(void* user,const Nba97CourtMathRequest* request,
    Nba97CourtValue* value){
    if(!user||!request||!value)return NBA97_COURT_ARGUMENT;
    auto& owner=*static_cast<GameCourtFrameCompose*>(user);*value={0,0};
    if(request->kind==NBA97_COURT_LEADING_BITS){
        if(!canonical(owner.leading_bits))return NBA97_COURT_ARGUMENT;
        if(!owner.leading_bits.known)return NBA97_COURT_UNKNOWN;
        if(owner.leading_bits.word>32)return NBA97_COURT_ARGUMENT;
        *value={owner.leading_bits.word,1};return NBA97_COURT_COMPLETE;
    }
    Nba97PlayerMathRequest translated{};translated.pc=request->pc;
    translated.word=request->word;translated.index=request->index;
    switch(request->kind){
    case NBA97_COURT_PROJECT_THREE:translated.kind=NBA97_PROJECTION_THREE;break;
    case NBA97_COURT_PROJECT_ONE:translated.kind=NBA97_FRAME_PROJECT_ONE;break;
    case NBA97_COURT_NORMAL_CLIP:translated.kind=NBA97_PROJECTION_CLIP;break;
    case NBA97_COURT_SCREEN:translated.kind=NBA97_PROJECTION_SCREEN;break;
    case NBA97_COURT_AVERAGE_FOUR:translated.kind=NBA97_NET_AVERAGE_FOUR;break;
    case NBA97_COURT_ORDER_DEPTH:translated.kind=NBA97_PROJECTION_DEPTH;break;
    case NBA97_COURT_LOAD_VERTEX_WORD:translated.kind=NBA97_PROJECTION_VERTEX;break;
    case NBA97_COURT_LOAD_ROTATION_WORD:translated.kind=NBA97_PROJECTION_ROTATION;break;
    case NBA97_COURT_LOAD_TRANSLATION_WORD:translated.kind=NBA97_PROJECTION_TRANSLATION;break;
    default:return NBA97_COURT_ARGUMENT;
    }
    Nba97GamePeriodValue result{};const int status=owner.geometry.apply(translated,result);
    const int mapped=courtStatus(status,owner.bridge_status_);if(mapped!=NBA97_COURT_COMPLETE)return mapped;
    // GamePlayerProjectionGeometry retains NCLIP's consumed result in MAC0;
    // its generic projection call has no scalar-return contract. 4AC68 does
    // consume MFC2 MAC0 immediately, so bridge that retained register here.
    if(request->kind==NBA97_COURT_NORMAL_CLIP)result=owner.geometry.player.root.mac0;
    if(!canonical(result))return NBA97_COURT_ARGUMENT;
    *value={result.word,result.known};return NBA97_COURT_COMPLETE;
}

int GameCourtFrameCompose::run(std::size_t budget,Nba97CourtProgress& progress){
    progress={};bridge_status_=NBA97_BODY_OK;
    if(!memory.access)return NBA97_BODY_ARGUMENT;
    Nba97CourtContext context{access,math,this,budget};
    const int status=nba97_game_court_frame(&context,&progress);
    if(status==NBA97_COURT_COMPLETE)return NBA97_BODY_OK;
    if(bridge_status_!=NBA97_BODY_OK)return bridge_status_;
    if(status==NBA97_COURT_UNKNOWN)return NBA97_BODY_UNKNOWN;
    if(status==NBA97_COURT_ALIGNMENT)return NBA97_PROJECTION_UNSUPPORTED_ALIGNMENT;
    if(status==NBA97_COURT_LIMIT)return NBA97_BODY_JOURNAL_LIMIT;
    if(status==NBA97_COURT_MATH_REQUIRED)return NBA97_FRAME_MATH_REQUIRED;
    return NBA97_BODY_ARGUMENT;
}
}
