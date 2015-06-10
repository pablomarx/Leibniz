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

#define SCREEN_WIDTH 336
#define SCREEN_HEIGHT 240

#define INTERRUPT_MASK_TABLET 0x00000400

void runt_log_access(runt_t *c, uint32_t addr, uint32_t val, bool write) {
  const char *prefix = NULL;
  uint32_t flag = 0;

  switch ((addr >> 8) & 0xff) {
    case 0x04:
      flag = RuntLogInterrupts;
      prefix = "get-interrupt";
      break;
    case 0x08:
      flag = RuntLogInterrupts;
      prefix = "clear-interrupt";
      break;
    case 0x1c:
      // don't know, but it happens a ton...
      return;
    case 0x30:
      flag = RuntLogTimer;
      prefix = "timer";
      break;
    case 0x34:
      flag = RuntLogTicks;
      prefix = "ticks";
      break;
    case 0x58:
      flag = RuntLogTablet;
      prefix = "tablet";
      break;
    case 0x60: {
      flag = RuntLogLCD;
      switch(addr) {
        case 0x01406004: // lcd not busy
          prefix = "lcd-not-busy";
          break;
        case 0x01406038: // lcd sync
          prefix = "lcd-sync";
          break;
        case 0x014060b0: // lcd contrast?
          prefix = "lcd-contrast";
          break;
        case 0x014060a0:
          prefix = "lcd-fill-mode";
          break;
        case 0x014060e0:
          prefix = "lcd-display-inverse";
          break;
        case 0x014060ec:
          prefix = "lcd-display-orientation";
          break;
        case 0x01406088:
          prefix = "lcd-pixel-data";
          break;
        case 0x014060c0:
          prefix = "lcd-cursor-x-lsb";
          break;
        case 0x014060c4:
          prefix = "lcd-cursor-x-msb";
          break;
        case 0x014060c8:
          prefix = "lcd-cursor-x-lsb";
          break;
        case 0x014060cc:
          prefix = "lcd-cursor-x-msb";
          break;
        case 0x014060f0:
          prefix = "lcd-screen-height-lsb";
          break;
        case 0x014060f4:
          prefix = "lcd-screen height-msb";
          break;
        case 0x014060f8:
          prefix = "lcd-screen width-lsb";
          break;
        case 0x014060fc:
          prefix = "lcd-screen width-msb";
          break;
        default:
          prefix = "lcd-unknown";
          break;
      }
      break;
    }
    case 0x80:
      flag = RuntLogIR;
      if (addr == 0x01408004) {
        prefix = "ir-data";
      }
      else if (addr == 0x01408008) {
        prefix = "ir-config";
      }
      else {
        prefix = "ir-unknown";
      }
      break;
    case 0x81:
      flag = RuntLogSerial;
      if (addr == 0x01408104) {
        prefix = "serial-data";
      }
      else if (addr == 0x01408108) {
        prefix = "serial-config";
      }
      else {
        prefix = "serial-unknown";
      }
      break;
    case 0x10:
      flag = RuntLogPower;
      prefix = "power";
      break;
    case 0x0c:
      flag = RuntLogSwitch;
      prefix = "switch";
      break;
    case 0x5c:
    case 0x64:
      flag = RuntLogSound;
      prefix = "sound";
      break;
    case 0x24:
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
  switch (addr) {
    case 0x014060c0:
      c->displayCursorX = (val & 0xff);
      if (c->displayCursorX < 0) {
        fprintf(c->logFile, "bad x coordinate: 0x%08x\n", val);
      }
      break;
    case 0x014060c4:
      c->displayCursorX = ((val & 0xff) << 8) | c->displayCursorX;
      break;
    case 0x014060cc:
      c->displayCursorY = ((val & 0xff) << 8) | c->displayCursorY;
      break;
    case 0x014060c8:
      c->displayCursorY = (val & 0xff);
      if (c->displayCursorY < 0) {
        fprintf(c->logFile, "bad y coordinate: 0x%08x\n", val);
      }
      break;
    case 0x014060a0:
      c->displayFillMode = ((val & 0xff) == 0x03);
      break;
    case 0x014060e0:
      c->displayInverse = (val & 0xff);
      break;
    case 0x014060ec:
      c->displayOrientation = (val == 0x00); //1;//((data & 0xff) == 0x00);
      break;
      
    case 0x01406088: {
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
    case 0x014060b8:
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
    case 0x01406004: // lcd not busy
      result = (c->displayBusy++ % 2 ? 0x02000002 : 0x00000000);
      break;
      
    case 0x01406038: // lcd sync
      result = 165;
      break;
      
    case 0x0140603c: // lcd driver version?
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

  c->interrupt |= INTERRUPT_MASK_TABLET;
}

void runt_touch_up(runt_t *c) {
  c->touchX = 0;
  c->touchY = 0;
  c->touchActive = false;

  c->interrupt ^= INTERRUPT_MASK_TABLET;
}

void runt_switch_state(runt_t *c, int switchNum, int state) {
  c->switches[switchNum] = state;
}

#pragma mark -
#pragma mark
uint32_t runt_set_mem32(runt_t *c, uint32_t addr, uint32_t val) {
  runt_log_access(c, addr, val, true);
  
  switch ((addr >> 8) & 0xff) {
    case 0x60:
      runt_display_set_mem32(c, addr, val);
      break;
    case 0x08: // interrupt
      if (c->interruptStick > 0) {
        c->interruptStick--;
      }
      else {
        bool log = ((c->logFlags & RuntLogInterrupts) == RuntLogInterrupts);
        if (val != INTERRUPT_MASK_TABLET && val != 0x00040000 && val != 0x00001000 && val != 0x00008000 && val != 0x00010000) {
          if (log) fprintf(c->logFile, "clearing interrupt: 0x%08x\n", val);
        }
        if ((c->interrupt & val) == val) {
          if (log) fprintf(c->logFile, " c->interrupt was: 0x%08x, ", c->interrupt);
          c->interrupt ^= val;
          if (log) fprintf(c->logFile, " c->interrupt now: 0x%08x \n", c->interrupt);
        }
      }
      break;
    case 0x58:
      //val = (val & 0xff);
      break;
    case 0x1c:
      break;
    case 0x5c:
      // sound eval menu wrote here!!
      break;
    case 0x64: // sound?
      c->interrupt |= 0x1000;
      break;
    case 0x81:
      // serial
      // 0x01408104 is output byte
      // 0x01408108 something else...
      break;
    case 0x80:
      // ir ???
      // 0x01408004 is output byte
      // 0x01408008 something else...
      break;
    case 0x0c: // switch
      // c->interrupt |= 0x00008000;
      break;
    case 0x10: // power
      if ((c->logFlags & RuntLogPower) == RuntLogPower) {
        fprintf(c->logFile, "power on: 0x%08x -> 0x%08x: ", c->memory[0x1000/4], val);
        if (val & 0x40) {
          fprintf(c->logFile, "trim, ");
        }
        if (val & 0x20) {
          fprintf(c->logFile, "serial, ");
        }
        if (val & 0x10) {
          fprintf(c->logFile, "sound, ");
        }
        if (val & 0x08) {
          fprintf(c->logFile, "vpp1, ");
        }
        if (val & 0x04) {
          fprintf(c->logFile, "vpp2, ");
        }
        if (val & 0x02) {
          fprintf(c->logFile, "lcd, ");
        }
        if (val & 0x01) {
          fprintf(c->logFile, "tablet?, ");
        }
        fprintf(c->logFile, "\n");
      }
      // 0x00000060 = trim on
      // 0x00000030 = sound on
      // 0x00000028 = vpp1 on
      // 0x00000024 = vpp2 on
      break;
    /*
     during IR tests:
     unknown write: addr=0x01406800, val=0x00000004...
     unknown write: addr=0x01406800, val=0x00000000...
     */
    default:
      break;
  }
  
  c->memory[(addr-0x01400000) / 4] = val;
  return val;
}


uint32_t runt_get_mem32(runt_t *c, uint32_t addr) {
  uint32_t result = c->memory[(addr - 0x01400000) / 4];
  
  switch ((addr >> 8) & 0xff) {
    case 0x60:
      result = runt_display_get_mem32(c, addr, result);
      break;
    case 0x34: // ticks
      result = arm_get_opcnt(c->arm) * 1000;
      break;
    case 0x58: // tablet
      if (((result >> 24) & 0xff) == 0x04) {
        result = 0xfff - c->touchY;
      }
      else {
        result = 0xfff - c->touchX;
      }
      break;
    case 0x04: // interrupt
      if (c->touchActive) {
        c->interrupt |= INTERRUPT_MASK_TABLET;
      }
      if (c->switches[1]) {
        c->interrupt |= 0x00008000;
      }
      if (c->switches[2]) {
        c->interrupt |= 0x00010000;
      }
      result = c->interrupt;
      break;
    case 0x10:
      break;
    case 0x0c: // switches?
      result = 0;
      if (c->switches[0]) result |= 0x00000000; // nicd switch?
      if (c->switches[1]) result |= 0x00008000; // card lock switch?
      if (c->switches[2]) result |= 0x00010000; // power switch?
      break;
    case 0x1c:
      result = 0;
      break;
    case 0x80: // ir?
    case 0x81: // serial
      if ((result & 0xff) == 0x00) {
        result = 0xffffffff;
      }
      break;
    case 0x24: // rtc?
      result = (uint32_t)(time(NULL) - c->bootTime) ;
      break;
      /*
    case 0x10: // 0x01401000 == power ?
      result = 0x00000020;
      break;
       */
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
  c->memory = calloc(0x01408fff - 0x01400000, 1);

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
