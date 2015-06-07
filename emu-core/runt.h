//
//  runt.h
//  Leibniz
//
//  Created by Steve White on 9/13/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#ifndef __Leibniz__runt__
#define __Leibniz__runt__

#include "arm.h"

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

enum {
  RuntLogNone       = 0x00000000,
  
  RuntLogInterrupts = (1 << 1),
  RuntLogTimer      = (1 << 2),
  RuntLogTicks      = (1 << 3),
  RuntLogTablet     = (1 << 4),
  RuntLogLCD        = (1 << 5),
  RuntLogIR         = (1 << 6),
  RuntLogSerial     = (1 << 7),
  RuntLogPower      = (1 << 8),
  RuntLogSwitch     = (1 << 9),
  RuntLogSound      = (1 << 10),
  RuntLogRTC        = (1 << 11),
  RuntLogUnknown    = (1 << 12),

  RuntLogAll        = 0xffffffff,
};

typedef struct runt_s {
  arm_t *arm;
  uint32_t *memory;
  time_t bootTime;
  
  // Logging
  uint32_t logFlags;
  FILE *logFile;
  
  // Interrupts
  uint32_t interrupt;
  uint32_t interruptStick;
  
  // Display
  int displayFillMode;
  int displayOrientation;
  int displayInverse;
  int displayCursorX;
  int displayCursorY;
  int displayBusy;
  int displayDirty;
  unsigned char *displayFramebuffer;

  // Switches
  int8_t switches[3];
  
  // Tablet
  bool touchActive;
  uint32_t touchX;
  uint32_t touchY;
} runt_t;

void runt_init (runt_t *c);
runt_t *runt_new (void);
void runt_free (runt_t *c);
void runt_del (runt_t *c);
void runt_set_arm (runt_t *c, arm_t *arm);

void runt_set_log_flags (runt_t *c, unsigned flags, int val);
void runt_set_log_file (runt_t *c, FILE *file);

uint32_t runt_set_mem32(runt_t *c, uint32_t addr, uint32_t val);
uint32_t runt_get_mem32(runt_t *c, uint32_t addr);

void runt_switch_state(runt_t *c, int switchNum, int state);

void runt_touch_down(runt_t *c, int x, int y);
void runt_touch_up(runt_t *c);

extern void FlushDisplay(const char *display, int width, int height);

#endif /* defined(__Leibniz__runt__) */
