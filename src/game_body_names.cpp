#include "game_body_names.hpp"
#include <exception>

namespace nba97 {
namespace {
struct NameIo {
    Nba97GameRenderIo call;
    void* context;
    bool threw=false;
    static int invoke(void* user,const Nba97GameRenderIoEvent* event) noexcept {
        auto& io=*static_cast<NameIo*>(user);
        try{return io.call(io.context,event);}
        catch(...){io.threw=true;return 0;}
    }
};
}
GameBodyNamesResult recenterGameBodyNames(GameBodyResources& body,GameBodyNameState& names,
                                         std::size_t journalCapacity) {
    GameBodyNamesResult result;
    try {
        std::array<Nba97GameBodyBuffer,GameBodyResources::Count> buffers{};
        for(unsigned i=0;i<buffers.size();++i)buffers[i]=body.buffer(i);
        Nba97GameBodyNamesState state{};
        state.buffers=buffers.data();state.buffer_count=buffers.size();
        state.contexts_f0ed8=GameBodyResources::context(0);
        for(unsigned p=0;p<10;++p)for(unsigned j=0;j<4;++j){
            state.name_polygon[p][j]=names.polygon[p][j];state.name_center[p][j]=names.center[p][j];
        }
        result.journal.resize(journalCapacity);
        result.result=nba97_game_body_names(&state,result.journal.empty()?nullptr:result.journal.data(),
            result.journal.size(),&result.progress);
        result.journal.resize(result.progress.writes);
        for(unsigned p=0;p<10;++p)for(unsigned j=0;j<4;++j){
            names.polygon[p][j]=state.name_polygon[p][j];names.center[p][j]=state.name_center[p][j];
        }
        if(result.result!=NBA97_BODY_OK)result.detail="504A8 name tail stopped at its required source input or journal bound";
    }catch(const std::exception& error){result.result=NBA97_BODY_ARGUMENT;result.detail=error.what();}
    return result;
}
GameBodyNameRenderResult renderGameBodyName(GameBodyResources* body,GameBodyNameState& names,
    Nba97GameRenderTextures& textures,unsigned player,Nba97GameRenderIo io,void* context) {
    GameBodyNameRenderResult result;
    if(player>=10||!io){result.detail="name player or required I/O callback is invalid";return result;}
    try {
        std::array<Nba97GameRenderBuffer,4> packets{};
        for(unsigned j=0;j<4;++j){
            if(names.center[player][j].known>1){result.detail="noncanonical center knownness";return result;}
            if(!textures.bypass_name_uv){
                if(!body){result.result=NBA97_RENDER_RESOURCE;result.detail="name geometry owner is required";return result;}
                //4E3CC reaches through byte1C. No numeric pointer bytes or
                // unknown packet contents can enter its byte-only interface.
                packets[j]=body->knownBuffer(names.polygon[player][j],29);
            }
        }
        for(unsigned j=0;j<4;++j){
            if(!textures.bypass_name_uv)textures.name_polygon[player][j]=packets[j];
            //4E3CC never reads an old center before overwriting it. The exact
            // write receipt below prevents an early refusal making it known.
            textures.name_center[player][j]=names.center[player][j].word;
        }
        result.entered=true;
        NameIo callback{io,context};
        result.result=nba97_game_render_name_tracked(&textures,player,NameIo::invoke,&callback,&result.centersWritten);
        for(unsigned j=0;j<4;++j)if(result.centersWritten&(1u<<j))
            names.center[player][j]={textures.name_center[player][j],1};
        if(callback.threw)result.detail="required render callback threw; source write prefix retained";
    }catch(const std::exception& error){result.result=NBA97_RENDER_RESOURCE;result.detail=error.what();}
    return result;
}
}
