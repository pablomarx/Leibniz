//
//  pcmcia.c
//  Leibniz
//
//  Created by Steve White on 2/2/17.
//  Copyright (c) 2017 Steve White. All rights reserved.
//

#include "pcmcia.h"
#include <stdlib.h>

void pcmcia_init (pcmcia_t *c)
{
    c->memory = calloc(0xffff, 4);
    c->logFile = stdout;
}

pcmcia_t *pcmcia_new (void)
{
    pcmcia_t *c;
  
    c = calloc(1, sizeof (pcmcia_t));
    if (c == NULL) {
      return (NULL);
    }
  
    pcmcia_init (c);
  
    return (c);
}

void pcmcia_free (pcmcia_t *c)
{
	if (c->memory != NULL) {
		free(c->memory);
		c->memory = NULL;
	}
}

void pcmcia_del (pcmcia_t *c)
{
    if (c != NULL) {
      pcmcia_free (c);
      free (c);
    }
}

uint32_t pcmcia_set_mem32(pcmcia_t *c, uint32_t addr, uint32_t val, uint32_t pc)
{
    fprintf(c->logFile, "[PCMCIA:WRITE] PC:0x%08x addr:0x%08x => val:0x%08x\n", pc, addr, val);
	uint32_t reg = (addr & 0xffff);
	c->memory[reg/4] = val;
	return val;
}

uint32_t pcmcia_get_mem32(pcmcia_t *c, uint32_t addr, uint32_t pc)
{
	uint32_t reg = (addr & 0xffff);
	uint32_t result = c->memory[reg/4];
	if (reg == 0x6c00) {
		result = 0xff;
	}
	else if (reg == 0x7400) {
		result = 16;
	}

    fprintf(c->logFile, "[PCMCIA:READ] PC:0x%08x addr:0x%08x => val:0x%08x\n", pc, addr, result);
	return result;
}

void pcmcia_set_log_file (pcmcia_t *c, FILE *file) {
  c->logFile = file;
}
