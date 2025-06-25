/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/definitions/command_encoder_args.h"

#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"

#include <array>

namespace NEO {
class Device;
} // namespace NEO

namespace L0::MCL {

struct MutableWalkerSpecificFieldsArguments {
    const uint32_t *threadGroupDimensions;
    const uint32_t threadGroupCount;
    const uint32_t requiredThreadGroupDispatchSize;
    const uint32_t grfCount;
    const uint32_t threadsPerThreadGroup;
    const uint32_t totalWorkGroupSize;
    uint32_t slmTotalSize;
    uint32_t slmPolicy;
    uint32_t partitionCount;
    uint32_t maxWgCountPerTile;
    NEO::RequiredPartitionDim requiredPartitionDim;
    bool isRequiredDispatchWorkGroupOrder;
    bool isSlmKernel;
    bool cooperativeKernel;
    bool updateGroupCount;
    bool updateGroupSize;
    bool updateSlm;
};

struct MutableComputeWalker {
    MutableComputeWalker(void *walker, uint8_t indirectOffset, uint8_t scratchOffset, bool stageCommitMode)
        : walker(walker),
          indirectOffset(indirectOffset),
          scratchOffset(scratchOffset),
          stageCommitMode(stageCommitMode) {}
    virtual ~MutableComputeWalker() {}

    virtual void setKernelStartAddress(GpuAddress kernelStartAddress) = 0;
    virtual void setIndirectDataStartAddress(GpuAddress indirectDataStartAddress) = 0;
    virtual void setIndirectDataSize(size_t indirectDataSize) = 0;
    virtual void setBindingTablePointer(GpuAddress bindingTablePointer) = 0;

    virtual void setGenerateLocalId(bool generateLocalIdsByGpu, uint32_t walkOrder, uint32_t localIdDimensions) = 0;
    virtual void setNumberThreadsPerThreadGroup(uint32_t numThreadPerThreadGroup) = 0;
    virtual void setNumberWorkGroups(std::array<uint32_t, 3> numberWorkGroups) = 0;
    virtual void setWorkGroupSize(std::array<uint32_t, 3> workgroupSize) = 0;
    virtual void setExecutionMask(uint32_t mask) = 0;

    virtual void setPostSyncAddress(GpuAddress postSyncAddress, GpuAddress inOrderIncrementAddress) = 0;

    virtual void updateSpecificFields(const NEO::Device &device,
                                      MutableWalkerSpecificFieldsArguments &args) = 0;

    virtual void updateSlmSize(const NEO::Device &device, uint32_t slmTotalSize) = 0;

    virtual void *getInlineDataPointer() const = 0;
    virtual size_t getInlineDataOffset() const = 0;
    virtual size_t getInlineDataSize() const = 0;
    virtual void *getHostMemoryInlineDataPointer() const = 0;

    void *getWalkerCmdPointer() const {
        return walker;
    }

    uint8_t getIndirectOffset() const {
        return indirectOffset;
    }

    uint8_t getScratchOffset() const {
        return scratchOffset;
    }

    virtual void copyWalkerDataToHostBuffer(MutableComputeWalker *sourceWalker) = 0;
    virtual void updateWalkerScratchPatchAddress(GpuAddress scratchPatchAddress) = 0;
    virtual void saveCpuBufferIntoGpuBuffer(bool useDispatchPart) = 0;

  protected:
    void *walker;
    uint8_t indirectOffset;
    uint8_t scratchOffset;
    bool stageCommitMode = false;
};

} // namespace L0::MCL
