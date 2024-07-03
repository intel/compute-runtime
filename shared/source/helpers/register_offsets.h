/*
 * Copyright (C) 2019-2024 Intel Corporation
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
inline constexpr uint32_t bcs0Base = 0x20000;

inline constexpr uint32_t csPredicateResult = 0x2418;
inline constexpr uint32_t csPredicateResult2 = 0x23BC;

inline constexpr uint32_t semaWaitPoll = 0x0224c;

inline constexpr uint32_t gpThreadTimeRegAddressOffsetLow = 0x23A8;
inline constexpr uint32_t gpThreadTimeRegAddressOffsetHigh = 0x23AC;

inline constexpr uint32_t globalTimestampLdw = 0x2358;
inline constexpr uint32_t globalTimestampUn = 0x235c;
} // namespace RegisterOffsets

namespace DebuggerRegisterOffsets {
inline constexpr uint32_t csGprR15 = 0x2678;
} // namespace DebuggerRegisterOffsets

// Alu opcodes
enum class AluRegisters : uint32_t {
    opcodeNone = 0x000,
    opcodeFenceRd = 0x001,
    opcodeFenceWr = 0x002,

    opcodeLoad = 0x080,
    opcodeLoad0 = 0x081,
    opcodeLoadind = 0x082,
    opcodeStore = 0x180,
    opcodeAdd = 0x100,
    opcodeSub = 0x101,
    opcodeAnd = 0x102,
    opcodeOr = 0x103,

    opcodeShl = 0x105,
    opcodeStoreind = 0x181,

    gpr0 = 0x0,
    gpr1 = 0x1,
    gpr2 = 0x2,
    gpr3 = 0x3,
    gpr4 = 0x4,
    gpr5 = 0x5,
    gpr6 = 0x6,
    gpr7 = 0x7,
    gpr8 = 0x8,
    gpr9 = 0x9,
    gpr10 = 0xA,
    gpr11 = 0xB,
    gpr12 = 0xC,
    gpr13 = 0xD,
    gpr14 = 0xE,

    srca = 0x20,
    srcb = 0x21,
    accu = 0x31,
    zf = 0x32,
    cf = 0x33
};

enum class DebuggerAluRegisters : uint32_t {
    gpr15 = 0xF
};
