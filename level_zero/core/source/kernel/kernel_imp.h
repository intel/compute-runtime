/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/unified_memory/unified_memory.h"

#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/kernel/kernel_mutable_state.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/source/module/module_imp.h"

#include <memory>
#include <mutex>
#include <vector>

namespace NEO {
struct ImplicitArgs;
}

namespace L0 {

struct KernelExt {
    virtual ~KernelExt() = default;
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

    std::unique_ptr<KernelImp> cloneWithStateOverride(const KernelMutableState *stateOverride);

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
        UNRECOVERABLE_IF(0 == this->state.groupSize[0]);
        UNRECOVERABLE_IF(0 == this->state.groupSize[1]);
        UNRECOVERABLE_IF(0 == this->state.groupSize[2]);

        return suggestMaxCooperativeGroupCount(engineGroupType, this->state.groupSize, forceSingleTileQuery);
    }

    uint32_t suggestMaxCooperativeGroupCount(NEO::EngineGroupType engineGroupType, uint32_t *groupSize, bool forceSingleTileQuery);

    const uint8_t *getCrossThreadData() const override { return state.crossThreadData.data(); }
    uint32_t getCrossThreadDataSize() const override { return static_cast<uint32_t>(state.crossThreadData.size()); }

    const std::vector<NEO::GraphicsAllocation *> &getArgumentsResidencyContainer() const override {
        return state.argumentsResidencyContainer;
    }

    const std::vector<NEO::GraphicsAllocation *> &getInternalResidencyContainer() const override {
        return state.internalResidencyContainer;
    }

    ze_result_t setArgImmediate(uint32_t argIndex, size_t argSize, const void *argVal);

    ze_result_t setArgBuffer(uint32_t argIndex, size_t argSize, const void *argVal);

    ze_result_t setArgUnknown(uint32_t argIndex, size_t argSize, const void *argVal);

    ze_result_t setArgRedescribedImage(uint32_t argIndex, ze_image_handle_t argVal, bool isPacked) override;

    ze_result_t setArgBufferWithAlloc(uint32_t argIndex, uintptr_t argVal, NEO::GraphicsAllocation *allocation, NEO::SvmAllocationData *allocData) override;

    ze_result_t setArgImage(uint32_t argIndex, size_t argSize, const void *argVal);

    ze_result_t setArgSampler(uint32_t argIndex, size_t argSize, const void *argVal);

    virtual void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) = 0;

    void setInlineSamplers();

    ze_result_t initialize(const ze_kernel_desc_t *desc);

    const uint8_t *getPerThreadData() const override { return state.perThreadDataForWholeThreadGroup; }
    uint32_t getPerThreadDataSizeForWholeThreadGroup() const override { return state.perThreadDataSizeForWholeThreadGroup; }

    uint32_t getPerThreadDataSize() const override { return state.perThreadDataSize; }
    uint32_t getNumThreadsPerThreadGroup() const override { return state.numThreadsPerThreadGroup; }
    uint32_t getThreadExecutionMask() const override { return state.threadExecutionMask; }

    std::mutex *getDevicePrintfKernelMutex() override { return this->devicePrintfKernelMutex; }
    NEO::GraphicsAllocation *getPrintfBufferAllocation() override { return this->printfBuffer; }
    void printPrintfOutput(bool hangDetected) override;

    bool usesSyncBuffer() override;
    bool usesRegionGroupBarrier() const override;
    void patchSyncBuffer(NEO::GraphicsAllocation *gfxAllocation, size_t bufferOffset) override;
    void patchRegionGroupBarrier(NEO::GraphicsAllocation *gfxAllocation, size_t bufferOffset) override;

    const uint8_t *getSurfaceStateHeapData() const override { return state.surfaceStateHeapData.data(); }
    uint32_t getSurfaceStateHeapDataSize() const override;

    const uint8_t *getDynamicStateHeapData() const override { return state.dynamicStateHeapData.get(); }

    const KernelImmutableData *getImmutableData() const override { return kernelImmData; }

    UnifiedMemoryControls getUnifiedMemoryControls() const override { return state.unifiedMemoryControls; }
    bool hasIndirectAllocationsAllowed() const override;

    const NEO::KernelDescriptor &getKernelDescriptor() const override {
        return kernelImmData->getDescriptor();
    }
    const uint32_t *getGroupSize() const override {
        return state.groupSize;
    }
    uint32_t getSlmTotalSize() const override;

    NEO::SlmPolicy getSlmPolicy() const override {
        if (state.cacheConfigFlags & ZE_CACHE_CONFIG_FLAG_LARGE_SLM) {
            return NEO::SlmPolicy::slmPolicyLargeSlm;
        } else if (state.cacheConfigFlags & ZE_CACHE_CONFIG_FLAG_LARGE_DATA) {
            return NEO::SlmPolicy::slmPolicyLargeData;
        } else {
            return NEO::SlmPolicy::slmPolicyNone;
        }
    }

    NEO::GraphicsAllocation *getIsaAllocation() const override;
    uint64_t getIsaOffsetInParentAllocation() const override;

    uint32_t getRequiredWorkgroupOrder() const override { return state.requiredWorkgroupOrder; }
    bool requiresGenerationOfLocalIdsByRuntime() const override { return state.kernelRequiresGenerationOfLocalIdsByRuntime; }
    bool getKernelRequiresUncachedMocs() { return (state.kernelRequiresUncachedMocsCount > 0); }
    bool getKernelRequiresQueueUncachedMocs() { return (state.kernelRequiresQueueUncachedMocsCount > 0); }
    void setKernelArgUncached(uint32_t index, bool val) { state.isArgUncached[index] = val; }

    uint32_t *getGlobalOffsets() override {
        return this->state.globalOffsets;
    }
    ze_result_t setGlobalOffsetExp(uint32_t offsetX, uint32_t offsetY, uint32_t offsetZ) override;
    void patchGlobalOffset() override;

    ze_result_t setCacheConfig(ze_cache_config_flags_t flags) override;
    bool usesRayTracing() {
        return kernelImmData->getDescriptor().kernelAttributes.flags.hasRTCalls;
    }

    ze_result_t getProfileInfo(zet_profile_properties_t *pProfileProperties) override;

    bool hasIndirectAccess() const {
        return state.kernelHasIndirectAccess;
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

    NEO::ImplicitArgs *getImplicitArgs() const override { return state.pImplicitArgs.get(); }

    KernelExt *getExtension(uint32_t extensionType);

    bool checkKernelContainsStatefulAccess();

    size_t getSyncBufferIndex() const {
        return state.syncBufferIndex;
    }

    NEO::GraphicsAllocation *getSyncBufferAllocation() const {
        if (std::numeric_limits<size_t>::max() == state.syncBufferIndex) {
            return nullptr;
        }
        return state.internalResidencyContainer[state.syncBufferIndex];
    }

    size_t getRegionGroupBarrierIndex() const {
        return state.regionGroupBarrierIndex;
    }

    NEO::GraphicsAllocation *getRegionGroupBarrierAllocation() const {
        if (std::numeric_limits<size_t>::max() == state.regionGroupBarrierIndex) {
            return nullptr;
        }
        return state.internalResidencyContainer[state.regionGroupBarrierIndex];
    }

    const std::vector<uint32_t> &getSlmArgSizes() {
        return state.slmArgSizes;
    }
    const std::vector<uint32_t> &getSlmArgOffsetValues() {
        return state.slmArgOffsetValues;
    }
    uint8_t getRequiredSlmAlignment(uint32_t argIndex) const;

    const std::vector<KernelArgInfo> &getKernelArgInfos() const {
        return state.kernelArgInfos;
    }

    uint32_t getIndirectSize() const override;
    KernelMutableState &getMutableState() { return state; }

  protected:
    KernelImp() = default;

    void patchWorkgroupSizeInCrossThreadData(uint32_t x, uint32_t y, uint32_t z);
    void createPrintfBuffer();
    void setAssertBuffer();
    MOCKABLE_VIRTUAL void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor);
    void *patchBindlessSurfaceState(NEO::GraphicsAllocation *alloc, uint32_t bindless);
    uint32_t getSurfaceStateIndexForBindlessOffset(NEO::CrossThreadDataOffset bindlessOffset) const;
    ze_result_t validateWorkgroupSize() const;
    ArrayRef<uint8_t> getCrossThreadDataSpan() { return ArrayRef<uint8_t>(state.crossThreadData.data(), state.crossThreadData.size()); }
    ArrayRef<uint8_t> getSurfaceStateHeapDataSpan() { return ArrayRef<uint8_t>(state.surfaceStateHeapData.data(), state.surfaceStateHeapData.size()); }

    const KernelImmutableData *kernelImmData = nullptr;
    Module *module = nullptr;
    std::mutex *devicePrintfKernelMutex = nullptr;
    KernelImp *cloneOrigin = nullptr;

    NEO::GraphicsAllocation *privateMemoryGraphicsAllocation = nullptr;
    NEO::GraphicsAllocation *printfBuffer = nullptr;
    uintptr_t surfaceStateAlignmentMask = 0;
    uintptr_t surfaceStateAlignment = 0;

    uint32_t implicitArgsVersion = 0;
    uint32_t walkerInlineDataSize = 0;

    KernelMutableState state{};
};

} // namespace L0
