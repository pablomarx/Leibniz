//
//  lcd_sharp.c
//  Leibniz
//
//  Created by Steve White on 6/13/15.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#include "lcd_sharp.h"
#include <stdlib.h>
#include <string.h>

enum {
  SharpLCDUnknown1  = 0x00,  // CR
  SharpLCDNotBusy   = 0x04,  // SR
  SharpLCDPixelData = 0x08,  // DR
  SharpLCDUnknown2  = 0x0c,  // BM
  
  SharpLCDUnknown3  = 0x10,  // IDR
  SharpLCDDirection = 0x14,  // IDW
  SharpLCDUnknown4  = 0x18,  // LPL
  SharpLCDUnknown5  = 0x1c,  // LPH
  
  SharpLCDFillMode  = 0x20,  // MD
  SharpLCDUnknown6  = 0x24,  // WP
  SharpLCDUnknown7  = 0x28,  // WM
  SharpLCDContrast  = 0x2c,  // CT
  
  SharpLCDUnknown8  = 0x30,  // FML
  SharpLCDUnknown9  = 0x34,  // FMH
  SharpLCDFlush     = 0x38,  // ST0
  SharpLCDVersion   = 0x3c,  // ST1
  
  SharpLCDWriteX_l  = 0x40,  // XWL
  SharpLCDWriteX_h  = 0x44,  // XWH
  SharpLCDWriteY_l  = 0x48,  // YWL
  SharpLCDWriteY_h  = 0x4c,  // YWH
  
  SharpLCDReadX_l   = 0x50,  // XRL
  SharpLCDReadX_h   = 0x54,  // XRH
  SharpLCDReadY_l   = 0x58,  // YRL
  SharpLCDReadY_h   = 0x5c,  // YRH
  
  SharpLCDWindowX_l = 0x60,  // XLTL
  SharpLCDWindowX_h = 0x64,  // XLTH
  SharpLCDWindowY_l = 0x68,  // YTL
  SharpLCDWindowY_h = 0x6c,  // YTH
  
  SharpLCDWindowW_l = 0x70,  // XRTL
  SharpLCDWindowW_h = 0x74,  // XRTH
  SharpLCDWindowH_l = 0x78,  // YBL
  SharpLCDWindowH_h = 0x7c,  // YBH
};

enum {
  SharpLCDFillModeNormal = 0,
  SharpLCDFillModeInvert = 1,
  SharpLCDFillModeOnlySetBits = 3,
};

#define SCREEN_WIDTH 336
#define SCREEN_HEIGHT 240

const char *lcd_sharp_get_address_name(lcd_sharp_t *c, uint32_t addr) {
  if (addr >= 0x80) {
    addr -= 0x80;
  }
  
  const char *prefix = NULL;
  switch(addr) {
    case SharpLCDNotBusy: // lcd not busy
      prefix = "lcd-not-busy";
      break;
    case SharpLCDFillMode:
      prefix = "lcd-fill-mode";
      break;
    case SharpLCDPixelData:
      prefix = "lcd-pixel-data";
      break;
    case SharpLCDDirection:
      prefix = "direction";
      break;
    case SharpLCDFlush:
      prefix = "lcd-flush";
      break;

    case SharpLCDContrast:
      prefix = "lcd-contrast";
      break;
    case SharpLCDWriteX_l:
      prefix = "lcd-write-x-low";
      break;
    case SharpLCDWriteX_h:
      prefix = "lcd-write-x-high";
      break;
    case SharpLCDWriteY_l:
      prefix = "lcd-write-y-low";
      break;
    case SharpLCDWriteY_h:
      prefix = "lcd-write-y-high";
      break;
      
    case SharpLCDReadX_l:
      prefix = "lcd-read-x-low";
      break;
    case SharpLCDReadX_h:
      prefix = "lcd-read-x-high";
      break;
    case SharpLCDReadY_l:
      prefix = "lcd-read-y-low";
      break;
    case SharpLCDReadY_h:
      prefix = "lcd-read-y-high";
      break;
      
    case SharpLCDWindowX_l:
      prefix = "lcd-window-x-low";
      break;
    case SharpLCDWindowX_h:
      prefix = "lcd-window-x-high";
      break;
    case SharpLCDWindowY_l:
      prefix = "lcd-window-y-low";
      break;
    case SharpLCDWindowY_h:
      prefix = "lcd-window-y-high";
      break;
      
    case SharpLCDWindowW_l:
      prefix = "lcd-window-width-low";
      break;
    case SharpLCDWindowW_h:
      prefix = "lcd-window-width-high";
      break;
    case SharpLCDWindowH_l:
      prefix = "lcd-window-height-low";
      break;
    case SharpLCDWindowH_h:
      prefix = "lcd-window-height-high";
      break;
      
    default:
      prefix = "lcd-unknown";
      break;
  }
  return prefix;
}

static inline uint8_t lcd_sharp_read_pixels(lcd_sharp_t *c) {
  // Not yet implemented...
  return 0;
}

static inline void lcd_sharp_write_pixels(lcd_sharp_t *c, uint8_t val) {
  int x = c->writeX;
  int y = c->writeY;
  if (x < 0) {
    x += SCREEN_WIDTH;
  }
  else if (x >= SCREEN_WIDTH) {
    x -= SCREEN_WIDTH;
  }
  if (y < 0) {
    y += SCREEN_HEIGHT;
  }
  else if (y >= SCREEN_HEIGHT) {
    y -= SCREEN_HEIGHT;
  }
  
  uint8_t direction = c->memory[SharpLCDDirection/4];
  if (direction == 0x06) {
    y = SCREEN_HEIGHT - y;
  }
  
  // This isn't accurate -- should be taking window x/y w/h
  // into account.  This is just to match the previous
  // behavior of the emulator.
  int i;
  if (c->fillMode == SharpLCDFillModeInvert) {
    i = 4;
  }
  else {
    i = 7;
  }
  
  for (; i>=0; i--, x++) {
    int offset = (y * SCREEN_WIDTH) + (x);
    if (offset < 0 || offset >= SCREEN_HEIGHT * SCREEN_WIDTH) {
      offset = 0;
    }

    uint8_t pixel = ((val >> i) & 1) ? BLACK_COLOR : WHITE_COLOR;

    if (c->fillMode == SharpLCDFillModeInvert) {
      // Invert existing pixels...
      pixel = c->displayFramebuffer[offset];
      pixel = (pixel == BLACK_COLOR) ? WHITE_COLOR : BLACK_COLOR;
    }
    else if (c->fillMode == SharpLCDFillModeOnlySetBits) {
      // Only use the pixel if it's set (black), otherwise
      // use the existing pixel
      if (pixel == WHITE_COLOR) {
        pixel = c->displayFramebuffer[offset];
      }
    }
    
    c->displayFramebuffer[offset] = pixel;
  }
  
  
  if (direction != 144 ) c->writeY++;
  // See above warning about not being accurate
  else if (c->fillMode == SharpLCDFillModeInvert) c->writeX += 5;
  else c->writeX+=8;
  
  c->displayDirty = true;
}

uint32_t lcd_sharp_set_mem32(lcd_sharp_t *c, uint32_t addr, uint32_t val) {
  if (addr >= 0x80) {
    addr -= 0x80;
  }
  
  uint8_t byteVal = (val >> 24) & 0xff;
  c->memory[addr/4] = byteVal;
  
  switch (addr) {
    case SharpLCDFillMode:
      c->fillMode = byteVal;
      break;

    case SharpLCDContrast:
      c->contrast = byteVal;
      break;

    case SharpLCDWriteX_l:
      c->writeX = (c->writeX & 0xff00) | byteVal;
      break;
    case SharpLCDWriteX_h:
      c->writeX = (c->writeX & 0x00ff) | (byteVal << 8);
      break;
    case SharpLCDWriteY_l:
      c->writeY = (c->writeY & 0xff00) | byteVal;
      break;
    case SharpLCDWriteY_h:
      c->writeY = (c->writeY & 0x00ff) | (byteVal << 8);
      break;
      
    case SharpLCDReadX_l:
      c->readX = (c->readX & 0xff00) | byteVal;
      break;
    case SharpLCDReadX_h:
      c->readX = (c->readX & 0x00ff) | (byteVal << 8);
      break;
    case SharpLCDReadY_l:
      c->readY = (c->readY & 0xff00) | byteVal;
      break;
    case SharpLCDReadY_h:
      c->readY = (c->readY & 0x00ff) | (byteVal << 8);
      break;
      
    case SharpLCDWindowX_l:
      c->windowX = (c->windowX & 0xff00) | byteVal;
      break;
    case SharpLCDWindowX_h:
      c->windowX = (c->windowX & 0x00ff) | (byteVal << 8);
      break;
    case SharpLCDWindowY_l:
      c->windowY = (c->windowY & 0xff00) | byteVal;
      break;
    case SharpLCDWindowY_h:
      c->windowY = (c->windowY & 0x00ff) | (byteVal << 8);
      break;
      
    case SharpLCDWindowW_l:
      c->windowW = (c->windowW & 0xff00) | byteVal;
      break;
    case SharpLCDWindowW_h:
      c->windowW = (c->windowW & 0x00ff) | (byteVal << 8);
      break;
    case SharpLCDWindowH_l:
      c->windowH = (c->windowH & 0xff00) | byteVal;
      break;
    case SharpLCDWindowH_h:
      c->windowH = (c->windowH & 0x00ff) | (byteVal << 8);
      break;
      
    case SharpLCDPixelData:
      lcd_sharp_write_pixels(c, byteVal);
      break;
      
    case SharpLCDFlush:
      if (c->displayDirty == true) {
        newton_display_set_framebuffer((const char *)c->displayFramebuffer, SCREEN_WIDTH, SCREEN_HEIGHT);
        c->displayDirty = false;
      }
      break;
  }
  return val;
}

uint32_t lcd_sharp_get_mem32(lcd_sharp_t *c, uint32_t addr) {
  if (addr >= 0x80) {
    addr -= 0x80;
  }
  
  uint32_t result = c->memory[addr/4];
  
  switch (addr) {
    case SharpLCDNotBusy:
      result = (c->displayBusy++ % 2 ? 0x02 : 0x00);
      break;
      
    case SharpLCDVersion:
      result = 91;
      break;

    case SharpLCDPixelData:
      result = lcd_sharp_read_pixels(c);
      break;

    case SharpLCDContrast:
      result = c->contrast;
      break;

      
    case SharpLCDWriteX_l:
      result = (c->writeX & 0xff);
      break;
    case SharpLCDWriteX_h:
      result = (c->writeX >> 8) & 0xff;
      break;
    case SharpLCDWriteY_l:
      result = (c->writeY & 0xff);
      break;
    case SharpLCDWriteY_h:
      result = (c->writeY >> 8) & 0xff;
      break;
      
    case SharpLCDReadX_l:
      result = (c->readX & 0xff);
      break;
    case SharpLCDReadX_h:
      result = (c->readX >> 8) & 0xff;
      break;
    case SharpLCDReadY_l:
      result = (c->readY & 0xff);
      break;
    case SharpLCDReadY_h:
      result = (c->readY >> 8) & 0xff;
      break;
      
    case SharpLCDWindowX_l:
      result = (c->windowX & 0xff);
      break;
    case SharpLCDWindowX_h:
      result = (c->windowX >> 8) & 0xff;
      break;
    case SharpLCDWindowY_l:
      result = (c->windowY & 0xff);
      break;
    case SharpLCDWindowY_h:
      result = (c->windowY >> 8) & 0xff;
      break;
      
    case SharpLCDWindowW_l:
      result = (c->windowW & 0xff);
      break;
    case SharpLCDWindowW_h:
      result = (c->windowW >> 8) & 0xff;
      break;
    case SharpLCDWindowH_l:
      result = (c->windowH & 0xff);
      break;
    case SharpLCDWindowH_h:
      result = (c->windowH >> 8) & 0xff;
      break;
      
  }
  
  return (result << 24);
}

void lcd_sharp_set_log_file (lcd_sharp_t *c, FILE *file) {
  c->logFile = file;
}

void lcd_sharp_init (lcd_sharp_t *c) {
  c->memory = calloc(0xff, sizeof(uint32_t));
  
  c->windowW = SCREEN_WIDTH - 1;
  c->windowH = SCREEN_HEIGHT - 1;
  c->contrast = 138;
  
  //
  // Display
  //
  c->displayFramebuffer = calloc(SCREEN_WIDTH * SCREEN_HEIGHT, 1);
  memset(c->displayFramebuffer, 0xff, SCREEN_WIDTH * SCREEN_HEIGHT);
  
  c->logFile = stdout;
  
  newton_display_open(SCREEN_HEIGHT, SCREEN_WIDTH);
}

lcd_sharp_t *lcd_sharp_new () {
  lcd_sharp_t *c;
  
  c = calloc(1, sizeof (lcd_sharp_t));
  if (c == NULL) {
    return (NULL);
  }
  
  lcd_sharp_init (c);
  
  return (c);
}

void lcd_sharp_free (lcd_sharp_t *c) {
  free(c->memory);
  free(c->displayFramebuffer);
  
}

void lcd_sharp_del (lcd_sharp_t *c) {
  if (c != NULL) {
    lcd_sharp_free (c);
    free (c);
  }
}
