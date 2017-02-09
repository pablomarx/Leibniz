//
//  lcd_squirt.c
//  Leibniz
//
//  Created by Steve White on 6/13/15.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#include "lcd_squirt.h"
#include <stdlib.h>
#include <string.h>

enum {
  SquirtLCDNotBusy = 0x00,
  SquirtLCDDataRead = 0x08,
  SquirtLCDBacklight = 0x30,
  SquirtLCDDataWrite = 0x48,
  SquirtLCDCursorHigh = 0x4c,
  SquirtLCDCursorLow = 0x50,
  SquirtLCDDisplayMode = 0x54,
  
  SquirtLCDOrientation = 0x70,
  
  // Not sure what these are, but see them
  // being written when invoking:
  // Evaluation -> LCD Blanking
  SquirtLCDBlanking = 0x60,
  // Evaluation -> LCD MCount
  SquirtLCDMCount = 0x6c,
};

// Evaluation -> LCD VRAM displays:
// 68: VRAM DATA: 000000AA
// 69: VRAM ABUS: 000000FF
// 70: VRAM DBUS LO: 000000FF
// 73: VRAM DBUS SH: 00000000

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

const char *lcd_squirt_get_address_name(lcd_squirt_t *c, uint32_t addr) {
  const char *prefix = NULL;
  switch(addr) {
    case SquirtLCDNotBusy:
      prefix = "lcd-not-busy";
      break;
    case SquirtLCDCursorLow:
      prefix = "lcd-cursor-low";
      break;
    case SquirtLCDCursorHigh:
      prefix = "lcd-cursor-high";
      break;
    case SquirtLCDDataWrite:
      prefix = "lcd-data-write";
      break;
    case SquirtLCDDataRead:
      prefix = "lcd-data-read";
      break;
    case SquirtLCDDisplayMode:
      prefix = "lcd-display-mode";
      break;
    case SquirtLCDBlanking:
      prefix = "lcd-blanking";
      break;
    case SquirtLCDMCount:
      prefix = "lcd-m-count";
      break;
    case SquirtLCDOrientation:
      prefix = "lcd-orientation";
      break;
    default:
      prefix = "lcd-unknown";
      break;
  }
  return prefix;
}

uint32_t lcd_squirt_set_mem32(lcd_squirt_t *c, uint32_t addr, uint32_t val) {
  c->memory[addr/4] = val;
  
  switch(addr) {
    case SquirtLCDCursorLow: // Values written are 0x00 through 0xff
      c->cursorLow = (val >> 24);
      break;
    case SquirtLCDCursorHigh: // Values written are 0x00 through 0x24
      c->cursorHigh = (val >> 24);
      break;
    case SquirtLCDDisplayMode:
      c->displayMode = (val >> 24);
      break;
    case SquirtLCDOrientation: {
      c->orientation = (val >> 24);
      break;
    }
    case SquirtLCDDataWrite:
    {
      int8_t horizontal = (c->orientation & 0xf) != 0;
      int8_t vertical   = (c->orientation >> 4) != 0;
      
      int32_t displayCursor = (c->cursorHigh << 8) | c->cursorLow;
      int32_t framebufferIdx = (displayCursor) * 8;
      
      if (framebufferIdx < 0 || framebufferIdx >= (SCREEN_WIDTH * SCREEN_HEIGHT) - 8) {
        fprintf(c->logFile, "Squirt LCD Data Write: Bad frame buffer index: %i.  Cursor high:0x%02x, cursor low:0x%02x\n", framebufferIdx, c->cursorHigh, c->cursorLow);
        framebufferIdx = 0;
      }
      
      // Splat the pixels
      uint8_t pixels = ((uint8_t)(val >> 24));
      for (int bitIdx=7; bitIdx>=0; bitIdx--) {
        uint8_t bitVal = ((pixels>>bitIdx) & 1);
        c->displayFramebuffer[framebufferIdx] = (bitVal ? 0x00 : 0xff);
        
        if (horizontal == 1) {
          framebufferIdx++;
        }
      }
      
      // Advance the cursor
      if (vertical == 0) {
        displayCursor -= SCREEN_WIDTH/8;
      }
      else if (horizontal == 1) {
        displayCursor++;
      }
      
      c->cursorHigh = displayCursor >> 8;
      c->cursorLow = displayCursor & 0xff;
      
      c->displayDirty++;
      c->stepsSinceLastFlush = 0;
      break;
    }
    case SquirtLCDBlanking:
    case SquirtLCDMCount:
      break;
    default:
      printf("unknown %02x => %08x\n", addr, val);
      break;
  }
  return val;
}

void lcd_squirt_step(lcd_squirt_t *c) {
  if (c->displayDirty != 0) {
    c->stepsSinceLastFlush++;
    
    if (c->stepsSinceLastFlush >= 500) {
      FlushDisplay((const char *)c->displayFramebuffer, SCREEN_WIDTH, SCREEN_HEIGHT);
      c->displayDirty = 0;
      c->stepsSinceLastFlush = 0;
    }
  }
}

uint32_t lcd_squirt_get_mem32(lcd_squirt_t *c, uint32_t addr) {
  uint32_t result = c->memory[addr/4];
  
  switch (addr) {
    case SquirtLCDCursorHigh:
      result = (c->cursorHigh << 24);
      break;
    case SquirtLCDCursorLow:
      result = (c->cursorLow << 24);
      break;
    case SquirtLCDDisplayMode:
      result = (c->displayMode << 24);
      break;
    case SquirtLCDOrientation:
      result = (c->orientation << 24);
      break;
    case SquirtLCDDataRead: {
      int32_t displayCursor = (c->cursorHigh << 8) | c->cursorLow;
      int32_t framebufferIdx = (displayCursor) * 8;
      if (framebufferIdx < 0 || framebufferIdx >= (SCREEN_WIDTH * SCREEN_HEIGHT) - 8) {
        fprintf(c->logFile, "Squirt LCD Data Read: Bad frame buffer index: %i.  Cursor high:0x%02x, cursor low:0x%02x\n", framebufferIdx, c->cursorHigh, c->cursorLow);
        framebufferIdx = 0;
      }
      result = 0;
      for (int j=7; j>=0; j--) {
        int pixel = (c->displayFramebuffer[framebufferIdx] != 0x00) ? 0 : 1;
        result |= (pixel << j);
        framebufferIdx++;
      }
      result = result << 24;
      break;
    }
    case SquirtLCDNotBusy:
      // We're never busy...
      result = (0x20 << 24);
      break;
  }
  
  return result;
}

void lcd_squirt_set_log_file (lcd_squirt_t *c, FILE *file) {
  c->logFile = file;
}

void lcd_squirt_init (lcd_squirt_t *c) {
  c->memory = calloc(0xff, 1);
  
  //
  // Display
  //
  c->displayFramebuffer = calloc(SCREEN_WIDTH * SCREEN_HEIGHT, 1);
  memset(c->displayFramebuffer, 0xff, SCREEN_WIDTH * SCREEN_HEIGHT);
  
  c->logFile = stdout;
}

lcd_squirt_t *lcd_squirt_new () {
  lcd_squirt_t *c;
  
  c = calloc(1, sizeof (lcd_squirt_t));
  if (c == NULL) {
    return (NULL);
  }
  
  lcd_squirt_init (c);
  
  return (c);
}

void lcd_squirt_free (lcd_squirt_t *c) {
  free(c->memory);
  free(c->displayFramebuffer);
  
}

void lcd_squirt_del (lcd_squirt_t *c) {
  if (c != NULL) {
    lcd_squirt_free (c);
    free (c);
  }
}
