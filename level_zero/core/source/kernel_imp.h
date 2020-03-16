/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/unified_memory/unified_memory.h"

#include "level_zero/core/source/kernel.h"

#include <memory>

namespace L0 {

struct GraphicsAllocation;

struct KernelImp : Kernel {
    KernelImp(Module *module);

    ~KernelImp() override;

    ze_result_t destroy() override {
        delete this;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setAttribute(ze_kernel_attribute_t attr, uint32_t size, const void *pValue) override;

    ze_result_t getAttribute(ze_kernel_attribute_t attr, uint32_t *pSize, void *pValue) override;

    ze_result_t getProperties(ze_kernel_properties_t *pKernelProperties) override;

    ze_result_t setIntermediateCacheConfig(ze_cache_config_t cacheConfig) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    ze_result_t setArgumentValue(uint32_t argIndex, size_t argSize, const void *pArgValue) override;

    void setGroupCount(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

    bool getGroupCountOffsets(uint32_t *locations) override;

    bool getGroupSizeOffsets(uint32_t *locations) override;

    ze_result_t setGroupSize(uint32_t groupSizeX, uint32_t groupSizeY,
                             uint32_t groupSizeZ) override;

    ze_result_t suggestGroupSize(uint32_t globalSizeX, uint32_t globalSizeY, uint32_t globalSizeZ,
                                 uint32_t *groupSizeX, uint32_t *groupSizeY,
                                 uint32_t *groupSizeZ) override;

    ze_result_t suggestMaxCooperativeGroupCount(uint32_t *totalGroupCount) override;

    const uint8_t *getCrossThreadData() const override { return crossThreadData.get(); }
    uint32_t getCrossThreadDataSize() const override { return crossThreadDataSize; }

    const std::vector<NEO::GraphicsAllocation *> &getResidencyContainer() const override {
        return residencyContainer;
    }

    void getGroupSize(uint32_t &outGroupSizeX, uint32_t &outGroupSizeY,
                      uint32_t &outGroupSizeZ) const override {
        outGroupSizeX = this->groupSize[0];
        outGroupSizeY = this->groupSize[1];
        outGroupSizeZ = this->groupSize[2];
    }

    ze_result_t setArgImmediate(uint32_t argIndex, size_t argSize, const void *argVal);

    ze_result_t setArgBuffer(uint32_t argIndex, size_t argSize, const void *argVal);

    ze_result_t setArgRedescribedImage(uint32_t argIndex, ze_image_handle_t argVal) override;

    ze_result_t setArgBufferWithAlloc(uint32_t argIndex, const void *argVal, NEO::GraphicsAllocation *allocation) override;

    ze_result_t setArgImage(uint32_t argIndex, size_t argSize, const void *argVal);

    ze_result_t setArgSampler(uint32_t argIndex, size_t argSize, const void *argVal);

    virtual void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) = 0;

    ze_result_t initialize(const ze_kernel_desc_t *desc);

    const uint8_t *getPerThreadData() const override { return perThreadDataForWholeThreadGroup; }
    uint32_t getPerThreadDataSizeForWholeThreadGroup() const override { return perThreadDataSizeForWholeThreadGroup; }

    uint32_t getPerThreadDataSize() const override { return perThreadDataSize; }
    uint32_t getThreadsPerThreadGroup() const override { return threadsPerThreadGroup; }
    uint32_t getThreadExecutionMask() const override { return threadExecutionMask; }

    NEO::GraphicsAllocation *getPrintfBufferAllocation() override { return this->printfBuffer; }
    void printPrintfOutput() override;

    const uint8_t *getSurfaceStateHeapData() const override { return surfaceStateHeapData.get(); }
    uint32_t getSurfaceStateHeapDataSize() const override { return surfaceStateHeapDataSize; }

    const uint8_t *getDynamicStateHeapData() const override { return dynamicStateHeapData.get(); }
    size_t getDynamicStateHeapDataSize() const override { return dynamicStateHeapDataSize; }

    const KernelImmutableData *getImmutableData() const override { return kernelImmData; }

    UnifiedMemoryControls getUnifiedMemoryControls() const override { return unifiedMemoryControls; }
    bool hasIndirectAllocationsAllowed() const override;

    bool hasBarriers() override;
    uint32_t getSlmTotalSize() override;
    uint32_t getBindingTableOffset() override;
    uint32_t getBorderColor() override;
    uint32_t getSamplerTableOffset() override;
    uint32_t getNumSurfaceStates() override;
    uint32_t getNumSamplers() override;
    uint32_t getSimdSize() override;
    uint32_t getSizeCrossThreadData() override;
    uint32_t getPerThreadScratchSize() override;
    uint32_t getThreadsPerThreadGroupCount() override;
    uint32_t getSizePerThreadData() override;
    uint32_t getSizePerThreadDataForWholeGroup() override;
    uint32_t getSizeSurfaceStateHeapData() override;
    uint32_t getPerThreadExecutionMask() override;
    uint32_t *getCountOffsets() override;
    uint32_t *getSizeOffsets() override;
    uint32_t *getLocalWorkSize() override;
    uint32_t getNumGrfRequired() override;
    NEO::GraphicsAllocation *getIsaAllocation() override;
    bool hasGroupCounts() override;
    bool hasGroupSize() override;
    const void *getSurfaceStateHeap() override;
    const void *getDynamicStateHeap() override;
    const void *getCrossThread() override;
    const void *getPerThread() override;

  protected:
    KernelImp() = default;

    void patchWorkgroupSizeInCrossThreadData(uint32_t x, uint32_t y, uint32_t z);

    void createPrintfBuffer();
    void setDebugSurface();

    const KernelImmutableData *kernelImmData = nullptr;
    Module *module = nullptr;

    typedef ze_result_t (KernelImp::*KernelArgHandler)(uint32_t argIndex, size_t argSize, const void *argVal);
    std::vector<KernelImp::KernelArgHandler> kernelArgHandlers;
    std::vector<NEO::GraphicsAllocation *> residencyContainer;

    NEO::GraphicsAllocation *printfBuffer = nullptr;

    uint32_t groupSize[3] = {0u, 0u, 0u};
    uint32_t threadsPerThreadGroup = 0u;
    uint32_t threadExecutionMask = 0u;

    std::unique_ptr<uint8_t[]> crossThreadData = 0;
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
    uint32_t slmArgsTotalSize = 0U;
};

} // namespace L0
