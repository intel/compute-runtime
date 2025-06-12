/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/unified_memory/unified_memory.h"

#include "level_zero/core/source/helpers/api_handle_helper.h"

#include <memory>
#include <vector>

struct _ze_kernel_handle_t : BaseHandleWithLoaderTranslation<ZEL_HANDLE_KERNEL> {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_kernel_handle_t>);

namespace NEO {
class Device;
struct KernelInfo;
class MemoryManager;
} // namespace NEO

namespace L0 {
struct Device;
struct Module;
struct CmdListKernelLaunchParams;

struct KernelImmutableData {
    KernelImmutableData(L0::Device *l0device = nullptr);
    virtual ~KernelImmutableData();

    MOCKABLE_VIRTUAL ze_result_t initialize(NEO::KernelInfo *kernelInfo, Device *device, uint32_t computeUnitsUsedForSratch,
                                            NEO::GraphicsAllocation *globalConstBuffer, NEO::GraphicsAllocation *globalVarBuffer,
                                            bool internalKernel);

    const std::vector<NEO::GraphicsAllocation *> &getResidencyContainer() const {
        return residencyContainer;
    }

    std::vector<NEO::GraphicsAllocation *> &getResidencyContainer() {
        return residencyContainer;
    }

    uint32_t getIsaSize() const;
    NEO::GraphicsAllocation *getIsaGraphicsAllocation() const;
    void setIsaPerKernelAllocation(NEO::GraphicsAllocation *allocation);
    inline NEO::GraphicsAllocation *getIsaParentAllocation() const { return isaParentAllocation; }
    inline void setIsaParentAllocation(NEO::GraphicsAllocation *allocation) { isaParentAllocation = allocation; };
    inline size_t getIsaOffsetInParentAllocation() const { return isaSubAllocationOffset; }
    inline void setIsaSubAllocationOffset(size_t offset) { isaSubAllocationOffset = offset; }
    inline void setIsaSubAllocationSize(size_t size) { isaSubAllocationSize = size; }
    inline size_t getIsaSubAllocationSize() const { return isaSubAllocationSize; }

    const uint8_t *getCrossThreadDataTemplate() const { return crossThreadDataTemplate.get(); }

    uint32_t getSurfaceStateHeapSize() const { return surfaceStateHeapSize; }
    const uint8_t *getSurfaceStateHeapTemplate() const { return surfaceStateHeapTemplate.get(); }

    uint32_t getDynamicStateHeapDataSize() const { return dynamicStateHeapSize; }
    const uint8_t *getDynamicStateHeapTemplate() const { return dynamicStateHeapTemplate.get(); }

    const NEO::KernelDescriptor &getDescriptor() const { return *kernelDescriptor; }

    Device *getDevice() { return this->device; }

    const NEO::KernelInfo *getKernelInfo() const { return kernelInfo; }

    void setIsaCopiedToAllocation() {
        isaCopiedToAllocation = true;
    }

    bool isIsaCopiedToAllocation() const {
        return isaCopiedToAllocation;
    }

    MOCKABLE_VIRTUAL void createRelocatedDebugData(NEO::GraphicsAllocation *globalConstBuffer,
                                                   NEO::GraphicsAllocation *globalVarBuffer);

  protected:
    Device *device = nullptr;
    NEO::KernelInfo *kernelInfo = nullptr;
    NEO::KernelDescriptor *kernelDescriptor = nullptr;
    std::unique_ptr<NEO::GraphicsAllocation> isaGraphicsAllocation = nullptr;
    NEO::GraphicsAllocation *isaParentAllocation = nullptr;
    size_t isaSubAllocationOffset = 0lu;
    size_t isaSubAllocationSize = 0lu;

    uint32_t crossThreadDataSize = 0;
    std::unique_ptr<uint8_t[]> crossThreadDataTemplate = nullptr;

    uint32_t surfaceStateHeapSize = 0;
    std::unique_ptr<uint8_t[]> surfaceStateHeapTemplate = nullptr;

    uint32_t dynamicStateHeapSize = 0;
    std::unique_ptr<uint8_t[]> dynamicStateHeapTemplate = nullptr;

    std::vector<NEO::GraphicsAllocation *> residencyContainer;

    bool isaCopiedToAllocation = false;
};

struct Kernel : _ze_kernel_handle_t, virtual NEO::DispatchKernelEncoderI, NEO::NonCopyableAndNonMovableClass {
    template <typename Type>
    struct Allocator {
        static Kernel *allocate(Module *module) { return new Type(module); }
    };

    static Kernel *create(uint32_t productFamily, Module *module,
                          const ze_kernel_desc_t *desc, ze_result_t *ret);

    ~Kernel() override = default;

    virtual ze_result_t destroy() = 0;
    virtual ze_result_t getBaseAddress(uint64_t *baseAddress) = 0;
    virtual ze_result_t getKernelProgramBinary(size_t *kernelSize, char *pKernelBinary) = 0;
    virtual ze_result_t setIndirectAccess(ze_kernel_indirect_access_flags_t flags) = 0;
    virtual ze_result_t getIndirectAccess(ze_kernel_indirect_access_flags_t *flags) = 0;
    virtual ze_result_t getSourceAttributes(uint32_t *pSize, char **pString) = 0;
    virtual ze_result_t getProperties(ze_kernel_properties_t *pKernelProperties) = 0;
    virtual ze_result_t setArgumentValue(uint32_t argIndex, size_t argSize, const void *pArgValue) = 0;
    virtual void setGroupCount(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;

    virtual ze_result_t setArgBufferWithAlloc(uint32_t argIndex, uintptr_t argVal, NEO::GraphicsAllocation *allocation, NEO::SvmAllocationData *peerAllocData) = 0;
    virtual ze_result_t setArgRedescribedImage(uint32_t argIndex, ze_image_handle_t argVal, bool isPacked) = 0;
    virtual ze_result_t setGroupSize(uint32_t groupSizeX, uint32_t groupSizeY,
                                     uint32_t groupSizeZ) = 0;
    virtual ze_result_t suggestGroupSize(uint32_t globalSizeX, uint32_t globalSizeY,
                                         uint32_t globalSizeZ, uint32_t *groupSizeX,
                                         uint32_t *groupSizeY, uint32_t *groupSizeZ) = 0;
    virtual ze_result_t getKernelName(size_t *pSize, char *pName) = 0;
    virtual ze_result_t getArgumentSize(uint32_t argIndex, uint32_t *argSize) const = 0;
    virtual ze_result_t getArgumentType(uint32_t argIndex, uint32_t *pSize, char *pString) const = 0;

    virtual uint32_t *getGlobalOffsets() = 0;
    virtual ze_result_t setGlobalOffsetExp(uint32_t offsetX, uint32_t offsetY, uint32_t offsetZ) = 0;
    virtual void patchGlobalOffset() = 0;
    virtual void patchRegionParams(const CmdListKernelLaunchParams &launchParams, const ze_group_count_t &threadGroupDimensions) = 0;

    virtual uint32_t suggestMaxCooperativeGroupCount(NEO::EngineGroupType engineGroupType, bool forceSingleTileQuery) = 0;
    virtual ze_result_t setCacheConfig(ze_cache_config_flags_t flags) = 0;

    virtual ze_result_t getProfileInfo(zet_profile_properties_t *pProfileProperties) = 0;

    virtual const KernelImmutableData *getImmutableData() const = 0;

    virtual const std::vector<NEO::GraphicsAllocation *> &getArgumentsResidencyContainer() const = 0;
    virtual const std::vector<NEO::GraphicsAllocation *> &getInternalResidencyContainer() const = 0;

    virtual UnifiedMemoryControls getUnifiedMemoryControls() const = 0;
    virtual bool hasIndirectAllocationsAllowed() const = 0;

    virtual std::mutex *getDevicePrintfKernelMutex() = 0;
    virtual NEO::GraphicsAllocation *getPrintfBufferAllocation() = 0;
    virtual void printPrintfOutput(bool hangDetected) = 0;

    virtual bool usesSyncBuffer() = 0;
    virtual bool usesRegionGroupBarrier() const = 0;
    virtual void patchSyncBuffer(NEO::GraphicsAllocation *gfxAllocation, size_t bufferOffset) = 0;
    virtual void patchRegionGroupBarrier(NEO::GraphicsAllocation *gfxAllocation, size_t bufferOffset) = 0;

    virtual NEO::GraphicsAllocation *allocatePrivateMemoryGraphicsAllocation() = 0;
    virtual void patchCrossthreadDataWithPrivateAllocation(NEO::GraphicsAllocation *privateAllocation) = 0;

    virtual NEO::GraphicsAllocation *getPrivateMemoryGraphicsAllocation() = 0;

    virtual ze_result_t setSchedulingHintExp(ze_scheduling_hint_exp_desc_t *pHint) = 0;

    static Kernel *fromHandle(ze_kernel_handle_t handle) { return static_cast<Kernel *>(handle); }

    inline ze_kernel_handle_t toHandle() { return this; }

    uint32_t getMaxWgCountPerTile(NEO::EngineGroupType engineGroupType) const {
        auto value = maxWgCountPerTileCcs;
        if (engineGroupType == NEO::EngineGroupType::renderCompute) {
            value = maxWgCountPerTileRcs;
        } else if (engineGroupType == NEO::EngineGroupType::cooperativeCompute) {
            value = maxWgCountPerTileCooperative;
        }
        return value;
    }

    virtual uint32_t getIndirectSize() const = 0;

  protected:
    uint32_t maxWgCountPerTileCcs = 0;
    uint32_t maxWgCountPerTileRcs = 0;
    uint32_t maxWgCountPerTileCooperative = 0;
    bool heaplessEnabled = false;
    bool implicitScalingEnabled = false;
    bool localDispatchSupport = false;
    bool rcsAvailable = false;
    bool cooperativeSupport = false;
};

using KernelAllocatorFn = Kernel *(*)(Module *module);
extern KernelAllocatorFn kernelFactory[];

template <uint32_t productFamily, typename KernelType>
struct KernelPopulateFactory {
    KernelPopulateFactory() {
        kernelFactory[productFamily] = KernelType::template Allocator<KernelType>::allocate;
    }
};

static_assert(NEO::NonCopyableAndNonMovable<Kernel>);

} // namespace L0
