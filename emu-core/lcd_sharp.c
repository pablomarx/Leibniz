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
  SharpLCDBitMask   = 0x0c,  // BM
  
  SharpLCDIDR       = 0x10,  // IDR
  SharpLCDIDW       = 0x14,  // IDW
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
  
  SharpLCDWindowLeft_l   = 0x60,  // XLTL
  SharpLCDWindowLeft_h   = 0x64,  // XLTH
  SharpLCDWindowTop_l    = 0x68,  // YTL
  SharpLCDWindowTop_h    = 0x6c,  // YTH
  
  SharpLCDWindowRight_l  = 0x70,  // XRTL
  SharpLCDWindowRight_h  = 0x74,  // XRTH
  SharpLCDWindowBottom_l = 0x78,  // YBL
  SharpLCDWindowBottom_h = 0x7c,  // YBH
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
    case SharpLCDIDR:
      prefix = "lcd-idr";
      break;
    case SharpLCDIDW:
      prefix = "lcd-idw";
      break;
    case SharpLCDFlush:
      prefix = "lcd-flush";
      break;
      
    case SharpLCDBitMask:
      prefix = "lcd-bitmask";
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
      
    case SharpLCDWindowLeft_l:
      prefix = "lcd-window-left_low";
      break;
    case SharpLCDWindowLeft_h:
      prefix = "lcd-window-left_high";
      break;
    case SharpLCDWindowTop_l:
      prefix = "lcd-window-top_low";
      break;
    case SharpLCDWindowTop_h:
      prefix = "lcd-window-top_high";
      break;
      
    case SharpLCDWindowRight_l:
      prefix = "lcd-window-right_low";
      break;
    case SharpLCDWindowRight_h:
      prefix = "lcd-window-right_high";
      break;
    case SharpLCDWindowBottom_l:
      prefix = "lcd-window-bottom_low";
      break;
    case SharpLCDWindowBottom_h:
      prefix = "lcd-window-bottom_high";
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
  
  
  for (int i=7; i>=0; i--, x++) {
    if (x >= SCREEN_WIDTH) {
      x = 0;
      y++;
    }
    if (y >= SCREEN_HEIGHT) {
      y = 0;
    }
    
    if (x < c->windowLeft || x > c->windowRight || y < c->windowTop || y > c->windowBottom) {
      continue;
    }
    if (((c->bitMask >> i) & 1) == 0) {
      continue;
    }
    
    int offset = (y * SCREEN_WIDTH) + (x);
    if (offset < 0 || offset >= SCREEN_HEIGHT * SCREEN_WIDTH) {
      offset = 0;
    }

    uint8_t pixel = ((val >> i) & 1) ? BLACK_COLOR : WHITE_COLOR;

    if (c->fillMode == SharpLCDFillModeInvert) {
      // Invert existing pixels...
      pixel = c->displayFramebuffer[offset];
      pixel = (pixel == WHITE_COLOR) ? BLACK_COLOR : WHITE_COLOR;
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
  
  if (c->idw == 0x05 || c->idw == 0x06) {
    x -= 8;
    if (x < 0) {
      y--;
      x = SCREEN_WIDTH - x;
    }

    if (c->idw == 0x06) {
      y--;
    }
    else {
      y++;
    }
      
    if (y >= SCREEN_HEIGHT) {
      y = y - SCREEN_HEIGHT;
    }
    else if (y < 0) {
      y = y + SCREEN_HEIGHT;
    }
  }
  
  c->writeX = x;
  c->writeY = y;
  
  c->displayDirty = true;
}

static inline void lcd_sharp_flush_framebuffer(lcd_sharp_t *c) {
  newton_display_set_framebuffer((const char *)c->displayFramebuffer, SCREEN_WIDTH, SCREEN_HEIGHT);
  c->displayDirty = false;
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
    case SharpLCDBitMask:
      c->bitMask = byteVal;
      break;
    case SharpLCDIDR:
      c->idr = byteVal;
      break;
    case SharpLCDIDW:
      c->idw = byteVal;
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
      
    case SharpLCDWindowLeft_l:
      c->windowLeft = (c->windowLeft & 0xff00) | byteVal;
      break;
    case SharpLCDWindowLeft_h:
      c->windowLeft = (c->windowLeft & 0x00ff) | (byteVal << 8);
      break;
    case SharpLCDWindowTop_l:
      c->windowTop = (c->windowTop & 0xff00) | byteVal;
      break;
    case SharpLCDWindowTop_h:
      c->windowTop = (c->windowTop & 0x00ff) | (byteVal << 8);
      break;
      
    case SharpLCDWindowRight_l:
      c->windowRight = (c->windowRight & 0xff00) | byteVal;
      break;
    case SharpLCDWindowRight_h:
      c->windowRight = (c->windowRight & 0x00ff) | (byteVal << 8);
      break;
    case SharpLCDWindowBottom_l:
      c->windowBottom = (c->windowBottom & 0xff00) | byteVal;
      break;
    case SharpLCDWindowBottom_h:
      c->windowBottom = (c->windowBottom & 0x00ff) | (byteVal << 8);
      break;
      
    case SharpLCDPixelData:
      lcd_sharp_write_pixels(c, byteVal);
      break;
      
    case SharpLCDFlush:
      if (c->displayDirty == true) {
        lcd_sharp_flush_framebuffer(c);
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
      
    case SharpLCDBitMask:
      result = c->bitMask;
      break;
      
    case SharpLCDIDR:
      result = c->idr;
      break;
    case SharpLCDIDW:
      result = c->idw;
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
      
    case SharpLCDWindowLeft_l:
      result = (c->windowLeft & 0xff);
      break;
    case SharpLCDWindowLeft_h:
      result = (c->windowLeft >> 8) & 0xff;
      break;
    case SharpLCDWindowTop_l:
      result = (c->windowTop & 0xff);
      break;
    case SharpLCDWindowTop_h:
      result = (c->windowTop >> 8) & 0xff;
      break;
      
    case SharpLCDWindowRight_l:
      result = (c->windowRight & 0xff);
      break;
    case SharpLCDWindowRight_h:
      result = (c->windowRight >> 8) & 0xff;
      break;
    case SharpLCDWindowBottom_l:
      result = (c->windowBottom & 0xff);
      break;
    case SharpLCDWindowBottom_h:
      result = (c->windowBottom >> 8) & 0xff;
      break;
      
  }
  
  return (result << 24);
}

void lcd_sharp_set_powered (lcd_sharp_t *c, bool powered) {
  uint8_t blackColor = (powered ? BLACK_COLOR : SLEEP_COLOR);
  for (int i=0; i<(SCREEN_WIDTH*SCREEN_HEIGHT);i++) {
    if (c->displayFramebuffer[i] != 0xff) {
      c->displayFramebuffer[i] = blackColor;
    }
  }
  lcd_sharp_flush_framebuffer(c);
}

void lcd_sharp_set_log_file (lcd_sharp_t *c, FILE *file) {
  c->logFile = file;
}

void lcd_sharp_init (lcd_sharp_t *c) {
  c->memory = calloc(0xff, sizeof(uint32_t));
  
  c->windowRight = SCREEN_WIDTH - 1;
  c->windowBottom = SCREEN_HEIGHT - 1;
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
