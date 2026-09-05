#include "recovered/game_match_session.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if(!value) {
        std::fprintf(stderr,"game match-session check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram=0x80000000u;
constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807fffd0u;
constexpr std::uint32_t FrameSp=EntrySp-0x28u;
constexpr std::uint32_t CustomLocation=0x8001ec94u;
constexpr std::uint32_t TeamIndex=0x80021d74u;

std::uint32_t record(std::uint32_t index) {
    return 0x80023af8u+index*0x68u;
}

struct Fixture {
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x100000,0xcd);
    std::vector<std::uint8_t> ram_known=std::vector<std::uint8_t>(0x100000,1);
    std::array<std::uint8_t,0x100> stack{},stack_known{};
    Nba97GameTextRegion regions[2]={{Ram,ram.data(),ram_known.data(),ram.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}};
    Nba97GameMatchSessionContext context{{regions,2},200,EntrySp,
        0x80029ae4u,{0xa0a0a0a0u,0xb1b1b1b1u,0xc2c2c2c2u},
        0x800d79c8u,io,this};
    Nba97GameMatchSessionProgress progress{};
    std::vector<Nba97GameMatchSessionEvent> calls;
    std::size_t refuse_call=static_cast<std::size_t>(-1);
    std::size_t malformed_call=static_cast<std::size_t>(-1);
    std::uint32_t location_result=0x80027c1cu;
    std::uint8_t location_result_known=1;
    bool mutate_index_on_lookup=false;
    std::uint32_t lookup_index=0;
    bool mutate_on_teardown=false;
    std::uint32_t teardown_custom=0;
    std::uint32_t teardown_index=0;
    bool mutate_ra_on_last_wait=false;
    bool lookup_saw_cleared_record=false;
    bool stages_saw_patch=false;
    std::uint32_t original_record=0;

    Fixture() {
        stack.fill(0xcd);stack_known.fill(1);
        put(CustomLocation,0);put(TeamIndex,1);
    }
    std::uint8_t* byte(std::uint32_t address) {
        for(auto& region:regions)
            if(address>=region.base &&
               std::uint64_t(address-region.base)<region.size)
                return region.data+(address-region.base);
        return nullptr;
    }
    std::uint8_t* known(std::uint32_t address) {
        for(auto& region:regions)
            if(address>=region.base &&
               std::uint64_t(address-region.base)<region.size)
                return region.known ? region.known+(address-region.base):nullptr;
        return nullptr;
    }
    void put(std::uint32_t address,std::uint32_t value,unsigned width=4) {
        for(unsigned i=0;i<width;++i) {
            *byte(address+i)=static_cast<std::uint8_t>(value>>(i*8u));
            if(known(address+i))*known(address+i)=1;
        }
    }
    std::uint32_t get(std::uint32_t address,unsigned width=4) {
        std::uint32_t value=0;
        for(unsigned i=0;i<width;++i)
            value|=std::uint32_t(*byte(address+i))<<(i*8u);
        return value;
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GameMatchSessionEvent* event,
        Nba97GameMatchSessionValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        const auto call=f.calls.size();
        f.calls.push_back(*event);
        if(call==f.refuse_call)return 0;
        *value={0,1};
        if(event->kind==NBA97_GAME_MATCH_SESSION_LOCATION_LOOKUP) {
            f.lookup_saw_cleared_record=f.original_record &&
                f.get(f.original_record)==0;
            if(f.mutate_index_on_lookup)f.put(TeamIndex,f.lookup_index);
            *value={f.location_result,f.location_result_known};
        }
        if(event->kind>=NBA97_GAME_MATCH_SESSION_INITIALIZE &&
           event->kind<=NBA97_GAME_MATCH_SESSION_TEARDOWN &&
           f.original_record)
            f.stages_saw_patch=f.stages_saw_patch ||
                f.get(f.original_record)==0;
        if(event->kind==NBA97_GAME_MATCH_SESSION_TEARDOWN &&
           f.mutate_on_teardown) {
            f.put(CustomLocation,f.teardown_custom);
            f.put(TeamIndex,f.teardown_index);
        }
        if(event->kind==NBA97_GAME_MATCH_SESSION_PRESENTATION_WAIT &&
           event->pc==0x8002db38u && event->saved_register[0]==10u &&
           f.mutate_ra_on_last_wait)
            f.put(FrameSp+0x24u,0x55667788u);
        if(call==f.malformed_call)value->known=2;
        return 1;
    }
    int run() {return nba97_game_match_session(&context,&progress);}
};

void cold_path_and_call_order() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.calls.size()==23 && f.progress.callbacks_completed==23);
    check(f.progress.operations==54 && f.progress.accesses==31 &&
        f.progress.reads==6 && f.progress.stores==25);
    check(f.progress.clear_rectangle_calls==2 &&
        f.progress.frame_rate_reset_calls==1 &&
        f.progress.environment_calls==4 &&
        f.progress.location_lookup_calls==0 &&
        f.progress.session_stage_calls==4 &&
        f.progress.presentation_wait_calls==11 &&
        f.progress.draw_sync_calls==1 &&
        f.progress.direct_control_bytes_written==14);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp &&
        f.progress.restored_return_address==0x80029ae4u &&
        f.progress.restored_saved_register[0]==0xa0a0a0a0u &&
        f.progress.restored_saved_register[1]==0xb1b1b1b1u &&
        f.progress.restored_saved_register[2]==0xc2c2c2c2u &&
        f.progress.return_v0==0 && f.progress.return_v0_known);
    check(f.progress.initial_custom_location==0 &&
        f.progress.final_custom_location==0 &&
        !f.progress.initial_custom_location_active &&
        !f.progress.final_custom_location_active &&
        f.progress.saved_team_field[0]==0 &&
        f.progress.saved_team_field[1]==0 &&
        f.progress.saved_team_field_known_mask[0]==0x0f &&
        f.progress.saved_team_field_known_mask[1]==0x0f);

    check(f.calls[0].pc==0x8002d8f8u &&
        f.calls[0].entry==0x800aa0bcu &&
        f.calls[0].kind==NBA97_GAME_MATCH_SESSION_CLEAR_RECTANGLE &&
        f.calls[0].argument_count==5 && f.calls[0].argument[0]==0x200u &&
        f.calls[0].argument[1]==0 && f.calls[0].argument[2]==0x400u &&
        f.calls[0].argument[3]==0x200u && f.calls[0].argument[4]==0 &&
        f.calls[0].saved_register[0]==0xa0a0a0a0u);
    check(f.calls[1].pc==0x8002d908u &&
        f.calls[1].entry==0x800a7738u &&
        f.calls[1].kind==NBA97_GAME_MATCH_SESSION_FRAME_RATE_RESET &&
        f.calls[1].saved_register[0]==0xf0u);
    static constexpr std::uint32_t env_pc[4]={0x8002d928u,0x8002d948u,
        0x8002d960u,0x8002d978u};
    static constexpr std::uint32_t env_entry[4]={0x8009ca00u,0x8009cad0u,
        0x8009ca00u,0x8009cad0u};
    static constexpr std::uint32_t env_pointer[4]={0x80021eecu,0x8002205cu,
        0x80021f48u,0x80022070u};
    static constexpr std::uint32_t env_y[4]={0,0x100u,0x100u,0};
    for(unsigned i=0;i<4;++i)
        check(f.calls[2+i].pc==env_pc[i] &&
            f.calls[2+i].entry==env_entry[i] &&
            f.calls[2+i].argument_count==5 &&
            f.calls[2+i].argument[0]==env_pointer[i] &&
            f.calls[2+i].argument[1]==0 &&
            f.calls[2+i].argument[2]==env_y[i] &&
            f.calls[2+i].argument[3]==0x200u &&
            f.calls[2+i].argument[4]==0xf0u);
    static constexpr std::uint32_t stage_pc[4]={0x8002da7cu,0x8002da84u,
        0x8002da8cu,0x8002da94u};
    static constexpr std::uint32_t stage_entry[4]={0x8002db90u,
        0x8002db68u,0x8002dc38u,0x8002dc58u};
    for(unsigned i=0;i<4;++i)
        check(f.calls[6+i].pc==stage_pc[i] &&
            f.calls[6+i].entry==stage_entry[i] &&
            f.calls[6+i].argument_count==0);
    check(f.calls[10].pc==0x8002db20u &&
        f.calls[10].entry==0x800aa0bcu &&
        f.calls[10].argument[0]==0 && f.calls[10].argument[2]==0x200u &&
        f.calls[10].argument[3]==0x200u);
    check(f.calls[11].pc==0x8002db28u &&
        f.calls[11].entry==0x80029bdcu &&
        f.calls[11].saved_register[0]==0);
    check(f.calls[12].pc==0x8002db30u &&
        f.calls[12].entry==0x800994f4u &&
        f.calls[12].argument_count==1 && f.calls[12].argument[0]==0);
    for(unsigned i=0;i<10;++i)
        check(f.calls[13+i].pc==0x8002db38u &&
            f.calls[13+i].entry==0x80029bdcu &&
            f.calls[13+i].saved_register[0]==i+1u);

    check(f.get(0x80021498u,2)==0 && f.get(0x80021f05u,1)==0 &&
        f.get(0x80021f06u,1)==0 && f.get(0x80021f07u,1)==0 &&
        f.get(0x80021f61u,1)==0 && f.get(0x80021f62u,1)==0 &&
        f.get(0x80021f63u,1)==0 && f.get(0x80021f5eu,1)==0 &&
        f.get(0x80021f02u,1)==0 && f.get(0x80021f60u,1)==1 &&
        f.get(0x80021f04u,1)==1 && f.get(0x80022081u,1)==0 &&
        f.get(0x8002206du,1)==0 && f.get(0x800eb680u,1)==1 &&
        f.get(0x80015021u,1)==0);
    check(f.get(FrameSp+0x10u)==0 &&
        f.get(FrameSp+0x18u)==0xa0a0a0a0u &&
        f.get(FrameSp+0x1cu)==0xb1b1b1b1u &&
        f.get(FrameSp+0x20u)==0xc2c2c2c2u &&
        f.get(FrameSp+0x24u)==0x80029ae4u);
}

void custom_location_patch_and_restore() {
    Fixture f;f.put(CustomLocation,3);f.put(TeamIndex,2);
    f.original_record=record(2);
    f.put(f.original_record,0x11112222u);
    f.put(f.original_record+4u,0x33334444u);
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.calls.size()==24 && f.progress.operations==65 &&
        f.progress.accesses==41 && f.progress.reads==12 &&
        f.progress.stores==29 && f.progress.callbacks_completed==24);
    check(f.lookup_saw_cleared_record && f.stages_saw_patch);
    check(f.calls[6].kind==NBA97_GAME_MATCH_SESSION_LOCATION_LOOKUP &&
        f.calls[6].pc==0x8002da4cu && f.calls[6].entry==0x80081b50u &&
        f.calls[6].argument_count==1 && f.calls[6].argument[0]==3);
    check(f.progress.initial_custom_location_active &&
        f.progress.final_custom_location_active &&
        f.progress.initial_custom_location==3 &&
        f.progress.final_custom_location==3 &&
        f.progress.initial_team_index==2 &&
        f.progress.post_lookup_team_index==2 &&
        f.progress.first_restore_team_index==2 &&
        f.progress.second_restore_team_index==2);
    check(f.progress.cleared_record_address==f.original_record &&
        f.progress.replacement_record_address==f.original_record &&
        f.progress.first_restore_record_address==f.original_record &&
        f.progress.second_restore_record_address==f.original_record &&
        f.progress.saved_team_field[0]==0x11112222u &&
        f.progress.saved_team_field[1]==0x33334444u &&
        f.progress.saved_team_field_known_mask[0]==0x0f &&
        f.progress.saved_team_field_known_mask[1]==0x0f &&
        f.progress.replacement_location==f.location_result &&
        f.progress.replacement_location_known);
    check(f.get(f.original_record)==0x11112222u &&
        f.get(f.original_record+4u)==0x33334444u);

    Fixture negative;negative.put(CustomLocation,0x12348005u);
    negative.put(TeamIndex,0);negative.original_record=record(0);
    negative.put(record(0),1);negative.put(record(0)+4u,2);
    check(negative.run()==NBA97_TEXT_COMPLETE &&
        negative.calls[6].argument[0]==0xffff8005u);

    /* There is no retail range check. Adding 2^29 to the index wraps the
       source shift/add multiply by 0x68 back onto the same record. */
    Fixture unchecked;unchecked.put(CustomLocation,2);
    unchecked.put(TeamIndex,0x20000001u);
    unchecked.original_record=record(1);
    unchecked.put(record(1),0x01020304u);
    unchecked.put(record(1)+4u,0x05060708u);
    check(unchecked.run()==NBA97_TEXT_COMPLETE &&
        unchecked.progress.initial_team_index==0x20000001u &&
        unchecked.progress.cleared_record_address==record(1) &&
        unchecked.progress.replacement_record_address==record(1));
    check(unchecked.get(record(1))==0x01020304u &&
        unchecked.get(record(1)+4u)==0x05060708u);
}

void retail_recheck_and_reloaded_index_quirks() {
    /* The lookup callback changes the index. The first field was cleared in
       record 1, the replacement pointer is written in record 2, and final
       restoration uses record 2 because the source reloads the live index. */
    Fixture split;split.put(CustomLocation,1);split.put(TeamIndex,1);
    split.original_record=record(1);split.put(record(1),0xaaaabbbbu);
    split.put(record(1)+4u,0xccccddddu);split.put(record(2),0x11111111u);
    split.put(record(2)+4u,0x22222222u);split.mutate_index_on_lookup=true;
    split.lookup_index=2;
    check(split.run()==NBA97_TEXT_COMPLETE);
    check(split.progress.initial_team_index==1 &&
        split.progress.post_lookup_team_index==2 &&
        split.progress.cleared_record_address==record(1) &&
        split.progress.replacement_record_address==record(2) &&
        split.progress.first_restore_record_address==record(2) &&
        split.progress.second_restore_record_address==record(2));
    check(split.get(record(1))==0 &&
        split.get(record(1)+4u)==0xccccddddu &&
        split.get(record(2))==0xaaaabbbbu &&
        split.get(record(2)+4u)==0xccccddddu);

    /* Initial false does not prevent the independent final test from
       restoring zero-initialized s1/s2 into a newly enabled record. */
    Fixture late;late.put(CustomLocation,0);late.put(TeamIndex,1);
    late.put(record(3),0x12345678u);late.put(record(3)+4u,0x9abcdef0u);
    late.mutate_on_teardown=true;late.teardown_custom=7;
    late.teardown_index=3;
    check(late.run()==NBA97_TEXT_COMPLETE &&
        !late.progress.initial_custom_location_active &&
        late.progress.final_custom_location_active &&
        late.progress.operations==58 && late.progress.callbacks_completed==23);
    check(late.get(record(3))==0 && late.get(record(3)+4u)==0);

    /* Conversely, disabling it during the session skips restoration and
       leaves the temporary patch in place. This retail behavior is retained. */
    Fixture disabled;disabled.put(CustomLocation,4);disabled.put(TeamIndex,1);
    disabled.original_record=record(1);disabled.put(record(1),0x11112222u);
    disabled.put(record(1)+4u,0x33334444u);
    disabled.mutate_on_teardown=true;disabled.teardown_custom=0;
    disabled.teardown_index=1;
    check(disabled.run()==NBA97_TEXT_COMPLETE &&
        disabled.progress.initial_custom_location_active &&
        !disabled.progress.final_custom_location_active &&
        disabled.progress.operations==61 &&
        disabled.progress.callbacks_completed==24);
    check(disabled.get(record(1))==0 &&
        disabled.get(record(1)+4u)==disabled.location_result);
}

void unknownness_live_epilogue_and_refusals() {
    Fixture unknown_custom;
    for(unsigned i=0;i<4;++i)*unknown_custom.known(CustomLocation+i)=0;
    check(unknown_custom.run()==NBA97_TEXT_UNKNOWN &&
        unknown_custom.progress.operations==30 &&
        unknown_custom.progress.accesses==24 &&
        unknown_custom.progress.reads==1 &&
        unknown_custom.progress.stores==23 &&
        unknown_custom.progress.callbacks_completed==6 &&
        unknown_custom.progress.direct_control_bytes_written==13 &&
        unknown_custom.progress.stopped_pc==0x8002d9f8u &&
        unknown_custom.progress.stopped_address==CustomLocation);
    check(unknown_custom.get(0x800eb680u,1)==1);

    Fixture unknown_index;unknown_index.put(CustomLocation,1);
    for(unsigned i=0;i<4;++i)*unknown_index.known(TeamIndex+i)=0;
    check(unknown_index.run()==NBA97_TEXT_UNKNOWN &&
        unknown_index.progress.operations==31 &&
        unknown_index.progress.stopped_pc==0x8002da10u &&
        unknown_index.progress.stopped_address==TeamIndex);

    Fixture partial;partial.put(CustomLocation,1);partial.put(TeamIndex,1);
    partial.original_record=record(1);partial.put(record(1),0x11223344u);
    partial.put(record(1)+4u,0x55667788u);
    *partial.known(record(1)+1u)=0;*partial.known(record(1)+4u)=0;
    check(partial.run()==NBA97_TEXT_COMPLETE);
    check(partial.get(record(1))==0x11223344u &&
        partial.get(record(1)+4u)==0x55667788u &&
        *partial.known(record(1))==1 &&
        *partial.known(record(1)+1u)==0 &&
        *partial.known(record(1)+4u)==0 &&
        *partial.known(record(1)+5u)==1 &&
        partial.progress.saved_team_field_known_mask[0]==0x0d &&
        partial.progress.saved_team_field_known_mask[1]==0x0e);

    Fixture live;live.mutate_ra_on_last_wait=true;
    check(live.run()==NBA97_TEXT_COMPLETE &&
        live.progress.restored_return_address==0x55667788u);

    Fixture refused;refused.refuse_call=8;
    check(refused.run()==NBA97_TEXT_IO_REFUSED &&
        refused.calls.size()==9 && refused.progress.callbacks_completed==8 &&
        refused.progress.stopped_entry==0x8002dc38u);
    Fixture malformed;malformed.malformed_call=12;
    check(malformed.run()==NBA97_TEXT_ARGUMENT &&
        malformed.calls.size()==13 && malformed.progress.callbacks_completed==12 &&
        malformed.progress.stopped_entry==0x800994f4u);
    Fixture no_io;no_io.context.io=nullptr;
    check(no_io.run()==NBA97_TEXT_IO_REFUSED && no_io.progress.operations==6 &&
        no_io.progress.stores==5 && no_io.progress.stopped_pc==0x8002d8f8u &&
        no_io.progress.stopped_entry==0x800aa0bcu);
}

void budgets_and_memory_validation() {
    for(std::size_t budget=0;budget<54;++budget) {
        Fixture f;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations==budget);
    }
    Fixture exact;exact.context.operation_budget=54;
    check(exact.run()==NBA97_TEXT_COMPLETE && exact.progress.operations==54);

    Fixture missing_stack;missing_stack.context.memory={missing_stack.regions,1};
    check(missing_stack.run()==NBA97_TEXT_RESOURCE &&
        missing_stack.progress.stopped_pc==0x8002d8e8u);
    Fixture missing_ram;missing_ram.context.memory.region++;
    missing_ram.context.memory.count=1;
    check(missing_ram.run()==NBA97_TEXT_RESOURCE &&
        missing_ram.progress.stopped_pc==0x8002d904u);
    Fixture malformed_known;*malformed_known.known(0x80021f61u)=2;
    check(malformed_known.run()==NBA97_TEXT_ARGUMENT &&
        malformed_known.progress.stopped_pc==0x8002d9acu);
    Fixture unaligned;unaligned.context.stack_pointer++;
    check(unaligned.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc==0x8002d8e8u);
    Fixture overlap;Nba97GameTextRegion duplicate[2]={overlap.regions[0],
        overlap.regions[0]};overlap.context.memory={duplicate,2};
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture zero;zero.regions[0].size=0;
    check(zero.run()==NBA97_TEXT_ARGUMENT && !zero.progress.operations);
    Fixture null_data;null_data.regions[0].data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT && !null_data.progress.operations);
    Fixture null_regions;null_regions.context.memory={nullptr,1};
    check(null_regions.run()==NBA97_TEXT_ARGUMENT && !null_regions.progress.operations);
    Fixture wraps;wraps.regions[0].base=0xfffffffcu;wraps.regions[0].size=8;
    check(wraps.run()==NBA97_TEXT_ARGUMENT && !wraps.progress.operations);
    Nba97GameMatchSessionProgress progress{};
    check(nba97_game_match_session(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_match_session(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    cold_path_and_call_order();
    custom_location_patch_and_restore();
    retail_recheck_and_reloaded_index_quirks();
    unknownness_live_epilogue_and_refusals();
    budgets_and_memory_validation();
    std::printf("game_match_session: %u checks passed\n",checks);
}
