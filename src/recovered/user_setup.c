#include "user_setup.h"
#include <string.h>
#include <limits.h>

int nba97_user_setup_open(Nba97UserSetup* out,const uint8_t assignment[8],const int8_t profile[8]) {
    Nba97UserSetup s;
    unsigned i;
    if(!out || !assignment || !profile) return 0;
    for(i=0;i<8;++i) if(assignment[i]>2 || profile[i]<-2 || profile[i]>=20) return 0;
    memset(&s,0,sizeof(s));
    for(i=0;i<8;++i) {
        s.assignment[i]=assignment[i];
        s.side[i]=(uint8_t)(assignment[i]==0 ? 1:assignment[i]==1 ? 2:0);
        s.profile[i]=profile[i];s.alphabet[i]=-1;
    }
    *out=s;return 1;
}
unsigned nba97_user_setup_row_count(unsigned topology) {
    const unsigned count[4]={2,5,5,8};
    return topology<4 ? count[topology]:0;
}
int nba97_user_setup_physical(unsigned topology,unsigned row) {
    const uint8_t remap[8]={0,4,5,6,7,1,2,3};
    if(row>=nba97_user_setup_row_count(topology)) return -1;
    return topology==0 || topology==2 ? remap[row]:(int)row;
}
uint8_t nba97_user_setup_topology_mask(unsigned topology) {
    unsigned row,mask=0;
    for(row=0;row<nba97_user_setup_row_count(topology);++row)
        mask|=1u<<nba97_user_setup_physical(topology,row);
    return (uint8_t)mask;
}
int nba97_user_setup_busy(const Nba97UserSetup* s) {
    unsigned i;
    if(!s) return 1;
    for(i=0;i<8;++i) if(s->alphabet[i]!=-1 || (s->side[i]!=1 && s->profile[i]==-1)) return 1;
    return 0;
}
Nba97UserEvent nba97_user_setup_global(Nba97UserSetup* s,const uint16_t masks[8],uint8_t connected) {
    unsigned i;uint16_t aggregate=0;
    if(!s || !masks || s->result) return NBA97_USER_NONE;
    s->sound=0;
    for(i=0;i<8;++i) if(connected&(1u<<i)) {
        aggregate|=masks[i];if(masks[i]&0x100) s->controller=(uint8_t)i;
    }
    if(!(aggregate&0x80)) s->start_latch=0;
    if(s->start_latch) aggregate&=0xff7fu;
    if(aggregate==0x100) {
        for(i=0;i<8;++i) {s->profile[i]=-2;s->alphabet[i]=-1;s->cursor[i]=0;}
        s->cancel_origin=8; /* Source clearing loop overwrites origin before context+71B. */
        s->result=-1;s->sound=10;return NBA97_USER_CANCELLED;
    }
    if(aggregate==0x80) {
        if(nba97_user_setup_busy(s)) return NBA97_USER_REFUSED;
        for(i=0;i<8;++i) s->assignment[i]=(uint8_t)(s->side[i]==0 ? 2:s->side[i]==1 ? 0:1);
        s->result=6;s->sound=9;return NBA97_USER_CONFIRMED;
    }
    return NBA97_USER_NONE;
}
int nba97_user_setup_disconnect(Nba97UserSetup* s,unsigned i) {
    int changed;
    if(!s || s->result || i>=8) return 0;
    changed=s->side[i]!=1 || s->profile[i]!=-2 || s->alphabet[i]!=-1 || s->cursor[i]!=0;
    s->side[i]=1;s->profile[i]=-2;s->alphabet[i]=-1;s->cursor[i]=0;s->hide_marker[i]=1;
    return changed;
}
int nba97_user_setup_connections(Nba97UserSetup* s,uint8_t connected,unsigned topology) {
    unsigned i;int changed=0;
    if(!s || s->result || topology>3) return 0;
    connected&=nba97_user_setup_topology_mask(topology);
    for(i=0;i<8;++i) if(!(connected&(1u<<i))) changed|=nba97_user_setup_disconnect(s,i);
    return changed;
}
static int claimed(const Nba97UserSetup* s,unsigned owner,int profile,int include_neutral) {
    unsigned i;
    for(i=0;i<8;++i) if(i!=owner && (include_neutral || s->side[i]!=1) && s->profile[i]==profile) return 1;
    return 0;
}
static int next_profile(const Nba97UserSetup* s,const Nba97UserNames* names,unsigned owner,int direction) {
    int candidate=s->profile[owner];unsigned count;
    for(count=0;count<22;++count) {
        candidate+=direction;
        if(candidate>19) candidate=-2;
        if(candidate<-2) candidate=19;
        if(candidate<0 || (names->name[candidate][0] && !claimed(s,owner,candidate,0))) return candidate;
    }
    return -2; /* Both negative sentinels are eligible, so valid input never reaches this guard. */
}
Nba97UserEvent nba97_user_setup_input(Nba97UserSetup* s,unsigned controller,uint16_t token,
                                    const Nba97UserNames* names) {
    unsigned i,count=0;int next;
    if(!s || !names || controller>=8 || s->result) return NBA97_USER_NONE;
    s->sound=0;s->controller=(uint8_t)controller;
    s->hide_marker[controller]=0; /* A later timed update restores the neutral marker. */
    if(s->alphabet[controller]!=-1)
        return !token ? NBA97_USER_NONE:token==0x20 ? NBA97_USER_HELP:NBA97_USER_EDIT_REQUEST;
    if(token==4 || token==8) {
        next=(int)s->side[controller]+(token==4 ? 1:-1);
        if(next<0 || next>2) return NBA97_USER_NONE;
        for(i=0;i<8;++i) if(s->side[i]==(token==4 ? 2:0)) ++count;
        if(s->side[controller]==1 && count==5) return NBA97_USER_CAPACITY;
        s->side[controller]=(uint8_t)next;s->sound=(uint8_t)(token==4 ? 1:2);
        s->hide_marker[controller]=(uint8_t)(next==1);
        if(next!=1 && s->profile[controller]>=0 && claimed(s,controller,s->profile[controller],1)) {
            s->profile[controller]=-2;s->draft[controller][0]=0;
        }
        return NBA97_USER_SIDE;
    }
    if(token==0x20) return NBA97_USER_HELP;
    if(s->side[controller]==1) return NBA97_USER_NONE;
    if(token==1 || token==2) {
        s->profile[controller]=(int8_t)next_profile(s,names,controller,token==1 ? 1:-1);
        s->sound=(uint8_t)(token==1 ? 3:4);
        return NBA97_USER_PROFILE;
    }
    if(token==0x10 && s->profile[controller]>=0) return NBA97_USER_DELETE_REQUEST;
    if(token==0x800 && s->profile[controller]!=-2) return NBA97_USER_EDIT_REQUEST;
    return NBA97_USER_NONE;
}
uint16_t nba97_user_setup_repeat(Nba97UserRepeat* r,uint16_t mask,int32_t clock) {
    if(!r) return 0;
    if(clock<=r->clock) r->remaining=-1;
    else {
        /* SUBU wraps32bits; BLTZ then clamps every signed-negative result to-1. */
        uint32_t remaining=(uint32_t)r->remaining-((uint32_t)clock-(uint32_t)r->clock);
        r->remaining=(remaining&0x80000000u) ? -1:(int32_t)remaining;
    }
    r->clock=clock;
    if(!mask) {r->last=0;return 0;} /* Before changed-mask branch; no60 reset. */
    if(mask!=r->last) {
        r->last=mask;r->remaining=60;
        return mask;
    }
    if(r->remaining>0) return 0;
    r->remaining=12;return mask;
}

static unsigned name_length(const char name[14]) {
    unsigned n=0;while(n<14 && name[n]) ++n;return n;
}
static void scan_letter(Nba97UserSetup* s,unsigned c,const char alphabet[68]) {
    unsigned i;
    for(i=0;i<68;++i) if(s->draft[c][s->cursor[c]]==alphabet[i]) {
        s->alphabet[c]=(int8_t)i;break;
    }
}
Nba97UserEvent nba97_user_setup_edit_begin(Nba97UserSetup* s,unsigned c,const Nba97UserNames* names) {
    int p;unsigned i,n;
    if(!s || !names || c>=8 || s->result || s->side[c]==1 || s->alphabet[c]!=-1) return NBA97_USER_NONE;
    s->controller=(uint8_t)c;s->sound=0;p=s->profile[c];
    if(p<-2 || p>=20) return NBA97_USER_NONE;
    if(p==-2) return NBA97_USER_EDITOR_UPDATE;
    if(p==-1) {
        for(i=0;i<20;++i) if(!names->name[i][0] && !claimed(s,c,(int)i,0)) break;
        if(i==20) return NBA97_USER_PROFILE_FULL;
        s->profile[c]=(int8_t)i;s->draft[c][0]='A';s->draft[c][1]=0;
        /* existing[c] intentionally survives a disconnected saved-name edit. */
    } else {
        n=name_length(names->name[p]);
        if(!n || n>13) return NBA97_USER_NONE; /* Native bounded input guard. */
        memcpy(s->draft[c],names->name[p],n+1);s->existing[c]=1;
    }
    s->cursor[c]=0;s->alphabet[c]=0;s->sound=6;
    return NBA97_USER_EDITOR_UPDATE;
}
Nba97UserEvent nba97_user_setup_edit_input(Nba97UserSetup* s,unsigned c,uint16_t token,
        const Nba97UserNames* names,const char alphabet[68],Nba97UserTextWidth width,void* context) {
    unsigned len,cursor,i;int measured;
    if(!s || !names || !alphabet || !width || c>=8 || s->result ||
       s->alphabet[c]<0 || s->alphabet[c]>=68 || s->profile[c]<0 || s->profile[c]>=20)
        return NBA97_USER_NONE;
    s->controller=(uint8_t)c;s->sound=0;
    len=name_length(s->draft[c]);cursor=s->cursor[c];
    if(!len || len>13 || cursor>=len) return NBA97_USER_NONE;
    /* Native boundary rejects malformed empty/NUL-cursor drafts. Source's
     * repair fallback can overrun a14-byte name; that raw branch is unclaimed. */
    measured=width(context,s->draft[c]);
    scan_letter(s,c,alphabet);
    if(token==0x20) return NBA97_USER_HELP;
    if(token==1 || token==2) {
        s->alphabet[c]=(int8_t)((s->alphabet[c]+(token==1 ? 1:67))%68);
        s->draft[c][cursor]=alphabet[(unsigned)s->alphabet[c]];
        s->sound=(uint8_t)(token==1 ? 3:4);
        while((int)cursor<(int)len-1 && width(context,s->draft[c])>=106) s->draft[c][--len]=0;
        return NBA97_USER_EDITOR_UPDATE;
    }
    if(token==0x800) {
        if(len!=13 && measured<96 && cursor<12) {
            s->draft[c][cursor]=alphabet[(unsigned)s->alphabet[c]];
            if(!s->draft[c][cursor+1]) {
                s->draft[c][cursor+1]=alphabet[(unsigned)s->alphabet[c]];
                s->draft[c][cursor+2]=0;
            }
            ++s->cursor[c];scan_letter(s,c,alphabet);s->sound=6;
        }
        return NBA97_USER_EDITOR_UPDATE;
    }
    if(token==0x10) {
        if(len<=1) {s->draft[c][0]=alphabet[0];s->draft[c][1]=0;s->cursor[c]=0;s->alphabet[c]=0;}
        else {
            /* Through the logical NUL only: raw MIPS reads the next buffer's
             * byte14 into unused tail storage. No native out-of-bounds access. */
            memmove(s->draft[c]+cursor,s->draft[c]+cursor+1,len-cursor);
            if(!s->draft[c][cursor] && cursor) --s->cursor[c];
        }
        s->sound=5;return NBA97_USER_EDITOR_UPDATE;
    }
    if(token==0x40) {
        if(len<=1 || !cursor) return NBA97_USER_NONE;
        memmove(s->draft[c]+cursor-1,s->draft[c]+cursor,len-cursor+1);
        --s->cursor[c];s->sound=5;return NBA97_USER_EDITOR_UPDATE;
    }
    if(token==8 || token==4) {
        if(token==8 && cursor) {--s->cursor[c];s->sound=2;}
        if(token==4 && cursor<13 && s->draft[c][cursor+1]) {++s->cursor[c];s->sound=4;}
        return NBA97_USER_EDITOR_UPDATE;
    }
    if(token==0x80) {
        for(i=20;i--;) if((int)i!=s->profile[c] && !strcmp(names->name[i],s->draft[c]))
            return NBA97_USER_NAME_DUPLICATE;
        return NBA97_USER_SAVE_REQUEST;
    }
    return NBA97_USER_NONE;
}
Nba97UserEvent nba97_user_setup_edit_accept(Nba97UserSetup* s,unsigned c) {
    if(!s || c>=8 || s->alphabet[c]<0 || s->profile[c]<0) return NBA97_USER_NONE;
    s->alphabet[c]=-1;s->existing[c]=0;s->start_latch|=1;s->sound=9;s->controller=(uint8_t)c;
    return NBA97_USER_SAVED;
}
Nba97UserEvent nba97_user_setup_deleted(Nba97UserSetup* s,unsigned c,const Nba97UserNames* names) {
    int p;unsigned tries;
    if(!s || !names || c>=8 || s->profile[c]<0 || s->profile[c]>=20) return NBA97_USER_NONE;
    p=s->profile[c];
    for(tries=0;tries<22;++tries) {
        if(++p>19) p=-2;
        if(p<0 || (names->name[p][0] && !claimed(s,c,p,1))) break;
    }
    s->profile[c]=(int8_t)p;s->sound=0;s->controller=(uint8_t)c;
    return NBA97_USER_DELETED;
}
