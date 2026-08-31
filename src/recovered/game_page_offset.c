#include "game_page_offset.h"

#define NBA97_PAGE_MODE_PC UINT32_C(0x800993e0)
#define NBA97_PAGE_MODE_ADDRESS UINT32_C(0x800c55c0)

static int mode_read(Nba97GamePageOffsetContext *context,
    Nba97GamePageOffsetProgress *progress,uint8_t *mode)
{
    Nba97GamePageOffsetReadEvent event;
    Nba97GamePageOffsetByte value;
    int status;
    event.pc=NBA97_PAGE_MODE_PC;
    event.address=NBA97_PAGE_MODE_ADDRESS;
    event.width=1;
    progress->stopped_pc=event.pc;
    progress->stopped_address=event.address;
    if(!context->read)return NBA97_GAME_PAGE_OFFSET_READ_REQUIRED;
    value.value=0;
    value.known=0;
    status=context->read(context->user,&event,&value);
    if(status!=NBA97_GAME_PAGE_OFFSET_OK)return status;
    ++progress->reads;
    if(value.known>1)return NBA97_GAME_PAGE_OFFSET_ARGUMENT;
    if(!value.known)return NBA97_GAME_PAGE_OFFSET_UNKNOWN;
    *mode=value.value;
    return NBA97_GAME_PAGE_OFFSET_OK;
}

int nba97_game_page_offset(Nba97GamePageOffsetContext *context,
    uint32_t texture_mode,uint32_t abr,uint32_t x,uint32_t y,
    uint32_t *result,Nba97GamePageOffsetProgress *progress)
{
    uint32_t page;
    uint8_t graphics;
    int alternate;
    int status;
    if(!progress)return NBA97_GAME_PAGE_OFFSET_ARGUMENT;
    progress->reads=0;
    progress->stopped_pc=0;
    progress->stopped_address=0;
    progress->completed=0;
    if(!context||!result)return NBA97_GAME_PAGE_OFFSET_ARGUMENT;

    status=mode_read(context,progress,&graphics);
    if(status!=NBA97_GAME_PAGE_OFFSET_OK)return status;
    alternate=graphics==1;
    /* Original quirk: a first value of 2 does not select this route. 9BFD0
     * performs a fresh 993DC call and only that second value is compared to 2. */
    if(!alternate){
        status=mode_read(context,progress,&graphics);
        if(status!=NBA97_GAME_PAGE_OFFSET_OK)return status;
        alternate=graphics==2;
    }

    if(alternate){
        page=((texture_mode&3u)<<9)|((abr&3u)<<7)|
            ((y&0x300u)>>3)|((x&0x3ffu)>>6);
    }else{
        page=((texture_mode&3u)<<7)|((abr&3u)<<5)|
            ((y&0x100u)>>4)|((x&0x3ffu)>>6)|((y&0x200u)<<2);
    }
    *result=page;
    progress->stopped_pc=0;
    progress->stopped_address=0;
    progress->completed=1;
    return NBA97_GAME_PAGE_OFFSET_OK;
}
