/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpuintrinsics.h"

#if defined(_WIN32)
#include <immintrin.h>
#include <intrin.h>
#pragma intrinsic(__rdtsc)
#elif defined(__riscv)
#else
#if defined(__ARM_ARCH)
extern "C" uint64_t __rdtsc();
#else
#include <immintrin.h>
#include <x86intrin.h>
#endif
#endif

#if defined(__ARM_ARCH)
#include <sse2neon.h>
#elif defined(__riscv)
#else
#include <emmintrin.h>
#endif

namespace NEO {
namespace CpuIntrinsics {

void clFlush(void const *ptr) {
#if defined(__riscv)
    return;
#else
    _mm_clflush(ptr);
#endif
}

void clFlushOpt(void *ptr) {
#ifdef SUPPORTS_CLFLUSHOPT
    _mm_clflushopt(ptr);
#elif defined(__riscv)
    return;
#else
    _mm_clflush(ptr);
#endif
}

void sfence() {
#if defined(__riscv)
// According to:
//
// https://blog.jiejiss.com/Rust-is-incompatible-with-LLVM-at-least-partially/
// https://github.com/riscv/riscv-isa-manual/issues/43
// https://stackoverflow.com/questions/68537854/pause-instruction-unrecognized-opcode-pause-in-risc-v
//
// gcc/clang assembler will not currently (2022) accept `fence w,unknown`;
// `hand-rolling` the instruction (see below) does the job.
//
    __asm__ __volatile__(".insn i 0x0F, 0, x0, x0, 0x010");
#else
    _mm_sfence();
#endif
}

void pause() {
#if defined(__riscv)
    __asm__ volatile("fence"::);
#else
    _mm_pause();
#endif
}

uint8_t tpause(uint32_t control, uint64_t counter) {
#ifdef SUPPORTS_WAITPKG
    return _tpause(control, counter);
#else
    return 0;
#endif
}

unsigned char umwait(unsigned int ctrl, uint64_t counter) {
#ifdef SUPPORTS_WAITPKG
    return _umwait(ctrl, counter);
#else
    return 0;
#endif
}

void umonitor(void *a) {
#ifdef SUPPORTS_WAITPKG
    _umonitor(a);
#endif
}

uint64_t rdtsc() {
#if defined(__riscv)
    std::uint64_t val = 0;
    __asm__ __volatile__(
       "rdtime %0;\n"
       : "=r"(val)
       :: );
    return val;
#else
    return __rdtsc();
#endif
}

} // namespace CpuIntrinsics
} // namespace NEO
