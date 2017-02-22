//
//  runt.c
//  Leibniz
//
//  Created by Steve White on 9/13/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#include "runt.h"

#include <ctype.h>
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
    RuntIRConfig = 0x0b,
    RuntIRTxByte   = 0x07,
  
  RuntSerial = 0x81,
    RuntSerialConfig = 0x0b,
    RuntSerialTxByte = 0x07,
    
  RuntSoundDMA1Base = 0xb0,
  RuntSoundDMA1Length = 0xb4,
  RuntSoundDMA2Base = 0xc0,
  RuntSoundDMA2Length = 0xc4,
};

enum {
  RuntSerialRegTxEmpty = 0x00,
  RuntSerialRegRxEmpty = 0x01,
  RuntSerialRegParityStopBits = 0x04,
  RuntSerialRegTxError = 0x08,
  RuntSerialRegBaud = 0x0c,
};

enum {
  RuntADCSourceThermistor,
  RuntADCSourceMainBattery,
  RuntADCSourceBackupBattery,

  RuntADCSourceTabletPositionX,
  RuntADCSourceTabletPositionY,
  RuntADCSourceTabletPressureX,
  RuntADCSourceTabletPressureY,
};

typedef struct {
  uint32_t mask;
  uint32_t test;
  uint32_t val;
  char *name;
} runt_adc_source_t;

static runt_adc_source_t runt_adc_sources[] = {
  { .mask = 0xffff0fff, .test = 0x00000402, .val = RuntADCSourceMainBattery, .name = "MainBattery" },
  { .mask = 0xffff0fff, .test = 0x00000802, .val = RuntADCSourceBackupBattery, .name = "BackupBattery" },
  { .mask = 0xffff0fff, .test = 0x00000202, .val = RuntADCSourceThermistor, .name = "Thermistor" },

  { .mask = 0x000000ff, .test = 0x00000032, .val = RuntADCSourceTabletPositionX, .name = "TabletPositionX" },
  { .mask = 0x000000ff, .test = 0x0000000e, .val = RuntADCSourceTabletPositionY, .name = "TabletPositionY" },
  { .mask = 0x000000ff, .test = 0x0000000a, .val = RuntADCSourceTabletPressureY, .name = "TabletPressureY" },
  { .mask = 0x000000ff, .test = 0x000000a2, .val = RuntADCSourceTabletPressureX, .name = "TabletPressureX" },
};

static const char *runt_power_names[] = {
  "adc", "lcd", "vpp2", "vpp1", "sound", "serial", "trim", "tablet",
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

static const char *runt_interrupt_names[] = {
  "Clock", "Alarm", "Timer1", "Timer3", "Timer2", "SoundDMA", "SerialDMATx",
  "SerialDMARx", "IRDMATx", "IRDMARx", "ADConverter", "Serial", "Sound", "Tric",
  "BatteryCover", "CardLock", "SysPower", "GeneralPurpose", "Tablet", "SoundDMAErr", "SerialDMATxErr",
  "SerialMARxErr", "IRDMATxErr", "IRDMARxErr", "Timer0", "VCCFault", "BatteryRemoved",
};

#pragma mark -

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
        case RuntIRTxByte:
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
        case RuntSerialTxByte:
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
      
      // J1 logs:
      // the voltage =   4.83 . ADC = 888
      // Nicad Battery Level =  60%.
      // the voltage =   4.94 . ADC = 8ba
      // Nicad Battery Level =  80%.
      result = 0x8ba;
      break;
    case RuntADCSourceBackupBattery:
      // backup battery, 0xfc3 = 4.8, 0xfc7 = 4.9, 0x986 = 2.9, 0x686 = 2.0, 0x486 = 1.4
      // the voltage =   2.95 . ADC = 986
      result = 0x986;
      break;
    case RuntADCSourceThermistor:
      //result = 0x666; // 11c, 53f
      result = 0x765; // 19c, 66f
      break;
    case RuntADCSourceTabletPressureY:
    case RuntADCSourceTabletPressureX:
    case RuntADCSourceTabletPositionY:
    case RuntADCSourceTabletPositionX:
    {
      if (c->touchActive == true && runt_power_state_get_subsystem(c, RuntPowerTablet) == true) {
        switch (c->adcSource) {
          case RuntADCSourceTabletPositionX:
            result = 0xf30 - (c->touchX * 9.6);
            break;
          case RuntADCSourceTabletPositionY:
            result = 0xef0 - (c->touchY * 14.6);
            break;
            
            // These two values need to differ by
            // 0x03e8 in order for Newton alignment
            // to accept the positions...
          case RuntADCSourceTabletPressureX:
            result = 0xfff;
            break;
          case RuntADCSourceTabletPressureY:
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
      const char *name = runt_interrupt_names[i];
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
  
  if (interrupt == RuntInterruptVCCFault || interrupt == RuntInterruptBatteryRemoved || interrupt == RuntInterruptSerial) {
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
  
  if ((c->interrupt & RuntInterruptVCCFault) == 0 && (c->interrupt & RuntInterruptBatteryRemoved) == 0 && (c->interrupt & RuntInterruptSerial) == 0) {
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

#pragma mark - Switches
void runt_switch_set_state(runt_t *c, int switchNum, int state) {
  c->switches[switchNum] = state;
  switch (switchNum) {
    case RuntSwitchNicad:
      break;
    case RuntSwitchPower:
      runt_interrupt_raise(c, RuntInterruptSysPower);
      if (runt_power_state_get_subsystem(c, RuntPowerSleep) == true) {
        runt_power_state_set_subsystem(c, RuntPowerSleep, false);
        runt_cpu_control_set(c, 0);
      }
      break;
    case RuntSwitchCardLock:
      runt_interrupt_raise(c, RuntInterruptCardLock);
      break;
  }
}

void runt_switch_toggle(runt_t *c, int switchNum) {
  runt_switch_set_state(c, switchNum, !c->switches[switchNum]);
}

#pragma mark - Serial
static inline bool runt_serial_should_log_port(runt_t *c, uint8_t port) {
  if (port == 0) {
    return ((c->logFlags & RuntLogIR) == RuntLogIR);
  }
  else {
    return ((c->logFlags & RuntLogSerial) == RuntLogSerial);
  }
}

const char *runt_serial_get_baud_description(uint8_t baud) {
  // Observed via Lindy diags, Evaluation -> Serial Test
  // Speeds greater than 57600 started writing to other
  // serial registers (e.g. 0x01408xYY, where YY is:
  // 0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c)
  switch (baud) {
    case 0x7d: return "300";
    case 0xbd: return "600";
    case 0x5e: return "1200";
    case 0x2e: return "2400";
    case 0x16: return "4800";
    case 0x0a: return "9600";
    case 0x04: return "19200";
    case 0x01: return "38400";
    case 0x00: return "57600";
    default: return "unknown!";
  }
}

const char *runt_serial_get_parity_description(uint8_t val) {
  // Observed via Lindy diags, Evaluation -> Serial Test
  switch (val) {
    case 0x44: return "parity: none, stop bits: 1";
    case 0x45: return "parity: odd,  stop bits: 1";
    case 0x47: return "parity: even, stop bits: 1";
    case 0x4c: return "parity: none, stop bits: 2";
    case 0x4d: return "parity: odd,  stop bits: 2";
    case 0x4f: return "parity: even, stop bits: 2";
    default: return "unknown!";
  }
}

void runt_serial_raise_interrupt_for_port(runt_t *c, uint8_t port) {
  runt_interrupt_raise(c, RuntInterruptSerial);
}

uint8_t runt_serial_port_get_config(runt_t *c, uint8_t port) {
  uint8_t result = 0;
  
  runt_serial_port_t *config = NULL;
  if (port == 0) {
    config = c->irPort;
  }
  else {
    config = c->serialPort;
  }
  
  uint8_t loadedReg = config->loadedReg;
  if (config->state != 1 && runt_serial_should_log_port(c, port) == true) {
    fprintf(c->logFile, "[%s:CONFIG:GETVAL] Unexpected state %i, defaulting to reg 0\n", port ? "SERIAL" : "IR", config->state);
    loadedReg = 0;
  }
  
  if (loadedReg > countof(config->registers)) {
    fprintf(c->logFile, "[%s:CONFIG:GETVAL] Bad loaded register #%i\n", port ? "SERIAL" : "IR", loadedReg);
  }
  else {
    result = config->registers[loadedReg];
  }
  
  // If these return 0, diags will angrily loop
  // Perhaps they're tx/rx buffer empty?
  if (loadedReg == RuntSerialRegTxEmpty || loadedReg == RuntSerialRegRxEmpty) {
    if (result == 0) {
      config->registers[loadedReg] = 0xff;
    }
  }
  
  if (runt_serial_should_log_port(c, port) == true) {
    fprintf(c->logFile, "[%s:CONFIG:GETVAL] reg:0x%02x => 0x%02x\n", port ? "SERIAL" : "IR", loadedReg, result);
  }
  
  
  config->state = 0;
  return result;
}

uint32_t runt_serial_get_value(runt_t *c, uint32_t addr) {
  uint32_t result = 0;
  uint8_t reg = (addr & 0xff);
  uint8_t port = ((addr >> 8) & 0xf);

  if (reg == RuntSerialTxByte) {
    if (runt_serial_should_log_port(c, port) == true) {
      fprintf(c->logFile, "[%s:TXBYTE] Unexpected read from TX register\n", port ? "SERIAL" : "IR");
    }
  }
  else if (reg == RuntSerialConfig) {
    result = runt_serial_port_get_config(c, port);
  }
  else {
    if ((addr & 3) == 0) {
      result = c->memory[(addr - 0x01400000) / 4];
    }
  }

  return result;
}

uint32_t runt_serial_port_set_config(runt_t *c, uint8_t port, uint8_t val) {
  runt_serial_port_t *config = NULL;
  if (port == 0) {
    config = c->irPort;
  }
  else {
    config = c->serialPort;
  }
  
  // Loading a register
  if (config->state == 0) {
    config->loadedReg = val;
    config->state = 1;
    if (runt_serial_should_log_port(c, port) == true) {
      fprintf(c->logFile, "[%s:CONFIG:LOADREG] reg:0x%02x ", port ? "SERIAL" : "IR", val);
      switch (config->loadedReg) {
        case RuntSerialRegBaud:
          fprintf(c->logFile, "baud rate?");
          break;
        case RuntSerialRegParityStopBits:
          fprintf(c->logFile, "parity/stop bits ?");
          break;
        case RuntSerialRegTxEmpty:
          fprintf(c->logFile, "tx buffer empty ?");
          break;
        case RuntSerialRegRxEmpty:
          fprintf(c->logFile, "rx buffer empty ?");
          break;
        case RuntSerialRegTxError:
          fprintf(c->logFile, "tx error ?");
          break;
      }
      fprintf(c->logFile, "\n");
    }
    return val;
  }

  // Setting a loaded register's value
  config->state = 0;
  uint8_t loadedReg = config->loadedReg;
  
  if (loadedReg > countof(config->registers)) {
    fprintf(c->logFile, "[%s:CONFIG:SETVAL] Bad loaded register #%i\n", port ? "SERIAL" : "IR", config->loadedReg);
  }
  else {
    config->registers[loadedReg] = val;
  }
  
  if (runt_serial_should_log_port(c, port) == true) {
    fprintf(c->logFile, "[%s:CONFIG:SETVAL] reg:0x%02x => 0x%02x ", port ? "SERIAL" : "IR", loadedReg, val);
    
    switch (loadedReg) {
      case RuntSerialRegBaud:
        fprintf(c->logFile, "baud rate? %s", runt_serial_get_baud_description(val));
        break;
      case RuntSerialRegParityStopBits:
        fprintf(c->logFile, "%s ?", runt_serial_get_parity_description(val));
        break;
    }
    
    fprintf(c->logFile, "\n");
  }
  
  return val;
}

uint8_t runt_serial_port_transmit(runt_t *c, uint8_t port, uint8_t val) {
  if (runt_serial_should_log_port(c, port) == true) {
    fprintf(c->logFile, "[%s:TXBYTE] 0x%02x = '%c'\n", port ? "SERIAL" : "IR", val, isalnum(val) ? val : ' ');
  }
  
  runt_serial_raise_interrupt_for_port(c, port);
  
  runt_serial_port_t *config = NULL;
  if (port == 0) {
    config = c->irPort;
  }
  else {
    config = c->serialPort;
  }

  // We'll claim to be full, which runt_serial_port_get_config()
  // will return once, before flipping it back to empty.
  config->registers[RuntSerialRegTxEmpty] = 0x00;
  
  return val;
}

uint32_t runt_serial_set_value(runt_t *c, uint32_t addr, uint32_t val) {
  uint8_t reg = (addr & 0xff);
  uint8_t port = ((addr >> 8) & 0xf);
  uint8_t byteVal = (val & 0xff);

  if (reg == RuntSerialTxByte) {
    return runt_serial_port_transmit(c, port, val);
  }
  else if (reg == RuntSerialConfig) {
    return runt_serial_port_set_config(c, port, byteVal);
  }
  else if (reg == 0) {

  }
  
  return val;
}

#pragma mark - Clocks
uint32_t runt_get_ticks(runt_t *c) {
  return c->ticks;
}

uint32_t runt_get_rtc(runt_t *c) {
  return (uint32_t)(time(NULL) - c->bootTime);
}

#pragma mark - Memory access
uint32_t runt_set_mem32(runt_t *c, uint32_t addr, uint32_t val, uint32_t pc) {
  runt_log_access(c, addr, val, true);
  
  uint32_t localAddr = ((addr - 0x01400000) / 4);
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
      fprintf(stderr, "Unsupported word set for Runt LCD (0x%08x) at PC:0x%08x\n", addr, pc);
      abort();
      break;
    case RuntSerial:
    case RuntIR:
      runt_serial_set_value(c, addr, val);
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
      // DMA error helps the boot process proceed...
      runt_interrupt_raise(c, RuntInterruptSoundDMAErr);
      break;
    default:
      break;
  }
  
  c->memory[localAddr] = val;
  return val;
}


uint32_t runt_get_mem32(runt_t *c, uint32_t addr, uint32_t pc) {
  uint32_t result = c->memory[(addr - 0x01400000) / 4];
  
  switch ((addr >> 8) & 0xff) {
    case RuntLCD:
      fprintf(stderr, "Unsupported word set for Runt LCD (0x%08x) at PC:0x%08x\n", addr, pc);
      abort();
      break;
    case RuntIR:
    case RuntSerial:
      result = runt_serial_get_value(c, addr);
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

// Byte access seems to be used solely in the LCD+serial subsystems.
uint8_t runt_set_mem8(runt_t *c, uint32_t addr, uint8_t val, uint32_t pc) {
  switch ((addr >> 8) & 0xff) {
    case RuntLCD:
      val = c->lcd_set_uint8(c->lcd_driver, (addr & 0xff), val);
      break;
    case RuntSerial:
    case RuntIR:
      val = runt_serial_set_value(c, addr, val);
      break;
    case RuntADCValue:
      break;
    default:
      fprintf(stderr, "Unsupported byte set in Runt for address:0x%08x\n", addr);
      abort();
      break;
  }
  
  runt_log_access(c, addr, val, true);

  return val;
}

uint8_t runt_get_mem8(runt_t *c, uint32_t addr, uint32_t pc) {
  uint8_t result = 0;
  switch ((addr >> 8) & 0xff) {
    case RuntLCD:
      result = c->lcd_get_uint8(c->lcd_driver, (addr & 0xff));
      break;
    case RuntSerial:
    case RuntIR:
      result = runt_serial_get_value(c, addr);
      break;
    default:
      fprintf(stderr, "Unsupported byte set in Runt for address:0x%08x\n", addr);
      abort();
      break;
  }

  runt_log_access(c, addr, result, false);
  return result;
}


#pragma mark -
bool runt_step(runt_t *c) {
  if (c->runtAwake == false) {
    usleep(100);
    return false;
  }
    
  c->ticks += 2;
  
  if (c->rtcAlarm != 0 && runt_get_rtc(c) >= c->rtcAlarm) {
    runt_interrupt_raise(c, RuntInterruptAlarm);
    c->rtcAlarm = 0;
  }
  
  if (c->ticksAlarm1 != 0 && runt_get_ticks(c) >= c->ticksAlarm1) {
    runt_interrupt_raise(c, RuntInterruptTimer1);
    c->ticksAlarm1 = 0;
  }
  
  if (c->ticksAlarm2 != 0 && runt_get_ticks(c) >= c->ticksAlarm2) {
    runt_interrupt_raise(c, RuntInterruptTimer2);
    c->ticksAlarm2 = 0;
  }
  
  if (c->ticksAlarm3 != 0 && runt_get_ticks(c) >= c->ticksAlarm3) {
    runt_interrupt_raise(c, RuntInterruptTimer3);
    c->ticksAlarm3 = 0;
  }
  
  if (c->touchActive == true) {
    runt_interrupt_raise(c, RuntInterruptTablet);
  }
  
  uint32_t sampleSource = c->adcSource;
  switch (sampleSource) {
    case RuntADCSourceBackupBattery:
    case RuntADCSourceMainBattery:
    case RuntADCSourceThermistor:
    case RuntADCSourceTabletPositionX:
    case RuntADCSourceTabletPositionY:
    case RuntADCSourceTabletPressureX:
    case RuntADCSourceTabletPressureY:
      if (runt_interrupt_is_enabled(c, RuntInterruptADConverter) == true && runt_power_state_get_subsystem(c, RuntPowerADC) == true) {
        runt_interrupt_raise(c, RuntInterruptADConverter);
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
            void *get8, void *set8, void *getname, void *step, void *powered)
{
  c->lcd_driver = ext;
  c->lcd_get_uint8 = get8;
  c->lcd_set_uint8 = set8;
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
  
  memset(c->irPort, 0x00, sizeof(runt_serial_port_t));
  memset(c->serialPort, 0x00, sizeof(runt_serial_port_t));
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
    runt_set_lcd_fct(c, squirt, lcd_squirt_get_mem8, lcd_squirt_set_mem8, lcd_squirt_get_address_name, lcd_squirt_step, NULL);
    c->lcd_driver = squirt;
  }
  else {
    lcd_sharp_t *sharp = lcd_sharp_new();
    runt_set_lcd_fct(c, sharp, lcd_sharp_get_mem8, lcd_sharp_set_mem8, lcd_sharp_get_address_name, NULL, lcd_sharp_set_powered);
    c->lcd_driver = sharp;
  }
  
  //
  // Serial
  //
  c->irPort = calloc(1, sizeof(runt_serial_port_t));
  c->serialPort = calloc(1, sizeof(runt_serial_port_t));
  
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
  free(c->irPort);
  free(c->serialPort);
  
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
