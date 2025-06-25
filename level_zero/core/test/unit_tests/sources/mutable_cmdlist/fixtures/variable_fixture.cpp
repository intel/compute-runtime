/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/variable_fixture.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/indirect_heap/indirect_heap.h"

namespace L0 {
namespace ult {

void VariableFixture::setUp(bool inOrder) {
    this->inOrder = inOrder;
    MutableCommandListFixtureInit::setUp(inOrder);
    this->qwordIndirect = this->mutableCommandList->isQwordInOrderCounter();

    this->grfSize = this->device->getHwInfo().capabilityTable.grfSize;
    this->perThreadDataSize = this->mutableCommandList->maxPerThreadDataSize;

    this->perThreadData = std::unique_ptr<uint8_t, decltype(&alignedFree)>(reinterpret_cast<uint8_t *>(alignedMalloc(this->perThreadDataSize, 32)), &alignedFree);
    this->crossThreadData = std::unique_ptr<uint8_t, decltype(&alignedFree)>(reinterpret_cast<uint8_t *>(alignedMalloc(this->crossThreadDataSize, 32)), &alignedFree);

    this->dispatchParams.groupCount = this->initialGroupCount;
    this->dispatchParams.groupSize = this->initialGroupSize;
    this->dispatchParams.globalOffset = this->initialGlobalOffset;
    this->dispatchParams.perThreadSize = this->perThreadDataSize;
    this->dispatchParams.numThreadsPerThreadGroup = 1;
    this->dispatchParams.threadExecutionMask = 1;

    auto &cmdlistContainer = this->mutableCommandList->getBase()->getCmdContainer();
    this->cmdBuffer = cmdlistContainer.getCommandStream();
    this->ioh = cmdlistContainer.getIndirectHeap(NEO::HeapType::indirectObject);

    this->cmdListBufferCpuBase = this->cmdBuffer->getCpuBase();
    this->cmdListIndirectCpuBase = this->ioh->getCpuBase();
    this->cmdBuffer->getSpace(VariableFixture::walkerBufferSize);
    this->walkerBuffer = this->cmdListBufferCpuBase;
    this->ioh->getSpace(VariableFixture::indirectBufferSize);

    auto &arg = this->kernelArgPtr.as<NEO::ArgDescPointer>();
    arg.stateless = defaultBufferStatelessOffset;

    auto &argValue = this->kernelArgValue.as<NEO::ArgDescValue>();
    NEO::ArgDescValue::Element element{};
    element.offset = defaultValueStatelessOffset;
    element.size = defaultValueStatelessSize;
    argValue.elements.push_back(element);

    this->kernelArgSlmFirst.getTraits().addressQualifier = NEO::KernelArgMetadata::AddressSpaceQualifier::AddrLocal;
    auto &argSlmFirst = this->kernelArgSlmFirst.as<NEO::ArgDescPointer>();
    argSlmFirst.requiredSlmAlignment = defaultSlmArgumentAlignment;
    argSlmFirst.slmOffset = defaultSlmFirstOffset;

    this->kernelArgSlmSecond.getTraits().addressQualifier = NEO::KernelArgMetadata::AddressSpaceQualifier::AddrLocal;
    auto &argSlmSecond = this->kernelArgSlmSecond.as<NEO::ArgDescPointer>();
    argSlmSecond.requiredSlmAlignment = defaultSlmArgumentAlignment;
    argSlmSecond.slmOffset = defaultSlmSecondOffset;
}

void VariableFixture::tearDown() {
    MutableCommandListFixtureInit::tearDown();
}

void VariableFixture::reAllocateCrossThread(size_t newSize) {
    this->crossThreadDataSize = newSize;
    this->crossThreadData = std::unique_ptr<uint8_t, decltype(&alignedFree)>(reinterpret_cast<uint8_t *>(alignedMalloc(newSize, 32)), &alignedFree);
}

void VariableFixture::createVariable(L0::MCL::VariableType varType, bool baseVariable, int32_t overrideStageCommit, int32_t overrideValueChunks) {
    bool variableSupportsStageCommit = false;
    if (varType == L0::MCL::VariableType::groupCount ||
        varType == L0::MCL::VariableType::groupSize ||
        varType == L0::MCL::VariableType::slmBuffer) {
        variableSupportsStageCommit = true;
    }
    if (overrideStageCommit != -1) {
        variableSupportsStageCommit = !!overrideStageCommit;
    }
    bool immediateValueChunks = false;
    if (varType == L0::MCL::VariableType::value) {
        immediateValueChunks = true;
    }
    if (overrideValueChunks != -1) {
        immediateValueChunks = !!overrideValueChunks;
    }
    variable = std::make_unique<Variable>();
    variable->cmdList = this->mutableCommandList.get();
    variable->isType(varType);
    variable->desc.immediateValueChunks = immediateValueChunks;
    variable->desc.isStageCommit = this->stageCommitMode && variableSupportsStageCommit;

    L0::MCL::IndirectObjectHeapOffset relativeIohOffset = 0;
    L0::MCL::CommandBufferOffset relativeWalkerOffset = 0;

    if (varType == L0::MCL::VariableType::buffer || varType == L0::MCL::VariableType::value || varType == L0::MCL::VariableType::slmBuffer) {
        initCommandBufferWithWalker();
    }

    if (varType == L0::MCL::VariableType::buffer) {
        variable->addKernelArgUsage(this->kernelArgPtr,
                                    relativeIohOffset, reinterpret_cast<L0::MCL::IndirectObjectHeapOffset>(this->cmdListIndirectCpuBase),
                                    undefined<L0::MCL::SurfaceStateHeapOffset>,
                                    0, 0,
                                    relativeWalkerOffset, this->mutableComputeWalker.get(),
                                    this->inlineData);
    }
    if (varType == L0::MCL::VariableType::value) {
        variable->addKernelArgUsage(this->kernelArgValue,
                                    relativeIohOffset, reinterpret_cast<L0::MCL::IndirectObjectHeapOffset>(this->cmdListIndirectCpuBase),
                                    undefined<L0::MCL::SurfaceStateHeapOffset>,
                                    0, 0,
                                    relativeWalkerOffset, this->mutableComputeWalker.get(),
                                    this->inlineData);
    }
    if (varType == L0::MCL::VariableType::slmBuffer) {
        NEO::ArgDescriptor &kernelArg = baseVariable ? this->kernelArgSlmFirst : this->kernelArgSlmSecond;
        variable->addKernelArgUsage(kernelArg,
                                    relativeIohOffset, reinterpret_cast<L0::MCL::IndirectObjectHeapOffset>(this->cmdListIndirectCpuBase),
                                    undefined<L0::MCL::SurfaceStateHeapOffset>,
                                    this->slmSize, this->slmOffset,
                                    relativeWalkerOffset, this->mutableComputeWalker.get(),
                                    this->inlineData);
    }
    if (!baseVariable) {
        variablesMap[varType] = std::move(variable);
    }
}

void VariableFixture::createVariableDispatch(bool useGroupCount, bool useGroupSize, bool useGlobalOffset, bool useSlm) {
    Variable *groupCount = nullptr;
    Variable *groupSize = nullptr;
    Variable *globalOffset = nullptr;
    Variable *lastSlm = nullptr;

    auto kernelData = this->mutableCommandList->getKernelData(this->kernel.get());
    this->kernelDispatch = std::make_unique<::L0::MCL::KernelDispatch>();
    this->kernelDispatch->kernelData = kernelData;
    this->kernelDispatch->slmInlineSize = this->slmInlineSize;
    this->kernelDispatch->slmTotalSize = this->slmInlineSize + this->slmSize;
    createMutableIndirectOffset();

    if (useGroupCount) {
        createVariable(L0::MCL::VariableType::groupCount, false, -1, -1);
        groupCount = getVariable(L0::MCL::VariableType::groupCount);
    }
    if (useGroupSize) {
        createVariable(L0::MCL::VariableType::groupSize, false, -1, -1);
        groupSize = getVariable(L0::MCL::VariableType::groupSize);
    }
    if (useGlobalOffset) {
        createVariable(L0::MCL::VariableType::globalOffset, false, -1, -1);
        globalOffset = getVariable(L0::MCL::VariableType::globalOffset);
    }
    if (useSlm) {
        createVariable(L0::MCL::VariableType::slmBuffer, false, -1, -1);
        lastSlm = getVariable(L0::MCL::VariableType::slmBuffer);
    }

    variableDispatch = std::make_unique<VariableDispatch>(kernelDispatch.get(),
                                                          std::move(this->indirectData),
                                                          this->mutableComputeWalker.get(),
                                                          groupCount,
                                                          groupSize,
                                                          globalOffset,
                                                          lastSlm,
                                                          this->grfSize,
                                                          this->dispatchParams,
                                                          this->partitionCount,
                                                          this->engineGroupType,
                                                          this->calculateRegion);
}

void VariableFixture::createMutableIndirectOffset() {
    auto indirectOffsets = std::make_unique<L0::MCL::MutableIndirectData::Offsets>();
    indirectOffsets->globalWorkSize = offsets.globalWorkSize;
    indirectOffsets->localWorkSize = offsets.localWorkSize;
    indirectOffsets->localWorkSize2 = offsets.localWorkSize2;
    indirectOffsets->enqLocalWorkSize = offsets.enqLocalWorkSize;
    indirectOffsets->numWorkGroups = offsets.numWorkGroups;
    indirectOffsets->workDimensions = offsets.workDimensions;
    indirectOffsets->globalWorkOffset = offsets.globalWorkOffset;

    ArrayRef<uint8_t> inlineDataRef;

    size_t crossThreadDataSize = this->crossThreadDataSize;
    if (this->inlineData) {
        crossThreadDataSize -= this->mutableComputeWalker->getInlineDataSize();
        inlineDataRef = {reinterpret_cast<uint8_t *>(this->mutableComputeWalker->getInlineDataPointer()), this->mutableComputeWalker->getInlineDataSize()};
    }
    ArrayRef<uint8_t> crossThreadDataRef = {this->crossThreadData.get(), crossThreadDataSize};

    ArrayRef<uint8_t> perThreadDataRef;
    if (this->usePerThread) {
        perThreadDataRef = {this->perThreadData.get(), this->perThreadDataSize};
    }

    this->indirectData = std::make_unique<L0::MCL::MutableIndirectData>(std::move(indirectOffsets), crossThreadDataRef, perThreadDataRef, inlineDataRef);
}

void VariableFixture::initCommandBufferWithWalker() {
    if (this->cpuWalkerBuffer != nullptr) {
        memcpy(this->cmdListBufferCpuBase, this->cpuWalkerBuffer, this->walkerCmdSize);
    }
}

void VariableInOrderFixture::setUp() {
    VariableFixture::setUp(true);
    this->mockBaseCmdList = static_cast<L0::ult::MockCommandList *>(this->mutableCommandList.get()->base);
}

void VariableInOrderFixture::attachCbEvent(Event *event, L0::ult::MockCommandList *cmdList) {
    this->cmdListInOrderCounterValue = cmdList->inOrderExecInfo->getCounterValue();
    this->cmdListInOrderAllocationOffset = cmdList->inOrderExecInfo->getAllocationOffset();

    event->updateInOrderExecState(cmdList->inOrderExecInfo,
                                  this->cmdListInOrderCounterValue,
                                  this->cmdListInOrderAllocationOffset);
    if (event->isEventTimestampFlagSet()) {
        event->resetInOrderTimestampNode(this->device->getInOrderTimestampAllocator()->getTag(), 1);
    }
}

} // namespace ult
} // namespace L0
