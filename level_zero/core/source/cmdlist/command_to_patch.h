/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace L0 {

struct PatchFrontEndState {
    void *pDestination = nullptr;
    void *pCommand = nullptr;
    uint64_t gpuAddress = 0;
};

struct PatchPauseOnEnqueueSemaphoreStart {
    void *pCommand = nullptr;
};

struct PatchPauseOnEnqueueSemaphoreEnd {
    void *pCommand = nullptr;
};

struct PatchPauseOnEnqueuePipeControlStart {
    void *pCommand = nullptr;
};

struct PatchPauseOnEnqueuePipeControlEnd {
    void *pCommand = nullptr;
};

struct PatchComputeWalkerInlineDataScratch {
    void *pDestination = nullptr;
    uint64_t baseAddress = 0;
    uint64_t gpuAddress = 0;
    mutable uint64_t scratchAddressAfterPatch = 0;
    size_t offset = 0;
    size_t patchSize = 0;
};

struct PatchComputeWalkerImplicitArgsScratch {
    void *pDestination = nullptr;
    uint64_t baseAddress = 0;
    uint64_t gpuAddress = 0;
    mutable uint64_t scratchAddressAfterPatch = 0;
    size_t offset = 0;
    size_t patchSize = 0;
};

struct PatchNoopSpace {
    void *pDestination = nullptr;
    uint64_t gpuAddress = 0;
    size_t offset = 0;
    size_t patchSize = 0;
};

struct PatchHostFunctionId {
    void *cmdBufferSpace = nullptr;
    uint64_t gpuAddress = 0;
    uint64_t callbackAddress = 0;
    uint64_t userDataAddress = 0;
    bool memorySynchronizationRequired = true;
};

struct PatchHostFunctionWait {
    void *cmdBufferSpace = nullptr;
    uint64_t gpuAddress = 0;
    uint32_t partitionId = 0;
};

struct PatchSignalEventPostSyncPipeControl {
    void *pDestination = nullptr;
};

struct PatchWaitEventSemaphoreWait {
    uint64_t gpuDestination = 0;
    void *pDestination = nullptr;
    void *commandView = nullptr;
    size_t offset = 0;
    size_t patchSize = 0;
};

struct PatchTimestampEventPostSyncStoreRegMem {
    void *pDestination = nullptr;
    size_t offset = 0;
};

struct PatchCbEventTimestampPostSyncSemaphoreWait {
    uint64_t gpuDestination = 0;
    void *pDestination = nullptr;
    void *commandView = nullptr;
    size_t offset = 0;
    size_t patchSize = 0;
};

struct PatchCbEventTimestampClearStoreDataImm {
    void *pDestination = nullptr;
    size_t offset = 0;
};

struct PatchCbWaitEventSemaphoreWait {
    uint64_t gpuDestination = 0;
    void *pDestination = nullptr;
    void *commandView = nullptr;
    size_t offset = 0;
    size_t patchSize = 0;
};

struct PatchCbWaitEventLoadRegisterImm {
    void *pDestination = nullptr;
    size_t offset = 0;
};

struct PatchPrefetchKernelMemory {
    void *pDestination = nullptr;
    size_t offset = 0;
    size_t patchSize = 0;
};

struct PatchInvalidPatchType {};

using CommandToPatchOnQueue = std::variant<
    PatchInvalidPatchType,
    PatchFrontEndState,
    PatchPauseOnEnqueueSemaphoreStart,
    PatchPauseOnEnqueueSemaphoreEnd,
    PatchPauseOnEnqueuePipeControlStart,
    PatchPauseOnEnqueuePipeControlEnd,
    PatchComputeWalkerInlineDataScratch,
    PatchComputeWalkerImplicitArgsScratch,
    PatchNoopSpace,
    PatchHostFunctionId,
    PatchHostFunctionWait>;

using CommandToPatchInCmdList = std::variant<
    PatchInvalidPatchType,
    PatchSignalEventPostSyncPipeControl,
    PatchWaitEventSemaphoreWait,
    PatchTimestampEventPostSyncStoreRegMem,
    PatchCbEventTimestampPostSyncSemaphoreWait,
    PatchCbEventTimestampClearStoreDataImm,
    PatchCbWaitEventSemaphoreWait,
    PatchCbWaitEventLoadRegisterImm,
    PatchPrefetchKernelMemory>;

struct CommandToPatchContainer {
    using VectorType = std::vector<CommandToPatchInCmdList>;
    using iterator = VectorType::iterator;                 // NOLINT(readability-identifier-naming)
    using reverse_iterator = VectorType::reverse_iterator; // NOLINT(readability-identifier-naming)

    iterator begin() {
        return container.begin();
    }

    iterator end() {
        return container.end();
    }

    void clear() {
        return container.clear();
    }

    reverse_iterator rbegin() {
        return container.rbegin();
    }

    reverse_iterator rend() {
        return container.rend();
    }

    template <typename... ArgsT>
    void push_back(ArgsT &&...val) { // NOLINT(readability-identifier-naming)
        container.push_back(std::forward<ArgsT>(val)...);
    }

    template <typename... ArgsT>
    auto &emplace_back(ArgsT &&...val) { // NOLINT(readability-identifier-naming)
        return container.emplace_back(std::forward<ArgsT>(val)...);
    }

    auto size() {
        return container.size();
    }

    auto &operator[](size_t index) {
        return container[index];
    }

    const auto &operator[](size_t index) const {
        return container[index];
    }

  private:
    VectorType container;

  public:
    bool makeCommandView = false;
};

} // namespace L0
