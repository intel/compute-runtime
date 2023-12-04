/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <stdint.h>

namespace RegisterConstants {
inline constexpr uint32_t l3SqcBitLqscR0PerfDis = 0x08000000;

inline constexpr uint32_t gpgpuWalkerCookieValueBeforeWalker = 0xFFFFFFFF;
inline constexpr uint32_t gpgpuWalkerCookieValueAfterWalker = 0x00000000;
inline constexpr uint32_t numAluInstForReadModifyWrite = 4;
} // namespace RegisterConstants
namespace RegisterOffsets {
inline constexpr uint32_t l3sqcReg4 = 0xB118;

// Threads Dimension X/Y/Z
inline constexpr uint32_t gpgpuDispatchDimX = 0x2500;
inline constexpr uint32_t gpgpuDispatchDimY = 0x2504;
inline constexpr uint32_t gpgpuDispatchDimZ = 0x2508;

inline constexpr uint32_t gpgpuDispatchDim[3] = {gpgpuDispatchDimX, gpgpuDispatchDimY, gpgpuDispatchDimZ};

inline constexpr uint32_t csGprR0 = 0x2600;
inline constexpr uint32_t csGprR1 = 0x2608;
inline constexpr uint32_t csGprR2 = 0x2610;
inline constexpr uint32_t csGprR3 = 0x2618;
inline constexpr uint32_t csGprR4 = 0x2620;
inline constexpr uint32_t csGprR5 = 0x2628;
inline constexpr uint32_t csGprR6 = 0x2630;
inline constexpr uint32_t csGprR7 = 0x2638;
inline constexpr uint32_t csGprR8 = 0x2640;
inline constexpr uint32_t csGprR9 = 0x2648;
inline constexpr uint32_t csGprR10 = 0x2650;
inline constexpr uint32_t csGprR11 = 0x2658;
inline constexpr uint32_t csGprR12 = 0x2660;
inline constexpr uint32_t csGprR13 = 0x2668;
inline constexpr uint32_t csGprR14 = 0x2670;
inline constexpr uint32_t csGprR15 = 0x2678;

inline constexpr uint32_t csPredicateResult = 0x2418;
inline constexpr uint32_t csPredicateResult2 = 0x23BC;

inline constexpr uint32_t semaWaitPoll = 0x0224c;

inline constexpr uint32_t gpThreadTimeRegAddressOffsetLow = 0x23A8;

inline constexpr uint32_t globalTimestampLdw = 0x2358;
inline constexpr uint32_t globalTimestampUn = 0x235c;
} // namespace RegisterOffsets

// Alu opcodes
enum class AluRegisters : uint32_t {
    OPCODE_NONE = 0x000,
    OPCODE_FENCE_RD = 0x001,
    OPCODE_FENCE_WR = 0x002,

    OPCODE_LOAD = 0x080,
    OPCODE_LOAD0 = 0x081,
    OPCODE_LOADIND = 0x082,
    OPCODE_STORE = 0x180,
    OPCODE_ADD = 0x100,
    OPCODE_SUB = 0x101,
    OPCODE_AND = 0x102,
    OPCODE_OR = 0x103,

    OPCODE_SHL = 0x105,
    OPCODE_STOREIND = 0x181,

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
