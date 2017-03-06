//
//  pcmcia.c
//  Leibniz
//
//  Created by Steve White on 2/2/17.
//  Copyright (c) 2017 Steve White. All rights reserved.
//

#include "pcmcia.h"
#include "newton.h"
#include <stdlib.h>

enum {
  PCMCIAStatus = 0x7c,
  
  PCMCIAEnabledInterrupts = 0x74,
  PCMCIAActiveInterrupts = 0x6c,
  PCMCIAClearInterrupts = 0x64,
};

void pcmcia_init (pcmcia_t *c)
{
  c->registers = calloc(0xff, sizeof(uint8_t));
  c->cardMemory = memory_new("SRAM", 0x10000000, 1024 * 1024);
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
  if (c->registers != NULL) {
    free(c->registers);
    c->registers = NULL;
  }
  if (c->cardMemory != NULL) {
    memory_delete(c->cardMemory);
    c->cardMemory = NULL;
  }
}

void pcmcia_del (pcmcia_t *c)
{
  if (c != NULL) {
    pcmcia_free (c);
    free (c);
  }
}

#pragma mark -
void pcmcia_set_log_file (pcmcia_t *c, FILE *file) {
  c->logFile = file;
}

void pcmcia_set_log_flags (pcmcia_t *c, uint32_t logFlags) {
  c->logFlags = logFlags;
}

void pcmcia_set_runt (pcmcia_t *c, runt_t *runt) {
  c->runt = runt;
}

#pragma mark -
void pcmcia_set_card_inserted (pcmcia_t *c, bool cardInserted) {
  c->cardInserted = cardInserted;

  if (cardInserted == false) {
    runt_interrupt_raise(c->runt, RuntInterruptCardLock);
  }
  runt_interrupt_raise(c->runt, RuntInterruptTric);
}

#pragma mark -
static inline const char *pcmcia_get_adress_description(uint32_t addr) {
  if ((addr >> 24) == 0x70) {
    return "CONTROL";
  }
  else {
    return "CARD";
  }
}

static inline bool pcmcia_should_log_address(pcmcia_t *c, uint32_t addr) {
  if ((addr >> 24) == 0x70) {
    return ((c->logFlags & NewtonLogPCMCIA) == NewtonLogPCMCIA);
  }
  else {
    return ((c->logFlags & NewtonLogCard) == NewtonLogCard);
  }
}

#pragma mark -
uint32_t pcmcia_set_status_mem32(pcmcia_t *c, uint32_t addr, uint32_t val) {
  uint32_t reg = ((addr & 0xff00) >> 8);
  c->registers[reg/4] = val;
  
  if (c->cardInserted == true) {
    if (reg == PCMCIAEnabledInterrupts) {
      if (val != 0) {
        runt_interrupt_raise(c->runt, RuntInterruptTric);
      }
      else {
        runt_interrupt_lower(c->runt, RuntInterruptTric);
      }
    }
    else if (reg == PCMCIAClearInterrupts) {
      runt_interrupt_lower(c->runt, RuntInterruptTric);
    }
  }

  return val;
}

uint32_t pcmcia_set_card_mem32(pcmcia_t *c, uint32_t addr, uint32_t val) {
  return memory_set_uint32(c->cardMemory, addr, val, 0);
}

uint32_t pcmcia_set_mem32(pcmcia_t *c, uint32_t addr, uint32_t val, uint32_t pc)
{
  if ((addr >> 24) == 0x70) {
    val = pcmcia_set_status_mem32(c, addr, val);
  }
  else {
    val = pcmcia_set_card_mem32(c, addr, val);
  }
  
  if (pcmcia_should_log_address(c, addr) == true) {
    fprintf(c->logFile, "[PCMCIA:WRITE:%s] PC:0x%08x addr:0x%08x => val:0x%08x\n", pcmcia_get_adress_description(addr), pc, addr, val);
  }
  
  return val;
}

static inline uint32_t pcmcia_get_register(pcmcia_t *c, uint8_t reg) {
  return c->registers[reg/4];
}

uint32_t pcmcia_get_status_mem32(pcmcia_t *c, uint32_t addr) {
  uint32_t reg = ((addr & 0xff00) >> 8);
  uint32_t result = c->registers[reg/4];
  if (reg == PCMCIAStatus) {
    uint8_t reg58 = pcmcia_get_register(c, 0x58);
    // "LOAD DIAGS TO ICCARD" writes 0x1b, and will fail with the VPP results
    // "IC CARD CHECK"'s "VPP1 and "VPP2" writes 0x0b, and will fail without the VPP results.
    if (reg58 == 0x0b || reg58 == 0x1b) {
      bool vpp1 = runt_power_state_get_subsystem(c->runt, RuntPowerVPP1);
      bool vpp2 = runt_power_state_get_subsystem(c->runt, RuntPowerVPP2);

      if (c->cardInserted == false) {
        // Diags needs this to pass the VPP tests.  It's admittedly
        // weird.  It came from inferred behavior of func 0x001d6e28 in the Notepad ROM.
        result = (vpp2 << 2) | (!vpp1 << 3) | (vpp1 << 4) | (!vpp2 << 5);
      }
      else {
        // However the inclusion of (!vpp2 << 5) causes diags to fail "IC CARD SRAM"
        // and it causes NewtonOS to report  the card is write protected, refusing to
        // format it.
        // so bit6 certainly seems to be write protect...
        result = (!vpp1 << 3) | (vpp1 << 4) | (vpp2 << 2);
      }
    }
    else if (reg58 == 0x00 || reg58 == 0x0a) {
      // Diags sets reg58 to 0x00 and reads here during
      // card detect tests.
      // NewtonOS sets reg58 to 0x0a and reads here after
      // we fire a PCMCIA IRQ, provided we return 0xff for reg 0x6c
      //
      // In both instances, the results are &'ed with 2.
      //
      // 2 seems to indicate card present as NewtonOS will
      // say the card is unreadable and offer to format it.
      //
      // For diagnostics, if we return 0 for it's first read
      // and 2 for the subsequent two reads, we'll pass the CD LO test.

      if (c->cardInserted == true) {
        result = 2;
      }
      else {
        result = 0;
      }
    }
    else {
      result = 0;
    }
  }
  // When raising a PCMCIA IRQ, 0x74 and 0x6c are read
  // If 0x6c=0xff, a lot of subsequent PCMCIA read/writes
  // are performed.
  else if (reg == PCMCIAActiveInterrupts) {
    if (c->cardInserted == true) {
      result = pcmcia_get_register(c, PCMCIAEnabledInterrupts);
    }
    else {
      result = 0;
    }
  }

  return result;
}

uint32_t pcmcia_get_card_mem32(pcmcia_t *c, uint32_t addr) {
  return memory_get_uint32(c->cardMemory, addr, 0);
}

uint32_t pcmcia_get_mem32(pcmcia_t *c, uint32_t addr, uint32_t pc)
{
  uint32_t result = 0;
  if ((addr >> 24) == 0x70) {
    result = pcmcia_get_status_mem32(c, addr);
  }
  else {
    result = pcmcia_get_card_mem32(c, addr);
  }
  
  if (pcmcia_should_log_address(c, addr) == true) {
    fprintf(c->logFile, "[PCMCIA:READ:%s] PC:0x%08x addr:0x%08x => val:0x%08x\n", pcmcia_get_adress_description(addr), pc, addr, result);
  }
  return result;
}

