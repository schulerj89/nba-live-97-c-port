#include "recovered/frontend_help.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks=0;
void check(bool ok,const char* why) {
    ++checks;
    if(!ok) throw std::runtime_error(why);
}
bool sameRect(const Nba97HelpRect& a,const Nba97HelpRect& b) {
    return a.x==b.x && a.y==b.y && a.width==b.width && a.height==b.height;
}
bool sameModal(const Nba97HelpModal& a,const Nba97HelpModal& b) {
    return sameRect(a.target,b.target) && sameRect(a.rect,b.rect) && a.phase==b.phase && a.held==b.held;
}
constexpr Nba97HelpRect target{111,70,290,140};
constexpr Nba97HelpRect collapsed{246,110,20,10};

// This fixture owns accepted presentations only. Rendering/getters do not call
// prepare; raw input is processed after the returned shown state is retained.
// It tests the source-owner phase split, not assets, audio or physical timing.
struct Sequence {
    Nba97HelpModal state{},shown{};
    unsigned frames=0,polls=0,close_sounds=0,returns=0;
    Nba97HelpEvent present(uint16_t raw) {
        const bool poll=nba97_help_prepare_presentation(&state,&shown)!=0;
        ++frames;
        if(!poll) return NBA97_HELP_NO_EVENT;
        ++polls;
        const auto event=nba97_help_input(&state,raw);
        close_sounds+=event==NBA97_HELP_CLOSE_SOUND;
        returns+=event==NBA97_HELP_RETURNED;
        return event;
    }
};

void growthAndAcknowledgement() {
    Sequence s;
    check(nba97_help_open(&s.state,target,0x20)==NBA97_HELP_OPEN_SOUND,"open prepares the collapsed rectangle");
    check(sameRect(s.state.rect,collapsed),"opening does not consume a presentation");
    for(unsigned frame=1;frame<=17;++frame) {
        // Changed input is intentionally held throughout growth. It cannot be
        // polled until a later presentation with the newly created text.
        check(s.present(0x800)==NBA97_HELP_NO_EVENT,"growth cannot acknowledge input");
        check(s.shown.phase==NBA97_HELP_GROWING && nba97_help_visible(&s.shown) &&
              !nba97_help_text_visible(&s.shown),"every growth frame shows box without text");
        check(s.state.held==0x20 && s.polls==0,"growth must preserve invoking input and skip polls");
        if(frame==1)
            check(sameRect(s.shown.rect,{237,106,38,18}),"first displayed growth occurs exactly once");
        if(frame==16)
            check(sameRect(s.shown.rect,{111,70,290,138}) && s.state.phase==NBA97_HELP_GROWING,
                  "penultimate growth retains unfinished height");
    }
    check(sameRect(s.shown.rect,target) && s.state.phase==NBA97_HELP_WAIT_CHANGE,
          "last growth displays full box while logical owner prepares text");
    check(s.shown.phase==NBA97_HELP_GROWING && !nba97_help_text_visible(&s.shown),
          "full growth box is not already the first text presentation");

    check(s.present(0x800)==NBA97_HELP_CLOSE_SOUND,"first text presentation can acknowledge changed held input");
    check(s.shown.phase==NBA97_HELP_WAIT_CHANGE && nba97_help_text_visible(&s.shown) &&
          s.shown.held==0x20 && sameRect(s.shown.rect,target),
          "acknowledgement cannot erase text from the just-completed frame");
    check(s.state.phase==NBA97_HELP_SHRINKING && s.state.held==0x800 &&
          !nba97_help_text_visible(&s.state) && sameRect(s.state.rect,target),
          "acknowledgement prepares shrink without advancing its rectangle");
    check(s.polls==1 && s.close_sounds==1,"acknowledgement owns one input poll and one close event");
    check(s.present(0)==NBA97_HELP_NO_EVENT,"first shrink is a geometry-only presentation");
    check(s.shown.phase==NBA97_HELP_SHRINKING && sameRect(s.shown.rect,{120,74,272,132}) &&
          !nba97_help_text_visible(&s.shown),"first shrink belongs to the following frame");
}

void releaseThenFreshPress() {
    Sequence s;nba97_help_open(&s.state,target,0x20);
    for(unsigned frame=0;frame<17;++frame)s.present(0x20);
    for(unsigned frame=0;frame<3;++frame) {
        check(s.present(0x20)==NBA97_HELP_NO_EVENT && s.state.phase==NBA97_HELP_WAIT_CHANGE,
              "held opener remains in the changed-input wait");
        check(nba97_help_text_visible(&s.shown),"held opener wait presents text");
    }
    check(s.present(0)==NBA97_HELP_NO_EVENT && s.state.phase==NBA97_HELP_READY,
          "release arms fresh input after displaying the wait frame");
    check(s.shown.phase==NBA97_HELP_WAIT_CHANGE,"shown phase precedes logical release transition");
    check(s.present(0)==NBA97_HELP_NO_EVENT && s.shown.phase==NBA97_HELP_READY,
          "ready owner can present without input");
    check(s.present(0x100)==NBA97_HELP_CLOSE_SOUND && s.state.phase==NBA97_HELP_SHRINKING,
          "fresh nonzero input acknowledges after the ready frame");
    check(s.shown.phase==NBA97_HELP_READY && nba97_help_text_visible(&s.shown),
          "ready acknowledgement frame retains its text");
}

void shrinkAndReturnBarrier() {
    Sequence s;nba97_help_open(&s.state,target,0x20);
    for(unsigned frame=0;frame<17;++frame)s.present(0x20);
    s.present(0x800);
    const auto polls=s.polls;
    for(unsigned frame=1;frame<=17;++frame) {
        check(s.present(0)==NBA97_HELP_NO_EVENT,"changed input during shrink cannot return early");
        check(s.polls==polls && s.state.held==0x800 && !nba97_help_text_visible(&s.shown),
              "all shrink presentations retain closing mask and contain no input poll or text");
        if(frame<17)check(nba97_help_visible(&s.shown),"unfinished shrink keeps its box visible");
    }
    check(s.state.phase==NBA97_HELP_RETURN_BARRIER && s.shown.phase==NBA97_HELP_RETURN_BARRIER &&
          sameRect(s.shown.rect,collapsed) && !nba97_help_visible(&s.shown),
          "last shrink removes the box before the first return-barrier poll");
    check(s.returns==0 && s.polls==1,"last shrink cannot also consume changed return input");
    check(s.present(0x800)==NBA97_HELP_NO_EVENT && s.state.phase==NBA97_HELP_RETURN_BARRIER,
          "held closing input keeps the invisible return barrier");
    check(s.present(8)==NBA97_HELP_RETURNED && s.state.phase==NBA97_HELP_CLOSED,
          "a later presentation may return on a changed nonzero mask");
    check(s.shown.phase==NBA97_HELP_RETURN_BARRIER && !nba97_help_visible(&s.shown),
          "completed return presentation remains the invisible old state");
    check(s.close_sounds==1 && s.returns==1,"one complete Help sequence emits close/return once");
    const auto closed=s.state;const auto polls_after_return=s.polls;
    check(s.present(0x800)==NBA97_HELP_NO_EVENT && sameModal(s.state,closed) &&
          sameModal(s.shown,closed) && s.polls==polls_after_return,
          "closed presentation copies the state without input polling or reopening");
}

void geometryBoundaries() {
    // Counts follow the original +/-9,+/-4,+/-18,+/-8 clamped geometry.
    // Include the already-collapsed target: it still owns a growth presentation
    // before text and an invisible shrink presentation before return polling.
    const std::array<Nba97HelpRect,6> targets{{
        {121,70,270,140},{121,85,270,110},{130,75,250,130},
        {130,60,250,140},collapsed,{0,0,512,240}
    }};
    const std::array<unsigned,6> counts{{17,14,15,17,1,29}};
    for(unsigned i=0;i<targets.size();++i) {
        Sequence s;check(nba97_help_open(&s.state,targets[i],0x20)==NBA97_HELP_OPEN_SOUND,"geometry fixture opens");
        for(unsigned frame=0;frame<counts[i];++frame) {
            s.present(0x10);
            check(s.shown.phase==NBA97_HELP_GROWING && !nba97_help_text_visible(&s.shown),
                  "growth phase survives in shown copy at every target size");
        }
        check(s.state.phase==NBA97_HELP_WAIT_CHANGE && sameRect(s.shown.rect,targets[i]) && !s.polls,
              "target rectangle reaches logical wait only after its complete growth count");
        check(s.present(0x10)==NBA97_HELP_CLOSE_SOUND && nba97_help_text_visible(&s.shown),
              "first post-growth frame owns text before close");
        for(unsigned frame=0;frame<counts[i];++frame)s.present(0);
        check(s.state.phase==NBA97_HELP_RETURN_BARRIER && !nba97_help_visible(&s.shown) &&
              sameRect(s.shown.rect,collapsed) && s.polls==1,
              "shrink target reached without an extra same-frame input poll");
        check(s.present(0)==NBA97_HELP_RETURNED && s.polls==2,
              "next presentation owns the first return-barrier input sample");
    }
}

void shownStateIsStable() {
    Sequence s;nba97_help_open(&s.state,target,0x20);
    for(unsigned i=0;i<17;++i)s.present(0x20);
    s.present(0x800);
    const auto shown=s.shown;const auto logical=s.state;
    for(unsigned rebuild=0;rebuild<25;++rebuild) {
        // These are the read-only decisions used by drawing. Rebuild/paint
        // must reuse shown rather than call the accepted-presentation helper.
        check(nba97_help_visible(&s.shown) && nba97_help_text_visible(&s.shown),
              "repeated drawing retains the acknowledgement frame's text");
        check(sameModal(s.shown,shown) && sameModal(s.state,logical),
              "drawing queries cannot advance geometry or logical input state");
    }
    check(s.frames==18 && s.polls==1,"redraw queries are not accepted presentations");
    s.present(0x800);
    check(!sameRect(s.shown.rect,shown.rect) && sameRect(shown.rect,target) &&
          shown.phase==NBA97_HELP_WAIT_CHANGE,"shown state is an owning value, not an alias to live modal");
}

void invalidOutputDoesNotAdvance() {
    Nba97HelpModal state{},shown{};
    nba97_help_open(&state,target,0x20);
    shown.phase=NBA97_HELP_READY;shown.held=0x800;
    const auto initial=state;const auto sentinel=shown;
    check(!nba97_help_prepare_presentation(nullptr,&shown) && sameModal(shown,sentinel),
          "null logical state cannot overwrite the shown output");
    check(!nba97_help_prepare_presentation(&state,nullptr) && sameModal(state,initial),
          "null shown output cannot advance geometry");
    check(!nba97_help_prepare_presentation(&state,&state) && sameModal(state,initial),
          "aliased shown output cannot advance geometry or corrupt its logical phase");
    for(unsigned i=0;i<17;++i)nba97_help_prepare_presentation(&state,&shown);
    const auto waiting=state;
    check(!nba97_help_prepare_presentation(&state,&state) && sameModal(state,waiting),
          "aliased wait output cannot authorize an input sample");
}
}

int main() {
    try {
        growthAndAcknowledgement();releaseThenFreshPress();shrinkAndReturnBarrier();
        geometryBoundaries();shownStateIsStable();invalidOutputDoesNotAdvance();
        std::cout<<"HELP PRESENTATION PASS: "<<checks<<" phase/geometry/input-order assertions; no assets or physical timing\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
