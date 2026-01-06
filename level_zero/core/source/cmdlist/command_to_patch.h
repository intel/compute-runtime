/*
 * Copyright (C) 2025 Intel Corporation
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
    void *pCommand = nullptr;
    uint64_t callbackAddress = 0;
    uint64_t userDataAddress = 0;
};

struct PatchHostFunctionWait {
    void *pCommand = nullptr;
    uint32_t partitionId = 0;
};

struct PatchSignalEventPostSyncPipeControl {
    void *pDestination = nullptr;
};

struct PatchWaitEventSemaphoreWait {
    void *pDestination = nullptr;
    size_t inOrderPatchListIndex = 0;
    size_t offset = 0;
};

struct PatchTimestampEventPostSyncStoreRegMem {
    void *pDestination = nullptr;
    size_t offset = 0;
};

struct PatchCbEventTimestampPostSyncSemaphoreWait {
    void *pDestination = nullptr;
    size_t offset = 0;
};

struct PatchCbEventTimestampClearStoreDataImm {
    void *pDestination = nullptr;
    size_t offset = 0;
};

struct PatchCbWaitEventSemaphoreWait {
    void *pDestination = nullptr;
    size_t inOrderPatchListIndex = 0;
    size_t offset = 0;
};

struct PatchCbWaitEventLoadRegisterImm {
    void *pDestination = nullptr;
    size_t inOrderPatchListIndex = 0;
    size_t offset = 0;
};

struct PatchPrefetchKernelMemory {
    void *pDestination = nullptr;
    size_t offset = 0;
    size_t patchSize = 0;
};

struct PatchInvalidPatchType {};

using CommandToPatch = std::variant<
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
    PatchHostFunctionWait,
    PatchSignalEventPostSyncPipeControl,
    PatchWaitEventSemaphoreWait,
    PatchTimestampEventPostSyncStoreRegMem,
    PatchCbEventTimestampPostSyncSemaphoreWait,
    PatchCbEventTimestampClearStoreDataImm,
    PatchCbWaitEventSemaphoreWait,
    PatchCbWaitEventLoadRegisterImm,
    PatchPrefetchKernelMemory>;

using CommandToPatchContainer = std::vector<CommandToPatch>;
} // namespace L0
