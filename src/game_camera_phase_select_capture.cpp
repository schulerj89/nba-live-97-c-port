#include "game_camera_phase_select_capture.h"
#include "game_camera_phase_select_adapter.h"
#include "game_camera_elapsed_dispatch_capture.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 { namespace {
struct Fixture {
  std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x200000);
  std::vector<std::uint8_t> known=std::vector<std::uint8_t>(bytes.size(),1);
  Nba97GameTextRegion region{0x80000000u,bytes.data(),known.data(),bytes.size()};
  Nba97GameCameraSelectContext parent{};
  Nba97GameCameraSelectProgress parent_progress{};
  Nba97GameCameraPhaseSelectBinding binding{};
  GameCameraElapsedDispatchCapture elapsed;
  std::vector<std::uint32_t> camera_pcs,phase_pcs,phase_args;
  void put(std::uint32_t a,std::uint32_t v,unsigned width=4){for(unsigned i=0;i<width;++i)bytes.at(a-region.base+i)=std::uint8_t(v>>(8*i));}
  std::uint32_t get(std::uint32_t a,unsigned width=4)const{std::uint32_t v=0;for(unsigned i=0;i<width;++i)v|=std::uint32_t(bytes.at(a-region.base+i))<<(8*i);return v;}
  static int camera(void*u,const Nba97GameTextMemory* memory,const Nba97GameCameraSelectEvent* e,Nba97GameCameraSelectRegisters* r){
    auto&f=*static_cast<Fixture*>(u);f.camera_pcs.push_back(e->pc);
    if(e->entry==0x800798b4)return f.elapsed.dispatch(memory,e,r);
    // Explicit camera resource/service response, without drawing a frame.
    r->gpr[2]={0,15};return 1;
  }
  static int phase(void*u,const Nba97GameTextMemory*,const Nba97GameCameraPhaseSelectEvent* e,Nba97GameCameraPhaseSelectMachine* m){
    auto&f=*static_cast<Fixture*>(u);f.phase_pcs.push_back(e->pc);
    const auto expected=f.phase_pcs.size()==1?15u:8u;
    if(e->entry!=0x80079ebcu||m->registers.gpr[4].word!=expected||m->registers.gpr[4].known_mask!=15)return 0;
    f.phase_args.push_back(m->registers.gpr[4].word);
    // The adjustment dependency is explicit; no camera interpolation is claimed.
    return 1;
  }
};
std::string captureCase(unsigned busy){
  Fixture f;
  f.parent.memory={&f.region,1};f.parent.operation_budget=1000;f.parent.io=Fixture::camera;f.parent.user=&f;
  for(auto&r:f.parent.registers.gpr)r={0,15};
  f.parent.registers.gpr[29]={0x801ff000u,15};f.parent.registers.gpr[31]={0x81234568u,15};
  // Independent synthetic natural-caller contract: this is a mode-zero probe,
  // not the frontend's current mode-12 startup or a live frame-pump bridge.
  f.put(0x800fc99cu,busy);f.put(0x800fdb90u,0x81,2);f.put(0x800bc940u,1);f.put(0x800bc944u,1);
  f.put(0x800bc1f8,10);f.put(0x800bc1fc,100);f.put(0x800bc200,1);f.put(0x800bc1f4,0xffffffff);
  nba97_game_camera_phase_select_binding_init(&f.binding,200,500);f.binding.io=Fixture::phase;f.binding.user=&f;
  const int result=nba97_game_camera_select_with_phase_select(&f.parent,&f.binding,&f.parent_progress);
  const auto&p=f.binding.progress;
  if(result!=NBA97_TEXT_COMPLETE||!p.completed||!f.parent_progress.completed||f.binding.invocations!=1||f.binding.completions!=1||f.binding.event.pc!=0x80079a0cu||f.binding.event.delay_slot_pc!=0x80079a10u||f.get(0x800bc940u)!=1||f.get(0x800bc944u)!=1||f.get(0x800fc99cu)!=0||p.machine.hi.known_mask||p.machine.lo.known_mask)throw std::runtime_error("camera phase native composition drifted");
  if(bool(p.phase_changed)!=bool(busy)||f.binding.camera_invocations!=(busy?1u:0u)||f.phase_pcs!=(busy?std::vector<std::uint32_t>{0x8007e3a8,0x8007e3b0,0x8007e3b8}:std::vector<std::uint32_t>{}))throw std::runtime_error("camera phase change sequence drifted");
  std::ostringstream o;o<<"{\"busy_before\":"<<busy<<",\"phase_before\":1,\"phase_after\":1,\"published_phase\":"<<f.get(0x800bc944u)<<",\"busy_after\":0,\"completed\":true,\"same_parent_memory\":true,\"call_pc\":"<<f.binding.event.pc<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"callbacks\":"<<p.callbacks_completed<<",\"phase_changed\":"<<unsigned(p.phase_changed)<<",\"nested_camera_calls\":"<<f.binding.camera_invocations<<",\"return_address\":"<<p.machine.registers.gpr[31].word<<",\"sp\":"<<p.machine.registers.gpr[29].word<<",\"hilo_known_masks\":[0,0],\"adjustment_pcs\":[";
  for(std::size_t i=0;i<f.phase_pcs.size();++i){if(i)o<<',';o<<f.phase_pcs[i];}o<<"],\"adjustment_args\":[";
  for(std::size_t i=0;i<f.phase_args.size();++i){if(i)o<<',';o<<f.phase_args[i];}o<<"],\"camera_child_pcs\":[";
  for(std::size_t i=0;i<f.camera_pcs.size();++i){if(i)o<<',';o<<f.camera_pcs[i];}o<<"],\"elapsed_dispatch\":"<<(f.elapsed.receipt.empty()?"null":f.elapsed.receipt)<<"}";return o.str();
}
}
std::string captureGameCameraPhaseSelect(){
  return "{\"program\":\"GAMEONLY\",\"address\":\"0x8007E26C\",\"inclusive_end\":\"0x8007E463\",\"bytes\":504,\"instructions\":126,\"classification\":\"no direct visual effect\",\"scope\":\"independent synthetic mode-zero natural camera caller; actual phase owner and nested camera selector; typed adjustment/resources; no live frame-pump bridge or advancing match\",\"cases\":["+captureCase(0)+","+captureCase(7)+"]}";
}
}
