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

memory_t *memory_new(char *name, uint32_t length) {
  memory_t *mem = calloc(sizeof(memory_t), 1);
  mem->contents = calloc(sizeof(uint8_t), length);
  mem->length = length;
  mem->logFile = stdout;

  if (name != NULL) {
    mem->name = calloc(sizeof(char), strlen(name) + 1);
    strcpy(mem->name, name);
  }
  
  return mem;
}

void memory_delete(memory_t *mem) {
  free(mem->contents);
  free(mem->name);
  free(mem);
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

uint32_t memory_get_uint32(memory_t *mem, uint32_t address, uint32_t pc) {
  uint32_t result = mem->contents[(address % mem->length)/4];
  
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
    mem->contents[(address % mem->length)/4] = val;
  }
  
  return val;
}
