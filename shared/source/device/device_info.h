/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/stackvec.h"

#include <cstdint>
#include <string>

namespace NEO {

struct DeviceInfo {
    StackVec<size_t, 3> maxSubGroups;
    double profilingTimerResolution;
    uint64_t globalMemSize;
    uint64_t localMemSize;
    uint64_t maxMemAllocSize;
    const char *ilVersion;
    size_t image2DMaxHeight;
    size_t image2DMaxWidth;
    size_t image3DMaxDepth;
    size_t imageMaxArraySize;
    size_t imageMaxBufferSize;
    size_t maxNumEUsPerSubSlice;
    size_t maxNumEUsPerDualSubSlice;
    size_t maxParameterSize;
    size_t maxWorkGroupSize;
    size_t maxWorkItemSizes[3];
    size_t outProfilingTimerResolution;
    size_t outProfilingTimerClock;
    size_t printfBufferSize;
    uint32_t addressBits;
    uint32_t computeUnitsUsedForScratch;
    uint32_t errorCorrectionSupport;
    uint32_t globalMemCachelineSize;
    uint32_t imageSupport;
    uint32_t maxClockFrequency;
    uint32_t maxFrontEndThreads;
    uint32_t maxReadImageArgs;
    uint32_t maxSamplers;
    uint32_t maxWriteImageArgs;
    uint32_t numThreadsPerEU;
    StackVec<uint32_t, 6> threadsPerEUConfigs;
    uint32_t vendorId;
    uint32_t vmeAvcSupportsPreemption;
    bool force32BitAddresses;
    std::string name;
};

} // namespace NEO
