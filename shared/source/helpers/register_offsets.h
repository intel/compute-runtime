/*
 * Copyright (C) 2019-2020 Intel Corporation
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

enum class AluRegisters : uint32_t {
    OPCODE_LOAD = 0x080,
    OPCODE_STORE = 0x180,
    OPCODE_ADD = 0x100,
    OPCODE_SUB = 0x101,
    OPCODE_AND = 0x102,
    OPCODE_OR = 0x103,

    R_0 = 0x0,
    R_1 = 0x1,
    R_2 = 0x2,
    R_3 = 0x3,
    R_4 = 0x4,
    R_5 = 0x5,
    R_6 = 0x6,
    R_7 = 0x7,
    R_8 = 0x8,
    R_9 = 0x9,
    R_10 = 0xA,
    R_11 = 0xB,
    R_12 = 0xC,
    R_13 = 0xD,
    R_14 = 0xE,
    R_15 = 0xF,

    R_SRCA = 0x20,
    R_SRCB = 0x21,
    R_ACCU = 0x31,
    R_ZF = 0x32,
    R_CF = 0x33
};

constexpr uint32_t GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW = 0x23A8;
