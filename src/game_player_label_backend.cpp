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

    Nba97GameRenderBuffer at(std::uint32_t address, std::size_t size, bool requireKnown) {
        for(const auto& region:regions) {
            if(address<region.base) continue;
            const auto offset=static_cast<std::uint64_t>(address)-region.base;
            if(offset>region.size || size>region.size-static_cast<std::size_t>(offset)) continue;
            if(region.known) for(std::size_t i=0;i<size;++i) {
                const auto known=region.known[static_cast<std::size_t>(offset)+i];
                if(known>1 || (requireKnown && !known))
                    throw std::invalid_argument("legacy label field needs known retained bytes");
            }
            return {region.data+static_cast<std::size_t>(offset),size};
        }
        throw std::out_of_range("text source reference is outside retained bindings");
    }
    void knownLegacy(Nba97GameRenderBuffer view) {
        Nba97GameImageMemory described{};
        if(!memory.describe(view,described)) throw std::invalid_argument("unowned legacy label buffer");
        if(described.known) for(std::size_t i=0;i<described.size;++i)
            if(described.known[i]!=1) throw std::invalid_argument("legacy label buffer is not fully known");
    }
    void validateLegacyInputs() {
        // SDK callbacks can replace these live views or change their bytes and
        // knownness. Revalidate before returning to35A44's unchecked reads.
        for(const auto* entity:labels.entity_table)
            if(entity && entity->player.data) knownLegacy(entity->player);
        if(labels.position_count && !labels.position_name)
            throw std::invalid_argument("position string table missing");
        for(std::size_t i=0;i<labels.position_count;++i) knownLegacy(labels.position_name[i]);
    }
    void refreshStyle() {
        const auto root=at(0x800b2048u,4,true);
        std::uint32_t address=0;
        for(unsigned i=0;i<4;++i) address|=std::uint32_t(root.data[i])<<(i*8);
        if(address&1u) throw std::invalid_argument("legacy style halfword alignment is unsupported");
        // Require known bytes for legacy35A44's direct halfword writes. The
        // text owner itself retains full knownness for every other allocation.
        labels.style=at(address,0x54,true);
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
                    self.lastObject=address?self.at(address,64,true):Nba97GameRenderBuffer{};
                    *created=self.lastObject;
                }
            }else if(event->kind==NBA97_LABEL_RESET_PACKET_99960) {
                //35A44 immediately resets the returned object, then object+4.
                // Use that exact returned source identity even if two source
                // bindings alias the same native bytes.
                if(!self.lastObject.data) throw std::invalid_argument("no returned label object");
                std::uint32_t delta=0;
                if(event->object.data==self.lastObject.data) delta=0;
                else if(event->object.data==self.lastObject.data+4) delta=4;
                else throw std::invalid_argument("reset is not the returned label object");
                if(event->object.size>self.lastObject.size-delta || event->object.size<4)
                    throw std::invalid_argument("returned label object span changed");
                code=nba97_game_text_reset_packet(&self.text,self.lastAddress+delta,
                    static_cast<std::uint32_t>(event->argument),&self.result.textProgress);
            }else throw std::invalid_argument("unowned label call");
            self.result.textResult=code;
            if(code!=NBA97_TEXT_COMPLETE) return 0;
            self.refreshStyle();
            self.validateLegacyInputs();
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
        LabelRun run{memory,labels,result,{},{},{},0};
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
        // Legacy C buffers have no knownness member. Refuse this restricted
        // native domain up front rather than silently read an unknown payload.
        run.validateLegacyInputs();
        run.text={{run.regions.data(),run.regions.size()},stepBudget,io,user};
        result.result=nba97_game_player_labels(&labels,LabelRun::invoke,&run,&result.completed);
        if(result.result!=NBA97_RENDER_COMPLETE && result.detail.empty())
            result.detail="label owner stopped at its retained-resource or required SDK boundary";
    }catch(const std::exception& error) {result.detail=error.what();}
    return result;
}
}
