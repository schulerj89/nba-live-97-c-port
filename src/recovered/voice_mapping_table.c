#include "voice_mapping_table.h"
static int validate(const Nba97VoiceMappingTable* table,uint32_t at){
    size_t j;
    if(!table||!table->data||table->size>UINT32_MAX||at>table->size||4>table->size-at||(at&3u))return NBA97_VOICE_TABLE_RESOURCE;
    if(table->known)for(j=at;j<(size_t)at+4;++j)if(table->known[j]>1)return NBA97_VOICE_TABLE_METADATA;
    return NBA97_VOICE_TABLE_COMPLETE;
}
int nba97_voice_mapping_table_read(const Nba97VoiceMappingTable* table,uint32_t at,uint32_t* value){
    size_t j;uint32_t v=0;int rc;
    if(!value)return NBA97_VOICE_TABLE_ARGUMENT;
    rc=validate(table,at);if(rc!=1)return rc;
    if(table->known)for(j=at;j<(size_t)at+4;++j)if(!table->known[j])return NBA97_VOICE_TABLE_RESOURCE;
    for(j=0;j<4;++j)v|=(uint32_t)table->data[at+j]<<(j*8);
    *value=v;return NBA97_VOICE_TABLE_COMPLETE;
}
int nba97_voice_mapping_table_write(Nba97VoiceMappingTable* table,uint32_t at,uint32_t value){
    size_t j;int rc=validate(table,at);if(rc!=1)return rc;
    for(j=0;j<4;++j){table->data[at+j]=(uint8_t)(value>>(j*8));if(table->known)table->known[at+j]=1;}
    return NBA97_VOICE_TABLE_COMPLETE;
}
