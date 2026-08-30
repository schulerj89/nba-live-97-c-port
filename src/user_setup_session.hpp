#pragma once
#include "recovered/user_setup.h"
#include "recovered/frontend_help.h"
#include "recovered/roster_reset.h"
#include "user_setup_dialog.hpp"
#include "user_profiles.hpp"
#include <array>
#include <functional>

namespace nba97 {
struct UserSetupAction {
    Nba97UserEvent event=NBA97_USER_NONE;
    uint16_t token=0;
    uint8_t controller=0,sound=0,old_side=1,new_side=1;
    int8_t old_profile=-2,new_profile=-2;
};
// Platform-neutral host boundary. The C owner decides behavior; this class
// retains stable profile IDs and schedules its source-ordered input passes.
// Import is read-only. Editor acceptance uses an explicit durable transaction.
class UserSetupSession {
public:
    void open(const std::array<uint8_t,8>& initial_assignments,
              const std::vector<UserProfile>& profiles,int32_t clock,uint16_t prior_mask=0x80,uint8_t prior_controller=0);
    void setControllers(unsigned topology,uint8_t connected);
    void primeEntryTopology(); // Prepare only the first outer observation for initial rendering.
    void key(unsigned controller,uint16_t mask,bool down);
    void releaseKeys() noexcept {masks_={};}
    void configureEditor(const std::array<char,68>& alphabet,std::function<int(const char*)> width);
    bool saveEditor(unsigned controller,UserProfileStore&);
    bool deleteProfile(unsigned controller,UserProfileStore&);
    std::vector<UserSetupAction> step(int32_t clock);
    void openHelp(Nba97HelpRect rect,unsigned index);
    Nba97HelpEvent tickHelp();
    void openDialog(UserSetupDialog,Nba97HelpRect,unsigned controller,int preference=0);
    int tickDialog(); // RESET events; bit32 adds closing sound for either path.
    void finishDialog() noexcept {dialog_kind_=UserSetupDialog::None;}
    void tickPresentation();
    void deferMatch() noexcept; // Native pending boundary, not a retail transition.
    const Nba97UserSetup& state() const {return state_;}
    const Nba97UserPlacement& placement() const {return placement_;}
    bool hasPendingRowTail() const {return pending_row_tail_>=0;}
    const Nba97UserNames& names() const {return names_;}
    const Nba97HelpModal& help() const {return help_;}
    unsigned helpIndex() const {return help_index_;}
    unsigned topology() const {return topology_.active;}
    int topologyCountdown() const {return topology_.countdown;}
    uint16_t priorMask() const {return prior_mask_;}
    uint8_t priorController() const {return prior_controller_;}
    bool cancelReady() const {return nba97_user_setup_cancel_ready(&state_,masks_.data(),connected_)!=0;}
    uint8_t connected() const {return connected_;}
    uint16_t raw(unsigned controller) const {return masks_.at(controller);}
    const std::array<uint64_t,20>& profileIds() const {return ids_;}
    UserSetupDialog dialogKind() const {return dialog_kind_;}
    const Nba97ResetPrompt& dialogState() const {return dialog_;}
    unsigned dialogController() const {return dialog_controller_;}
    const std::array<Nba97ReorderTint,8>& editorTints() const {return editor_tints_;}
private:
    void importProfiles(const std::vector<UserProfile>& profiles,bool retain_claims=false);
    void observeTopology();
    Nba97UserSetup state_{};
    Nba97UserPlacement placement_{};
    Nba97UserNames names_{};
    std::array<uint64_t,20> ids_{};
    std::array<Nba97UserRepeat,8> repeat_{};
    std::array<uint16_t,8> masks_{};
    Nba97HelpModal help_{};
    Nba97UserTopology topology_{99,-1};
    unsigned observed_topology_=0,help_index_=0,help_controller_=0;
    uint8_t connected_=1;
    int32_t last_pass_=0;
    bool initialized_=false;
    bool continuing_pass_=false;
    bool entry_topology_primed_=false;
    unsigned next_row_=0;
    int pending_row_tail_=-1;
    std::array<char,68> editor_alphabet_{};
    std::function<int(const char*)> editor_width_;
    UserSetupDialog dialog_kind_=UserSetupDialog::None;
    Nba97ResetPrompt dialog_{};
    unsigned dialog_controller_=0;
    uint16_t prior_mask_=0x80;
    uint8_t prior_controller_=0;
    std::array<Nba97ReorderTint,8> editor_tints_{};
};
}
