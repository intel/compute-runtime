/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/utilities/arrayref.h"

#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"
#include "level_zero/core/source/mutable_cmdlist/variable_dispatch.h"

#include <memory>
#include <string>

namespace NEO {
class GraphicsAllocation;
} // namespace NEO

namespace L0 {
struct Module;
} // namespace L0

namespace L0::MCL {

struct KernelData {
    using LocalInstructionsOffset = uint16_t;
    std::string kernelName;
    ArrayRef<const uint8_t> kernelIsa;

    GpuAddress kernelStartAddress = undefined<GpuAddress>;
    InstructionsOffset kernelStartOffset = undefined<InstructionsOffset>;

    const L0::Module *module = nullptr;
    size_t isaOffsetWithinAllocation = 0;

    uint32_t grfCount = 0;

    LocalInstructionsOffset skipPerThreadDataLoad = undefined<LocalInstructionsOffset>;
    CrossThreadDataOffset syncBufferAddressOffset = undefined<CrossThreadDataOffset>;
    CrossThreadDataOffset regionGroupBarrierBufferOffset = undefined<CrossThreadDataOffset>;

    uint8_t workgroupWalkOrder[3] = {0, 0, 0};
    uint8_t simdSize = 0;
    uint8_t requiredThreadGroupDispatchSize = 0;
    uint8_t indirectOffset = 0;
    uint8_t barrierCount = 0;
    uint8_t syncBufferPointerSize = 0;
    uint8_t regionGroupBarrierBufferPointerSize = 0;
    uint8_t numLocalIdChannels = 3;

    bool passInlineData = false;
    bool requiresWorkgroupWalkOrder = false;
    bool usesSyncBuffer = false;
    bool usesRegionGroupBarrier = false;
};

struct KernelDispatchOffsets {
    IndirectObjectHeapOffset crossThreadOffset = undefined<IndirectObjectHeapOffset>;
    IndirectObjectHeapOffset perThreadOffset = undefined<IndirectObjectHeapOffset>;
    SurfaceStateHeapOffset sshOffset = undefined<SurfaceStateHeapOffset>;
    CommandBufferOffset walkerCmdOffset = undefined<CommandBufferOffset>;
    InstructionsOffset kernelStartOffset = undefined<InstructionsOffset>;
    LocalSshOffset btOffset = undefined<LocalSshOffset>;
};

struct KernelDispatch {
    KernelDispatchOffsets offsets;
    KernelData *kernelData;
    std::unique_ptr<VariableDispatch> varDispatch;
    ArrayRef<const uint8_t> indirectObjectHeap = {};
    size_t surfaceStateHeapSize;
    void *walkerCmd = nullptr;

    NEO::GraphicsAllocation *syncBuffer = nullptr;
    size_t syncBufferSize = 0;
    size_t syncBufferNoopPatchIndex = undefined<size_t>;
    NEO::GraphicsAllocation *regionBarrier = nullptr;
    size_t regionBarrierSize = 0;
    size_t regionBarrierNoopPatchIndex = undefined<size_t>;

    uint32_t slmTotalSize = 0;
    uint32_t slmInlineSize = 0;
    uint32_t slmPolicy = 0;
};

struct MutableKernelDispatchParameters {
    const uint32_t *groupCount = nullptr;
    const uint32_t *groupSize = nullptr;
    const uint32_t *globalOffset = nullptr;
    size_t perThreadSize = 0;
    uint32_t walkOrder = 0;
    uint32_t numThreadsPerThreadGroup = 0;
    uint32_t threadExecutionMask = 0;
    uint32_t maxWorkGroupCountPerTile = 0;
    uint32_t maxCooperativeGroupCount = 0;
    uint32_t localRegionSize = NEO::localRegionSizeParamNotSet;
    NEO::RequiredPartitionDim requiredPartitionDim = NEO::RequiredPartitionDim::none;
    NEO::RequiredDispatchWalkOrder requiredDispatchWalkOrder = NEO::RequiredDispatchWalkOrder::none;
    bool generationOfLocalIdsByRuntime = false;
    bool cooperativeDispatch = false;
};

} // namespace L0::MCL
