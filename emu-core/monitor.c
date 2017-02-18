//
//  monitor.c
//  Leibniz
//
//  Created by Steve White on 9/14/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#include "monitor.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "hexdump.h"
#include "linenoise.h"
#include "internal.h"

monitor_t *gMonitor = NULL;

void monitor_interrupt() {
  if (gMonitor == NULL) {
    exit(-1);
  }
  
  newton_stop(gMonitor->newton);
}

void monitor_init (monitor_t *c) {
  linenoiseHistoryLoad("history.txt"); /* Load the history at startup */
  
  struct sigaction action;
  action.sa_handler = monitor_interrupt;
  sigemptyset (&action.sa_mask);
  action.sa_flags = 0;
  
  sigaction(SIGINT, &action, NULL);
  
  c->lastInput = strdup("");
}

monitor_t *monitor_new (void) {
  monitor_t *c;
  
  c = calloc(sizeof (monitor_t), 1);
  if (c == NULL) {
    return (NULL);
  }
  
  monitor_init (c);
  
  return (c);
}

void monitor_free (monitor_t *c) {
  if (c->lastInput != NULL) {
    free(c->lastInput);
  }
}

void monitor_del (monitor_t *c) {
  if (c != NULL) {
    monitor_free (c);
    free (c);
  }
}

void monitor_set_newton (monitor_t *c, newton_t *newton) {
  c->newton = newton;
}

void monitor_dump_mmu (monitor_t *c) {
  arm_copr15_t *cp15 = arm_get_mmu(c->newton->arm);
  uint32_t pageTable = cp15->reg[2];
  printf("page table: 0x%08x\n", pageTable);
  if (pageTable == 0x00) {
    return;
  }
  
  bool trace = c->newton->memTrace;
  c->newton->memTrace = false;
  
  /*
   * ARM architecture-based application processors implement an MMU
   * defined by ARM's virtual memory system architecture. The current
   * architecture defines PTEs for describing 4 KB and 64 KB pages,
   * 1 MB sections and 16 MB super-sections; legacy versions also
   * defined a 1 KB tiny page. The ARM uses a two-level page table if
   * using 4 KB and 64 KB pages, or just a one-level page table for
   * 1 MB sections and 16 MB sections.
   */
  
  /*
   * First Level Table
   * • This table (Translation table in ARM parlance) consists
   *   of 4096, 32 bit entries
   * • Each entry covers 1MB of ARM address space
   * • Each entry points to physical address of a level 2 table
   * Alignment required for level 1 page table: 4096 * 4 bytes
   *
   * Second level table
   * • This table (Page table in ARM parlance) consists of 256 entries in each
   * • Each entry covers 4kB of ARM address space
   * • Each entry points to a 4kB block of physical memory
   * Alignment required for level 2 page table: 256 * 4 bytes
   */
  
  printf("virtual        physical        type  perm  domain  flags\n");
  printf("--------------------------------------------------------\n");
  for (int i=0; i<4096; i++) {
    uint32_t translationTableEntry = pageTable + (i * 4);
    uint32_t desc1 = newton_get_mem32(c->newton, translationTableEntry);
    uint8_t type = (desc1 & 0x03);
    if (type == 0x00) {
      continue;
    }
    
    const char *typeDesc = NULL;
    uint32_t mask = 0;
    uint32_t domain = 0;
    uint32_t permission = 0;
    switch (type) {
      case 0x01:
        mask = 0xfffffc00;
        typeDesc = "page";
        domain = arm_get_bits (desc1, 5, 4);
        break;
      case 0x02:
        mask = 0xfff00000;
        typeDesc = "section";
        domain = arm_get_bits (desc1, 5, 4);
        permission = arm_get_bits (desc1, 10, 2);
        break;
      case 0x03:
        mask = 0xfffff000;
        typeDesc = "fine";
        domain = arm_get_bits (desc1, 5, 4);
        break;
    }
    
    uint32_t addr = i*(1024*1024);
    
    printf("0x%08x     0x%08x   %7s  %i     %i        %i\n", addr, desc1 & mask, typeDesc, permission, domain, 0);
    if (type == 0x01) {
      for (int i=0; i<256; i++) {
        uint32_t pageTableEntry = (desc1 & mask) + (i * 4);
        uint32_t desc2 = newton_get_mem32(c->newton, pageTableEntry);
        uint32_t addr2 = addr + (i * 4096);
        
        uint32_t mask2 = 0;
        uint32_t permission2 = 0;
        uint32_t ap = 0;
        char *type2Desc = NULL;
        switch (desc2 & 0x03) {
          case 0x00:
            continue;
          case 0x01:
            type2Desc = "large";
            break;
          case 0x02:
            type2Desc = "small";
            mask2 = 0xfffff000;
            ap = 4 + 2 * arm_get_bits (addr2, 10, 2);
            permission2 = arm_get_bits (desc2, ap, 2);
            break;
          case 0x03:
            type2Desc = "tiny";
            break;
        }
        printf("| 0x%08x   0x%08x   %7s  %i\t(entry: 0x%08x)\n", addr2, desc2 & mask2, type2Desc, permission2, pageTableEntry);
      }
    }
  }
  
  c->newton->memTrace = trace;
}

void monitor_parse_input(monitor_t *c, const char *input) {
  int argValue = 0;
  int arg2Value = 0;
  char strValue[255];
  if (strcmp(input, "go") == 0 || strcmp(input, "run") == 0) {
    c->instructionsToExecute = INT32_MAX;
  }
  else if (sscanf(input, "break 0x%x", &argValue) == 1 || sscanf(input, "break %x", &argValue) == 1) {
    newton_breakpoint_add(c->newton, argValue, BP_PC);
    printf("Breakpoint added: 0x%08x\n", argValue);
  }
  else if (sscanf(input, "break %s", &strValue) == 1) {
    uint32_t addr = newton_address_for_symbol(c->newton, strValue);
    if (addr == 0) {
      printf("Couldn't find symbol: %s\n", strValue);
    }
    else {
      newton_breakpoint_add(c->newton, addr, BP_PC);
      printf("Breakpoint added: 0x%08x => %s\n", addr, strValue);
    }
  }
  else if (sscanf(input, "delete 0x%x", &argValue) == 1 || sscanf(input, "delete %x", &argValue) == 1) {
    newton_breakpoint_del(c->newton, argValue, BP_PC);
  }
  else if (sscanf(input, "memwatch 0x%x", &argValue) == 1 || sscanf(input, "memwatch %x", &argValue) == 1) {
    newton_breakpoint_add(c->newton, argValue, BP_READ);
    newton_breakpoint_add(c->newton, argValue, BP_WRITE);
    printf("Watching memory: 0x%08x\n", argValue);
  }
  else if (sscanf(input, "runt-interrupt %i %i", &argValue, &arg2Value) == 2) {
    if (arg2Value == 1) {
      runt_interrupt_raise(c->newton->runt, argValue);
      printf("Raising interrupt: %i\n", argValue);
    }
    else {
      runt_interrupt_lower(c->newton->runt, argValue);
      printf("Lowering interrupt: %i\n", argValue);
    }
  }
  
  else if (sscanf(input, "step %i", &argValue) == 1) {
    c->instructionsToExecute = argValue;
  }
  else if (strcmp(input, "dump") == 0) {
#warning fixme
#if 0
    FILE *fp = fopen("ram.dump", "w");
    for (uint32_t i=0; i<c->newton->ramSize/4; i++) {
      uint32_t word = htonl(*(c->newton->ram + i));
      fwrite(&word, sizeof(word), 1, fp);
    }
    fclose(fp);
#endif
  }
  else if (strcmp(input, "step") == 0 || strcmp(input, "s") == 0) {
    c->instructionsToExecute = 1;
  }
  else if (strcmp(input, "pcspy") == 0) {
    bool pcspy = !newton_get_pc_spy(c->newton);
    newton_set_pc_spy(c->newton, pcspy);
    printf("PC spying now %s\n", pcspy ? "on" : "off");
  }
  else if (strcmp(input, "spspy") == 0) {
    bool spspy = !newton_get_sp_spy(c->newton);
    newton_set_sp_spy(c->newton, spspy);
    printf("SP spying now %s\n", spspy ? "on" : "off");
  }
  else if (strcmp(input, "mem-trace") == 0) {
    bool trace = !c->newton->memTrace;
    c->newton->memTrace = trace;
    printf("Mem tracing now %s\n", trace ? "on" : "off");
  }
  else if (strcmp(input, "mmu") == 0) {
    monitor_dump_mmu(c);
  }
  else if (strcmp(input, "trace") == 0) {
    bool trace = !newton_get_instruction_trace(c->newton);
    newton_set_instruction_trace(c->newton, trace);
    printf("Tracing now %s\n", trace ? "on" : "off");
  }
  else if (sscanf(input, "write 0x%x 0x%x", &argValue, &arg2Value) == 2) {
    newton_set_mem32(c->newton, argValue, arg2Value);
  }
  else if (sscanf(input, "dasm 0x%x %i", &argValue, &arg2Value) == 2) {
    for (uint32_t i=0; i<arg2Value; i++) {
      uint32_t pc = argValue + (i * 4);
      arm_dasm_t         op;
      char               str[256];
      arm_dasm_mem (c->newton->arm, &op, pc, ARM_XLAT_CPU);
      arm_dasm_str (str, &op);
      
      fprintf(c->newton->logFile, "%08lX  %s\n", (unsigned long) pc, str);
    }
  }
  else if (sscanf(input, "read 0x%x %i", &argValue, &arg2Value) == 2 || sscanf(input, "read 0x%x", &argValue) == 1) {
    if (arg2Value == 0) {
      arg2Value = 4;
    }
    
    newton_mem_hexdump(c->newton, argValue, arg2Value);
  }
  else if (sscanf(input, "set r%i 0x%x", &argValue, &arg2Value) == 2) {
    printf("Setting r%i to 0x%08x\n", argValue, arg2Value);
    c->newton->arm->reg[argValue] = arg2Value;
  }
  else if (sscanf(input, "switch %i %i", &argValue, &arg2Value) == 2) {
    printf("Setting switch %i to %i\n", argValue, arg2Value);
    runt_switch_set_state(newton_get_runt(c->newton), argValue, arg2Value);
    c->newton->arm->reg[argValue] = arg2Value;
  }
  else {
    printf("Unknown command: %s\n", input);
  }
  
  //  return NULL;
}

void monitor_run(monitor_t *c) {
  if (gMonitor == NULL) {
    gMonitor = c;
  }
  
  bool dumpState = true;
  while (true) {
    if (dumpState && !c->newton->instructionTrace) {
      newton_print_state(c->newton);
    }
    
    char *line = linenoise("newton> ");
    if (line == NULL) {
      break;
    }
    
    if (line[0] != 0x00) {
      linenoiseHistoryAdd(line); /* Add to the history. */
      linenoiseHistorySave("history.txt"); /* Save the history on disk. */
      
      if (c->lastInput != NULL) {
        free(c->lastInput);
      }
      c->lastInput = strdup(line);
    }
    free(line);
    
    monitor_parse_input(c, c->lastInput);
    
    if (c->instructionsToExecute > 0) {
      newton_emulate(c->newton, c->instructionsToExecute);
      dumpState = true;
    }
    else {
      dumpState = false;
    }
    
    c->instructionsToExecute = 0;
  }
  
  if (gMonitor == c) {
    gMonitor = NULL;
  }
}
