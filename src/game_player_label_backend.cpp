#include "game_player_label_backend.hpp"
#include <limits>
#include <stdexcept>

namespace nba97 {
namespace {
struct LabelRun {
    GameRenderMemory& memory;
    Nba97GamePlayerLabels& labels;
    GamePlayerLabelResult& result;
    std::vector<Nba97GameTextRegion> regions;
    Nba97GameTextContext text{};
    Nba97GameRenderBuffer lastObject{};
    std::uint32_t lastAddress = 0;
    std::uint32_t styleAddress = 0;

    Nba97GameRenderBuffer at(std::uint32_t address, std::size_t size) {
        for(const auto& region:regions) {
            if(address<region.base) continue;
            const auto offset=static_cast<std::uint64_t>(address)-region.base;
            if(offset>region.size || size>region.size-static_cast<std::size_t>(offset)) continue;
            return {region.data+static_cast<std::size_t>(offset),size};
        }
        throw std::out_of_range("text source reference is outside retained bindings");
    }
    int refreshStyle() {
        const auto root=at(0x800b2048u,4);
        Nba97GameImageMemory described{};
        if(!memory.describe(root,described)) return NBA97_RENDER_ARGUMENT;
        if(described.known) {
            for(unsigned i=0;i<4;++i) if(described.known[i]>1) return NBA97_RENDER_ARGUMENT;
            for(unsigned i=0;i<4;++i) if(!described.known[i]) return NBA97_LABEL_UNKNOWN;
        }
        std::uint32_t address=0;
        for(unsigned i=0;i<4;++i) address|=std::uint32_t(root.data[i])<<(i*8);
        if(address&1u) return NBA97_LABEL_ALIGNMENT;
        styleAddress=address;
        // This borrowed legacy identity is informational in the checked
        // entry. Do not require unused byte0 to be mapped just to write+2A.
        labels.style={};
        for(const auto& region:regions) if(address>=region.base && std::uint64_t(address)-region.base<region.size) {
            labels.style={region.data+(address-region.base),1};break;
        }
        return NBA97_RENDER_COMPLETE;
    }
    static int resolve(void* context,const Nba97GamePlayerLabelAccess* access,Nba97GamePlayerLabelStorage* out) {
        auto& self=*static_cast<LabelRun*>(context);
        try {
            auto view=access->buffer;
            if(access->role==1) {
                const auto code=self.refreshStyle();
                if(code!=NBA97_RENDER_COMPLETE) return code;
                view=self.at(self.styleAddress+static_cast<std::uint32_t>(access->offset),access->size);
            }else if(access->role==2) {
                if(!self.lastObject.data || view.data!=self.lastObject.data || view.size!=self.lastObject.size)
                    return NBA97_RENDER_ARGUMENT;
                view=self.at(self.lastAddress+static_cast<std::uint32_t>(access->offset),access->size);
            }else if(access->role==0) {
                if(!view.data || access->offset>view.size || access->size>view.size-access->offset)
                    return NBA97_RENDER_RESOURCE;
                Nba97GameImageMemory owner{};
                if(!self.memory.describe(view,owner)) return NBA97_RENDER_ARGUMENT;
                view={owner.data+access->offset,access->size};
            }else return NBA97_RENDER_ARGUMENT;
            Nba97GameImageMemory described{};
            if(!self.memory.describe(view,described)) return NBA97_RENDER_ARGUMENT;
            // Reached views are resolved afresh after every callback. C owns
            // canonical metadata validation and commits each source store;
            // unused padding, other players and unselected names stay unread.
            *out={described.data,described.known,static_cast<std::uint8_t>(described.address_mod4&1u),described.address_mod4_known};
            return NBA97_RENDER_COMPLETE;
        }catch(const std::out_of_range& error) {
            self.result.detail=error.what();return NBA97_RENDER_RESOURCE;
        }catch(const std::exception& error) {
            self.result.detail=error.what();return NBA97_RENDER_ARGUMENT;
        }
    }
    static int invoke(void* context,const Nba97GamePlayerLabelEvent* event,Nba97GameRenderBuffer* created) {
        auto& self=*static_cast<LabelRun*>(context);
        try {
            int code=NBA97_TEXT_ARGUMENT;
            if(event->kind==NBA97_LABEL_RESET_GROUP_30758) {
                code=nba97_game_text_reset_group(&self.text,event->group,&self.result.textProgress);
            }else if(event->kind==NBA97_LABEL_CREATE_30D18) {
                if(!created || !event->text) throw std::invalid_argument("missing native label text output");
                std::size_t size=0;
                //35A44 owns a32-byte NUL-terminated temporary. Preserve its
                // overflow refusal; this scan does not truncate a source name.
                while(size<32 && event->text[size]) ++size;
                if(size==32) throw std::invalid_argument("label temporary is not terminated");
                std::uint32_t address=0;
                const Nba97GameTextSpan span{reinterpret_cast<const std::uint8_t*>(event->text),nullptr,size+1};
                code=nba97_game_text_create_span(&self.text,event->id,span,event->x,event->y,
                    static_cast<std::uint32_t>(event->argument),&address,&self.result.textProgress);
                if(code==NBA97_TEXT_COMPLETE) {
                    self.lastAddress=address;
                    self.lastObject=address?self.at(address,1):Nba97GameRenderBuffer{};
                    *created=self.lastObject;
                }
            }else if(event->kind==NBA97_LABEL_RESET_PACKET_99960) {
                //35A44 immediately resets the returned object, then object+4.
                // Use that exact returned source identity even if two source
                // bindings alias the same native bytes.
                if(!self.lastObject.data) throw std::invalid_argument("no returned label object");
                if(event->object.data!=self.lastObject.data || event->object.size!=self.lastObject.size ||
                    (event->object_offset!=0 && event->object_offset!=4))
                    throw std::invalid_argument("returned label object span changed");
                code=nba97_game_text_reset_packet(&self.text,self.lastAddress+static_cast<std::uint32_t>(event->object_offset),
                    static_cast<std::uint32_t>(event->argument),&self.result.textProgress);
            }else throw std::invalid_argument("unowned label call");
            self.result.textResult=code;
            if(code!=NBA97_TEXT_COMPLETE) return 0;
            return 1;
        }catch(const std::exception& error) {
            self.result.detail=error.what();
            self.result.textResult=NBA97_TEXT_ARGUMENT;
            return 0;
        }
    }
};
}
GamePlayerLabelResult runGamePlayerLabels(GameRenderMemory& memory,Nba97GamePlayerLabels& labels,
    const std::vector<GameTextBinding>& bindings,Nba97GameTextIo io,void* user,std::size_t stepBudget) {
    GamePlayerLabelResult result;
    try {
        LabelRun run{memory,labels,result,{},{},{},0,0};
        run.regions.reserve(bindings.size());
        for(const auto& binding:bindings) {
            Nba97GameImageMemory described{};
            if(!binding.view.size || !memory.describe(binding.view,described) ||
                !described.address_mod4_known || described.address_mod4!=(binding.sourceAddress&3u) ||
                std::uint64_t(binding.sourceAddress)+binding.view.size>std::uint64_t(UINT32_MAX)+1)
                throw std::invalid_argument("text binding ownership/address provenance");
            for(const auto& existing:run.regions)
                if(std::uint64_t(binding.sourceAddress)<std::uint64_t(existing.base)+existing.size &&
                    std::uint64_t(existing.base)<std::uint64_t(binding.sourceAddress)+binding.view.size)
                    throw std::invalid_argument("overlapping source text bindings");
            run.regions.push_back({binding.sourceAddress,described.data,described.known,described.size});
        }
        run.text={{run.regions.data(),run.regions.size()},stepBudget,io,user};
        result.result=nba97_game_player_labels_checked(&labels,LabelRun::invoke,LabelRun::resolve,&run,&result.completed);
        if(result.result!=NBA97_RENDER_COMPLETE && result.detail.empty())
            result.detail="label owner stopped at its retained-resource or required SDK boundary";
    }catch(const std::exception& error) {result.detail=error.what();}
    return result;
}
}
