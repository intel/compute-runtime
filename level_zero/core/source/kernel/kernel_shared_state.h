/*
 * Copyright (C) 2025-2026 Intel Corporation
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

    bool heaplessEnabled = false;
    bool implicitScalingEnabled = false;

    // High-water mark of how many bytes of the shared printf buffer have already been printed.
    // Only the region beyond it is emitted on a drain, so repeated drains (two channels, or
    // re-launches sharing the buffer) print each launch's output once. Reset when the buffer is rewound.
    uint32_t printfOutputAlreadyPrintedSize = 0;
};

} // namespace L0
