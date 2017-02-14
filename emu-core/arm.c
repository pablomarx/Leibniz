/*****************************************************************************
 * pce                                                                       *
 *****************************************************************************/

/*****************************************************************************
 * File name:   src/cpu/arm/arm.c                                            *
 * Created:     2004-11-03 by Hampa Hug <hampa@hampa.ch>                     *
 * Copyright:   (C) 2004-2009 Hampa Hug <hampa@hampa.ch>                     *
 * Copyright:   (C) 2004-2006 Lukas Ruf <ruf@lpr.ch>                         *
 *****************************************************************************/

/*****************************************************************************
 * This program is free software. You can redistribute it and / or modify it *
 * under the terms of the GNU General Public License version 2 as  published *
 * by the Free Software Foundation.                                          *
 *                                                                           *
 * This program is distributed in the hope  that  it  will  be  useful,  but *
 * WITHOUT  ANY   WARRANTY,   without   even   the   implied   warranty   of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  General *
 * Public License for more details.                                          *
 *****************************************************************************/

/*****************************************************************************
 * This software was developed at the Computer Engineering and Networks      *
 * Laboratory (TIK), Swiss Federal Institute of Technology (ETH) Zurich.     *
 *****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arm.h"
#include "internal.h"

void arm_init (arm_t *c)
{
	unsigned i;

	c->flags = 0;

	c->mem_ext = NULL;

	c->get_uint8 = NULL;
	c->get_uint16 = NULL;
	c->get_uint32 = NULL;

	c->set_uint8 = NULL;
	c->set_uint16 = NULL;
	c->set_uint32 = NULL;

	c->ram = NULL;
	c->ram_cnt = 0;

	c->log_ext = NULL;
	c->log_opcode = NULL;
	c->log_undef = NULL;
	c->log_exception = NULL;

	arm_set_opcodes (c);

	c->cpsr = 0;

	c->exception_base = 0;

	c->bigendian = 0;
	c->privileged = 1;

	c->irq = 0;
	c->fiq = 0;
	c->irq_or_fiq = 0;

	c->delay = 0;
	c->oprcnt = 0;
	c->clkcnt = 0;

	for (i = 0; i < 16; i++) {
		c->copr[i] = NULL;
	}

	cp14_init (&c->copr14);
	c->copr[14] = &c->copr14.copr;

	cp15_init (&c->copr15);
	c->copr[15] = &c->copr15.copr;
}

arm_t *arm_new (void)
{
	arm_t *c;

	c = calloc (1, sizeof (arm_t));
	if (c == NULL) {
		return (NULL);
	}

	arm_init (c);

	return (c);
}

void arm_free (arm_t *c)
{
	cp14_free (&c->copr14);
	cp15_free (&c->copr15);
}

void arm_del (arm_t *c)
{
	if (c != NULL) {
		arm_free (c);
		free (c);
	}
}


void arm_set_mem_fct (arm_t *c, void *ext,
	void *get8, void *get16, void *get32,
	void *set8, void *set16, void *set32)
{
	c->mem_ext = ext;

	c->get_uint8 = get8;
	c->get_uint16 = get16;
	c->get_uint32 = get32;

	c->set_uint8 = set8;
	c->set_uint16 = set16;
	c->set_uint32 = set32;
}

void arm_set_ram (arm_t *c, unsigned char *ram, unsigned long cnt)
{
	c->ram = ram;
	c->ram_cnt = cnt;
}

unsigned arm_get_flags (const arm_t *c, unsigned flags)
{
	return (c->flags & flags);
}

void arm_set_flags (arm_t *c, unsigned flags, int val)
{
	if (val) {
		c->flags |= flags;
	}
	else {
		c->flags &= ~flags;
	}
}

uint32_t arm_get_id (arm_t *c)
{
	return (c->copr15.reg[0]);
}

void arm_set_id (arm_t *c, uint32_t id)
{
	c->copr15.reg[0] = id;
}

static
int arm_get_reg_idx (const char *reg, unsigned *idx, unsigned max)
{
	unsigned n;

	n = 0;

	if ((reg[0] < '0') || (reg[0] > '9')) {
		return (1);
	}

	while ((reg[0] >= '0') && (reg[0] <= '9')) {
		n = 10 * n + (unsigned) (reg[0] - '0');
		reg += 1;
	}

	if (reg[0] != 0) {
		return (1);
	}

	if (n > max) {
		return (1);
	}

	*idx = n;

	return (0);
}

int arm_get_reg (arm_t *c, const char *reg, uint32_t *val)
{
	unsigned idx;

	if (reg[0] == '%') {
		reg += 1;
	}

	if (strcmp (reg, "pc") == 0) {
		*val = arm_get_pc (c);
		return (0);
	}
	else if (strcmp (reg, "lr") == 0) {
		*val = arm_get_lr (c);
		return (0);
	}
	else if (strcmp (reg, "cpsr") == 0) {
		*val = arm_get_cpsr (c);
		return (0);
	}
	else if (strcmp (reg, "spsr") == 0) {
		*val = arm_get_spsr (c);
		return (0);
	}

	if (reg[0] == 'r') {
		if (arm_get_reg_idx (reg + 1, &idx, 15)) {
			return (1);
		}

		*val = arm_get_gpr (c, idx);

		return (0);
	}

	return (1);
}

int arm_set_reg (arm_t *c, const char *reg, uint32_t val)
{
	unsigned idx;

	if (reg[0] == '%') {
		reg += 1;
	}

	if (strcmp (reg, "pc") == 0) {
		arm_set_pc (c, val);
		return (0);
	}
	else if (strcmp (reg, "lr") == 0) {
		arm_set_lr (c, val);
		return (0);
	}
	else if (strcmp (reg, "cpsr") == 0) {
		arm_set_cpsr (c, val);
		return (0);
	}
	else if (strcmp (reg, "spsr") == 0) {
		arm_set_spsr (c, val);
		return (0);
	}

	if (reg[0] == 'r') {
		if (arm_get_reg_idx (reg + 1, &idx, 15)) {
			return (1);
		}

		arm_set_gpr (c, idx, val);

		return (0);
	}

	return (1);
}

unsigned long long arm_get_opcnt (arm_t *c)
{
	return (c->oprcnt);
}

unsigned long long arm_get_clkcnt (arm_t *c)
{
	return (c->clkcnt);
}

unsigned long arm_get_delay (arm_t *c)
{
	return (c->delay);
}


void arm_copr_init (arm_copr_t *p)
{
	p->copr_idx = 0;
	p->exec = NULL;
	p->reset = NULL;
	p->ext = NULL;
}

void arm_copr_free (arm_copr_t *p)
{
}

int arm_copr_check (arm_t *c, unsigned i)
{
	if (i >= 16) {
		return (1);
	}

	if (c->copr[i] == NULL) {
		return (1);
	}

	if (c->flags & ARM_FLAG_CPAR) {
		uint32_t cpar;

		cpar = c->copr15.reg[15];

		if ((i <= 13) && (cpar & (1UL << i))) {
			return (1);
		}
	}

	return (0);
}

void arm_set_copr (arm_t *c, unsigned i, arm_copr_t *p)
{
	if (i < 16) {
		c->copr[i] = p;
	}
}


void arm_set_reg_map (arm_t *c, unsigned new_mode)
{
  uint32_t *bank = NULL;
  unsigned old_mode = c->old_mode;
  if (old_mode == new_mode) {
    return;
  }
  
  c->old_mode = new_mode;
  switch (old_mode) {
    case ARM_MODE_USR:
    case ARM_MODE_SYS:
      // usr and sys mode share the same "bank" of registers
      c->usr_regs[0] = c->reg[13]; // sp
      c->usr_regs[1] = c->reg[14]; // lr
      break;
      
    case ARM_MODE_FIQ:
      // fiq mode has a seperate bank for r8-r14, and spsr
      // fiq mode has a seperate bank for r8-r14, and spsr
      memcpy(c->fiq_regs, &c->reg[8], sizeof(uint32_t) * 7); // r8-r14
      c->fiq_regs[7] = c->spsr;
      
      // we must be moving to a "user" mode so switch to the user r8-r12
      memcpy(&c->reg[8], c->usr_regs_low, sizeof(uint32_t) * 5); // r8-r12
      break;
      
      // the other 4 modes are similar, so select the bank and share the code
    case ARM_MODE_IRQ:
      bank = c->irq_regs;
      goto save_3reg;
    case ARM_MODE_SVC:
      bank = c->svc_regs;
      goto save_3reg;
    case ARM_MODE_ABT:
      bank = c->abt_regs;
      goto save_3reg;
    case ARM_MODE_UND:
      bank = c->und_regs;
    save_3reg:
      bank[0] = c->reg[13]; // sp
      bank[1] = c->reg[14]; // lr
      bank[2] = c->spsr;  // spsr
      break;
  }
  
  switch (new_mode) {
    case ARM_MODE_USR:
    case ARM_MODE_SYS:
      // usr and sys mode share the same "bank" of registers
      c->reg[13] = c->usr_regs[0];
      c->reg[14] = c->usr_regs[1];
      break;
      
    case ARM_MODE_FIQ:
      // we must be coming from one of the "user" modes, so save r8-r12
      memcpy(c->usr_regs_low, &c->reg[8], sizeof(uint32_t) * 5); // r8-r12
      
      // fiq mode has a seperate bank for r8-r14, and spsr
      memcpy(&c->reg[8], c->fiq_regs, sizeof(uint32_t) * 7); // r8-r14
      c->spsr = c->fiq_regs[7];
      break;
      
    case ARM_MODE_IRQ:
      bank = c->irq_regs;
      goto restore_3reg;
    case ARM_MODE_SVC:
      bank = c->svc_regs;
      goto restore_3reg;
    case ARM_MODE_ABT:
      bank = c->abt_regs;
      goto restore_3reg;
    case ARM_MODE_UND:
      bank = c->und_regs;
    restore_3reg:
      c->reg[13] = bank[0]; // sp
      c->reg[14] = bank[1]; // lr
      c->spsr = bank[2];  // spsr
      break;
  }
}

void arm_reset (arm_t *c)
{
	unsigned i;

	c->cpsr = ARM_MODE_SVC;

	arm_set_reg_map (c, ARM_MODE_SVC);

	for (i = 0; i < 16; i++) {
		c->reg[i] = 0;
	}

  for (i = 0; i < 5; i ++) {
    c->usr_regs_low[i] = 0x00;
  }
  for (i = 0; i < 2; i ++) {
    c->usr_regs[i] = 0x00;
  }
  for (i = 0; i < 3; i ++) {
    c->irq_regs[i] = 0x00;
  }
  for (i = 0; i < 3; i ++) {
    c->svc_regs[i] = 0x00;
  }
  for (i = 0; i < 3; i ++) {
    c->abt_regs[i] = 0x00;
  }
  for (i = 0; i < 3; i ++) {
    c->und_regs[i] = 0x00;
  }
  for (i = 0; i < 8; i ++) {
    c->fiq_regs[i] = 0x00;
  }

	c->lastpc[0] = 0;
	c->lastpc[1] = 0;

	c->ir = 0;

	c->exception_base = 0;

	/* Actually, endianness should be reset to little endian and then
	 * initialized by the CPU itself. We don't do that because we
	 * have a byte based memory system and therefore (contrary to
	 * the specification) this setting *does* affect word loads and
	 * stores. */
	c->bigendian = ((c->flags & ARM_FLAG_BIGENDIAN) != 0);

	c->privileged = 1;

	c->irq = 0;
	c->fiq = 0;
	c->irq_or_fiq = 0;

	c->delay = 1;

	c->oprcnt = 0;
	c->clkcnt = 0;

	arm_tbuf_flush (c);

	for (i = 0; i < 16; i++) {
		if (c->copr[i] != NULL) {
			if (c->copr[i]->reset != NULL) {
				c->copr[i]->reset (c, c->copr[i]);
			}
		}
	}
}

void arm_exception (arm_t *c, uint32_t addr, uint32_t ret, unsigned mode)
{
	uint32_t cpsr, spsr;

	if (c->log_exception != NULL) {
		c->log_exception (c->log_ext, addr);
	}

	c->delay += 1;

	cpsr = arm_get_cpsr (c);
	spsr = cpsr;

	cpsr &= ~(ARM_PSR_T | ARM_PSR_M);
	cpsr |= ARM_PSR_I | (mode & 0x1f);

	arm_write_cpsr (c, cpsr, 0);
	arm_set_spsr (c, spsr);
	arm_set_lr (c, ret);
	arm_set_pc (c, (c->exception_base + addr) & 0xffffffffUL);
}

void arm_exception_reset (arm_t *c)
{
	arm_exception (c, 0x00000000UL, 0, ARM_MODE_SVC);
	arm_set_cpsr_f (c, 1);
}

void arm_exception_undefined (arm_t *c)
{
	arm_exception (c, 0x00000004UL, arm_get_pc (c) + 4, ARM_MODE_UND);
}

void arm_exception_swi (arm_t *c)
{
	arm_exception (c, 0x00000008UL, arm_get_pc (c) + 4, ARM_MODE_SVC);
}

void arm_exception_prefetch_abort (arm_t *c)
{
	arm_exception (c, 0x0000000cUL, arm_get_pc (c) + 4, ARM_MODE_ABT);
}

void arm_exception_data_abort (arm_t *c)
{
	arm_exception (c, 0x00000010UL, arm_get_pc (c) + 8, ARM_MODE_ABT);
}

void arm_exception_irq (arm_t *c)
{
	arm_exception (c, 0x00000018UL, arm_get_pc (c) + 4, ARM_MODE_IRQ);
}

void arm_exception_fiq (arm_t *c)
{
	arm_exception (c, 0x0000001cUL, arm_get_pc (c) + 4, ARM_MODE_FIQ);
	arm_set_cpsr_f (c, 1);
}

void arm_set_irq (arm_t *c, unsigned char val)
{
	c->irq = (val != 0);
	c->irq_or_fiq = c->irq || c->fiq;
}

void arm_set_fiq (arm_t *c, unsigned char val)
{
	c->fiq = (val != 0);
	c->irq_or_fiq = c->irq || c->fiq;
}

void arm_execute (arm_t *c)
{
	c->oprcnt += 1;

	c->lastpc[1] = c->lastpc[0];
	c->lastpc[0] = arm_get_pc (c);

	if (arm_ifetch (c, c->lastpc[0], &c->ir)) {
		return;
	}

#if 0
	if (c->log_opcode != NULL) {
		if (c->log_opcode (c->log_ext, c->ir)) {
			/* nop */
			c->ir = 0xe1a00000UL;
		}
	}
#endif

  if (arm_check_cond_al (c->ir) || arm_check_cond (c, arm_ir_cond (c->ir))) {
    if ((c->ir & 0xfffff0ff) == 0xe6000010) {
      uint32_t pc = arm_get_pc(c);
      if (c->log_undef) {
        c->log_undef(c->log_ext, c->ir);
      }
      if (pc == arm_get_pc(c)) {
#if 1
        arm_exception_undefined(c);
#else
        arm_set_clk (c, 4, 1);
#endif
      }
    }
    else {
      c->opcodes[(c->ir >> 20) & 0xff] (c);
    }
  }
  else {
    arm_set_clk (c, 4, 1);
  }

	if (c->irq_or_fiq) {
		if (c->fiq && (arm_get_cpsr_f (c) == 0)) {
			arm_exception_fiq (c);
		}
		else if (c->irq && (arm_get_cpsr_i (c) == 0)) {
			arm_exception_irq (c);
		}
	}
}

void arm_clock (arm_t *c, unsigned long n)
{
	while (n >= c->delay) {
		n -= c->delay;

		c->clkcnt += c->delay;
		c->delay = 0;

		arm_execute (c);

#if 0
		if (c->delay == 0) {
			fprintf (stderr, "%08lX ARM: delay == 0\n",
				(unsigned long) arm_get_pc (c)
			);
			fflush (stderr);
			c->delay = 1;
		}
#endif
	}

	c->clkcnt += n;
	c->delay -= n;
}
