//
//  fpa.c
//  Leibniz
//
//  Created by Steve White on 2/5/17.
//  Copyright (c) 2017 Steve White. All rights reserved.
//

#include "fpa.h"
#include "fpa11.h"
#include "internal.h"

#include <stdlib.h>

#if 0
#define FPA_Debug(...) { printf(__VA_ARGS__); }
#else
#define FPA_Debug(...) {}
#endif


extern unsigned int EmulateAll(unsigned int opcode);

static arm_copr_t fpa_bridge;

int fpa_exec(arm_t *arm, arm_copr_t *copro) 
{
	int r;
	r = EmulateAll(arm->ir);
	return !r;
}

int fpa_reset(arm_t *arm, arm_copr_t *copro) 
{
	resetFPA11();
	return 0;
}

void fpa_init(arm_t *arm) 
{
	fpa_bridge.copr_idx = 1;
	fpa_bridge.exec = fpa_exec;
	fpa_bridge.reset = fpa_reset;
	fpa_bridge.ext = arm;
	arm_set_copr(arm, 1, &fpa_bridge);
	
	fpa11 = calloc(1, sizeof(FPA11));
}

void fpa_delete(void)
{
	fpa_bridge.ext = 0;
	free(fpa11);
}

// Previously implemented in fpmodule.inl
unsigned int readRegister(const unsigned int nReg)
{
	FPA_Debug("[FPA] %s %i\n", __PRETTY_FUNCTION__, nReg);
	arm_t *arm = (arm_t *)fpa_bridge.ext;
	return arm_get_gpr(arm, nReg);
}

void writeRegister(const unsigned int nReg, const unsigned int val)
{
	FPA_Debug("[FPA] %s %i %x\n", __PRETTY_FUNCTION__, nReg, val);
	arm_t *arm = (arm_t *)fpa_bridge.ext;
	arm_set_gpr(arm, nReg, val);
}

unsigned int readCPSR(void) 
{
	FPA_Debug("[FPA] %s\n", __PRETTY_FUNCTION__);
	arm_t *arm = (arm_t *)fpa_bridge.ext;
	return arm_get_cpsr(arm);
}

void writeCPSR(const unsigned int val)
{
	FPA_Debug("[FPA] %s %x\n", __PRETTY_FUNCTION__, val);
	arm_t *arm = (arm_t *)fpa_bridge.ext;
	arm_set_cpsr(arm, val);
}

unsigned int readConditionCodes(void)
{
	FPA_Debug("[FPA] %s\n", __PRETTY_FUNCTION__);
    return(readCPSR() & ARM_PSR_CC);
}

void writeConditionCodes(const unsigned int val)
{
	FPA_Debug("[FPA] %s %i\n", __PRETTY_FUNCTION__, val);
	arm_t *arm = (arm_t *)fpa_bridge.ext;
	unsigned rval = readCPSR() & ~ARM_PSR_CC;
	writeCPSR(rval | (val & ARM_PSR_CC));
}

unsigned int readMode(void)
{
	FPA_Debug("[FPA] %s\n", __PRETTY_FUNCTION__);
	arm_t *arm = (arm_t *)fpa_bridge.ext;
	return arm_get_cpsr_m (arm);
}

// Previously in the Linux kernel
void get_user(unsigned int *val, const unsigned int *addr)
{
	FPA_Debug("[FPA] %s %08x %08x\n", __PRETTY_FUNCTION__, addr, val);
	arm_t *arm = (arm_t *)fpa_bridge.ext;
	arm_dload32_t(arm, addr, val);
}

void put_user(unsigned int val, unsigned int *addr)
{
	FPA_Debug("[FPA] %s %08x %08x\n", __PRETTY_FUNCTION__, addr, val);
	arm_t *arm = (arm_t *)fpa_bridge.ext;
	arm_dstore32_t(arm, addr, val);
}

/*
 * Enable IRQs
 */
void sti(void) {}

/*
 * Save the current interrupt enable state & disable IRQs
 */
void save_flags(unsigned int flags) {}

/*
 * restore saved IRQ & FIQ state
 */
void restore_flags(unsigned int flags) {}
