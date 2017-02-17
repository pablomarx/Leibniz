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
#include <unistd.h>

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
  RuntRTC = 0x24,
  RuntRTCAlarm = 0x28,
  RuntTimer = 0x30,
  RuntTicks = 0x34,
  RuntTicksAlarm1 = 0x38,
  RuntTicksAlarm2 = 0x40,
  RuntTicksAlarm3 = 0x48,
    
  RuntFaultRead = 0x50,
  RuntFaultWrite = 0x54,

  RuntADCValue = 0x58,
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
    
  RuntSoundDMA1Base = 0xb0,
  RuntSoundDMA1Length = 0xb4,
  RuntSoundDMA2Base = 0xc0,
  RuntSoundDMA2Length = 0xc4,
};

enum {
  RuntADCSourceNicad,
  RuntADCSourceThermistor,
  RuntADCSourceMainBattery,
  RuntADCSourceBackupBattery,

  RuntADCSourceUnknownA,
  RuntADCSourceUnknownB,
  RuntADCSourceUnknownC,
  RuntADCSourceUnknownD,

  RuntADCSourceTabletPositionX,
  RuntADCSourceTabletPositionY,
  RuntADCSourceTabletUnknownX,
  RuntADCSourceTabletUnknownY,
};

typedef struct {
  uint32_t mask;
  uint32_t test;
  uint32_t val;
  char *name;
} runt_adc_source_t;

static runt_adc_source_t runt_adc_sources[] = {
  { .mask = 0xffffffff, .test = 0x00000202, .val = RuntADCSourceUnknownA, .name = "UnknownA" },
  { .mask = 0xffffffff, .test = 0x00000402, .val = RuntADCSourceUnknownB, .name = "UnknownB" },
  { .mask = 0xffffffff, .test = 0x00000802, .val = RuntADCSourceUnknownC, .name = "UnknownC" },
  { .mask = 0xffffffff, .test = 0x00001202, .val = RuntADCSourceUnknownD, .name = "UnknownD" },

  { .mask = 0xffffffff, .test = 0x00001402, .val = RuntADCSourceNicad, .name = "Nicad" },
  { .mask = 0xffffffff, .test = 0x00003202, .val = RuntADCSourceThermistor, .name = "Thermistor" },
  { .mask = 0xffffffff, .test = 0x00003402, .val = RuntADCSourceMainBattery, .name = "MainBattery" },
  { .mask = 0xffffffff, .test = 0x00003802, .val = RuntADCSourceBackupBattery, .name = "BackupBattery" },
  
  { .mask = 0x000000ff, .test = 0x00000032, .val = RuntADCSourceTabletPositionX, .name = "TabletPositionX" },
  { .mask = 0x000000ff, .test = 0x0000000e, .val = RuntADCSourceTabletPositionY, .name = "TabletPositionY" },
  { .mask = 0x000000ff, .test = 0x0000000a, .val = RuntADCSourceTabletUnknownY, .name = "TabletUnknownY" },
  { .mask = 0x000000ff, .test = 0x000000a2, .val = RuntADCSourceTabletUnknownX, .name = "TabletUnknownX" },
};

static const char *runt_power_names[] = {
  "adc", "lcd", "vpp2", "vpp1", "sound", "serial", "trim", "tablet",
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

static const char *runt_interrupt_names[] = {
  "rtc", "ticks", "ticks2", NULL, NULL, NULL, NULL, NULL,
  NULL, "adc", "serialA", "sound", "pcmcia", "diags", "cardlock", "powerswitch",
  "serial", "tablet", "sound-dma", NULL, NULL, NULL, NULL, NULL,
  "power-fault", "battery-removed", NULL, NULL, NULL, NULL, NULL, NULL,
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
    case RuntADCValue:
      flag = RuntLogADC;
      prefix = "adc-value";
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
    case RuntFaultRead:
    case RuntFaultWrite:
      flag = RuntLogFaults;
      prefix = "fault";
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
    case RuntSoundDMA1Base:
    case RuntSoundDMA2Base:
      flag = RuntLogSound;
      prefix = "sound-dma-base";
      break;
    case RuntSoundDMA1Length:
    case RuntSoundDMA2Length:
      flag = RuntLogSound;
      prefix = "sound-dma-length";
      break;
    default:
      flag = RuntLogUnknown;
      prefix = "unknown";
      break;
  }
  
  if ((c->logFlags & flag) == flag) {
    fprintf(c->logFile, "[RUNT ASIC:%s:%02x:%s] 0x%08x => 0x%08x (PC:0x%08x, LR:0x%08x)\n", write?"WR":"RD", ((addr >> 8) & 0xff), prefix, addr, val, arm_get_pc(c->arm), arm_get_lr(c->arm));
  }
}

#pragma mark -
static inline uint32_t runt_register_get(runt_t *c, uint8_t reg) {
  return c->memory[(reg<<8)/4];
}

static inline void runt_register_set(runt_t *c, uint8_t reg, uint32_t val) {
  c->memory[(reg<<8)/4] = val;
}

#pragma mark - ADC
uint32_t runt_get_adc_source(runt_t *c) {
  return runt_register_get(c, RuntADCSource);
}

void runt_set_adc_source(runt_t *c, uint32_t val) {
  bool log = ((c->logFlags & RuntLogADC) == RuntLogADC);
  if (log == true) {
    uint32_t oldVal = runt_get_adc_source(c);
    fprintf(c->logFile, " => ADC sample: 0x%08x -> 0x%08x: ", oldVal, val);
  }
  
  
  bool matched = false;
  for (int i=0; i<countof(runt_adc_sources); i++) {
    if ((val & runt_adc_sources[i].mask) == runt_adc_sources[i].test) {
      if (log == true) {
        fprintf(c->logFile, "%s\n", runt_adc_sources[i].name);
      }
      c->adcSource = runt_adc_sources[i].val;
      matched = true;
      break;
    }
  }
  
  if (matched == false) {
    c->adcSource = -1;
    if (log == true) {
      fprintf(c->logFile, "**unknown: 0x%08x**\n", val);
    }
  }
}

uint32_t runt_get_adc_value(runt_t *c) {
  if (runt_power_state_get_subsystem(c, RuntPowerADC) == false) {
    return 0;
  }
  
  uint32_t result = 0;
  switch (c->adcSource) {
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
    case RuntADCSourceUnknownA:
      result = 0x888;
      break;
    case RuntADCSourceUnknownB:
      result = 0xccc;
      break;
    case RuntADCSourceUnknownC:
      result = 0xaaa;
      break;
    case RuntADCSourceUnknownD:
      // J1 logs:
      // the voltage =  57.15 . ADC = ccc
      // the voltage =  34.56 . ADC = 999
      // the voltage =  19.50 . ADC = 777
      // the voltage =  11.96 . ADC = 666
      // the voltage = 32764.90 . ADC = 444
      result = 0x666;
      break;
    case RuntADCSourceTabletUnknownY:
    case RuntADCSourceTabletUnknownX:
    case RuntADCSourceTabletPositionY:
    case RuntADCSourceTabletPositionX:
    {
      if (c->touchActive == true && runt_power_state_get_subsystem(c, RuntPowerTablet) == true) {
        switch (c->adcSource) {
          case RuntADCSourceTabletPositionX:
            result = 0xf15 - (c->touchX * 10);
            break;
          case RuntADCSourceTabletPositionY:
            result = 0xf25 - (c->touchY * 15);
            break;
            
            // These two values need to differ by
            // 0x03e8 in order for Newton alignment
            // to accept the positions...
          case RuntADCSourceTabletUnknownX:
            result = 0xfff;
            break;
          case RuntADCSourceTabletUnknownY:
            result = 0xc17;
            break;
        }
      }
      break;
    }
    default:
      fprintf(c->logFile, "[RUNT ASIC:RD:%02x:adc] unhandled source type:0x%08x\n", RuntADCSource, runt_get_adc_source(c));
      break;
  }
  return result;
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

bool runt_interrupt_is_enabled(runt_t *c, uint32_t interrupt) {
  uint32_t irqs = runt_register_get(c, RuntEnableInterrupt);
  return ((irqs & interrupt) == interrupt);
}

void runt_interrupt_raise(runt_t *c, uint32_t interrupt) {
  if (runt_interrupt_is_enabled(c, interrupt) == false) {
    return;
  }
  
  c->runtAwake = true;
  c->armAwake = true;
    
  if ((c->interrupt & interrupt) != interrupt) {
    c->interrupt |= interrupt;
    
    if ((c->logFlags & RuntLogInterrupts) == RuntLogInterrupts) {
      fprintf(c->logFile, "[RUNT:ASIC] Raising interrupt 0x%02x: ", interrupt);
      runt_print_description_for_interrupts(c, interrupt);
      fprintf(c->logFile, "\n");
    }
  }
  
  if (interrupt == RuntInterruptPowerFault || interrupt == RuntInterruptBatteryRemoved || interrupt == RuntInterruptSerialA) {
    arm_set_fiq(c->arm, 1);
  }
  else {
    arm_set_irq(c->arm, 1);
  }
}

void runt_interrupt_lower(runt_t *c, uint32_t interrupt) {
  if ((c->interrupt & interrupt) == interrupt) {
    c->interrupt ^= interrupt;
  }
  
  if ((c->interrupt & RuntInterruptPowerFault) == 0 && (c->interrupt & RuntInterruptBatteryRemoved) == 0 && (c->interrupt & RuntInterruptSerialA) == 0) {
    arm_set_fiq(c->arm, 0);
  }
  
  if (c->interrupt == 0x00) {
    arm_set_irq(c->arm, 0);
  }
}

void runt_print_enabled_interrupts(runt_t *c) {
  uint32_t val = runt_register_get(c, RuntEnableInterrupt);
  fprintf(c->logFile, "Enabled interrupts: ");
  runt_print_description_for_interrupts(c, val);
  fprintf(c->logFile, "\n");
}

void runt_set_enabled_interrupts(runt_t *c, uint32_t val) {
  if ((c->logFlags & RuntLogInterrupts) == RuntLogInterrupts) {
    uint32_t oldVal = runt_register_get(c, RuntEnableInterrupt);
    if (val != oldVal) {
      fprintf(c->logFile, " => enable interrupts was: 0x%08x (", oldVal);
      runt_print_description_for_interrupts(c, oldVal);
      fprintf(c->logFile, "), now: 0x%08x (", val);
      runt_print_description_for_interrupts(c, val);
      fprintf(c->logFile, ")\n");
    }
  }
}

#pragma mark - Touches
void runt_touch_down(runt_t *c, int x, int y) {
  c->touchX = x;
  c->touchY = y;
  c->touchActive = true;
}

void runt_touch_up(runt_t *c) {
  c->touchX = 0;
  c->touchY = 0;
  c->touchActive = false;
}

#pragma mark -
bool runt_cpu_paused(runt_t *c) {
  if ((runt_register_get(c, 0) & 2) == 2) {
    return false;
  }
  return (runt_register_get(c, RuntCPUControl) != 0x00);
}

void runt_cpu_state_update(runt_t *c) {
  if (runt_power_state_get_subsystem(c, RuntPowerSleep) == true) {
    c->armAwake = false;
    c->runtAwake = false;
  }
  else if (runt_cpu_paused(c) == true) {
    c->armAwake = false;
    c->runtAwake = true;
  }
  else {
    c->armAwake = true;
    c->runtAwake = true;
  }
}

void runt_cpu_control_set(runt_t *c, uint32_t val) {
  if (val == runt_register_get(c, RuntCPUControl)) {
    return;
  }

  runt_register_set(c, RuntCPUControl, val);
  runt_cpu_state_update(c);
}

#pragma mark - Power
void runt_power_state_set(runt_t *c, uint32_t val) {
  uint32_t oldVal = runt_register_get(c, RuntPower);
  if (oldVal == val) {
    return;
  }
  
  if ((c->logFlags & RuntLogPower) == RuntLogPower) {
    fprintf(c->logFile, " => power on: 0x%08x -> 0x%08x: ", oldVal, val);
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
    
  runt_register_set(c, RuntPower, val);
  
  if ((val & RuntPowerSleep) != (oldVal & RuntPowerSleep)) {
    runt_cpu_state_update(c);
  }
    
  if ((val & RuntPowerLCD) != (oldVal & RuntPowerLCD)) {
    if (c->lcd_powered != NULL) {
      c->lcd_powered(c->lcd_driver, (val & RuntPowerLCD));
    }
  }
}

void runt_power_state_set_subsystem(runt_t *c, uint32_t subsystem, bool powered) {
  uint32_t val = runt_register_get(c, RuntPowerLCD);
  val &= ~subsystem;
  if (powered == true) {
    val |= subsystem;
  }
  runt_power_state_set(c, val);
}

bool runt_power_state_get_subsystem(runt_t *c, uint32_t subsystem) {
  uint32_t val = runt_register_get(c, RuntPower);
  return ((val & subsystem) == subsystem);
}

#pragma mark -
void runt_switch_state(runt_t *c, int switchNum, int state) {
  c->switches[switchNum] = state;
  switch (switchNum) {
    case RuntSwitchNicad:
      break;
    case RuntSwitchPower:
      runt_interrupt_raise(c, RuntInterruptPowerSwitch);
      if (runt_power_state_get_subsystem(c, RuntPowerSleep) == true) {
        runt_power_state_set_subsystem(c, RuntPowerSleep, false);
        runt_cpu_control_set(c, 0);
      }
      break;
    case RuntSwitchCardLock:
      runt_interrupt_raise(c, RuntInterruptCardLockSwitch);
      break;
  }
}

#pragma mark -
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
        runt_interrupt_lower(c, val);
      }
      break;
    case RuntCPUControl:
      runt_cpu_control_set(c, val);
      break;
    case RuntSound:
      // To get diagnostics moving, we'll set the interrupt...
      runt_interrupt_raise(c, RuntInterruptSound);
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
          runt_interrupt_raise(c, RuntInterruptSerialA);
        }
      }
      break;
    case RuntADCSource:
      runt_set_adc_source(c, val);
      break;
    case RuntPower:
      runt_power_state_set(c, val);
      break;
    case RuntEnableInterrupt:
      runt_set_enabled_interrupts(c, val);
      break;
    case RuntRTC:
      c->bootTime = time(NULL);
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
    case RuntTicksAlarm3:
      c->ticksAlarm3 = val;
      break;
    case RuntSoundDMA1Length:
    case RuntSoundDMA2Length:
      runt_interrupt_raise(c, RuntInterruptSoundDMA);
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
    case RuntADCValue:
      result = runt_get_adc_value(c);
      break;
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

bool runt_step(runt_t *c) {
  if (c->runtAwake == false) {
    usleep(100);
    return false;
  }
    
  c->ticks += 5;
  
  if (c->rtcAlarm != 0 && runt_get_rtc(c) >= c->rtcAlarm) {
    runt_interrupt_raise(c, RuntInterruptRTC);
    c->rtcAlarm = 0;
  }
  
  if (c->ticksAlarm1 != 0 && runt_get_ticks(c) >= c->ticksAlarm1) {
    runt_interrupt_raise(c, RuntInterruptTicks);
    c->ticksAlarm1 = 0;
  }
  
  if (c->ticksAlarm2 != 0 && runt_get_ticks(c) >= c->ticksAlarm2) {
    runt_interrupt_raise(c, RuntInterruptTicks);
    c->ticksAlarm2 = 0;
  }
  
  if (c->ticksAlarm3 != 0 && runt_get_ticks(c) >= c->ticksAlarm3) {
    runt_interrupt_raise(c, RuntInterruptTicks2);
    c->ticksAlarm3 = 0;
  }
  
  if (c->touchActive == true) {
    runt_interrupt_raise(c, RuntInterruptTablet);
  }
  
  uint32_t sampleSource = c->adcSource;
  switch (sampleSource) {
    case RuntADCSourceUnknownA:
    case RuntADCSourceUnknownB:
    case RuntADCSourceUnknownC:
    case RuntADCSourceUnknownD:
    case RuntADCSourceNicad:
    case RuntADCSourceBackupBattery:
    case RuntADCSourceMainBattery:
    case RuntADCSourceThermistor:
    case RuntADCSourceTabletPositionX:
    case RuntADCSourceTabletPositionY:
    case RuntADCSourceTabletUnknownX:
    case RuntADCSourceTabletUnknownY:
      if (runt_interrupt_is_enabled(c, RuntInterruptADC) == true && runt_power_state_get_subsystem(c, RuntPowerADC) == true) {
        runt_interrupt_raise(c, RuntInterruptADC);
      }
      break;
  }
  
  if (c->lcd_step != NULL) {
    c->lcd_step(c->lcd_driver);
  }
    
  return c->armAwake;
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
            void *get32, void *set32, void *getname, void *step, void *powered)
{
  c->lcd_driver = ext;
  c->lcd_get_uint32 = get32;
  c->lcd_set_uint32 = set32;
  c->lcd_get_address_name = getname;
  c->lcd_step = step;
  c->lcd_powered = powered;
}

void runt_reset(runt_t *c) {
  memset(c->memory, 0, 0xffff * 4);

  c->ticks = 0;
  c->runtAwake = true;
  c->armAwake = true;
  
  c->rtcAlarm = 0;
  c->bootTime = time(NULL);
  
  c->ticksAlarm1 = 0;
  c->ticksAlarm2 = 0;
  c->ticksAlarm3 = 0;
  c->adcSource = 0;
  
  c->interrupt = 0;
  c->interruptStick = 0;
}

void runt_init (runt_t *c, int machineType) {
  c->memory = calloc(0xffff, sizeof(uint32_t));
  c->machineType = machineType;
  c->runtAwake = true;
  c->armAwake = true;
    
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
    runt_set_lcd_fct(c, squirt, lcd_squirt_get_mem32, lcd_squirt_set_mem32, lcd_squirt_get_address_name, lcd_squirt_step, NULL);
    c->lcd_driver = squirt;
  }
  else {
    lcd_sharp_t *sharp = lcd_sharp_new();
    runt_set_lcd_fct(c, sharp, lcd_sharp_get_mem32, lcd_sharp_set_mem32, lcd_sharp_get_address_name, NULL, lcd_sharp_set_powered);
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
  runt_set_log_flags(c, RuntLogPower, 0);
  runt_set_log_flags(c, RuntLogFaults, 0);
  runt_set_log_flags(c, RuntLogSerial, 0);
  runt_set_log_flags(c, RuntLogIR, 0);
  
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
