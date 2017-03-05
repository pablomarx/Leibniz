//
//  memory.h
//  Leibniz
//
//  Created by Steve White on 4/20/16.
//  Copyright © 2016 Steve White. All rights reserved.
//

#ifndef memory_h
#define memory_h

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct memory_map_s memory_map_t;

struct memory_map_s {
  uint32_t virtaddr;
  uint32_t physaddr;
  uint32_t length;
  memory_map_t *next;
};

typedef struct memory_s {
  char *name;
  
  uint32_t *contents;
  uint32_t length;
  
  memory_map_t *mappings;
  uint32_t mappingCount;
  
  bool readOnly;
  bool logsReads;
  bool logsWrites;
  FILE *logFile;
} memory_t;

memory_t *memory_new(char *name, uint32_t base, uint32_t length);
void memory_delete(memory_t *mem);

void memory_clear(memory_t *mem);

uint32_t memory_get_length(memory_t *mem);

void memory_write_to_file(memory_t *mem, const char *file);

void memory_set_readonly(memory_t *mem, bool readOnly);
bool memory_get_readonly(memory_t *mem);

void memory_set_logs_reads(memory_t *mem, bool logsReads);
bool memory_get_logs_reads(memory_t *mem);

void memory_set_logs_writes(memory_t *mem, bool logsWrites);
bool memory_get_logs_writes(memory_t *mem);

void memory_set_log_file(memory_t *mem, FILE *logFile);
FILE *memory_get_log_file(memory_t *mem);

void memory_add_mapping(memory_t *mem, uint32_t virtaddr, uint32_t physaddr, uint32_t length);

uint32_t memory_get_uint32(memory_t *mem, uint32_t address, uint32_t pc);
uint32_t memory_set_uint32(memory_t *mem, uint32_t address, uint32_t val, uint32_t pc);

uint8_t memory_get_uint8(memory_t *mem, uint32_t address, uint32_t pc);
uint8_t memory_set_uint8(memory_t *mem, uint32_t address, uint8_t val, uint32_t pc);

#endif /* memory_h */
