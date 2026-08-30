#include "user_setup_session.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <unordered_set>

namespace nba97 {
void UserSetupSession::importProfiles(const std::vector<UserProfile>& profiles,bool retain_claims) {
    if(profiles.size()>20) throw std::runtime_error("User Setup profile capacity exceeded");
    std::unordered_set<uint64_t> seen;
    for(const auto& p:profiles) {
        if(!p.id || !seen.insert(p.id).second || p.name.empty() || p.name.size()>13 ||
           std::any_of(p.name.begin(),p.name.end(),[](unsigned char c){return c<32 || c>126;}))
            throw std::runtime_error("User Setup needs unique stable IDs and bounded profile names");
    }
    const bool fixed=std::any_of(profiles.begin(),profiles.end(),[](const UserProfile& p){return p.slot!=255;});
    auto ids=fixed ? std::array<uint64_t,20>{}:ids_;
    for(auto& id:ids) if(!seen.count(id)) id=0;
    Nba97UserNames names{};
    for(const auto& p:profiles) {
        if(fixed && (p.slot>=20 || ids[p.slot])) throw std::runtime_error("User Setup invalid persisted profile slot");
        auto at=fixed ? ids.begin()+p.slot:std::find(ids.begin(),ids.end(),p.id);
        if(at==ids.end()) at=std::find(ids.begin(),ids.end(),uint64_t(0));
        if(at==ids.end()) throw std::runtime_error("User Setup runtime slot overflow");
        const auto index=static_cast<unsigned>(at-ids.begin());
        *at=p.id;std::copy(p.name.begin(),p.name.end(),names.name[index]);
    }
    // Existing v1 saves have no fixed-slot metadata. First import uses vector
    // order; later imports retain IDs in their slots, including holes.
    for(unsigned i=0;i<8;++i)
        if(!retain_claims && initialized_ && state_.profile[i]>=0 &&
           ids[unsigned(state_.profile[i])]!=ids_[unsigned(state_.profile[i])])
            state_.profile[i]=-2;
    ids_=ids;names_=names;
}
void UserSetupSession::open(const std::array<uint8_t,8>& initial,
                            const std::vector<UserProfile>& profiles,int32_t clock,uint16_t prior_mask,uint8_t prior_controller) {
    importProfiles(profiles);
    std::array<int8_t,8> selectors;selectors.fill(-2); // 80035D80 initial frontend path.
    std::array<uint8_t,8> assignments=initial;
    if(initialized_) {
        std::copy_n(state_.assignment,8,assignments.begin());
        std::copy_n(state_.profile,8,selectors.begin());
    }
    if(!nba97_user_setup_open(&state_,assignments.data(),selectors.data()))
        throw std::runtime_error("User Setup invalid incoming assignment");
    // 37010 does not reset shared36B80 history on reentry.
    if(!initialized_) for(auto& r:repeat_) r.clock=clock;
    initialized_=true;masks_={};help_={};last_pass_=clock;continuing_pass_=false;next_row_=0;
    dialog_kind_=UserSetupDialog::None;dialog_={};prior_mask_=prior_mask;editor_tints_={};
    prior_controller_=prior_controller;topology_={99,-1};entry_topology_primed_=false;
}
void UserSetupSession::setControllers(unsigned topology,uint8_t connected) {
    if(topology>3) throw std::runtime_error("User Setup invalid topology");
    observed_topology_=topology;connected_=connected;
}
void UserSetupSession::primeEntryTopology() {
    if(!initialized_ || topology_.active!=99) return;
    nba97_user_setup_topology_observe(&topology_,observed_topology_&1 ? 0x8000:0,observed_topology_&2 ? 0x8000:0);
    entry_topology_primed_=true;
}
void UserSetupSession::key(unsigned controller,uint16_t mask,bool down) {
    if(controller>=8) throw std::runtime_error("User Setup invalid physical controller");
    if(down) masks_[controller]|=mask;
    else masks_[controller]&=static_cast<uint16_t>(~mask);
}
std::vector<UserSetupAction> UserSetupSession::step(int32_t clock) {
    std::vector<UserSetupAction> actions;
    if(state_.result || help_.phase!=NBA97_HELP_CLOSED || dialog_kind_!=UserSetupDialog::None) return actions;
    if(!continuing_pass_) {
        if(!entry_topology_primed_)
            nba97_user_setup_topology_observe(&topology_,observed_topology_&1 ? 0x8000:0,
                                                       observed_topology_&2 ? 0x8000:0);
        entry_topology_primed_=false;
    }
    auto before=state_;
    auto event=continuing_pass_ ? NBA97_USER_NONE:
        nba97_user_setup_global(&state_,masks_.data(),connected_);
    auto record=[&](Nba97UserEvent e,uint16_t token,const Nba97UserSetup& old) {
        const auto c=state_.controller;
        actions.push_back({e,token,c,state_.sound,old.side[c],state_.side[c],old.profile[c],state_.profile[c]});
    };
    if(event!=NBA97_USER_NONE) {
        record(event,event==NBA97_USER_CANCELLED?0x100:0x80,before);
        if(event==NBA97_USER_CANCELLED) {prior_controller_=8;prior_mask_=0x100;}
        if(state_.result) return actions;
    }
    const auto elapsed=static_cast<uint32_t>(clock)-static_cast<uint32_t>(last_pass_);
    if(!continuing_pass_) {
        // 37010 uses signed SUBU delta, unlike36B80's ordered-clock check.
        if((elapsed&0x80000000u) || elapsed<=6) return actions;
        last_pass_=clock;next_row_=0;
    }
    continuing_pass_=false;
    for(unsigned row=next_row_;row<nba97_user_setup_row_count(topology_.active);++row) {
        const auto c=unsigned(nba97_user_setup_physical(topology_.active,row));
        if(!(connected_&(1u<<c))) {nba97_user_setup_disconnect(&state_,c);continue;}
        before=state_;
        const auto token=nba97_user_setup_repeat(&repeat_[c],masks_[c],clock);
        if(editor_width_ && state_.alphabet[c]>=0) {
            auto measure=[](void* ctx,const char* text) {return static_cast<UserSetupSession*>(ctx)->editor_width_(text);};
            event=nba97_user_setup_edit_input(&state_,c,token,&names_,editor_alphabet_.data(),measure,this);
        } else {
            event=nba97_user_setup_input(&state_,c,token,&names_);
            if(event==NBA97_USER_EDIT_REQUEST && editor_width_)
                event=nba97_user_setup_edit_begin(&state_,c,&names_);
        }
        if(event!=NBA97_USER_NONE) record(event,token,before);
        if(event==NBA97_USER_EDITOR_UPDATE && state_.alphabet[c]>=0) {
            editor_tints_[c]={};std::fill_n(editor_tints_[c].rgb,3,uint8_t(128));
            nba97_reorder_tint_pulse(&editor_tints_[c]);editor_tints_[c].duration=10;
        }
        // These call blocking child owners in retail. The host must handle the
        // event before another controller can run.
        if(event==NBA97_USER_HELP || event==NBA97_USER_CAPACITY ||
           event==NBA97_USER_EDIT_REQUEST || event==NBA97_USER_DELETE_REQUEST ||
           event==NBA97_USER_PROFILE_FULL || event==NBA97_USER_NAME_DUPLICATE || event==NBA97_USER_SAVE_REQUEST) {
            continuing_pass_=true;next_row_=row+1;return actions;
        }
    }
    const auto included=nba97_user_setup_topology_mask(topology_.active);
    for(unsigned c=0;c<8;++c) if(!(included&(1u<<c))) nba97_user_setup_disconnect(&state_,c);
    return actions;
}
void UserSetupSession::openHelp(Nba97HelpRect rect,unsigned index) {
    help_index_=index;help_controller_=state_.controller;prior_controller_=state_.controller;
    nba97_modal_open_prior(&help_,rect,prior_mask_);
}
Nba97HelpEvent UserSetupSession::tickHelp() {
    const auto event=nba97_help_tick(&help_,masks_[help_controller_]);
    if(event==NBA97_HELP_CLOSE_SOUND) prior_mask_=help_.held;
    return event;
}
void UserSetupSession::openDialog(UserSetupDialog kind,Nba97HelpRect rect,unsigned c,int preference) {
    if(c>=8 || kind==UserSetupDialog::None || dialog_kind_!=UserSetupDialog::None)
        throw std::runtime_error("invalid User Setup modal request");
    dialog_={};dialog_kind_=kind;dialog_controller_=c;prior_controller_=static_cast<uint8_t>(c);
    if(kind==UserSetupDialog::Delete) nba97_reset_open_deferred(&dialog_,rect,prior_mask_,preference);
    else nba97_modal_open_prior(&dialog_.modal,rect,prior_mask_);
}
int UserSetupSession::tickDialog() {
    if(dialog_kind_==UserSetupDialog::None) return 0;
    const auto raw=masks_[dialog_controller_];
    if(dialog_kind_==UserSetupDialog::Delete) {
        const int event=nba97_reset_tick(&dialog_,raw);
        if(event&NBA97_RESET_CHOSEN) {prior_mask_=dialog_.modal.held;return event|32;}
        return event;
    }
    const auto event=nba97_help_tick(&dialog_.modal,raw);
    if(event==NBA97_HELP_CLOSE_SOUND) {prior_mask_=dialog_.modal.held;return 32;}
    return event==NBA97_HELP_RETURNED ? NBA97_RESET_RETURN:0;
}
void UserSetupSession::tickPresentation() {
    for(unsigned c=0;c<8;++c) if(state_.alphabet[c]>=0) nba97_reorder_tint_tick(&editor_tints_[c]);
}
void UserSetupSession::deferMatch() noexcept {
    state_.result=0;state_.start_latch=1;
}
void UserSetupSession::configureEditor(const std::array<char,68>& alphabet,std::function<int(const char*)> width) {
    if(!width) throw std::runtime_error("User Setup editor requires original font measurement");
    editor_alphabet_=alphabet;editor_width_=std::move(width);
}
bool UserSetupSession::saveEditor(unsigned c,UserProfileStore& store) {
    if(c>=8 || state_.alphabet[c]<0 || state_.profile[c]<0) return false;
    const auto slot=static_cast<uint8_t>(state_.profile[c]);
    if(!store.acceptExact(slot,ids_[slot],state_.draft[c],!state_.existing[c])) return false;
    // Durable state is accepted. This fixed-size update cannot allocate or fail.
    const auto* profile=store.atSlot(slot);
    ids_[slot]=profile->id;
    std::fill_n(names_.name[slot],14,char(0));
    std::copy(profile->name.begin(),profile->name.end(),names_.name[slot]);
    nba97_user_setup_edit_accept(&state_,c);
    return true;
}
bool UserSetupSession::deleteProfile(unsigned c,UserProfileStore& store) {
    if(c>=8 || state_.profile[c]<0) return false;
    const auto slot=static_cast<uint8_t>(state_.profile[c]);
    if(!store.eraseExact(slot,ids_[slot])) return false;
    ids_[slot]=0;std::fill_n(names_.name[slot],14,char(0));
    nba97_user_setup_deleted(&state_,c,&names_);
    return true;
}
}
