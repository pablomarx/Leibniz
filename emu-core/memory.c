//
//  memory.c
//  Leibniz
//
//  Created by Steve White on 4/20/16.
//  Copyright © 2016 Steve White. All rights reserved.
//

#include "memory.h"

#include <stdlib.h>
#include <string.h>

memory_t *memory_new(char *name, uint32_t base, uint32_t length) {
  memory_t *mem = calloc(1, sizeof(memory_t));
  mem->contents = calloc(length, sizeof(uint8_t));
  
  memory_add_mapping(mem, base, 0, length);

  mem->logFile = stdout;

  if (name != NULL) {
    mem->name = calloc(strlen(name) + 1, sizeof(char));
    strcpy(mem->name, name);
  }
  
  return mem;
}

void memory_delete(memory_t *mem) {
  if (mem->contents != NULL) {
    free(mem->contents);
  }
  if (mem->name != NULL) {
    free(mem->name);
  }
  while (mem->mappings != NULL) {
    memory_map_t *next = mem->mappings->next;
    free(mem->mappings);
    mem->mappings = next;
  }
  free(mem);
}

void memory_clear(memory_t *mem) {
  memset(mem->contents, 0, memory_get_length(mem));
}

uint32_t memory_get_length(memory_t *mem) {
  return mem->mappings->length;
}

void memory_set_readonly(memory_t *mem, bool readOnly) {
  mem->readOnly = readOnly;
}

bool memory_get_readonly(memory_t *mem) {
  return mem->readOnly;
}

void memory_set_logs_reads(memory_t *mem, bool logsReads) {
  mem->logsReads = logsReads;
}

bool memory_get_logs_reads(memory_t *mem) {
  return mem->logsReads;
}

void memory_set_logs_writes(memory_t *mem, bool logsWrites) {
  mem->logsWrites = logsWrites;
}

bool memory_get_logs_writes(memory_t *mem) {
  return mem->logsWrites;
}

void memory_set_log_file(memory_t *mem, FILE *logFile) {
  mem->logFile = logFile;
}

FILE *memory_get_log_file(memory_t *mem) {
  return mem->logFile;
}

void memory_add_mapping(memory_t *mem, uint32_t virtaddr, uint32_t physaddr, uint32_t length) {
  memory_map_t *map = calloc(1, sizeof(memory_map_t));
  map->length = length;
  map->virtaddr = virtaddr;
  map->physaddr = physaddr;
  map->next = mem->mappings;
  mem->mappings = map;
}

static inline uint32_t memory_physaddr_for_virtaddr(memory_t *mem, uint32_t virtaddr) {
  memory_map_t *map = mem->mappings;
  if (map->next != NULL) {
    while (map != NULL) {
      if (map->virtaddr <= virtaddr && map->virtaddr + map->length > virtaddr) {
        break;
      }
      if (map->next == NULL) {
        break;
      }
      map = map->next;
    }
  }
  return map->physaddr + ((virtaddr - map->virtaddr) % map->length);
}

uint32_t memory_get_uint32(memory_t *mem, uint32_t address, uint32_t pc) {
  uint32_t physaddr = memory_physaddr_for_virtaddr(mem, address);
  uint32_t result = mem->contents[physaddr/4];
  
  if (mem->logsReads == true) {
    fprintf(mem->logFile, "[%s:READ] PC:0x%08x addr:0x%08x => val:0x%08x\n", mem->name, pc, address, result);
  }

  return result;
}

uint32_t memory_set_uint32(memory_t *mem, uint32_t address, uint32_t val, uint32_t pc) {
  if (mem->logsWrites == true) {
    fprintf(mem->logFile, "[%s:WRITE] PC:0x%08x addr:0x%08x => val:0x%08x\n", mem->name, pc, address, val);
  }

  if (mem->readOnly == true) {
    fprintf(mem->logFile, "[%s:WRITE] Attempted write to read-only memory! PC:0x%08x addr:0x%08x => val:0x%08x\n", mem->name, pc, address, val);
  }
  else {
    uint32_t physaddr = memory_physaddr_for_virtaddr(mem, address);
    mem->contents[physaddr/4] = val;
  }
  
  return val;
}

uint8_t memory_get_uint8(memory_t *mem, uint32_t addr, uint32_t pc) {
  int bytenum = addr & 3;
  uint32_t aligned = addr - (bytenum);

  uint32_t physaddr = memory_physaddr_for_virtaddr(mem, aligned);

  uint32_t word = mem->contents[physaddr/4];
  word >>= ((3-bytenum) * 8);

  uint8_t result = (word & 0xff);
  
  if (mem->logsReads == true) {
    fprintf(mem->logFile, "[%s:READ] PC:0x%08x addr:0x%08x => val:0x%02x\n", mem->name, pc, addr, result);
  }

  return result;
}

uint8_t memory_set_uint8(memory_t *mem, uint32_t addr, uint8_t val, uint32_t pc) {
  static const unsigned masktab[] = {
    0x00ffffff, 0xff00ffff, 0xffff00ff, 0xffffff00
  };
  
  int bytenum = addr & 3;
  uint32_t aligned = addr - bytenum;
  uint32_t mask = masktab[addr & 3];
  
  uint32_t physaddr = memory_physaddr_for_virtaddr(mem, aligned);
  uint32_t word = mem->contents[physaddr/4];
  
  uint32_t newval = word & mask;
  newval |= (val << ((3 - bytenum) * 8));
  
  if (mem->logsWrites == true) {
    fprintf(mem->logFile, "[%s:WRITE] PC:0x%08x addr:0x%08x => val:0x%02x\n", mem->name, pc, addr, val);
  }
  
  if (mem->readOnly == true) {
    fprintf(mem->logFile, "[%s:WRITE] Attempted write to read-only memory! PC:0x%08x addr:0x%08x => val:0x%02x\n", mem->name, pc, addr, val);
  }
  else {
    mem->contents[physaddr/4] = newval;
  }

  
  return val;
}
