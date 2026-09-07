/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {
namespace CpuIntrinsics {

void sfence();

void mfence();

void clFlush(void const *ptr);

void clFlushOpt(void *ptr);

void pause();

// Declared here so ULTs get the libult substitute of this file, which counts the call.
void yield();

uint8_t tpause(uint32_t control, uint64_t counter);

unsigned char umwait(unsigned int ctrl, uint64_t counter);

void umonitor(void *a);

uint64_t rdtsc();

} // namespace CpuIntrinsics
} // namespace NEO
