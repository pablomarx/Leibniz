//
//  pcmcia.h
//  Leibniz
//
//  Created by Steve White on 2/2/17.
//  Copyright (c) 2017 Steve White. All rights reserved.
//

#ifndef __PCMCIA_H
#define __PCMCIA_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "runt.h"

typedef struct pcmcia_s {
  uint32_t *registers;
  
  uint32_t cardCapacity;
  uint32_t *cardData;
  bool cardInserted;
  
  runt_t *runt;
  
  FILE *logFile;
  uint32_t logFlags;
} pcmcia_t;

void pcmcia_init (pcmcia_t *c);
pcmcia_t *pcmcia_new (void);
void pcmcia_free (pcmcia_t *c);
void pcmcia_del (pcmcia_t *c);

uint32_t pcmcia_set_mem32(pcmcia_t *c, uint32_t addr, uint32_t val, uint32_t pc);
uint32_t pcmcia_get_mem32(pcmcia_t *c, uint32_t addr, uint32_t pc);

void pcmcia_set_log_flags (pcmcia_t *c, uint32_t logFlags);
void pcmcia_set_log_file (pcmcia_t *c, FILE *file);
void pcmcia_set_runt (pcmcia_t *c, runt_t *runt);

#endif
