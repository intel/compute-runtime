/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpuintrinsics.h"

#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/helpers/ptr_math.h"

#include <atomic>
#include <cstdint>
#include <functional>

namespace CpuIntrinsicsTests {
// std::atomic is used for sake of sanitation in MT tests
std::atomic<uintptr_t> lastClFlushedPtr(0u);
std::atomic<uint32_t> clFlushCounter(0u);
std::atomic<uint32_t> pauseCounter(0u);
std::atomic<uint32_t> sfenceCounter(0u);
std::atomic<uint32_t> mfenceCounter(0u);

std::atomic<uint64_t> lastUmwaitCounter(0u);
std::atomic<unsigned int> lastUmwaitControl(0u);
std::atomic<uint32_t> umwaitCounter(0u);

std::atomic<uintptr_t> lastUmonitorPtr(0u);
std::atomic<uint32_t> umonitorCounter(0u);

std::atomic<uint32_t> rdtscCounter(0u);

std::atomic_uint32_t tpauseCounter{};

volatile TagAddressType *pauseAddress = nullptr;
TaskCountType pauseValue = 0u;
uint32_t pauseOffset = 0u;
uint64_t rdtscRetValue = 0;
unsigned char umwaitRetValue = 0;

std::function<void()> setupPauseAddress;
std::function<void()> controlUmwait;
} // namespace CpuIntrinsicsTests

namespace NEO {
namespace CpuIntrinsics {

void clFlush(void const *ptr) {
    CpuIntrinsicsTests::clFlushCounter++;
    CpuIntrinsicsTests::lastClFlushedPtr = reinterpret_cast<uintptr_t>(ptr);
}

void clFlushOpt(void *ptr) {
    clFlush(ptr);
}

void sfence() {
    CpuIntrinsicsTests::sfenceCounter++;
}

void mfence() {
    CpuIntrinsicsTests::mfenceCounter++;
}

void pause() {
    CpuIntrinsicsTests::pauseCounter++;
    if (CpuIntrinsicsTests::pauseAddress != nullptr) {
        *CpuIntrinsicsTests::pauseAddress = CpuIntrinsicsTests::pauseValue;
        if (CpuIntrinsicsTests::setupPauseAddress) {
            CpuIntrinsicsTests::setupPauseAddress();
        } else {
            CpuIntrinsicsTests::pauseAddress = ptrOffset(CpuIntrinsicsTests::pauseAddress, CpuIntrinsicsTests::pauseOffset);
        }
    }
}

uint8_t tpause(uint32_t control, uint64_t counter) {
    CpuIntrinsicsTests::tpauseCounter++;
    return 0;
}

unsigned char umwait(unsigned int ctrl, uint64_t counter) {
    CpuIntrinsicsTests::lastUmwaitControl = ctrl;
    CpuIntrinsicsTests::lastUmwaitCounter = counter;
    CpuIntrinsicsTests::umwaitCounter++;
    if (CpuIntrinsicsTests::controlUmwait) {
        CpuIntrinsicsTests::controlUmwait();
        return CpuIntrinsicsTests::umwaitRetValue;
    } else {
        return CpuIntrinsicsTests::umwaitRetValue;
    }
}

void umonitor(void *a) {
    CpuIntrinsicsTests::lastUmonitorPtr = reinterpret_cast<uintptr_t>(a);
    CpuIntrinsicsTests::umonitorCounter++;
}

uint64_t rdtsc() {
    CpuIntrinsicsTests::rdtscCounter++;
    return CpuIntrinsicsTests::rdtscRetValue;
}

} // namespace CpuIntrinsics
} // namespace NEO
