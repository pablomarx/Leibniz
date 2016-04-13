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
  SquirtLCDBacklight = 0x30,
  SquirtLCDData = 0x48,
  SquirtLCDCursorHigh = 0x4c,
  SquirtLCDCursorLow = 0x50,
  SquirtLCDPixelInvert = 0x54,
};

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
    case SquirtLCDData:
      prefix = "lcd-data";
      break;
    case SquirtLCDPixelInvert:
      prefix = "lcd-pixel-invert";
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
      c->displayCursor = (c->displayCursor & 0xff00) | (val >> 24);
      break;
    case SquirtLCDCursorHigh: // Values written are 0x00 through 0x24
      c->displayCursor = (c->displayCursor & 0x00ff) | ((val >> 24) << 8);
      break;
    case SquirtLCDPixelInvert:
      c->displayPixelInvert = (val >> 24) & 1;
      break;
    case SquirtLCDData:
    {
      uint32_t framebufferIdx = (c->displayCursor) * 8;
      
      // Splat the pixels
      uint8_t pixels = ((uint8_t)(val >> 24));
      for (int bitIdx=7; bitIdx>=0; bitIdx--) {
        uint8_t bitVal = ((pixels>>bitIdx) & 1);
        
        if (c->displayPixelInvert == 0) {
          bitVal = !(c->displayFramebuffer[framebufferIdx]);
        }

        c->displayFramebuffer[framebufferIdx] = (bitVal ? 0x00 : 0xff);
        
        framebufferIdx++;
      }
      
      // Advance the cursor
      c->displayCursor++;

      c->displayDirty++;
      break;
    }
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
      result = ((c->displayCursor >> 8) << 24);
      break;
    case SquirtLCDCursorLow:
      result = ((c->displayCursor & 0xff) << 24);
      break;
    case SquirtLCDPixelInvert:
      result = (c->displayPixelInvert << 24);
      break;
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
