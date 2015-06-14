#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arm.h"
#include "newton.h"
#include "runt.h"
#include "lcd_sharp.h"
#include "lcd_squirt.h"
#include "HammerConfigBits.h"

#if 1
# define dbug(...) if (c->memTrace) printf(__VA_ARGS__)
#else
# define dbug(...) {}
#endif



uint32_t newton_get_mem32 (newton_t *c, uint32_t addr) {
  if (c->memwatchTail > 0) {
    for (int i=0; i<c->memwatchTail; i++) {
      if (addr == c->memwatch[i]) {
        fprintf(c->logFile, "\n\nAddress 0x%08x read from PC 0x%08x\n", addr, arm_get_pc(c->arm));
        newton_stop(c);
      }
    }
  }
  if (addr % 4 != 0) {
    printf("misaligned read access: 0x%08x, PC: 0x%08x\n", addr, arm_get_pc(c->arm));
  }
  
  uint32_t result = 0;
  if (addr < 0x00800000) {      // rom, 8MB
    if (c->machineType == kGestalt_MachineType_MessagePad) {
      // ROM is mirrored after 4MB
      addr = addr % 0x400000;
    }
    
    result = c->rom[addr / 4];

    switch (addr) {
      case 0x000013f4:
        fprintf(c->logFile, "Read access: gDebuggerBits, PC=0x%08x\n", arm_get_pc(c->arm));
        result = 1;
        break;
      case 0x000013f8:
        fprintf(c->logFile, "Read access: gNewtTests, PC=0x%08x\n", arm_get_pc(c->arm));
        break;
      case 0x000013fc:
        fprintf(c->logFile, "Read access: gNewtConfig, PC=0x%08x\n", arm_get_pc(c->arm));
        result = kConfigBit3 | kDontPauseCPU | kHeapChecking | kStopOnThrows | kEnableStdout | kDefaultStdioOn | kEnableListener;
        break;
    }
    if (addr >= c->romSize) {
      printf("reading from mirrored rom\n");
    }
  }
  else if (addr >= 0x00800000 && addr < 0x01000000) {
    if (c->machineType == kGestalt_MachineType_MessagePad) {
      // Noticed on the Notepad
      result = 0xe1a01006;
    }
    else {
      // mirror the ROM?
      result = c->rom[(addr - 0x00800000) / 4];
      
    }
  }
  else if (addr >= 0x01000000 && addr < 0x01400000) {
    uint32_t *bank = NULL;
    if (c->machineType == kGestalt_MachineType_MessagePad) {
      if (addr >= 0x01000000 && addr < 0x01200000) {
		  // Mirror the RAM 
        bank = c->ram1;
      }
      else {
        // Noticed on the notepad
        result = 0xe1a010ff;
      }
    }
    else {
      if (addr >= 0x01000000 && addr < 0x01100000) {
        bank = c->ram1;
      }
      else if (addr >= 0x01100000 && addr < 0x01200000) {
        bank = c->ram2;
      }
      else if (addr >= 0x01200000 && addr < 0x01300000) {
        bank = c->ram3;
      }
    }
    
    if (bank != NULL) {
      result = bank[(addr & 0x000fffff) / 4];
    }
  }
  else if (addr >= 0x01400000 && addr < 0x01800000) { // runt, 4MB
    result = runt_get_mem32(c->runt, addr);
  }
  else if (addr >= 0x10000000 && addr < 0x20000000) { // "I/O card"
    if (addr >= 0x14000000 && addr < 0x14100000 ) {
      result = c->flash[(addr - 0x14000000) / 4];
    }
    fprintf(c->logFile, "[FLASH:READ] 0x%08x => 0x%08x, PC=0x%08x\n", addr, result, arm_get_pc(c->arm));
    // return iocard_get_mem32(c, addr);
  }
  else if (addr >= 0x70000000 && addr < 0x80000000) { // card control registers
    fprintf(c->logFile, "[CARD:READ] 0x%08x, PC=0x%08x\n", addr, arm_get_pc(c->arm));
    if (addr == 0x70007C00) {
      // 0x0a with no card & eject
      // 0x0a with dummy card
      // 0x1e with 2MB flash card
      // 0x1a with 1MB SRAM card
      result = 0x0000000a;
    }
  }
  else {
    fprintf(c->logFile, "UNKNOWN MEMORY READ: 0x%08x, PC=0x%08x\n", addr, arm_get_pc(c->arm));
	if (c->breakOnUnknownMemory) {
		newton_stop(c);
	}
  }
  
  dbug("GET: 0x%08x => 0x%08x, PC=0x%08x\n", addr, result, arm_get_pc(c->arm));
  
  return result;
}

uint8_t newton_get_mem8 (newton_t *c, uint32_t addr) {
  int bytenum = addr & 3;
  uint32_t word = newton_get_mem32(c, addr - bytenum);
  
  word >>= ((3-bytenum) * 8);
  uint8_t result = (word & 0xff);
  return result;
}

uint16_t newton_get_mem16 (newton_t *c, uint32_t addr) {
#if 1
  abort();
#else
  uint8_t low = newton_get_mem8(c, addr);
  uint8_t high = newton_get_mem8(c, addr + 1);
  return (high << 16) | low;
#endif
}

uint8_t newton_set_mem8 (newton_t *c, uint32_t addr, uint8_t val) {
  static const unsigned masktab[] = {
    0x00ffffff, 0xff00ffff, 0xffff00ff, 0xffffff00
  };
  
  uint32_t aligned = addr - (addr & 3);
  uint32_t mask = masktab[addr & 3];
  uint32_t word = newton_get_mem32(c, aligned);
  
  uint32_t newval = word & mask;
  newval |= (val << ((3 - (addr & 3)) * 8));
  
  newton_set_mem32(c, aligned, newval);
  
  return val;
}

uint16_t newton_set_mem16 (newton_t *c, uint32_t addr, uint16_t val) {
#if 1
  abort();
#else
  newton_set_mem8(c, addr, val & 0xff);
  newton_set_mem8(c, addr + 1, (val >> 16) & 0xff);
  return val;
#endif
}

uint32_t newton_set_mem32 (newton_t *c, uint32_t addr, uint32_t val) {
  dbug("SET: 0x%08x => 0x%08x, PC=0x%08x\n", addr, val, arm_get_pc(c->arm));
  if (c->memwatchTail > 0) {
    for (int i=0; i<c->memwatchTail; i++) {
      if (addr == c->memwatch[i]) {
        fprintf(c->logFile, "\n\nAddress 0x%08x changed to 0x%08x from PC 0x%08x\n", addr, val, arm_get_pc(c->arm));
        newton_stop(c);
      }
    }
  }
  if (addr < 0x00800000) {      // rom, 8MB
    fprintf(c->logFile, "bad write to ROM!!!! addr=0x%08x, val=0x%08x\n", addr, val);
    fprintf(c->logFile, "pc: 0x%08x\n", arm_get_pc(c->arm));
    
    //arm_exception_data_abort(c);
    //return 0;
    // return rom_set_mem32(c, addr, val);
  }
  
  
  else if (addr >= 0x01000000 && addr < 0x01400000) { // ram, 1MB
    uint32_t *bank = NULL;
    if (c->machineType == kGestalt_MachineType_MessagePad) {
      if (addr >= 0x01000000 && addr < 0x01200000) { // ram, 1MB
        bank = c->ram1;
      }
    }
    else {
      if (addr >= 0x01000000 && addr < 0x01100000) {
        bank = c->ram1;
      }
      else if (addr >= 0x01100000 && addr < 0x01200000) {
        bank = c->ram2;
      }
      else if (addr >= 0x01200000 && addr < 0x01300000) {
        bank = c->ram3;
      }
    }
    
    if (bank != NULL) {
      bank[(addr & 0x000fffff) / 4] = val;
    }
    else {
      newton_stop(c);
    }
  }
  else if (addr >= 0x01400000 && addr < 0x01800000) { // runt, 4MB
    return runt_set_mem32(c->runt, addr, val);
  }
  else if (addr >= 0x10000000 && addr < 0x20000000) { // "I/O card"
    // return iocard_set_mem32(c, addr, val);
    fprintf(c->logFile, "[FLASH:WRITE] 0x%08x => 0x%08x, PC=0x%08x\n", addr, val, arm_get_pc(c->arm));
    if (addr >= 0x14000000 && addr < 0x14100000 ) {
      c->flash[(addr - 0x14000000) / 4] = val;
    }
  }
  else if (addr >= 0x70000000 && addr < 0x80000000) { // card control registers
    fprintf(c->logFile, "[CARD:WRITE] 0x%08x => 0x%08x, PC=0x%08x\n", addr, val, arm_get_pc(c->arm));
    // return cardcont_set_mem32(c, addr, val);
  }
  else {
    fprintf(c->logFile, "UNKNOWN MEMORY SET: 0x%08x => 0x%08x, PC=0x%08x\n", addr, val, arm_get_pc(c->arm));
	if (c->breakOnUnknownMemory) {
        newton_stop(c);
	}
  }
  return val;
}

#pragma mark -
#pragma mark Disassembly helpers
static const char *arm_modes[32] = {
  "0x00", "0x01", "0x02", "0x03",
  "0x04", "0x05", "0x06", "0x07",
  "0x08", "0x09", "0x0a", "0x0b",
  "0x0c", "0x0d", "0x0e", "0x0f",
  "usr",  "fiq",  "irq",  "svc",
  "0x14", "0x15", "0x16", "abt",
  "0x18", "0x19", "und",  "0x1b",
  "0x1c", "0x1d", "0x1e", "sys"
};

void arm_dasm_str (char *dst, arm_dasm_t *op)
{
  unsigned i, j;
  
  if (op->argn == 0) {
    sprintf (dst, "%08lX  %s", (unsigned long) op->ir, op->op);
  }
  else {
    j = sprintf (dst, "%08lX  %-8s %s", (unsigned long) op->ir, op->op, op->arg[0]);
    
    for (i = 1; i < op->argn; i++) {
      j += sprintf (dst + j, ", %s", op->arg[i]);
    }
  }
}

void newton_print_state(newton_t *newt) {
  arm_t *c = newt->arm;
  unsigned long long opcnt, clkcnt;
  unsigned long      delay;
  arm_dasm_t         op;
  char               str[256];
  bool memTrace = newt->memTrace;
  newt->memTrace = false;
  
  //	pce_prt_sep ("ARM");
  
  opcnt = arm_get_opcnt (c);
  clkcnt = arm_get_clkcnt (c);
  delay = arm_get_delay (c);
  
  fprintf(newt->logFile, "CLK=%llx  OP=%llx  DLY=%lx  CPI=%.4f\n",
          clkcnt, opcnt, delay,
          (opcnt > 0) ? ((double) (clkcnt + delay) / (double) opcnt) : 1.0
          );
  
  fprintf(newt->logFile, "r00=%08lX  r04=%08lX  r08=%08lX  r12=%08lX  CPSR=%08lX\n",
          (unsigned long) arm_get_gpr (c, 0),
          (unsigned long) arm_get_gpr (c, 4),
          (unsigned long) arm_get_gpr (c, 8),
          (unsigned long) arm_get_gpr (c, 12),
          (unsigned long) arm_get_cpsr (c)
          );
  
  fprintf(newt->logFile, "r01=%08lX  r05=%08lX  r09=%08lX   SP=%08lX  SPSR=%08lX\n",
          (unsigned long) arm_get_gpr (c, 1),
          (unsigned long) arm_get_gpr (c, 5),
          (unsigned long) arm_get_gpr (c, 9),
          (unsigned long) arm_get_gpr (c, 13),
          (unsigned long) arm_get_spsr (c)
          );
  
  fprintf(newt->logFile, "r02=%08lX  r06=%08lX  r10=%08lX   LR=%08lX    CC=[%c%c%c%c]\n",
          (unsigned long) arm_get_gpr (c, 2),
          (unsigned long) arm_get_gpr (c, 6),
          (unsigned long) arm_get_gpr (c, 10),
          (unsigned long) arm_get_gpr (c, 14),
          (arm_get_cc_n (c)) ? 'N' : '-',
          (arm_get_cc_z (c)) ? 'Z' : '-',
          (arm_get_cc_c (c)) ? 'C' : '-',
          (arm_get_cc_v (c)) ? 'V' : '-'
          );
  
  fprintf(newt->logFile, "r03=%08lX  r07=%08lX  r11=%08lX   PC=%08lX     M=%02X (%s)\n",
          (unsigned long) arm_get_gpr (c, 3),
          (unsigned long) arm_get_gpr (c, 7),
          (unsigned long) arm_get_gpr (c, 11),
          (unsigned long) arm_get_gpr (c, 15),
          (unsigned) arm_get_cpsr_m (c),
          arm_modes[arm_get_cpsr_m (c) & 0x1f]
          );
  
  fprintf(newt->logFile, "---------------------------------------------------------------------\n");
  uint32_t pc = arm_get_pc (c);
  for (uint32_t i=pc; i<pc+12; i+=4) {
    arm_dasm_mem (c, &op, i, ARM_XLAT_CPU);
    arm_dasm_str (str, &op);
    
    fprintf(newt->logFile, "%08lX  %s\n", (unsigned long) i, str);
  }
  fprintf(newt->logFile, "---------------------------------------------------------------------\n");
  fprintf(newt->logFile, "\n\n\n");
  
  newt->memTrace = memTrace;
}

#pragma mark -
#pragma mark
void newton_breakpoint_add(newton_t *c, uint32_t breakpoint) {
  if (c->breakpointsTail == c->breakpointsCapacity) {
    if (c->breakpointsCapacity == 0) {
      c->breakpointsCapacity = 128;
      c->breakpoints = calloc(sizeof(uint32_t), c->breakpointsCapacity);
    }
    else {
      c->breakpointsCapacity += 128;
      c->breakpoints = realloc(c->breakpoints, c->breakpointsCapacity);
    }
  }
  
  c->breakpoints[c->breakpointsTail++] = breakpoint;
}

void newton_breakpoint_del(newton_t *c, uint32_t breakpoint) {
  for (int i=0; i<c->breakpointsTail; i++) {
    if (c->breakpoints[i] == breakpoint) {
      for (int j=i+1; j<c->breakpointsTail; j++) {
        c->breakpoints[j-1] = c->breakpoints[j];
      }
      c->breakpointsTail--;
      break;
    }
  }
}

void newton_memwatch_add(newton_t *c, uint32_t memwatch) {
  if (c->memwatchTail == c->memwatchCapacity) {
    if (c->memwatchCapacity == 0) {
      c->memwatchCapacity = 128;
      c->memwatch = calloc(sizeof(uint32_t), c->memwatchCapacity);
    }
    else {
      c->memwatchCapacity += 128;
      c->memwatch = realloc(c->memwatch, c->memwatchCapacity);
    }
  }
  
  c->memwatch[c->memwatchTail++] = memwatch;
}

void newton_memwatch_del(newton_t *c, uint32_t memwatch) {
  for (int i=0; i<c->memwatchTail; i++) {
    if (c->memwatch[i] == memwatch) {
      for (int j=i+1; j<c->memwatchTail; j++) {
        c->memwatch[j-1] = c->memwatch[j];
      }
      c->memwatchTail--;
      break;
    }
  }
}

void newton_set_break_on_unknown_memory(newton_t *c, bool breakOnUnknownMemory) {
	c->breakOnUnknownMemory = breakOnUnknownMemory;
}

bool newton_get_break_on_unknown_memory(newton_t *c) {
	return c->breakOnUnknownMemory;
}

void newton_set_instruction_trace(newton_t *c, bool instructionTrace) {
  c->instructionTrace = instructionTrace;
}

bool newton_get_instruction_trace(newton_t *c) {
  return c->instructionTrace;
}

void newton_set_pc_spy(newton_t *c, bool pcSpy) {
  c->pcSpy = pcSpy;
}

bool newton_get_pc_spy(newton_t *c) {
  return c->pcSpy;
}

void newton_set_sp_spy(newton_t *c, bool spSpy) {
  c->spSpy = spSpy;
}

bool newton_get_sp_spy(newton_t *c) {
  return c->spSpy;
}

void newton_set_logfile(newton_t *c, FILE *file) {
  c->logFile = file;
  runt_set_log_file(c->runt, file);
}

int newton_log_opcode (void *ext, uint32_t ir) {
  newton_t *c = (newton_t *)ext;
  fprintf(c->logFile, "%s %i\n", __PRETTY_FUNCTION__, ir);
  newton_stop((newton_t *)ext);
  return 0;
}

void newton_log_undef (void *ext, uint32_t ir) {
  newton_t *c = (newton_t *)ext;
  fprintf(c->logFile, "%s instr=0x%08x, PC=0x%08x: ", __PRETTY_FUNCTION__, ir, arm_get_pc(c->arm));
  
  if (ir == 0xE6000010) {
    fprintf(c->logFile, "SystemBoot");
    c->arm->reg[15] = c->arm->reg[14] + 4;
  }
  else if (ir == 0xE6000110) {
    fprintf(c->logFile, "ExitToShell");
    newton_stop((newton_t *)ext);
  }
  else if (ir == 0xE6000210) {
    fprintf(c->logFile, "Debugger");
  }
  else if (ir == 0xE6000310) {
    fprintf(c->logFile, "DebugStr");
  }
  else if (ir == 0xE6000410) {
    fprintf(c->logFile, "PublicFiller");
  }
  else if (ir == 0xE6000510) {
    char *msg = calloc(1024, 1);
    
    uint32_t address = arm_get_pc(c->arm) + 4;
    uint32_t value;
    int index = 0;
    while (1) {
      value = newton_get_mem32(c, address);
      msg[index+0] = (value >> 24) & 0xff;
      msg[index+1] = (value >> 16) & 0xff;
      msg[index+2] = (value >>  8) & 0xff;
      msg[index+3] = (value      ) & 0xff;
      if (msg[index+3] == 0x00 || msg[index+2] == 0x00 || msg[index+1] == 0x00 || msg[index+0] == 0x00) {
        break;
      }
      index += 4;
      address += 4;
    }
    
    fprintf(c->logFile, "SystemPanic: %s", msg);
    newton_stop((newton_t *)ext);
  }
  else if (ir == 0xE6000710) {
    fprintf(c->logFile, "SendTestResults");
  }
  else if (ir == 0xE6000810) {
    enum {
      do_sys_open = 0x10,
      do_sys_close = 0x11,
      do_sys_istty = 0x12,
      do_sys_read = 0x13,
      do_sys_write = 0x14,
      do_sys_set_input_notify = 0x15,
    };

    fprintf(c->logFile, "TapFileCntl: ");

    static int lastFp = 1;
    switch (c->arm->reg[0]) {
      case do_sys_open: { // 0x0018c0cc
        bool memTrace = c->memTrace;
        c->memTrace = false;
        
        uint32_t arg1 = 0;
        arm_get_mem32(c->arm, c->arm->reg[1], 0, &arg1);
        
        int index = 0;
        char name[255];
        char letter;
        while((letter = newton_get_mem8(c, arg1)) != 0x00) {
          name[index++] = letter;
          arg1++;
        }
        name[index] = 0;
        
        uint32_t arg2 = 0;
        arm_get_mem32(c->arm, c->arm->reg[1] + 4, 0, &arg2);
        
        fprintf(c->logFile, "Open: name='%s', mode=", name);
        switch(arg2) {
          case 4:
            fprintf(c->logFile, "write");
            break;
          case 0:
            fprintf(c->logFile, "read");
            break;
          default:
            fprintf(c->logFile, "unknown %i", arg2);
            break;
        }
        fprintf(c->logFile, ", fp=%i", lastFp);
        c->memTrace = memTrace;
        c->arm->reg[0] = lastFp++; // fp
        c->arm->reg[7] = 8;
        break;
      }
      case do_sys_close: // 0x0018c120
        fprintf(c->logFile, "close");
        c->arm->reg[0] = 0x01;
        break;
      case do_sys_istty: { // 0x0018c170
        uint32_t arg1 = 0;
        arm_get_mem32(c->arm, c->arm->reg[1], 0, &arg1);
        fprintf(c->logFile, "istty, fp=%i", arg1);
        c->arm->reg[0] = 0x01; // 1, yes, is tty.
      }
        break;
      case do_sys_read: // 0x0018c1a0
        fprintf(c->logFile, "read");
        c->arm->reg[0] = 0x01;
        break;
      case do_sys_write: // 0x0018c2b4
        fprintf(c->logFile, "write");
        c->arm->reg[0] = 0x01;
        break;
      case do_sys_set_input_notify: // 0x0018c06c
        fprintf(c->logFile, "set_input_notify");
        c->arm->reg[0] = 0x01;
        break;
    }
    c->arm->reg[15] = c->arm->reg[14];
  }
  else {
    fprintf(c->logFile, "UNKNOWN!");
  }
  fprintf(c->logFile, "\n");
  
  //newton_stop((newton_t *)ext);
}

void newton_log_exception (void *ext, uint32_t addr) {
  if (addr == 0x08) {
    return;
  }
  newton_t *c = (newton_t *)ext;
  fprintf(c->logFile, "%s PC=0x%08x: ", __PRETTY_FUNCTION__, arm_get_pc(c->arm));
  switch(addr) {
    case 0x00:
      fprintf(c->logFile, "reset_handler");
      break;
    case 0x04:
      fprintf(c->logFile, "undefined_handler");
      break;
    case 0x08:
      fprintf(c->logFile, "swi_handler 0x%06x", newton_get_mem32(c, arm_get_pc(c->arm)) & 0x00ffffff);
      break;
    case 0x0c:
      fprintf(c->logFile, "prefetch_handler");
      break;
    case 0x10:
      fprintf(c->logFile, "data_handler");
      newton_stop((newton_t *)ext);
      break;
    case 0x14:
      fprintf(c->logFile, "unused_handler");
      break;
    case 0x18:
      fprintf(c->logFile, "irq_handler");
      break;
    case 0x1c:
      fprintf(c->logFile, "fiq_handler");
      break;
  }
  fprintf(c->logFile, "\n");
  //  newton_stop((newton_t *)ext);
}


#pragma mark -
#pragma mark
void newton_set_bootmode(newton_t *c, NewtonBootMode bootMode) {
  switch (bootMode) {
    case NewtonBootModeAutoPWB:
      c->runt->interrupt = 0xffffffff;
      c->runt->interruptStick = 2;
      break;
    case NewtonBootModeDiagnostics:
      c->runt->interrupt = 0x00004000;
      c->runt->interruptStick = ((c->machineType == kGestalt_MachineType_Lindy) ? 1 : 0);
      break;
    case NewtonBootModeNormal:
    default:
      c->runt->interrupt = 0;
      c->runt->interruptStick = 0;
      break;
  }
}

void newton_stop(newton_t *c) {
  c->stop = true;
}

void newton_emulate(newton_t *c, int32_t count) {
  int32_t remaining = count;
  c->stop = false;
  
  while (remaining > 0 && c->stop == false) {
    if (c->instructionTrace == true) {
      newton_print_state(c);
    }
    
    arm_execute(c->arm);
    
    if (count != INT32_MAX) {
      remaining--;
    }
    
    uint32_t pc = arm_get_pc (c->arm);
    if (pc != c->lastPc + 4 && c->pcSpy) {
      fprintf(c->logFile, "PC changed to 0x%08x (from 0x%08x)\n", pc, c->lastPc);
    }
    if (c->lastSp != c->arm->reg[13] && c->spSpy) {
      fprintf(c->logFile, "SP changed from 0x%08x to 0x%08x (at PC 0x%08x)\n", c->lastSp, c->arm->reg[13], pc);
      c->lastSp = c->arm->reg[13];
    }
    for (int i=0; i<c->breakpointsTail; i++) {
      if (c->breakpoints[i] == pc) {
        remaining = 0;
        break;
      }
    }
    
    c->lastPc = pc;
    fflush(c->logFile);
  }
}

#pragma mark -
#pragma mark
newton_t *newton_new (void)
{
  newton_t *c;
  
  c = calloc(1, sizeof (newton_t));
  if (c == NULL) {
    return (NULL);
  }
  
  newton_init (c);
  
  return (c);
}

void newton_init (newton_t *c)
{
  //
  // Setup processor
  //
  c->arm = arm_new();
  c->arm->log_ext = c;
  c->arm->log_exception = newton_log_exception;
  c->arm->log_opcode = newton_log_opcode;
  c->arm->log_undef = newton_log_undef;
  arm_set_mem_fct(c->arm, c,
                  newton_get_mem8, newton_get_mem16, newton_get_mem32,
                  newton_set_mem8, newton_set_mem16, newton_set_mem32);
  arm_reset(c->arm);
  
  //
  // Setup RAM
  //
  c->ram1 = calloc(1024 * 1024, 1);
  c->ram2 = calloc(1024 * 1024, 1);
  c->ram3 = calloc(1024 * 1024, 1);
  
  //
  // Setup flash
  //
  c->flash = calloc(0x14100000 - 0x14000000, 1);
  // c->flash[0] = 'SERD';
  // c->flash[0] = 'SEWR';
  
  //
  // Setup RUNT
  //
  c->runt = runt_new();
  runt_set_arm(c->runt, c->arm);
  
  //
  //
  //
  c->lcd_driver = NULL;
  
  //
  // Logging
  //
  newton_set_logfile(c, stdout);
  
  //
  //
  //
  c->lastPc = 0;
  c->lastSp = 0;
  
  //
  // Sanity checking
  //
  newton_set_mem8(c, 0x01000000, 0x00);
  newton_set_mem8(c, 0x01000001, 0x44);
  newton_set_mem8(c, 0x01000002, 0x88);
  newton_set_mem8(c, 0x01000003, 0xcc);
  
  assert(newton_get_mem8(c, 0x01000000) == 0x00);
  assert(newton_get_mem8(c, 0x01000001) == 0x44);
  assert(newton_get_mem8(c, 0x01000002) == 0x88);
  assert(newton_get_mem8(c, 0x01000003) == 0xcc);
  
  newton_set_mem32(c, 0x01000000, 0x00);
}

int newton_load_rom(newton_t *c, const char *path) {
  FILE *romFP = fopen(path, "r");
  if (romFP == NULL) {
    printf("Couldn't open ROM '%s': %s\n", path, strerror(errno));
    return -1;
  }
  
  fseek(romFP, 0l, SEEK_END);
  uint32_t romSize = (uint32_t)ftell(romFP);
  fseek(romFP, 0l, SEEK_SET);
  
  if (c->rom != NULL) {
    free(c->rom);
    c->rom = NULL;
  }
  
  uint32_t romWord = 0;
  if (fread(&romWord, 1, sizeof(romWord), romFP) == sizeof(romWord)) {
    if (htonl(romWord) == 0xE1A00000) {
      fprintf(c->logFile, "ROM file appears to be an AIF image\n");
      romSize -= 128;
      fseek(romFP, 128, SEEK_SET);
    }
    else {
      fseek(romFP, 0, SEEK_SET);
    }
  }
  
  
  c->rom = calloc(romSize, 1);
  c->romSize = romSize;
  uint32_t i=0;
  while (fread(&romWord, 1, sizeof(romWord), romFP) == sizeof(romWord)) {
    c->rom[i++] = (((romWord      ) & 0xff) << 24) |
    (((romWord >>  8) & 0xff) << 16) |
    (((romWord >> 16) & 0xff) << 8 ) |
    (((romWord >> 24) & 0xff));
  }
  fclose(romFP);
  
  printf("Loaded ROM: %s => %i bytes\n", path, c->romSize);
  
  c->machineType = c->rom[0x000013ec / 4];
  printf("Machine Type    : 0x%08x => ", c->machineType);
  switch (c->machineType) {
    case kGestalt_MachineType_MessagePad:
      printf("Junior");
      break;
    case kGestalt_MachineType_Lindy:
      printf("Lindy");
      break;
    case kGestalt_MachineType_Bic:
      printf("Bic");
      break;
    default:
      printf("UNKNOWN - Defaulting to Junior");
      c->machineType = kGestalt_MachineType_MessagePad;
      break;
  }
  printf("\n");

  c->romManufacturer = c->rom[0x000013f0 / 4];
  printf("ROM Manufacturer: 0x%08x => ", c->romManufacturer);
  switch (c->romManufacturer) {
    case kGestalt_Manufacturer_Apple:
      printf("Apple");
      break;
    case kGestalt_Manufacturer_Sharp:
      printf("Sharp");
      break;
    default:
      printf("UNKNOWN - Defaulting to Apple");
      c->romManufacturer = kGestalt_Manufacturer_Apple;
      break;
  }
  printf("\n");
  
  if (c->machineType == kGestalt_MachineType_Lindy) {
    lcd_squirt_t *squirt = lcd_squirt_new();
    runt_set_lcd_fct(c->runt, squirt, lcd_squirt_get_mem32, lcd_squirt_set_mem32, lcd_squirt_get_address_name);
    c->lcd_driver = squirt;
  }
  else {
    lcd_sharp_t *sharp = lcd_sharp_new();
    runt_set_lcd_fct(c->runt, sharp, lcd_sharp_get_mem32, lcd_sharp_set_mem32, lcd_sharp_get_address_name);
    c->lcd_driver = sharp;
  }
  
  return 0;
}

void newton_free (newton_t *c)
{
  if (c->lcd_driver != NULL) {
    if (c->machineType == kGestalt_MachineType_Lindy) {
      lcd_squirt_free((lcd_squirt_t *)c->lcd_driver);
    }
    else {
      lcd_sharp_free((lcd_sharp_t *)c->lcd_driver);
    }
  }

  arm_free(c->arm);
  runt_free(c->runt);
  free(c->ram1);
  free(c->ram2);
  free(c->ram3);
  free(c->rom);
  free(c->flash);
}

void newton_del (newton_t *c)
{
  if (c != NULL) {
    newton_free (c);
    free (c);
  }
}
