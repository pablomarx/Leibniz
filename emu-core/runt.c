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

#include "newton.h"
#include "lcd_sharp.h"
#include "lcd_squirt.h"

#define countof(__a__) (sizeof(__a__) / sizeof(__a__[0]))

enum {
  RuntGetInterrupt = 0x04,
  RuntClearInterrupt = 0x08,
  RuntEnableInterrupt = 0x0c,
  RuntPower = 0x10,
  RuntCPUControl = 0x14,
  RuntADCSource = 0x1c,
    RuntADCSourceNicad = 0x14,
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
  RuntTicksAlarm3 = 0x48,
  // 0x50: 14 reads
  // 0x54: 14 writes [these two are accessed during data aborts]
  RuntADC = 0x58,
  RuntSound1 = 0x5c, // probably not sound, but frequently seen together.
                     // 6 writes during notepad boot. vals: 0x08[2], 0x09[3], 0x0b
  RuntLCD = 0x60,
  RuntSound = 0x64,
  
  RuntIR = 0x80,
    RuntIRConfig = 0x08,
    RuntIRData   = 0x04,
  
  RuntSerial = 0x81,
    RuntSerialConfig = 0x08,
    RuntSerialData   = 0x04,
    
  RuntSoundDMABase = 0xb0,
  RuntSoundDMALength = 0xb4,
  // a8: two writes, vals: 0x00, 0x22
  // b0: one write, val: 0x0106f030 (looks like a RAM address)
  // b4: four writes, vals: 0x00[3], 0x113
  // c0: one write, val = 0x0106f030 (looks like a RAM address)
  // c4: one read, two writes, vals: 0x00, 0x113
  // c8: two writes, vals: 0x100, 0x10f 
};

/* Per 0xb0 + 0xc0 above, reading the RAM address at the time of the reg write:
newton> read 0x0106F030 128
0x0106f030: 00000000 00000000 00000000 00000000 |................|
0x0106f040: 00000000 00000000 00000000 00000000 |................|
0x0106f050: 00000000 00000000 00000000 00000000 |................|
0x0106f060: 00000000 00000000 00000000 00000000 |................|
0x0106f070: 00000000 00000000 00000000 00000000 |................|
0x0106f080: 00000000 00000000 01010101 01010101 |................|
0x0106f090: 01010101 01010101 01010101 01010101 |................|
0x0106f0a0: 01010101 01010101 01010101 02020202 |................|
*/

typedef struct {
  uint32_t index;
  const char *name;
} name_index;

static name_index runt_adc_names[] = {
  { .index = 0x14, .name = "Nicad" },
  { .index = 0x30, .name = "TabletA" },
  { .index = 0x31, .name = "TabletB" },
  { .index = 0x32, .name = "Thermistor" },
  { .index = 0x34, .name = "MainBattery" },
  { .index = 0x38, .name = "BackupBattery" },
};

static const char *runt_power_names[] = {
  "adc", "lcd", "vpp2", "vpp1", "sound", "serial", "trim", "pcmcia",
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

static const char *runt_interrupt_names[] = {
  "rtc", "ticks", "ticks2", NULL, NULL, NULL, NULL, NULL,
  NULL, "adc", "serialA", "sound", "pcmcia", "diags", "cardlock", "powerswitch",
  "serial", "tablet", NULL, NULL, NULL, NULL, NULL, NULL,
  "debugcard1", "debugcard2", NULL, NULL, NULL, NULL, NULL, NULL,
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
    case RuntEnableInterrupt:
      flag = RuntLogInterrupts;
      prefix = "enable-interrupt";
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
    case RuntTicksAlarm3:
      flag = RuntLogTicks;
      prefix = "set-ticks-alarm3";
      break;
    case RuntADC:
      flag = RuntLogADC;
      prefix = "adc";
      break;
    case RuntCPUControl:
      flag = RuntLogCPUControl;
      prefix = "cpu-control";
      break;
    case RuntLCD: {
      flag = RuntLogLCD;
      prefix = c->lcd_get_address_name(c->lcd_driver, (addr & 0xff));
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
    case RuntSoundDMABase:
      flag = RuntLogSound;
      prefix = "sound-dma-base";
      break;
    case RuntSoundDMALength:
      flag = RuntLogSound;
      prefix = "sound-dma-length";
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
void runt_print_description_for_interrupts(runt_t *c, uint32_t val) {
  for (int i=0; i<32; i++) {
    uint32_t bit = (1 << i);
    if ((val & bit) == bit) {
      const char *name = runt_interrupt_names[i-1];
      if (name != NULL) {
        fprintf(c->logFile, "%s, ", name);
      }
      else {
        fprintf(c->logFile, "unknown-%i, ", i);
      }
    }
  }
}

void runt_print_enabled_interrupts(runt_t *c) {
    uint32_t val = c->memory[(RuntEnableInterrupt << 8)/4];
    fprintf(c->logFile, "Enabled interrupts: ");
    runt_print_description_for_interrupts(c, val);
    fprintf(c->logFile, "\n");
}

void runt_raise_interrupt(runt_t *c, uint32_t interrupt) {
  if ((c->memory[0x0c00 / 4] & interrupt) == interrupt) {
    if ((c->interrupt & interrupt) != interrupt) {
      c->interrupt |= interrupt;
    }
    if ((c->logFlags & RuntLogInterrupts) == RuntLogInterrupts) {
      fprintf(c->logFile, "[RUNT:ASIC] Raising interrupt 0x%02x\n", interrupt);
    }
	if (interrupt == RuntInterruptDebugCard1 || interrupt == RuntInterruptDebugCard2 || interrupt == RuntInterruptSerialA) {
	    arm_set_fiq(c->arm, 1);
	}
	else {
	    arm_set_irq(c->arm, 1);
	}
  }
}

void runt_lower_interrupt(runt_t *c, uint32_t interrupt) {
  if (interrupt == RuntInterruptADC) {
    uint32_t sampleSource = c->adcSource;
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
  
  if ((c->interrupt & RuntInterruptDebugCard1) == 0 && (c->interrupt & RuntInterruptDebugCard2) == 0 && (c->interrupt & RuntInterruptSerialA) == 0) {
    arm_set_fiq(c->arm, 0);
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
  
  uint32_t sampleSource = c->adcSource;
  if (sampleSource == RuntADCSourceTabletA || sampleSource == RuntADCSourceTabletB) {
    runt_raise_interrupt(c, RuntInterruptADC);
  }
  runt_raise_interrupt(c, RuntInterruptTablet);
}

void runt_touch_up(runt_t *c) {
  c->touchX = 0;
  c->touchY = 0;
  c->touchActive = false;
  
  uint32_t sampleSource = c->adcSource;
  if (sampleSource == RuntADCSourceTabletA || sampleSource == RuntADCSourceTabletB) {
    runt_lower_interrupt(c, RuntInterruptADC);
  }
  runt_lower_interrupt(c, RuntInterruptTablet);
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

uint8_t runt_get_power_state(runt_t *c, uint32_t subsystem) {
    return ((c->memory[(RuntPower<<8)/4] & subsystem) == subsystem);
}

#pragma mark -
#pragma mark
uint32_t runt_get_ticks(runt_t *c) {
	return c->ticks;
}

uint32_t runt_get_rtc(runt_t *c) {
  return (uint32_t)(time(NULL) - c->bootTime);
}

uint32_t runt_set_mem32(runt_t *c, uint32_t addr, uint32_t val, uint32_t pc) {
  runt_log_access(c, addr, val, true);
  
  uint32_t localAddr = (addr / 4);
  switch ((addr >> 8) & 0xff) {
	  case 0x00:
	  // Observed bit 2 changing depending on gNewtConfig
	  // kDontPauseCPU being set...
	  if ((val & 2) == 2) {
		  fprintf(c->logFile, " => CPU pausing enabled?\n");
	  }
	  else {
		  fprintf(c->logFile, " => CPU pausing disabled?\n");
	  }
	  break;
    case RuntLCD:
      c->lcd_set_uint32(c->lcd_driver, (addr & 0xff), val);
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
    case RuntIR:
      if ((addr & 0xff) == RuntSerialData) {
        //fprintf(c->logFile, "serial_putc: 0x%02x '%c'\n", val & 0xff, val & 0xff);
      }
	  else if ((addr & 0xff) == RuntSerialConfig) {
	  	
	  }
	  else if ((addr & 0xff) == 0) {
		  if (val == 0x05) {
			  val = 0x03;
	          runt_raise_interrupt(c, RuntInterruptSerialA);
		  }
	  }
      break;
    case RuntADCSource: {
      uint8_t source = ((val >> 8) & 0xff);
      c->adcSource = source;

      if (source == RuntADCSourceBackupBattery || source == RuntADCSourceMainBattery ||
          source == RuntADCSourceThermistor || source == RuntADCSourceNicad)
      {
        runt_raise_interrupt(c, RuntInterruptADC);
      }
      else if ((source == RuntADCSourceTabletA || source == RuntADCSourceTabletB) && c->touchActive == true) {
        runt_raise_interrupt(c, RuntInterruptADC);
      }
      else {
        runt_lower_interrupt(c, RuntInterruptADC);
      }
      
      
      if ((c->logFlags & RuntLogADC) == RuntLogADC) {
        fprintf(c->logFile, " => ADC sample: 0x%08x -> 0x%08x: ", c->memory[localAddr], val);
        bool matched = false;
        for (int i=0; i<countof(runt_adc_names); i++) {
          if (runt_adc_names[i].index == source) {
            fprintf(c->logFile, "%s", runt_adc_names[i].name);
            matched = true;
            break;
          }
        }
        if (matched == false) {
          fprintf(c->logFile, "**unknown: 0x%02x**", source);
        }
        fprintf(c->logFile, "\n");
      }
      
      break;
    }
    case RuntPower:
      if ((c->logFlags & RuntLogPower) == RuntLogPower) {
        fprintf(c->logFile, " => power on: 0x%08x -> 0x%08x: ", c->memory[localAddr], val);
        for (int i=0; i<32; i++) {
          uint32_t bit = (1 << i);
          if ((val & bit) == bit) {
            const char *name = runt_power_names[i];
            if (name != NULL) {
              fprintf(c->logFile, "%s, ", name);
            }
            else {
              fprintf(c->logFile, "unknown-%i, ", i);
            }
          }
        }
        fprintf(c->logFile, "\n");
      }
      break;
    case RuntEnableInterrupt: {
      if ((c->logFlags & RuntLogInterrupts) == RuntLogInterrupts) {
        if (val != c->memory[localAddr]) {
          fprintf(c->logFile, " => enable interrupts was: 0x%08x (", c->memory[localAddr]);
          runt_print_description_for_interrupts(c, c->memory[localAddr]);
          fprintf(c->logFile, "), now: 0x%08x (", val);
          runt_print_description_for_interrupts(c, val);
          fprintf(c->logFile, ")\n");
        }
      }
      break;
    }
    case RuntRTCAlarm:
      c->rtcAlarm = val;
      break;
    case RuntTicksAlarm1:
      c->ticksAlarm1 = val;
      break;
    case RuntTicksAlarm2:
      c->ticksAlarm2 = val;
      break;
    case RuntTicksAlarm3:
      c->ticksAlarm3 = val;
      break;
    case RuntSoundDMALength:
      runt_raise_interrupt(c, RuntInterruptSoundDMA);
      break;
    default:
      break;
  }
  
  c->memory[localAddr] = val;
  return val;
}


uint32_t runt_get_mem32(runt_t *c, uint32_t addr, uint32_t pc) {
  uint32_t result = c->memory[addr / 4];
  
  switch ((addr >> 8) & 0xff) {
    case RuntLCD:
      result = c->lcd_get_uint32(c->lcd_driver, (addr & 0xff));
      break;
    case RuntTicks:
      result = runt_get_ticks(c);
      break;
    case RuntADC: {
      uint32_t sampleSource = c->adcSource;
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
        case RuntADCSourceNicad:
          // the voltage =   4.83 . ADC = 888
          // Nicad Battery Level =  60%.
          result = 0x888;
          break;
        default:
          result = 0;
          break;
      }
      
      break;
    }
    case RuntEnableInterrupt:
      break;
    case RuntGetInterrupt:
      result = c->interrupt;
      break;
    case RuntPower:
      break;
    case RuntIR:
    case RuntSerial:
      if ((addr & 0xff) == RuntSerialData) {

      }
      else if ((addr & 0xff) == RuntSerialConfig) {
		  
      }
      else {
		  result = 3;
      }
	  if ((result & 0xff) == 0x00) {
        result = 0xffffffff;
      }
      break;
    case RuntRTC:
      result = result + runt_get_rtc(c);
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
    case RuntTicksAlarm3:
      result = c->ticksAlarm3;
      break;
  }
  
  runt_log_access(c, addr, result, false);
  
  return result;
}

void runt_step(runt_t *c) {
  c->ticks += 10;
  
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

  if (c->ticksAlarm3 != 0 && runt_get_ticks(c) >= c->ticksAlarm3) {
    runt_raise_interrupt(c, RuntInterruptTicks2);
    c->ticksAlarm3 = 0;
  }

  if (c->lcd_step != NULL) {
    c->lcd_step(c->lcd_driver);
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
                      void *get32, void *set32, void *getname, void *step)
{
  c->lcd_driver = ext;
  c->lcd_get_uint32 = get32;
  c->lcd_set_uint32 = set32;
  c->lcd_get_address_name = getname;
  c->lcd_step = step;
}

void runt_init (runt_t *c, int machineType) {
  c->memory = calloc(0xffff, 1);
  c->machineType = machineType;
  
  //
  // Tablet
  //
  c->touchActive = false;
  c->touchX = 0;
  c->touchY = 0;
  
  //
  // LCD
  //
  if (machineType == kGestalt_MachineType_Lindy) {
    lcd_squirt_t *squirt = lcd_squirt_new();
    runt_set_lcd_fct(c, squirt, lcd_squirt_get_mem32, lcd_squirt_set_mem32, lcd_squirt_get_address_name, lcd_squirt_step);
    c->lcd_driver = squirt;
  }
  else {
    lcd_sharp_t *sharp = lcd_sharp_new();
    runt_set_lcd_fct(c, sharp, lcd_sharp_get_mem32, lcd_sharp_set_mem32, lcd_sharp_get_address_name, NULL);
    c->lcd_driver = sharp;
  }

  
  //
  // Logging
  //
  c->logFile = stdout;
  runt_set_log_flags(c, RuntLogAll, 1);
  runt_set_log_flags(c, RuntLogTicks, 0);
  runt_set_log_flags(c, RuntLogInterrupts, 0);
  runt_set_log_flags(c, RuntLogADC, 0);
  runt_set_log_flags(c, RuntLogLCD, 0);
  runt_set_log_flags(c, RuntLogSound, 0);
  runt_set_log_flags(c, RuntLogRTC, 0);
  runt_set_log_flags(c, RuntLogCPUControl, 0);
  
  c->bootTime = time(NULL);
}

runt_t *runt_new (int machineType) {
  runt_t *c;
  
  c = calloc(1, sizeof (runt_t));
  if (c == NULL) {
    return (NULL);
  }
  
  runt_init (c, machineType);
  
  return (c);
}

void runt_set_arm(runt_t *c, arm_t *arm) {
  c->arm = arm;
}

void runt_free (runt_t *c) {
  free(c->memory);
  
  if (c->lcd_driver != NULL) {
    if (c->machineType == kGestalt_MachineType_Lindy) {
      lcd_squirt_del((lcd_squirt_t *)c->lcd_driver);
    }
    else {
      lcd_sharp_del((lcd_sharp_t *)c->lcd_driver);
    }
  }
}

void runt_del (runt_t *c) {
  if (c != NULL) {
    runt_free (c);
    free (c);
  }
}
