/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/helpers/vec.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/unified_memory/unified_memory.h"

#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/source/module/module_imp.h"

#include <memory>
#include <mutex>
#include <vector>

namespace L0 {

struct KernelExt {
    virtual ~KernelExt() = default;
};

struct KernelArgInfo {
    const void *value;
    uint32_t allocId;
    uint32_t allocIdMemoryManagerCounter;
    bool isSetToNullptr = false;
};

struct KernelImp : Kernel {
    KernelImp(Module *module);

    ~KernelImp() override;

    ze_result_t destroy() override {
        if (this->devicePrintfKernelMutex == nullptr) {
            delete this;
            return ZE_RESULT_SUCCESS;
        } else {
            return static_cast<ModuleImp *>(this->module)->destroyPrintfKernel(this);
        }
    }

    ze_result_t getBaseAddress(uint64_t *baseAddress) override;
    ze_result_t getKernelProgramBinary(size_t *kernelSize, char *pKernelBinary) override;
    ze_result_t setIndirectAccess(ze_kernel_indirect_access_flags_t flags) override;
    ze_result_t getIndirectAccess(ze_kernel_indirect_access_flags_t *flags) override;
    ze_result_t getSourceAttributes(uint32_t *pSize, char **pString) override;

    ze_result_t getProperties(ze_kernel_properties_t *pKernelProperties) override;

    ze_result_t setArgumentValue(uint32_t argIndex, size_t argSize, const void *pArgValue) override;

    void setGroupCount(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
    void patchRegionParams(const CmdListKernelLaunchParams &launchParams, const ze_group_count_t &threadGroupDimensions) override;

    ze_result_t setGroupSize(uint32_t groupSizeX, uint32_t groupSizeY,
                             uint32_t groupSizeZ) override;

    ze_result_t suggestGroupSize(uint32_t globalSizeX, uint32_t globalSizeY, uint32_t globalSizeZ,
                                 uint32_t *groupSizeX, uint32_t *groupSizeY,
                                 uint32_t *groupSizeZ) override;

    ze_result_t getKernelName(size_t *pSize, char *pName) override;
    ze_result_t getArgumentSize(uint32_t argIndex, uint32_t *argSize) const override;
    ze_result_t getArgumentType(uint32_t argIndex, uint32_t *pSize, char *pString) const override;

    uint32_t suggestMaxCooperativeGroupCount(NEO::EngineGroupType engineGroupType, bool forceSingleTileQuery) override {
        UNRECOVERABLE_IF(0 == this->groupSize[0]);
        UNRECOVERABLE_IF(0 == this->groupSize[1]);
        UNRECOVERABLE_IF(0 == this->groupSize[2]);

        return suggestMaxCooperativeGroupCount(engineGroupType, this->groupSize, forceSingleTileQuery);
    }

    uint32_t suggestMaxCooperativeGroupCount(NEO::EngineGroupType engineGroupType, uint32_t *groupSize, bool forceSingleTileQuery);

    const uint8_t *getCrossThreadData() const override { return crossThreadData.get(); }
    uint32_t getCrossThreadDataSize() const override { return crossThreadDataSize; }

    const std::vector<NEO::GraphicsAllocation *> &getArgumentsResidencyContainer() const override {
        return argumentsResidencyContainer;
    }

    const std::vector<NEO::GraphicsAllocation *> &getInternalResidencyContainer() const override {
        return internalResidencyContainer;
    }

    ze_result_t setArgImmediate(uint32_t argIndex, size_t argSize, const void *argVal);

    ze_result_t setArgBuffer(uint32_t argIndex, size_t argSize, const void *argVal);

    ze_result_t setArgUnknown(uint32_t argIndex, size_t argSize, const void *argVal);

    ze_result_t setArgRedescribedImage(uint32_t argIndex, ze_image_handle_t argVal, bool isPacked) override;

    ze_result_t setArgBufferWithAlloc(uint32_t argIndex, uintptr_t argVal, NEO::GraphicsAllocation *allocation, NEO::SvmAllocationData *peerAllocData) override;

    ze_result_t setArgImage(uint32_t argIndex, size_t argSize, const void *argVal);

    ze_result_t setArgSampler(uint32_t argIndex, size_t argSize, const void *argVal);

    virtual void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) = 0;

    void setInlineSamplers();

    ze_result_t initialize(const ze_kernel_desc_t *desc);

    const uint8_t *getPerThreadData() const override { return perThreadDataForWholeThreadGroup; }
    uint32_t getPerThreadDataSizeForWholeThreadGroup() const override { return perThreadDataSizeForWholeThreadGroup; }

    uint32_t getPerThreadDataSize() const override { return perThreadDataSize; }
    uint32_t getNumThreadsPerThreadGroup() const override { return numThreadsPerThreadGroup; }
    uint32_t getThreadExecutionMask() const override { return threadExecutionMask; }

    std::mutex *getDevicePrintfKernelMutex() override { return this->devicePrintfKernelMutex; }
    NEO::GraphicsAllocation *getPrintfBufferAllocation() override { return this->printfBuffer; }
    void printPrintfOutput(bool hangDetected) override;

    bool usesSyncBuffer() override;
    bool usesRegionGroupBarrier() const override;
    void patchSyncBuffer(NEO::GraphicsAllocation *gfxAllocation, size_t bufferOffset) override;
    void patchRegionGroupBarrier(NEO::GraphicsAllocation *gfxAllocation, size_t bufferOffset) override;

    const uint8_t *getSurfaceStateHeapData() const override { return surfaceStateHeapData.get(); }
    uint32_t getSurfaceStateHeapDataSize() const override;

    const uint8_t *getDynamicStateHeapData() const override { return dynamicStateHeapData.get(); }

    const KernelImmutableData *getImmutableData() const override { return kernelImmData; }

    UnifiedMemoryControls getUnifiedMemoryControls() const override { return unifiedMemoryControls; }
    bool hasIndirectAllocationsAllowed() const override;

    const NEO::KernelDescriptor &getKernelDescriptor() const override {
        return kernelImmData->getDescriptor();
    }
    const uint32_t *getGroupSize() const override {
        return groupSize;
    }
    uint32_t getSlmTotalSize() const override;

    NEO::SlmPolicy getSlmPolicy() const override {
        if (cacheConfigFlags & ZE_CACHE_CONFIG_FLAG_LARGE_SLM) {
            return NEO::SlmPolicy::slmPolicyLargeSlm;
        } else if (cacheConfigFlags & ZE_CACHE_CONFIG_FLAG_LARGE_DATA) {
            return NEO::SlmPolicy::slmPolicyLargeData;
        } else {
            return NEO::SlmPolicy::slmPolicyNone;
        }
    }

    NEO::GraphicsAllocation *getIsaAllocation() const override;
    uint64_t getIsaOffsetInParentAllocation() const override;

    uint32_t getRequiredWorkgroupOrder() const override { return requiredWorkgroupOrder; }
    bool requiresGenerationOfLocalIdsByRuntime() const override { return kernelRequiresGenerationOfLocalIdsByRuntime; }
    bool getKernelRequiresUncachedMocs() { return (kernelRequiresUncachedMocsCount > 0); }
    bool getKernelRequiresQueueUncachedMocs() { return (kernelRequiresQueueUncachedMocsCount > 0); }
    void setKernelArgUncached(uint32_t index, bool val) { isArgUncached[index] = val; }

    uint32_t *getGlobalOffsets() override {
        return this->globalOffsets;
    }
    ze_result_t setGlobalOffsetExp(uint32_t offsetX, uint32_t offsetY, uint32_t offsetZ) override;
    void patchGlobalOffset() override;

    ze_result_t setCacheConfig(ze_cache_config_flags_t flags) override;
    bool usesRayTracing() {
        return kernelImmData->getDescriptor().kernelAttributes.flags.hasRTCalls;
    }

    ze_result_t getProfileInfo(zet_profile_properties_t *pProfileProperties) override;

    bool hasIndirectAccess() {
        return kernelHasIndirectAccess;
    }

    const Module &getParentModule() const {
        return *this->module;
    }

    NEO::GraphicsAllocation *allocatePrivateMemoryGraphicsAllocation() override;
    void patchCrossthreadDataWithPrivateAllocation(NEO::GraphicsAllocation *privateAllocation) override;
    void patchBindlessOffsetsInCrossThreadData(uint64_t bindlessSurfaceStateBaseOffset) const override;
    void patchBindlessOffsetsForImplicitArgs(uint64_t bindlessSurfaceStateBaseOffset) const;
    void patchSamplerBindlessOffsetsInCrossThreadData(uint64_t samplerStateOffset) const override;

    NEO::GraphicsAllocation *getPrivateMemoryGraphicsAllocation() override {
        return privateMemoryGraphicsAllocation;
    }

    ze_result_t setSchedulingHintExp(ze_scheduling_hint_exp_desc_t *pHint) override;

    NEO::ImplicitArgs *getImplicitArgs() const override { return pImplicitArgs.get(); }

    KernelExt *getExtension(uint32_t extensionType);

    bool checkKernelContainsStatefulAccess();

    size_t getSyncBufferIndex() const {
        return syncBufferIndex;
    }

    NEO::GraphicsAllocation *getSyncBufferAllocation() const {
        if (std::numeric_limits<size_t>::max() == syncBufferIndex) {
            return nullptr;
        }
        return internalResidencyContainer[syncBufferIndex];
    }

    size_t getRegionGroupBarrierIndex() const {
        return regionGroupBarrierIndex;
    }

    NEO::GraphicsAllocation *getRegionGroupBarrierAllocation() const {
        if (std::numeric_limits<size_t>::max() == regionGroupBarrierIndex) {
            return nullptr;
        }
        return internalResidencyContainer[regionGroupBarrierIndex];
    }

    const std::vector<uint32_t> &getSlmArgSizes() {
        return slmArgSizes;
    }
    const std::vector<uint32_t> &getSlmArgOffsetValues() {
        return slmArgOffsetValues;
    }
    uint8_t getRequiredSlmAlignment(uint32_t argIndex) const;

    const std::vector<KernelArgInfo> &getKernelArgInfos() const {
        return kernelArgInfos;
    }

  protected:
    KernelImp() = default;

    void patchWorkgroupSizeInCrossThreadData(uint32_t x, uint32_t y, uint32_t z);

    NEO::GraphicsAllocation *privateMemoryGraphicsAllocation = nullptr;

    void createPrintfBuffer();
    void setAssertBuffer();
    virtual void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) = 0;
    void *patchBindlessSurfaceState(NEO::GraphicsAllocation *alloc, uint32_t bindless);
    uint32_t getSurfaceStateIndexForBindlessOffset(NEO::CrossThreadDataOffset bindlessOffset) const;
    ze_result_t validateWorkgroupSize() const;

    const KernelImmutableData *kernelImmData = nullptr;
    Module *module = nullptr;
    uint32_t implicitArgsVersion = 0;

    typedef ze_result_t (KernelImp::*KernelArgHandler)(uint32_t argIndex, size_t argSize, const void *argVal);
    std::vector<KernelArgInfo> kernelArgInfos;
    std::vector<KernelImp::KernelArgHandler> kernelArgHandlers;
    std::vector<NEO::GraphicsAllocation *> argumentsResidencyContainer;
    std::vector<size_t> implicitArgsResidencyContainerIndices;
    std::vector<NEO::GraphicsAllocation *> internalResidencyContainer;

    std::mutex *devicePrintfKernelMutex = nullptr;
    NEO::GraphicsAllocation *printfBuffer = nullptr;
    size_t syncBufferIndex = std::numeric_limits<size_t>::max();
    size_t regionGroupBarrierIndex = std::numeric_limits<size_t>::max();

    uint32_t groupSize[3] = {0u, 0u, 0u};
    uint32_t numThreadsPerThreadGroup = 1u;
    uint32_t threadExecutionMask = 0u;

    std::unique_ptr<uint8_t[]> crossThreadData = nullptr;
    uint32_t crossThreadDataSize = 0;

    std::unique_ptr<uint8_t[]> surfaceStateHeapData = nullptr;
    uint32_t surfaceStateHeapDataSize = 0;

    std::unique_ptr<uint8_t[]> dynamicStateHeapData = nullptr;
    uint32_t dynamicStateHeapDataSize = 0;

    uint8_t *perThreadDataForWholeThreadGroup = nullptr;
    uint32_t perThreadDataSizeForWholeThreadGroupAllocated = 0;
    uint32_t perThreadDataSizeForWholeThreadGroup = 0u;
    uint32_t perThreadDataSize = 0u;

    UnifiedMemoryControls unifiedMemoryControls;
    std::vector<uint32_t> slmArgSizes;
    std::vector<uint32_t> slmArgOffsetValues;
    uint32_t slmArgsTotalSize = 0U;
    uint32_t requiredWorkgroupOrder = 0u;

    bool kernelRequiresGenerationOfLocalIdsByRuntime = true;
    uint32_t kernelRequiresUncachedMocsCount = 0;
    uint32_t kernelRequiresQueueUncachedMocsCount = 0;
    std::vector<bool> isArgUncached;
    std::vector<bool> isBindlessOffsetSet;
    std::vector<bool> usingSurfaceStateHeap;

    uint32_t globalOffsets[3] = {};

    ze_cache_config_flags_t cacheConfigFlags = 0u;

    bool kernelHasIndirectAccess = false;

    std::unique_ptr<NEO::ImplicitArgs> pImplicitArgs;

    std::unique_ptr<KernelExt> pExtension;

    struct SuggestGroupSizeCacheEntry {
        Vec3<size_t> groupSize;
        uint32_t slmArgsTotalSize = 0u;

        Vec3<size_t> suggestedGroupSize;
        SuggestGroupSizeCacheEntry(size_t groupSize[3], uint32_t slmArgsTotalSize, size_t suggestedGroupSize[3]) : groupSize(groupSize), slmArgsTotalSize(slmArgsTotalSize), suggestedGroupSize(suggestedGroupSize){};
    };
    std::vector<SuggestGroupSizeCacheEntry> suggestGroupSizeCache;
};

} // namespace L0
