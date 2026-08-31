#include "game_court_textures.h"
#include <string.h>

typedef struct Run {Nba97GameCourtTextureProgress* out;} Run;
#define TRY(x) do{int result_=(x);if(result_!=NBA97_IMAGE_COMPLETE)return result_;}while(0)
static int32_t signed32(uint32_t v){return v<=0x7fffffffu?(int32_t)v:-1-(int32_t)~v;}
static int32_t signed16(uint32_t v){v&=65535;return v<32768?(int32_t)v:(int32_t)v-65536;}
static int64_t signed24(uint32_t v){v>>=8;return v<0x800000?(int64_t)v:(int64_t)v-0x1000000;}
static int plus(Nba97GameImageReference base,int64_t delta,Nba97GameImageReference* out){
    if((delta>0&&base.offset>INT64_MAX-delta)||(delta<0&&base.offset<INT64_MIN-delta))return NBA97_IMAGE_RESOURCE;
    *out=base;out->offset+=delta;return NBA97_IMAGE_COMPLETE;
}
static int access(Run* r,Nba97GameImageReference ref,uint32_t pc,unsigned width,int write,uint32_t* word){
    Nba97GameImageMemory* m=ref.memory;size_t at;unsigned i;int unknown=0;
    r->out->stopped_pc=pc;r->out->stopped_offset=ref.offset;
    if(!m||!m->data||ref.offset<0||(uint64_t)ref.offset>SIZE_MAX||(size_t)ref.offset>m->size||width>m->size-(size_t)ref.offset)return NBA97_IMAGE_RESOURCE;
    if(m->address_mod4_known>1||m->address_mod4>3)return NBA97_IMAGE_ARGUMENT;
    at=(size_t)ref.offset;
    if(m->known)for(i=0;i<width;++i){if(m->known[at+i]>1)return NBA97_IMAGE_ARGUMENT;if(!m->known[at+i])unknown=1;}
    if(width>1){if(!m->address_mod4_known)return NBA97_IMAGE_UNKNOWN;
        if(((uint64_t)ref.offset+m->address_mod4)&(width-1u))return NBA97_IMAGE_ALIGNMENT_TRAP;}
    if(write){for(i=0;i<width;++i){m->data[at+i]=(uint8_t)(*word>>(i*8));if(m->known)m->known[at+i]=1;}}
    else{if(unknown)return NBA97_IMAGE_UNKNOWN;*word=0;for(i=0;i<width;++i)*word|=(uint32_t)m->data[at+i]<<(i*8);}
    return NBA97_IMAGE_COMPLETE;
}
static int read(Run* r,Nba97GameImageReference base,int64_t delta,uint32_t pc,unsigned width,uint32_t* word){
    Nba97GameImageReference ref;r->out->stopped_pc=pc;
    TRY(plus(base,delta,&ref));return access(r,ref,pc,width,0,word);
}
int nba97_game_court_textures(Nba97GameImageReference container,
    Nba97GameCourtTextureState* state,size_t image_budget,size_t header_budget,
    Nba97GameImageTransferIo io,void* user,Nba97GameCourtTextureProgress* out){
    Run run;uint32_t index=0,cursor_x=512,cursor_y=252,other_y=240,count,offset,format,width,x,y;
    Nba97GameImageReference image,palette,field;Nba97GameImagePlacement placement;int status;
    if(!state||!out||!io)return NBA97_IMAGE_ARGUMENT;
    memset(out,0,sizeof *out);run.out=out;
    for(;;){
        out->index=index;
        TRY(read(&run,container,8,0x800a3fe0,4,&count));
        if(signed32(index)>=signed32(count)){out->completed=1;out->stopped_pc=0x80048894;return NBA97_IMAGE_COMPLETE;}
        if(out->images_completed>=image_budget){out->stopped_pc=0x800487e0;return NBA97_COURT_TEXTURE_IMAGE_LIMIT;}
        TRY(read(&run,container,8,0x800a3fec,4,&count));
        if(index>=count){image.memory=0;image.offset=0;}
        else{TRY(read(&run,container,(uint32_t)(index<<3)+20u,0x800a4000,4,&offset));
            TRY(plus(container,offset,&image));}
        TRY(read(&run,image,0,0x800487ec,1,&format));
        if(format&3){
            TRY(read(&run,image,12,0x80048800,2,&x));TRY(read(&run,image,14,0x80048804,1,&y));
            placement.x=(int32_t)(x&0x3fff);placement.y=(int32_t)y;placement.clut_x=512;placement.clut_y=signed32(other_y);
            ++other_y;out->stopped_pc=0x80048814;
        }else{
            TRY(read(&run,image,0,0x80048824,4,&offset));TRY(plus(image,signed24(offset),&palette));
            TRY(read(&run,palette,4,0x80048834,2,&width));
            out->stopped_pc=0x8004883c;
            if(state->palette_known>1||(!state->palette_known&&(state->palette_fed1c.memory||state->palette_fed1c.offset)))return NBA97_IMAGE_ARGUMENT;
            state->palette_fed1c=palette;state->palette_known=1;++out->palette_stores;
            if(signed16(width)>16){uint32_t capped=16;TRY(plus(palette,4,&field));TRY(access(&run,field,0x8004884c,2,1,&capped));++out->palette_width_stores;}
            TRY(read(&run,image,12,0x80048850,2,&x));TRY(read(&run,image,14,0x80048854,1,&y));
            placement.x=(int32_t)(x&0x3fff);placement.y=(int32_t)y;placement.clut_x=signed32(cursor_x);placement.clut_y=signed32(cursor_y);
            cursor_x+=16;out->stopped_pc=0x80048868;
        }
        memset(&out->image,0,sizeof out->image);
        status=nba97_game_image_upload(&state->upload,image,placement,header_budget,io,user,&out->image);
        if(status!=NBA97_IMAGE_COMPLETE){out->stopped_offset=out->image.stopped_offset;return status;}
        ++out->images_completed;
        if(!(format&3)){cursor_y-=(cursor_x-512)>>8;if(signed32(cursor_x)>=768)cursor_x=512;}
        ++index;
    }
}
