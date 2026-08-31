#pragma once
#include "game_player_projection.hpp"
#include "recovered/game_player_frame.h"
#include "recovered/game_player_root.h"
#include <vector>
namespace nba97 {
// Borrowed retained allocations, with explicit original address provenance.
// The caller owns bytes/knownness/cells for the entire synchronous call. Aliases
// use the same allocation id; allocation ranges do not overlap. No memory or
// camera/default resource is created here. Clone memory AND geometry outside
// this adapter if a refusal must not publish its source mutation prefix.
class GamePlayerFrame {
public:
    Nba97GameBodyBuffer* buffers{};
    std::size_t buffer_count{};
    const Nba97PlayerProjectionAddress* addresses{};
    std::size_t address_count{};
    GamePlayerProjectionGeometry geometry;
    std::size_t child_store_budget{100000}; // native bound, not source count
    Nba97GamePlayerGeometryProgress part_progress{};
    Nba97GamePlayerRootProgress root_progress{};
    Nba97GamePlayerProjectionProgress projection_progress{};
    std::vector<Nba97GamePlayerGeometryWrite> last_child_writes;
    uint32_t last_child{};
    // These call the actual portable C owners, not source instruction runners.
    int run(std::size_t operation_budget,Nba97PlayerFrameProgress&);
    int shadow(std::size_t operation_budget,Nba97PlayerFrameProgress&);
    int indicator(std::size_t operation_budget,Nba97PlayerFrameProgress&);
    int copy40(uint32_t source,uint32_t destination,std::size_t operation_budget,Nba97PlayerFrameProgress&);
    // Subsequent49300/49D34 passes share these exact buffers and geometry.
    int ball(std::size_t operation_budget,Nba97PlayerFrameProgress&);
    int ballShadow(std::size_t operation_budget,Nba97PlayerFrameProgress&);
    int attachment(uint32_t entry,std::size_t operation_budget,
                   Nba97GamePeriodValue& return_value,Nba97PlayerFrameProgress&);
    // Borrow this owner's exact address/reference/knownness and math adapters
    // for another recovered pass. Does not clear RAM, geometry or progress.
    // The owner, descriptors and storage must outlive synchronous context use.
    int bindContext(std::size_t operation_budget,Nba97PlayerFrameContext&);
private:
    int validateAddresses() const;
    int locate(uint32_t,unsigned,Nba97GameBodyReference&) const;
    Nba97GameBodyReference slot(uint32_t) const;
    int encoded(Nba97GameBodyReference,uint32_t&) const;
    int access(uint32_t,uint32_t,unsigned,unsigned,Nba97PlayerFrameValue&);
    int child(uint32_t);
    int math(const Nba97PlayerMathRequest&,Nba97GamePeriodValue&);
    void resetProgress();
    Nba97PlayerFrameContext context(std::size_t);
    static int accessCallback(void*,uint32_t,uint32_t,unsigned,unsigned,Nba97PlayerFrameValue*);
    static int childCallback(void*,uint32_t,uint32_t);
    static int mathCallback(void*,const Nba97PlayerMathRequest*,Nba97GamePeriodValue*);
};
}
