#pragma once
#include "match_runtime.hpp"
#include "gameplay_pose.hpp"

namespace nba97 {
struct MatchPoseResult {
    Nba97GamePoseResult result=NBA97_GAME_POSE_ARGUMENT;
    bool published=false;
    unsigned completed_requests=0,sampled=0;
    std::string detail;
    GameplayPoseResource resources;
    std::array<unsigned,10> physical_entity{}; //57B18 request span from20BEC[0].
    std::array<unsigned,10> render_entity{}; //530FC render span fromFC654, separately supplied.
    std::array<Nba97GamePoseEntity,10> requests{}; // Includes a refused source prefix.
    std::array<Nba97GamePosePacket,10> packet{};
    std::array<Nba97GamePose,10> pose{};
};
// Actual57B18 request/foot owner followed by530FC render-value sampling.
// Ten request entities start at20BEC[0]; render entities start at the separately
// supplied FC654 reference. Actual4D418 sets that render reference to physical0.
// Do not infer render order from the request pointer. Publishes canonical
// request/cache/foot writes only after all requests and samples succeed.
// On native refusal, result retains source prefix diagnostics; live state does
// not advance. This frame transaction is not an original rollback behavior.
MatchPoseResult prepareMatchRuntimePoses(MatchRuntimeState&,const GameplayPoseResource&,
                                       Nba97GamePeriodReference render_first_fc654);
}
