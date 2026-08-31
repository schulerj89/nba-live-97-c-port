#include "match_input_edges.hpp"
#include <utility>

namespace nba97 {
MatchInputEdgeResult updateMatchRuntimeInputEdges(MatchRuntimeState& live,
    Nba97GamePeriodReference controller,std::uint32_t mappedMask,const MatchInputCamera& camera) {
    MatchInputEdgeResult result;
    try {
        if(controller.known>1 || (!controller.known&&controller.record))
            throw std::invalid_argument("controller reference provenance");
        if(!controller.known) {
            result.result=NBA97_INPUT_UNRESOLVED;result.detail="700E4 needs its actual controller reference";return result;
        }
        if(controller.record>=live.controller.size()) {
            result.result=NBA97_INPUT_REFERENCE;result.detail="700E4 controller is outside the owned records";return result;
        }
        Nba97GamePlayerInputState view{};
        for(unsigned field=0;field<NBA97_INPUT_CONTROL_COUNT;++field)
            view.controller[controller.record][field]=live.controller[controller.record].read(
                nba97_game_input_controller_offset(field),nba97_game_input_controller_width(field));
        for(unsigned i=0;i<live.entity_table.size();++i)view.entity_table[i]=live.entity_table[i];
        view.global[NBA97_INPUT_D8EEC]=camera.direction_d8eec;
        view.global[NBA97_INPUT_FC99C]=camera.mode_fc99c;
        view.global[NBA97_INPUT_FA378]=camera.flip_fa378;
        result.result=nba97_game_input_edge(&view,controller.record,mappedMask,&result.receipt);
        // No allocation, accepted-roster or resource copy is needed. Retain
        // source stores in a candidate even when a later dependency refuses.
        auto controllers=live.controller;
        auto entities=live.entity;
        for(unsigned i=0;i<result.receipt.count;++i) {
            const auto& event=result.receipt.event[i];
            if(event.kind==2) {
                if(event.record!=controller.record || event.field>=NBA97_INPUT_CONTROL_COUNT)
                    throw std::invalid_argument("unowned controller input write");
                controllers[event.record].write(nba97_game_input_controller_offset(event.field),
                    nba97_game_input_controller_width(event.field),event.value);
            }else if(event.kind==0) {
                if(event.record>=entities.size() || event.field!=NBA97_INPUT_E4)
                    throw std::invalid_argument("unowned input entity write");
                // E4 is write-only in700E4. Validate its prior native metadata
                // when reached so an invalid representation cannot be erased.
                const auto offset=nba97_game_input_entity_offset(event.field);
                const auto width=nba97_game_input_entity_width(event.field);
                entities[event.record].record.read(offset,width);
                entities[event.record].record.write(offset,width,event.value);
            }else if(event.kind!=3)throw std::invalid_argument("unowned input effect");
        }
        if(result.result!=NBA97_INPUT_OK) {
            result.detail="700E4 needs known source controller/camera data or an owned selected entity";
            return result;
        }
        live.controller=std::move(controllers);live.entity=std::move(entities);
        result.published=true;
    }catch(const std::exception& error) {result.result=NBA97_INPUT_ARGUMENT;result.detail=error.what();}
    return result;
}
}
