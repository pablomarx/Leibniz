//
//  lcd_squirt.h
//  Leibniz
//
//  Created by Steve White on 6/13/15.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#ifndef __Leibniz__lcd_squirt__
#define __Leibniz__lcd_squirt__

#include "arm.h"

#include <stdbool.h>
#include <stdio.h>

typedef struct lcd_squirt_s {
    FILE *logFile;
  uint32_t *memory;

  int displayFillMode;
  int displayOrientation;
  int displayInverse;

  int displayDirty;
  int stepsSinceLastFlush;
  uint16_t displayCursor;
  uint8_t displayPixelInvert;
  unsigned char *displayFramebuffer;
} lcd_squirt_t;

void lcd_squirt_init (lcd_squirt_t *c);
lcd_squirt_t *lcd_squirt_new ();
void lcd_squirt_free (lcd_squirt_t *c);
void lcd_squirt_del (lcd_squirt_t *c);

void lcd_squirt_set_log_file (lcd_squirt_t *c, FILE *file);

void lcd_squirt_step(lcd_squirt_t *c);

uint32_t lcd_squirt_set_mem32(lcd_squirt_t *c, uint32_t addr, uint32_t val);
uint32_t lcd_squirt_get_mem32(lcd_squirt_t *c, uint32_t addr);

const char *lcd_squirt_get_address_name(lcd_squirt_t *c, uint32_t addr);

extern void FlushDisplay(const char *display, int width, int height);

#endif