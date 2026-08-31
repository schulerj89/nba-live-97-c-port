#include "game_player_frame.hpp"
#include "recovered/game_ball_attachment.h"
namespace nba97 {
int GamePlayerFrame::attachment(uint32_t entry,std::size_t budget,
    Nba97GamePeriodValue& return_value,Nba97PlayerFrameProgress& progress){
    return_value={};progress={};resetProgress();int status=validateAddresses();
    if(status!=NBA97_BODY_OK)return status;
    auto input=context(budget);
    return nba97_game_ball_attachment(&input,entry,&return_value,&progress);
}
}
