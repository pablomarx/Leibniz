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
#include <sys/time.h>

enum {
  RuntGetInterrupt = 0x04,
  RuntClearInterrupt = 0x08,
  RuntSetInterrupt = 0x0c,
  RuntPower = 0x10,
    RuntPowerTrim = 0x40,
    RuntPowerSerial = 0x20,
    RuntPowerSound = 0x10,
    RuntPowerVPP1 = 0x08,
    RuntPowerVPP2 = 0x04,
    RuntPowerLCD = 0x02,
    RuntPowerADC = 0x01,
  RuntADCSource = 0x1c,
    RuntADCSourceTabletA = 0x30,
    RuntADCSourceTabletB = 0x31,
    RuntADCSourceThermistor = 0x32,
    RuntADCSourceMainBattery = 0x34,
    RuntADCSourceBackupBattery = 0x38,
  
  RuntRTC = 0x24,
  RuntRTCAlarm = 0x28,
  RuntTimer = 0x30,
  RuntTicks = 0x34,
  RuntTicksAlarm1 = 0x38,
  RuntTicksAlarm2 = 0x40,
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
    case RuntSetInterrupt:
      flag = RuntLogInterrupts;
      prefix = "set-interrupt";
      break;
    case RuntADCSource:
      prefix = "adc-source";
      flag = RuntLogADC;
      break;
    case RuntTimer:
      flag = RuntLogTimer;
      prefix = "timer";
      break;
    case RuntTicks:
      flag = RuntLogTicks;
      prefix = "get-ticks";
      break;
    case RuntTicksAlarm1:
      flag = RuntLogTicks;
      prefix = "set-ticks-alarm";
      break;
    case RuntTicksAlarm2:
      flag = RuntLogTicks;
      prefix = "set-ticks-alarm2";
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

#pragma mark - Interrupts
void runt_raise_interrupt(runt_t *c, uint32_t interrupt) {
  if ((c->interrupt & interrupt) != interrupt) {
    c->interrupt |= interrupt;
  }
  
  if ((c->memory[0x0c00 / 4] & interrupt) == interrupt) {
    arm_set_irq(c->arm, 1);
  }
}

void runt_lower_interrupt(runt_t *c, uint32_t interrupt) {
  if (interrupt == RuntInterruptADC) {
    uint32_t sampleSource = (c->memory[0x1c00 / 4] >> 8);
    if (sampleSource == RuntADCSourceBackupBattery || sampleSource == RuntADCSourceMainBattery || sampleSource == RuntADCSourceThermistor) {
      return;
    }
    else if (sampleSource == RuntADCSourceTabletA || sampleSource == RuntADCSourceTabletB) {
      if (c->touchActive == true) {
        return;
      }
    }
  }
  else if (interrupt == RuntInterruptCardLockSwitch && c->switches[1] == 1) {
    return;
  }
  else if (interrupt == RuntInterruptPowerSwitch && c->switches[2] == 1) {
    return;
  }
  
  if ((c->interrupt & interrupt) == interrupt) {
    c->interrupt ^= interrupt;
  }
  
  if (c->interrupt == 0x00) {
    arm_set_irq(c->arm, 0);
  }
}

#pragma mark - Touches
void runt_touch_down(runt_t *c, int x, int y) {
  c->touchX = x;
  c->touchY = y;
  c->touchActive = true;
  
  runt_raise_interrupt(c, RuntInterruptADC);
}

void runt_touch_up(runt_t *c) {
  c->touchX = 0;
  c->touchY = 0;
  c->touchActive = false;
  
  runt_lower_interrupt(c, RuntInterruptADC);
}

void runt_switch_state(runt_t *c, int switchNum, int state) {
  uint32_t interrupt = 0;
  switch (switchNum) {
    case 0:
      break;
    case 1:
      interrupt = RuntInterruptCardLockSwitch;
      break;
    case 2:
      interrupt = RuntInterruptPowerSwitch;
      break;
  }
  
  if (state == 1) {
    runt_raise_interrupt(c, interrupt);
  }
  else {
    runt_lower_interrupt(c, interrupt);
  }
  c->switches[switchNum] = state;
}

#pragma mark -
#pragma mark
uint32_t runt_get_ticks(runt_t *c) {
  struct timeval now;
  (void) gettimeofday( &now, NULL );
  uint32_t theResult = (uint32_t) now.tv_sec * 3686400;
  theResult += (uint32_t)((((uint64_t)now.tv_usec) * 36864) / 10000);
  return theResult;
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
        runt_lower_interrupt(c, val);
      }
      break;
      
    case RuntSound:
      // To get diagnostics moving, we'll set the interrupt...
      runt_raise_interrupt(c, RuntInterruptSound);
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
    case RuntADCSource: {
      uint8_t source = ((val >> 8) & 0xff);
      if (source == RuntADCSourceBackupBattery || source == RuntADCSourceMainBattery || source == RuntADCSourceThermistor) {
        runt_raise_interrupt(c, RuntInterruptADC);
      }
      else if ((source == RuntADCSourceTabletA || source == RuntADCSourceTabletB) && c->touchActive == true) {
        runt_raise_interrupt(c, RuntInterruptADC);
      }
      else {
        runt_lower_interrupt(c, RuntInterruptADC);
      }
      break;
    }
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
    case RuntRTCAlarm:
      c->rtcAlarm = val;
      break;
    case RuntTicksAlarm1:
      c->ticksAlarm1 = val;
      break;
    case RuntTicksAlarm2:
      c->ticksAlarm2 = val;
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
    case RuntADC: {
      uint32_t sampleSource = (c->memory[0x1c00 / 4] >> 8);
      switch (sampleSource) {
        case RuntADCSourceTabletA:
        case RuntADCSourceTabletB:
          if (((result >> 24) & 0xff) == 0x04) {
            result = 0xfff - c->touchY;
          }
          else {
            result = 0xfff - c->touchX;
          }
          break;
        case RuntADCSourceMainBattery:
          // AD Main Battery, 0xaba = 6.0, 0xaca = 6.1, 0xa9a = 5.9
          // AutoPWB on Notepad 1.0b1 wants ~ 0x8ba.
          // AutoPWB on J1 & omp1.3 images wants ~ 0xaba
          result = 0x8ba;
          break;
        case RuntADCSourceBackupBattery:
          // backup battery, 0xfc3 = 4.8, 0xfc7 = 4.9, 0x986 = 2.9, 0x686 = 2.0, 0x486 = 1.4
          result = 0x986;
          break;
        case RuntADCSourceThermistor:
          result = 0x765; // 19.0
          break;
        default:
          result = 0;
          break;
      }
      
      break;
    }
    case RuntSetInterrupt:
      break;
    case RuntGetInterrupt:
      result = c->interrupt;
      break;
    case RuntPower:
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
    case RuntRTCAlarm:
      result = c->rtcAlarm;
      break;
    case RuntTicksAlarm1:
      result = c->ticksAlarm1;
      break;
    case RuntTicksAlarm2:
      result = c->ticksAlarm2;
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
  if (c->rtcAlarm != 0 && runt_get_rtc(c) >= c->rtcAlarm) {
    runt_raise_interrupt(c, RuntInterruptRTC);
    c->rtcAlarm = 0;
  }
  
  if (c->ticksAlarm1 != 0 && runt_get_ticks(c) >= c->ticksAlarm1) {
    runt_raise_interrupt(c, RuntInterruptTicks);
    c->ticksAlarm1 = 0;
  }
  
  if (c->ticksAlarm2 != 0 && runt_get_ticks(c) >= c->ticksAlarm2) {
    runt_raise_interrupt(c, RuntInterruptTicks);
    c->ticksAlarm2 = 0;
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
