/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>
#include <igfxfmid.h>

namespace NEO {

struct KernelInfo;
struct RootDeviceEnvironment;

struct WorkSizeInfo {
    uint32_t maxWorkGroupSize;
    uint32_t minWorkGroupSize;
    bool hasBarriers;
    uint32_t simdSize;
    uint32_t slmTotalSize;
    GFXCORE_FAMILY coreFamily;
    uint32_t numThreadsPerSubSlice;
    uint32_t localMemSize;
    bool imgUsed = false;
    bool yTiledSurfaces = false;
    bool useRatio = false;
    bool useStrictRatio = false;
    float targetRatio = 0;
    uint32_t preferredWgCountPerSubSlice = 0;

    WorkSizeInfo(uint32_t maxWorkGroupSize, bool hasBarriers, uint32_t simdSize, uint32_t slmTotalSize, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t numThreadsPerSubSlice, uint32_t localMemSize, bool imgUsed, bool yTiledSurface, bool disableEUFusion);

    void setIfUseImg(const KernelInfo &kernelInfo);
    void setMinWorkGroupSize(const RootDeviceEnvironment &rootDeviceEnvironment, bool disableEUFusion);
    void checkRatio(const size_t workItems[3]);
    void setPreferredWgCountPerSubslice(uint32_t preferredWgCount);
};

} // namespace NEO