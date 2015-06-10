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

typedef struct newton_s {
  arm_t *arm;
  bool stop;
  
  runt_t *runt;
  
  uint32_t *ram1;
  uint32_t *ram2;
  uint32_t *rom;
  uint32_t romSize;
  uint32_t *flash;
  
  uint32_t *breakpoints;
  uint32_t breakpointsCapacity;
  uint32_t breakpointsTail;

  uint32_t *memwatch;
  uint32_t memwatchCapacity;
  uint32_t memwatchTail;

  bool instructionTrace;
  bool memTrace;

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

void newton_emulate(newton_t *c, int32_t count);
void newton_stop(newton_t *c);
void newton_set_bootmode(newton_t *c, NewtonBootMode bootMode);

void newton_set_logfile(newton_t *c, FILE *file);
void newton_print_state(newton_t *c);

void newton_breakpoint_add(newton_t *c, uint32_t breakpoint);
void newton_breakpoint_del(newton_t *c, uint32_t breakpoint);

void newton_memwatch_add(newton_t *c, uint32_t memwatch);
void newton_memwatch_del(newton_t *c, uint32_t memwatch);

void newton_set_instruction_trace(newton_t *c, bool instructionTrace);
bool newton_get_instruction_trace(newton_t *c);

void newton_set_pc_spy(newton_t *c, bool pcSpy);
bool newton_get_pc_spy(newton_t *c);

void newton_set_sp_spy(newton_t *c, bool spSpy);
bool newton_get_sp_spy(newton_t *c);

#endif
