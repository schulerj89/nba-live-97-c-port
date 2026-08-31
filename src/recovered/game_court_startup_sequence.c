#include "game_court_startup_sequence.h"
#include <string.h>

int nba97_game_court_startup_resolve_image(
    const Nba97GameCourtStartupAllocation* allocation,size_t count,
    uint32_t raw_address,Nba97GameImageReference* out){
    size_t i;const Nba97GameCourtStartupAllocation* match=0;
    if(!out||(!allocation&&count)||!raw_address)return NBA97_IMAGE_ARGUMENT;
    for(i=0;i<count;++i)if(allocation[i].raw_address==raw_address){
        if(match)return NBA97_IMAGE_ARGUMENT;
        match=&allocation[i];
    }
    if(!match||!match->image.memory)return NBA97_IMAGE_RESOURCE;
    if(match->image.memory->address_mod4_known>1||
       match->image.memory->address_mod4>3)return NBA97_IMAGE_ARGUMENT;
    if(!match->image.memory->address_mod4_known)return NBA97_IMAGE_UNKNOWN;
    if((((uint64_t)match->image.memory->address_mod4+
         (uint64_t)match->image.offset)&3u)!=(raw_address&3u))
        return NBA97_IMAGE_ARGUMENT;
    *out=match->image;
    return NBA97_IMAGE_COMPLETE;
}

static int stopped(Nba97GameCourtStartupSequenceProgress* out,int result){
    out->child_result=result;return result;
}
static void advanced(Nba97GameCourtStartupSequenceProgress* out){
    ++out->source_intervals_completed;out->child_result=NBA97_TEXT_COMPLETE;
}

int nba97_game_court_startup_sequence(
    Nba97GameCourtStartupSequenceContext* context,
    const Nba97GameCourtStartupSequenceBudgets* budget,
    const Nba97GameCourtStartupSequenceJournals* journal,
    Nba97GameCourtStartupSequenceProgress* out){
    Nba97GameCourtRosterContext roster;
    Nba97CourtInteractiveContext interactive;
    Nba97CourtPacketStartupContext packet;
    Nba97GameCourtStartupContext startup;
    Nba97GameTextPoolContext resources;
    int result;
    if(!context||!budget||!journal||!out)return NBA97_TEXT_ARGUMENT;
    memset(out,0,sizeof *out);
    out->child_result=NBA97_TEXT_COMPLETE;

    out->stage=NBA97_COURT_SEQUENCE_ROSTER;
    roster.memory=context->memory;roster.access_budget=budget->roster_accesses;
    result=nba97_game_court_roster_startup(&roster,journal->roster,
        journal->roster_capacity,&out->roster);
    if(result!=NBA97_TEXT_COMPLETE)return stopped(out,result);
    advanced(out);

    out->stage=NBA97_COURT_SEQUENCE_INTERACTIVE;
    interactive.memory=context->memory;
    interactive.access_budget=budget->interactive_accesses;
    interactive.io=context->interactive_io;interactive.user=context->interactive_user;
    result=nba97_game_court_interactive(&interactive,journal->interactive,
        journal->interactive_capacity,&out->interactive);
    if(result!=NBA97_TEXT_COMPLETE)return stopped(out,result);
    advanced(out);

    out->stage=NBA97_COURT_SEQUENCE_PACKET;
    packet.memory=context->memory;packet.access_budget=budget->packet_accesses;
    packet.io=context->packet_io;packet.user=context->packet_user;
    result=nba97_game_court_packet_startup(&packet,journal->packet,
        journal->packet_capacity,&out->packet);
    if(result!=NBA97_TEXT_COMPLETE)return stopped(out,result);
    advanced(out);

    out->stage=NBA97_COURT_SEQUENCE_TEXTURE_SELECT;
    startup.memory=context->memory;startup.io=context->startup_io;
    startup.user=context->startup_user;
    result=nba97_game_court_startup_select_texture(&startup,
        budget->texture_select_accesses,journal->texture_select,
        journal->texture_select_capacity,&out->texture_select);
    if(result!=NBA97_TEXT_COMPLETE)return stopped(out,result);
    out->loaded_texture=out->texture_select.loaded_resource;advanced(out);

    out->stage=NBA97_COURT_SEQUENCE_TEXTURE_RESOLVE;
    result=nba97_game_court_startup_resolve_image(context->allocation,
        context->allocation_count,out->loaded_texture,&out->texture_reference);
    if(result!=NBA97_IMAGE_COMPLETE)return stopped(out,result);

    out->stage=NBA97_COURT_SEQUENCE_TEXTURES;
    result=nba97_game_court_textures(out->texture_reference,
        context->texture_state,budget->texture_images,budget->texture_headers,
        context->image_io,context->image_user,&out->textures);
    if(result!=NBA97_IMAGE_COMPLETE)return stopped(out,result);
    advanced(out);

    out->stage=NBA97_COURT_SEQUENCE_GEOMETRY_SELECT;
    result=nba97_game_court_startup_select_geometry(&startup,
        out->loaded_texture,budget->geometry_select_accesses,
        journal->geometry_select,journal->geometry_select_capacity,
        &out->geometry_select);
    if(result!=NBA97_TEXT_COMPLETE)return stopped(out,result);
    out->loaded_geometry=out->geometry_select.loaded_resource;advanced(out);

    out->stage=NBA97_COURT_SEQUENCE_RESOURCES;
    resources.memory=context->memory;resources.io=context->pool_io;
    resources.user=context->pool_user;
    result=nba97_game_court_resources(&resources,out->loaded_geometry,
        budget->resource_accesses,journal->resources,
        journal->resource_capacity,&out->resources);
    if(result!=NBA97_TEXT_COMPLETE)return stopped(out,result);
    advanced(out);

    /* 48D28..48D58 restores the private ABI frame and returns. Its stack and
     * saved registers are intentionally outside every retained public owner. */
    out->stage=NBA97_COURT_SEQUENCE_PRIVATE_EPILOGUE;advanced(out);
    out->stage=NBA97_COURT_SEQUENCE_COMPLETE;out->completed=1;
    out->natural_entry=0;return NBA97_TEXT_COMPLETE;
}
