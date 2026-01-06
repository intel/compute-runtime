/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace std {
class mutex;
}

namespace NEO {
class GraphicsAllocation;
}

namespace L0 {
struct KernelImmutableData;
struct Module;

struct KernelSharedState {
    KernelSharedState(Module *module);
    KernelSharedState(const KernelSharedState &) = delete;
    KernelSharedState(KernelSharedState &&) noexcept = default;
    KernelSharedState &operator=(const KernelSharedState &) = delete;
    KernelSharedState &operator=(KernelSharedState &&) noexcept = default;
    ~KernelSharedState();

    Module *module = nullptr;
    const KernelImmutableData *kernelImmData = nullptr;

    std::mutex *devicePrintfKernelMutex = nullptr;

    NEO::GraphicsAllocation *privateMemoryGraphicsAllocation = nullptr;
    NEO::GraphicsAllocation *printfBuffer = nullptr;

    uintptr_t surfaceStateAlignmentMask = 0;
    uintptr_t surfaceStateAlignment = 0;

    uint32_t implicitArgsVersion = 0;
    uint32_t walkerInlineDataSize = 0;

    uint32_t maxWgCountPerTileCcs = 0;
    uint32_t maxWgCountPerTileRcs = 0;
    uint32_t maxWgCountPerTileCooperative = 0;

    bool heaplessEnabled = false;
    bool implicitScalingEnabled = false;
    bool localDispatchSupport = false;
    bool rcsAvailable = false;
    bool cooperativeSupport = false;
};

} // namespace L0
