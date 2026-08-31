#include "recovered/music_transition.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
void need(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Event { Nba97MusicTransitionCall call; std::uint32_t a,b,c; };
struct Fixture {
    Nba97MusicTransition state{};
    Nba97MusicRouting route{};
    Nba97MusicInputs input{};
    std::vector<Event> events;
    std::uint32_t finished = 0;
    bool mutate = false;
    static std::uint32_t call(void* context, Nba97MusicTransitionCall call,
        std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        auto& f = *static_cast<Fixture*>(context);
        f.events.push_back({call,a,b,c});
        if (f.mutate && call == NBA97_MUSIC_TRANSITION_FINISHED) {
            f.input.pause = 1; f.state.resource_handle = 71;
        }
        if (f.mutate && call == NBA97_MUSIC_TRANSITION_RELEASE) {
            f.state.saved_volume = 0x12345678;
        }
        return call == NBA97_MUSIC_TRANSITION_FINISHED ? f.finished : 0;
    }
    void begin(std::uint32_t resource) {
        need(nba97_music_transition_begin(&state,&route,&input,resource,call,this)==1,"begin refused valid state");
    }
};
void lifecycle() {
    Fixture f; f.route.voice=42; f.route.phase=3;
    f.input.volume=9; f.state.other_volume=8; f.state.resource_handle=70;
    f.begin(0x24);
    need(f.events.size()==3 && f.events[0].call==NBA97_MUSIC_TRANSITION_FINISHED &&
        f.events[1].call==NBA97_MUSIC_TRANSITION_FADE && f.events[1].a==42 &&
        f.events[1].b==50 && f.events[1].c==UINT32_MAX &&
        f.events[2].call==NBA97_MUSIC_TRANSITION_RELEASE && f.events[2].a==70,"source transition callback order/arguments");
    need(f.route.phase==4 && f.input.pause==1 && f.input.selection_blocked==1 &&
        f.input.volume==4 && f.state.saved_volume==9 && !f.state.resource_handle,"View Player begin state");
    nba97_music_transition_end(&f.input,0x24);
    need(f.input.selection_blocked==1,"View Player end unblocked selection without dispatch evidence");
    f.input.selection_blocked=0; nba97_music_transition_end(&f.input,0x24);
    need(f.input.selection_blocked==0,"View Player end overwrote dispatch effect");
    f.input.volume=1; f.finished=1; f.route.phase=1; f.events.clear(); f.begin(7);
    need(f.route.phase==1 && f.input.volume==9 && !f.input.pause && f.input.selection_blocked==1,
        "exit restore/finished state");
    need(f.events.size()==2 && f.events[1].a==0,"release-zero call was removed");
    nba97_music_transition_end(&f.input,7);
    need(!f.input.selection_blocked,"ordinary resource end failed to unblock");
}
void quirks() {
    Fixture f; f.input.volume=9; f.state.other_volume=0; f.finished=1;
    f.begin(0x24); need(f.input.volume==9 && f.input.selection_blocked==1,"zero adjacent volume incorrectly reduced music");
    f.state.other_volume=3; f.begin(0x24);
    need(f.input.volume==2 && f.state.saved_volume==9,"first reduced entry");
    f.begin(0x24); need(f.state.saved_volume==2,"repeated-entry saved-volume bug silently repaired");
    f.begin(7); need(f.input.volume==2,"repeated-entry exit restore");
    f.input.pause=0; f.state.transition_guard=1; f.events.clear(); f.begin(0x1f);
    need(f.events.empty() && !f.input.selection_blocked,"guarded1F transition should skip fade");
    f.state.transition_guard=0; f.mutate=true; f.events.clear(); f.begin(0x1f);
    need(f.events.size()==2 && f.events[1].a==71 && f.input.volume==0x78,
        "callback mutation was snapshotted instead of re-read");
    need(nba97_music_transition_begin(nullptr,&f.route,&f.input,0,f.call,&f)==0 &&
        nba97_music_transition_end(nullptr,0)==0,"null refusal");
}
}
int main() {
    try { lifecycle(); quirks(); std::cout<<"music resource transition tests passed\n"; }
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
    return 0;
}
