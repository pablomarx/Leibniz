//
//  lcd_sharp.h
//  Leibniz
//
//  Created by Steve White on 6/13/15.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#ifndef __Leibniz__lcd_sharp__
#define __Leibniz__lcd_sharp__

#include "arm.h"
#include "lcd.h"

#include <stdbool.h>
#include <stdio.h>

typedef struct lcd_sharp_s {
  FILE *logFile;
  uint32_t *memory;
  
  uint16_t writeX;
  uint16_t writeY;
  uint16_t readX;
  uint16_t readY;
  uint16_t windowW;
  uint16_t windowH;
  uint16_t windowX;
  uint16_t windowY;
  
  uint8_t contrast;
  uint8_t fillMode;
  
  bool displayBusy;
  int displayDirty;
  unsigned char *displayFramebuffer;
} lcd_sharp_t;

void lcd_sharp_init (lcd_sharp_t *c);
lcd_sharp_t *lcd_sharp_new ();
void lcd_sharp_free (lcd_sharp_t *c);
void lcd_sharp_del (lcd_sharp_t *c);

void lcd_sharp_set_log_file (lcd_sharp_t *c, FILE *file);

uint32_t lcd_sharp_set_mem32(lcd_sharp_t *c, uint32_t addr, uint32_t val);
uint32_t lcd_sharp_get_mem32(lcd_sharp_t *c, uint32_t addr);

const char *lcd_sharp_get_address_name(lcd_sharp_t *c, uint32_t addr);

#endif
