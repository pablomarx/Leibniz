//
//  runt.c
//  Leibniz
//
//  Created by Steve White on 9/13/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#include "runt.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  RuntGetInterrupt = 0x04,
  RuntClearInterrupt = 0x08,
  RuntSwitches = 0x0c,
  RuntPower = 0x10,
    RuntPowerTrim = 0x40,
    RuntPowerSerial = 0x20,
    RuntPowerSound = 0x10,
    RuntPowerVPP1 = 0x08,
    RuntPowerVPP2 = 0x04,
    RuntPowerLCD = 0x02,
    RuntPowerADC = 0x01,
  RuntFlooder = 0x1c,
  RuntRTC = 0x24,
  RuntRTCAlarm = 0x28,
  RuntTimer = 0x30,
  RuntTicks = 0x34,
  RuntTicksAlarm = 0x38,
  RuntSound1 = 0x5c, // probably not sound, but frequently seen together.
  RuntADC = 0x58,
  RuntLCD = 0x60,
  RuntSound = 0x64,
  
  RuntIR = 0x80,
    RuntIRConfig = 0x08,
    RuntIRData   = 0x04,
  
  RuntSerial = 0x81,
    RuntSerialConfig = 0x08,
    RuntSerialData   = 0x04,
};

void runt_log_access(runt_t *c, uint32_t addr, uint32_t val, bool write) {
  const char *prefix = NULL;
  uint32_t flag = 0;
  
  switch ((addr >> 8) & 0xff) {
    case RuntGetInterrupt:
      flag = RuntLogInterrupts;
      prefix = "get-interrupt";
      break;
    case RuntClearInterrupt:
      flag = RuntLogInterrupts;
      prefix = "clear-interrupt";
      break;
    case RuntFlooder:
      // don't know, but it happens a ton...
      return;
    case RuntTimer:
      flag = RuntLogTimer;
      prefix = "timer";
      break;
    case RuntTicks:
      flag = RuntLogTicks;
      prefix = "get-ticks";
      break;
    case RuntTicksAlarm:
      flag = RuntLogTicks;
      prefix = "set-ticks-alarm";
      break;
    case RuntADC:
      flag = RuntLogADC;
      prefix = "adc";
      break;
    case RuntLCD: {
      flag = RuntLogLCD;
      prefix = c->lcd_get_address_name(c->lcd_ext, (addr & 0xff));
      break;
    }
    case RuntIR:
      flag = RuntLogIR;
      switch (addr & 0xff) {
        case RuntIRConfig:
          prefix = "ir-config";
          break;
        case RuntIRData:
          prefix = "ir-data";
          break;
        default:
          prefix = "ir-unknown";
          break;
      }
      break;
    case RuntSerial:
      flag = RuntLogSerial;
      switch (addr & 0xff) {
        case RuntSerialConfig:
          prefix = "serial-config";
          break;
        case RuntSerialData:
          prefix = "serial-data";
          break;
        default:
          prefix = "serial-unknown";
          break;
      }
      break;
    case RuntPower:
      flag = RuntLogPower;
      prefix = "power";
      break;
    case RuntSwitches:
      flag = RuntLogSwitch;
      prefix = "switch";
      break;
    case RuntSound1:
    case RuntSound:
      flag = RuntLogSound;
      prefix = "sound";
      break;
    case RuntRTC:
      flag = RuntLogRTC;
      prefix = "get-rtc";
      break;
    case RuntRTCAlarm:
      flag = RuntLogRTC;
      prefix = "set-rtc-alarm";
      break;
    default:
      flag = RuntLogUnknown;
      prefix = "unknown";
      break;
  }
  
  if ((c->logFlags & flag) == flag) {
    fprintf(c->logFile, "[RUNT ASIC:%s:%02x:%s] 0x%08x => 0x%08x (PC:0x%08x)\n", write?"WR":"RD", ((addr >> 8) & 0xff), prefix, addr, val, arm_get_pc(c->arm));
  }
}

#pragma mark -
#pragma mark Touches
void runt_touch_down(runt_t *c, int x, int y) {
  c->touchX = x;
  c->touchY = y;
  c->touchActive = true;
  
  c->interrupt |= RuntInterruptADC;
}

void runt_touch_up(runt_t *c) {
  c->touchX = 0;
  c->touchY = 0;
  c->touchActive = false;
  
  c->interrupt ^= RuntInterruptADC;
}

void runt_switch_state(runt_t *c, int switchNum, int state) {
  c->switches[switchNum] = state;
}

#pragma mark -
#pragma mark
uint32_t runt_get_ticks(runt_t *c) {
  return arm_get_opcnt(c->arm) * 1000;
}

uint32_t runt_get_rtc(runt_t *c) {
  return (uint32_t)(time(NULL) - c->bootTime);
}

uint32_t runt_set_mem32(runt_t *c, uint32_t addr, uint32_t val) {
  runt_log_access(c, addr, val, true);
  
  switch ((addr >> 8) & 0xff) {
    case RuntLCD:
      c->lcd_set_uint32(c->lcd_ext, (addr & 0xff), val);
      break;
    case RuntClearInterrupt:
      if (c->interruptStick > 0) {
        c->interruptStick--;
      }
      else {
        bool log = ((c->logFlags & RuntLogInterrupts) == RuntLogInterrupts);
        if (val != RuntInterruptADC && val != 0x00040000 && val != 0x00001000 && val != 0x00008000 && val != 0x00010000) {
          if (log) fprintf(c->logFile, "clearing interrupt: 0x%08x\n", val);
        }
        if ((c->interrupt & val) == val) {
          if (log) fprintf(c->logFile, " c->interrupt was: 0x%08x, ", c->interrupt);
          c->interrupt ^= val;
          if (log) fprintf(c->logFile, " c->interrupt now: 0x%08x \n", c->interrupt);
        }
      }
      break;

    case RuntSound:
      // To get diagnostics moving, we'll set the interrupt...
      c->interrupt |= RuntInterruptSound;
      break;
    
    case RuntSerial:
      if ((addr & 0xff) == RuntSerialData) {
        //fprintf(c->logFile, "serial_putc: 0x%02x '%c'\n", val & 0xff, val & 0xff);
      }
      break;
    case RuntIR:
      if ((addr & 0xff) == RuntIRData) {
        //fprintf(c->logFile, "ir_putc: 0x%02x '%c'\n", val & 0xff, val & 0xff);
      }
      break;
    case RuntPower:
      if ((c->logFlags & RuntLogPower) == RuntLogPower) {
        
        fprintf(c->logFile, "power on: 0x%08x -> 0x%08x: ", c->memory[0x1000/4], val);
        if (val & RuntPowerTrim) {
          fprintf(c->logFile, "trim, ");
        }
        if (val & RuntPowerSerial) {
          fprintf(c->logFile, "serial, ");
        }
        if (val & RuntPowerSound) {
          fprintf(c->logFile, "sound, ");
        }
        if (val & RuntPowerVPP1) {
          fprintf(c->logFile, "vpp1, ");
        }
        if (val & RuntPowerVPP2) {
          fprintf(c->logFile, "vpp2, ");
        }
        if (val & RuntPowerLCD) {
          fprintf(c->logFile, "lcd, ");
        }
        if (val & RuntPowerADC) {
          fprintf(c->logFile, "adc, ");
        }
        fprintf(c->logFile, "\n");
      }
      break;
    default:
      break;
  }
  
  c->memory[(addr - c->base) / 4] = val;
  return val;
}


uint32_t runt_get_mem32(runt_t *c, uint32_t addr) {
  uint32_t result = c->memory[(addr - c->base) / 4];
  
  switch ((addr >> 8) & 0xff) {
    case RuntLCD:
      result = c->lcd_get_uint32(c->lcd_ext, (addr & 0xff));
      break;
    case RuntTicks:
      result = runt_get_ticks(c);
      break;
    case RuntADC:
      if (((result >> 24) & 0xff) == 0x04) {
        result = 0xfff - c->touchY;
      }
      else {
        result = 0xfff - c->touchX;
      }
      break;
    case RuntGetInterrupt:
      if (c->touchActive) {
        c->interrupt |= RuntInterruptADC;
      }
      if (c->switches[1]) {
        c->interrupt |= RuntInterruptCardLockSwitch;
      }
      if (c->switches[2]) {
        c->interrupt |= RuntInterruptPowerSwitch;
      }
      result = c->interrupt;
      break;
    case RuntPower:
      break;
    case RuntSwitches:
      result = 0;
      if (c->switches[0]) result |= 0x00000000; // nicd switch?
      if (c->switches[1]) result |= 0x00008000; // card lock switch?
      if (c->switches[2]) result |= 0x00010000; // power switch?
      break;
    case RuntFlooder:
      result = 0;
      break;
    case RuntIR:
    case RuntSerial:
      if ((result & 0xff) == 0x00) {
        result = 0xffffffff;
      }
      break;
    case RuntRTC:
      result = runt_get_rtc(c);
      break;
    default:
      fprintf(c->logFile, "unknown read: addr=0x%08x, PC=0x%08x...\n", addr, arm_get_pc(c->arm));
      break;
  }
  
  if (result == addr) {
    result = 0;
  }
  
  runt_log_access(c, addr, result, false);
  
  return result;
}

void runt_step(runt_t *c) {
  uint32_t rtc = c->memory[0x2800 / 4];
  if (rtc != 0 && runt_get_rtc(c) >= rtc) {
    if ((c->interrupt & RuntInterruptRTC) != RuntInterruptRTC) {
      c->interrupt |= RuntInterruptRTC;
    }
    c->memory[0x2800 / 4] = 0;
  }
  
  uint32_t ticks = c->memory[0x3800 / 4];
#define RuntInterruptTicks (1 << 4)
  if (ticks != 0 && runt_get_ticks(c) >= ticks) {
    if ((c->interrupt & RuntInterruptTicks) != RuntInterruptTicks) {
      c->interrupt |= RuntInterruptTicks;
    }
    c->memory[0x3800 / 4] = 0;
  }
}

#pragma mark -
#pragma mark Logging
void runt_set_log_flags (runt_t *c, unsigned flags, int val) {
  if (val) {
    c->logFlags |= flags;
  }
  else {
    c->logFlags &= ~flags;
  }
}

void runt_set_log_file (runt_t *c, FILE *file) {
  c->logFile = file;
}

#pragma mark -
#pragma mark
void runt_set_lcd_fct(runt_t *c, void *ext,
                      void *get32, void *set32, void *getname)
{
  c->lcd_ext = ext;
  c->lcd_get_uint32 = get32;
  c->lcd_set_uint32 = set32;
  c->lcd_get_address_name = getname;
}

void runt_init (runt_t *c) {
  c->base = 0x01400000;
  c->memory = calloc(0x8fff, 1);
  
  //
  // Tablet
  //
  c->touchActive = false;
  c->touchX = 0;
  c->touchY = 0;
  
  //
  // Logging
  //
  c->logFile = stdout;
  runt_set_log_flags(c, RuntLogAll, 1);
  runt_set_log_flags(c, RuntLogTicks, 0);
  runt_set_log_flags(c, RuntLogInterrupts, 0);
  runt_set_log_flags(c, RuntLogADC, 0);
  runt_set_log_flags(c, RuntLogLCD, 0);
  runt_set_log_flags(c, RuntLogSwitch, 0);
  
  c->bootTime = time(NULL);
}

runt_t *runt_new (void) {
  runt_t *c;
  
  c = calloc(1, sizeof (runt_t));
  if (c == NULL) {
    return (NULL);
  }
  
  runt_init (c);
  
  return (c);
}

void runt_set_arm(runt_t *c, arm_t *arm) {
  c->arm = arm;
}

void runt_free (runt_t *c) {
  free(c->memory);
  
}

void runt_del (runt_t *c) {
  if (c != NULL) {
    runt_free (c);
    free (c);
  }
}
