#include "spu_transfer_mapping.h"
#include <string.h>

int nba97_spu_transfer_mapping_invoke(void* context,const Nba97VoicePatlMemory* memory,
    const Nba97VoiceMappingEvent* event,uint32_t* result) {
    Nba97SpuTransferMapping* bridge=(Nba97SpuTransferMapping*)context;
    Nba97SpuTransferMappingProgress* p;Nba97SpuTransfer owner;
    Nba97SpuTransferEvent* journal;int rc;
    if(!bridge||!memory||!event||!result)return 0;
    if(event->call!=NBA97_MAPPING_TRANSFER_7DC90&&event->call!=NBA97_MAPPING_TEST_EVENT_7F568)
        return bridge->platform?bridge->platform(bridge->platform_context,memory,event,result):0;
    p=&bridge->progress;
    p->last_operation=event->call==NBA97_MAPPING_TRANSFER_7DC90?
        NBA97_SPU_TRANSFER_7DC90:NBA97_SPU_TEST_EVENT_7F568;
    memset(&p->last,0,sizeof p->last);
    if(p->accesses>bridge->access_budget||p->events>bridge->journal_capacity||
        (!bridge->journal&&bridge->journal_capacity)) { p->completion=NBA97_PATL_ARGUMENT;return 0; }
    owner.memory=*memory;owner.io=bridge->io;owner.user=bridge->io_context;
    owner.access_budget=bridge->access_budget-p->accesses;
    journal=bridge->journal?bridge->journal+p->events:NULL;
    rc=nba97_spu_transfer(&owner,p->last_operation,event->a0,event->a1,0,journal,
        bridge->journal_capacity-p->events,&p->last);
    p->accesses+=p->last.accesses;p->events+=p->last.events;p->completion=rc;
    if(rc!=1)return 0;
    if(!p->last.returned.known) { p->completion=NBA97_PATL_RESOURCE;return 0; }
    ++p->operations_completed;*result=p->last.returned.word;return 1;
}
