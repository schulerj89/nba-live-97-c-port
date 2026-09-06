#include "game_match_buffer_compress_capture.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
namespace nba97 { namespace {
std::uint32_t word(const Nba97GameTextMemory& memory,std::uint32_t address,unsigned width) {
 for(std::size_t i=0;i<memory.count;++i){const auto&r=memory.region[i];const auto offset=std::uint64_t(address)-r.base;
  if(address<r.base||offset>r.size||width>r.size-offset)continue;
  std::uint32_t value=0;for(unsigned b=0;b<width;++b){if(r.known&&r.known[offset+b]!=1)throw std::runtime_error("buffer-compress capture unknown byte");value|=std::uint32_t(r.data[offset+b])<<(8*b);}return value;
 }throw std::runtime_error("buffer-compress capture unmapped access");
}
}
bool GameMatchBufferCompressCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97GameMatchBufferRecordEvent* event,Nba97GameMatchBufferRecordMachine* machine) {
 if(!memory||!event||!machine||!receipt.empty())return false;
 const auto current=machine->registers.gpr[4].word,previous=machine->registers.gpr[5].word,output=machine->registers.gpr[6].word,count=machine->registers.gpr[7].word,toggle=word(*memory,0x800f9ffcu,2);
 Nba97GameMatchBufferCompressBinding binding{};binding.operation_budget=2048;
 if(nba97_game_match_buffer_compress_from_record(&binding,memory,event,machine)!=1)return false;
 const auto&p=binding.progress;const auto end=p.machine.registers.gpr[2].word,length=end-output;
 // Independent diagnostic decode verifies the generated packet against both
 // retained snapshots; these are runtime fixtures, not recorded gameplay.
 auto cursor=output+((count+7u)>>2);bool verified=true;
 for(unsigned i=0;i<count;++i){const auto code=(word(*memory,output+1+i/4,1)>>(6-2*(i%4)))&3u;std::uint32_t delta=0;
  if(code==1){delta=word(*memory,cursor++,1);if(delta&128u)delta|=0xffffff00u;}
  else if(code==3){delta=word(*memory,cursor,1)|(word(*memory,cursor+1,1)<<8);cursor+=2;}
  else if(code==2)verified=false;
  verified=verified&&((word(*memory,previous+2*i,2)+delta)&65535u)==word(*memory,current+2*i,2);
 }
 verified=verified&&cursor+1==end&&word(*memory,output,1)==(length&255u)&&word(*memory,end-1,1)==(length&255u)&&word(*memory,0x800f9ffcu,2)==(toggle^1u);
 if(!p.completed||!verified||count!=130)throw std::runtime_error("buffer-compress packet verification failed");
 std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x800767FC\",\"inclusive_end\":\"0x800768EF\",\"bytes\":244,\"instructions\":61,\"classification\":\"no direct visual effect\",\"scope\":\"actual frame recorder and compressor on same synthetic retained memory; independent packet decode, no advancing match\",\"completed\":true,\"same_parent_memory\":true,\"call_pc\":"<<event->pc<<",\"current_snapshot\":"<<current<<",\"previous_snapshot\":"<<previous<<",\"output\":"<<output<<",\"count\":"<<count<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"elements\":"<<p.element_iterations<<",\"full_flag_groups\":"<<p.completed_flag_groups<<",\"encoded_length\":"<<length<<",\"packet_decoded\":true,\"toggle_before\":"<<toggle<<",\"toggle_after\":"<<(toggle^1u)<<",\"returned_pointer\":"<<end<<",\"return_address\":"<<p.machine.registers.gpr[31].word<<",\"sp\":"<<p.machine.registers.gpr[29].word<<",\"hilo_known_masks\":["<<unsigned(p.machine.hi.known_mask)<<','<<unsigned(p.machine.lo.known_mask)<<"]}";
 receipt=o.str();return true;
}
}
