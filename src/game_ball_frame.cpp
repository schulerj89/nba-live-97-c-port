#include "game_player_frame.hpp"
#include "recovered/game_ball_frame.h"
namespace nba97 {
int GamePlayerFrame::ball(std::size_t budget,Nba97PlayerFrameProgress& progress){
    progress={};resetProgress();int status=validateAddresses();
    if(status!=NBA97_BODY_OK)return status;
    auto input=context(budget);return nba97_game_ball_frame(&input,&progress);
}
int GamePlayerFrame::ballShadow(std::size_t budget,Nba97PlayerFrameProgress& progress){
    progress={};resetProgress();int status=validateAddresses();
    if(status!=NBA97_BODY_OK)return status;
    auto input=context(budget);return nba97_game_ball_shadow(&input,&progress);
}
}
