#pragma once
#include "team_select_assets.hpp"
#include "recovered/user_setup.h"
#include "recovered/roster_reset.h"
#include "user_setup_dialog.hpp"

namespace nba97 {
class UserSetupAssets {
public:
    explicit UserSetupAssets(const std::filesystem::path&);
    const std::array<TeamSelectLayout,35>& layout() const {return layout_;}
    const std::array<uint8_t,8>& initialAssignments() const {return assignments_;}
    const std::array<uint32_t,8>& colors() const {return colors_;}
    const std::array<char,68>& alphabet() const {return alphabet_;}
    const FrontendHelpPack& help() const {return help_;}
    std::string playerLabel(unsigned visual_row) const;
    const std::string& newLabel() const {return new_label_;}
    Nba97HelpRect dialogRect(UserSetupDialog) const;
    uint8_t deletePreference() const {return delete_preference_;}
    void drawDialog(PshImage&,const PshFont&,UserSetupDialog,const Nba97ResetPrompt&,const std::string& name) const;
private:
    void loadDialogs(const std::filesystem::path&);
    struct Dialog {FrontendHelpDescriptor body;std::array<std::string,2> choices;};
    std::array<Dialog,4> dialogs_;
    std::string continuation_;
    uint8_t delete_preference_=0;
    FrontendHelpPack help_;
    std::array<TeamSelectLayout,35> layout_;
    std::array<uint8_t,8> assignments_{};
    std::array<uint32_t,8> colors_{};
    std::array<char,68> alphabet_{};
    std::string player_format_,new_label_;
};
}
