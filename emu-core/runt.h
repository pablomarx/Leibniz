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
  RuntLogSound      = (1 << 9),
  RuntLogRTC        = (1 << 10),
  RuntLogCPUControl = (1 << 11),
  RuntLogUnknown    = (1 << 12),

  RuntLogAll        = 0xffffffff,
};

enum {
  RuntPowerTrim = 0x40,       // X5VTRIM_ON - enable card +5V supply
  RuntPowerSerial = 0x20,     // X5VSERIAL_ON - enable scc, driver and IR supply
  RuntPowerSound = 0x10,      // X5VSOUND_ON - enable sound out supply
  RuntPowerVPP1 = 0x08,       // X12V_CONT_1 - enable voltage vpp1, from +5VTRM to +12V
  RuntPowerVPP2 = 0x04,       // X12V_CONT_2 - enable voltage vpp2, from +5VTRM to +12V
  RuntPowerLCD = 0x02,
  RuntPowerADC = 0x01,
};

// Unknown interrupts were observed being registered
// via RegisterInterrupt() in a MP130 v2.0 ROM.
enum {
  RuntInterruptRTC              = (1 << 1),
  RuntInterruptTicks            = (1 << 2),
  RuntInterruptTicks2           = (1 << 3), // Notepad 1.0b1: RegisterInterrupt(0x8, 0x0, 0x74344); -- tablet?
  RuntInterruptUnknown1         = (1 << 4),
  RuntInterruptADC              = (1 << 10),
  RuntInterruptSerialA          = (1 << 11), // FIQ - Notepad 1.0b1: RegisterInterrupt(0x800, 0x0, 0x747c4); -- nop
  RuntInterruptSound            = (1 << 12),
  RuntInterruptPCMCIA           = (1 << 13), // Notepad 1.0b1: RegisterInterrupt(0x2000, 0x0, 0x747e0) -- something with 0x01500000, which in MMU world, 0x01500000 maps to 0x70000000, so PCMCIA interrupt?
  RuntInterruptDiagnostics      = (1 << 14), // the diagnostic jumper pads? creates a reg dump on screen...
  RuntInterruptCardLockSwitch   = (1 << 15),
  RuntInterruptPowerSwitch      = (1 << 16),
  RuntInterruptSerial           = (1 << 17), // questionable
  RuntInterruptTablet           = (1 << 18), // FIQ - Notepad 1.0b1: RegisterInterrupt(0x40000, 0x0, 0x74790); -- looks at ticks??
  RuntInterruptSoundDMA         = (1 << 19),
  RuntInterruptUnknown5         = (1 << 24), // FIQ - Notepad 1.0b1: RegisterInterrupt(0x1000000, 0x0, 0x7435c); -- nop -- raises FIQ?
  RuntInterruptDebugCard1       = (1 << 25), // debug card?
  RuntInterruptDebugCard2       = (1 << 26), // Notepad 1.0b1: RegisterInterrupt(0x4000000, 0x0, 0x74294); -- more debug card?
};

typedef uint32_t (*lcd_get_uint32_f) (void *ext, uint32_t addr);
typedef uint32_t (*lcd_set_uint32_f) (void *ext, uint32_t addr, uint32_t val);
typedef const char * (*lcd_get_address_name_f) (void *ext, uint32_t addr);
typedef void (*lcd_step_f)(void *ext);

typedef struct runt_s {
  arm_t *arm;
  uint32_t *memory;
  time_t bootTime;
  uint32_t ticks;
  int machineType;
  
  uint32_t rtcAlarm;
  uint32_t ticksAlarm1;
  uint32_t ticksAlarm2;
  uint32_t ticksAlarm3;
  uint32_t adcSource;
  uint32_t adcLastFire;
  
  // Logging
  uint32_t logFlags;
  FILE *logFile;
  
  // Interrupts
  uint32_t interrupt;
  uint32_t interruptStick;
  
  // Display
  void               *lcd_driver;
  lcd_get_uint32_f   lcd_get_uint32;
  lcd_set_uint32_f   lcd_set_uint32;
  lcd_get_address_name_f lcd_get_address_name;
  lcd_step_f          lcd_step;
  
  // Switches
  int8_t switches[3];
  
  // Tablet
  bool touchActive;
  uint32_t touchX;
  uint32_t touchY;
} runt_t;

void runt_init (runt_t *c, int machineType);
runt_t *runt_new (int machineType);
void runt_free (runt_t *c);
void runt_del (runt_t *c);
void runt_set_arm (runt_t *c, arm_t *arm);
void runt_step(runt_t *c);

void runt_set_lcd_fct(runt_t *c, void *ext,
                      void *get32, void *set32, void *getname, void *step);

void runt_set_log_flags (runt_t *c, unsigned flags, int val);
void runt_set_log_file (runt_t *c, FILE *file);

uint32_t runt_set_mem32(runt_t *c, uint32_t addr, uint32_t val, uint32_t pc);
uint32_t runt_get_mem32(runt_t *c, uint32_t addr, uint32_t pc);

void runt_raise_interrupt(runt_t *c, uint32_t interrupt);
void runt_lower_interrupt(runt_t *c, uint32_t interrupt);

uint8_t runt_get_power_state(runt_t *c, uint32_t subsystem);

void runt_switch_state(runt_t *c, int switchNum, int state);

void runt_touch_down(runt_t *c, int x, int y);
void runt_touch_up(runt_t *c);

#endif /* defined(__Leibniz__runt__) */
