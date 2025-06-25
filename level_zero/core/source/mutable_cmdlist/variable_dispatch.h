/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/definitions/engine_group_types.h"

#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_indirect_data.h"

#include <array>
#include <functional>
#include <memory>

namespace NEO {
class Device;
struct KernelDescriptor;
struct RootDeviceEnvironment;
} // namespace NEO
namespace L0::MCL {
struct KernelDispatch;
struct MutableComputeWalker;
struct MutableKernelDispatchParameters;
struct Variable;

struct VariableDispatch {
    VariableDispatch(KernelDispatch *kernelDispatch,
                     std::unique_ptr<MutableIndirectData> mutableIndirectData, MutableComputeWalker *mutableCommandWalker,
                     Variable *groupSizeVariable, Variable *groupCountVariable, Variable *globalOffsetVariable, Variable *lastSlmArgumentVariable,
                     uint32_t grfSize, const MutableKernelDispatchParameters &dispatchParams, uint32_t partitionCount,
                     NEO::EngineGroupType cmdListEngineType, bool calculateRegion);

    void setGroupSize(const uint32_t groupSize[3], NEO::Device &device, bool stageData);
    void setGroupCount(const uint32_t groupCount[3], const NEO::Device &device, bool stageData);
    void setGlobalOffset(const uint32_t globalOffset[3]);
    void setSlmSize(const uint32_t slmArgTotalSize, NEO::Device &device, bool stageData);
    void commitChanges(const NEO::Device &device);

    Variable *getGroupSizeVar() const;
    Variable *getGroupCountVar() const;
    Variable *getGlobalOffsetVar() const;
    Variable *getLastSlmArgumentVariable() const;
    const MutableIndirectData::Offsets &getIndirectDataOffsets() const;

    MutableIndirectData *getIndirectData() const {
        return indirectData.get();
    }

    bool isCooperativeDispatch() const {
        return isCooperative;
    }

    uint32_t getTotalGroupCount() const {
        return threadGroupCount;
    }

    uint32_t getMaxCooperativeGroupCount() const {
        return maxCooperativeGroupCount;
    }

  protected:
    void setGws();
    void setWorkDim();

    bool requiresLocalIdGeneration(size_t localWorkSize, uint32_t &outWalkOrder, const NEO::RootDeviceEnvironment &rootDeviceEnvironment);
    void generateLocalIds(size_t localWorkSize, const NEO::RootDeviceEnvironment &rootDeviceEnvironment);

    bool doCommitVariableDispatch() {
        return commitGroupCount || commitGroupSize || commitSlmSize;
    }

    void cleanCommitVariableDispatch() {
        commitGroupCount = false;
        commitGroupSize = false;
        commitSlmSize = false;
    }

    std::array<uint32_t, 3> groupSize = {1, 1, 1};
    std::array<uint32_t, 3> groupCount = {1, 1, 1};
    std::array<uint32_t, 3> globalOffset = {0, 0, 0};

    std::unique_ptr<uint8_t, std::function<void(void *)>> perThreadData;

    KernelDispatch *kernelDispatch;
    std::unique_ptr<MutableIndirectData> indirectData;
    MutableComputeWalker *mutableCommandWalker;
    Variable *groupSizeVar = nullptr;
    Variable *groupCountVar = nullptr;
    Variable *globalOffsetVar = nullptr;
    Variable *lastSlmArgumentVar = nullptr;

    size_t perThreadDataAllocSize = 0U;
    size_t perThreadDataSize = 0U;
    size_t totalLwsSize = 1u;
    size_t syncBufferOffset = undefined<size_t>;
    size_t regionBarrierOffset = undefined<size_t>;

    const uint32_t grfSize;
    const uint32_t numChannels = 3U;
    uint32_t walkOrder = 0U;
    uint32_t numThreadsPerThreadGroup;
    uint32_t threadExecutionMask;
    uint32_t threadGroupCount = 1;
    uint32_t maxWgCountPerTile = 1;
    uint32_t maxCooperativeGroupCount = 0;
    uint32_t slmTotalSize = 0;
    uint32_t alignedSlmSize = 0;

    uint32_t localRegionSize = NEO::localRegionSizeParamNotSet;
    NEO::RequiredPartitionDim requiredPartitionDim = NEO::RequiredPartitionDim::none;
    NEO::RequiredDispatchWalkOrder requiredDispatchWalkOrder = NEO::RequiredDispatchWalkOrder::none;

    const uint32_t partitionCount = 1;
    const NEO::EngineGroupType cmdListEngineType;

    bool commitGroupCount = false;
    bool commitGroupSize = false;
    bool commitSlmSize = false;
    bool localIdGenerationByRuntime = false;
    bool calculateRegion = false;
    bool isCooperative = false;
    bool isSlmKernel = false;
};

} // namespace L0::MCL
