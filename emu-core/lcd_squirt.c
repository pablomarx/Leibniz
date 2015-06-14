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
};

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

const char *lcd_squirt_get_address_name(lcd_squirt_t *c, uint32_t addr) {
  const char *prefix = NULL;
  switch(addr) {
    case SquirtLCDNotBusy: // lcd not busy
      prefix = "lcd-not-busy";
      break;
    default:
      prefix = "lcd-unknown";
      break;
  }
  return prefix;
}

uint32_t lcd_squirt_set_mem32(lcd_squirt_t *c, uint32_t addr, uint32_t val) {
  c->memory[addr] = val;
  return val;
}

uint32_t lcd_squirt_get_mem32(lcd_squirt_t *c, uint32_t addr) {
  uint32_t result = c->memory[addr];
  
  switch (addr) {
    case SquirtLCDNotBusy:
      result = (c->displayBusy++ % 2 ? 0x20202020 : 0x00000000);
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
