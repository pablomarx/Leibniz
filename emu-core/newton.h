//
//  newton.h
//  Leibniz
//
//  Created by Steve White on 9/13/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#ifndef Leibniz_newton_h
#define Leibniz_newton_h

#include "arm.h"
#include "docker.h"
#include "memory.h"
#include "pcmcia.h"
#include "runt.h"

#include <stdio.h>

/*	From NewtonGestalt.h: TGestaltSystemInfo */

#define kGestalt_Manufacturer_Apple			0x01000000
#define kGestalt_Manufacturer_Sharp 		0x10000100
#define kGestalt_Manufacturer_Motorola	0x01000200

#define kGestalt_MachineType_MessagePad 0x10001000
#define kGestalt_MachineType_Bic        0x10002000
#define kGestalt_MachineType_Senior     0x10003000
#define kGestalt_MachineType_Emate      0x10004000
#define kGestalt_MachineType_Lindy			0x00726377

typedef struct symbol_s symbol_t;
struct symbol_s {
  char *name;
  uint32_t address;
  symbol_t *next;
};

typedef enum {
  BP_NONE   = 0,
  BP_PC     = 1,
  BP_READ   = 2,
  BP_WRITE  = 3,
} bp_type;

typedef struct bp_entry_s bp_entry_t;
struct bp_entry_s {
  bp_type    type;
  uint32_t   addr;
  bp_entry_t *next;
};

enum {
  NewtonLogNone        = 0x00000000,
  
  NewtonLogFlash       = (1 << 1),
  NewtonLogSWI         = (1 << 2),
  NewtonLogVectorTable = (1 << 3),
  NewtonLogTapFileCntl = (1 << 4),
  NewtonLogUndefined   = (1 << 5),
  NewtonLogCard        = (1 << 6),
  NewtonLogPCMCIA      = (1 << 7),
  NewtonLogAll         = 0xffffffff,
};

//
// TapFileCntl
//
typedef int32_t (*newton_do_sys_open_f) (void *ext, const char *name, int mode);
typedef int32_t (*newton_do_sys_close_f) (void *ext, uint32_t fildes);
typedef int32_t (*newton_do_sys_istty_f) (void *ext, uint32_t fildes);
typedef int32_t (*newton_do_sys_read_f) (void *ext, uint32_t fildes, void *buf, uint32_t nbyte);
typedef int32_t (*newton_do_sys_write_f) (void *ext, uint32_t fildes, const void *buf, uint32_t nbyte);
typedef int32_t (*newton_do_sys_set_input_notify_f) (void *ext, uint32_t fildes, uint32_t addr);


//
// Membank
//
typedef uint32_t (*membank_set_uint32_f) (void *ext, uint32_t addr, uint32_t val, uint32_t pc);
typedef uint32_t (*membank_get_uint32_f) (void *ext, uint32_t addr, uint32_t pc);
typedef uint8_t (*membank_set_uint8_f) (void *ext, uint32_t addr, uint8_t val, uint32_t pc);
typedef uint8_t (*membank_get_uint8_f) (void *ext, uint32_t addr, uint32_t pc);
typedef void (*membank_del_f) (void *ext);

typedef struct membank_s membank_t;
struct membank_s {
  uint32_t base;
  uint32_t length;

  void *context;
  membank_get_uint32_f get_uint32;
  membank_set_uint32_f set_uint32;
  membank_get_uint8_f get_uint8;
  membank_set_uint8_f set_uint8;
  membank_del_f del;
  
  membank_t *next;
};

//
//
//
typedef struct newton_s newton_t;
typedef void (*newton_system_panic_f) (newton_t *c, const char *msg);
typedef void (*newton_debugstr_f) (newton_t *c, const char *msg);
typedef void (*newton_undefined_opcode_f) (newton_t *c, uint32_t opcode);

typedef enum {
  NewtonBootModeNormal = 0,
  NewtonBootModeDiagnostics,
  NewtonBootModeAutoPWB,
} NewtonBootMode;

typedef enum {
  NewtonRebootStyleCold,
  NewtonRebootStyleWarm,
} NewtonRebootStyle;

typedef struct {
  uint8_t *buffer;
  uint32_t offset;
  uint32_t length;
} newton_serial_queue_t;

struct newton_s {
  arm_t *arm;
  bool stop;
  
  runt_t *runt;
  pcmcia_t *pcmcia;
  memory_t *ram;
  
  membank_t *membanks;
  
  uint32_t machineType;
  uint32_t romManufacturer;
  uint32_t romVersion;
  uint32_t debuggerBits;
  uint32_t newtConfig;
  uint32_t newtTests;
  NewtonBootMode bootMode;
  
  docker_t *docker;
  newton_serial_queue_t serialQueues[2];
  
  // TapFileCntl related
  bool supportsRegularFiles;
  void *tapfilecntl_ext;
  newton_do_sys_open_f do_sys_open;
  newton_do_sys_close_f do_sys_close;
  newton_do_sys_istty_f do_sys_istty;
  newton_do_sys_read_f do_sys_read;
  newton_do_sys_write_f do_sys_write;
  newton_do_sys_set_input_notify_f do_sys_set_input_notify;
  
  //
#if !DISABLE_DEBUGGER
  bp_entry_t *breakpoints;
  symbol_t *symbols;
  
  bool instructionTrace;
  bool memTrace;
	
  bool pcSpy;
  bool spSpy;
  uint32_t lastPc;
  uint32_t lastSp;
#endif

	bool breakOnUnknownMemory;

  FILE *logFile;
  uint32_t logFlags;
  
  newton_undefined_opcode_f  undefined_opcode;
  newton_system_panic_f system_panic;
  newton_debugstr_f debug_str;
};

void arm_dasm_str (char *dst, arm_dasm_t *op);

void newton_init (newton_t *c);
newton_t *newton_new (void);
int newton_load_rom(newton_t *c, const char *path);
void newton_free (newton_t *c);
void newton_del (newton_t *c);

void newton_set_log_flags (newton_t *c, unsigned flags, int val);

uint32_t newton_get_mem32 (newton_t *c, uint32_t addr);
uint8_t newton_get_mem8 (newton_t *c, uint32_t addr);
uint16_t newton_get_mem16 (newton_t *c, uint32_t addr);
uint8_t newton_set_mem8 (newton_t *c, uint32_t addr, uint8_t val);
uint16_t newton_set_mem16 (newton_t *c, uint32_t addr, uint16_t val);
uint32_t newton_set_mem32 (newton_t *c, uint32_t addr, uint32_t val);

runt_t *newton_get_runt (newton_t *c);
pcmcia_t *newton_get_pcmcia (newton_t *c);
docker_t *newton_get_docker (newton_t *c);

void newton_emulate(newton_t *c, int32_t count);
void newton_stop(newton_t *c);
void newton_set_bootmode(newton_t *c, NewtonBootMode bootMode);
void newton_reboot(newton_t *c, NewtonRebootStyle style);

void newton_set_debugger_bits(newton_t *c, uint32_t debugger_bits);
uint32_t newton_get_debugger_bits(newton_t *c);

void newton_set_newt_config(newton_t *c, uint32_t newt_config);
uint32_t newton_get_newt_config(newton_t *c);

void newton_set_newt_tests(newton_t *c, uint32_t newt_tests);
uint32_t newton_get_newt_tests(newton_t *c);

uint32_t newton_address_for_symbol(newton_t *c, const char *symbol);
void newton_load_mapfile(newton_t *c, const char *mapfile);
void newton_set_logfile(newton_t *c, FILE *file);
void newton_print_state(newton_t *c);

void newton_breakpoint_add(newton_t *c, uint32_t address, bp_type type);
void newton_breakpoint_del(newton_t *c, uint32_t address, bp_type type);

void newton_set_break_on_unknown_memory(newton_t *c, bool breakOnUnknownMemory);
bool newton_get_break_on_unknown_memory(newton_t *c);

void newton_set_instruction_trace(newton_t *c, bool instructionTrace);
bool newton_get_instruction_trace(newton_t *c);

void newton_mem_hexdump(newton_t *c, uint32_t addr, uint32_t length);

void newton_set_pc_spy(newton_t *c, bool pcSpy);
bool newton_get_pc_spy(newton_t *c);

void newton_set_sp_spy(newton_t *c, bool spSpy);
bool newton_get_sp_spy(newton_t *c);

void newton_file_input_notify(newton_t *c, uint32_t addr, uint32_t value);

//
void newton_set_system_panic(newton_t *c, newton_system_panic_f system_panic);
void newton_set_undefined_opcode(newton_t *c, newton_undefined_opcode_f undefined_opcode);
void newton_set_debugstr(newton_t *c, newton_debugstr_f debugstr);

void newton_set_tapfilecntl_functions (newton_t *c, void *ext,
                     newton_do_sys_open_f do_sys_open,
                     newton_do_sys_close_f do_sys_close,
                     newton_do_sys_istty_f do_sys_istty,
                     newton_do_sys_read_f do_sys_read,
                     newton_do_sys_write_f do_sys_write,
                     newton_do_sys_set_input_notify_f do_sys_set_input_notify);


#endif
