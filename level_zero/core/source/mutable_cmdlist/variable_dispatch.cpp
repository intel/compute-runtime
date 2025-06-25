/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/mutable_cmdlist/variable_dispatch.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_walk_order.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/helpers/local_work_size.h"
#include "shared/source/helpers/per_thread_data.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/program/sync_buffer_handler.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_imp.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_dispatch.h"
#include "level_zero/core/source/mutable_cmdlist/variable.h"

namespace L0::MCL {
VariableDispatch::VariableDispatch(KernelDispatch *kernelDispatch,
                                   std::unique_ptr<MutableIndirectData> mutableIndirectData, MutableComputeWalker *mutableCommandWalker,
                                   Variable *groupSizeVariable, Variable *groupCountVariable, Variable *globalOffsetVariable, Variable *lastSlmArgumentVariable,
                                   uint32_t grfSize, const MutableKernelDispatchParameters &dispatchParams, uint32_t partitionCount,
                                   NEO::EngineGroupType cmdListEngineType, bool calculateRegion)
    : kernelDispatch(kernelDispatch),
      indirectData(std::move(mutableIndirectData)), mutableCommandWalker(mutableCommandWalker),
      groupSizeVar(groupSizeVariable), groupCountVar(groupCountVariable), globalOffsetVar(globalOffsetVariable), lastSlmArgumentVar(lastSlmArgumentVariable),
      perThreadDataSize(dispatchParams.perThreadSize),
      grfSize(grfSize), walkOrder(dispatchParams.walkOrder), numThreadsPerThreadGroup(dispatchParams.numThreadsPerThreadGroup),
      threadExecutionMask(dispatchParams.threadExecutionMask), maxWgCountPerTile(dispatchParams.maxWorkGroupCountPerTile), maxCooperativeGroupCount(dispatchParams.maxCooperativeGroupCount),
      localRegionSize(dispatchParams.localRegionSize), requiredPartitionDim(dispatchParams.requiredPartitionDim), requiredDispatchWalkOrder(dispatchParams.requiredDispatchWalkOrder),
      partitionCount(partitionCount), cmdListEngineType(cmdListEngineType),
      localIdGenerationByRuntime(dispatchParams.generationOfLocalIdsByRuntime), calculateRegion(calculateRegion), isCooperative(dispatchParams.cooperativeDispatch) {

    if (groupSizeVar != nullptr) {
        groupSizeVar->getDesc().size = 3 * sizeof(uint32_t);
        groupSizeVar->addDispatch(this);
        groupSizeVar->setGroupSizeProperty(dispatchParams.groupSize);
    }

    if (groupCountVar != nullptr) {
        groupCountVar->getDesc().size = 3 * sizeof(uint32_t);
        groupCountVar->addDispatch(this);
        groupCountVar->setGroupCountProperty(dispatchParams.groupCount);

        // check if kernel uses sync buffer or region group barrier and save currently used offset within allocation
        // only append kernel from kernel group (ISA mutation) can have noop patch index set at VariableDispatch ctor
        if (kernelDispatch->syncBufferNoopPatchIndex != undefined<size_t>) {
            void *noopPtr = nullptr;
            size_t noopSize = 0;
            groupCountVar->fillCmdListNoopPatchData(kernelDispatch->syncBufferNoopPatchIndex, noopPtr, noopSize, this->syncBufferOffset);
            UNRECOVERABLE_IF(noopSize != kernelDispatch->syncBufferSize);
        }
        if (kernelDispatch->regionBarrierNoopPatchIndex != undefined<size_t>) {
            void *noopPtr = nullptr;
            size_t noopSize = 0;
            groupCountVar->fillCmdListNoopPatchData(kernelDispatch->regionBarrierNoopPatchIndex, noopPtr, noopSize, this->regionBarrierOffset);
            UNRECOVERABLE_IF(noopSize != kernelDispatch->regionBarrierSize);
        }
    }

    if (globalOffsetVar != nullptr) {
        globalOffsetVar->getDesc().size = 3 * sizeof(uint32_t);
        globalOffsetVar->addDispatch(this);
        globalOffsetVar->setGlobalOffsetProperty(dispatchParams.globalOffset);
    }

    if (lastSlmArgumentVar != nullptr) {
        lastSlmArgumentVar->addDispatch(this);
    }

    for (auto dimension = 0u; dimension < numChannels; dimension++) {
        this->groupSize[dimension] = dispatchParams.groupSize[dimension];
        this->groupCount[dimension] = dispatchParams.groupCount[dimension];
        this->globalOffset[dimension] = dispatchParams.globalOffset[dimension];

        this->totalLwsSize *= this->groupSize[dimension];
        this->threadGroupCount *= this->groupCount[dimension];
    }
    this->slmTotalSize = kernelDispatch->slmTotalSize;
    if ((this->isCooperative || this->calculateRegion) && this->slmTotalSize > 0) {
        Variable *var = groupSizeVar != nullptr ? groupSizeVar : lastSlmArgumentVar != nullptr ? lastSlmArgumentVar
                                                                                               : nullptr;
        if (var != nullptr) {
            this->alignedSlmSize = var->getAlignedSlmSize(this->slmTotalSize);
        }
    }

    // SLM kernel is when SLM arguments or SLM inline is present
    this->isSlmKernel = this->lastSlmArgumentVar != nullptr || this->kernelDispatch->slmInlineSize > 0;
}

void VariableDispatch::setGroupSize(const uint32_t groupSize[3], NEO::Device &device, bool stageData) {
    this->totalLwsSize = 1u;

    for (auto dimension = 0u; dimension < 3; dimension++) {
        this->groupSize[dimension] = groupSize[dimension];
        this->totalLwsSize *= groupSize[dimension];
    }

    this->localIdGenerationByRuntime = requiresLocalIdGeneration(this->totalLwsSize, this->walkOrder, device.getRootDeviceEnvironment());
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL mutation group size variable %p new value %u %u %u totalLws %zu SW generated %d\n",
                       groupSizeVar,
                       groupSize[0], groupSize[1], groupSize[2],
                       this->totalLwsSize,
                       this->localIdGenerationByRuntime);

    if (this->localIdGenerationByRuntime) {
        generateLocalIds(this->totalLwsSize, device.getRootDeviceEnvironment());
        indirectData->setPerThreadData(ArrayRef<const uint8_t>::fromAny(perThreadData.get(), this->perThreadDataSize));
    } else {
        this->perThreadDataSize = 0;
    }

    const auto simdSize = kernelDispatch->kernelData->simdSize;
    this->numThreadsPerThreadGroup = static_cast<uint32_t>((this->totalLwsSize + simdSize - 1u) / simdSize);
    auto remainderSimdLanes = this->totalLwsSize & (simdSize - 1u);
    this->threadExecutionMask = static_cast<uint32_t>(maxNBitValue(remainderSimdLanes));
    if (!this->threadExecutionMask) {
        this->threadExecutionMask = static_cast<uint32_t>(maxNBitValue((isSimd1(simdSize)) ? 32 : simdSize));
    }

    if (this->isCooperative) {
        const uint32_t workDim = 3;
        const size_t localWorkSize[] = {groupSize[0], groupSize[1], groupSize[2]};
        this->maxCooperativeGroupCount = NEO::KernelHelper::getMaxWorkGroupCount(device,
                                                                                 kernelDispatch->kernelData->grfCount,
                                                                                 kernelDispatch->kernelData->simdSize,
                                                                                 kernelDispatch->kernelData->barrierCount,
                                                                                 this->alignedSlmSize,
                                                                                 workDim,
                                                                                 localWorkSize,
                                                                                 this->cmdListEngineType,
                                                                                 this->partitionCount > 1,
                                                                                 false);
    }

    if (stageData) {
        this->commitGroupSize = true;
        return;
    }

    if (this->calculateRegion) {
        constexpr uint32_t singleTile = 1;
        const size_t localWorkSizes[3] = {this->groupSize[0], this->groupSize[1], this->groupSize[2]};
        this->maxWgCountPerTile = NEO::KernelHelper::getMaxWorkGroupCount(device.getRootDeviceEnvironment(), kernelDispatch->kernelData->grfCount,
                                                                          kernelDispatch->kernelData->simdSize, kernelDispatch->kernelData->barrierCount, singleTile,
                                                                          this->alignedSlmSize, numChannels,
                                                                          localWorkSizes, this->cmdListEngineType);
    }

    if (this->localIdGenerationByRuntime || kernelDispatch->kernelData->numLocalIdChannels == 0) {
        mutableCommandWalker->setGenerateLocalId(false, 0, kernelDispatch->kernelData->numLocalIdChannels);
    } else {
        mutableCommandWalker->setGenerateLocalId(true, this->walkOrder, kernelDispatch->kernelData->numLocalIdChannels);
        mutableCommandWalker->setWorkGroupSize(this->groupSize);
    }

    size_t indirectDataLength = indirectData->getCrossThreadDataSize() + this->perThreadDataSize;
    mutableCommandWalker->setIndirectDataSize(indirectDataLength);

    auto kernelStartAddress = kernelDispatch->kernelData->kernelStartAddress;
    if (false == localIdGenerationByRuntime) {
        kernelStartAddress += kernelDispatch->kernelData->skipPerThreadDataLoad;
    }
    mutableCommandWalker->setKernelStartAddress(kernelStartAddress);

    indirectData->setLocalWorkSize(this->groupSize);
    indirectData->setLocalWorkSize2(this->groupSize);
    indirectData->setEnqLocalWorkSize(this->groupSize);
    setGws();
    setWorkDim();

    mutableCommandWalker->setNumberThreadsPerThreadGroup(this->numThreadsPerThreadGroup);
    mutableCommandWalker->setExecutionMask(this->threadExecutionMask);

    constexpr bool updateGroupCount = false;
    constexpr bool updateGroupSize = true;
    constexpr bool updateSlm = false;
    MutableWalkerSpecificFieldsArguments args{
        .threadGroupDimensions = this->groupCount.data(),
        .threadGroupCount = this->threadGroupCount,
        .requiredThreadGroupDispatchSize = kernelDispatch->kernelData->requiredThreadGroupDispatchSize,
        .grfCount = kernelDispatch->kernelData->grfCount,
        .threadsPerThreadGroup = this->numThreadsPerThreadGroup,
        .totalWorkGroupSize = static_cast<uint32_t>(this->totalLwsSize),
        .slmTotalSize = this->slmTotalSize,
        .slmPolicy = kernelDispatch->slmPolicy,
        .partitionCount = this->partitionCount,
        .maxWgCountPerTile = this->maxWgCountPerTile,
        .requiredPartitionDim = this->requiredPartitionDim,
        .isRequiredDispatchWorkGroupOrder = this->requiredDispatchWalkOrder != NEO::RequiredDispatchWalkOrder::none,
        .isSlmKernel = this->isSlmKernel,
        .cooperativeKernel = this->isCooperative,
        .updateGroupCount = updateGroupCount,
        .updateGroupSize = updateGroupSize,
        .updateSlm = updateSlm};
    mutableCommandWalker->updateSpecificFields(device, args);
}

void VariableDispatch::setGroupCount(const uint32_t groupCount[3], const NEO::Device &device, bool stageData) {
    this->threadGroupCount = 1;

    for (auto dimension = 0u; dimension < 3; dimension++) {
        this->groupCount[dimension] = groupCount[dimension];
        this->threadGroupCount *= this->groupCount[dimension];
    }

    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL mutation group count variable %p new value %u %u %u\n", groupCountVar, groupCount[0], groupCount[1], groupCount[2]);

    if (kernelDispatch->kernelData->usesSyncBuffer) {
        auto newSize = NEO::KernelHelper::getSyncBufferSize(this->threadGroupCount);
        if (newSize > kernelDispatch->syncBufferSize) {
            auto syncBufferPair = device.syncBufferHandler->obtainAllocationAndOffset(newSize);
            uint64_t newAddress = syncBufferPair.first->getGpuAddressToPatch() + syncBufferPair.second;
            indirectData->setAddress(kernelDispatch->kernelData->syncBufferAddressOffset,
                                     newAddress,
                                     kernelDispatch->kernelData->syncBufferPointerSize);

            groupCountVar->updateAllocationResidency(kernelDispatch->syncBuffer, syncBufferPair.first);

            kernelDispatch->syncBufferSize = newSize;
            kernelDispatch->syncBuffer = syncBufferPair.first;
            this->syncBufferOffset = syncBufferPair.second;

            void *newCpuPtr = ptrOffset(kernelDispatch->syncBuffer->getUnderlyingBuffer(), this->syncBufferOffset);
            if (kernelDispatch->syncBufferNoopPatchIndex == undefined<size_t>) {
                kernelDispatch->syncBufferNoopPatchIndex = groupCountVar->createNewCmdListNoopPatchData(newCpuPtr, newSize, this->syncBufferOffset);
            } else {
                groupCountVar->updateCmdListNoopPatchData(kernelDispatch->syncBufferNoopPatchIndex, newCpuPtr, newSize, this->syncBufferOffset);
            }
        } else {
            // mutation of kernels - check noop patch needs update and repatch the sync buffer address
            // if new size is not bigger (else branch - here), then buffer must exists and so noop patch must exists
            // but noop patch could be reused by other kernel, so at group count mutation must check and update noop patch
            UNRECOVERABLE_IF(kernelDispatch->syncBufferNoopPatchIndex == undefined<size_t>);

            void *noopCpuPtr = nullptr;
            size_t noopSize = 0;
            size_t noopOffset = 0;
            groupCountVar->fillCmdListNoopPatchData(kernelDispatch->syncBufferNoopPatchIndex, noopCpuPtr, noopSize, noopOffset);

            void *currentNoopPtr = ptrOffset(kernelDispatch->syncBuffer->getUnderlyingBuffer(), this->syncBufferOffset);

            if (noopSize != kernelDispatch->syncBufferSize || noopOffset != this->syncBufferOffset || noopCpuPtr != currentNoopPtr) {
                uint64_t newAddress = kernelDispatch->syncBuffer->getGpuAddressToPatch() + this->syncBufferOffset;
                indirectData->setAddress(kernelDispatch->kernelData->syncBufferAddressOffset,
                                         newAddress,
                                         kernelDispatch->kernelData->syncBufferPointerSize);

                groupCountVar->updateCmdListNoopPatchData(kernelDispatch->syncBufferNoopPatchIndex, currentNoopPtr, kernelDispatch->syncBufferSize, this->syncBufferOffset);
            }
        }
    }

    if (kernelDispatch->kernelData->usesRegionGroupBarrier) {
        auto newSize = NEO::KernelHelper::getRegionGroupBarrierSize(this->threadGroupCount, this->localRegionSize);
        if (newSize > kernelDispatch->regionBarrierSize) {
            auto regionBarrierPair = device.syncBufferHandler->obtainAllocationAndOffset(newSize);
            uint64_t newAddress = regionBarrierPair.first->getGpuAddressToPatch() + regionBarrierPair.second;
            indirectData->setAddress(kernelDispatch->kernelData->regionGroupBarrierBufferOffset,
                                     newAddress,
                                     kernelDispatch->kernelData->regionGroupBarrierBufferPointerSize);

            groupCountVar->updateAllocationResidency(kernelDispatch->regionBarrier, regionBarrierPair.first);

            kernelDispatch->regionBarrierSize = newSize;
            kernelDispatch->regionBarrier = regionBarrierPair.first;
            this->regionBarrierOffset = regionBarrierPair.second;

            void *newCpuPtr = ptrOffset(kernelDispatch->regionBarrier->getUnderlyingBuffer(), this->regionBarrierOffset);
            if (kernelDispatch->regionBarrierNoopPatchIndex == undefined<size_t>) {
                kernelDispatch->regionBarrierNoopPatchIndex = groupCountVar->createNewCmdListNoopPatchData(newCpuPtr, newSize, this->regionBarrierOffset);
            } else {
                groupCountVar->updateCmdListNoopPatchData(kernelDispatch->regionBarrierNoopPatchIndex, newCpuPtr, newSize, this->regionBarrierOffset);
            }
        } else {
            // mutation of kernels - check noop patch needs update and repatch the region barrier address
            // if new size is not bigger (else branch - here), then region barrier must exists and so noop patch must exists
            // but noop patch could be reused by other kernel, so at group count mutation must check and update noop patch
            UNRECOVERABLE_IF(kernelDispatch->regionBarrierNoopPatchIndex == undefined<size_t>);

            void *noopCpuPtr = nullptr;
            size_t noopSize = 0;
            size_t noopOffset = 0;
            groupCountVar->fillCmdListNoopPatchData(kernelDispatch->regionBarrierNoopPatchIndex, noopCpuPtr, noopSize, noopOffset);

            void *currentNoopPtr = ptrOffset(kernelDispatch->regionBarrier->getUnderlyingBuffer(), this->regionBarrierOffset);

            if (noopSize != kernelDispatch->regionBarrierSize || noopOffset != this->regionBarrierOffset || noopCpuPtr != currentNoopPtr) {
                uint64_t newAddress = kernelDispatch->regionBarrier->getGpuAddressToPatch() + this->regionBarrierOffset;
                indirectData->setAddress(kernelDispatch->kernelData->regionGroupBarrierBufferOffset,
                                         newAddress,
                                         kernelDispatch->kernelData->regionGroupBarrierBufferPointerSize);

                groupCountVar->updateCmdListNoopPatchData(kernelDispatch->regionBarrierNoopPatchIndex, currentNoopPtr, kernelDispatch->regionBarrierSize, this->regionBarrierOffset);
            }
        }
    }

    if (stageData) {
        this->commitGroupCount = true;
        return;
    }

    mutableCommandWalker->setNumberWorkGroups(this->groupCount);

    constexpr bool updateGroupCount = true;
    constexpr bool updateGroupSize = false;
    constexpr bool updateSlm = false;
    MutableWalkerSpecificFieldsArguments args{
        .threadGroupDimensions = this->groupCount.data(),
        .threadGroupCount = this->threadGroupCount,
        .requiredThreadGroupDispatchSize = kernelDispatch->kernelData->requiredThreadGroupDispatchSize,
        .grfCount = kernelDispatch->kernelData->grfCount,
        .threadsPerThreadGroup = this->numThreadsPerThreadGroup,
        .totalWorkGroupSize = static_cast<uint32_t>(this->totalLwsSize),
        .slmTotalSize = this->slmTotalSize,
        .slmPolicy = kernelDispatch->slmPolicy,
        .partitionCount = this->partitionCount,
        .maxWgCountPerTile = this->maxWgCountPerTile,
        .requiredPartitionDim = this->requiredPartitionDim,
        .isRequiredDispatchWorkGroupOrder = this->requiredDispatchWalkOrder != NEO::RequiredDispatchWalkOrder::none,
        .isSlmKernel = this->isSlmKernel,
        .cooperativeKernel = this->isCooperative,
        .updateGroupCount = updateGroupCount,
        .updateGroupSize = updateGroupSize,
        .updateSlm = updateSlm};
    mutableCommandWalker->updateSpecificFields(device, args);

    setGws();
    setWorkDim();
    indirectData->setNumWorkGroups(this->groupCount);
}

void VariableDispatch::setGlobalOffset(const uint32_t globalOffset[3]) {
    for (int i = 0; i < 3; i++) {
        this->globalOffset[i] = globalOffset[i];
    }

    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL mutation global offset variable %p new value %u %u %u\n", globalOffsetVar, globalOffset[0], globalOffset[1], globalOffset[2]);

    indirectData->setGlobalWorkOffset(this->globalOffset);
}

void VariableDispatch::setGws() {
    std::array<uint32_t, 3> gws;
    for (auto dimension = 0u; dimension < numChannels; dimension++) {
        gws[dimension] = groupSize[dimension] * groupCount[dimension];
    }
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL mutation set gws %u %u %u\n", gws[0], gws[1], gws[2]);
    indirectData->setGlobalWorkSize(gws);
}

void VariableDispatch::setWorkDim() {
    uint32_t workDimensions = 1;
    if (groupCount[2] * groupSize[2] > 1) {
        workDimensions = 3;
    } else if (groupCount[1] * groupSize[1] > 1) {
        workDimensions = 2;
    }
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL mutation set work dim %u\n", workDimensions);
    indirectData->setWorkDimensions(workDimensions);
}

bool VariableDispatch::requiresLocalIdGeneration(size_t localWorkSize, uint32_t &outWalkOrder, const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    size_t localWorkSizes[3] = {this->groupSize[0], this->groupSize[1], this->groupSize[2]};
    std::array<uint8_t, 3> kernelWalkOrder{
        kernelDispatch->kernelData->workgroupWalkOrder[0],
        kernelDispatch->kernelData->workgroupWalkOrder[1],
        kernelDispatch->kernelData->workgroupWalkOrder[2]};

    bool returnCode = rootDeviceEnvironment.getHelper<NEO::GfxCoreHelper>().isRuntimeLocalIdsGenerationRequired(kernelDispatch->kernelData->numLocalIdChannels, localWorkSizes, kernelWalkOrder,
                                                                                                                kernelDispatch->kernelData->requiresWorkgroupWalkOrder,
                                                                                                                outWalkOrder, kernelDispatch->kernelData->simdSize);

    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL local id generation - requiresWalkOrder %d %" PRIu8 " %" PRIu8 " %" PRIu8 " simd %" PRIu8 " outWalkOrder %u num channels\n",
                       kernelDispatch->kernelData->requiresWorkgroupWalkOrder, kernelWalkOrder[0], kernelWalkOrder[1], kernelWalkOrder[2], kernelDispatch->kernelData->simdSize, outWalkOrder, numChannels);
    return returnCode;
}

void VariableDispatch::generateLocalIds(size_t localWorkSize, const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    const auto simdSize = kernelDispatch->kernelData->simdSize;
    auto grfCount = kernelDispatch->kernelData->grfCount;
    perThreadDataSize = NEO::PerThreadDataHelper::getPerThreadDataSizeTotal(simdSize, grfSize, grfCount, numChannels, localWorkSize, rootDeviceEnvironment);
    if (perThreadDataSize > perThreadDataAllocSize) {
        this->perThreadData.reset();
        this->perThreadData = std::unique_ptr<uint8_t, decltype(&alignedFree)>(reinterpret_cast<uint8_t *>(alignedMalloc(perThreadDataSize, 32)), &alignedFree);
        perThreadDataAllocSize = perThreadDataSize;
    }
    memset(perThreadData.get(), 0, perThreadDataSize);

    std::array<uint8_t, 3> walkOrder{0, 1, 2};
    if (kernelDispatch->kernelData->requiresWorkgroupWalkOrder) {
        walkOrder = {kernelDispatch->kernelData->workgroupWalkOrder[0],
                     kernelDispatch->kernelData->workgroupWalkOrder[1],
                     kernelDispatch->kernelData->workgroupWalkOrder[2]};
    }

    NEO::generateLocalIDs(
        perThreadData.get(),
        static_cast<uint16_t>(simdSize),
        std::array<uint16_t, 3>{static_cast<uint16_t>(groupSize[0]),
                                static_cast<uint16_t>(groupSize[1]),
                                static_cast<uint16_t>(groupSize[2])},
        walkOrder,
        false, grfSize, grfCount, rootDeviceEnvironment);
}

Variable *VariableDispatch::getGroupSizeVar() const { return groupSizeVar; }

Variable *VariableDispatch::getGroupCountVar() const { return groupCountVar; }

Variable *VariableDispatch::getGlobalOffsetVar() const { return globalOffsetVar; }

Variable *VariableDispatch::getLastSlmArgumentVariable() const { return lastSlmArgumentVar; }

const MutableIndirectData::Offsets &VariableDispatch::getIndirectDataOffsets() const { return indirectData->getIndirectDataOffsets(); }

void VariableDispatch::setSlmSize(const uint32_t slmArgTotalSize, NEO::Device &device, bool stageData) {
    this->slmTotalSize = slmArgTotalSize + kernelDispatch->slmInlineSize;
    if (this->isCooperative || this->calculateRegion) {
        this->alignedSlmSize = device.getGfxCoreHelper().alignSlmSize(this->slmTotalSize);
    }

    if (this->isCooperative) {
        const uint32_t workDim = 3;
        const size_t localWorkSize[] = {this->groupSize[0], this->groupSize[1], this->groupSize[2]};
        this->maxCooperativeGroupCount = NEO::KernelHelper::getMaxWorkGroupCount(device,
                                                                                 kernelDispatch->kernelData->grfCount,
                                                                                 kernelDispatch->kernelData->simdSize,
                                                                                 kernelDispatch->kernelData->barrierCount,
                                                                                 this->alignedSlmSize,
                                                                                 workDim,
                                                                                 localWorkSize,
                                                                                 this->cmdListEngineType,
                                                                                 this->partitionCount > 1,
                                                                                 false);
    }

    if (stageData) {
        this->commitSlmSize = true;
        return;
    }

    if (this->calculateRegion) {
        constexpr uint32_t singleTile = 1;
        const size_t localWorkSizes[3] = {this->groupSize[0], this->groupSize[1], this->groupSize[2]};
        this->maxWgCountPerTile = NEO::KernelHelper::getMaxWorkGroupCount(device.getRootDeviceEnvironment(), kernelDispatch->kernelData->grfCount,
                                                                          kernelDispatch->kernelData->simdSize, kernelDispatch->kernelData->barrierCount, singleTile,
                                                                          this->alignedSlmSize, numChannels,
                                                                          localWorkSizes, this->cmdListEngineType);
    }

    mutableCommandWalker->updateSlmSize(device, this->slmTotalSize);
    kernelDispatch->slmTotalSize = this->slmTotalSize;

    // preferred slm size
    constexpr bool updateGroupCount = false;
    constexpr bool updateGroupSize = false;
    constexpr bool updateSlm = true;
    MutableWalkerSpecificFieldsArguments args{
        .threadGroupDimensions = this->groupCount.data(),
        .threadGroupCount = this->threadGroupCount,
        .requiredThreadGroupDispatchSize = kernelDispatch->kernelData->requiredThreadGroupDispatchSize,
        .grfCount = kernelDispatch->kernelData->grfCount,
        .threadsPerThreadGroup = this->numThreadsPerThreadGroup,
        .totalWorkGroupSize = static_cast<uint32_t>(this->totalLwsSize),
        .slmTotalSize = this->slmTotalSize,
        .slmPolicy = kernelDispatch->slmPolicy,
        .partitionCount = this->partitionCount,
        .maxWgCountPerTile = this->maxWgCountPerTile,
        .requiredPartitionDim = this->requiredPartitionDim,
        .isRequiredDispatchWorkGroupOrder = this->requiredDispatchWalkOrder != NEO::RequiredDispatchWalkOrder::none,
        .isSlmKernel = this->isSlmKernel,
        .cooperativeKernel = this->isCooperative,
        .updateGroupCount = updateGroupCount,
        .updateGroupSize = updateGroupSize,
        .updateSlm = updateSlm};
    mutableCommandWalker->updateSpecificFields(device, args);
}

void VariableDispatch::commitChanges(const NEO::Device &device) {
    if (!this->doCommitVariableDispatch()) {
        return;
    }

    if (this->commitGroupSize) {
        if (this->localIdGenerationByRuntime || kernelDispatch->kernelData->numLocalIdChannels == 0) {
            mutableCommandWalker->setGenerateLocalId(false, 0, kernelDispatch->kernelData->numLocalIdChannels);
        } else {
            mutableCommandWalker->setGenerateLocalId(true, this->walkOrder, kernelDispatch->kernelData->numLocalIdChannels);
            mutableCommandWalker->setWorkGroupSize(this->groupSize);
        }

        size_t indirectDataLength = indirectData->getCrossThreadDataSize() + this->perThreadDataSize;
        mutableCommandWalker->setIndirectDataSize(indirectDataLength);

        auto kernelStartAddress = kernelDispatch->kernelData->kernelStartAddress;
        if (false == this->localIdGenerationByRuntime) {
            kernelStartAddress += kernelDispatch->kernelData->skipPerThreadDataLoad;
        }
        mutableCommandWalker->setKernelStartAddress(kernelStartAddress);
        indirectData->setLocalWorkSize(this->groupSize);
        indirectData->setLocalWorkSize2(this->groupSize);
        indirectData->setEnqLocalWorkSize(this->groupSize);

        mutableCommandWalker->setNumberThreadsPerThreadGroup(this->numThreadsPerThreadGroup);
        mutableCommandWalker->setExecutionMask(this->threadExecutionMask);
    }

    if (this->commitGroupCount) {
        indirectData->setNumWorkGroups(this->groupCount);
        mutableCommandWalker->setNumberWorkGroups(this->groupCount);
    }

    if (this->commitGroupCount || this->commitGroupSize) {
        setGws();
        setWorkDim();
    }

    if (this->commitSlmSize) {
        mutableCommandWalker->updateSlmSize(device, this->slmTotalSize);
        kernelDispatch->slmTotalSize = this->slmTotalSize;
    }

    if (this->commitGroupSize || this->commitSlmSize) {
        if (this->calculateRegion) {
            constexpr uint32_t singleTile = 1;
            const size_t localWorkSizes[3] = {this->groupSize[0], this->groupSize[1], this->groupSize[2]};
            this->maxWgCountPerTile = NEO::KernelHelper::getMaxWorkGroupCount(device.getRootDeviceEnvironment(), kernelDispatch->kernelData->grfCount,
                                                                              kernelDispatch->kernelData->simdSize, kernelDispatch->kernelData->barrierCount, singleTile,
                                                                              this->alignedSlmSize, numChannels,
                                                                              localWorkSizes, this->cmdListEngineType);
        }
    }

    MutableWalkerSpecificFieldsArguments args{
        .threadGroupDimensions = this->groupCount.data(),
        .threadGroupCount = this->threadGroupCount,
        .requiredThreadGroupDispatchSize = kernelDispatch->kernelData->requiredThreadGroupDispatchSize,
        .grfCount = kernelDispatch->kernelData->grfCount,
        .threadsPerThreadGroup = this->numThreadsPerThreadGroup,
        .totalWorkGroupSize = static_cast<uint32_t>(this->totalLwsSize),
        .slmTotalSize = this->slmTotalSize,
        .slmPolicy = kernelDispatch->slmPolicy,
        .partitionCount = this->partitionCount,
        .maxWgCountPerTile = this->maxWgCountPerTile,
        .requiredPartitionDim = this->requiredPartitionDim,
        .isRequiredDispatchWorkGroupOrder = this->requiredDispatchWalkOrder != NEO::RequiredDispatchWalkOrder::none,
        .isSlmKernel = this->isSlmKernel,
        .cooperativeKernel = this->isCooperative,
        .updateGroupCount = this->commitGroupCount,
        .updateGroupSize = this->commitGroupSize,
        .updateSlm = this->commitSlmSize};
    mutableCommandWalker->updateSpecificFields(device, args);

    mutableCommandWalker->saveCpuBufferIntoGpuBuffer(true);

    cleanCommitVariableDispatch();
}

} // namespace L0::MCL
