//
//  newton.h
//  Leibniz
//
//  Created by Steve White on 9/13/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#ifndef Leibniz_newton_h
#define Leibniz_newton_h

#include "arm.h"
#include "runt.h"

#include <stdio.h>

/*	From NewtonGestalt.h: TGestaltSystemInfo */

#define kGestalt_Manufacturer_Apple			0x01000000
#define kGestalt_Manufacturer_Sharp 		0x10000100

#define kGestalt_MachineType_MessagePad		0x10001000
#define kGestalt_MachineType_Lindy			0x00726377
#define kGestalt_MachineType_Bic			0x10002000

typedef struct symbol_s symbol_t;
struct symbol_s {
  const char *name;
  uint32_t address;
  symbol_t *next;
};

typedef enum {
  BP_NONE   = 0,
  BP_PC     = 1,
  BP_READ   = 2,
  BP_WRITE  = 3,
} bp_type;

typedef struct bp_entry_s bp_entry_t;
struct bp_entry_s {
  bp_type    type;
  uint32_t   addr;
  bp_entry_t *next;
};


typedef struct newton_s {
  arm_t *arm;
  bool stop;
  
  void *lcd_driver;
  runt_t *runt;
  
  uint32_t *ram;
  uint32_t ramSize;

  uint32_t *rom;
  uint32_t romSize;
  
  uint32_t *flash;
  uint32_t flashSize;
  
  uint32_t machineType;
  uint32_t romManufacturer;
  uint32_t debuggerBits;
  uint32_t newtConfig;

  bp_entry_t *breakpoints;
  symbol_t *symbols;
  
  bool instructionTrace;
  bool memTrace;
  bool breakOnUnknownMemory;

  bool pcSpy;
  bool spSpy;
  uint32_t lastPc;
  uint32_t lastSp;
  
  FILE *logFile;
} newton_t;

typedef enum {
  NewtonBootModeNormal = 0,
  NewtonBootModeDiagnostics,
  NewtonBootModeAutoPWB,
} NewtonBootMode;

void arm_dasm_str (char *dst, arm_dasm_t *op);

void newton_init (newton_t *c);
newton_t *newton_new (void);
int newton_load_rom(newton_t *c, const char *path);
void newton_free (newton_t *c);
void newton_del (newton_t *c);

uint32_t newton_get_mem32 (newton_t *c, uint32_t addr);
uint8_t newton_get_mem8 (newton_t *c, uint32_t addr);
uint16_t newton_get_mem16 (newton_t *c, uint32_t addr);
uint8_t newton_set_mem8 (newton_t *c, uint32_t addr, uint8_t val);
uint16_t newton_set_mem16 (newton_t *c, uint32_t addr, uint16_t val);
uint32_t newton_set_mem32 (newton_t *c, uint32_t addr, uint32_t val);

runt_t *newton_get_runt (newton_t *c);

void newton_emulate(newton_t *c, int32_t count);
void newton_stop(newton_t *c);
void newton_set_bootmode(newton_t *c, NewtonBootMode bootMode);

void newton_set_debugger_bits(newton_t *c, uint32_t debugger_bits);
uint32_t newton_get_debugger_bits(newton_t *c);

void newton_set_newt_config(newton_t *c, uint32_t newt_config);
uint32_t newton_get_newt_config(newton_t *c);

uint32_t newton_address_for_symbol(newton_t *c, const char *symbol);
void newton_load_mapfile(newton_t *c, const char *mapfile);
void newton_set_logfile(newton_t *c, FILE *file);
void newton_print_state(newton_t *c);

void newton_breakpoint_add(newton_t *c, uint32_t address, bp_type type);
void newton_breakpoint_del(newton_t *c, uint32_t address, bp_type type);

void newton_set_break_on_unknown_memory(newton_t *c, bool breakOnUnknownMemory);
bool newton_get_break_on_unknown_memory(newton_t *c);

void newton_set_instruction_trace(newton_t *c, bool instructionTrace);
bool newton_get_instruction_trace(newton_t *c);

void newton_set_pc_spy(newton_t *c, bool pcSpy);
bool newton_get_pc_spy(newton_t *c);

void newton_set_sp_spy(newton_t *c, bool spSpy);
bool newton_get_sp_spy(newton_t *c);

#endif
