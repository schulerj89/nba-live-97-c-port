#include "game_camera.h"
#include <string.h>
typedef struct Run {Nba97GameCameraContext* c;Nba97GameCameraEvent* journal;size_t capacity;Nba97GameCameraProgress* p;} Run;
typedef struct Bits {uint32_t word;unsigned known;} Bits;
#define TRY(x) do{int rc_=(x);if(rc_!=NBA97_TEXT_COMPLETE)return rc_;}while(0)
static int32_t signed16(uint32_t x){x&=65535;return x<32768?(int32_t)x:(int32_t)x-65536;}
static int64_t signed32(uint32_t x){return x<0x80000000u?(int64_t)x:(int64_t)x-INT64_C(0x100000000);}
static uint32_t sar(uint32_t v,unsigned n){return(v>>n)|((v&0x80000000u)?(~0u<<(32-n)):0);}
static void stop(Run* r,uint32_t pc,uint32_t a){r->p->stopped_pc=pc;r->p->stopped_address=a;}
static int access(Run* r,uint32_t pc,uint32_t a,unsigned n,unsigned alignment,uint8_t** data,uint8_t** known){
    size_t i;stop(r,pc,a);
    if(r->p->accesses>=r->c->access_budget)return NBA97_TEXT_LIMIT;
    ++r->p->accesses;
    if(a&(alignment-1u))return NBA97_TEXT_ALIGNMENT_TRAP;
    for(i=0;i<r->c->memory.count;++i){Nba97GameTextRegion* b=&r->c->memory.region[i];uint64_t o=(uint64_t)a-b->base;unsigned k;
        if(a<b->base||o>b->size||n>b->size-o)continue;
        *data=b->data+(size_t)o;*known=b->known?b->known+(size_t)o:0;
        if(*known)for(k=0;k<n;++k)if((*known)[k]>1)return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;
    }
    return NBA97_TEXT_RESOURCE;
}
static int bits(Run* r,uint32_t pc,uint32_t a,unsigned n,unsigned alignment,Bits* out){
    uint8_t *d,*k;unsigned i;TRY(access(r,pc,a,n,alignment,&d,&k));out->word=0;out->known=0;
    for(i=0;i<n;++i){out->word|=(uint32_t)d[i]<<(8*i);if(!k||k[i])out->known|=1u<<i;}
    return NBA97_TEXT_COMPLETE;
}
static int read_mask(Run* r,uint32_t pc,uint32_t a,unsigned n,unsigned mask,uint32_t* out){
    Bits b;TRY(bits(r,pc,a,n,n,&b));if((b.known&mask)!=mask)return NBA97_TEXT_UNKNOWN;*out=b.word;return NBA97_TEXT_COMPLETE;
}
#define READ(a,pc,v) TRY(read_mask(r,pc,a,4,15,&v))
#define HALF(a,pc,v) TRY(read_mask(r,pc,a,2,3,&v))
#define BYTE(a,pc,v) TRY(read_mask(r,pc,a,1,1,&v))
static int store_bits(Run* r,uint32_t pc,uint32_t a,unsigned n,unsigned alignment,Bits b){
    uint8_t *d,*k;unsigned i;Nba97GameCameraEvent* e;stop(r,pc,a);
    if(r->p->events>=r->capacity)return NBA97_TEXT_LIMIT;
    TRY(access(r,pc,a,n,alignment,&d,&k));
    if(!k&&(b.known&((1u<<n)-1u))!=((1u<<n)-1u))return NBA97_TEXT_ARGUMENT;
    e=&r->journal[r->p->events++];memset(e,0,sizeof *e);e->pc=pc;e->address=a;e->width=(uint8_t)n;e->known_mask=(uint8_t)(b.known&((1u<<n)-1u));
    e->value=n==4?b.word:b.word&((1u<<(n*8))-1u);e->completed=1;
    for(i=0;i<n;++i){d[i]=(uint8_t)(b.word>>(8*i));if(k)k[i]=(uint8_t)((b.known>>i)&1u);}
    ++r->p->stores;return NBA97_TEXT_COMPLETE;
}
static int write(Run* r,uint32_t pc,uint32_t a,unsigned n,uint32_t v){Bits b;b.word=v;b.known=15;return store_bits(r,pc,a,n,n,b);}
#define WRITE(a,pc,n,v) TRY(write(r,pc,a,n,v))
static int io(Run* r,uint32_t pc,uint32_t target,uint32_t a0,uint32_t a1,uint32_t a2,const uint16_t* rect,uint32_t* result){
    Nba97GameCameraEvent* e;Nba97GamePeriodValue value={0,0};stop(r,pc,0);
    if(r->p->events>=r->capacity)return NBA97_TEXT_LIMIT;
    e=&r->journal[r->p->events++];memset(e,0,sizeof *e);e->pc=pc;e->kind=1;e->target=target;e->argument[0]=a0;e->argument[1]=a1;e->argument[2]=a2;
    if(rect)memcpy(e->rectangle,rect,sizeof e->rectangle);
    if(!r->c->io||r->c->io(r->c->user,&r->c->memory,e,&value)!=1)return NBA97_TEXT_IO_REFUSED;
    if(value.known>1||(!value.known&&value.word))return NBA97_TEXT_ARGUMENT;
    e->completed=1;++r->p->callbacks;
    if(result){if(!value.known)return NBA97_TEXT_UNKNOWN;*result=value.word;}
    return NBA97_TEXT_COMPLETE;
}
static int input(Run* r,uint32_t index,uint32_t* value){
    uint32_t flag,tick,old,aggregate=0,i,mode,pad,v;
    READ(0x800c4a70,0x8008f228,flag);
    if(flag)TRY(io(r,0x8008f24c,0x8008f1d4,2,0,0,0,0));
    TRY(io(r,0x8008f254,0x800a5810,0,0,0,0,&tick));READ(0x800c4a74,0x8008f260,old);
    if(old!=tick){
        WRITE(0x800c4a74,0x8008f274,4,tick);TRY(io(r,0x8008f278,0x80090f6c,0,0,0,0,0));
        for(i=0;i<8;++i){
            READ(0x800d7a48,0x8008f288,mode);
            TRY(io(r,0x8008f2a0,0x800913bc,signed32(mode)<3?i*4:i,0,0,0,&pad));pad&=65535;v=0;
            if(pad&0x10)v|=1;
            if(pad&0x40)v|=2;
            if(pad&0x80)v|=8;
            if(pad&0x20)v|=4;
            if(pad&0x1000)v|=0x20;
            if(pad&0x4000)v|=0x800;
            if(pad&0x8000)v|=0x10;
            if(pad&0x2000)v|=0x40;
            if(pad&0x800)v|=0x400;
            if(pad&0x400)v|=0x200;
            if(pad&0x200)v|=0x2000;
            if(pad&0x100)v|=0x1000;
            if(pad&1)v|=0x100;
            if(pad&8)v|=0x80;
            WRITE(0x80103fb4+i*4,0x8008f360,4,v);aggregate|=v;
        }
        WRITE(0x80103fd4,0x8008f37c,4,aggregate);
    }
    /* The cached path does not rewrite aggregate. Unsigned index>7 selects8. */
    READ(0x80103fb4+(index<8?index:8)*4,0x8008f39c,*value);return NBA97_TEXT_COMPLETE;
}
static int monitor(Run* r,uint32_t* yes){
    uint32_t a,v;*yes=0;
    for(a=0x1f800030;a<=0x1f80003c;a+=4){READ(a,0x80055f0c,v);if(v!=a)return NBA97_TEXT_COMPLETE;}
    READ(0x1f800004,0x80055f0c,v);*yes=(v&0xff0000ffu)==0xff0000ffu;return NBA97_TEXT_COMPLETE;
}
static int visibility(Run* r,uint32_t bit,int enabled){
    uint32_t v,mask=1u<<(bit&31);READ(0x800fcc54,enabled?0x8004d364:0x8004d338,v);
    if(enabled){if(!(v&mask))WRITE(0x800fcc54,0x8004d380,4,v+mask);}
    else if(v&mask)WRITE(0x800fcc54,0x8004d354,4,v-mask);
    return NBA97_TEXT_COMPLETE;
}
static int random_word(Run* r,uint32_t* out){
    uint32_t v[6],carry,next,temp;unsigned i;
    for(i=0;i<6;++i)READ(0x800c4afc-i*4,0x800935cc+i*4,v[5-i]);
    temp=v[4]+v[5];carry=temp<v[5];v[4]=temp;
    for(i=3;i>0;--i){temp=v[i]+v[i+1];next=temp<v[i+1];v[i]=temp+carry;carry=next+(v[i]<carry);}
    v[0]+=v[1]+carry;
    for(i=5;;--i){++v[i];if(v[i]||!i)break;}
    for(i=0;i<6;++i)WRITE(0x800c4afc-i*4,0x80093674+i*4,4,v[5-i]);
    *out=v[0];return NBA97_TEXT_COMPLETE;
}
static int unaligned_read(Run* r,uint32_t pc,uint32_t a,Bits* out){
    Bits l,h;uint32_t end=a+3;unsigned n=(end&3)+1,shift=4-n,low=4-(a&3);uint32_t mask=low==4?~0u:(1u<<(low*8))-1u;
    TRY(bits(r,pc,end&~3u,n,1,&h));TRY(bits(r,pc+4,a,low,1,&l));
    out->word=(h.word<<(shift*8)&~mask)|l.word;out->known=((h.known<<shift)&~((1u<<low)-1u))|l.known;return NBA97_TEXT_COMPLETE;
}
static int unaligned_write(Run* r,uint32_t pc,uint32_t a,Bits b){
    uint32_t end=a+3;unsigned n=(end&3)+1;Bits h={0,0};h.word=b.word>>((4-n)*8);h.known=b.known>>(4-n);
    TRY(store_bits(r,pc,end&~3u,n,1,h));return store_bits(r,pc+4,a,4-(a&3),1,b);
}
static int copy26(Run* r,uint32_t source,uint32_t destination){
    Bits b[4];unsigned i;int backward=signed32(source)<signed32(destination)&&signed32(destination)<signed32(source+26u);
    int aligned=((source|destination)&3u)==0;uint32_t s=source,d=destination;
    /* Only source-proven26-byte calls. Original ADD (not ADDU) traps on
     * signed overflow in its overlap checks; the fixed source is in GAME. */
    if(signed32(source)<signed32(destination)&&(signed32(source)+26>INT32_MAX)){stop(r,0x800aa65c,source);return NBA97_TEXT_ARGUMENT;}
    if(backward){s+=26;d+=26;
        for(i=0;i<4;++i)TRY(unaligned_read(r,0x800aa6e0+i*8,s-16+i*4,&b[i]));
        for(i=0;i<4;++i)TRY(unaligned_write(r,0x800aa700+i*8,d-16+i*4,b[i]));
        s-=16;d-=16;
        for(i=0;i<2;++i){TRY(unaligned_read(r,0x800aa73c,s-4,&b[0]));TRY(unaligned_write(r,0x800aa748,d-4,b[0]));s-=4;d-=4;}
        for(i=0;i<2;++i){TRY(bits(r,0x800aa768,s-1,1,1,&b[0]));TRY(store_bits(r,0x800aa770,d-1,1,1,b[0]));--s;--d;}
    }else{
        for(i=0;i<4;++i){if(aligned)TRY(bits(r,0x800aa528+i*4,s+i*4,4,4,&b[i]));else TRY(unaligned_read(r,0x800aa5b4+i*8,s+i*4,&b[i]));}
        for(i=0;i<4;++i){if(aligned)TRY(store_bits(r,0x800aa538+i*4,d+i*4,4,4,b[i]));else TRY(unaligned_write(r,0x800aa5d4+i*8,d+i*4,b[i]));}s+=16;d+=16;
        for(i=0;i<2;++i){if(aligned){TRY(bits(r,0x800aa564,s,4,4,&b[0]));TRY(store_bits(r,0x800aa56c,d,4,4,b[0]));}else{TRY(unaligned_read(r,0x800aa610,s,&b[0]));TRY(unaligned_write(r,0x800aa61c,d,b[0]));}s+=4;d+=4;}
        for(i=0;i<2;++i){TRY(bits(r,aligned?0x800aa588:0x800aa63c,s,1,1,&b[0]));TRY(store_bits(r,aligned?0x800aa590:0x800aa644,d,1,1,b[0]));++s;++d;}
    }
    return NBA97_TEXT_COMPLETE;
}
static int physical_attribute(Run* r,uint32_t offset,uint32_t pc,uint32_t* attribute){
    uint32_t base;READ(0x800fc654,pc,base);READ(base+offset+0x20,pc+12,*attribute);return NBA97_TEXT_COMPLETE;
}
static int selected_attribute(Run* r,uint32_t index,uint32_t pc,uint32_t* attribute){
    uint32_t base,p;READ(0x800fc650,pc,base);READ(base+index*4,pc+12,p);READ(p+0x20,pc+20,*attribute);return NBA97_TEXT_COMPLETE;
}
static int height_edit(Run* r,uint32_t controller,uint32_t keys){
    uint32_t i,offset=0,base,record,field,v;
    for(i=0;i<10;++i,offset+=0xf4){
        READ(0x800fc654,0x8004eb70,base);record=base+offset;HALF(record+4,0x8004eb7c,v);
        if(signed16(v)!=(int32_t)controller)continue;
        if(keys&1){READ(record+0x20,0x8004eb94,field);BYTE(field+9,0x8004eb9c,v);WRITE(field+9,0x8004eba8,1,v+1);
            TRY(selected_attribute(r,i,0x8004ebb0,&field));BYTE(field+9,0x8004ebcc,v);WRITE(field+9,0x8004ebd8,1,v+1);}
        if(keys&2){TRY(physical_attribute(r,offset,0x8004ebec,&field));BYTE(field+9,0x8004ec00,v);WRITE(field+9,0x8004ec0c,1,v-1);
            TRY(selected_attribute(r,i,0x8004ec14,&field));BYTE(field+9,0x8004ec30,v);WRITE(field+9,0x8004ec3c,1,v-1);}
        TRY(physical_attribute(r,offset,0x8004ec44,&field));BYTE(field+9,0x8004ec58,v);
        if(v>0x90){WRITE(field+9,0x8004ec6c,1,0x90);TRY(selected_attribute(r,i,0x8004ec74,&field));WRITE(field+9,0x8004ec90,1,0x90);}
        TRY(physical_attribute(r,offset,0x8004ec98,&field));BYTE(field+9,0x8004ecac,v);
        if(v<0x12){WRITE(field+9,0x8004ecc0,1,0x12);TRY(selected_attribute(r,i,0x8004ecc8,&field));WRITE(field+9,0x8004ece4,1,0x12);}
        TRY(physical_attribute(r,offset,0x8004ecec,&field));BYTE(field+9,0x8004ed00,v);
        WRITE(0x80105f48+(i<<2),0x80051ef8u,4,v*624u);break;
    }
    return NBA97_TEXT_COMPLETE;
}
static int unlocks(Run* r){
    static const uint32_t target[]={0x8004ed94,0x8004eda8,0x8004edbc,0x8004edcc,0x8004eddc,0x8004edf0,0x8004ee04,0x8004ee18,0x8004ee2c,0x8004ee40,0x8004ee54,0x8004ee68,0x8004ee7c,0x8004ee90,0x8004eea4,0x8004eeb4,0x8004eec4,0x8004eed8,0x8004eeec,0x8004ef00,0x8004ef14,0x8004ef28,0x8004ef3c,0x8004ef50};
    static const uint32_t keys[]={0x200,0x1000,1,2,8,4,0x400,0x2000,0x20,0x800,0x10,0x40,0x200,0x1000,1,2,8,4,0x400,0x2000,0x20,0x800,0x10,0x40};
    static const uint32_t stores[]={0x8004eda4,0x8004edb8,0x8004edc8,0x8004edd8,0x8004edec,0x8004ee00,0x8004ee14,0x8004ee28,0x8004ee3c,0x8004ee50,0x8004ee64,0x8004ee78,0x8004ee8c,0x8004eea0,0x8004eeb0,0x8004eec0,0x8004eed4,0x8004eee8,0x8004eefc,0x8004ef10,0x8004ef24,0x8004ef38,0x8004ef4c,0x8004ef60};
    uint32_t i,mask,state,previous,destination,jump;unsigned j;
    for(i=0;i<8;++i){
        TRY(input(r,i,&mask));READ(0x800faba4+i*4,0x8004ead8,state);
        if(state==1){
            if(mask&0x200){READ(0x80109a90,0x8004eaf8,destination);TRY(copy26(r,0x800b31fc,destination));}
            if(mask&0x2000){READ(0x80109a90,0x8004eb1c,destination);TRY(copy26(r,0x800b3234,destination));}
            if(mask==0x3200)WRITE(0x800faba4+i*4,0x8004eb3c,4,2);
            if(mask&0x1000){READ(0x80109a90,0x8004eb4c,destination);TRY(copy26(r,0x800b3218,destination));TRY(height_edit(r,i,mask));}
        }else if(state==2){
            if(mask==0x3e1a)WRITE(0x800faba4+i*4,0x8004ed40,4,1);
            if(mask==0x3600)WRITE(0x800faba4+i*4,0x8004ed50,4,0);
        }else{
            READ(0x801029cc+i*4,0x8004ed5c,previous);
            if(!previous&&mask&&state<26){
                READ(0x80026234+(state<<2),0x8004ed84,jump);
                for(j=0;j<24&&target[j]!=jump;++j){}
                /* A changed jump table cannot silently select a guessed case. */
                if(jump!=0x8004ef68){
                    if(j==24){stop(r,0x8004ed8c,jump);return NBA97_TEXT_RESOURCE;}
                    if(mask==keys[j])WRITE(0x800faba4+i*4,stores[j],4,j==23?2:j+3);
                    else WRITE(0x800faba4+i*4,0x8004ef64,4,0);
                }
            }
        }
        WRITE(0x801029cc+i*4,0x8004ef70,4,mask);
    }
    return NBA97_TEXT_COMPLETE;
}
static int debug_keys(Run* r,uint32_t* keys){
    uint32_t v,old,index,limit,mask,flags;
    READ(0x1f80001c,0x80055f0c,v);
    if(!v||(*keys&0x3000)!=0x3000||*keys==0x3614)return NBA97_TEXT_COMPLETE;
    WRITE(0x800febec,0x8004efc0,4,*keys&0x800);
    if(*keys&0x200){READ(0x800d8ee8,0x8004efd4,old);if(!(old&0x200)){READ(0x800fe9cc,0x8004efec,index);WRITE(0x800fe9cc,0x8004effc,4,index-1);}}
    if(*keys&0x400){READ(0x800d8ee8,0x8004f010,old);if(!(old&0x400)){READ(0x800fe9cc,0x8004f028,index);WRITE(0x800fe9cc,0x8004f038,4,index+1);}}
    READ(0x800fe9cc,0x8004f040,index);
    if(signed32(index)<0){READ(0x8010b60c,0x8004f054,limit);WRITE(0x800fe9cc,0x8004f05c,4,limit);}
    else{READ(0x8010b60c,0x8004f06c,limit);if(signed32(limit)<signed32(index))WRITE(0x800fe9cc,0x8004f084,4,0);}
    if(*keys&0x40){READ(0x800fe9cc,0x8004f094,index);READ(0x800fcc54,0x8004f09c,mask);v=1u<<(index&31);if(mask&v)WRITE(0x800fcc54,0x8004f0b4,4,mask-v);}
    if(*keys&0x10){READ(0x800fe9cc,0x8004f0c8,index);READ(0x800fcc54,0x8004f0d0,mask);v=1u<<(index&31);if(!(mask&v))WRITE(0x800fcc54,0x8004f0ec,4,mask+v);}
    if(*keys&0x80){READ(0x1f800004,0x80055f0c,flags);WRITE(0x1f800004,0x80055f00,4,flags^0x400);}
    *keys=0;return NBA97_TEXT_COMPLETE;
}
static int camera_keys(Run* r,uint32_t keys){
    static const uint32_t bits_[]={0x20,0x800,0x200,0x400,0x40,0x10,0x2000,0x1000,4,8,2,1};
    static const uint32_t address[]={0x800fa638,0x800fa638,0x800fa63a,0x800fa63a,0x800fa63c,0x800fa63c,0x800fa630,0x800fa630,0x800fa632,0x800fa632,0x800fa634,0x800fa634};
    static const uint32_t change[]={16,0u-16u,16,0u-16u,16,0u-16u,64,0u-64u,32,0u-32u,128,0u-128u};
    uint32_t p,v,yes;unsigned i;READ(0x800fc648,0x8004f11c,p);READ(p,0x8004f124,v);
    if(v!=1)return NBA97_TEXT_COMPLETE;
    WRITE(0x800fab98,0x8004f138,2,0);WRITE(0x800fab9a,0x8004f140,2,0);WRITE(0x800fab9c,0x8004f148,2,0);
    for(i=0;i<12;++i)if(keys&bits_[i]){HALF(address[i],0x8004f15c+i*0x24,v);WRITE(address[i],0x8004f168+i*0x24,2,v+change[i]);}
    if(keys==0x1080){TRY(monitor(r,&yes));if(yes){READ(0x1f800004,0x80055f0c,v);WRITE(0x1f800004,0x80055f00,4,v|0x200);}else TRY(io(r,0x80053688,0x800536a0,2,0,0,0,0));}
    if(keys==0x1100){TRY(monitor(r,&yes));if(yes){READ(0x1f800004,0x80055f0c,v);if(v&0x200){READ(0x1f800004,0x80055f0c,v);WRITE(0x1f800004,0x80055f00,4,v^0x200);}}else TRY(io(r,0x80053688,0x800536a0,3,0,0,0,0));}
    if(keys==0x680)TRY(io(r,0x8004f3a4,0x800a8df4,0,0,0,0,0));
    return NBA97_TEXT_COMPLETE;
}
static int relative_visibility(Run* r,uint32_t offset,uint32_t special_pc,uint32_t count_pc,int enabled){
    uint32_t special,count;
    if(special_pc<count_pc){READ(0x800dcf10,special_pc,special);READ(0x8010b60c,count_pc,count);}
    else{READ(0x8010b60c,count_pc,count);READ(0x800dcf10,special_pc,special);}
    return visibility(r,count-(special+offset),enabled);
}
#define REL(o,p,q,e) TRY(relative_visibility(r,o,p,q,e))
static int court_visibility(Run* r){
    uint32_t x,y,z,force,p,v,i;
    HALF(0x800fa638,0x8004f3b4,x);HALF(0x800fa63a,0x8004f3bc,y);HALF(0x800fa63c,0x8004f3c4,z);READ(0x800fa62c,0x8004f3cc,force);
    x&=4095;y&=4095;z&=4095;
    WRITE(0x800fa638,0x8004f3dc,2,x);WRITE(0x800fa63a,0x8004f3e4,2,y);WRITE(0x800fa63c,0x8004f3ec,2,z);
    if(force){
        static const uint32_t off[]={13,12,11,10,9,8,7,5,4,1};
        for(i=0;i<10;++i)REL(off[i],0x8004f3fc+i*28,0x8004f404+i*28,1);
        REL(0,0x8004f51c,0x8004f514,1);
    }else if(x-0xa01u<0x3ffu){
        static const uint32_t off[]={12,11,10,9,8,7,1};
        REL(13,0x8004f538,0x8004f540,0);
        for(i=0;i<7;++i)REL(off[i],0x8004f554+i*28,0x8004f55c+i*28,1);
        REL(0,0x8004f620,0x8004f618,1);HALF(0x800fab9c,0x8004f630,z);
        if(signed16(z)>=1701){REL(4,0x8004f648,0x8004f650,0);REL(5,0x8004f664,0x8004f66c,1);}
        else if(signed16(z)<-1700){REL(4,0x8004f684,0x8004f68c,1);REL(5,0x8004f6a0,0x8004f6a8,0);}
        else{REL(4,0x8004f6b8,0x8004f6c0,1);REL(5,0x8004f6d4,0x8004f6dc,1);}
    }else{
        HALF(0x800fab9c,0x8004f6ec,z);
        if(signed16(z)>=1701){REL(4,0x8004f704,0x8004f70c,0);REL(5,0x8004f78c,0x8004f794,1);}
        else if(signed16(z)<-1700){REL(4,0x8004f730,0x8004f738,1);REL(5,0x8004f74c,0x8004f754,0);}
        else{REL(4,0x8004f770,0x8004f778,1);REL(5,0x8004f78c,0x8004f794,1);}
        HALF(0x800fa638,0x8004f7a8,x);
        if(x-0x981u<0x4ffu)REL(13,0x8004f7c4,0x8004f7cc,0);else REL(13,0x8004f7e8,0x8004f7f0,1);
        HALF(0x800fa63a,0x8004f804,y);
        if(y-0x201u<0x3ffu){
            REL(7,0x8004f820,0x8004f828,0);REL(9,0x8004f83c,0x8004f844,0);
            READ(0x800fc648,0x8004f858,p);READ(p,0x8004f860,v);
            if(v==1)HALF(0x800fa634,0x8004f874,z);else HALF(0x800fab98,0x8004f894,z);
            if(signed16(z)>=0xd00){REL(11,0x8004f8e8,0x8004f8f0,1);REL(0,0x8004f95c,0x8004f954,1);}
            else{REL(11,0x8004f8ac,0x8004f8b4,0);REL(0,0x8004f8d0,0x8004f8c8,0);}
        }else{
            REL(11,0x8004f900,0x8004f908,1);REL(9,0x8004f91c,0x8004f924,1);REL(7,0x8004f938,0x8004f940,1);REL(0,0x8004f95c,0x8004f954,1);
        }
        HALF(0x800fa63a,0x8004f96c,y);
        if(y-0xa01u<0x3ffu){
            REL(8,0x8004f988,0x8004f990,0);REL(10,0x8004f9a4,0x8004f9ac,0);
            READ(0x800fc648,0x8004f9c0,p);READ(p,0x8004f9c8,v);
            if(v==1){HALF(0x800fa634,0x8004f9dc,z);force=signed16(z)>=0xd00;}
            else{HALF(0x800fab98,0x8004f9fc,z);force=signed16(z)<-0xcff;}
            if(force){REL(12,0x8004fa54,0x8004fa5c,1);REL(1,0x8004fac0,0x8004fac8,1);}
            else{REL(12,0x8004fa14,0x8004fa1c,0);REL(1,0x8004fa30,0x8004fa38,0);}
        }else{REL(12,0x8004fa6c,0x8004fa74,1);REL(10,0x8004fa88,0x8004fa90,1);REL(8,0x8004faa4,0x8004faac,1);REL(1,0x8004fac0,0x8004fac8,1);}
    }
    READ(0x800dcf10,0x8004fadc,v);
    if(v){
        READ(0x800eb684,0x8004faf0,v);
        if(signed32(v)>0){
            TRY(visibility(r,5,(v&1)!=0));READ(0x800eb684,0x8004fb24,v);--v;WRITE(0x800eb684,0x8004fb34,4,v);
            if(!v){TRY(random_word(r,&v));TRY(visibility(r,5,(v&3)!=1));}
        }else{TRY(random_word(r,&v));if((v&15)==1){TRY(random_word(r,&v));WRITE(0x800eb684,0x8004fba0,4,v&7);}}
        READ(0x800fa62c,0x8004fba8,v);if(v)TRY(visibility(r,5,1));
    }
    return NBA97_TEXT_COMPLETE;
}
static int controller(Run* r){
    uint32_t keys,v,i,screen,bank,p,yes;uint16_t rect[4];int32_t x,y;
    TRY(unlocks(r));TRY(input(r,0,&keys));TRY(debug_keys(r,&keys));TRY(camera_keys(r,keys));TRY(court_visibility(r));
    READ(0x8010b270,0x8004fbc4,v);WRITE(0x800d8ee8,0x8004fbd0,4,keys);
    if(v==2)for(i=0;i<10;++i){
        READ(0x1f80000c,0x80055f0c,v);if(!(v&(1u<<i)))continue;
        READ(0x800fea94+i*4,0x8004fc10,screen);x=signed16(screen);y=signed16((uint32_t)signed16(screen>>16)-48u);
        rect[0]=(uint16_t)x;rect[1]=(uint16_t)y;rect[2]=rect[3]=16;
        if(x<0||y<0||x>=497)continue;
        READ(0x8001ede8,0x8004fc6c,bank);if(signed32((bank<<8)+224u)<y)continue;
        TRY(io(r,0x8004fc8c,0x800997e4,0,512,351+i*16,rect,0));
    }
    if(keys==0x820){
        READ(0x800fc648,0x8004fcb8,p);WRITE(p,0x8004fcc0,4,0);TRY(monitor(r,&yes));
        if(yes){READ(0x1f800004,0x80055f0c,v);if(v&0x200){READ(0x1f800004,0x80055f0c,v);WRITE(0x1f800004,0x80055f00,4,v^0x200);}}
        else TRY(io(r,0x80053688,0x800536a0,0,0,0,0,0));
    }
    return NBA97_TEXT_COMPLETE;
}
static int math(Run* r,uint32_t pc,unsigned kind,unsigned index,uint32_t word,uint32_t* out){
    Nba97PlayerMathRequest q;Nba97GamePeriodValue value={0,0};int result;stop(r,pc,0);
    if(!r->c->math)return NBA97_TEXT_IO_REFUSED;
    q.pc=pc;q.kind=kind;q.index=index;q.word=word;result=r->c->math(r->c->math_user,&q,&value);r->p->math_result=result;
    if(result!=NBA97_BODY_OK)return NBA97_TEXT_IO_REFUSED;
    ++r->p->math_calls;
    if(out){if(value.known>1||(!value.known&&value.word))return NBA97_TEXT_ARGUMENT;if(!value.known)return NBA97_TEXT_UNKNOWN;*out=value.word;}
    return NBA97_TEXT_COMPLETE;
}
static int trig(Run* r,uint16_t a,unsigned axis,uint32_t* sine,uint32_t* cosine){
    static const uint32_t positive[]={0x800560d0,0x80056134,0x800561cc},negative[]={0x800560a8,0x8005610c,0x80056198};
    int32_t signed_a=signed16(a);uint32_t w,index=(uint32_t)(signed_a<0?-signed_a:signed_a)&4095;
    READ(0x800b3254+index*4,signed_a<0?negative[axis]:positive[axis],w);
    *sine=(uint32_t)signed16(w);if(signed_a<0)*sine=0u-*sine;*cosine=(uint32_t)signed16(w>>16);return NBA97_TEXT_COMPLETE;
}
static int euler(Run* r,const uint16_t a[3]){
    uint32_t sx,cx,sy,cy,sz,cz,t;
    TRY(trig(r,a[0],0,&sx,&cx));TRY(trig(r,a[1],1,&sy,&cy));
    WRITE(0x800f9fdc,0x80056154,2,sy);WRITE(0x800f9fe2,0x80056168,2,sar(0u-cy*sx,12));
    WRITE(0x800f9fe8,signed16(a[2])<0?0x8005617c:0x800561bc,2,sar(cy*cx,12));TRY(trig(r,a[2],2,&sz,&cz));
    /* Original56080 negates before its arithmetic shift, retaining its
     * asymmetric rounding. These products wrap32bits before the shift. */
    WRITE(0x800f9fd8,0x800561ec,2,sar(cz*cy,12));WRITE(0x800f9fda,0x80056204,2,sar(0u-sz*cy,12));
    t=sar(cz*(0u-sy),12);WRITE(0x800f9fde,0x8005623c,2,sar(sz*cx,12)-sar(t*sx,12));WRITE(0x800f9fe4,0x80056264,2,sar(sz*sx,12)+sar(t*cx,12));
    t=sar(sz*(0u-sy),12);WRITE(0x800f9fe0,0x8005629c,2,sar(cz*cx,12)+sar(t*sx,12));WRITE(0x800f9fe6,0x800562c0,2,sar(cz*sx,12)-sar(t*cx,12));
    return NBA97_TEXT_COMPLETE;
}
static int camera(Run* r){
    uint32_t flag,a[3],base[3],offset[3],w[5],v,t[3];uint16_t angles[3];unsigned i;
    READ(0x800eb678,0x8005109c,flag);if(!flag)TRY(controller(r));
    for(i=0;i<3;++i)HALF(0x800fa638+i*2,0x800510c8+i*8,a[i]);
    for(i=0;i<3;++i)HALF(0x800fa630+i*2,0x800510e0+i*8,base[i]);
    for(i=0;i<3;++i)HALF(0x800fb858+i*2,0x800510f8+i*8,offset[i]);
    for(i=0;i<3;++i){WRITE(0x800fa638+i*2,0x8005112c+i*12,2,a[i]&4095);angles[i]=(uint16_t)((a[i]&4095)+offset[i]);}
    for(i=0;i<3;++i)WRITE(0x800fb828+i*2,0x8005114c+i*8,2,base[i]);
    TRY(euler(r,angles));
    HALF(0x800f9fd8,0x80051170,w[0]);HALF(0x800f9fda,0x80051188,w[1]);HALF(0x800f9fdc,0x800511a0,w[2]);
    WRITE(0x800f9fec,0x800511ac,4,0);WRITE(0x800f9ff0,0x800511bc,4,0);WRITE(0x800f9ff4,0x800511c4,4,0);
    WRITE(0x800f9fd8,0x800511d8,2,(uint32_t)(signed16(w[0])*16/10));WRITE(0x800f9fda,0x800511ec,2,(uint32_t)(signed16(w[1])*16/10));WRITE(0x800f9fdc,0x80051200,2,(uint32_t)(signed16(w[2])*16/10));
    for(i=0;i<5;++i)TRY(read_mask(r,0x80055f18+i*4,0x800f9fd8+i*4,4,i==4?3:15,&w[i]));
    for(i=0;i<5;++i)TRY(math(r,i==4?0x80055f40:0x80055f2c+i*4,NBA97_PLAYER_ROTATION,i,i==4?w[i]&65535:w[i],0));
    for(i=0;i<3;++i)READ(0x800f9fec+i*4,0x80055f44+i*4,t[i]);
    for(i=0;i<3;++i)TRY(math(r,i==2?0x80055f5c:0x80055f50+i*4,NBA97_PLAYER_TRANSLATION,i,t[i],0));
    READ(0x800fab98,0x80056650,v);TRY(math(r,0x80056650,NBA97_PLAYER_VERTEX,0,v,0));
    TRY(read_mask(r,0x80056654,0x800fab9c,4,3,&v));TRY(math(r,0x80056654,NBA97_PLAYER_VERTEX,1,v&65535,0));
    TRY(math(r,0x8005665c,NBA97_PLAYER_TRANSFORM,0,0,0));
    for(i=0;i<3;++i){TRY(math(r,0x80056660+i*4,NBA97_PLAYER_MAC,i,0,&v));WRITE(0x800fc61c+i*4,0x80056660+i*4,4,v);}
    TRY(math(r,0x8005666c,NBA97_ROOT_FLAGS,0,0,&v));
    /* Retained geometry still has the installed ZERO translation here. Only
     * the final matrix bytes get the following sums; later callers reload it. */
    HALF(0x800fb828,0x80051230,base[0]);READ(0x800fc61c,0x80051234,t[0]);
    HALF(0x800fb82a,0x8005123c,base[1]);READ(0x800fc620,0x80051244,t[1]);
    HALF(0x800fb82c,0x8005124c,base[2]);READ(0x800fc624,0x80051254,t[2]);
    for(i=0;i<3;++i)WRITE(0x800f9fec+i*4,0x80051268+i*8,4,(uint32_t)signed16(base[i])+t[i]);
    return NBA97_TEXT_COMPLETE;
}
static int initialize(Run* r,Nba97GameCameraContext* c,Nba97GameCameraEvent* journal,size_t capacity,Nba97GameCameraProgress* p){
    size_t i,j;
    if(!c||!p||(!journal&&capacity)||(!c->memory.region&&c->memory.count))return NBA97_TEXT_ARGUMENT;
    for(i=0;i<c->memory.count;++i){Nba97GameTextRegion* a=&c->memory.region[i];
        if(!a->data||!a->size||a->size>UINT64_C(0x100000000)||(uint64_t)a->base+a->size>UINT64_C(0x100000000))return NBA97_TEXT_ARGUMENT;
        for(j=0;j<i;++j){Nba97GameTextRegion* b=&c->memory.region[j];if((uint64_t)a->base<(uint64_t)b->base+b->size&&(uint64_t)b->base<(uint64_t)a->base+a->size)return NBA97_TEXT_ARGUMENT;}
    }
    memset(p,0,sizeof *p);r->c=c;r->journal=journal;r->capacity=capacity;r->p=p;return NBA97_TEXT_COMPLETE;
}
int nba97_game_camera_input_8f224(Nba97GameCameraContext* c,uint32_t index,Nba97GameCameraEvent* j,size_t capacity,Nba97GameCameraProgress* p){
    Run r;TRY(initialize(&r,c,j,capacity,p));TRY(input(&r,index,&p->return_v0));p->completed=1;return NBA97_TEXT_COMPLETE;
}
int nba97_game_camera_controller(Nba97GameCameraContext* c,Nba97GameCameraEvent* j,size_t capacity,Nba97GameCameraProgress* p){
    Run r;TRY(initialize(&r,c,j,capacity,p));TRY(controller(&r));p->completed=1;return NBA97_TEXT_COMPLETE;
}
int nba97_game_camera(Nba97GameCameraContext* c,Nba97GameCameraEvent* j,size_t capacity,Nba97GameCameraProgress* p){
    Run r;TRY(initialize(&r,c,j,capacity,p));TRY(camera(&r));p->completed=1;return NBA97_TEXT_COMPLETE;
}
