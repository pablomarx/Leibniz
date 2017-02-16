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


extern FPA11* qemufpa;

extern unsigned int EmulateAll(unsigned int opcode, FPA11* qfpa);

static arm_copr_t fpa_bridge;

int fpa_exec(arm_t *arm, arm_copr_t *copro) 
{
	int r;

    FPA_Debug("[FPA] %s executing 0x%08x at PC:0x%08x\n", __PRETTY_FUNCTION__, arm->ir, arm_get_pc(arm));

    r = EmulateAll(arm->ir, qemufpa);
	if (r) {
		arm_set_clk (arm, 4, 1);
	}
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
	
	qemufpa = calloc(1, sizeof(FPA11));
}

void fpa_delete(void)
{
	fpa_bridge.ext = 0;
	free(qemufpa);
}

// Previously implemented in fpmodule.inl
uint32_t readRegister(const unsigned int nReg)
{
	arm_t *arm = (arm_t *)fpa_bridge.ext;
	uint32_t result = arm_get_gpr(arm, nReg);
    FPA_Debug("[FPA] %s %i => 0x%08x\n", __PRETTY_FUNCTION__, nReg, result);
    return result;
}

void writeRegister(const unsigned int nReg, const uint32_t val)
{
	FPA_Debug("[FPA] %s %i 0x%08x\n", __PRETTY_FUNCTION__, nReg, val);
	arm_t *arm = (arm_t *)fpa_bridge.ext;
	arm_set_gpr(arm, nReg, val);
}

void writeConditionCodes(const unsigned int val)
{
	FPA_Debug("[FPA] %s 0x%08x\n", __PRETTY_FUNCTION__, val);
    arm_t *arm = (arm_t *)fpa_bridge.ext;
    uint32_t cpsr = arm_get_cpsr(arm);
    cpsr &= ~ARM_PSR_CC;
    cpsr |= val;
    arm_set_cpsr(arm, cpsr);
}

void get_user_u32(uint32_t *val, uint32_t addr)
{
	arm_t *arm = (arm_t *)fpa_bridge.ext;
	arm_dload32_t(arm, addr, val);
    FPA_Debug("[FPA] %s %08x %08x\n", __PRETTY_FUNCTION__, addr, *val);
}

void put_user_u32(uint32_t val, uint32_t addr)
{
	FPA_Debug("[FPA] %s %08x %08x\n", __PRETTY_FUNCTION__, addr, val);
	arm_t *arm = (arm_t *)fpa_bridge.ext;
	arm_dstore32_t(arm, addr, val);
}

