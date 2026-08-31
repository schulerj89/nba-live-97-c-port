#include "voice_programs.h"
#include <string.h>
static int32_t s32(uint32_t u){return u<=0x7fffffffu?(int32_t)u:-1-(int32_t)~u;}
static Nba97VoiceApiResult result(int completion,int32_t value){Nba97VoiceApiResult r;r.completion=completion;r.value=value;return r;}
static int valid(const Nba97VoicePrograms* s){return s&&s->shared&&s->banks&&s->call;}
static int memory(Nba97VoicePrograms* s,enum Nba97VoiceProgramCall op,uint32_t address,uint32_t value,uint32_t* out){
    Nba97VoiceProgramRequest request;memset(&request,0,sizeof request);
    request.argument[0]=address;request.argument[1]=value;
    return s->call(s->context,op,&request,out)==1;
}
Nba97VoiceApiResult nba97_voice_bank_validate(Nba97VoicePrograms* s,uint32_t bank){
    if(!valid(s))return result(NBA97_PROGRAM_ARGUMENT,0);
    return result(NBA97_PROGRAM_COMPLETE,bank<10&&s->banks[bank]?0:-8);
}
Nba97VoiceApiResult nba97_voice_program_play(Nba97VoicePrograms* s,uint32_t bank,uint32_t program,uint32_t volume){
    uint32_t header,version,count=128,pointer,tag,value;enum Nba97VoiceProgramCall op;
    Nba97VoiceProgramRequest request;
    if(!valid(s))return result(NBA97_PROGRAM_ARGUMENT,0);
    if(!s->shared->enabled)return result(NBA97_PROGRAM_COMPLETE,-10);
    if(nba97_voice_bank_validate(s,bank).value<0)return result(NBA97_PROGRAM_COMPLETE,-8);
    header=s->banks[bank];
    if(!memory(s,NBA97_PROGRAM_READ8,header+4,0,&version))return result(NBA97_PROGRAM_IO_REFUSED,0);
    if((version&255u)&&!memory(s,NBA97_PROGRAM_READ16,header+6,0,&count))return result(NBA97_PROGRAM_IO_REFUSED,0);
    count&=65535u;
    if(s32(program)<0||program>=count)return result(NBA97_PROGRAM_COMPLETE,-8);
    if(!memory(s,NBA97_PROGRAM_READ32,header+8+program*4,0,&pointer))return result(NBA97_PROGRAM_IO_REFUSED,0);
    /* 92B74 null and tag rejections are source results, distinct from a
     * native refusal to read an unowned but nonzero memory token. */
    if(!pointer)return result(NBA97_PROGRAM_COMPLETE,-8);
    if(!memory(s,NBA97_PROGRAM_READ32,pointer,0,&tag))return result(NBA97_PROGRAM_IO_REFUSED,0);
    if(tag==0x6c544150u)op=NBA97_PROGRAM_PLAY_PATL_9267C;
    else{
        if(!memory(s,NBA97_PROGRAM_READ16,pointer,0,&tag))return result(NBA97_PROGRAM_IO_REFUSED,0);
        if((tag&65535u)!=0x5450u)return result(NBA97_PROGRAM_COMPLETE,-7);
        op=NBA97_PROGRAM_PLAY_PT_91CD8;
    }
    memset(&request,0,sizeof request);request.argument[0]=pointer;request.argument[1]=bank;
    request.argument[2]=request.argument[3]=request.argument[4]=request.argument[5]=request.argument[7]=0xffffffffu;
    request.argument[6]=volume;
    if(s->call(s->context,op,&request,&value)!=1)return result(NBA97_PROGRAM_IO_REFUSED,0);
    return result(NBA97_PROGRAM_COMPLETE,s32(value));
}
Nba97VoiceApiResult nba97_voice_program_register(Nba97VoicePrograms* s,uint32_t bank,uint32_t* output_program,uint32_t program,uint32_t body){
    uint32_t header,version,count=128,i,pointer,tag,value;
    enum Nba97VoiceProgramCall op;Nba97VoiceProgramRequest request;
    if(!valid(s)||!output_program)return result(NBA97_PROGRAM_ARGUMENT,0);
    if(nba97_voice_bank_validate(s,bank).value!=0||!program||!body)return result(NBA97_PROGRAM_COMPLETE,-8);
    header=s->banks[bank];
    if(!memory(s,NBA97_PROGRAM_READ8,header+4,0,&version))return result(NBA97_PROGRAM_IO_REFUSED,0);
    if((version&255u)&&!memory(s,NBA97_PROGRAM_READ16,header+6,0,&count))return result(NBA97_PROGRAM_IO_REFUSED,0);
    count&=65535u;
    for(i=0;i<count;++i){
        if(!memory(s,NBA97_PROGRAM_READ32,header+8+i*4,0,&pointer))return result(NBA97_PROGRAM_IO_REFUSED,0);
        if(pointer)continue;
        /* Complete92628 tag dispatch; memory/upload ownership is required. */
        if(!memory(s,NBA97_PROGRAM_READ32,program,0,&tag))return result(NBA97_PROGRAM_IO_REFUSED,0);
        if(tag==0x6c544150u)op=NBA97_PROGRAM_UPLOAD_PATL_924B4;
        else{
            if(!memory(s,NBA97_PROGRAM_READ16,program,0,&tag))return result(NBA97_PROGRAM_IO_REFUSED,0);
            if((tag&65535u)!=0x5450u){*output_program=0xffffffffu;return result(NBA97_PROGRAM_COMPLETE,-1);}
            op=NBA97_PROGRAM_UPLOAD_PT_921F4;
        }
        memset(&request,0,sizeof request);request.argument[0]=program;request.argument[1]=body;request.auxiliary=0xffffffffu;
        if(s->call(s->context,op,&request,&value)!=1)return result(NBA97_PROGRAM_IO_REFUSED,0);
        if(s32(value)<0){
            *output_program=0xffffffffu; /* Original ignores upload's output word. */
            return result(NBA97_PROGRAM_COMPLETE,s32(value));
        }
        /* Retain the bank address/index selected BEFORE upload; do not reread
         * a changed bank pointer or search a new vacancy after the callback. */
        if(!memory(s,NBA97_PROGRAM_WRITE32,header+8+i*4,program,&value))return result(NBA97_PROGRAM_IO_REFUSED,0);
        *output_program=i;
        return result(NBA97_PROGRAM_COMPLETE,8); /* Original constant, not upload return. */
    }
    return result(NBA97_PROGRAM_COMPLETE,-9);
}
