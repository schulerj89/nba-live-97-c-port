#include "game_match_frame.hpp"
#include "recovered/game_ball_attachment.h"
namespace nba97 {
namespace {
bool canonical(Nba97GamePeriodValue v){return v.known<=1&&(v.known||!v.word);}
}
int GameMatchFrame::access(void* user,std::uint32_t pc,std::uint32_t a,unsigned n,unsigned kind,Nba97PlayerFrameValue* value){
    if(!user)return NBA97_BODY_ARGUMENT;
    auto& m=static_cast<GameMatchFrame*>(user)->memory_;
    return m.access?m.access(m.user,pc,a,n,kind,value):NBA97_BODY_ARGUMENT;
}
int GameMatchFrame::call(void* user,const Nba97MatchFrameCall* q,Nba97GamePeriodValue* value){
    if(!user||!q||!value)return NBA97_BODY_ARGUMENT;
    auto& owner=*static_cast<GameMatchFrame*>(user);auto& frame=owner.frame_;
    *value={};owner.last_native_entry=q->entry;
    switch(q->entry){
    case 0x80048ff4:{
        auto& binding=owner.interrupt_disable;
        binding.cp0_status=owner.interrupt_status;
        binding.operation_budget=owner.child_operation_budget;
        const int result=nba97_game_frame_interrupt_disable_from_match_frame(&binding,q,value);
        owner.interrupt_status=binding.cp0_status;
        return result;
    }
    case 0x8004900c:{
        auto& binding=owner.interrupt_restore;
        binding.cp0_status=owner.interrupt_status;
        binding.operation_budget=owner.child_operation_budget;
        const int result=nba97_game_frame_interrupt_restore_from_match_frame(&binding,q,value);
        owner.interrupt_status=binding.cp0_status;
        return result;
    }
    case 0x800530fc:
        return nba97_game_pose_frame(&owner.memory_,&owner.pose_progress);
    case 0x80035bec:
        return nba97_game_player_label_frame(&owner.memory_,&owner.label_progress);
    case 0x8004a044:{
        GamePlayerMarkerUpdate marker;marker.memory=owner.memory_;
        marker.io=owner.marker_io;marker.user=owner.marker_user;
        return marker.run(owner.child_operation_budget,owner.marker_progress);
    }
    case 0x80056074:{
        auto& h=frame.geometry.root.distance;if(!canonical(h))return NBA97_BODY_ARGUMENT;
        // CTC2 H is sign-extended when read as a register, unsigned16 in RTPS.
        const auto low=q->args[0]&65535u;h={low&0x8000u?low|0xffff0000u:low,1};return NBA97_BODY_OK;
    }
    case 0x8005605c:{
        auto& x=frame.geometry.root.offset_x;auto& y=frame.geometry.root.offset_y;
        if(!canonical(x)||!canonical(y))return NBA97_BODY_ARGUMENT;
        x={q->args[0]<<16,1};y={q->args[1]<<16,1};return NBA97_BODY_OK;
    }
    case 0x8004b1a4:{
        GameNet net;net.memory=owner.memory_;net.geometry.player=frame.geometry;
        net.geometry.average_scale4=owner.average_scale4;
        const int result=net.frame(owner.child_operation_budget,owner.net_progress);
        // Transport all geometry back even on a partial call. RAM already uses
        // the exact shared allocation callbacks; neither owner is reset here.
        frame.geometry=net.geometry.player;owner.average_scale4=net.geometry.average_scale4;
        return result;
    }
    case 0x8004ac68:{
        GameCourtFrameCompose court;court.memory=owner.memory_;
        court.geometry.player=frame.geometry;
        court.geometry.average_scale4=owner.average_scale4;
        court.leading_bits=owner.leading_bits;
        const int result=court.run(owner.child_operation_budget,owner.court_progress);
        // 4AC68 shares the preceding net/player GTE state. Preserve every
        // retained geometry prefix even when the recovered owner refuses.
        frame.geometry=court.geometry.player;
        owner.average_scale4=court.geometry.average_scale4;
        owner.leading_bits=court.leading_bits;
        return result;
    }
    case 0x80052914:return frame.run(owner.child_operation_budget,owner.pass_progress);
    case NBA97_BALL_ATTACH_BLEND:case NBA97_BALL_ATTACH_PRIMARY:case NBA97_BALL_ATTACH_SECONDARY:
        return frame.attachment(q->entry,owner.child_operation_budget,*value,owner.pass_progress);
    case 0x80049300:return frame.ball(owner.child_operation_budget,owner.pass_progress);
    case 0x80049d34:return frame.ballShadow(owner.child_operation_budget,owner.pass_progress);
    default:owner.last_native_entry=0;return owner.io?owner.io(owner.user,q,value):NBA97_MATCH_FRAME_IO_REQUIRED;
    }
}
int GameMatchFrame::run(std::size_t budget,Nba97MatchFrameProgress& progress){
    progress={};pass_progress={};pose_progress={};label_progress={};marker_progress={};net_progress={};court_progress={};last_native_entry=0;
    const int result=frame_.bindContext(child_operation_budget,memory_);if(result!=NBA97_BODY_OK)return result;
    Nba97MatchFrameContext input{access,call,this,budget};return nba97_game_match_frame(&input,&progress);
}
}
