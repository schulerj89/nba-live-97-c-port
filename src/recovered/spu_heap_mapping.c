#include "spu_heap_mapping.h"
#include <string.h>

int nba97_spu_heap_mapping_invoke(void* context,const Nba97VoicePatlMemory* memory,
    const Nba97VoiceMappingEvent* event,uint32_t* result) {
    Nba97SpuHeapMapping* bridge=(Nba97SpuHeapMapping*)context;
    Nba97SpuHeap heap;Nba97SpuHeapMappingProgress* p;
    Nba97SpuHeapStore* journal;int rc;
    if(!bridge||!memory||!event||!result)return 0;
    if(event->call!=NBA97_MAPPING_ALLOCATE_7EC2C&&event->call!=NBA97_MAPPING_FREE_7E56C)
        return bridge->platform?bridge->platform(bridge->platform_context,memory,event,result):0;
    p=&bridge->progress;
    p->last_operation=event->call==NBA97_MAPPING_ALLOCATE_7EC2C?
        NBA97_SPU_HEAP_ALLOCATE_7EC2C:NBA97_SPU_HEAP_FREE_7E56C;
    memset(&p->last,0,sizeof p->last);
    if(p->accesses>bridge->access_budget||p->stores>bridge->journal_capacity||
        (!bridge->journal&&bridge->journal_capacity)) { p->completion=NBA97_PATL_ARGUMENT;return 0; }
    heap.memory=*memory;heap.access_budget=bridge->access_budget-p->accesses;
    journal=bridge->journal?bridge->journal+p->stores:NULL;
    rc=nba97_spu_heap(&heap,p->last_operation,event->a0,event->a1,journal,
        bridge->journal_capacity-p->stores,&p->last);
    p->accesses+=p->last.accesses;p->stores+=p->last.stores;p->completion=rc;
    if(rc!=1)return 0;
    ++p->operations_completed;*result=p->last.return_v0;return 1;
}
