//
//  newton.c
//  Leibniz
//
//  Created by Steve White on 9/13/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "arm.h"
#include "fpa.h"
#include "hexdump.h"
#include "newton.h"
#include "runt.h"
#include "pcmcia.h"
#include "HammerConfigBits.h"

#define countof(__a__) (sizeof(__a__) / sizeof(__a__[0]))


#if DISABLE_LOGGING
#define LOG_STR(...) {}
#define SHOULD_LOG(__x__) false
#else
#define LOG_STR(...) fprintf(c->logFile, __VA_ARGS__)
#define SHOULD_LOG(__x__) ((c->logFlags & __x__) == __x__)
#endif


#pragma mark - Debugging helpers
#if !DISABLE_DEBUGGER
void newton_breakpoint_add(newton_t *c, uint32_t address, bp_type type) {
  bp_entry_t *bp = calloc(1, sizeof(bp_entry_t));
  bp->addr = address;
  bp->type = type;
  bp->next = c->breakpoints;
  
  c->breakpoints = bp;
}

void newton_breakpoint_del(newton_t *c, uint32_t address, bp_type type) {
  bp_entry_t *cur = c->breakpoints;
  bp_entry_t *last = NULL;
  
  while (cur != NULL) {
    if (cur->addr != address || cur->type != address) {
      last = cur;
      cur = cur->next;
      continue;
    }
    
    if (last == NULL) {
      c->breakpoints = cur->next;
    }
    else {
      last->next = cur->next;
    }
    free(cur);
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

uint32_t newton_address_for_symbol(newton_t *c, const char *symbol) {
  symbol_t *sym = c->symbols;
  while(sym != NULL) {
    if (strcasecmp(sym->name, symbol) == 0) {
      return sym->address;
    }
  }
  return 0;
}

void newton_add_symbol(newton_t *c, uint32_t address, const char *name) {
  symbol_t *sym = calloc(1, sizeof(symbol_t));
  sym->address = address;
  sym->name = calloc(strlen(name)+1, sizeof(char));
  strcpy(sym->name, name);
  sym->next = c->symbols;
  c->symbols = sym;
}

const char *newton_get_symbol_for_address(newton_t *c, uint32_t addr) {
  symbol_t *sym = c->symbols;
  while(sym != NULL) {
    if (sym->address == addr) {
      return sym->name;
    }
    sym = sym->next;
  }
  
  return NULL;
}

void newton_mem_hexdump(newton_t *c, uint32_t addr, uint32_t length) {
  uint32_t translated = addr;
  arm_translate_extern(c->arm, &translated, 0, NULL, NULL);
  
  char *data = malloc(length);
  for (int i=0; i<length; ) {
    uint32_t word = newton_get_mem32(c, translated + i);
    
    data[i++] = word & 0xff;
    if (i < length) data[i++] = (word >>  8) & 0xff;
    if (i < length) data[i++] = (word >> 16) & 0xff;
    if (i < length) data[i++] = (word >> 24) & 0xff;
  }
  hexdump(c->logFile, data, translated, length);
  free(data);
}

void newton_dump_task(newton_t *c, uint32_t taskAddr) {
  uint32_t name = newton_get_mem32(c, taskAddr + 132);
  const char *pcSymbol = newton_get_symbol_for_address(c, taskAddr);
  
  LOG_STR("Name: %c%c%c%c\n", (name >> 24) & 0xff, (name >> 16) & 0xff, (name >> 8) & 0xff, name & 0xff);
  LOG_STR("Current Task: 0x%08x\n", newton_get_mem32(c, taskAddr + 124));
  LOG_STR("Priority: %i\n", newton_get_mem32(c, taskAddr + 128));
  LOG_STR("Stack Base: 0x%08x\n", newton_get_mem32(c, taskAddr + 136));
  LOG_STR("Fault Address: 0x%08x\n", newton_get_mem32(c, taskAddr + 84));
  LOG_STR("Fault Status: 0x%08x\n", newton_get_mem32(c, taskAddr + 88));
  
  LOG_STR("r00=%08X  r04=%08X  r08=%08X  r12=%08X  \n",
          newton_get_mem32(c, taskAddr + 16),
          newton_get_mem32(c, taskAddr + 32),
          newton_get_mem32(c, taskAddr + 48),
          newton_get_mem32(c, taskAddr + 64)
          );
  
  LOG_STR("r01=%08X  r05=%08X  r09=%08X   SP=%08X  SPSR=%08X\n",
          newton_get_mem32(c, taskAddr + 20),
          newton_get_mem32(c, taskAddr + 36),
          newton_get_mem32(c, taskAddr + 52),
          newton_get_mem32(c, taskAddr + 68),
          newton_get_mem32(c, taskAddr + 80)
          );
  
  LOG_STR("r02=%08X  r06=%08X  r10=%08X   LR=%08X  \n",
          newton_get_mem32(c, taskAddr + 24),
          newton_get_mem32(c, taskAddr + 40),
          newton_get_mem32(c, taskAddr + 56),
          newton_get_mem32(c, taskAddr + 72)
          );
  
  LOG_STR("r03=%08X  r07=%08X  r11=%08X   PC=%08X %s  \n",
          newton_get_mem32(c, taskAddr + 28),
          newton_get_mem32(c, taskAddr + 44),
          newton_get_mem32(c, taskAddr + 60),
          newton_get_mem32(c, taskAddr + 76),
          pcSymbol ? pcSymbol : ""
          );
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
  "0x18", "0x19", "0x1a", "und",
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

void newton_print_state(newton_t *c) {
  arm_t *arm = c->arm;
  unsigned long long opcnt, clkcnt;
  unsigned long    delay;
  arm_dasm_t     op;
  char         str[256];
  bool memTrace = c->memTrace;
  c->memTrace = false;
  
  //	pce_prt_sep ("ARM");
  
  opcnt = arm_get_opcnt (arm);
  clkcnt = arm_get_clkcnt (arm);
  delay = arm_get_delay (arm);
  
  LOG_STR("CLK=%llx  OP=%llx  DLY=%lx  CPI=%.4f\n",
          clkcnt, opcnt, delay,
          (opcnt > 0) ? ((double) (clkcnt + delay) / (double) opcnt) : 1.0
          );
  
  LOG_STR("r00=%08lX  r04=%08lX  r08=%08lX  r12=%08lX  CPSR=%08lX\n",
          (unsigned long) arm_get_gpr (arm, 0),
          (unsigned long) arm_get_gpr (arm, 4),
          (unsigned long) arm_get_gpr (arm, 8),
          (unsigned long) arm_get_gpr (arm, 12),
          (unsigned long) arm_get_cpsr (arm)
          );
  
  LOG_STR("r01=%08lX  r05=%08lX  r09=%08lX   SP=%08lX  SPSR=%08lX\n",
          (unsigned long) arm_get_gpr (arm, 1),
          (unsigned long) arm_get_gpr (arm, 5),
          (unsigned long) arm_get_gpr (arm, 9),
          (unsigned long) arm_get_gpr (arm, 13),
          (unsigned long) arm_get_spsr (arm)
          );
  
  LOG_STR("r02=%08lX  r06=%08lX  r10=%08lX   LR=%08lX  CC=[%c%c%c%c]\n",
          (unsigned long) arm_get_gpr (arm, 2),
          (unsigned long) arm_get_gpr (arm, 6),
          (unsigned long) arm_get_gpr (arm, 10),
          (unsigned long) arm_get_gpr (arm, 14),
          (arm_get_cc_n (arm)) ? 'N' : '-',
          (arm_get_cc_z (arm)) ? 'Z' : '-',
          (arm_get_cc_c (arm)) ? 'C' : '-',
          (arm_get_cc_v (arm)) ? 'V' : '-'
          );
  
  LOG_STR("r03=%08lX  r07=%08lX  r11=%08lX   PC=%08lX   M=%02X (%s)\n",
          (unsigned long) arm_get_gpr (arm, 3),
          (unsigned long) arm_get_gpr (arm, 7),
          (unsigned long) arm_get_gpr (arm, 11),
          (unsigned long) arm_get_gpr (arm, 15),
          (unsigned) arm_get_cpsr_m (arm),
          arm_modes[arm_get_cpsr_m (arm) & 0x1f]
          );
  
  LOG_STR("---------------------------------------------------------------------\n");
  uint32_t pc = arm_get_pc (arm);
  for (uint32_t i=pc; i<pc+12; i+=4) {
    arm_dasm_mem (arm, &op, i, ARM_XLAT_CPU);
    arm_dasm_str (str, &op);
    
    LOG_STR("%08lX  %s\n", (unsigned long) i, str);
  }
  LOG_STR("---------------------------------------------------------------------\n");
  LOG_STR("\n\n\n");
  
  c->memTrace = memTrace;
}

void newton_parse_aif_debug_data(newton_t *c, void *debugData, uint32_t length) {
  LOG_STR("%s debugData=%p, length=%i\n", __PRETTY_FUNCTION__, debugData, length);
  
  uint32_t *bytes = (uint32_t *)debugData;
  bytes += 8; // skip past header
  
  uint32_t numOfEntries = htonl(bytes[0]);
  bytes++;
  
  const char *nameTable = (const char *)(bytes + (numOfEntries * 2));
  
  uint32_t entry = 0;
  while (entry < numOfEntries) {
    uint32_t symbol = htonl(*bytes);
    bytes++;
    //uint8_t type = ((symbol >> 24) & 0xff);
    uint32_t tableIndex = (symbol & 0x00ffffff);
    uint32_t address = htonl(*bytes);
    bytes++;
    
    const char *name = nameTable+tableIndex;
    //uint8_t nameLen = name[0];
    name++;
    
    newton_add_symbol(c, address, name);
    entry++;
  }
  
  LOG_STR("Parsed %i symbols from AIF debug data\n", entry);
}

void newton_load_mapfile(newton_t *c, const char *mapfile) {
  FILE *fp = fopen(mapfile, "r");
  if (fp == NULL) {
    fprintf(stderr, "Couldn't open: %s\n", mapfile);
    return;
  }
  
  uint32_t symbolIndex = 0;
  
  while (!feof(fp)) {
    uint32_t addr = 0;
    uint32_t segment = 0;
    
    int matches = fscanf(fp, " %04x:%08x", &segment, &addr);
    bool validLine = true;
    if (matches != 2) {
      validLine = false;
    }
    
    for (int i=0; i<7; i++) {
      if (fgetc(fp) != ' ') {
        validLine = false;
        break;
      }
    }
    
    if (validLine == true) {
      char name[512];
      uint16_t nameIndex=0;
      
      char letter = 0;
      while((letter = fgetc(fp))) {
        if (letter == '\n' || letter == '\r') {
          break;
        }
        name[nameIndex++] = letter;
      }
      
      name[nameIndex] = 0x00;
      newton_add_symbol(c, addr, name);
      symbolIndex++;
    }
    else {
      while(fgetc(fp) != '\n' && !feof(fp))
        ;
    }
  }
  
  LOG_STR("Loaded %i symbols\n", symbolIndex);
  fclose(fp);
}
#endif

#pragma mark - Memory helpers
static inline membank_t* newton_get_membank_for_address(newton_t *c, uint32_t addr) {
  membank_t *membank = c->membanks;
  while (membank != NULL) {
    if (addr >= membank->base && addr < membank->base + membank->length) {
      return membank;
    }
    else {
      membank = membank->next;
    }
  }
  
  LOG_STR("UNKNOWN MEMORY READ: 0x%08x, PC=0x%08x\n", addr, arm_get_pc(c->arm));
  if (c->breakOnUnknownMemory) {
    newton_stop(c);
  }
  
  return NULL;
}

#if DISABLE_DEBUGGER
static inline void newton_get_mem_entry(newton_t *c, uint32_t addr) {}
static inline void newton_get_mem_exit(newton_t *c, uint32_t addr, uint32_t result) {}
static inline void newton_set_mem_entry(newton_t *c, uint32_t addr, uint32_t val) {}
static inline void newton_set_mem_exit(newton_t *c, uint32_t addr, uint32_t val) {}
#else
static inline bool newton_has_breakpoint_at_address(newton_t *c, uint32_t addr, bp_type type) {
  bp_entry_t *bp = c->breakpoints;
  while (bp != NULL) {
    if (bp->type == type && bp->addr == addr) {
      return true;
    }
    bp = bp->next;
  }
  return false;
}

static inline void newton_get_mem_entry(newton_t *c, uint32_t addr) {
  if (newton_has_breakpoint_at_address(c, addr, BP_READ) == true) {
    LOG_STR("\n\nAddress 0x%08x read from PC 0x%08x\n", addr, arm_get_pc(c->arm));
    newton_stop(c);
  }
}

static inline void newton_get_mem_exit(newton_t *c, uint32_t addr, uint32_t result) {
  if (c->memTrace == true) {
    if (addr != arm_get_pc(c->arm)) {
      const char *symName = newton_get_symbol_for_address(c, addr);
      if (symName == NULL) {
        symName = "";
      }
      LOG_STR("GET: 0x%08x %s => 0x%08x, PC=0x%08x\n", addr, symName, result, arm_get_pc(c->arm));
    }
  }
}

static inline void newton_set_mem_entry(newton_t *c, uint32_t addr, uint32_t val) {
  if (newton_has_breakpoint_at_address(c, addr, BP_WRITE) == true) {
    LOG_STR("\n\nAddress 0x%08x changed from:0x%08x to:0x%08x from PC 0x%08x\n", addr, newton_get_mem32(c, addr), val, arm_get_pc(c->arm));
    newton_stop(c);
  }
}

static inline void newton_set_mem_exit(newton_t *c, uint32_t addr, uint32_t val) {
  if (c->memTrace == true) {
    const char *symName = newton_get_symbol_for_address(c, addr);
    if (symName == NULL) {
      symName = "";
    }
    LOG_STR("SET: 0x%08x %s => 0x%08x, PC=0x%08x\n", addr, symName, val, arm_get_pc(c->arm));
  }
}
#endif


#pragma mark - Memory access
uint32_t newton_get_mem32 (newton_t *c, uint32_t addr) {
  newton_get_mem_entry(c, addr);
  
  if (addr % 4 != 0) {
    LOG_STR("misaligned read access: 0x%08x, PC: 0x%08x\n", addr, arm_get_pc(c->arm));
  }
  
  uint32_t result = 0;
  if (addr == 0x000013f4) {
    result = c->debuggerBits;
  }
  else if (addr == 0x000013f8) {
    result = c->newtTests;
  }
  else if (addr == 0x000013fc) {
    result = c->newtConfig;
  }
  else {
    membank_t *membank = newton_get_membank_for_address(c, addr);
    if (membank != NULL) {
      result = membank->get_uint32(membank->context, addr, arm_get_pc(c->arm));
    }
  }
  
  newton_get_mem_exit(c, addr, result);
  
  return result;
}

uint32_t newton_set_mem32 (newton_t *c, uint32_t addr, uint32_t val) {
  newton_set_mem_entry(c, addr, val);
  
  membank_t *membank = newton_get_membank_for_address(c, addr);
  if (membank != NULL) {
    val = membank->set_uint32(membank->context, addr, val, arm_get_pc(c->arm));
  }
  
  newton_set_mem_exit(c, addr, val);
  
  return val;
}

uint8_t newton_get_mem8 (newton_t *c, uint32_t addr) {
  uint8_t result = 0;
  
  newton_get_mem_entry(c, addr);
  
  membank_t *membank = newton_get_membank_for_address(c, addr);
  if (membank != NULL && membank->get_uint8 != NULL) {
    result = membank->get_uint8(membank->context, addr, arm_get_pc(c->arm));
  }
  else {
    int bytenum = addr & 3;
    uint32_t word = newton_get_mem32(c, addr - bytenum);
    
    word >>= ((3-bytenum) * 8);
    result = (word & 0xff);
  }
  
  newton_get_mem_exit(c, addr, result);
  
  return result;
}

uint8_t newton_set_mem8 (newton_t *c, uint32_t addr, uint8_t val) {
  uint8_t result = val;
  newton_set_mem_entry(c, addr, val);
  
  membank_t *membank = newton_get_membank_for_address(c, addr);
  if (membank != NULL && membank->set_uint8 != NULL) {
    result = membank->set_uint8(membank->context, addr, val, arm_get_pc(c->arm));
  }
  else {
    static const unsigned masktab[] = {
      0x00ffffff, 0xff00ffff, 0xffff00ff, 0xffffff00
    };
    
    int bytenum = addr & 3;
    uint32_t aligned = addr - bytenum;
    uint32_t mask = masktab[bytenum];
    uint32_t word = newton_get_mem32(c, aligned);
    
    uint32_t newval = word & mask;
    newval |= (val << ((3 - (bytenum)) * 8));
    
    newton_set_mem32(c, aligned, newval);
  }
  
  newton_set_mem_exit(c, addr, result);
  
  return val;
}


uint16_t newton_get_mem16 (newton_t *c, uint32_t addr) {
  abort();
}

uint16_t newton_set_mem16 (newton_t *c, uint32_t addr, uint16_t val) {
  abort();
}

#pragma mark -
void newton_set_debugger_bits(newton_t *c, uint32_t debugger_bits) {
  c->debuggerBits = debugger_bits;
}

uint32_t newton_get_debugger_bits(newton_t *c) {
  return c->debuggerBits;
}

void newton_set_newt_config(newton_t *c, uint32_t newt_config) {
  c->newtConfig = newt_config;
}

uint32_t newton_get_newt_config(newton_t *c) {
  return c->newtConfig;
}

void newton_set_newt_tests(newton_t *c, uint32_t newt_tests) {
  c->newtTests = newt_tests;
}

uint32_t newton_get_newt_tests(newton_t *c) {
  return c->newtTests;
}

void newton_set_logfile(newton_t *c, FILE *file) {
  c->logFile = file;
  if (c->runt != NULL) {
    runt_set_log_file(c->runt, file);
  }
  if (c->pcmcia != NULL) {
    pcmcia_set_log_file(c->pcmcia, file);
  }
}

int newton_log_opcode (void *ext, uint32_t ir) {
  newton_t *c = (newton_t *)ext;
  LOG_STR("%s %i\n", __PRETTY_FUNCTION__, ir);
  newton_stop((newton_t *)c);
  return 0;
}

char *newton_get_string(newton_t *c, uint32_t address, uint32_t length) {
  uint32_t translated = address;
  arm_translate_extern(c->arm, &translated, 0, NULL, NULL);
  
  char *msg = calloc(1024, sizeof(char));
  uint32_t value;
  int index = 0;
  while (1) {
    value = newton_get_mem32(c, translated);
    if (index < length) msg[index++] = (value >> 24) & 0xff;
    if (index < length) msg[index++] = (value >> 16) & 0xff;
    if (index < length) msg[index++] = (value >>  8) & 0xff;
    if (index < length) msg[index++] = (value    ) & 0xff;
    if (index >= length) {
      break;
    }
    translated += 4;
  }
  return msg;
}


char *newton_get_cstring(newton_t *c, uint32_t address) {
  uint32_t translated = address;
  arm_translate_extern(c->arm, &translated, 0, NULL, NULL);
  
  char *msg = calloc(1024, sizeof(char));
  uint32_t value;
  int index = 0;
  while (1) {
    value = newton_get_mem32(c, translated);
    msg[index+0] = (value >> 24) & 0xff;
    msg[index+1] = (value >> 16) & 0xff;
    msg[index+2] = (value >>  8) & 0xff;
    msg[index+3] = (value    ) & 0xff;
    if (msg[index+3] == 0x00 || msg[index+2] == 0x00 || msg[index+1] == 0x00 || msg[index+0] == 0x00) {
      break;
    }
    index += 4;
    translated += 4;
  }
  return msg;
}

#pragma mark - TapFileCntl
int32_t newton_do_sys_open(newton_t *c) {
  if (c->do_sys_open == NULL) {
    return -1;
  }
  
  uint32_t arg1 = 0;
  arm_get_mem32(c->arm, c->arm->reg[1], 0, &arg1);
  arm_translate_extern(c->arm, &arg1, 0, NULL, NULL);
  
  char *name = newton_get_cstring(c, arg1);
  
  uint32_t mode = 0;
  arm_get_mem32(c->arm, c->arm->reg[1] + 4, 0, &mode);
  
  int32_t fp = 0;
  if (c->supportsRegularFiles == false && name[0] != '%') {
    fp = -1;
    free(name);
  }
  else {
    fp = c->do_sys_open(c->tapfilecntl_ext, name, mode);
  }
  
  if (SHOULD_LOG(NewtonLogTapFileCntl)) {
    LOG_STR("Open: name='%s', mode=", name);
    switch(mode) {
      case 4:
        LOG_STR("write");
        break;
      case 0:
        LOG_STR("read");
        break;
      default:
        LOG_STR("unknown %i", mode);
        break;
    }
    LOG_STR(", fp=%i", fp);
  }
  
  
  /*
   On exit, R0 contains:
   • a nonzero handle if the call is successful
   • -1 if the call is not successful.
   */
  return fp;
}

int32_t newton_do_sys_close(newton_t *c) {
  if (c->do_sys_close == NULL) {
    return -1;
  }
  
  uint32_t fp = 0;
  uint32_t result = 0;
  
  arm_get_mem32(c->arm, c->arm->reg[1], 0, &fp);
  
  result = c->do_sys_close(c->tapfilecntl_ext, fp);
  
  if (SHOULD_LOG(NewtonLogTapFileCntl)) {
    LOG_STR("close fp=%i => %i", fp, result);
  }
  
  /*
   On exit, R0 contains:
   • 0 if the call is successful
   • -1 if the call is not successful.
   */
  return result;
}

int32_t newton_do_sys_istty(newton_t *c) {
  if (c->do_sys_istty == NULL) {
    return -1;
  }
  
  uint32_t fp = 0;
  uint32_t result = 0;
  
  arm_get_mem32(c->arm, c->arm->reg[1], 0, &fp);
  result = c->do_sys_istty(c->tapfilecntl_ext, fp);
  
  if (SHOULD_LOG(NewtonLogTapFileCntl)) {
    LOG_STR("istty, fp=%i, result=%i", fp, result);
  }
  
  /*
   On exit, R0 contains:
   • 1 if the handle identifies an interactive device
   • 0 if the handle identifies a file
   • a value other than 1 or 0 if an error occurs.
   */
  return result;
}

int32_t newton_do_sys_read(newton_t *c) {
  if (c->do_sys_read == NULL) {
    return -1;
  }
  
  uint32_t fp = 0;
  uint32_t len = 0;
  uint32_t addr = 0;
  int32_t result = 0;
  
  arm_get_mem32(c->arm, c->arm->reg[1], 0, &fp);
  arm_get_mem32(c->arm, c->arm->reg[1] + 4, 0, &addr);
  arm_get_mem32(c->arm, c->arm->reg[1] + 8, 0, &len);
  
  arm_translate_extern(c->arm, &addr, 0, NULL, NULL);
  
  uint8_t *buffer = calloc(len, sizeof(uint8_t));
  result = c->do_sys_read(c->tapfilecntl_ext, fp, buffer, len);
  
  if (result == 0) {
    buffer[0] = 0x00;
    result = 1;
  }
  
  for (int32_t i=0; i<result; i++) {
    newton_set_mem8(c, addr + i, buffer[i]);
  }
  free(buffer);
  
  if (SHOULD_LOG(NewtonLogTapFileCntl)) {
    LOG_STR("read fp=%i => %i", fp, result);
  }
  
  /*
   On exit:
   • R0 contains zero if the call is successful.
   • If R0 contains the same value as word 3, the call has failed and EOF is assumed.
   • If R0 contains a smaller value than word 3, the call was partially successful. No error is assumed, but the buffer has not been filled.
   */
  
  if (result == -1) {
    return len;
  }
  return len - result;
}

int32_t newton_do_sys_write(newton_t *c) {
  if (c->do_sys_write == NULL) {
    return -1;
  }
  
  uint32_t fp = 0;
  uint32_t len = 0;
  uint32_t addr = 0;
  
  arm_get_mem32(c->arm, c->arm->reg[1], 0, &fp);
  arm_get_mem32(c->arm, c->arm->reg[1] + 4, 0, &addr);
  arm_get_mem32(c->arm, c->arm->reg[1] + 8, 0, &len);
  
  char *msg = newton_get_string(c, addr, len);
  
  uint32_t result = c->do_sys_write(c->tapfilecntl_ext, fp, msg, len);
  
  if (SHOULD_LOG(NewtonLogTapFileCntl)) {
    LOG_STR("write(fp=%i, len=0x%08x, buf=0x%08x)\n", fp, len, addr);
    LOG_STR("msg: %s\n", msg);
  }
  
  free(msg);
  /*
   On exit, R0 contains:
   • 0 if the call is successful
   • the number of bytes that are not written, if there is an error.
   */
  
  return result;
}

int32_t newton_do_sys_set_input_notify(newton_t *c) {
  if (c->do_sys_set_input_notify == NULL) {
    return -1;
  }
  
  uint32_t fp, input_notify, result;
  arm_get_mem32(c->arm, c->arm->reg[1], 0, &fp);
  arm_get_mem32(c->arm, c->arm->reg[1] + 4, 0, &input_notify);
  
  result = c->do_sys_set_input_notify(c->tapfilecntl_ext, fp, input_notify);
  
  if (SHOULD_LOG(NewtonLogTapFileCntl)) {
    LOG_STR("set_input_notify: fp=%i, addr=%08x => %i", fp, input_notify, result);
  }
  return result;
}

void newton_file_input_notify(newton_t *c, uint32_t addr, uint32_t value)
{
  uint32_t translated = addr;
  arm_translate_extern(c->arm, &translated, 0, NULL, NULL);
  newton_set_mem32(c, translated, value);
}

void newton_tap_file_control(newton_t *c)
{
  enum {
    do_sys_open = 0x10,
    do_sys_close = 0x11,
    do_sys_istty = 0x12,
    do_sys_read = 0x13,
    do_sys_write = 0x14,
    do_sys_set_input_notify = 0x15,
  };
  
  if (SHOULD_LOG(NewtonLogTapFileCntl)) {
    LOG_STR("TapFileCntl: ");
  }
  
  uint32_t result = -1;
  switch (c->arm->reg[0]) {
    default:
      if (SHOULD_LOG(NewtonLogTapFileCntl)) {
        LOG_STR("unknown command: 0x%02x", c->arm->reg[0]);
      }
      break;
    case do_sys_open:
      result = newton_do_sys_open(c);
      c->arm->reg[7] = 8;
      break;
    case do_sys_close:
      result = newton_do_sys_close(c);
      break;
    case do_sys_istty:
      result = newton_do_sys_istty(c);
      break;
    case do_sys_read:
      result = newton_do_sys_read(c);
      break;
    case do_sys_write:
      result = newton_do_sys_write(c);
      break;
    case do_sys_set_input_notify:
      result = newton_do_sys_set_input_notify(c);
      break;
  }
  
  c->arm->reg[0] = result;
}

void newton_log_undef (void *ext, uint32_t ir) {
  newton_t *c = (newton_t *)ext;
  bool shouldLog = (SHOULD_LOG(NewtonLogUndefined));
  if (shouldLog == true) {
    LOG_STR("%s instr=0x%08x, PC=0x%08x: ", __PRETTY_FUNCTION__, ir, arm_get_pc(c->arm));
  }
  
  if (ir == 0xE6000010) {
    if (shouldLog == true) {
      LOG_STR("SystemBoot");
    }
    c->arm->reg[15] = c->arm->reg[15] + 4;
  }
  else if (ir == 0xE6000110) {
    if (shouldLog == true) {
      LOG_STR("ExitToShell");
    }
    newton_stop((newton_t *)ext);
  }
  else if (ir == 0xE6000210) {
    if (shouldLog == true) {
      LOG_STR("Debugger");
    }
    newton_stop((newton_t *)ext);
  }
  else if (ir == 0xE6000310) {
    if (c->debug_str != NULL || shouldLog == true) {
      char *msg = newton_get_cstring((newton_t *)ext, c->arm->reg[0]);
      if (c->debug_str != NULL) {
        c->debug_str(c, msg);
      }
      if (shouldLog == true) {
        LOG_STR("DebugStr: %s", msg);
      }
      free(msg);
    }
    newton_stop((newton_t *)ext);
  }
  else if (ir == 0xE6000410) {
    if (shouldLog == true) {
      LOG_STR("PublicFiller");
    }
  }
  else if (ir == 0xE6000510) {
    if (c->system_panic != NULL || shouldLog == true) {
      uint32_t address = arm_get_pc(c->arm) + 4;
      char *msg = newton_get_cstring((newton_t *)ext, address);
      if (shouldLog == true) {
        LOG_STR("SystemPanic: %s", msg);
      }
      if (c->system_panic != NULL) {
        c->system_panic(c, msg);
      }
      free(msg);
    }
    newton_stop((newton_t *)ext);
  }
  else if (ir == 0xE6000710) {
    if (shouldLog == true) {
      LOG_STR("SendTestResults");
    }
  }
  else if (ir == 0xE6000810) {
    newton_tap_file_control(c);
    c->arm->reg[15] = c->arm->reg[14];
  }
  else {
    if (shouldLog == true) {
      LOG_STR("UNKNOWN!");
    }
    if (c->undefined_opcode != NULL) {
      c->undefined_opcode(c, ir);
      newton_stop((newton_t *)ext);
    }
  }
  
  if (shouldLog == true) {
    LOG_STR("\n");
  }
}

static const char *swiNames[] = {
  "GetPort",
  "PortSend",
  "PortReceive",
  "EnterAtomic",
  "ExitAtomic",
  "Generic",
  "GenerateMessageIRQ",
  "PurgeMMUTLBEntry",
  "FlushMMU",
  "FlushIDC",
  "GetCPUVersion",
  "SemaphoreOp",
  "SetDomainRegister",
  "SMemSetBuffer",
  "SMemGetSize",
  "SMemCopyToShared",
  "SMemCopyFromShared",
  "SMemMsgSetTimerParms",
  "SMemMsgSetMsgAvailPort",
  "SMemMsgGetSenderTaskId",
  "SMemMsgSetUserRefCon",
  "SMemMsgGetUserRefCon",
  "SMemMsgCheckForDone",
  "SMemMsgMsgDone",
  "TurnOffCache",
  "TurnOnCache",
  "LowLevelCopyDone",
  "MonitorDispatch",
  "MonitorExit",
  "MonitorThrow",
  "EnterFIQAtomic",
  "ExitFIQAtomic",
  "SetGlobals",
  "GiveObject",
  "AcceptObject",
  "GetGlobalTime",
  "SetPCSample",
  "ResetAccountTime",
  "GetNextTaskId",
  "ForgetPhysMapping",
  "ForgetPermMapping",
  "ForgetBothMappings",
  "RememberPhysMapping",
  "RememberPermMapping",
  "RememberBothMappings",
  "AddPageTable",
  "RemovePageTable",
  "UnmapPhys",
  "RenamePhys",
  "ReleasePage",
  "CopyPage",
  "InvalidatePhys",
  "PhysSize",
  "PhysBase",
  "PhysAlign",
  "PhysReadOnly",
  "QueueTimer",
  "RemoveTimer",
  "FreeTasksBlockedOnMemory",
  "Yield",
  "Reboot",
  "Restart",
  "RegisterTimerFunction",
  "RemoveTimerFunction",
  "RememberMappingsUsingPAddr",
  "SetDomainRange",
  "ClearDomainRange",
  "SetEnvironment",
  "GetEnvironment",
  "AddDomainToEnvironment",
  "RemoveDomainFromEnvironment",
  "HasDomain",
  "SemGroupSetRefCon",
  "SemGroupGetRefCon",
  "VToP",
  "RealTimeAlarm",
  "GetMemObjInfo",
  "GetNetworkPersistentInfo",
  "BequeathId",
  "DispatchPatchInfo"
};

void newton_log_exception (void *ext, uint32_t addr) {
  newton_t *c = (newton_t *)ext;
  switch(addr) {
    case 0x00:
      if (SHOULD_LOG(NewtonLogVectorTable)) {
        LOG_STR("%s PC=0x%08x: reset_handler\n", __PRETTY_FUNCTION__, arm_get_pc(c->arm));
      }
      break;
    case 0x04:
      if (SHOULD_LOG(NewtonLogVectorTable)) {
        LOG_STR("%s PC=0x%08x: undefined_handler\n", __PRETTY_FUNCTION__, arm_get_pc(c->arm));
      }
      break;
    case 0x08: {
      if (SHOULD_LOG(NewtonLogSWI)) {
        uint32_t swi = newton_get_mem32(c, arm_get_pc(c->arm)) & 0x00ffffff;
        const char *swiName;
        if (swi < countof(swiNames)) {
          swiName = swiNames[swi];
        }
        else {
          swiName = "unknown";
        }
        LOG_STR("%s PC=0x%08x: swi_handler 0x%06x: %s", __PRETTY_FUNCTION__, arm_get_pc(c->arm), swi, swiName);
        switch (swi) {
          case 0x1d: {
            char *exception = newton_get_cstring(c, c->arm->reg[0]);
            LOG_STR(": %s", exception);
            free(exception);
            break;
          }
          case 0x01:
          case 0x02:
            LOG_STR(" ObjectId=%i, msgId=%i, msgFilter=%i, flags=%i", c->arm->reg[0], c->arm->reg[1], c->arm->reg[2], c->arm->reg[3]);
            break;
          case 0x05:
            LOG_STR(" inSelector=%i", c->arm->reg[0]);
            break;
          case 0x0d:
          case 0x0f:
            LOG_STR(" ObjectId=%i, buffer=0x%08x, size=%i, permissions=%i\n", c->arm->reg[0], c->arm->reg[1], c->arm->reg[2], c->arm->reg[3]);
            newton_mem_hexdump((newton_t *)ext, c->arm->reg[1], c->arm->reg[2]);
            break;
          case 0x0e:
          case 0x10:
          case 0x11:
          case 0x13:
          case 0x16:
          case 0x17:
            LOG_STR(" ObjectId=%i", c->arm->reg[0]);
            break;
        }
        LOG_STR("\n");
      }
      break;
    }
    case 0x0c:
      if (SHOULD_LOG(NewtonLogVectorTable)) {
        LOG_STR("%s PC=0x%08x: prefetch_handler\n", __PRETTY_FUNCTION__, arm_get_pc(c->arm));
        newton_stop((newton_t *)ext);
      }
      break;
    case 0x10:
      if (SHOULD_LOG(NewtonLogVectorTable)) {
        LOG_STR("%s PC=0x%08x: data_handler\n", __PRETTY_FUNCTION__, arm_get_pc(c->arm));
      }
      break;
    case 0x14:
      if (SHOULD_LOG(NewtonLogVectorTable)) {
        LOG_STR("%s PC=0x%08x: unused_handler\n", __PRETTY_FUNCTION__, arm_get_pc(c->arm));
      }
      break;
    case 0x18:
      if (SHOULD_LOG(NewtonLogVectorTable)) {
        LOG_STR("%s PC=0x%08x: irq_handler\n", __PRETTY_FUNCTION__, arm_get_pc(c->arm));
      }
      break;
    case 0x1c:
      if (SHOULD_LOG(NewtonLogVectorTable)) {
        LOG_STR("%s PC=0x%08x: fiq_handler\n", __PRETTY_FUNCTION__, arm_get_pc(c->arm));
      }
      break;
  }
  //  newton_stop((newton_t *)ext);
}

#pragma mark - Serial
void newton_serial_channel_enqueue_data(newton_t *c, uint8_t channel, uint8_t *data, uint32_t size) {
  newton_serial_queue_t *queue = &c->serialQueues[channel];
  
  if (queue->buffer == NULL) {
    queue->length = size;
    queue->buffer = malloc(size);
    queue->offset = 0;
    memcpy(queue->buffer, data, size);
  }
  else {
    uint32_t newLength = queue->length + size;
    queue->buffer = realloc(queue->buffer, newLength);
    
    memcpy(queue->buffer + queue->length, data, size);
    queue->length = newLength;
  }
}

void newton_serial_channel_dequeue_data(newton_t *c, uint8_t channel) {
  e8530_t *scc = runt_get_scc(c->runt);
  
  newton_serial_queue_t *queue = &c->serialQueues[channel];
  if (queue->length == 0) {
    return;
  }
  
  int success = e8530_receive(scc, channel, queue->buffer[queue->offset]);
  if (success == 0) {
    queue->offset++;
    if (queue->offset >= queue->length) {
      free(queue->buffer);
      queue->buffer = NULL;
      queue->length = 0;
      queue->offset = 0;
    }
  }
}

void newton_serial_channel_send(newton_t *c, uint8_t channel, uint8_t *data, uint32_t size) {
  e8530_t *scc = runt_get_scc(c->runt);
  for (uint32_t i=0; i<size; i++) {
    int success = e8530_receive(scc, channel, data[i]);
    if (success != 0) {
      uint32_t remaining = (size - i);
      newton_serial_channel_enqueue_data(c, channel, data + i, remaining);
      return;
    }
  }
}

void newton_handle_docker_payload(newton_t *c, uint8_t channel) {
  docker_t *docker = c->docker;
  
  docker_parse_payload(docker);
  
  uint32_t rspLen = 0;
  uint8_t *response = docker_get_response(docker, &rspLen);
  if (rspLen > 0 && response != NULL) {
    newton_serial_channel_send(c, channel, response, rspLen);
  }
  
  docker_reset(docker);
}

void newton_serial_channel_flush_output(newton_t *c, uint8_t channel) {
  e8530_t *scc = runt_get_scc(c->runt);
  docker_t *docker = c->docker;
  
  while (e8530_out_empty(scc, channel) == false) {
    uint8_t val = e8530_send(scc, channel);
    if (c->bootMode != NewtonBootModeNormal) {
      // Do a serial loopback for diagnostics
      e8530_receive(scc, channel, val);
    }
    else {
      if (channel == RuntSerialChannelSerial) {
        docker_receive_byte(docker, val);
        if (docker_can_parse_payload(docker) == true) {
          newton_handle_docker_payload(c, RuntSerialChannelSerial);
        }
      }
    }
  }
}

void newton_serial_chanA_output(void *ext, uint8_t val) {
  newton_t *newton = (newton_t *)ext;
  newton_serial_channel_flush_output(newton, RuntSerialChannelA);
}

void newton_serial_chanB_output(void *ext, uint8_t val) {
  newton_t *newton = (newton_t *)ext;
  newton_serial_channel_flush_output(newton, RuntSerialChannelB);
}

void newton_serial_chanA_input(void *ext, uint8_t val) {
  newton_t *newton = (newton_t *)ext;
  newton_serial_channel_dequeue_data(newton, RuntSerialChannelA);
}

void newton_serial_chanB_input(void *ext, uint8_t val) {
  newton_t *newton = (newton_t *)ext;
  newton_serial_channel_dequeue_data(newton, RuntSerialChannelB);
}

#pragma mark -
#pragma mark
void newton_set_bootmode(newton_t *c, NewtonBootMode bootMode) {
  c->bootMode = bootMode;
  
  switch (bootMode) {
    case NewtonBootModeAutoPWB:
      c->runt->interrupt = 0xffffffff;
      c->runt->interruptStick = 2;
      break;
    case NewtonBootModeDiagnostics:
      c->runt->interrupt = RuntInterruptBatteryCover;
      if ((c->romVersion >> 16) == 2) {
        c->runt->interruptStick = 1;
      }
      else {
        c->runt->interruptStick = 0;
      }
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

void newton_reboot(newton_t *c, NewtonRebootStyle style) {
  arm_reset(c->arm);
  runt_reset(c->runt);
  if (style == NewtonRebootStyleCold) {
    memory_clear(c->ram);
  }
  docker_reset(c->docker);
}

void newton_emulate(newton_t *c, int32_t count) {
  int32_t remaining = count;
  c->stop = false;
  
  bool armAwake = true;
  while (remaining > 0 && c->stop == false) {
#if !DISABLE_DEBUGGER
    if (c->instructionTrace == true) {
      newton_print_state(c);
    }
#endif
    
    if (armAwake == false) {
      usleep(10);
    }
    else {
      arm_execute(c->arm);
      
      if (count != INT32_MAX) {
        remaining--;
      }
      
#if !DISABLE_DEBUGGER
      uint32_t pc = arm_get_pc (c->arm);
      if (pc != c->lastPc + 4) {
        if (c->pcSpy) {
          const char *symbol = newton_get_symbol_for_address(c, pc);
          if (symbol == NULL) {
            symbol = "";
          }
          
          LOG_STR("PC changed to 0x%08x %s (from 0x%08x)\n", pc, symbol, c->lastPc);
          fflush(c->logFile);
        }
      }
      
      if (c->lastSp != c->arm->reg[13] && c->spSpy) {
        LOG_STR("SP changed from 0x%08x to 0x%08x (at PC 0x%08x)\n", c->lastSp, c->arm->reg[13], pc);
        fflush(c->logFile);
        c->lastSp = c->arm->reg[13];
      }
      
      bp_entry_t *bp = c->breakpoints;
      while(bp != NULL) {
        if (bp->addr == pc && bp->type == BP_PC) {
          remaining = 0;
          break;
        }
        bp = bp->next;
      }
      
      c->lastPc = pc;
#endif
    }
    
    armAwake = runt_step(c->runt);
  }
}

runt_t *newton_get_runt (newton_t *c) {
  return c->runt;
}

pcmcia_t *newton_get_pcmcia (newton_t *c) {
  return c->pcmcia;
}

docker_t *newton_get_docker (newton_t *c) {
  return c->docker;
}

void newton_set_log_flags (newton_t *c, unsigned flags, int val) {
  if (val) {
    c->logFlags |= flags;
  }
  else {
    c->logFlags &= ~flags;
  }
  
  pcmcia_set_log_flags(c->pcmcia, flags);
}

void newton_set_tapfilecntl_functions (newton_t *c, void *ext,
                                       newton_do_sys_open_f do_sys_open,
                                       newton_do_sys_close_f do_sys_close,
                                       newton_do_sys_istty_f do_sys_istty,
                                       newton_do_sys_read_f do_sys_read,
                                       newton_do_sys_write_f do_sys_write,
                                       newton_do_sys_set_input_notify_f do_sys_set_input_notify)
{
  c->tapfilecntl_ext = ext;
  c->do_sys_open = do_sys_open;
  c->do_sys_close = do_sys_close;
  c->do_sys_istty = do_sys_istty;
  c->do_sys_read = do_sys_read;
  c->do_sys_write = do_sys_write;
  c->do_sys_set_input_notify = do_sys_set_input_notify;
}

void newton_set_system_panic(newton_t *c, newton_system_panic_f system_panic) {
  c->system_panic = system_panic;
}

void newton_set_undefined_opcode(newton_t *c, newton_undefined_opcode_f undefined_opcode) {
  c->undefined_opcode = undefined_opcode;
}

void newton_set_debugstr(newton_t *c, newton_debugstr_f debugstr) {
  c->debug_str = debugstr;
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
  arm_set_flags(c->arm, ARM_FLAG_BIGENDIAN, 1);
  
  c->arm->log_ext = c;
  c->arm->log_exception = newton_log_exception;
  c->arm->log_opcode = newton_log_opcode;
  c->arm->log_undef = newton_log_undef;
  arm_set_mem_fct(c->arm, c,
                  newton_get_mem8, newton_get_mem16, newton_get_mem32,
                  newton_set_mem8, newton_set_mem16, newton_set_mem32);
  arm_reset(c->arm);
  
  //
  // Setup floating point coprocessor
  //
  fpa_init(c->arm);
  
  //
  // Logging
  //
  newton_set_logfile(c, stdout);
  
  //
  // Docker, used for the docking protocol
  //
  c->docker = docker_new();
  
  //
  //
  //
#if !DISABLE_DEBUGGER
  c->lastPc = 0;
  c->lastSp = 0;
#endif
}

void newton_install_memory_handler(newton_t *c,
                                   uint32_t base,
                                   uint32_t length,
                                   void *context,
                                   void *getuint32,
                                   void *setuint32,
                                   void *getuint8,
                                   void *setuint8,
                                   void *del)
{
  membank_t *bank = calloc(sizeof(membank_t), 1);
  bank->context = context;
  bank->set_uint32 = setuint32;
  bank->get_uint32 = getuint32;
  bank->set_uint8 = setuint8;
  bank->get_uint8 = getuint8;
  bank->del = del;
  
  bank->base = base;
  bank->length = length;
  
  bank->next = c->membanks;
  c->membanks = bank;
}

void newton_install_memory(newton_t *c, memory_t *memory, uint32_t base, uint32_t length) {
  newton_install_memory_handler(c, base, length,
                                memory,
                                memory_get_uint32, memory_set_uint32,
                                memory_get_uint8, memory_set_uint8,
                                memory_delete);
}


int newton_configure_voyager(newton_t *c, memory_t *rom) {
  if (c->machineType == kGestalt_MachineType_Emate) {
    arm_set_id(c->arm, ARM_C15_ID_710);
  }
  else {
    arm_set_id(c->arm, ARM_C15_ID_STRONGARM);
  }
  
  fprintf(stderr, "Voyager platform isn't supported yet...\n");
  return -1;
}

int newton_configure_runt(newton_t *c, memory_t *rom) {
  arm_set_id(c->arm, ARM_C15_ID_610);
  
  // Configure 16MB ROM space
  if (memory_get_length(rom) == 0x00800000) {
    // For 8MB ROMs, don't repeat the ROM twice in the 16MB
    // space.  Instead, repeat the last 4MB after the full 8MB.
    // MP130 v1.2 diagnostics wants to read a lot from
    // addresses like 0x00abe0ac. With the default mirroring,
    // this would point back at 0x002bdfbc, which isn't part of the
    // diagnostics region.  With this mapping, it ends up
    // pointing at 0x006be0ac, which is in diagnostics region
    // and happens to be the string "DIAGNOSTICS".
    // (Without this, diagnostics didn't work. With it, it does)
    memory_add_mapping(rom, 0x00800000, 0x00400000, 0x00400000);
    memory_add_mapping(rom, 0x00c00000, 0x00400000, 0x00400000);
  }
  newton_install_memory(c, rom, 0x00000000, 0x01000000);
  
  uint32_t ramBank1 = 0;
  
  if (c->machineType == kGestalt_MachineType_MessagePad) {
    ramBank1 = 512 * 1024;
    
    // RAM Bank 2 is really 128KB, but is accessed only through
    // one byte per word.  So, 512KB will look like 128KB.
    memory_t *ram2 = memory_new("RAM2", 0x01200000, 512 * 1024);
    newton_install_memory(c, ram2, 0x01200000, 2 * 1024 * 1024);
  }
  else { // Lindy
    ramBank1 = 2 * 1024 * 1024;
    
    memory_t *flash = memory_new("FLASH", 0x01200000, 1024 * 1024);
    // A2 = 28F008 1MB
    // A4 = 29F040 512K
    // Each byte represents an individual flash chip
    memory_set_flash_code(flash, 0xa2000000);
    newton_install_memory(c, flash, 0x01200000, 2 * 1024 * 1024);
  }
  
  // Configure RAM
  memory_t *ram = memory_new("RAM", 0x01000000, ramBank1);
  newton_install_memory(c, ram, 0x01000000, 2 * 1024 * 1024);
  c->ram = ram;
  
  // Configure the Runt ASIC
  c->runt = runt_new(c->machineType);
  runt_set_arm(c->runt, c->arm);
  runt_set_log_file(c->runt, c->logFile);
  newton_install_memory_handler(c, 0x01400000, 0x00400000, c->runt, runt_get_mem32, runt_set_mem32, runt_get_mem8, runt_set_mem8, runt_del);
  
  //
  // Route the output (TX) of the SCC into us
  //
  e8530_t *scc = runt_get_scc(c->runt);
  e8530_set_out_fct (scc, 0, c, newton_serial_chanA_output);
  e8530_set_out_fct (scc, 1, c, newton_serial_chanB_output);
  e8530_set_inp_fct (scc, 0, c, newton_serial_chanA_input);
  e8530_set_inp_fct (scc, 1, c, newton_serial_chanB_input);
  
  // Configure pcmcia handler
  pcmcia_t *pcmcia = pcmcia_new();
  pcmcia_set_log_file(pcmcia, c->logFile);
  pcmcia_set_runt(pcmcia, c->runt);
  c->pcmcia = pcmcia;
  
  // For PCMCIA card access (yes, a 512MB region...)
  newton_install_memory_handler(c, 0x10000000, 0x0fffffff, pcmcia, pcmcia_get_mem32, pcmcia_set_mem32, NULL, NULL, pcmcia_del);
  
  // For PCMCIA control registers
  // No delete, as the above will get it.
  newton_install_memory_handler(c, 0x70000000, 0x0fffffff, pcmcia, pcmcia_get_mem32, pcmcia_set_mem32, NULL, NULL, NULL);
  
  return 0;
}

int newton_load_rom(newton_t *c, const char *path) {
  FILE *romFP = fopen(path, "r");
  if (romFP == NULL) {
    LOG_STR("Couldn't open ROM '%s': %s\n", path, strerror(errno));
    return -1;
  }
  
  fseek(romFP, 0l, SEEK_END);
  uint32_t romSize = (uint32_t)ftell(romFP);
  fseek(romFP, 0l, SEEK_SET);
  
  if (romSize % 4 != 0) {
    LOG_STR("Bad ROM size: %i\n", romSize);
    return -1;
  }
  
  uint32_t romWord = 0;
  if (fread(&romWord, 1, sizeof(romWord), romFP) == sizeof(romWord)) {
    if (htonl(romWord) == 0xE1A00000) {
      LOG_STR("ROM file appears to be an AIF image\n");
      romSize -= 128;
      
      fseek(romFP, 0x14, SEEK_SET);
      
#define AIF_HEADER_SIZE 128
      uint32_t readOnlySize=0, readWriteSize=0, debugSize=0;
      
      fread(&readOnlySize, 1, sizeof(readOnlySize), romFP);
      readOnlySize = htonl(readOnlySize);
      fread(&readWriteSize, 1, sizeof(readWriteSize), romFP);
      readWriteSize = htonl(readWriteSize);
      fread(&debugSize, 1, sizeof(debugSize), romFP);
      debugSize = htonl(debugSize);
      
      if (readOnlySize + readWriteSize + debugSize != romSize) {
        LOG_STR("readOnlySize:%i + readWriteSize:%i + debugSize:%i != romSize:%i\n", readOnlySize, readWriteSize, debugSize, romSize);
      }
      else {
#if !DISABLE_DEBUGGER
        void *debugData = malloc(debugSize);
        fseek(romFP, AIF_HEADER_SIZE + readOnlySize + readWriteSize, SEEK_SET);
        if (fread(debugData, 1, debugSize, romFP) == debugSize) {
          newton_parse_aif_debug_data(c, debugData, debugSize);
        }
        free(debugData);
#endif
      }
      fseek(romFP, AIF_HEADER_SIZE, SEEK_SET);
    }
    else {
      fseek(romFP, 0, SEEK_SET);
    }
  }
  
  memory_t *rom = memory_new("ROM", 0x0, romSize);
  memory_set_readonly(rom, true);
  uint32_t i=0;
  while (fread(&romWord, 1, sizeof(romWord), romFP) == sizeof(romWord)) {
    rom->contents[i++] = (((romWord    ) & 0xff) << 24) |
    (((romWord >>  8) & 0xff) << 16) |
    (((romWord >> 16) & 0xff) << 8 ) |
    (((romWord >> 24) & 0xff));
  }
  fclose(romFP);
  
  LOG_STR("Loaded ROM: %s => %i bytes\n", path, romSize);
  
  c->machineType = memory_get_uint32(rom, 0x000013ec, 0);
  LOG_STR("Machine Type  : 0x%08x => ", c->machineType);
  switch (c->machineType) {
    case kGestalt_MachineType_MessagePad:
      LOG_STR("Junior");
      break;
    case kGestalt_MachineType_Lindy:
      LOG_STR("Lindy");
      break;
    case kGestalt_MachineType_Bic:
      LOG_STR("Bic");
      break;
    case kGestalt_MachineType_Senior:
      LOG_STR("Senior");
      break;
    case kGestalt_MachineType_Emate:
      LOG_STR("eMate");
      break;
    default:
      LOG_STR("UNKNOWN - Defaulting to Junior");
      c->machineType = kGestalt_MachineType_MessagePad;
      break;
  }
  LOG_STR("\n");
  
  c->romManufacturer = memory_get_uint32(rom, 0x000013f0, 0);
  LOG_STR("ROM Manufacturer: 0x%08x => ", c->romManufacturer);
  switch (c->romManufacturer) {
    case kGestalt_Manufacturer_Apple:
      LOG_STR("Apple");
      break;
    case kGestalt_Manufacturer_Sharp:
      LOG_STR("Sharp");
      break;
    case kGestalt_Manufacturer_Motorola:
      LOG_STR("Motorola");
      break;
    default:
      LOG_STR("UNKNOWN - Defaulting to Apple");
      c->romManufacturer = kGestalt_Manufacturer_Apple;
      break;
  }
  LOG_STR("\n");
  
  uint32_t romVersion = memory_get_uint32(rom, 0x13dc, 0);
  if (romVersion == 0x06290000) {
    c->supportsRegularFiles = false;
  }
  else {
    c->supportsRegularFiles = true;
  }
  c->romVersion = romVersion;
  
  if (c->machineType == kGestalt_MachineType_Senior || c->machineType == kGestalt_MachineType_Emate) {
    return newton_configure_voyager(c, rom);
  }
  else {
    return newton_configure_runt(c, rom);
  }
}

void newton_free (newton_t *c)
{
#if !DISABLE_DEBUGGER
  bp_entry_t *bp = c->breakpoints;
  while (bp != NULL) {
    bp_entry_t *next = bp->next;
    free(bp);
    bp = next;
  }
  
  symbol_t *sym = c->symbols;
  while(sym != NULL) {
    symbol_t *next = sym->next;
    free(sym->name);
    free(sym);
    sym = next;
  }
#endif
  
  membank_t *membank = c->membanks;
  while(membank != NULL) {
    membank_t *next = membank->next;
    if (membank->del != NULL) {
      membank->del(membank->context);
    }
    free(membank);
    membank = next;
  }
  
  if (c->serialQueues[0].buffer != NULL) {
    free(c->serialQueues[0].buffer);
  }
  if (c->serialQueues[1].buffer != NULL) {
    free(c->serialQueues[1].buffer);
  }
  
  docker_del(c->docker);
  arm_del(c->arm);
  fpa_delete();
}

void newton_del (newton_t *c)
{
  if (c != NULL) {
    newton_free (c);
    free (c);
  }
}
