/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/utilities/cpu_info.h"

#include <cpuid.h>

namespace OCLRT {

void cpuidex_linux_wrapper(int *cpuInfo, int functionId, int subfunctionId) {
    __cpuid_count(functionId, subfunctionId, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
}

void (*CpuInfo::cpuidexFunc)(int *, int, int) = cpuidex_linux_wrapper;

const CpuInfo CpuInfo::instance;

void CpuInfo::cpuid(
    uint32_t cpuInfo[4],
    uint32_t functionId) const {
    __cpuid_count(functionId, 0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
}

void CpuInfo::cpuidex(
    uint32_t cpuInfo[4],
    uint32_t functionId,
    uint32_t subfunctionId) const {
    cpuidexFunc(reinterpret_cast<int *>(cpuInfo), functionId, subfunctionId);
}

} // namespace OCLRT
