#include <snes.hpp>

#define SMP_CPP
namespace SNES {

#if defined(DEBUGGER)
  #include "debugger/debugger.cpp"
  SMPDebugger smp;
#else
  SMP smp;
#endif

#include "serialization.cpp"
#include "iplrom.cpp"
#include "memory/memory.cpp"
#include "mmio/mmio.cpp"
#include "timing/timing.cpp"

void SMP::step(unsigned clocks) {
  clock += clocks * (uint64)cpu.frequency;
  dsp.clock -= clocks;
}

void SMP::synchronize_cpu() {
  if(CPU::Threaded == true) {
    if(clock >= 0 && scheduler.sync != Scheduler::SynchronizeMode::All) co_switch(cpu.thread);
  } else {
    while(clock >= 0) cpu.enter();
  }
}

void SMP::synchronize_dsp() {
  if(DSP::Threaded == true) {
    if(dsp.clock < 0 && scheduler.sync != Scheduler::SynchronizeMode::All) co_switch(dsp.thread);
  } else {
    while(dsp.clock < 0) dsp.enter();
  }
}

void SMP::Enter() { smp.enter(); }

void SMP::enter() {
  while(true) {
    if(scheduler.sync == Scheduler::SynchronizeMode::All) {
      scheduler.exit(Scheduler::ExitReason::SynchronizeEvent);
    }

    op_step();
  }
}

void SMP::op_step() {
  (this->*opcode_table[op_readpc()])();
}

void SMP::power() {
  //targets not initialized/changed upon reset
  t0.target = 0;
  t1.target = 0;
  t2.target = 0;

  reset();
}

void SMP::reset() {
  create(Enter, system.apu_frequency());

  regs.pc = 0xffc0;
  regs.a = 0x00;
  regs.x = 0x00;
  regs.y = 0x00;
  regs.sp = 0xef;
  regs.p = 0x02;

  for(unsigned i = 0; i < memory::apuram.size(); i++) {
    memory::apuram.write(i, random(0));
  }

  status.clock_counter = 0;
  status.dsp_counter = 0;
  status.timer_step = 3;

  //$00f0
  status.clock_speed = 0;
  status.timer_speed = 0;
  status.timers_enabled = true;
  status.ram_disabled = false;
  status.ram_writable = true;
  status.timers_disabled = false;

  //$00f1
  status.iplrom_enabled = true;

  //$00f2
  status.dsp_addr = 0x00;

  //$00f4-$00f7
  for(unsigned i = 0; i < 4; i++) {
    port.cpu_to_smp[i] = 0;
    port.smp_to_cpu[i] = 0;
  }

  //$00f8-$00f9
  for(unsigned i = 0; i < 2; i++) {
    port.aux[i] = 0;
  }

  t0.stage0_ticks = 0;
  t1.stage0_ticks = 0;
  t2.stage0_ticks = 0;

  t0.stage1_ticks = 0;
  t1.stage1_ticks = 0;
  t2.stage1_ticks = 0;

  t0.stage2_ticks = 0;
  t1.stage2_ticks = 0;
  t2.stage2_ticks = 0;

  t0.stage3_ticks = 0;
  t1.stage3_ticks = 0;
  t2.stage3_ticks = 0;

  t0.current_line = 0;
  t1.current_line = 0;
  t2.current_line = 0;

  t0.enabled = false;
  t1.enabled = false;
  t2.enabled = false;
}

SMP::SMP() {
  // put this in the ctor instead of reset so that something will still get dumped on reset if it hasn't been (?)
  dump_spc = false;
}

SMP::~SMP() {
}

void SMP::save_spc_dump(string path) {
  dump_spc = true;
  spc_path = path;
}

void SMP::save_spc_dump() {
  dump_spc = false;
	
  file out;
  if (!out.open(spc_path(), file::mode::write)) {
    return;
  }
  
  out.write((const uint8_t*)"SNES-SPC700 Sound File Data v0.30\x1a\x1a\x1a\x1e", 0x25);
  out.writel(regs.pc, 2);
  out.write(regs.a);
  out.write(regs.x);
  out.write(regs.y);
  out.write(regs.p);
  out.write(regs.sp);
  out.write(0);
  out.write(0);
  
  // just write completely blank ID666 tag
  for (unsigned i = 0x2e; i < 0x100; i++) out.write(0);
  
  // 0000 - 00EF
  out.write(memory::apuram.data(), 0xF0);
  
  // 00F0 - 00FF
  out.write(memory::apuram[0xF0]);
  out.write(memory::apuram[0xF1]);
  out.write(status.dsp_addr);
  out.write(dsp.read(status.dsp_addr & 0x7f));
  
  out.write(port.cpu_to_smp[0]);
  out.write(port.cpu_to_smp[1]);
  out.write(port.cpu_to_smp[2]);
  out.write(port.cpu_to_smp[3]);
  
  out.write(port.aux[0]);
  out.write(port.aux[1]);
  
  out.write(memory::apuram[0xFA]);
  out.write(memory::apuram[0xFB]);
  out.write(memory::apuram[0xFC]);
  
  out.write(t0.stage3_ticks & 15);
  out.write(t1.stage3_ticks & 15);
  out.write(t2.stage3_ticks & 15);
  
  // 0100 - FFFF
  out.write(memory::apuram.data() + 0x100, 0xFF00);
  
  for (unsigned i = 0; i < 128; i++) {
    out.write(dsp.read(i));
  }
  
  for (unsigned i = 0; i < 128; i++) {
    out.write(0);
  }
  
  out.close();
}

}
