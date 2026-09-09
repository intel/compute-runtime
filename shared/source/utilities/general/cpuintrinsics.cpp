/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpuintrinsics.h"
#ifdef WIN32
#include <windows.h>
#else
#include <sched.h>
#endif

namespace NEO {
namespace CpuIntrinsics {

void clFlush(void const *ptr) {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

void clFlushOpt(void *ptr) {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

void sfence() {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

void mfence() {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

void pause() {
#ifdef WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
}

uint8_t tpause(uint32_t control, uint64_t counter) {
    return 0;
}

unsigned char umwait(unsigned int ctrl, uint64_t counter) {

    return 0;
}

void umonitor(void *a) {

}

uint64_t rdtsc() {
    return 0;
}

} // namespace CpuIntrinsics
} // namespace NEO
