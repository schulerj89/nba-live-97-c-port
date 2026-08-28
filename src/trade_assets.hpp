#pragma once
#include "frontend_help.hpp"
#include "roster_database.hpp"
#include "recovered/roster_trade.h"
#include "recovered/roster_reset.h"
#include <map>
namespace nba97 {
class TradeAssets {
public:
    explicit TradeAssets(const std::filesystem::path& root,bool sign=false);
    TradeAssets(const std::filesystem::path& root,std::uint8_t frontend_state);
    explicit TradeAssets(const std::vector<std::uint8_t>& bytes);
    const std::uint8_t* preference() const {return preference_.data();}
    const std::string& freeAgentName() const {return text_.at(0x8009D83A);}
    FrontendHelpDescriptor notice(std::uint32_t address,const std::string& subject={}) const;
    FrontendHelpDescriptor emptyNotice(bool compare) const;
    Nba97HelpRect rect(std::uint32_t address) const;
    void drawChoice(PshImage&,std::uint32_t address,const Nba97ResetPrompt&) const;
    PshImage labels(const Nba97TradeScreen&,const RosterDatabase&) const;
private:
    struct Dialog {Nba97HelpRect rect;std::vector<std::string> body,choices;};
    std::map<std::uint32_t,Dialog> dialogs_;
    std::map<std::uint32_t,std::string> text_;
    std::array<std::uint8_t,25> preference_{};
    PshFont font_,small_;
    std::uint8_t state_=13;
};
}
