/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/unified_memory/unified_memory.h"

#include <level_zero/ze_api.h>

#include <memory>
#include <vector>

struct _ze_kernel_handle_t {};

namespace NEO {
class Device;
struct KernelInfo;
class MemoryManager;
} // namespace NEO

namespace L0 {
struct Device;
struct Module;

struct KernelImmutableData {
    KernelImmutableData(L0::Device *l0device = nullptr);
    virtual ~KernelImmutableData();

    void initialize(NEO::KernelInfo *kernelInfo, NEO::MemoryManager &memoryManager, const NEO::Device *device,
                    uint32_t computeUnitsUsedForSratch,
                    NEO::GraphicsAllocation *globalConstBuffer, NEO::GraphicsAllocation *globalVarBuffer);

    const std::vector<NEO::GraphicsAllocation *> &getResidencyContainer() const {
        return residencyContainer;
    }

    uint32_t getIsaSize() const;
    NEO::GraphicsAllocation *getIsaGraphicsAllocation() const { return isaGraphicsAllocation.get(); }

    uint64_t getPrivateMemorySize() const;
    NEO::GraphicsAllocation *getPrivateMemoryGraphicsAllocation() const { return privateMemoryGraphicsAllocation.get(); }

    const uint8_t *getCrossThreadDataTemplate() const { return crossThreadDataTemplate.get(); }

    uint32_t getSurfaceStateHeapSize() const { return surfaceStateHeapSize; }
    const uint8_t *getSurfaceStateHeapTemplate() const { return surfaceStateHeapTemplate.get(); }

    uint32_t getDynamicStateHeapDataSize() const { return dynamicStateHeapSize; }
    const uint8_t *getDynamicStateHeapTemplate() const { return dynamicStateHeapTemplate.get(); }

    const NEO::KernelDescriptor &getDescriptor() const { return *kernelDescriptor; }

    Device *getDevice() { return this->device; }

  protected:
    Device *device = nullptr;
    NEO::KernelDescriptor *kernelDescriptor = nullptr;
    std::unique_ptr<NEO::GraphicsAllocation> isaGraphicsAllocation = nullptr;
    std::unique_ptr<NEO::GraphicsAllocation> privateMemoryGraphicsAllocation = nullptr;

    uint32_t crossThreadDataSize = 0;
    std::unique_ptr<uint8_t[]> crossThreadDataTemplate = nullptr;

    uint32_t surfaceStateHeapSize = 0;
    std::unique_ptr<uint8_t[]> surfaceStateHeapTemplate = nullptr;

    uint32_t dynamicStateHeapSize = 0;
    std::unique_ptr<uint8_t[]> dynamicStateHeapTemplate = nullptr;

    std::vector<NEO::GraphicsAllocation *> residencyContainer;
};

struct Kernel : _ze_kernel_handle_t, virtual NEO::DispatchKernelEncoderI {
    template <typename Type>
    struct Allocator {
        static Kernel *allocate(Module *module) { return new Type(module); }
    };

    static Kernel *create(uint32_t productFamily, Module *module,
                          const ze_kernel_desc_t *desc, ze_result_t *ret);

    ~Kernel() override = default;

    virtual ze_result_t destroy() = 0;
    virtual ze_result_t setAttribute(ze_kernel_attribute_t attr, uint32_t size, const void *pValue) = 0;
    virtual ze_result_t getAttribute(ze_kernel_attribute_t attr, uint32_t *pSize, void *pValue) = 0;
    virtual ze_result_t setIntermediateCacheConfig(ze_cache_config_t cacheConfig) = 0;
    virtual ze_result_t getProperties(ze_kernel_properties_t *pKernelProperties) = 0;
    virtual ze_result_t setArgumentValue(uint32_t argIndex, size_t argSize, const void *pArgValue) = 0;
    virtual void setGroupCount(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;

    virtual ze_result_t setArgBufferWithAlloc(uint32_t argIndex, const void *argVal, NEO::GraphicsAllocation *allocation) = 0;
    virtual ze_result_t setArgRedescribedImage(uint32_t argIndex, ze_image_handle_t argVal) = 0;
    virtual bool getGroupCountOffsets(uint32_t *locations) = 0;
    virtual bool getGroupSizeOffsets(uint32_t *locations) = 0;
    virtual ze_result_t setGroupSize(uint32_t groupSizeX, uint32_t groupSizeY,
                                     uint32_t groupSizeZ) = 0;
    virtual ze_result_t suggestGroupSize(uint32_t globalSizeX, uint32_t globalSizeY,
                                         uint32_t globalSizeZ, uint32_t *groupSizeX,
                                         uint32_t *groupSizeY, uint32_t *groupSizeZ) = 0;

    virtual ze_result_t suggestMaxCooperativeGroupCount(uint32_t *totalGroupCount) = 0;

    virtual const KernelImmutableData *getImmutableData() const = 0;
    virtual std::unique_ptr<Kernel> clone() const = 0;

    virtual const std::vector<NEO::GraphicsAllocation *> &getResidencyContainer() const = 0;

    virtual void getGroupSize(uint32_t &outGroupSizeX, uint32_t &outGroupSizeY, uint32_t &outGroupSizeZ) const = 0;
    virtual uint32_t getThreadsPerThreadGroup() const = 0;
    virtual uint32_t getThreadExecutionMask() const = 0;

    virtual const uint8_t *getCrossThreadData() const = 0;
    virtual uint32_t getCrossThreadDataSize() const = 0;

    virtual const uint8_t *getPerThreadData() const = 0;
    virtual uint32_t getPerThreadDataSizeForWholeThreadGroup() const = 0;
    virtual uint32_t getPerThreadDataSize() const = 0;
    virtual const uint8_t *getSurfaceStateHeapData() const = 0;
    virtual uint32_t getSurfaceStateHeapDataSize() const = 0;

    virtual const uint8_t *getDynamicStateHeapData() const = 0;
    virtual size_t getDynamicStateHeapDataSize() const = 0;

    virtual UnifiedMemoryControls getUnifiedMemoryControls() const = 0;
    virtual bool hasIndirectAllocationsAllowed() const = 0;

    virtual NEO::GraphicsAllocation *getPrintfBufferAllocation() = 0;
    virtual void printPrintfOutput() = 0;

    Kernel() = default;
    Kernel(const Kernel &) = delete;
    Kernel(Kernel &&) = delete;
    Kernel &operator=(const Kernel &) = delete;
    Kernel &operator=(Kernel &&) = delete;

    static Kernel *fromHandle(ze_kernel_handle_t handle) { return static_cast<Kernel *>(handle); }

    inline ze_kernel_handle_t toHandle() { return this; }
};

using KernelAllocatorFn = Kernel *(*)(Module *module);
extern KernelAllocatorFn kernelFactory[];

template <uint32_t productFamily, typename KernelType>
struct KernelPopulateFactory {
    KernelPopulateFactory() {
        kernelFactory[productFamily] = KernelType::template Allocator<KernelType>::allocate;
    }
};

} // namespace L0
