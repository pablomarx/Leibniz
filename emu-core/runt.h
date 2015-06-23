//
//  runt.h
//  Leibniz
//
//  Created by Steve White on 9/13/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#ifndef __Leibniz__runt__
#define __Leibniz__runt__

#include "arm.h"

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

enum {
  RuntLogNone       = 0x00000000,
  
  RuntLogInterrupts = (1 << 1),
  RuntLogTimer      = (1 << 2),
  RuntLogTicks      = (1 << 3),
  RuntLogADC        = (1 << 4),
  RuntLogLCD        = (1 << 5),
  RuntLogIR         = (1 << 6),
  RuntLogSerial     = (1 << 7),
  RuntLogPower      = (1 << 8),
  RuntLogSwitch     = (1 << 9),
  RuntLogSound      = (1 << 10),
  RuntLogRTC        = (1 << 11),
  RuntLogUnknown    = (1 << 12),

  RuntLogAll        = 0xffffffff,
};

// Unknown interrupts were observed being registered
// via RegisterInterrupt() in a MP130 v2.0 ROM.
enum {
  RuntInterruptUnknown1         = (1 << 1),
  RuntInterruptRTC              = (1 << 2), // questionable
  RuntInterruptUnknown2         = (1 << 4),
  RuntInterruptADC           = (1 << 10),
  RuntInterruptSound            = (1 << 12),
  RuntInterruptUnknown3         = (1 << 13),
  RuntInterruptUnknown4         = (1 << 14), // the diagnostic jumper pads?
  RuntInterruptCardLockSwitch   = (1 << 15),
  RuntInterruptPowerSwitch      = (1 << 16),
  RuntInterruptSerial           = (1 << 17), // questionable
  RuntInterruptUnknown5         = (1 << 25),
};

typedef uint32_t (*lcd_get_uint32_f) (void *ext, uint32_t addr);
typedef uint32_t (*lcd_set_uint32_f) (void *ext, uint32_t addr, uint32_t val);
typedef const char * (*lcd_get_address_name_f) (void *ext, uint32_t addr);


typedef struct runt_s {
  arm_t *arm;
  uint32_t base;
  uint32_t *memory;
  time_t bootTime;
  
  // Logging
  uint32_t logFlags;
  FILE *logFile;
  
  // Interrupts
  uint32_t interrupt;
  uint32_t interruptStick;
  
  // Display
  void               *lcd_ext;
  lcd_get_uint32_f   lcd_get_uint32;
  lcd_set_uint32_f   lcd_set_uint32;
  lcd_get_address_name_f lcd_get_address_name;
  
  // Switches
  int8_t switches[3];
  
  // Tablet
  bool touchActive;
  uint32_t touchX;
  uint32_t touchY;
} runt_t;

void runt_init (runt_t *c);
runt_t *runt_new (void);
void runt_free (runt_t *c);
void runt_del (runt_t *c);
void runt_set_arm (runt_t *c, arm_t *arm);

void runt_set_lcd_fct(runt_t *c, void *ext,
                      void *get32, void *set32, void *getname);

void runt_set_log_flags (runt_t *c, unsigned flags, int val);
void runt_set_log_file (runt_t *c, FILE *file);

uint32_t runt_set_mem32(runt_t *c, uint32_t addr, uint32_t val);
uint32_t runt_get_mem32(runt_t *c, uint32_t addr);

void runt_switch_state(runt_t *c, int switchNum, int state);

void runt_touch_down(runt_t *c, int x, int y);
void runt_touch_up(runt_t *c);

#endif /* defined(__Leibniz__runt__) */
