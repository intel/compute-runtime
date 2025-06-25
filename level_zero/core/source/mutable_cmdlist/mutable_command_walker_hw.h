/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker.h"

namespace L0::MCL {

template <typename GfxFamily>
struct MutableComputeWalkerHw : public MutableComputeWalker, NEO::NonCopyableAndNonMovableClass {
    MutableComputeWalkerHw(void *walker, uint8_t indirectOffset, uint8_t scratchOffset, void *cpuBuffer, bool stageCommitMode)
        : MutableComputeWalker(walker, indirectOffset, scratchOffset, stageCommitMode),
          cpuBuffer(cpuBuffer) {}
    ~MutableComputeWalkerHw() override {
        deleteCommandBuffer();
    }

    void setKernelStartAddress(GpuAddress kernelStartAddress) override;
    void setIndirectDataStartAddress(GpuAddress indirectDataStartAddress) override;
    void setIndirectDataSize(size_t indirectDataSize) override;
    void setBindingTablePointer(GpuAddress bindingTablePointer) override;

    void setGenerateLocalId(bool generateLocalIdsByGpu, uint32_t walkOrder, uint32_t localIdDimensions) override;
    void setNumberThreadsPerThreadGroup(uint32_t numThreadPerThreadGroup) override;
    void setNumberWorkGroups(std::array<uint32_t, 3> numberWorkGroups) override;
    void setWorkGroupSize(std::array<uint32_t, 3> workgroupSize) override;
    void setExecutionMask(uint32_t mask) override;

    void setPostSyncAddress(GpuAddress postSyncAddress, GpuAddress inOrderIncrementAddress) override;

    void updateSpecificFields(const NEO::Device &device,
                              MutableWalkerSpecificFieldsArguments &args) override;

    void updateSlmSize(const NEO::Device &device, uint32_t slmTotalSize) override;

    void *getInlineDataPointer() const override;
    size_t getInlineDataOffset() const override;
    size_t getInlineDataSize() const override;
    void *getHostMemoryInlineDataPointer() const override;

    static size_t getCommandSize();
    static void *createCommandBuffer();
    void deleteCommandBuffer();

    void copyWalkerDataToHostBuffer(MutableComputeWalker *sourceWalker) override;
    void updateWalkerScratchPatchAddress(GpuAddress scratchPatchAddress) override;
    void saveCpuBufferIntoGpuBuffer(bool useDispatchPart) override;

  protected:
    template <typename WalkerType>
    void updateImplicitScalingData(const NEO::Device &device,
                                   uint32_t partitionCount,
                                   uint32_t workgroupSize,
                                   uint32_t threadGroupCount,
                                   uint32_t maxWgCountPerTile,
                                   NEO::RequiredPartitionDim requiredPartitionDim,
                                   bool isRequiredDispatchWorkGroupOrder,
                                   bool cooperativeKernel);

    void setSlmSize(uint32_t slmSize);

    void *cpuBuffer = nullptr;
    static const bool isHeapless;
};

} // namespace L0::MCL
