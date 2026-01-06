/*
 * Copyright (C) 2020-2025 Intel Corporation
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

uint8_t tpause(uint32_t control, uint64_t counter);

unsigned char umwait(unsigned int ctrl, uint64_t counter);

void umonitor(void *a);

uint64_t rdtsc();

} // namespace CpuIntrinsics
} // namespace NEO
