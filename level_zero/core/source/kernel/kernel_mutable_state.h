/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/vec.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/mem_lifetime.h"

#include "level_zero/ze_api.h"

namespace NEO {
struct ImplicitArgs;
}

namespace L0 {

struct KernelImp;
struct KernelExt;

struct KernelArgInfo {
    const void *value;
    uint32_t allocId;
    uint32_t allocIdMemoryManagerCounter;
    bool isSetToNullptr = false;

    bool operator==(const KernelArgInfo &) const = default;
};

struct KernelMutableStateDefaultCopyableParams {
    using KernelArgHandler = ze_result_t (KernelImp::*)(uint32_t argIndex, size_t argSize, const void *argVal);

    struct SuggestGroupSizeCacheEntry {
        Vec3<size_t> groupSize;
        uint32_t slmArgsTotalSize = 0u;

        Vec3<size_t> suggestedGroupSize;
        SuggestGroupSizeCacheEntry(size_t groupSize[3], uint32_t slmArgsTotalSize, size_t suggestedGroupSize[3]) : groupSize(groupSize), slmArgsTotalSize(slmArgsTotalSize), suggestedGroupSize(suggestedGroupSize){};

        bool operator==(const SuggestGroupSizeCacheEntry &) const = default;
    };

    UnifiedMemoryControls unifiedMemoryControls;
    std::vector<SuggestGroupSizeCacheEntry> suggestGroupSizeCache;
    std::vector<KernelArgInfo> kernelArgInfos;
    std::vector<KernelArgHandler> kernelArgHandlers;
    std::vector<NEO::GraphicsAllocation *> argumentsResidencyContainer;
    std::vector<size_t> implicitArgsResidencyContainerIndices;
    std::vector<NEO::GraphicsAllocation *> internalResidencyContainer;
    std::vector<bool> isArgUncached;
    std::vector<bool> isBindlessOffsetSet;
    std::vector<bool> usingSurfaceStateHeap;
    std::vector<uint32_t> slmArgSizes;
    std::vector<uint32_t> slmArgOffsetValues;

    size_t syncBufferIndex = std::numeric_limits<size_t>::max();
    size_t regionGroupBarrierIndex = std::numeric_limits<size_t>::max();

    static constexpr size_t dimMax{3U};
    uint32_t globalOffsets[dimMax] = {};
    uint32_t groupSize[dimMax] = {0u, 0u, 0u};
    uint32_t perThreadDataSize = 0U;
    uint32_t slmArgsTotalSize = 0U;
    uint32_t kernelRequiresQueueUncachedMocsCount = 0U;
    uint32_t kernelRequiresUncachedMocsCount = 0U;
    uint32_t requiredWorkgroupOrder = 0U;
    uint32_t threadExecutionMask = 0U;
    uint32_t numThreadsPerThreadGroup = 1U;
    ze_cache_config_flags_t cacheConfigFlags = 0U;

    bool kernelHasIndirectAccess = false;
    bool kernelRequiresGenerationOfLocalIdsByRuntime = true;
};

struct KernelMutableState : public KernelMutableStateDefaultCopyableParams {
    using Params = KernelMutableStateDefaultCopyableParams;

    KernelMutableState();
    KernelMutableState(const KernelMutableState &rhs);
    KernelMutableState(KernelMutableState &&orig) noexcept;
    KernelMutableState &operator=(const KernelMutableState &rhs);
    KernelMutableState &operator=(KernelMutableState &&rhs) noexcept;
    ~KernelMutableState();
    void swap(KernelMutableState &rhs);
    void moveMembersFrom(KernelMutableState &&orig);

    void reservePerThreadDataForWholeThreadGroup(uint32_t sizeNeeded);

    Clonable<NEO::ImplicitArgs> pImplicitArgs;
    std::unique_ptr<KernelExt> pExtension;
    std::vector<uint8_t> crossThreadData{};
    std::vector<uint8_t> surfaceStateHeapData{};
    std::vector<uint8_t> dynamicStateHeapData{};

    uint8_t *perThreadDataForWholeThreadGroup = nullptr;
    uint32_t perThreadDataSizeForWholeThreadGroup = 0U;
    uint32_t perThreadDataSizeForWholeThreadGroupAllocated = 0U;
};
} // namespace L0