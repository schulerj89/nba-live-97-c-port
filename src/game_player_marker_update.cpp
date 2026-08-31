#include "game_player_marker_update.hpp"
namespace nba97 {
int GamePlayerMarkerUpdate::access(void* user,uint32_t pc,uint32_t address,
    unsigned width,unsigned kind,Nba97PlayerFrameValue* value){
    auto& owner=*static_cast<GamePlayerMarkerUpdate*>(user);
    return owner.memory.access(owner.memory.user,pc,address,width,kind,value);
}
int GamePlayerMarkerUpdate::call(void* user,const Nba97PlayerMarkerCall* request,
    Nba97GamePeriodValue* result){
    auto& owner=*static_cast<GamePlayerMarkerUpdate*>(user);
    return owner.io?owner.io(owner.user,request,result):NBA97_MARKER_IO_REQUIRED;
}
int GamePlayerMarkerUpdate::run(std::size_t budget,Nba97PlayerMarkerProgress& progress){
    progress={};if(!memory.access)return NBA97_BODY_ARGUMENT;
    Nba97PlayerMarkerContext context{access,call,this,budget};
    return nba97_game_player_marker_update(&context,&progress);
}
}
