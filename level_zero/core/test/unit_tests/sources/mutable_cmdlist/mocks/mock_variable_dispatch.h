/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/mutable_cmdlist/variable_dispatch.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::MCL::VariableDispatch>
    : public ::L0::MCL::VariableDispatch {

    using BaseClass = ::L0::MCL::VariableDispatch;
    using BaseClass::alignedSlmSize;
    using BaseClass::calculateRegion;
    using BaseClass::cleanCommitVariableDispatch;
    using BaseClass::cmdListEngineType;
    using BaseClass::commitGroupCount;
    using BaseClass::commitGroupSize;
    using BaseClass::commitSlmSize;
    using BaseClass::doCommitVariableDispatch;
    using BaseClass::generateLocalIds;
    using BaseClass::globalOffset;
    using BaseClass::globalOffsetVar;
    using BaseClass::grfSize;
    using BaseClass::groupCount;
    using BaseClass::groupCountVar;
    using BaseClass::groupSize;
    using BaseClass::groupSizeVar;
    using BaseClass::indirectData;
    using BaseClass::isCooperative;
    using BaseClass::isSlmKernel;
    using BaseClass::kernelDispatch;
    using BaseClass::lastSlmArgumentVar;
    using BaseClass::localIdGenerationByRuntime;
    using BaseClass::localRegionSize;
    using BaseClass::maxCooperativeGroupCount;
    using BaseClass::maxWgCountPerTile;
    using BaseClass::mutableCommandWalker;
    using BaseClass::numChannels;
    using BaseClass::numThreadsPerThreadGroup;
    using BaseClass::partitionCount;
    using BaseClass::perThreadData;
    using BaseClass::perThreadDataAllocSize;
    using BaseClass::perThreadDataSize;
    using BaseClass::regionBarrierOffset;
    using BaseClass::requiredDispatchWalkOrder;
    using BaseClass::requiredPartitionDim;
    using BaseClass::requiresLocalIdGeneration;
    using BaseClass::setGws;
    using BaseClass::setWorkDim;
    using BaseClass::slmTotalSize;
    using BaseClass::syncBufferOffset;
    using BaseClass::threadExecutionMask;
    using BaseClass::threadGroupCount;
    using BaseClass::totalLwsSize;
    using BaseClass::walkOrder;

    WhiteBox(::L0::MCL::KernelDispatch *kernelDispatch,
             std::unique_ptr<::L0::MCL::MutableIndirectData> mutableIndirectData,
             ::L0::MCL::MutableComputeWalker *mutableCommandWalker,
             ::L0::MCL::Variable *groupSizeVariable,
             ::L0::MCL::Variable *groupCountVariable,
             ::L0::MCL::Variable *globalOffsetVariable,
             ::L0::MCL::Variable *lastSlmArgumentVariable,
             uint32_t grfSize,
             const ::L0::MCL::MutableKernelDispatchParameters &dispatchParams,
             uint32_t partitionCount,
             NEO::EngineGroupType cmdListEngineType,
             bool calculateRegion) : ::L0::MCL::VariableDispatch(kernelDispatch,
                                                                 std::move(mutableIndirectData),
                                                                 mutableCommandWalker,
                                                                 groupSizeVariable,
                                                                 groupCountVariable,
                                                                 globalOffsetVariable,
                                                                 lastSlmArgumentVariable,
                                                                 grfSize,
                                                                 dispatchParams,
                                                                 partitionCount,
                                                                 cmdListEngineType,
                                                                 calculateRegion) {}
};

using VariableDispatch = WhiteBox<::L0::MCL::VariableDispatch>;

} // namespace ult
} // namespace L0
