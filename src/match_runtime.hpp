#pragma once
#include "match_snapshot.hpp"
#include "gameplay_setup.hpp"
#include "recovered/game_period.h"
#include "recovered/game_player_attributes.h"
#include <stdexcept>

namespace nba97 {
// Bounded owned records, never a PS1 address space. Byte offsets describe the
// recovered owners' field contracts. Native references live separately below.
template<std::size_t Size> struct MatchRuntimeRecord {
    std::array<std::uint8_t,Size> bytes{},known{};
    Nba97GamePeriodValue read(unsigned offset,unsigned width) const {
        if((width!=1 && width!=2 && width!=4) || offset>Size || width>Size-offset)
            throw std::out_of_range("match field span");
        Nba97GamePeriodValue v{0,1};
        for(unsigned i=0;i<width;++i) {
            if(known[offset+i]>1 || (!known[offset+i] && bytes[offset+i]))
                throw std::invalid_argument("match field provenance");
            if(!known[offset+i]) return {};
            v.word|=std::uint32_t(bytes[offset+i])<<(i*8);
        }
        return v;
    }
    void write(unsigned offset,unsigned width,Nba97GamePeriodValue v) {
        if((width!=1 && width!=2 && width!=4) || offset>Size || width>Size-offset || v.known>1 ||
           (!v.known && v.word) || (width<4 && v.word>=(1u<<(width*8))))
            throw std::invalid_argument("match field representation");
        for(unsigned i=0;i<width;++i) {
            bytes[offset+i]=std::uint8_t(v.word>>(i*8)); known[offset+i]=v.known;
        }
    }
    void put(unsigned offset,unsigned width,std::uint32_t value) {
        write(offset,width,{value,1});
    }
    void clearFromSource() {bytes.fill(0);known.fill(1);}
};
struct MatchRuntimeEntity {
    MatchRuntimeRecord<244> record;
    Nba97GamePeriodValue player{},status{},opponent{}; // Owned indices, not pointer bits.
};
enum MatchRuntimeAux {
    MatchRuntimeMarker8e,MatchRuntimeLock54,MatchRuntimeTail8ca,
    MatchRuntimeResetD4,MatchRuntimeResetD6,MatchRuntimeResetD0,MatchRuntimeResetD2,
    MatchRuntimeAuxCount
};
// Explicit entry input: UNKNOWN by default. Intervening resource/audio owners
// must establish these values; a new C++ object alone does not prove source0.
struct MatchRuntimeEntry {
    std::array<MatchRuntimeRecord<244>,11> entity;
    std::array<Nba97GamePeriodValue,NBA97_PERIOD_SCALAR_COUNT> scalar{};
    std::array<Nba97GamePeriodValue,MatchRuntimeAuxCount> auxiliary{};
    Nba97GamePeriodValue incoming_s6{};
    Nba97GamePeriodValue render_flag21498{};
};
struct MatchRuntimeState {
    std::shared_ptr<const MatchSnapshot> accepted;
    GameplaySetupResource setup;
    std::array<MatchRuntimeRecord<196>,2> team;
    std::array<MatchRuntimeEntity,11> entity;
    std::array<MatchRuntimeRecord<34>,24> status;
    std::array<MatchRuntimeRecord<120>,8> controller;
    std::array<std::array<std::uint16_t,12>,2> player_alias{};
    std::vector<PlayerRecord> players;
    std::array<Nba97GamePeriodValue,10> active_player{},active_status{};
    std::array<Nba97TeamHeaderRef,2> team_word08{},team_word0c{};
    std::array<Nba97GamePeriodReference,11> entity_table{},render_table{};
    std::array<Nba97GamePeriodReference,8> controller_table{};
    Nba97GamePeriodReference ball{},reference34{};
    std::array<Nba97GamePeriodValue,NBA97_PERIOD_SCALAR_COUNT> scalar{};
    std::array<Nba97GamePeriodValue,MatchRuntimeAuxCount> auxiliary{};
    Nba97GamePeriodValue incoming_s6{};
    Nba97GamePeriodValue render_flag21498{};
    std::array<Nba97GamePeriodValue,11> player_height165f48{};
    std::uint64_t completed_period_initializations=0;
};
// Copies the accepted live rosters and proven boundary effects. Applies only
// the actual659F0 header/status clears; entity/global prior state is explicit.
// Does not claim to run resource loading, full659F0, or the game-entry loop.
MatchRuntimeState prepareMatchRuntime(const MatchSnapshot&,GameplaySetupResource,
                                      const MatchRuntimeEntry&);
struct MatchRuntimePeriodResult {
    int result=NBA97_PERIOD_ARGUMENT;
    int dependency_result=0;
    int pending_owner=-1;
    std::string detail;
    Nba97GamePeriodReceipt receipt{};
};
// Clones all mutable state, including callback context; publishes only a fully
// completed65DB0. Retained immutable resources cannot dangle on failed retries.
// Source traps and unavailable transitive owners refuse publication explicitly.
MatchRuntimePeriodResult initializeMatchRuntimePeriod(MatchRuntimeState&);
struct MatchRuntimeAttributesResult {
    int result=NBA97_ATTRIBUTES_ARGUMENT;
    bool published=false;
    std::string detail;
    Nba97GamePlayerAttributesEffects effects{};
};
// Executes63EDC/51ED8 using current owned player bindings and raw fields.
// Publishes only the completed flag21498==0 route. Original traps retain their
// diagnostic prefix; render tails remain explicit pending work, never skipped.
MatchRuntimeAttributesResult initializeMatchRuntimeAttributes(MatchRuntimeState&);
}
