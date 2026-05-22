//
//  pcmcia.c
//  Leibniz
//
//  Created by Steve White on 2/2/17.
//  Copyright (c) 2017 Steve White. All rights reserved.
//

#include "pcmcia.h"
#include "newton.h"
#include "utils.h"

#include <stdlib.h>
#include <strings.h>

enum {
  PCMCIAStatus = 0x7c,
  
  PCMCIAEnabledInterrupts = 0x74,
  PCMCIAActiveInterrupts = 0x6c,
  PCMCIAClearInterrupts = 0x64,
};

enum {
  CardDetect    = (1 << 3),
  BatteryDetect = (1 << 4),
  WriteProtect  = (1 << 5),
};


void pcmcia_init (pcmcia_t *c)
{
  c->registers = calloc(0xff, sizeof(uint8_t));
  c->cardData = memory_new("CARD_DATA", 0x14000000, 4 * 1024 * 1024);
  c->cardCIS = memory_new("CARD_CIS", 0x10000000, 4096);
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
  if (c->cardData != NULL) {
    memory_delete(c->cardData);
    c->cardData = NULL;
  }
  if (c->cardCIS != NULL) {
    memory_delete(c->cardCIS);
    c->cardCIS = NULL;
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
    memory_clear(c->cardCIS);
    memory_clear(c->cardData);
  }
  runt_interrupt_raise(c->runt, RuntInterruptTric);
}

bool pcmcia_get_card_inserted (pcmcia_t *c) {
    return c->cardInserted;
}

bool pcmcia_set_pccard_data (pcmcia_t *c, uint8_t *data, uint32_t length) {
  if (length < 52) {
    printf("Too small to be correct\n");
    return false;
  }

  if (strncasecmp(&data[length-12], "TLinearCard", 12) != 0) {
    printf("Not a PC Card??\n");
    return false;
  }
  
#define READ32(_x) ((data[_x] << 24) | (data[_x + 1] << 16) | (data[_x + 2] << 8) | (data[_x + 3]))
  
  uint32_t version = READ32(length-16);
  if (version != 1) {
    printf("Only support PC card version 1\n");
    return false;
  }

  uint32_t dataStart = READ32(length-24);
  uint32_t dataSize = READ32(length-28);
  if (dataStart + dataSize > length) {
    printf("Bad data sizes?\n");
    return false;
  }
  
  uint32_t cisStart = READ32(length-32);
  uint32_t cisSize = READ32(length-36);
  if (cisStart + cisSize > length) {
    printf("Bad CIS sizes?\n");
    return false;
  }

  if (dataSize + cisSize > length) {
    printf("Bad combined size?\n");
    return false;
  }
  for (uint32_t addr=dataStart; addr<dataStart+dataSize; addr+=4) {
    memory_set_uint32(c->cardData, addr-dataStart, READ32(addr), 0);
  }
  for (uint32_t addr=cisStart; addr<cisStart+cisSize; addr+=4) {
    memory_set_uint32(c->cardCIS, addr-dataStart, READ32(addr), 0);
  }
  return true;
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
  return true;
  if ((addr >> 24) == 0x70) {
    return ((c->logFlags & NewtonLogPCMCIA) == NewtonLogPCMCIA);
  }
  else if ((addr >> 24) == 0x10) {
    return ((c->logFlags & NewtonLogCardReg) == NewtonLogCardReg);
  }
  else {
    return ((c->logFlags & NewtonLogCardData) == NewtonLogCardData);
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

uint32_t pcmcia_set_card_reg_mem32(pcmcia_t *c, uint32_t addr, uint32_t val) {
    // This shouldn't be writable?
    return 0xffffffff;
}

uint32_t pcmcia_set_card_data_mem32(pcmcia_t *c, uint32_t addr, uint32_t val) {
  return memory_set_uint32(c->cardData, addr, val, 0);
}

uint32_t pcmcia_set_mem32(pcmcia_t *c, uint32_t addr, uint32_t val, uint32_t pc)
{
  if ((addr >> 24) == 0x70) {
    val = pcmcia_set_status_mem32(c, addr, val);
  }
  else if ((addr >> 24) == 0x10) {
    val = pcmcia_set_card_reg_mem32(c, addr, val);
  }
  else {
    val = pcmcia_set_card_data_mem32(c, addr, val);
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
        result = CardDetect | BatteryDetect;
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

uint32_t pcmcia_get_card_reg_mem32(pcmcia_t *c, uint32_t addr) {
  if (c->cardInserted == false) {
    return 0xffffffff;
  }
  
  addr = addr & 0xffffff;
  if (addr < 4096) {
    // CIS region of card
    // Verified with the stdin/stdout log from the J1 Armistice image:
    // $$Recognized card: TCardHandlerMemory::intel SERIES2-02  200 ns 2048K
    // $$Send Client     kNewCard 'flsh' 14FB 44216EC off=0 size=200000 BadCIS=0
    //
    // $$Recognized card: TCardHandlerMemory::  250 ns 1024K bytes
    // $$Send Client   kNewCard 'rom ' 14FB 0 off=1000 size=FF000 BadCIS=0
#if 1
    uint8_t index = addr / 2;
    uint8_t high = memory_get_uint8(c->cardCIS, index, 0);
    uint8_t low = memory_get_uint8(c->cardCIS, index + 1, 0);
    return (0xff00ff00 | (low << 16) | high);
#else
    static uint8_t cis[] = {
      0x01, 0x03, 0x52, 0x06, 0xFF, 0x1E, 0x06, 0x02, 0x11, 0x01, 0x01, 0x03, 0x01, 0x18, 0x02, 0x89,
      0xA2, 0x15, 0x50, 0x04, 0x01, 0x69, 0x6E, 0x74, 0x65, 0x6C, 0x00, 0x53, 0x45, 0x52, 0x49, 0x45,
      0x53, 0x32, 0x2D, 0x30, 0x32, 0x20, 0x00, 0x32, 0x48, 0x20, 0x52, 0x45, 0x47, 0x42, 0x41, 0x53,
      0x45, 0x20, 0x34, 0x30, 0x30, 0x30, 0x68, 0x20, 0x44, 0x42, 0x42, 0x44, 0x52, 0x45, 0x4C, 0x50,
      0x00, 0x43, 0x4F, 0x50, 0x59, 0x52, 0x49, 0x47, 0x48, 0x54, 0x20, 0x69, 0x6E, 0x74, 0x65, 0x6C,
      0x20, 0x43, 0x4F, 0x52, 0x50, 0x4F, 0x52, 0x41, 0x54, 0x49, 0x4F, 0x4E, 0x20, 0x31, 0x39, 0x39,
      0x31, 0x00, 0xFF, 0x1A, 0x06, 0x01, 0x00, 0x00, 0x40, 0x03, 0xFF, 0xFF,
    };
    uint8_t index = addr / 2;
    if (index < countof(cis)) {
      uint8_t high = cis[index];
      uint8_t low = cis[index+1];
      return (0xff00ff00 | (low << 16) | high);
    }
#endif
    return 0xffffffff;
  }
  else if (addr == 0x4104) {
    // If bit 1 is set, NewtonOS complains:
    // "The memory card is write protected"
    //
    // Perhaps these bits reflect READY, WAIT#, WP ?
    return 0b101;
  }
  else if (addr == 0x4118) {
    // This gets read a lot when trying to format
    // a card.  Unsure what the purpose is.
    return 1;
  }
  return 0xffffffff;
}

uint32_t pcmcia_get_card_data_mem32(pcmcia_t *c, uint32_t addr) {
  if (c->cardInserted == false) {
    return 0xffffffff;
  }
  return memory_get_uint32(c->cardData, addr, 0);
}

uint32_t pcmcia_get_mem32(pcmcia_t *c, uint32_t addr, uint32_t pc)
{
  uint32_t result = 0;
  uint8_t type = (addr >> 24);
  if (type == 0x70) {
    result = pcmcia_get_status_mem32(c, addr);
  }
  else if (type == 0x10) {
    result = pcmcia_get_card_reg_mem32(c, addr);
  }
  else {
    result = pcmcia_get_card_data_mem32(c, addr);
  }
  
  if (pcmcia_should_log_address(c, addr) == true) {
    fprintf(c->logFile, "[PCMCIA:READ:%s] PC:0x%08x addr:0x%08x => val:0x%08x\n", pcmcia_get_adress_description(addr), pc, addr, result);
  }
  return result;
}

