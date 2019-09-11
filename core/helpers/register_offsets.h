/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <stdint.h>

constexpr uint32_t L3SQC_BIT_LQSC_RO_PERF_DIS = 0x08000000;
constexpr uint32_t L3SQC_REG4 = 0xB118;

constexpr uint32_t GPGPU_WALKER_COOKIE_VALUE_BEFORE_WALKER = 0xFFFFFFFF;
constexpr uint32_t GPGPU_WALKER_COOKIE_VALUE_AFTER_WALKER = 0x00000000;

//Threads Dimension X/Y/Z
constexpr uint32_t GPUGPU_DISPATCHDIMX = 0x2500;
constexpr uint32_t GPUGPU_DISPATCHDIMY = 0x2504;
constexpr uint32_t GPUGPU_DISPATCHDIMZ = 0x2508;

constexpr uint32_t CS_GPR_R0 = 0x2600;
constexpr uint32_t CS_GPR_R1 = 0x2608;
constexpr uint32_t CS_GPR_R2 = 0x2610;
constexpr uint32_t CS_GPR_R3 = 0x2618;
constexpr uint32_t CS_GPR_R4 = 0x2620;
constexpr uint32_t CS_GPR_R5 = 0x2628;
constexpr uint32_t CS_GPR_R6 = 0x2630;
constexpr uint32_t CS_GPR_R7 = 0x2638;
constexpr uint32_t CS_GPR_R8 = 0x2640;
constexpr uint32_t CS_GPR_R9 = 0x2648;
constexpr uint32_t CS_GPR_R10 = 0x2650;
constexpr uint32_t CS_GPR_R11 = 0x2658;
constexpr uint32_t CS_GPR_R12 = 0x2660;
constexpr uint32_t CS_GPR_R13 = 0x2668;
constexpr uint32_t CS_GPR_R14 = 0x2670;
constexpr uint32_t CS_GPR_R15 = 0x2678;

constexpr uint32_t CS_PREDICATE_RESULT = 0x2418;

//Alu opcodes
constexpr uint32_t NUM_ALU_INST_FOR_READ_MODIFY_WRITE = 4;

constexpr uint32_t ALU_OPCODE_LOAD = 0x080;
constexpr uint32_t ALU_OPCODE_STORE = 0x180;
constexpr uint32_t ALU_OPCODE_ADD = 0x100;
constexpr uint32_t ALU_OPCODE_SUB = 0x101;
constexpr uint32_t ALU_OPCODE_AND = 0x102;
constexpr uint32_t ALU_OPCODE_OR = 0x103;

constexpr uint32_t ALU_REGISTER_R_0 = 0x0;
constexpr uint32_t ALU_REGISTER_R_1 = 0x1;
constexpr uint32_t ALU_REGISTER_R_2 = 0x2;
constexpr uint32_t ALU_REGISTER_R_3 = 0x3;
constexpr uint32_t ALU_REGISTER_R_4 = 0x4;
constexpr uint32_t ALU_REGISTER_R_5 = 0x5;
constexpr uint32_t ALU_REGISTER_R_6 = 0x6;
constexpr uint32_t ALU_REGISTER_R_7 = 0x7;
constexpr uint32_t ALU_REGISTER_R_8 = 0x8;
constexpr uint32_t ALU_REGISTER_R_9 = 0x9;
constexpr uint32_t ALU_REGISTER_R_10 = 0xA;
constexpr uint32_t ALU_REGISTER_R_11 = 0xB;
constexpr uint32_t ALU_REGISTER_R_12 = 0xC;
constexpr uint32_t ALU_REGISTER_R_13 = 0xD;
constexpr uint32_t ALU_REGISTER_R_14 = 0xE;
constexpr uint32_t ALU_REGISTER_R_15 = 0xF;

constexpr uint32_t ALU_REGISTER_R_SRCA = 0x20;
constexpr uint32_t ALU_REGISTER_R_SRCB = 0x21;
constexpr uint32_t ALU_REGISTER_R_ACCU = 0x31;
constexpr uint32_t ALU_REGISTER_R_ZF = 0x32;
constexpr uint32_t ALU_REGISTER_R_CF = 0x33;

constexpr uint32_t GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW = 0x23A8;
