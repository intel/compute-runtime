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
constexpr uint32_t ALU_REGISTER_R_SRCA = 0x20;
constexpr uint32_t ALU_REGISTER_R_SRCB = 0x21;
constexpr uint32_t ALU_REGISTER_R_ACCU = 0x31;

constexpr uint32_t GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW = 0x23A8;