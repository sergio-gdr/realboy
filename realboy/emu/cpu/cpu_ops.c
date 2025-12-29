/*
 * Copyright (C) 2013-2016, 2025 Sergio Gómez Del Real
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdbool.h>

#include "cpu.h"
#include "monitor.h"

#define UNSET_FLAG(flag) cpu.state.registers.f &= ~(flag)
#define SET_FLAG(flag) cpu.state.registers.f |= (flag)
#define UNSET_FLAGS_ALL UNSET_FLAG(FLAG_SUB|FLAG_ZERO|FLAG_CARRY|FLAG_HCARRY)

#define FLAG_ZERO 0b10000000
#define FLAG_SUB 0b01000000
#define FLAG_HCARRY 0b00100000
#define FLAG_CARRY 0b00010000

#define BOOL_TO_ZERO_FLAG(x) (x)<<7
#define BOOL_TO_SUB_FLAG(x) (x)<<6
#define BOOL_TO_HALF_CARRY_FLAG(x) (x)<<5
#define BOOL_TO_CARRY_FLAG(x) (x)<<4

#define IS_ADD_ZERO(op1, op2) ((uint8_t)(op1 + op2) == 0)
#define IS_ADD_HALF_CARRY(op1, op2) (bool)(((op1) ^ (op2) ^ (op1+op2)) & 0x10)
#define IS_ADD_CARRY(op1, op2) (uint8_t)(op1 + op2) < op1
#define IS_ADD16_HALF_CARRY(op1, op2) (bool)(((op1) ^ (op2) ^ (op1+op2)) & 0x1000)
#define IS_ADD16_CARRY(op1, op2) (uint16_t)(op1 + op2) < op1
#define IS_ADD16_ZERO(op1, op2) ((uint16_t)(op1 + op2) == 0)

#define IS_SUB_ZERO(op1, op2) ((uint8_t)((op1) - (op2)) == 0)
#define IS_SUB_HALF_CARRY(op1, op2) (bool)(((op1) ^ (op2) ^ ((op1)-(op2))) & 0x10)
#define IS_SUB_CARRY(op1, op2) op1 < (uint8_t)((op1) - (op2))
#define IS_SUB16_HALF_CARRY(op1, op2) (bool)(((op1) ^ (op2) ^ ((op1)-(op2))) & 0x1000)
#define IS_SUB16_CARRY(op1, op2) op1 < (uint16_t)((op1) - (op2))
#define IS_SUB16_ZERO(op1, op2) ((uint16_t)((op1) - (op2)) == 0)
#define IS_CP_ZERO(op1, op2) ((uint8_t)((op1) - (op2)) == 0)

#define IS_CP_CARRY(op1, op2) op1 < (uint8_t)((op1) - (op2))
#define IS_CP_HALF_CARRY(op1, op2) (bool)(((op1) ^ (op2) ^ ((op1)-(op2))) & 0x10)

#define ADVANCE_PC(x) \
	cpu.state.registers.pc += x;

extern cpu_t cpu; // this variable is defined in cpu.c and freely accessed inside the cpu module
const uint8_t op_len[256] =
{
	1, 3, 1, 1, 1, 1, 2, 1, 3, 1, 1, 1, 1, 1, 2, 1,
	2, 3, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 2, 1,
	2, 3, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 2, 1,
	2, 3, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 3, 3, 3, 1, 2, 1, 1, 1, 3, 2, 3, 3, 2, 1,
	1, 1, 3, 3, 3, 1, 2, 1, 1, 1, 3, 1, 3, 3, 2, 1,
	2, 1, 1, 3, 3, 1, 2, 1, 2, 1, 3, 1, 3, 3, 2, 1,
	2, 1, 1, 1, 3, 1, 2, 1, 2, 1, 3, 1, 3, 3, 2, 1
};

static int op_nop() {
	return 1;
}

//
// LOAD INSTRUCTIONS
//

#define DO_LD_REG(dst, src) \
	dst = src;

#define OP_LD_REG_REG(dst, src) \
	DO_LD_REG(dst, src) \
	return 1 \

static int op_ld_b_b() {
	OP_LD_REG_REG(REG_B, REG_B);
}

static int op_ld_b_c() {
	OP_LD_REG_REG(REG_B, REG_C);
}

static int op_ld_b_d() {
	OP_LD_REG_REG(REG_B, REG_D);
}

static int op_ld_b_e() {
	OP_LD_REG_REG(REG_B, REG_E);
}

static int op_ld_b_h() {
	OP_LD_REG_REG(REG_B, REG_H);
}

static int op_ld_b_l() {
	OP_LD_REG_REG(REG_B, REG_L);
}

static int op_ld_b_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_LD_REG(REG_B, load);
	return 2;
}

static int op_ld_b_a() {
	OP_LD_REG_REG(REG_B, REG_A);
}

static int op_ld_c_b() {
	OP_LD_REG_REG(REG_C, REG_B);
}

static int op_ld_c_c() {
	OP_LD_REG_REG(REG_C, REG_C);
}

static int op_ld_c_d() {
	OP_LD_REG_REG(REG_C, REG_D);
}

static int op_ld_c_e() {
	OP_LD_REG_REG(REG_C, REG_E);
}

static int op_ld_c_h() {
	OP_LD_REG_REG(REG_C, REG_H);
}

static int op_ld_c_l() {
	OP_LD_REG_REG(REG_C, REG_L);
}

static int op_ld_c_a() {
	OP_LD_REG_REG(REG_C, REG_A);
}

static int op_ld_d_b() {
	OP_LD_REG_REG(REG_D, REG_B);
}

static int op_ld_d_c() {
	OP_LD_REG_REG(REG_D, REG_C);
}

static int op_ld_d_d() {
	OP_LD_REG_REG(REG_D, REG_D);
}

static int op_ld_d_e() {
	OP_LD_REG_REG(REG_D, REG_E);
}

static int op_ld_d_h() {
	OP_LD_REG_REG(REG_D, REG_H);
}

static int op_ld_d_l() {
	OP_LD_REG_REG(REG_D, REG_L);
}

static int op_ld_d_a() {
	OP_LD_REG_REG(REG_D, REG_A);
}

static int op_ld_e_b() {
	OP_LD_REG_REG(REG_E, REG_B);
}

static int op_ld_e_c() {
	OP_LD_REG_REG(REG_E, REG_C);
}

static int op_ld_e_d() {
	OP_LD_REG_REG(REG_E, REG_D);
}

static int op_ld_e_e() {
	OP_LD_REG_REG(REG_E, REG_E);
}

static int op_ld_e_h() {
	OP_LD_REG_REG(REG_E, REG_H);
}

static int op_ld_e_l() {
	OP_LD_REG_REG(REG_E, REG_L);
}

static int op_ld_e_a() {
	OP_LD_REG_REG(REG_E, REG_A);
}

static int op_ld_h_b() {
	OP_LD_REG_REG(REG_H, REG_B);
}

static int op_ld_h_c() {
	OP_LD_REG_REG(REG_H, REG_C);
}

static int op_ld_h_d() {
	OP_LD_REG_REG(REG_H, REG_D);
}

static int op_ld_h_e() {
	OP_LD_REG_REG(REG_H, REG_E);
}

static int op_ld_h_h() {
	OP_LD_REG_REG(REG_H, REG_H);
}

static int op_ld_h_l() {
	OP_LD_REG_REG(REG_H, REG_L);
}

static int op_ld_h_a() {
	OP_LD_REG_REG(REG_H, REG_A);
}

static int op_ld_l_b() {
	OP_LD_REG_REG(REG_L, REG_B);
}

static int op_ld_l_c() {
	OP_LD_REG_REG(REG_L, REG_C);
}

static int op_ld_l_d() {
	OP_LD_REG_REG(REG_L, REG_D);
}

static int op_ld_l_e() {
	OP_LD_REG_REG(REG_L, REG_E);
}

static int op_ld_l_h() {
	OP_LD_REG_REG(REG_L, REG_H);
}

static int op_ld_l_l() {
	OP_LD_REG_REG(REG_L, REG_L);
}

static int op_ld_l_a() {
	OP_LD_REG_REG(REG_L, REG_A);
}

static int op_ld_a_b() {
	OP_LD_REG_REG(REG_A, REG_B);
}

static int op_ld_a_c() {
	OP_LD_REG_REG(REG_A, REG_C);
}

static int op_ld_a_d() {
	OP_LD_REG_REG(REG_A, REG_D);
}

static int op_ld_a_e() {
	OP_LD_REG_REG(REG_A, REG_E);
}

static int op_ld_a_h() {
	OP_LD_REG_REG(REG_A, REG_H);
}

static int op_ld_a_l() {
	OP_LD_REG_REG(REG_A, REG_L);
}

static int op_ld_a_a() {
	OP_LD_REG_REG(REG_A, REG_A);
}

#define OP_LD_REG16_MEM(reg, addr) \
	uint16_t load = RD_WORD(addr); \
	DO_LD_REG(reg, load) \
	ADVANCE_PC(2); \
	return 3; \

static int op_ld_bc_n16() {
	OP_LD_REG16_MEM(REG_BC, REG_PC)
}

static int op_ld_de_n16() {
	OP_LD_REG16_MEM(REG_DE, REG_PC)
}

static int op_ld_hl_n16() {
	OP_LD_REG16_MEM(REG_HL, REG_PC)
}

static int op_ld_sp_n16() {
	OP_LD_REG16_MEM(REG_SP, REG_PC)
}

#define DO_LD_MEM(addr, value) \
	monitor_wr_mem(addr, value);

#define OP_LD_MEM_REG(addr, reg) \
	DO_LD_MEM(addr, reg) \
	return 2;

static int op_ld_ind_bc_a() {
	OP_LD_MEM_REG(REG_BC, REG_A);
}

static int op_ld_ind_de_a() {
	OP_LD_MEM_REG(REG_DE, REG_A);
}

static int op_ld_ind_hl_b() {
	OP_LD_MEM_REG(REG_HL, REG_B);
}

static int op_ld_ind_hl_c() {
	OP_LD_MEM_REG(REG_HL, REG_C);
}

static int op_ld_ind_hl_d() {
	OP_LD_MEM_REG(REG_HL, REG_D);
}

static int op_ld_ind_hl_e() {
	OP_LD_MEM_REG(REG_HL, REG_E);
}

static int op_ld_ind_hl_h() {
	OP_LD_MEM_REG(REG_HL, REG_H);
}

static int op_ld_ind_hl_l() {
	OP_LD_MEM_REG(REG_HL, REG_L);
}

static int op_ld_ind_hl_a() {
	OP_LD_MEM_REG(REG_HL, REG_A);
}

#define OP_LD_REG_MEM(reg, addr) \
	uint8_t load = monitor_rd_mem(addr); \
	DO_LD_REG(reg, load) \
	ADVANCE_PC(1); \
	return 2; \

static int op_ld_b_n8() {
	OP_LD_REG_MEM(REG_B, REG_PC);
}

static int op_ld_c_n8() {
	OP_LD_REG_MEM(REG_C, REG_PC);
}

static int op_ld_d_n8() {
	OP_LD_REG_MEM(REG_D, REG_PC);
}

static int op_ld_e_n8() {
	OP_LD_REG_MEM(REG_E, REG_PC);
}

static int op_ld_h_n8() {
	OP_LD_REG_MEM(REG_H, REG_PC);
}

static int op_ld_l_n8() {
	OP_LD_REG_MEM(REG_L, REG_PC);
}

static int op_ld_a_n8() {
	OP_LD_REG_MEM(REG_A, REG_PC);
}

#define OP_LD_A_IND_REG(reg) \
	uint8_t load = monitor_rd_mem(reg); \
	DO_LD_REG(REG_A, load); \
	return 2;

static int op_ld_a_ind_bc() {
	OP_LD_A_IND_REG(REG_BC);
}

static int op_ld_a_ind_de() {
	OP_LD_A_IND_REG(REG_DE);
}

static int op_ld_a_ind_hl() {
	OP_LD_A_IND_REG(REG_HL);
}

static int op_ld_a_ind_a16() {
	uint16_t addr = RD_WORD(REG_PC);
	REG_A = monitor_rd_mem(addr);
	ADVANCE_PC(2);
	return 4;
}

static int op_ld_sp_hl() {
	DO_LD_REG(REG_SP, REG_HL);
	return 1;
}

static int op_ldi_ind_hl_a() {
	monitor_wr_mem(REG_HL, REG_A);
	cpu.state.registers.hl++;
	return 2;
}

static int op_ldi_a_ind_hl() {
	cpu.state.registers.a = monitor_rd_mem(REG_HL);
	cpu.state.registers.hl++;
	return 2;
}

static int op_ldd_ind_hl_a() {
	monitor_wr_mem(REG_HL, REG_A);
	cpu.state.registers.hl--;
	return 2;
}

static int op_ldd_a_ind_hl() {
	cpu.state.registers.a = monitor_rd_mem(REG_HL);
	cpu.state.registers.hl--;
	return 2;
}

static int op_ld_c_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_LD_REG(REG_C, load);
	return 1;
}

static int op_ld_d_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_LD_REG(REG_D, load);
	return 1;
}

static int op_ld_e_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_LD_REG(REG_E, load);
	return 1;
}

static int op_ld_h_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_LD_REG(REG_H, load);
	return 1;
}

static int op_ld_l_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_LD_REG(REG_L, load);
	return 1;
}

// special snowflakes
static int op_ld_ind_a16_sp() {
	WR_WORD(RD_WORD(REG_PC), REG_SP);
	ADVANCE_PC(2);
	return 5;
}

static int op_ld_ind_hl_n8() {
	uint8_t load = monitor_rd_mem(REG_PC);
	monitor_wr_mem(REG_HL, load);
	ADVANCE_PC(1);
	return 3;
}

static int op_ld_ind_a16_a() {
	monitor_wr_mem(RD_WORD(REG_PC), REG_A);
	ADVANCE_PC(2);
	return 4;
}

static int op_ld_hl_sp_e8() {
	int8_t load = monitor_rd_mem(REG_PC);
	UNSET_FLAGS_ALL;
	SET_FLAG(BOOL_TO_HALF_CARRY_FLAG(IS_ADD_HALF_CARRY(REG_SP, load)));
	cpu.state.registers.hl = REG_SP+load;
	if (REG_HL < load)
		SET_FLAG(FLAG_CARRY);
	else if (REG_L < (uint8_t)load)
		SET_FLAG(FLAG_CARRY);
	ADVANCE_PC(1);
	return 3;
}

#define OP_INC_REG(reg) \
	UNSET_FLAG(FLAG_SUB | FLAG_HCARRY | FLAG_ZERO); \
	SET_FLAG(BOOL_TO_ZERO_FLAG(IS_ADD_ZERO(reg, 1))); \
	SET_FLAG(BOOL_TO_HALF_CARRY_FLAG(IS_ADD_HALF_CARRY(reg, 1))); \
	reg++; \
	return 1;

static int op_ldh_ind_a8_a() {
	uint16_t addr = monitor_rd_mem(REG_PC);
	addr |= 0xff00;
	monitor_wr_mem(addr, REG_A);
	ADVANCE_PC(1);
	return 2;
}

static int op_ldh_ind_c_a() {
	monitor_wr_mem(0xff00+REG_C, REG_A);
	return 2;
}

static int op_ldh_a_ind_a8() {
	REG_A = monitor_rd_mem(0xff00+monitor_rd_mem(REG_PC));
	ADVANCE_PC(1);
	return 3;
}

static int op_ldh_a_ind_c() {
	REG_A = monitor_rd_mem(0xff00 + REG_C);
	return 2;
}

static int op_inc_a() {
	OP_INC_REG(REG_A);
}

static int op_inc_b() {
	OP_INC_REG(REG_B);
}

static int op_inc_c() {
	OP_INC_REG(REG_C);
}

static int op_inc_d() {
	OP_INC_REG(REG_D);
}

static int op_inc_e() {
	OP_INC_REG(REG_E);
}

static int op_inc_h() {
	OP_INC_REG(REG_H);
}

static int op_inc_l() {
	OP_INC_REG(REG_L);
}

static int op_inc_bc() {
	cpu.state.registers.bc++;
	return 2;
}

static int op_inc_de() {
	cpu.state.registers.de++;
	return 2;
}

static int op_inc_hl() {
	cpu.state.registers.hl++;
	return 2;
}

static int op_inc_sp() {
	cpu.state.registers.sp++;
	return 2;
}

static int op_inc_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	UNSET_FLAG(FLAG_ZERO|FLAG_HCARRY|FLAG_SUB);
	SET_FLAG(BOOL_TO_HALF_CARRY_FLAG((bool)(((load) ^ 1 ^ (load + 1))&0x10)));
	load++;
	monitor_wr_mem(REG_HL, load);
	SET_FLAG(BOOL_TO_ZERO_FLAG(load == 0));
	return 3;
}

#define OP_DEC_REG(reg) \
	UNSET_FLAG(FLAG_ZERO | FLAG_HCARRY); \
	SET_FLAG(BOOL_TO_ZERO_FLAG(IS_SUB_ZERO(reg, 1))); \
	SET_FLAG(BOOL_TO_HALF_CARRY_FLAG(IS_SUB_HALF_CARRY(reg, 1))); \
	SET_FLAG(FLAG_SUB); \
	reg--; \
	return 1;

static int op_dec_a() {
	OP_DEC_REG(REG_A);
}

static int op_dec_b() {
	OP_DEC_REG(REG_B);
}

static int op_dec_c() {
	OP_DEC_REG(REG_C);
}

static int op_dec_d() {
	OP_DEC_REG(REG_D);
}

static int op_dec_e() {
	OP_DEC_REG(REG_E);
}

static int op_dec_h() {
	OP_DEC_REG(REG_H);
}

static int op_dec_l() {
	OP_DEC_REG(REG_L);
}

static int op_dec_bc() {
	cpu.state.registers.bc--;
	return 2;
}

static int op_dec_de() {
	cpu.state.registers.de--;
	return 2;
}

static int op_dec_hl() {
	cpu.state.registers.hl--;
	return 2;
}

static int op_dec_sp() {
	cpu.state.registers.sp--;
	return 2;
}

static int op_dec_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	UNSET_FLAG(FLAG_ZERO|FLAG_HCARRY);
	SET_FLAG(BOOL_TO_HALF_CARRY_FLAG((bool)(((load) ^ 1 ^ (load - 1))&0x10)));
	SET_FLAG(FLAG_SUB);
	load--;
	monitor_wr_mem(REG_HL, load);
	SET_FLAG(BOOL_TO_ZERO_FLAG(load == 0));
	return 3;
}

#define DO_ADD(reg, carry) \
	UNSET_FLAGS_ALL; \
	SET_FLAG(BOOL_TO_CARRY_FLAG(IS_ADD_CARRY(REG_A, reg+carry))); \
	SET_FLAG(BOOL_TO_HALF_CARRY_FLAG(IS_ADD_HALF_CARRY(REG_A, reg+carry))); \
	SET_FLAG(BOOL_TO_ZERO_FLAG(IS_ADD_ZERO(REG_A, reg+carry))); \
	REG_A += reg+carry; \

#define OP_ADD_REG(reg) \
	DO_ADD(reg, 0) \
	return 1;

static int op_add_a_a() {
	OP_ADD_REG(REG_A);
}

static int op_add_a_b() {
	OP_ADD_REG(REG_B);
}

static int op_add_a_c() {
	OP_ADD_REG(REG_C);
}

static int op_add_a_d() {
	OP_ADD_REG(REG_D);
}

static int op_add_a_e() {
	OP_ADD_REG(REG_E);
}

static int op_add_a_h() {
	OP_ADD_REG(REG_H);
}

static int op_add_a_l() {
	OP_ADD_REG(REG_L);
}

#define OP_ADD_REG16(reg) \
	UNSET_FLAG(FLAG_CARRY | FLAG_HCARRY | FLAG_SUB); \
	SET_FLAG(BOOL_TO_CARRY_FLAG(IS_ADD16_CARRY(REG_HL, reg))); \
	SET_FLAG(BOOL_TO_HALF_CARRY_FLAG(IS_ADD16_HALF_CARRY(REG_HL, reg))); \
	REG_HL += reg; \
	return 2;

static int op_add_hl_bc() {
	OP_ADD_REG16(REG_BC);
}

static int op_add_hl_de() {
	OP_ADD_REG16(REG_DE);
}

static int op_add_hl_hl() {
	OP_ADD_REG16(REG_HL);
}

static int op_add_hl_sp() {
	OP_ADD_REG16(REG_SP);
}

static int op_add_a_ind_hl() {
	int8_t load = monitor_rd_mem(REG_HL);
	DO_ADD(load, 0);
	return 2;
}

static int op_add_a_n8() {
	int8_t load = monitor_rd_mem(REG_PC);
	ADVANCE_PC(1);
	DO_ADD(load, 0);
	return 2;
}

static int op_add_sp_e8() {
	int8_t load = monitor_rd_mem(REG_PC);
	UNSET_FLAGS_ALL;
	SET_FLAG(BOOL_TO_HALF_CARRY_FLAG(IS_ADD_HALF_CARRY(REG_SP, load)));
	REG_SP += load;
	if (REG_SP < load)
		SET_FLAG(FLAG_CARRY);
	else if ((REG_SP&0xff) < (uint8_t)load)
		SET_FLAG(FLAG_CARRY);
	ADVANCE_PC(1);
	return 4;
}

#define DO_ADC(reg) \
	bool carry = REG_F & FLAG_CARRY; \
	UNSET_FLAGS_ALL; \
	if ((reg ^ REG_A ^ (REG_A+reg+carry)) & 0x10) \
		SET_FLAG(FLAG_HCARRY); \
	SET_FLAG(BOOL_TO_ZERO_FLAG(IS_ADD_ZERO(REG_A, reg+carry))); \
	if ((REG_A + reg + carry)&0xff00) \
		SET_FLAG(FLAG_CARRY); \
	REG_A += reg+carry;

#define OP_ADC_REG(reg) \
	DO_ADC(reg); \
	return 1;

static int op_adc_a_a() {
	OP_ADC_REG(REG_A);
}

static int op_adc_a_b() {
	OP_ADC_REG(REG_B);
}

static int op_adc_a_c() {
	OP_ADC_REG(REG_C);
}

static int op_adc_a_d() {
	OP_ADC_REG(REG_D);
}

static int op_adc_a_e() {
	OP_ADC_REG(REG_E);
}

static int op_adc_a_h() {
	OP_ADC_REG(REG_H);
}

static int op_adc_a_l() {
	OP_ADC_REG(REG_L);
}

static int op_adc_a_ind_hl() {
	uint8_t word = monitor_rd_mem(REG_HL);
	bool carry = cpu.state.registers.f & FLAG_CARRY;
	uint16_t sum = REG_A + word + carry;
	UNSET_FLAGS_ALL;
	if ((word ^ sum ^ REG_A)&0x10)
		SET_FLAG(FLAG_HCARRY);

	REG_A += word+carry;
	if (sum & 0xff00)
		SET_FLAG(FLAG_CARRY);
	if (REG_A == 0)
		SET_FLAG(FLAG_ZERO);
	return 2;
}

static int op_adc_a_n8() {
	uint8_t word = monitor_rd_mem(REG_PC);
	bool carry = cpu.state.registers.f & FLAG_CARRY;
	uint16_t sum = REG_A + word + carry;
	UNSET_FLAGS_ALL;
	if ((word ^ sum ^ REG_A)&0x10)
		SET_FLAG(FLAG_HCARRY);

	REG_A += word+carry;
	if (sum & 0xff00)
		SET_FLAG(FLAG_CARRY);
	if (REG_A == 0)
		SET_FLAG(FLAG_ZERO);
	ADVANCE_PC(1);
	return 2;
}

#define DO_SUB(reg, carry) \
	UNSET_FLAGS_ALL; \
	SET_FLAG(BOOL_TO_CARRY_FLAG(IS_SUB_CARRY(REG_A, reg+carry))); \
	SET_FLAG(BOOL_TO_HALF_CARRY_FLAG(IS_SUB_HALF_CARRY(REG_A, reg+carry))); \
	SET_FLAG(BOOL_TO_ZERO_FLAG(IS_SUB_ZERO(REG_A, reg+carry))); \
	SET_FLAG(FLAG_SUB); \
	REG_A -= (reg+carry); \
	return 1;

#define OP_SUB_REG(reg) \
	DO_SUB(reg, 0)

static int op_sub_a_a() {
	OP_SUB_REG(REG_A);
}

static int op_sub_a_b() {
	OP_SUB_REG(REG_B);
}

static int op_sub_a_c() {
	OP_SUB_REG(REG_C);
}

static int op_sub_a_d() {
	OP_SUB_REG(REG_D);
}

static int op_sub_a_e() {
	OP_SUB_REG(REG_E);
}

static int op_sub_a_h() {
	OP_SUB_REG(REG_H);
}

static int op_sub_a_l() {
	OP_SUB_REG(REG_L);
}

static int op_sub_a_ind_hl() {
	int8_t load = monitor_rd_mem(REG_HL);
	DO_SUB(load, 0);
	return 2;
}

static int op_sub_a_n8() {
	int8_t load = monitor_rd_mem(REG_PC);
	ADVANCE_PC(1);
	DO_SUB(load, 0);
	return 2;
}

#define DO_SBC(reg) \
	bool carry = REG_F & FLAG_CARRY; \
	UNSET_FLAGS_ALL; \
	uint16_t sum = REG_A - (reg + carry); \
	if ((reg ^ REG_A ^ sum) & 0x10) \
		SET_FLAG(FLAG_HCARRY); \
	if (sum&0xff00) \
		SET_FLAG(FLAG_CARRY); \
	REG_A -= reg+carry; \
	SET_FLAG(FLAG_SUB); \
	if (REG_A == 0) \
		SET_FLAG(FLAG_ZERO);

#define OP_SBC_REG(reg) \
	DO_SBC(reg); \
	return 1;

static int op_sbc_a_a() {
	OP_SBC_REG(REG_A);
}

static int op_sbc_a_b() {
	OP_SBC_REG(REG_B);
}

static int op_sbc_a_c() {
	OP_SBC_REG(REG_C);
}

static int op_sbc_a_d() {
	OP_SBC_REG(REG_D);
}

static int op_sbc_a_e() {
	OP_SBC_REG(REG_E);
}

static int op_sbc_a_h() {
	OP_SBC_REG(REG_H);
}

static int op_sbc_a_l() {
	OP_SBC_REG(REG_L);
}

static int op_sbc_a_ind_hl() {
	uint8_t word = monitor_rd_mem(REG_HL);
	bool carry = cpu.state.registers.f & FLAG_CARRY;
	uint16_t sum = REG_A - (word + carry);
	UNSET_FLAGS_ALL;
	if ((word ^ sum ^ REG_A)&0x10)
		SET_FLAG(FLAG_HCARRY);

	REG_A -= word+carry;
	if (sum & 0xff00)
		SET_FLAG(FLAG_CARRY);
	if (REG_A == 0)
		SET_FLAG(FLAG_ZERO);
	SET_FLAG(FLAG_SUB);
	return 2;
}

static int op_sbc_a_n8() {
	uint8_t word = monitor_rd_mem(REG_PC);
	bool carry = cpu.state.registers.f & FLAG_CARRY;
	uint16_t sum = REG_A - (word + carry);
	UNSET_FLAGS_ALL;
	if ((word ^ sum ^ REG_A)&0x10)
		SET_FLAG(FLAG_HCARRY);

	REG_A -= word+carry;
	if (sum & 0xff00)
		SET_FLAG(FLAG_CARRY);
	if (REG_A == 0)
		SET_FLAG(FLAG_ZERO);
	SET_FLAG(FLAG_SUB);
	ADVANCE_PC(1);
	return 2;
}

#define DO_RLC(value) \
	bool carry = value & 0x80; \
	UNSET_FLAGS_ALL; \
	SET_FLAG(BOOL_TO_CARRY_FLAG(carry)); \
	value <<= 1; \
	value &= ~1; \
	value |= carry; \
	SET_FLAG(BOOL_TO_ZERO_FLAG(value == 0));

// Rotate instructions
#define OP_RLC_REG(reg) \
	DO_RLC(reg) \
	return 2;

static int op_rlc_b() {
	OP_RLC_REG(REG_B);
}

static int op_rlc_c() {
	OP_RLC_REG(REG_C);
}

static int op_rlc_d() {
	OP_RLC_REG(REG_D);
}

static int op_rlc_e() {
	OP_RLC_REG(REG_E);
}

static int op_rlc_h() {
	OP_RLC_REG(REG_H);
}

static int op_rlc_l() {
	OP_RLC_REG(REG_L);
}

static int op_rlc_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_RLC(load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_rlc_a() {
	OP_RLC_REG(REG_A);
}

static int op_rlca() {
	DO_RLC(REG_A);
	UNSET_FLAG(FLAG_ZERO);
	return 1;
}

static int op_stop_n8() {
	ADVANCE_PC(1);
	return 2;
}

#define DO_RL(value) \
	bool carry = cpu.state.registers.f & FLAG_CARRY; \
	bool bit7 = value & 0x80; \
	UNSET_FLAGS_ALL; \
	SET_FLAG(BOOL_TO_CARRY_FLAG(bit7)); \
	value <<= 1; \
	value |= carry; \
	/* can be 0, in which this is a no-op. done to reduce branching by avoiding introducing an 'if' */ \
	SET_FLAG(BOOL_TO_ZERO_FLAG(value == 0)); \

#define OP_RL_REG(reg) \
	DO_RL(reg); \
	return 2;

static int op_rl_b() {
	OP_RL_REG(REG_B);
}

static int op_rl_c() {
	OP_RL_REG(REG_C);
}

static int op_rl_d() {
	OP_RL_REG(REG_D);
}

static int op_rl_e() {
	OP_RL_REG(REG_E);
}

static int op_rl_h() {
	OP_RL_REG(REG_H);
}

static int op_rl_l() {
	OP_RL_REG(REG_L);
}

static int op_rl_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_RL(load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_rl_a() {
	OP_RL_REG(REG_A);
}

static int op_rla() {
	DO_RL(REG_A);
	UNSET_FLAG(FLAG_ZERO);
	return 1;
}

#define DO_RRC(value) \
	UNSET_FLAGS_ALL; \
	SET_FLAG(BOOL_TO_CARRY_FLAG(value&0x1)); \
	uint8_t carry = (value & 0x1) ? 1 << 7 : 0; \
	value >>= 1; \
	value &= ~0x80; \
	value |= carry; \
	SET_FLAG(BOOL_TO_ZERO_FLAG(value == 0)); \

#define OP_RRC_REG(reg) \
	DO_RRC(reg); \
	return 2;

static int op_rrc_b() {
	OP_RRC_REG(REG_B);
}

static int op_rrc_c() {
	OP_RRC_REG(REG_C);
}

static int op_rrc_d() {
	OP_RRC_REG(REG_D);
}

static int op_rrc_e() {
	OP_RRC_REG(REG_E);
}

static int op_rrc_h() {
	OP_RRC_REG(REG_H);
}

static int op_rrc_l() {
	OP_RRC_REG(REG_L);
}

static int op_rrc_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_RRC(load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_rrc_a() {
	OP_RRC_REG(REG_A);
}

static int op_rrca() {
	DO_RRC(REG_A);
	UNSET_FLAG(FLAG_ZERO);
	return 1;
}

#define DO_RR(value) \
	uint8_t carry = cpu.state.registers.f & FLAG_CARRY; \
	bool bit0 = value & 0x01; \
	UNSET_FLAGS_ALL; \
	SET_FLAG(BOOL_TO_CARRY_FLAG(bit0)); \
	value >>= 1; \
	value &= ~0x80; \
	value |= ((bool)carry)<<7; \
	SET_FLAG(BOOL_TO_ZERO_FLAG(value == 0)); \

#define OP_RR_REG(reg) \
	DO_RR(reg); \
	return 2;

static int op_rr_b() {
	OP_RR_REG(REG_B);
}

static int op_rr_c() {
	OP_RR_REG(REG_C);
}

static int op_rr_d() {
	OP_RR_REG(REG_D);
}

static int op_rr_e() {
	OP_RR_REG(REG_E);
}

static int op_rr_h() {
	OP_RR_REG(REG_H);
}

static int op_rr_l() {
	OP_RR_REG(REG_L);
}

static int op_rr_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_RR(load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_rr_a() {
	OP_RR_REG(REG_A);
}

static int op_rra() {
	DO_RR(REG_A);
	UNSET_FLAG(FLAG_ZERO);
	return 1;
}

#define DO_SLA(value) \
	UNSET_FLAGS_ALL; \
	bool carry = value & 0x80; \
	value <<= 1; \
	SET_FLAG(BOOL_TO_CARRY_FLAG(carry)); \
	SET_FLAG(BOOL_TO_ZERO_FLAG(value==0));

#define OP_SLA_REG(reg) \
	DO_SLA(reg); \
	return 2;

static int op_sla_b() {
	OP_SLA_REG(REG_B);
}

static int op_sla_c() {
	OP_SLA_REG(REG_C);
}

static int op_sla_d() {
	OP_SLA_REG(REG_D);
}

static int op_sla_e() {
	OP_SLA_REG(REG_E);
}

static int op_sla_h() {
	OP_SLA_REG(REG_H);
}

static int op_sla_l() {
	OP_SLA_REG(REG_L);
}

static int op_sla_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_SLA(load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_sla_a() {
	OP_SLA_REG(REG_A);
}

#define DO_SRA(value) \
	UNSET_FLAGS_ALL; \
	uint8_t bit7 = value & 0x80; \
	uint8_t carry = value & 0x01; \
	value >>= 1; \
	value &= ~bit7; \
	value |= bit7; \
	SET_FLAG(BOOL_TO_CARRY_FLAG(carry)); \
	SET_FLAG(BOOL_TO_ZERO_FLAG(value==0));

#define OP_SRA_REG(reg) \
	DO_SRA(reg); \
	return 2;

static int op_sra_b() {
	OP_SRA_REG(REG_B);
}

static int op_sra_c() {
	OP_SRA_REG(REG_C);
}

static int op_sra_d() {
	OP_SRA_REG(REG_D);
}

static int op_sra_e() {
	OP_SRA_REG(REG_E);
}

static int op_sra_h() {
	OP_SRA_REG(REG_H);
}

static int op_sra_l() {
	OP_SRA_REG(REG_L);
}

static int op_sra_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_SRA(load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_sra_a() {
	OP_SRA_REG(REG_A);
}

#define DO_SRL(value) \
	uint8_t carry_flag = BOOL_TO_CARRY_FLAG(value & 0x01); \
	value >>= 1; \
	uint8_t zero_flag = BOOL_TO_ZERO_FLAG(value == 0); \
	UNSET_FLAGS_ALL; \
	SET_FLAG(zero_flag|carry_flag); \

#define OP_SRL_REG(reg) \
	DO_SRL(reg) \
	return 2;

static int op_srl_b() {
	OP_SRL_REG(REG_B);
}

static int op_srl_c() {
	OP_SRL_REG(REG_C);
}

static int op_srl_d() {
	OP_SRL_REG(REG_D);
}

static int op_srl_e() {
	OP_SRL_REG(REG_E);
}

static int op_srl_h() {
	OP_SRL_REG(REG_H);
}

static int op_srl_l() {
	OP_SRL_REG(REG_L);
}

static int op_srl_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_SRL(load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_srl_a() {
	OP_SRL_REG(REG_A);
}

#define DO_SWP(value) \
	uint8_t low_nibble = value & 0x0f; \
	value >>= 4; \
	value |= low_nibble << 4; \
	UNSET_FLAGS_ALL; \
	SET_FLAG(BOOL_TO_ZERO_FLAG(value == 0));

#define OP_SWP_REG(reg) \
	DO_SWP(reg); \
	return 2;

static int op_swp_b() {
	OP_SWP_REG(REG_B);
}

static int op_swp_c() {
	OP_SWP_REG(REG_C);
}

static int op_swp_d() {
	OP_SWP_REG(REG_D);
}

static int op_swp_e() {
	OP_SWP_REG(REG_E);
}

static int op_swp_h() {
	OP_SWP_REG(REG_H);
}

static int op_swp_l() {
	OP_SWP_REG(REG_L);
}

static int op_swp_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_SWP(load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_swp_a() {
	OP_SWP_REG(REG_A);
}

#define DO_BIT(bit, value) \
	UNSET_FLAG(FLAG_ZERO|FLAG_SUB); \
	SET_FLAG(FLAG_HCARRY); \
	bool is_zero = !(value & bit); \
	SET_FLAG(BOOL_TO_ZERO_FLAG(is_zero));

#define OP_BIT_REG(bit, reg) \
	DO_BIT(bit, reg); \
	return 2;

enum bits {
	BIT0 = 0b00000001,
	BIT1 = BIT0<<1,
	BIT2 = BIT0<<2,
	BIT3 = BIT0<<3,
	BIT4 = BIT0<<4,
	BIT5 = BIT0<<5,
	BIT6 = BIT0<<6,
	BIT7 = BIT0<<7,
};

static int op_bit0_b() {
	OP_BIT_REG(BIT0, REG_B);
}

static int op_bit0_c() {
	OP_BIT_REG(BIT0, REG_C);
}

static int op_bit0_d() {
	OP_BIT_REG(BIT0, REG_D);
}

static int op_bit0_e() {
	OP_BIT_REG(BIT0, REG_E);
}

static int op_bit0_h() {
	OP_BIT_REG(BIT0, REG_H);
}

static int op_bit0_l() {
	OP_BIT_REG(BIT0, REG_L);
}

static int op_bit0_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_BIT(BIT0, load);
	return 3;
}

static int op_bit0_a() {
	OP_BIT_REG(BIT0, REG_A);
}

static int op_bit1_b() {
	OP_BIT_REG(BIT1, REG_B);
}

static int op_bit1_c() {
	OP_BIT_REG(BIT1, REG_C);
}

static int op_bit1_d() {
	OP_BIT_REG(BIT1, REG_D);
}

static int op_bit1_e() {
	OP_BIT_REG(BIT1, REG_E);
}

static int op_bit1_h() {
	OP_BIT_REG(BIT1, REG_H);
}

static int op_bit1_l() {
	OP_BIT_REG(BIT1, REG_L);
}

static int op_bit1_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_BIT(BIT1, load);
	return 3;
}

static int op_bit1_a() {
	OP_BIT_REG(BIT1, REG_A);
}

static int op_bit2_b() {
	OP_BIT_REG(BIT2, REG_B);
}

static int op_bit2_c() {
	OP_BIT_REG(BIT2, REG_C);
}

static int op_bit2_d() {
	OP_BIT_REG(BIT2, REG_D);
}

static int op_bit2_e() {
	OP_BIT_REG(BIT2, REG_E);
}

static int op_bit2_h() {
	OP_BIT_REG(BIT2, REG_H);
}

static int op_bit2_l() {
	OP_BIT_REG(BIT2, REG_L);
}

static int op_bit2_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_BIT(BIT2, load);
	return 3;
}

static int op_bit2_a() {
	OP_BIT_REG(BIT2, REG_A);
}

static int op_bit3_b() {
	OP_BIT_REG(BIT3, REG_B);
}

static int op_bit3_c() {
	OP_BIT_REG(BIT3, REG_C);
}

static int op_bit3_d() {
	OP_BIT_REG(BIT3, REG_D);
}

static int op_bit3_e() {
	OP_BIT_REG(BIT3, REG_E);
}

static int op_bit3_h() {
	OP_BIT_REG(BIT3, REG_H);
}

static int op_bit3_l() {
	OP_BIT_REG(BIT3, REG_L);
}

static int op_bit3_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_BIT(BIT3, load);
	return 3;
}

static int op_bit3_a() {
	OP_BIT_REG(BIT3, REG_A);
}

static int op_bit4_b() {
	OP_BIT_REG(BIT4, REG_B);
}

static int op_bit4_c() {
	OP_BIT_REG(BIT4, REG_C);
}

static int op_bit4_d() {
	OP_BIT_REG(BIT4, REG_D);
}

static int op_bit4_e() {
	OP_BIT_REG(BIT4, REG_E);
}

static int op_bit4_h() {
	OP_BIT_REG(BIT4, REG_H);
}

static int op_bit4_l() {
	OP_BIT_REG(BIT4, REG_L);
}

static int op_bit4_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_BIT(BIT4, load);
	return 3;
}

static int op_bit4_a() {
	OP_BIT_REG(BIT4, REG_A);
}

static int op_bit5_b() {
	OP_BIT_REG(BIT5, REG_B);
}

static int op_bit5_c() {
	OP_BIT_REG(BIT5, REG_C);
}

static int op_bit5_d() {
	OP_BIT_REG(BIT5, REG_D);
}

static int op_bit5_e() {
	OP_BIT_REG(BIT5, REG_E);
}

static int op_bit5_h() {
	OP_BIT_REG(BIT5, REG_H);
}

static int op_bit5_l() {
	OP_BIT_REG(BIT5, REG_L);
}

static int op_bit5_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_BIT(BIT5, load);
	return 3;
}

static int op_bit5_a() {
	OP_BIT_REG(BIT5, REG_A);
}

static int op_bit6_b() {
	OP_BIT_REG(BIT6, REG_B);
}

static int op_bit6_c() {
	OP_BIT_REG(BIT6, REG_C);
}

static int op_bit6_d() {
	OP_BIT_REG(BIT6, REG_D);
}

static int op_bit6_e() {
	OP_BIT_REG(BIT6, REG_E);
}

static int op_bit6_h() {
	OP_BIT_REG(BIT6, REG_H);
}

static int op_bit6_l() {
	OP_BIT_REG(BIT6, REG_L);
}

static int op_bit6_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_BIT(BIT6, load);
	return 3;
}

static int op_bit6_a() {
	OP_BIT_REG(BIT6, REG_A);
}

static int op_bit7_b() {
	OP_BIT_REG(BIT7, REG_B);
}

static int op_bit7_c() {
	OP_BIT_REG(BIT7, REG_C);
}

static int op_bit7_d() {
	OP_BIT_REG(BIT7, REG_D);
}

static int op_bit7_e() {
	OP_BIT_REG(BIT7, REG_E);
}

static int op_bit7_h() {
	OP_BIT_REG(BIT7, REG_H);
}

static int op_bit7_l() {
	OP_BIT_REG(BIT7, REG_L);
}

static int op_bit7_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_BIT(BIT7, load);
	return 3;
}

static int op_bit7_a() {
	OP_BIT_REG(BIT7, REG_A);
}

#define DO_RST(bit, value) \
	value &= ~bit;

#define OP_RST_REG(bit, reg) \
	DO_RST(bit, reg) \
	return 2;

static int op_rst0_b() {
	OP_RST_REG(BIT0, REG_B);
}

static int op_rst0_c() {
	OP_RST_REG(BIT0, REG_C);
}

static int op_rst0_d() {
	OP_RST_REG(BIT0, REG_D);
}

static int op_rst0_e() {
	OP_RST_REG(BIT0, REG_E);
}

static int op_rst0_h() {
	OP_RST_REG(BIT0, REG_H);
}

static int op_rst0_l() {
	OP_RST_REG(BIT0, REG_L);
}

static int op_rst0_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_RST(BIT0, load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_rst0_a() {
	OP_RST_REG(BIT0, REG_A);
}

static int op_rst1_b() {
	OP_RST_REG(BIT1, REG_B);
}

static int op_rst1_c() {
	OP_RST_REG(BIT1, REG_C);
}

static int op_rst1_d() {
	OP_RST_REG(BIT1, REG_D);
}

static int op_rst1_e() {
	OP_RST_REG(BIT1, REG_E);
}

static int op_rst1_h() {
	OP_RST_REG(BIT1, REG_H);
}

static int op_rst1_l() {
	OP_RST_REG(BIT1, REG_L);
}

static int op_rst1_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_RST(BIT1, load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_rst1_a() {
	OP_RST_REG(BIT1, REG_A);
}

static int op_rst2_b() {
	OP_RST_REG(BIT2, REG_B);
}

static int op_rst2_c() {
	OP_RST_REG(BIT2, REG_C);
}

static int op_rst2_d() {
	OP_RST_REG(BIT2, REG_D);
}

static int op_rst2_e() {
	OP_RST_REG(BIT2, REG_E);
}

static int op_rst2_h() {
	OP_RST_REG(BIT2, REG_H);
}

static int op_rst2_l() {
	OP_RST_REG(BIT2, REG_L);
}

static int op_rst2_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_RST(BIT2, load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_rst2_a() {
	OP_RST_REG(BIT2, REG_A);
}

static int op_rst3_b() {
	OP_RST_REG(BIT3, REG_B);
}

static int op_rst3_c() {
	OP_RST_REG(BIT3, REG_C);
}

static int op_rst3_d() {
	OP_RST_REG(BIT3, REG_D);
}

static int op_rst3_e() {
	OP_RST_REG(BIT3, REG_E);
}

static int op_rst3_h() {
	OP_RST_REG(BIT3, REG_H);
}

static int op_rst3_l() {
	OP_RST_REG(BIT3, REG_L);
}

static int op_rst3_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_RST(BIT3, load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_rst3_a() {
	OP_RST_REG(BIT3, REG_A);
}

static int op_rst4_b() {
	OP_RST_REG(BIT4, REG_B);
}

static int op_rst4_c() {
	OP_RST_REG(BIT4, REG_C);
}

static int op_rst4_d() {
	OP_RST_REG(BIT4, REG_D);
}

static int op_rst4_e() {
	OP_RST_REG(BIT4, REG_E);
}

static int op_rst4_h() {
	OP_RST_REG(BIT4, REG_H);
}

static int op_rst4_l() {
	OP_RST_REG(BIT4, REG_L);
}

static int op_rst4_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_RST(BIT4, load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_rst4_a() {
	OP_RST_REG(BIT4, REG_A);
}

static int op_rst5_b() {
	OP_RST_REG(BIT5, REG_B);
}

static int op_rst5_c() {
	OP_RST_REG(BIT5, REG_C);
}

static int op_rst5_d() {
	OP_RST_REG(BIT5, REG_D);
}

static int op_rst5_e() {
	OP_RST_REG(BIT5, REG_E);
}

static int op_rst5_h() {
	OP_RST_REG(BIT5, REG_H);
}

static int op_rst5_l() {
	OP_RST_REG(BIT5, REG_L);
}

static int op_rst5_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_RST(BIT5, load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_rst5_a() {
	OP_RST_REG(BIT5, REG_A);
}

static int op_rst6_b() {
	OP_RST_REG(BIT6, REG_B);
}

static int op_rst6_c() {
	OP_RST_REG(BIT6, REG_C);
}

static int op_rst6_d() {
	OP_RST_REG(BIT6, REG_D);
}

static int op_rst6_e() {
	OP_RST_REG(BIT6, REG_E);
}

static int op_rst6_h() {
	OP_RST_REG(BIT6, REG_H);
}

static int op_rst6_l() {
	OP_RST_REG(BIT6, REG_L);
}

static int op_rst6_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_RST(BIT6, load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_rst6_a() {
	OP_RST_REG(BIT6, REG_A);
}

static int op_rst7_b() {
	OP_RST_REG(BIT7, REG_B);
}

static int op_rst7_c() {
	OP_RST_REG(BIT7, REG_C);
}

static int op_rst7_d() {
	OP_RST_REG(BIT7, REG_D);
}

static int op_rst7_e() {
	OP_RST_REG(BIT7, REG_E);
}

static int op_rst7_h() {
	OP_RST_REG(BIT7, REG_H);
}

static int op_rst7_l() {
	OP_RST_REG(BIT7, REG_L);
}

static int op_rst7_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_RST(BIT7, load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_rst7_a() {
	OP_RST_REG(BIT7, REG_A);
}

#define DO_SET(bit, value) \
	value |= bit;

#define OP_SET_REG(bit, reg) \
	DO_SET(bit, reg) \
	return 2;

static int op_set0_b() {
	OP_SET_REG(BIT0, REG_B);
}

static int op_set0_c() {
	OP_SET_REG(BIT0, REG_C);
}

static int op_set0_d() {
	OP_SET_REG(BIT0, REG_D);
}

static int op_set0_e() {
	OP_SET_REG(BIT0, REG_E);
}

static int op_set0_h() {
	OP_SET_REG(BIT0, REG_H);
}

static int op_set0_l() {
	OP_SET_REG(BIT0, REG_L);
}

static int op_set0_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_SET(BIT0, load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_set0_a() {
	OP_SET_REG(BIT0, REG_A);
}

static int op_set1_b() {
	OP_SET_REG(BIT1, REG_B);
}

static int op_set1_c() {
	OP_SET_REG(BIT1, REG_C);
}

static int op_set1_d() {
	OP_SET_REG(BIT1, REG_D);
}

static int op_set1_e() {
	OP_SET_REG(BIT1, REG_E);
}

static int op_set1_h() {
	OP_SET_REG(BIT1, REG_H);
}

static int op_set1_l() {
	OP_SET_REG(BIT1, REG_L);
}

static int op_set1_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_SET(BIT1, load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_set1_a() {
	OP_SET_REG(BIT1, REG_A);
}

static int op_set2_b() {
	OP_SET_REG(BIT2, REG_B);
}

static int op_set2_c() {
	OP_SET_REG(BIT2, REG_C);
}

static int op_set2_d() {
	OP_SET_REG(BIT2, REG_D);
}

static int op_set2_e() {
	OP_SET_REG(BIT2, REG_E);
}

static int op_set2_h() {
	OP_SET_REG(BIT2, REG_H);
}

static int op_set2_l() {
	OP_SET_REG(BIT2, REG_L);
}

static int op_set2_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_SET(BIT2, load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_set2_a() {
	OP_SET_REG(BIT2, REG_A);
}

static int op_set3_b() {
	OP_SET_REG(BIT3, REG_B);
}

static int op_set3_c() {
	OP_SET_REG(BIT3, REG_C);
}

static int op_set3_d() {
	OP_SET_REG(BIT3, REG_D);
}

static int op_set3_e() {
	OP_SET_REG(BIT3, REG_E);
}

static int op_set3_h() {
	OP_SET_REG(BIT3, REG_H);
}

static int op_set3_l() {
	OP_SET_REG(BIT3, REG_L);
}

static int op_set3_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_SET(BIT3, load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_set3_a() {
	OP_SET_REG(BIT3, REG_A);
}

static int op_set4_b() {
	OP_SET_REG(BIT4, REG_B);
}

static int op_set4_c() {
	OP_SET_REG(BIT4, REG_C);
}

static int op_set4_d() {
	OP_SET_REG(BIT4, REG_D);
}

static int op_set4_e() {
	OP_SET_REG(BIT4, REG_E);
}

static int op_set4_h() {
	OP_SET_REG(BIT4, REG_H);
}

static int op_set4_l() {
	OP_SET_REG(BIT4, REG_L);
}

static int op_set4_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_SET(BIT4, load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_set4_a() {
	OP_SET_REG(BIT4, REG_A);
}

static int op_set5_b() {
	OP_SET_REG(BIT5, REG_B);
}

static int op_set5_c() {
	OP_SET_REG(BIT5, REG_C);
}

static int op_set5_d() {
	OP_SET_REG(BIT5, REG_D);
}

static int op_set5_e() {
	OP_SET_REG(BIT5, REG_E);
}

static int op_set5_h() {
	OP_SET_REG(BIT5, REG_H);
}

static int op_set5_l() {
	OP_SET_REG(BIT5, REG_L);
}

static int op_set5_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_SET(BIT5, load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_set5_a() {
	OP_SET_REG(BIT5, REG_A);
}

static int op_set6_b() {
	OP_SET_REG(BIT6, REG_B);
}

static int op_set6_c() {
	OP_SET_REG(BIT6, REG_C);
}

static int op_set6_d() {
	OP_SET_REG(BIT6, REG_D);
}

static int op_set6_e() {
	OP_SET_REG(BIT6, REG_E);
}

static int op_set6_h() {
	OP_SET_REG(BIT6, REG_H);
}

static int op_set6_l() {
	OP_SET_REG(BIT6, REG_L);
}

static int op_set6_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_SET(BIT6, load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_set6_a() {
	OP_SET_REG(BIT6, REG_A);
}

static int op_set7_b() {
	OP_SET_REG(BIT7, REG_B);
}

static int op_set7_c() {
	OP_SET_REG(BIT7, REG_C);
}

static int op_set7_d() {
	OP_SET_REG(BIT7, REG_D);
}

static int op_set7_e() {
	OP_SET_REG(BIT7, REG_E);
}

static int op_set7_h() {
	OP_SET_REG(BIT7, REG_H);
}

static int op_set7_l() {
	OP_SET_REG(BIT7, REG_L);
}

static int op_set7_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_SET(BIT7, load);
	monitor_wr_mem(REG_HL, load);
	return 4;
}

static int op_set7_a() {
	OP_SET_REG(BIT7, REG_A);
}

#define DO_AND(value) \
	REG_A &= value; \
	UNSET_FLAGS_ALL; \
	SET_FLAG(FLAG_HCARRY); \
	SET_FLAG(BOOL_TO_ZERO_FLAG(REG_A == 0));

#define OP_AND_REG(reg) \
	DO_AND(reg) \
	return 1;

static int op_and_a_b() {
	OP_AND_REG(REG_B);
}

static int op_and_a_c() {
	OP_AND_REG(REG_C);
}

static int op_and_a_d() {
	OP_AND_REG(REG_D);
}

static int op_and_a_e() {
	OP_AND_REG(REG_E);
}

static int op_and_a_h() {
	OP_AND_REG(REG_H);
}

static int op_and_a_l() {
	OP_AND_REG(REG_L);
}

static int op_and_a_a() {
	OP_AND_REG(REG_A);
}

static int op_and_a_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_AND(load);
	return 2;
}

static int op_and_a_n8() {
	uint8_t load = monitor_rd_mem(REG_PC);
	ADVANCE_PC(1);
	DO_AND(load);
	return 2;
}

#define DO_XOR(value) \
	REG_A ^= value; \
	UNSET_FLAGS_ALL; \
	SET_FLAG(BOOL_TO_ZERO_FLAG(REG_A == 0));

#define OP_XOR_REG(reg) \
	DO_XOR(reg); \
	return 1;

static int op_xor_a_b() {
	OP_XOR_REG(REG_B);
}

static int op_xor_a_c() {
	OP_XOR_REG(REG_C);
}

static int op_xor_a_d() {
	OP_XOR_REG(REG_D);
}

static int op_xor_a_e() {
	OP_XOR_REG(REG_E);
}

static int op_xor_a_h() {
	OP_XOR_REG(REG_H);
}

static int op_xor_a_l() {
	OP_XOR_REG(REG_L);
}

static int op_xor_a_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_XOR(load);
	return 2;
}

static int op_xor_a_n8() {
	uint8_t load = monitor_rd_mem(REG_PC);
	ADVANCE_PC(1);
	DO_XOR(load);
	return 2;
}

static int op_xor_a_a() {
	OP_XOR_REG(REG_A);
}

#define DO_OR(value) \
	REG_A |= value; \
	UNSET_FLAGS_ALL; \
	SET_FLAG(BOOL_TO_ZERO_FLAG(REG_A == 0));

#define OP_OR_REG(reg) \
	DO_OR(reg); \
	return 1;

static int op_or_a_b() {
	OP_OR_REG(REG_B);
}

static int op_or_a_c() {
	OP_OR_REG(REG_C);
}

static int op_or_a_d() {
	OP_OR_REG(REG_D);
}

static int op_or_a_e() {
	OP_OR_REG(REG_E);
}

static int op_or_a_h() {
	OP_OR_REG(REG_H);
}

static int op_or_a_l() {
	OP_OR_REG(REG_L);
}

static int op_or_a_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_OR(load);
	return 2;
}

static int op_or_a_n8() {
	uint8_t load = monitor_rd_mem(REG_PC);
	ADVANCE_PC(1);
	DO_OR(load);
	return 2;
}

static int op_or_a_a() {
	OP_OR_REG(REG_A);
}

#define DO_CP(value) \
	UNSET_FLAGS_ALL; \
	SET_FLAG(BOOL_TO_ZERO_FLAG(IS_CP_ZERO(REG_A, value))); \
	SET_FLAG(BOOL_TO_CARRY_FLAG(IS_CP_CARRY(REG_A, value))); \
	SET_FLAG(BOOL_TO_HALF_CARRY_FLAG(IS_CP_HALF_CARRY(REG_A, value))); \
	SET_FLAG(FLAG_SUB);

#define OP_CP_REG(reg) \
	DO_CP(reg) \
	return 1;

static int op_cp_a_b() {
	OP_CP_REG(REG_B);
}

static int op_cp_a_c() {
	OP_CP_REG(REG_C);
}

static int op_cp_a_d() {
	OP_CP_REG(REG_D);
}

static int op_cp_a_e() {
	OP_CP_REG(REG_E);
}

static int op_cp_a_h() {
	OP_CP_REG(REG_H);
}

static int op_cp_a_l() {
	OP_CP_REG(REG_L);
}

static int op_cp_a_ind_hl() {
	uint8_t load = monitor_rd_mem(REG_HL);
	DO_CP(load);
	return 2;
}

static int op_cp_a_a() {
	OP_CP_REG(REG_A);
}

static int op_cp_a_n8() {
	uint8_t load = monitor_rd_mem(REG_PC);
	ADVANCE_PC(1);
	DO_CP(load);
	return 2;
}

// Control flow instructions

#define DO_JMP(addr) \
	REG_PC = addr; \
	return 4;

static int op_jmp_a16() {
	uint16_t addr = RD_WORD(REG_PC);
	DO_JMP(addr);
}

#define OP_JMP_COND(cond) \
	if (cond) { \
		uint16_t addr = RD_WORD(REG_PC); \
		DO_JMP(addr); \
	} \
	ADVANCE_PC(2); \
	return 3;

static int op_jmp_nz_a16() {
	OP_JMP_COND(!(REG_F & FLAG_ZERO));
}

static int op_jmp_z_a16() {
	OP_JMP_COND(REG_F & FLAG_ZERO);
}

static int op_jmp_nc_a16() {
	OP_JMP_COND(!(REG_F & FLAG_CARRY));
}

static int op_jmp_c_a16() {
	OP_JMP_COND(REG_F & FLAG_CARRY);
}

static int op_jmp_hl() {
	REG_PC = REG_HL;
	return 1;
}

#define DO_JR(off) \
	ADVANCE_PC(1); \
	REG_PC += off; \
	return 3;

static int op_jr_e8() {
	int8_t off = monitor_rd_mem(REG_PC);
	DO_JR(off);
}

#define OP_JR_COND(cond) \
	if (cond) { \
		int8_t off = monitor_rd_mem(REG_PC); \
		DO_JR(off); \
	} \
	ADVANCE_PC(1); \
	return 2;

static int op_jr_nz_e8() {
	OP_JR_COND(!(REG_F & FLAG_ZERO));
}

static int op_jr_z_e8() {
	OP_JR_COND(REG_F & FLAG_ZERO);
}

static int op_jr_nc_e8() {
	OP_JR_COND(!(REG_F & FLAG_CARRY));
}

static int op_jr_c_e8() {
	OP_JR_COND(REG_F & FLAG_CARRY);
}

#define DO_CALL() \
	uint16_t addr = RD_WORD(REG_PC); \
	REG_PC +=2; \
	REG_SP -= 2; \
	WR_WORD(REG_SP, REG_PC); \
	REG_PC = addr; \
	return 6;

static int op_call_a16() {
	DO_CALL();
}

#define OP_CALL_COND(cond) \
	if (cond) { \
		DO_CALL(); \
	} \
	ADVANCE_PC(2); \
	return 3;

static int op_call_nz_a16() {
	OP_CALL_COND(!(REG_F & FLAG_ZERO));
}

static int op_call_z_a16() {
	OP_CALL_COND(REG_F & FLAG_ZERO);
}

static int op_call_nc_a16() {
	OP_CALL_COND(!(REG_F & FLAG_CARRY));
}

static int op_call_c_a16() {
	OP_CALL_COND(REG_F & FLAG_CARRY);
}

#define DO_RET() \
	uint16_t addr = RD_WORD(REG_SP); \
	REG_PC = addr; \
	REG_SP += 2;

static int op_ret() {
	DO_RET();
	return 4;
}

#define OP_RET_COND(cond) \
	if (cond) { \
		DO_RET(); \
		return 5; \
	} \
	return 2;

static int op_ret_nz() {
	OP_RET_COND(!(REG_F & FLAG_ZERO));
}

static int op_ret_z() {
	OP_RET_COND(REG_F & FLAG_ZERO);
}

static int op_ret_nc() {
	OP_RET_COND(!(REG_F & FLAG_CARRY));
}

static int op_ret_c() {
	OP_RET_COND(REG_F & FLAG_CARRY);
}

static int op_reti() {
	DO_RET();
	cpu_enable_intr();
	return 4;
}

static int op_daa() {
	uint16_t acc = REG_A;
	uint16_t tmp = REG_A;

	if (REG_F & FLAG_SUB) {
		if (REG_F & FLAG_HCARRY) {
			acc -= 6;
			acc &= 0xff;
		}
		if (REG_F & FLAG_CARRY) {
			acc -= 60;
		}
	}
	else {
		if (!(REG_F & FLAG_HCARRY)) {
			tmp &= 0xf;
			if (tmp > 9) {
				acc += 6;
			}
		}
		else
			acc += 6;
		if (!(REG_F & FLAG_CARRY)) {
			if (acc > 0x9f)
				acc += 0x60;
		}
		else
			acc += 0x60;
	}

	REG_F &= ~FLAG_HCARRY;
	REG_A = acc & 0xff;
	acc &= 0x100;
	if (acc == 0x100)
		REG_F |= FLAG_CARRY;
	//else
	//  regs_sets.regs[AF].UByte[F] &= ~F_CARRY;
	if (REG_A == 0)
		REG_F |= FLAG_ZERO;
	else
		REG_F &= ~FLAG_ZERO;

	return 1;
}

static int op_cpl() {
	REG_A = ~REG_A;
	SET_FLAG(FLAG_SUB | FLAG_HCARRY);
	return 1;
}

static int op_scf() {
	SET_FLAG(FLAG_CARRY);
	UNSET_FLAG(FLAG_SUB | FLAG_HCARRY);
	return 1;
}

static int op_ccf() {
	bool carry = !(REG_F & FLAG_CARRY);
	REG_F &= ~FLAG_CARRY;
	REG_F |= BOOL_TO_CARRY_FLAG(carry);
	UNSET_FLAG(FLAG_SUB | FLAG_HCARRY);
	return 1;
}

static int op_halt() {
	cpu_halt();
	return 1;
}

#define OP_POP_REG(reg) \
	(reg) = RD_WORD(REG_SP); \
	REG_SP += 2; \
	return 3;

static int op_pop_bc() {
	OP_POP_REG(REG_BC);
}

static int op_pop_de() {
	OP_POP_REG(REG_DE);
}

static int op_pop_hl() {
	OP_POP_REG(REG_HL);
}

static int op_pop_af() {
	REG_AF = RD_WORD(REG_SP); \
	REG_SP += 2; \
	REG_F &= 0xf0;
	return 3;
}

#define OP_PUSH_REG(reg) \
	REG_SP -= 2; \
	WR_WORD(REG_SP, reg); \
	return 3;

static int op_push_bc() {
	OP_PUSH_REG(REG_BC);
}

static int op_push_de() {
	OP_PUSH_REG(REG_DE);
}

static int op_push_hl() {
	OP_PUSH_REG(REG_HL);
}

static int op_push_af() {
	OP_PUSH_REG(REG_AF);
}

#define OP_RESTART(addr) \
	REG_SP -= 2; \
	WR_WORD(REG_SP, REG_PC); \
	REG_PC = addr; \
	return 4;

static int op_restart_0h() {
	OP_RESTART(0x0);
}

static int op_restart_8h() {
	OP_RESTART(0x8);
}

static int op_restart_10h() {
	OP_RESTART(0x10);
}

static int op_restart_18h() {
	OP_RESTART(0x18);
}

static int op_restart_20h() {
	OP_RESTART(0x20);
}

static int op_restart_28h() {
	OP_RESTART(0x28);
}

static int op_restart_30h() {
	OP_RESTART(0x30);
}

static int op_restart_38h() {
	OP_RESTART(0x38);
}

static int op_prefix() {
	return 1;
}

static int op_inval() {
	return 1;
}

static int op_dis_ints() {
	cpu_disable_intr();
	return 1;
}

static int op_ena_ints() {
	cpu_enable_intr();
	return 1;
}

int (*table_ops[])() =
{
	op_nop,
	op_ld_bc_n16,
	op_ld_ind_bc_a,
	op_inc_bc,
	op_inc_b,
	op_dec_b,
	op_ld_b_n8,
	op_rlca,
	op_ld_ind_a16_sp,
	op_add_hl_bc,
	op_ld_a_ind_bc,
	op_dec_bc,
	op_inc_c,
	op_dec_c,
	op_ld_c_n8,
	op_rrca,

	op_stop_n8,
	op_ld_de_n16,
	op_ld_ind_de_a,
	op_inc_de,
	op_inc_d,
	op_dec_d,
	op_ld_d_n8,
	op_rla,
	op_jr_e8,
	op_add_hl_de,
	op_ld_a_ind_de,
	op_dec_de,
	op_inc_e,
	op_dec_e,
	op_ld_e_n8,
	op_rra,

	op_jr_nz_e8,
	op_ld_hl_n16,
	op_ldi_ind_hl_a,
	op_inc_hl,
	op_inc_h,
	op_dec_h,
	op_ld_h_n8,
	op_daa,
	op_jr_z_e8,
	op_add_hl_hl,
	op_ldi_a_ind_hl,
	op_dec_hl,
	op_inc_l,
	op_dec_l,
	op_ld_l_n8,
	op_cpl,

	op_jr_nc_e8,
	op_ld_sp_n16,
	op_ldd_ind_hl_a,
	op_inc_sp,
	op_inc_ind_hl,
	op_dec_ind_hl,
	op_ld_ind_hl_n8,
	op_scf,
	op_jr_c_e8,
	op_add_hl_sp,
	op_ldd_a_ind_hl,
	op_dec_sp,
	op_inc_a,
	op_dec_a,
	op_ld_a_n8,
	op_ccf,

	op_ld_b_b,
	op_ld_b_c,
	op_ld_b_d,
	op_ld_b_e,
	op_ld_b_h,
	op_ld_b_l,
	op_ld_b_ind_hl,
	op_ld_b_a,
	op_ld_c_b,
	op_ld_c_c,
	op_ld_c_d,
	op_ld_c_e,
	op_ld_c_h,
	op_ld_c_l,
	op_ld_c_ind_hl,
	op_ld_c_a,

	op_ld_d_b,
	op_ld_d_c,
	op_ld_d_d,
	op_ld_d_e,
	op_ld_d_h,
	op_ld_d_l,
	op_ld_d_ind_hl,
	op_ld_d_a,
	op_ld_e_b,
	op_ld_e_c,
	op_ld_e_d,
	op_ld_e_e,
	op_ld_e_h,
	op_ld_e_l,
	op_ld_e_ind_hl,
	op_ld_e_a,

	op_ld_h_b,
	op_ld_h_c,
	op_ld_h_d,
	op_ld_h_e,
	op_ld_h_h,
	op_ld_h_l,
	op_ld_h_ind_hl,
	op_ld_h_a,
	op_ld_l_b,
	op_ld_l_c,
	op_ld_l_d,
	op_ld_l_e,
	op_ld_l_h,
	op_ld_l_l,
	op_ld_l_ind_hl,
	op_ld_l_a,

	op_ld_ind_hl_b,
	op_ld_ind_hl_c,
	op_ld_ind_hl_d,
	op_ld_ind_hl_e,
	op_ld_ind_hl_h,
	op_ld_ind_hl_l,
	op_halt,
	op_ld_ind_hl_a,
	op_ld_a_b,
	op_ld_a_c,
	op_ld_a_d,
	op_ld_a_e,
	op_ld_a_h,
	op_ld_a_l,
	op_ld_a_ind_hl,
	op_ld_a_a,

	op_add_a_b,
	op_add_a_c,
	op_add_a_d,
	op_add_a_e,
	op_add_a_h,
	op_add_a_l,
	op_add_a_ind_hl,
	op_add_a_a,
	op_adc_a_b,
	op_adc_a_c,
	op_adc_a_d,
	op_adc_a_e,
	op_adc_a_h,
	op_adc_a_l,
	op_adc_a_ind_hl,
	op_adc_a_a,

	op_sub_a_b,
	op_sub_a_c,
	op_sub_a_d,
	op_sub_a_e,
	op_sub_a_h,
	op_sub_a_l,
	op_sub_a_ind_hl,
	op_sub_a_a,
	op_sbc_a_b,
	op_sbc_a_c,
	op_sbc_a_d,
	op_sbc_a_e,
	op_sbc_a_h,
	op_sbc_a_l,
	op_sbc_a_ind_hl,
	op_sbc_a_a,

	op_and_a_b,
	op_and_a_c,
	op_and_a_d,
	op_and_a_e,
	op_and_a_h,
	op_and_a_l,
	op_and_a_ind_hl,
	op_and_a_a,
	op_xor_a_b,
	op_xor_a_c,
	op_xor_a_d,
	op_xor_a_e,
	op_xor_a_h,
	op_xor_a_l,
	op_xor_a_ind_hl,
	op_xor_a_a,

	op_or_a_b,
	op_or_a_c,
	op_or_a_d,
	op_or_a_e,
	op_or_a_h,
	op_or_a_l,
	op_or_a_ind_hl,
	op_or_a_a,
	op_cp_a_b,
	op_cp_a_c,
	op_cp_a_d,
	op_cp_a_e,
	op_cp_a_h,
	op_cp_a_l,
	op_cp_a_ind_hl,
	op_cp_a_a,

	op_ret_nz,
	op_pop_bc,
	op_jmp_nz_a16,
	op_jmp_a16,
	op_call_nz_a16,
	op_push_bc,
	op_add_a_n8,
	op_restart_0h,
	op_ret_z,
	op_ret,
	op_jmp_z_a16,
	op_prefix,
	op_call_z_a16,
	op_call_a16,
	op_adc_a_n8,
	op_restart_8h,

	op_ret_nc,
	op_pop_de,
	op_jmp_nc_a16,
	op_inval,
	op_call_nc_a16,
	op_push_de,
	op_sub_a_n8,
	op_restart_10h,
	op_ret_c,
	op_reti,
	op_jmp_c_a16,
	op_inval,
	op_call_c_a16,
	op_inval,
	op_sbc_a_n8,
	op_restart_18h,

	op_ldh_ind_a8_a,
	op_pop_hl,
	op_ldh_ind_c_a,
	op_inval,
	op_inval,
	op_push_hl,
	op_and_a_n8,
	op_restart_20h,
	op_add_sp_e8,
	op_jmp_hl,
	op_ld_ind_a16_a,
	op_inval,
	op_inval,
	op_inval,
	op_xor_a_n8,
	op_restart_28h,

	op_ldh_a_ind_a8,
	op_pop_af,
	op_ldh_a_ind_c,
	op_dis_ints,
	op_inval,
	op_push_af,
	op_or_a_n8,
	op_restart_30h,
	op_ld_hl_sp_e8,
	op_ld_sp_hl,
	op_ld_a_ind_a16,
	op_ena_ints,
	op_inval,
	op_inval,
	op_cp_a_n8,
	op_restart_38h,

	op_rlc_b,
	op_rlc_c,
	op_rlc_d,
	op_rlc_e,
	op_rlc_h,
	op_rlc_l,
	op_rlc_ind_hl,
	op_rlc_a,
	op_rrc_b,
	op_rrc_c,
	op_rrc_d,
	op_rrc_e,
	op_rrc_h,
	op_rrc_l,
	op_rrc_ind_hl,
	op_rrc_a,

	op_rl_b,
	op_rl_c,
	op_rl_d,
	op_rl_e,
	op_rl_h,
	op_rl_l,
	op_rl_ind_hl,
	op_rl_a,
	op_rr_b,
	op_rr_c,
	op_rr_d,
	op_rr_e,
	op_rr_h,
	op_rr_l,
	op_rr_ind_hl,
	op_rr_a,

	op_sla_b,
	op_sla_c,
	op_sla_d,
	op_sla_e,
	op_sla_h,
	op_sla_l,
	op_sla_ind_hl,
	op_sla_a,
	op_sra_b,
	op_sra_c,
	op_sra_d,
	op_sra_e,
	op_sra_h,
	op_sra_l,
	op_sra_ind_hl,
	op_sra_a,

	op_swp_b,
	op_swp_c,
	op_swp_d,
	op_swp_e,
	op_swp_h,
	op_swp_l,
	op_swp_ind_hl,
	op_swp_a,
	op_srl_b,
	op_srl_c,
	op_srl_d,
	op_srl_e,
	op_srl_h,
	op_srl_l,
	op_srl_ind_hl,
	op_srl_a,

	op_bit0_b,
	op_bit0_c,
	op_bit0_d,
	op_bit0_e,
	op_bit0_h,
	op_bit0_l,
	op_bit0_ind_hl,
	op_bit0_a,
	op_bit1_b,
	op_bit1_c,
	op_bit1_d,
	op_bit1_e,
	op_bit1_h,
	op_bit1_l,
	op_bit1_ind_hl,
	op_bit1_a,

	op_bit2_b,
	op_bit2_c,
	op_bit2_d,
	op_bit2_e,
	op_bit2_h,
	op_bit2_l,
	op_bit2_ind_hl,
	op_bit2_a,
	op_bit3_b,
	op_bit3_c,
	op_bit3_d,
	op_bit3_e,
	op_bit3_h,
	op_bit3_l,
	op_bit3_ind_hl,
	op_bit3_a,

	op_bit4_b,
	op_bit4_c,
	op_bit4_d,
	op_bit4_e,
	op_bit4_h,
	op_bit4_l,
	op_bit4_ind_hl,
	op_bit4_a,
	op_bit5_b,
	op_bit5_c,
	op_bit5_d,
	op_bit5_e,
	op_bit5_h,
	op_bit5_l,
	op_bit5_ind_hl,
	op_bit5_a,

	op_bit6_b,
	op_bit6_c,
	op_bit6_d,
	op_bit6_e,
	op_bit6_h,
	op_bit6_l,
	op_bit6_ind_hl,
	op_bit6_a,
	op_bit7_b,
	op_bit7_c,
	op_bit7_d,
	op_bit7_e,
	op_bit7_h,
	op_bit7_l,
	op_bit7_ind_hl,
	op_bit7_a,

	op_rst0_b,
	op_rst0_c,
	op_rst0_d,
	op_rst0_e,
	op_rst0_h,
	op_rst0_l,
	op_rst0_ind_hl,
	op_rst0_a,
	op_rst1_b,
	op_rst1_c,
	op_rst1_d,
	op_rst1_e,
	op_rst1_h,
	op_rst1_l,
	op_rst1_ind_hl,
	op_rst1_a,

	op_rst2_b,
	op_rst2_c,
	op_rst2_d,
	op_rst2_e,
	op_rst2_h,
	op_rst2_l,
	op_rst2_ind_hl,
	op_rst2_a,
	op_rst3_b,
	op_rst3_c,
	op_rst3_d,
	op_rst3_e,
	op_rst3_h,
	op_rst3_l,
	op_rst3_ind_hl,
	op_rst3_a,

	op_rst4_b,
	op_rst4_c,
	op_rst4_d,
	op_rst4_e,
	op_rst4_h,
	op_rst4_l,
	op_rst4_ind_hl,
	op_rst4_a,
	op_rst5_b,
	op_rst5_c,
	op_rst5_d,
	op_rst5_e,
	op_rst5_h,
	op_rst5_l,
	op_rst5_ind_hl,
	op_rst5_a,

	op_rst6_b,
	op_rst6_c,
	op_rst6_d,
	op_rst6_e,
	op_rst6_h,
	op_rst6_l,
	op_rst6_ind_hl,
	op_rst6_a,
	op_rst7_b,
	op_rst7_c,
	op_rst7_d,
	op_rst7_e,
	op_rst7_h,
	op_rst7_l,
	op_rst7_ind_hl,
	op_rst7_a,

	op_set0_b,
	op_set0_c,
	op_set0_d,
	op_set0_e,
	op_set0_h,
	op_set0_l,
	op_set0_ind_hl,
	op_set0_a,
	op_set1_b,
	op_set1_c,
	op_set1_d,
	op_set1_e,
	op_set1_h,
	op_set1_l,
	op_set1_ind_hl,
	op_set1_a,

	op_set2_b,
	op_set2_c,
	op_set2_d,
	op_set2_e,
	op_set2_h,
	op_set2_l,
	op_set2_ind_hl,
	op_set2_a,
	op_set3_b,
	op_set3_c,
	op_set3_d,
	op_set3_e,
	op_set3_h,
	op_set3_l,
	op_set3_ind_hl,
	op_set3_a,

	op_set4_b,
	op_set4_c,
	op_set4_d,
	op_set4_e,
	op_set4_h,
	op_set4_l,
	op_set4_ind_hl,
	op_set4_a,
	op_set5_b,
	op_set5_c,
	op_set5_d,
	op_set5_e,
	op_set5_h,
	op_set5_l,
	op_set5_ind_hl,
	op_set5_a,

	op_set6_b,
	op_set6_c,
	op_set6_d,
	op_set6_e,
	op_set6_h,
	op_set6_l,
	op_set6_ind_hl,
	op_set6_a,
	op_set7_b,
	op_set7_c,
	op_set7_d,
	op_set7_e,
	op_set7_h,
	op_set7_l,
	op_set7_ind_hl,
	op_set7_a,
};

int ops_table_dispatch(uint8_t op, bool prefix) {
	return (table_ops[op|(prefix<<8)])();
}
