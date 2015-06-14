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
  // Sharp Driver
  SharpLCDNotBusy = 0x04,
  
  SharpLCDUnknown1 = 0x34, // 2 calls
  SharpLCDSync = 0x38,
  SharpLCDVersion = 0x3c,
  
  SharpLCDPixelData = 0x88,
  SharpLCDUnknown2 = 0x8c, // 13692 calls
  
  SharpLCDUnknown3 = 0x94, // 12956 calls
  
  SharpLCDFillMode = 0xa0,
  SharpLCDUnknown4 = 0xa4, // 2 calls
  SharpLCDUnknown5 = 0xac, // 14 calls
  
  SharpLCDContrast = 0xb0,
  SharpLCDUnknown6 = 0xb4, // 6 calls
  SharpLCDFlush = 0xb8,
  
  SharpLCDCursorXLSB = 0xc0,
  SharpLCDCursorXMSB = 0xc4,
  SharpLCDCursorYLSB = 0xc8,
  SharpLCDCursorYMSB = 0xcc,
  
  SharpLCDDisplayInverse = 0xe0,
  SharpLCDUnknown8 = 0xe4, // 66 calls
  SharpLCDUnknown9 = 0xe8, // 66 calls
  SharpLCDDisplayOrientation = 0xec,
  
  SharpLCDScreenHeightLSB = 0xf0,
  SharpLCDScreenHeightMSB = 0xf4,
  SharpLCDScreenWidthLSB = 0xf8,
  SharpLCDScreenWidthMSB = 0xfc,
};

#define SCREEN_WIDTH 336
#define SCREEN_HEIGHT 240

const char *lcd_sharp_get_address_name(lcd_sharp_t *c, uint32_t addr) {
  const char *prefix = NULL;
  switch(addr) {
    case SharpLCDNotBusy: // lcd not busy
      prefix = "lcd-not-busy";
      break;
    case SharpLCDSync: // lcd sync
      prefix = "lcd-sync";
      break;
    case SharpLCDContrast: // lcd contrast?
      prefix = "lcd-contrast";
      break;
    case SharpLCDFillMode:
      prefix = "lcd-fill-mode";
      break;
    case SharpLCDDisplayInverse:
      prefix = "lcd-display-inverse";
      break;
    case SharpLCDDisplayOrientation:
      prefix = "lcd-display-orientation";
      break;
    case SharpLCDPixelData:
      prefix = "lcd-pixel-data";
      break;
    case SharpLCDCursorXLSB:
      prefix = "lcd-cursor-x-lsb";
      break;
    case SharpLCDCursorXMSB:
      prefix = "lcd-cursor-x-msb";
      break;
    case SharpLCDCursorYLSB:
      prefix = "lcd-cursor-y-lsb";
      break;
    case SharpLCDCursorYMSB:
      prefix = "lcd-cursor-y-msb";
      break;
    case SharpLCDScreenHeightLSB:
      prefix = "lcd-screen-height-lsb";
      break;
    case SharpLCDScreenHeightMSB:
      prefix = "lcd-screen-height-msb";
      break;
    case SharpLCDScreenWidthLSB:
      prefix = "lcd-screen-width-lsb";
      break;
    case SharpLCDScreenWidthMSB:
      prefix = "lcd-screen-width-msb";
      break;
    case SharpLCDFlush:
      prefix = "lcd-flush";
      break;
    default:
      prefix = "lcd-unknown";
      break;
  }
  return prefix;
}

uint32_t lcd_sharp_set_mem32(lcd_sharp_t *c, uint32_t addr, uint32_t val) {
  c->memory[addr/4] = val;
  val = val >> 24;
  switch (addr) {
    case SharpLCDCursorXLSB:
      c->displayCursorX = (val & 0xff);
      if (c->displayCursorX < 0) {
        fprintf(c->logFile, "bad x coordinate: 0x%08x\n", val);
      }
      break;
    case SharpLCDCursorXMSB:
      c->displayCursorX = ((val & 0xff) << 8) | c->displayCursorX;
      break;
    case SharpLCDCursorYMSB:
      c->displayCursorY = ((val & 0xff) << 8) | c->displayCursorY;
      break;
    case SharpLCDCursorYLSB:
      c->displayCursorY = (val & 0xff);
      if (c->displayCursorY < 0) {
        fprintf(c->logFile, "bad y coordinate: 0x%08x\n", val);
      }
      break;
    case SharpLCDFillMode:
      c->displayFillMode = ((val & 0xff) == 0x03);
      break;
    case SharpLCDDisplayInverse:
      c->displayInverse = (val & 0xff);
      break;
    case SharpLCDDisplayOrientation:
      c->displayOrientation = (val == 0x00); //1;//((data & 0xff) == 0x00);
      break;
      
    case SharpLCDPixelData: {
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
    case SharpLCDFlush:
      if (c->displayDirty) {
        FlushDisplay((const char *)c->displayFramebuffer, SCREEN_WIDTH, SCREEN_HEIGHT);
        c->displayDirty = 0;
      }
      break;
  }
  return val;
}

uint32_t lcd_sharp_get_mem32(lcd_sharp_t *c, uint32_t addr) {
  uint32_t result = c->memory[addr/4];
  
  switch (addr) {
    case SharpLCDNotBusy:
      result = (c->displayBusy++ % 2 ? 0x02000002 : 0x00000000);
      break;
      
    case SharpLCDSync:
      result = 165;
      break;
      
    case SharpLCDVersion:
      result = 91;
      break;
  }
  
  return result;
}

void lcd_sharp_set_log_file (lcd_sharp_t *c, FILE *file) {
  c->logFile = file;
}

void lcd_sharp_init (lcd_sharp_t *c) {
  c->memory = calloc(0xff, 1);
  
  //
  // Display
  //
  c->displayFramebuffer = calloc(SCREEN_WIDTH * SCREEN_HEIGHT, 1);
  memset(c->displayFramebuffer, 0xff, SCREEN_WIDTH * SCREEN_HEIGHT);
  
  c->logFile = stdout;
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
