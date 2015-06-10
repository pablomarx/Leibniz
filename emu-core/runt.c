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
    RuntPowerTablet = 0x01,
  RuntFlooder = 0x1c,
  RuntRTC = 0x24,
  RuntTimer = 0x30,
  RuntTicks = 0x34,
  RuntSound1 = 0x5c, // probably not sound, but frequently seen together.
  RuntTablet = 0x58,
  RuntLCD = 0x60,
    RuntLCDNotBusy = 0x04,
    RuntLCDSync = 0x38,
    RuntLCDVersion = 0x3c,
    RuntLCDContrast = 0xb0,
    RuntLCDFillmode = 0xa0,
    RuntLCDDisplayInverse = 0xe0,
    RuntLCDDisplayOrientation = 0xec,
    RuntLCDPixelData = 0x88,
    RuntLCDCursorXLSB = 0xc0,
    RuntLCDCursorXMSB = 0xc4,
    RuntLCDCursorYLSB = 0xc8,
    RuntLCDCursorYMSB = 0xcc,
    RuntLCDScreenHeightLSB = 0xf0,
    RuntLCDScreenHeightMSB = 0xf4,
    RuntLCDScreenWidthLSB = 0xf8,
    RuntLCDScreenWidthMSB = 0xfc,
    RuntLCDFlush = 0xb8,

  RuntSound = 0x64,
  
  RuntIR = 0x80,
    RuntIRConfig = 0x08,
    RuntIRData   = 0x04,
  
  RuntSerial = 0x81,
    RuntSerialConfig = 0x08,
    RuntSerialData   = 0x04,
};

enum {
  RuntInterruptTablet = (1 << 10),
  RuntInterruptSound  = (1 << 12),
  RuntInterruptSwitch = (1 << 15),
};

#define SCREEN_WIDTH 336
#define SCREEN_HEIGHT 240

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
      prefix = "ticks";
      break;
    case RuntTablet:
      flag = RuntLogTablet;
      prefix = "tablet";
      break;
    case RuntLCD: {
      flag = RuntLogLCD;
      switch(addr & 0xff) {
        case RuntLCDNotBusy: // lcd not busy
          prefix = "lcd-not-busy";
          break;
        case RuntLCDSync: // lcd sync
          prefix = "lcd-sync";
          break;
        case RuntLCDContrast: // lcd contrast?
          prefix = "lcd-contrast";
          break;
        case RuntLCDFillmode:
          prefix = "lcd-fill-mode";
          break;
        case RuntLCDDisplayInverse:
          prefix = "lcd-display-inverse";
          break;
        case RuntLCDDisplayOrientation:
          prefix = "lcd-display-orientation";
          break;
        case RuntLCDPixelData:
          prefix = "lcd-pixel-data";
          break;
        case RuntLCDCursorXLSB:
          prefix = "lcd-cursor-x-lsb";
          break;
        case RuntLCDCursorXMSB:
          prefix = "lcd-cursor-x-msb";
          break;
        case RuntLCDCursorYLSB:
          prefix = "lcd-cursor-y-lsb";
          break;
        case RuntLCDCursorYMSB:
          prefix = "lcd-cursor-y-msb";
          break;
        case RuntLCDScreenHeightLSB:
          prefix = "lcd-screen-height-lsb";
          break;
        case RuntLCDScreenHeightMSB:
          prefix = "lcd-screen-height-msb";
          break;
        case RuntLCDScreenWidthLSB:
          prefix = "lcd-screen-width-lsb";
          break;
        case RuntLCDScreenWidthMSB:
          prefix = "lcd-screen-width-msb";
          break;
        default:
          prefix = "lcd-unknown";
          break;
      }
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
      prefix = "rtc";
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
#pragma mark LCD display
void runt_display_set_mem32(runt_t *c, uint32_t addr, uint32_t val) {
  val = val >> 24;
  switch (addr & 0xff) {
    case RuntLCDCursorXLSB:
      c->displayCursorX = (val & 0xff);
      if (c->displayCursorX < 0) {
        fprintf(c->logFile, "bad x coordinate: 0x%08x\n", val);
      }
      break;
    case RuntLCDCursorXMSB:
      c->displayCursorX = ((val & 0xff) << 8) | c->displayCursorX;
      break;
    case RuntLCDCursorYMSB:
      c->displayCursorY = ((val & 0xff) << 8) | c->displayCursorY;
      break;
    case RuntLCDCursorYLSB:
      c->displayCursorY = (val & 0xff);
      if (c->displayCursorY < 0) {
        fprintf(c->logFile, "bad y coordinate: 0x%08x\n", val);
      }
      break;
    case RuntLCDFillmode:
      c->displayFillMode = ((val & 0xff) == 0x03);
      break;
    case RuntLCDDisplayInverse:
      c->displayInverse = (val & 0xff);
      break;
    case RuntLCDDisplayOrientation:
      c->displayOrientation = (val == 0x00); //1;//((data & 0xff) == 0x00);
      break;
      
    case RuntLCDPixelData: {
      int x = c->displayCursorX;
      int y = c->displayCursorY;
      if (x < 0) {
        // fprintf(c->logFile,"negative X: %i\n", x);
        x += SCREEN_WIDTH;
      }
      else if (x >= SCREEN_WIDTH) {
        // fprintf(c->logFile,"excessive X: %i\n", x);
        x -= SCREEN_WIDTH;
      }
      if (y < 0) {
        // fprintf(c->logFile,"negative Y: %i\n", y);
        y += SCREEN_HEIGHT;
      }
      else if (y >= SCREEN_HEIGHT) {
        // fprintf(c->logFile,"excessive Y: %i\n", y);
        y -= SCREEN_HEIGHT;
      }
      
      int i=(c->displayInverse && x >= c->displayInverse ? 4 : 7);
      for (; i>=0; i--) {
        int offset = (y * SCREEN_WIDTH) + (x);
        if (offset > 0 && offset < SCREEN_HEIGHT * SCREEN_WIDTH)
        {
          if (c->displayInverse) {
            int pixel = c->displayFramebuffer[offset];
            pixel = pixel ? 0x00 : 0xff;
            c->displayFramebuffer[offset] = pixel;
          }
          else {
            int pixel = (((val & 0xff) >> i) & 1) ? 0x00 : 0xff;
            if (c->displayFillMode == 0 || pixel == 0x00) {
              c->displayFramebuffer[offset] = pixel;
            }
          }
        }
        x++;
      }
      
      if (c->displayInverse) c->displayCursorX+=5;
      else if (c->displayOrientation) c->displayCursorY++;
      else c->displayCursorX+=8;
      
      c->displayDirty = 1;
      break;
    }
    case RuntLCDFlush:
      if (c->displayDirty) {
        FlushDisplay((const char *)c->displayFramebuffer, SCREEN_WIDTH, SCREEN_HEIGHT);
        // fprintf(c->logFile,"flushing display\n");
        c->displayDirty = 0;
      }
      break;
  }
}

uint32_t runt_display_get_mem32(runt_t *c, uint32_t addr, uint32_t val) {
  uint32_t result = val;
  switch (addr) {
    case RuntLCDNotBusy:
      result = (c->displayBusy++ % 2 ? 0x02000002 : 0x00000000);
      break;
      
    case RuntLCDSync:
      result = 165;
      break;
      
    case RuntLCDVersion:
      result = 91;
      break;
  }
  return result;
}

#pragma mark -
#pragma mark Touches
void runt_touch_down(runt_t *c, int x, int y) {
  c->touchX = x;
  c->touchY = y;
  c->touchActive = true;
  
  c->interrupt |= RuntInterruptTablet;
}

void runt_touch_up(runt_t *c) {
  c->touchX = 0;
  c->touchY = 0;
  c->touchActive = false;
  
  c->interrupt ^= RuntInterruptTablet;
}

void runt_switch_state(runt_t *c, int switchNum, int state) {
  c->switches[switchNum] = state;
}

#pragma mark -
#pragma mark
uint32_t runt_set_mem32(runt_t *c, uint32_t addr, uint32_t val) {
  runt_log_access(c, addr, val, true);
  
  switch ((addr >> 8) & 0xff) {
    case RuntLCD:
      runt_display_set_mem32(c, addr, val);
      break;
    case RuntClearInterrupt:
      if (c->interruptStick > 0) {
        c->interruptStick--;
      }
      else {
        bool log = ((c->logFlags & RuntLogInterrupts) == RuntLogInterrupts);
        if (val != RuntInterruptTablet && val != 0x00040000 && val != 0x00001000 && val != 0x00008000 && val != 0x00010000) {
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
        if (val & RuntPowerTablet) {
          fprintf(c->logFile, "tablet?, ");
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
      result = runt_display_get_mem32(c, addr, result);
      break;
    case RuntTicks:
      result = arm_get_opcnt(c->arm) * 1000;
      break;
    case RuntTablet:
      if (((result >> 24) & 0xff) == 0x04) {
        result = 0xfff - c->touchY;
      }
      else {
        result = 0xfff - c->touchX;
      }
      break;
    case RuntGetInterrupt:
      if (c->touchActive) {
        c->interrupt |= RuntInterruptTablet;
      }
      if (c->switches[1]) {
        c->interrupt |= 0x00008000;
      }
      if (c->switches[2]) {
        c->interrupt |= 0x00010000;
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
      result = (uint32_t)(time(NULL) - c->bootTime) ;
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
void runt_init (runt_t *c) {
  c->base = 0x01400000;
  c->memory = calloc(0x8fff, 1);
  
  //
  // Display
  //
  c->displayFramebuffer = calloc(SCREEN_WIDTH * SCREEN_HEIGHT, 1);
  memset(c->displayFramebuffer, 0xff, SCREEN_WIDTH * SCREEN_HEIGHT);
  
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
  runt_set_log_flags(c, RuntLogTablet, 0);
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
  free(c->displayFramebuffer);
  
}

void runt_del (runt_t *c) {
  if (c != NULL) {
    runt_free (c);
    free (c);
  }
}
