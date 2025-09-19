/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_imp.h"

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/mutable_cmdlist/helper.h"
#include "level_zero/core/source/mutable_cmdlist/mcl_kernel_ext.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_dispatch.h"

#include "implicit_args.h"

#include <algorithm>
#include <memory>

namespace L0 {
namespace MCL {

void MutableResidencyAllocations::addAllocation(NEO::GraphicsAllocation *allocation) {
    auto allocationElement = std::find_if(addedAllocations.begin(),
                                          addedAllocations.end(),
                                          [allocation](const auto &argument) { return argument.allocation == allocation; });
    if (allocationElement == addedAllocations.end()) {
        addedAllocations.emplace_back(allocation, 1);
    } else {
        allocationElement->refCount++;
    }
}

void MutableResidencyAllocations::removeAllocation(NEO::GraphicsAllocation *allocation) {
    auto allocationElement = std::find_if(addedAllocations.begin(),
                                          addedAllocations.end(),
                                          [allocation](const auto &argument) { return argument.allocation == allocation; });
    if (allocationElement != addedAllocations.end()) {
        allocationElement->refCount--;
        if (allocationElement->refCount == 0) {
            // if not last element, swap with the last
            if (allocationElement + 1 != addedAllocations.end()) {
                auto lastElemIterator = addedAllocations.end() - 1;
                std::swap(*allocationElement, *lastElemIterator);
            }
            // remove last element
            addedAllocations.pop_back();
        }
    }
}

void MutableResidencyAllocations::populateInputResidencyContainer(NEO::ResidencyContainer &cmdListResidency, bool baseCmdListClosed) {
    if (!baseCmdListClosed) {
        immutableResidencySize = cmdListResidency.size();
    } else {
        cmdListResidency.resize(immutableResidencySize);
    }
    for (auto &allocationElement : addedAllocations) {
        auto lastImmutableIterator = cmdListResidency.begin() + immutableResidencySize;
        auto allocationInImmutable = std::lower_bound(cmdListResidency.begin(), lastImmutableIterator, allocationElement.allocation);
        if ((allocationInImmutable == lastImmutableIterator) || (*allocationInImmutable != allocationElement.allocation)) {
            cmdListResidency.emplace_back(allocationElement.allocation);
        }
    }
}

void MutableResidencyAllocations::cleanResidencyContainer() {
    addedAllocations.clear();
    immutableResidencySize = 0;
}

MutableCommandListAllocFn mutableCommandListFactory[IGFX_MAX_PRODUCT] = {};

MutableCommandList *MutableCommandList::fromHandle(ze_command_list_handle_t handle) {
    return static_cast<MutableCommandList *>(L0::CommandList::fromHandle(handle)->asMutable());
}

CommandList *MutableCommandList::create(uint32_t productFamily, Device *device, NEO::EngineGroupType engineGroupType,
                                        ze_command_list_flags_t flags, ze_result_t &resultValue, bool useInternalEngineType) {
    MutableCommandListAllocFn allocator = nullptr;
    if (productFamily < IGFX_MAX_PRODUCT) {
        allocator = mutableCommandListFactory[productFamily];
    }
    if (allocator == nullptr) {
        resultValue = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        return nullptr;
    }

    MutableCommandListImp *commandList = nullptr;
    resultValue = ZE_RESULT_ERROR_UNINITIALIZED;

    commandList = reinterpret_cast<MutableCommandListImp *>((*allocator)(CommandList::defaultNumIddsPerBlock));
    resultValue = commandList->initialize(device, engineGroupType, flags);
    if (resultValue != ZE_RESULT_SUCCESS) {
        commandList->getBase()->destroy();
        return nullptr;
    }

    return static_cast<CommandList *>(commandList->toHandle());
}

MutableCommandList::~MutableCommandList() {
}

MutableCommandListImp::~MutableCommandListImp() {
    auto memoryManager = base->getDevice()->getNEODevice()->getMemoryManager();
    auto freeGraphicsAllocation = [memoryManager](auto allocation) {
        if (allocation) {
            memoryManager->freeGraphicsMemory(allocation.release());
        }
    };
    freeGraphicsAllocation(std::move(allocs.ihAlloc));
    freeGraphicsAllocation(std::move(allocs.sshAlloc));
    freeGraphicsAllocation(std::move(allocs.constsAlloc));
    freeGraphicsAllocation(std::move(allocs.varsAlloc));
}

ze_command_list_handle_t MutableCommandListImp::toHandle() {
    return base->toHandle();
}

CommandList *MutableCommandListImp::getBase() {
    return base;
}

void MutableCommandListImp::processResidencyContainer(bool baseCmdListClosed) {
    if (this->finalizeCommandListResidency) {
        auto &cmdListResidency = this->base->getCmdContainer().getResidencyContainer();
        mutableAllocations.populateInputResidencyContainer(cmdListResidency, baseCmdListClosed);

        this->finalizeCommandListResidency = false;
    }
}

ze_result_t MutableCommandListImp::addVariableDispatch(const NEO::KernelDescriptor &kernelDescriptor, KernelDispatch &kernelDispatch, Variable *groupSize, Variable *groupCount, Variable *globalOffset,
                                                       Variable *lastSlmArgumentVariable, MutableComputeWalker *mutableComputeWalker, const MutableKernelDispatchParameters &dispatchParams) {
    if (groupSize != nullptr && (false == groupSize->isType(VariableType::groupSize))) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (groupCount != nullptr && (false == groupCount->isType(VariableType::groupCount))) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (globalOffset != nullptr && (false == globalOffset->isType(VariableType::globalOffset))) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL kernel dispatch parameters variables: group count %p, group size %p, global offset %p\n", groupCount, groupSize, globalOffset);
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL initial group count %u %u %u\n", dispatchParams.groupCount[0], dispatchParams.groupCount[1], dispatchParams.groupCount[2]);
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL initial group size %u %u %u\n", dispatchParams.groupSize[0], dispatchParams.groupSize[1], dispatchParams.groupSize[2]);
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL initial global offset %u %u %u\n", dispatchParams.globalOffset[0], dispatchParams.globalOffset[1], dispatchParams.globalOffset[2]);

    const auto iohCpuBase = reinterpret_cast<CpuAddress>(base->getCmdContainer().getIndirectHeap(NEO::HeapType::indirectObject)->getCpuBase());

    auto &dispatchTraits = kernelDescriptor.payloadMappings.dispatchTraits;
    auto offsets = std::make_unique<MutableIndirectData::Offsets>();
    offsets->globalWorkSize = dispatchTraits.globalWorkSize[0];
    offsets->localWorkSize = dispatchTraits.localWorkSize[0];
    offsets->localWorkSize2 = dispatchTraits.localWorkSize2[0];
    offsets->enqLocalWorkSize = dispatchTraits.enqueuedLocalWorkSize[0];
    offsets->numWorkGroups = dispatchTraits.numWorkGroups[0];
    offsets->workDimensions = dispatchTraits.workDim;
    offsets->globalWorkOffset = dispatchTraits.globalWorkOffset[0];

    ArrayRef<uint8_t> inlineData;

    size_t crossThreadDataSize = kernelDescriptor.kernelAttributes.crossThreadDataSize;
    if (kernelDescriptor.kernelAttributes.flags.passInlineData) {
        crossThreadDataSize -= std::min(crossThreadDataSize, static_cast<size_t>(mutableComputeWalker->getInlineDataSize()));
        inlineData = {reinterpret_cast<uint8_t *>(mutableComputeWalker->getInlineDataPointer()), mutableComputeWalker->getInlineDataSize()};
    }
    ArrayRef<uint8_t> crossThreadData = {reinterpret_cast<uint8_t *>(ptrOffset(iohCpuBase, kernelDispatch.offsets.crossThreadOffset)), crossThreadDataSize};

    ArrayRef<uint8_t> perThreadData;
    if (kernelDispatch.offsets.perThreadOffset != undefined<IndirectObjectHeapOffset>) {
        const size_t perThreadDataSize = kernelDispatch.indirectObjectHeap.size() - crossThreadDataSize;
        perThreadData = {reinterpret_cast<uint8_t *>(ptrOffset(iohCpuBase, kernelDispatch.offsets.perThreadOffset)), perThreadDataSize};
    }

    bool calcRegion = false;
    auto mutableIndirectData = std::make_unique<MutableIndirectData>(std::move(offsets), crossThreadData, perThreadData, inlineData);
    kernelDispatch.varDispatch = std::make_unique<VariableDispatch>(&kernelDispatch, std::move(mutableIndirectData), mutableComputeWalker,
                                                                    groupSize, groupCount, globalOffset, lastSlmArgumentVariable,
                                                                    base->getDevice()->getHwInfo().capabilityTable.grfSize,
                                                                    dispatchParams, base->getPartitionCount(), base->getEngineGroupType(), calcRegion);

    return ZE_RESULT_SUCCESS;
}

KernelData *MutableCommandListImp::getKernelData(L0::Kernel *kernel) {
    KernelData *kernelDataEntry = nullptr;

    auto &kernelDescriptor = kernel->getKernelDescriptor();

    const auto &kernelName = kernelDescriptor.kernelMetadata.kernelName;
    const L0::Module *module = &(static_cast<L0::KernelImp *>(kernel)->getParentModule());
    std::pair<const L0::Module *, std::string> pairFind(module, kernelName);

    auto kernelDataIt = std::find_if(kernelData.begin(), kernelData.end(), [&pairFind](auto &kd) {
        return (kd.get()->module == pairFind.first) && (kd.get()->kernelName == pairFind.second);
    });
    if (kernelDataIt != kernelData.end()) {
        kernelDataEntry = (*kernelDataIt).get();
    } else {
        auto kernelImmutableData = kernel->getImmutableData();

        auto isa = kernelImmutableData->getIsaGraphicsAllocation();
        auto isaSize = kernelImmutableData->getIsaSize();
        auto isaOffsetWithinAllocation = kernelImmutableData->getIsaOffsetInParentAllocation();

        kernelData.emplace_back(std::make_unique<KernelData>());
        kernelDataEntry = (*kernelData.rbegin()).get();

        kernelDataEntry->kernelIsa = ArrayRef<const uint8_t>{reinterpret_cast<const uint8_t *>(isa->getUnderlyingBuffer()) + isaOffsetWithinAllocation, isaSize};
        kernelDataEntry->kernelName = kernelName;
        kernelDataEntry->module = module;
        kernelDataEntry->simdSize = kernelDescriptor.kernelAttributes.simdSize;
        kernelDataEntry->requiredThreadGroupDispatchSize = kernelDescriptor.kernelMetadata.requiredThreadGroupDispatchSize;
        kernelDataEntry->grfCount = kernelDescriptor.kernelAttributes.numGrfRequired;
        kernelDataEntry->barrierCount = kernelDescriptor.kernelAttributes.barrierCount;
        kernelDataEntry->kernelStartOffset = isa->getAllocationOffset() + isaOffsetWithinAllocation;
        if (this->base->isHeaplessModeEnabled()) {
            kernelDataEntry->kernelStartAddress = isa->getGpuAddress() + isaOffsetWithinAllocation;
        } else {
            kernelDataEntry->kernelStartAddress = isa->getGpuAddressToPatch() + isaOffsetWithinAllocation;
        }
        kernelDataEntry->skipPerThreadDataLoad = kernelDescriptor.entryPoints.skipPerThreadDataLoad;
        kernelDataEntry->passInlineData = kernelDescriptor.kernelAttributes.flags.passInlineData;
        kernelDataEntry->indirectOffset = kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.offset;
        kernelDataEntry->requiresWorkgroupWalkOrder = kernelDescriptor.kernelAttributes.flags.requiresWorkgroupWalkOrder;
        kernelDataEntry->workgroupWalkOrder[0] = kernelDescriptor.kernelAttributes.workgroupWalkOrder[0];
        kernelDataEntry->workgroupWalkOrder[1] = kernelDescriptor.kernelAttributes.workgroupWalkOrder[1];
        kernelDataEntry->workgroupWalkOrder[2] = kernelDescriptor.kernelAttributes.workgroupWalkOrder[2];
        kernelDataEntry->isaOffsetWithinAllocation = isaOffsetWithinAllocation;
        if (kernel->usesSyncBuffer()) {
            kernelDataEntry->usesSyncBuffer = true;
            kernelDataEntry->syncBufferAddressOffset = kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress.stateless;
            kernelDataEntry->syncBufferPointerSize = kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress.pointerSize;
        }
        if (kernel->usesRegionGroupBarrier()) {
            kernelDataEntry->usesRegionGroupBarrier = true;
            kernelDataEntry->regionGroupBarrierBufferOffset = kernelDescriptor.payloadMappings.implicitArgs.regionGroupBarrierBuffer.stateless;
            kernelDataEntry->regionGroupBarrierBufferPointerSize = kernelDescriptor.payloadMappings.implicitArgs.regionGroupBarrierBuffer.pointerSize;
        }
        kernelDataEntry->numLocalIdChannels = kernelDescriptor.kernelAttributes.numLocalIdChannels;
    }

    return kernelDataEntry;
}

ze_result_t MutableCommandListImp::parseDispatchedKernel(L0::Kernel *kernel, MutableComputeWalker *mutableComputeWalker,
                                                         size_t extraHeapSize, NEO::GraphicsAllocation *syncBuffer, NEO::GraphicsAllocation *regionBarrier) {
    auto kernelData = getKernelData(kernel);
    auto &kernelDescriptor = kernel->getKernelDescriptor();

    auto ioh = base->getCmdContainer().getIndirectHeap(NEO::HeapType::indirectObject);
    size_t reservedPerThreadDataSize = 0;
    const size_t consumedPerThreadDataSize = kernel->getPerThreadDataSizeForWholeThreadGroup();
    if (this->enableReservePerThreadForLocalId) {
        reservedPerThreadDataSize = this->maxPerThreadDataSize;
    }
    const size_t totalPerThreadDataSize = reservedPerThreadDataSize + consumedPerThreadDataSize;

    IndirectObjectHeapOffset inlineDataSize = 0;
    if (kernelData->passInlineData) {
        inlineDataSize = mutableComputeWalker->getInlineDataSize();
    }
    const size_t currentCrossThreadDataSize = kernel->getCrossThreadDataSize() - std::min(kernel->getCrossThreadDataSize(), static_cast<uint32_t>(inlineDataSize));
    const size_t consumedHeapSize = currentCrossThreadDataSize + totalPerThreadDataSize + extraHeapSize;
    const size_t kernelIohSize = alignUp(consumedHeapSize, this->iohAlignment);
    const auto kernelIohStartOffset = ioh->getUsed() - kernelIohSize;
    const auto kernelFullOffset = reinterpret_cast<IndirectObjectHeapOffset>(ptrOffset(ioh->getCpuBase(), kernelIohStartOffset));

    auto kernelSshOffset = undefined<SurfaceStateHeapOffset>;
    size_t kernelSshSize = 0;

    auto ssh = base->getCmdContainer().getIndirectHeap(NEO::HeapType::surfaceState);
    if (ssh != nullptr) {
        kernelSshSize = kernel->getSurfaceStateHeapDataSize();
        if (kernelSshSize > 0) {
            kernelSshOffset = ssh->getUsed() - kernelSshSize;
        }
    }

    auto walkerCmd = mutableComputeWalker->getWalkerCmdPointer();
    auto walkerCmdOffset = ptrDiff(walkerCmd, base->getCmdContainer().findCpuBaseForCmdBufferAddress(walkerCmd));

    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL append kernel name %s from module: %p inline data %d\n", kernelData->kernelName.c_str(), kernelData->module, kernelData->passInlineData);
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL append kernel IOH cpu base %p full offset %zx and gpuva %" PRIx64 "\n", ioh->getCpuBase(), kernelFullOffset, ioh->getGraphicsAllocation()->getGpuAddress());
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL append kernel ioh total %zu cross-thread total %zu per-thread total %zu\n", kernelIohSize, currentCrossThreadDataSize, totalPerThreadDataSize);

    auto kdPtr = std::make_unique<KernelDispatch>();
    dispatches.emplace_back(std::move(kdPtr));
    auto &dispatch = *(*dispatches.rbegin()).get();
    dispatch.indirectObjectHeap = {reinterpret_cast<uint8_t *>(ptrOffset(ioh->getCpuBase(), kernelIohStartOffset)), kernelIohSize};
    dispatch.surfaceStateHeapSize = {kernelSshSize};
    dispatch.offsets.crossThreadOffset = kernelIohStartOffset;
    if (totalPerThreadDataSize == 0) {
        dispatch.offsets.perThreadOffset = undefined<IndirectObjectHeapOffset>;
    } else {
        dispatch.offsets.perThreadOffset = kernelIohStartOffset + currentCrossThreadDataSize;
    }
    dispatch.offsets.sshOffset = kernelSshOffset;
    dispatch.offsets.btOffset = kernelDescriptor.payloadMappings.bindingTable.tableOffset;
    dispatch.offsets.kernelStartOffset = kernel->getImmutableData()->getIsaGraphicsAllocation()->getAllocationOffset() + kernelData->isaOffsetWithinAllocation;
    dispatch.offsets.walkerCmdOffset = walkerCmdOffset;
    dispatch.kernelData = kernelData;
    dispatch.walkerCmd = walkerCmd;
    if (kernel->getSlmTotalSize() > 0) {
        dispatch.slmTotalSize = kernel->getSlmTotalSize();
        dispatch.slmInlineSize = kernelDescriptor.kernelAttributes.slmInlineSize;
    }
    dispatch.slmPolicy = static_cast<uint32_t>(kernel->getSlmPolicy());
    if (dispatch.kernelData->usesSyncBuffer) {
        dispatch.syncBuffer = syncBuffer;
    }
    if (dispatch.kernelData->usesRegionGroupBarrier) {
        dispatch.regionBarrier = regionBarrier;
    }

    const auto &args = kernelDescriptor.payloadMappings.explicitArgs;
    auto &kernelExt = MclKernelExt::get(kernel);
    auto &vars = kernelExt.getVariables();
    DEBUG_BREAK_IF(args.size() != vars.size());
    auto &slmArgOffsetValues = static_cast<L0::KernelImp *>(kernel)->getSlmArgOffsetValues();
    auto &slmArgSizes = static_cast<L0::KernelImp *>(kernel)->getSlmArgSizes();
    for (size_t i = 0; i < args.size(); ++i) {
        if (vars[i] == nullptr) {
            continue;
        }
        auto retVal = Variable::fromHandle(vars[i])->addKernelArgUsage(args[i], kernelIohStartOffset, kernelFullOffset, kernelSshOffset,
                                                                       slmArgSizes[i], slmArgOffsetValues[i],
                                                                       walkerCmdOffset, mutableComputeWalker, kernelData->passInlineData);
        if (retVal != ZE_RESULT_SUCCESS) {
            DEBUG_BREAK_IF(true);
            return retVal;
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MutableCommandListImp::getNextCommandId(const ze_mutable_command_id_exp_desc_t *desc, uint32_t numKernels, ze_kernel_handle_t *phKernels, uint64_t *pCommandId) {
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL getNextCommandId cmdlist: %p, flags: %u, numKernels: %u\n", this, desc->flags, numKernels);
    AppendKernelMutation nextAppendKernel;
    AppendEventMutation nextAppendEvents;
    if (desc->flags == 0) {
        this->nextMutationFlags = ZE_MUTABLE_COMMAND_EXP_FLAG_FORCE_UINT32 & (~ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION);
    } else {
        this->nextMutationFlags = desc->flags;
    }
    if (kernelInstructionMutationEnabled(this->nextMutationFlags)) {
        if (numKernels == 0) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        auto mutableKernelGroup = std::make_unique<MutableKernelGroup>(numKernels, phKernels, this->inlineDataSize, this->maxPerThreadDataSize);
        this->mutableKernelGroups.emplace_back(std::move(mutableKernelGroup));
        nextAppendKernel.kernelGroup = (*this->mutableKernelGroups.rbegin()).get();

        // prepare kernel data for the whole kernel group - next append call will have them all ready
        bool allocateSyncBufferHandler = false;
        auto &mutableKernels = nextAppendKernel.kernelGroup->getKernelsInGroup();
        for (uint32_t i = 0; i < numKernels; i++) {
            auto kernelData = getKernelData(Kernel::fromHandle(phKernels[i]));
            allocateSyncBufferHandler |= (kernelData->usesSyncBuffer || kernelData->usesRegionGroupBarrier);
            if (!mutableKernels[i]->checkKernelCompatible()) {
                return ZE_RESULT_ERROR_INVALID_KERNEL_ATTRIBUTE_VALUE;
            }
        }

        if (allocateSyncBufferHandler) {
            getBase()->getDevice()->getNEODevice()->allocateSyncBufferHandler();
        }
    }

    kernelMutations.push_back(std::move(nextAppendKernel));
    eventMutations.push_back(std::move(nextAppendEvents));

    *pCommandId = ++nextCommandId;
    nextAppendKernelMutable = true;
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL getNextCommandId cmdlist: %p, command id: %" PRIu64 "\n", this, *pCommandId);
    return ZE_RESULT_SUCCESS;
}

ze_result_t MutableCommandListImp::updateMutableCommandsExp(const ze_mutable_commands_exp_desc_t *desc) {
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL updateMutableCommandsExp cmdlist: %p\n", this);
    ze_result_t result = ZE_RESULT_SUCCESS;
    const void *next = desc->pNext;
    while (next != nullptr) {
        KernelVariableDescriptor *currentVariables = nullptr;
        const ze_base_desc_t *extendedDesc = reinterpret_cast<const ze_base_desc_t *>(next);
        if (extendedDesc->stype == ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC) {
            const ze_mutable_kernel_argument_exp_desc_t *apiKernelArgumentDesc = reinterpret_cast<const ze_mutable_kernel_argument_exp_desc_t *>(next);
            AppendKernelMutation &selectedAppend = this->kernelMutations[(apiKernelArgumentDesc->commandId - 1)];
            currentVariables = getVariableDescriptorContainer(selectedAppend);
            if (apiKernelArgumentDesc->argIndex + 1 > currentVariables->kernelArguments.size()) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            KernelArgumentVariableDescriptor &kernelArgDesc = currentVariables->kernelArguments[apiKernelArgumentDesc->argIndex];
            UNRECOVERABLE_IF(kernelArgDesc.argIndex != apiKernelArgumentDesc->argIndex);
            if (kernelArgDesc.kernelArgumentVariable == nullptr) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            if (kernelArgDesc.kernelArgumentVariable->getType() == VariableType::buffer) {
                auto argValue = apiKernelArgumentDesc->pArgValue == nullptr ? nullptr : *reinterpret_cast<void *const *>(apiKernelArgumentDesc->pArgValue);
                if (kernelArgDesc.kernelArgumentVariable->getDesc().argValue == argValue) {
                    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL update kernel arg commandId: %" PRIu64 " argument idx: %u, buffer - same value: %p\n",
                                       apiKernelArgumentDesc->commandId, apiKernelArgumentDesc->argIndex, argValue);
                    next = extendedDesc->pNext;
                    continue;
                }
            }
            result = kernelArgDesc.kernelArgumentVariable->setValue(apiKernelArgumentDesc->argSize, 0, apiKernelArgumentDesc->pArgValue);
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
            this->updatedCommandList = true;
            if (kernelArgDesc.kernelArgumentVariable->getType() == VariableType::slmBuffer && kernelArgDesc.kernelArgumentVariable->isCooperativeVariable()) {
                auto varDispatch = kernelArgDesc.kernelArgumentVariable->getInitialVariableDispatch();
                cooperativeKernelVariableDispatches.insert(varDispatch);
            }
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL update kernel arg commandId: %" PRIu64 " argument idx: %u, size: %zu, val: %p\n",
                               apiKernelArgumentDesc->commandId, apiKernelArgumentDesc->argIndex, apiKernelArgumentDesc->argSize, kernelArgDesc.kernelArgumentVariable->getDesc().argValue);
        }
        if (extendedDesc->stype == ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC) {
            const ze_mutable_group_count_exp_desc_t *groupCountDesc = reinterpret_cast<const ze_mutable_group_count_exp_desc_t *>(next);
            AppendKernelMutation &selectedAppend = this->kernelMutations[(groupCountDesc->commandId - 1)];
            currentVariables = getVariableDescriptorContainer(selectedAppend);
            if (currentVariables->groupCount == nullptr) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            result = currentVariables->groupCount->setValue(sizeof(ze_group_count_t), 0, groupCountDesc->pGroupCount);
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
            this->updatedCommandList = true;
            if (currentVariables->groupCount->isCooperativeVariable()) {
                auto varDispatch = currentVariables->groupCount->getInitialVariableDispatch();
                cooperativeKernelVariableDispatches.insert(varDispatch);
            }
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL update group count commandId: %" PRIu64 " x: %u y: %u z: %u\n",
                               groupCountDesc->commandId, groupCountDesc->pGroupCount->groupCountX, groupCountDesc->pGroupCount->groupCountY, groupCountDesc->pGroupCount->groupCountZ);
        }
        if (extendedDesc->stype == ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC) {
            const ze_mutable_group_size_exp_desc_t *groupSizeDesc = reinterpret_cast<const ze_mutable_group_size_exp_desc_t *>(next);
            AppendKernelMutation &selectedAppend = this->kernelMutations[(groupSizeDesc->commandId - 1)];
            currentVariables = getVariableDescriptorContainer(selectedAppend);
            if (currentVariables->groupSize == nullptr) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            uint32_t groupSize[3] = {groupSizeDesc->groupSizeX, groupSizeDesc->groupSizeY, groupSizeDesc->groupSizeZ};
            result = currentVariables->groupSize->setValue(sizeof(groupSize), 0, groupSize);
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
            this->updatedCommandList = true;
            if (currentVariables->groupSize->isCooperativeVariable()) {
                auto varDispatch = currentVariables->groupSize->getInitialVariableDispatch();
                cooperativeKernelVariableDispatches.insert(varDispatch);
            }
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL update group size commandId: %" PRIu64 " x: %u y: %u z: %u\n",
                               groupSizeDesc->commandId, groupSizeDesc->groupSizeX, groupSizeDesc->groupSizeY, groupSizeDesc->groupSizeZ);
        }
        if (extendedDesc->stype == ZE_STRUCTURE_TYPE_MUTABLE_GLOBAL_OFFSET_EXP_DESC) {
            const ze_mutable_global_offset_exp_desc_t *globalOffsetDesc = reinterpret_cast<const ze_mutable_global_offset_exp_desc_t *>(next);
            AppendKernelMutation &selectedAppend = this->kernelMutations[(globalOffsetDesc->commandId - 1)];
            currentVariables = getVariableDescriptorContainer(selectedAppend);
            if (currentVariables->globalOffset == nullptr) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            uint32_t globalOffset[3] = {globalOffsetDesc->offsetX, globalOffsetDesc->offsetY, globalOffsetDesc->offsetZ};
            result = currentVariables->globalOffset->setValue(sizeof(globalOffset), 0, globalOffset);
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
            this->updatedCommandList = true;
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL update global offset commandId: %" PRIu64 " x: %u y: %u z: %u\n",
                               globalOffsetDesc->commandId, globalOffsetDesc->offsetX, globalOffsetDesc->offsetY, globalOffsetDesc->offsetZ);
        }
        next = extendedDesc->pNext;
    }

    if (cooperativeKernelVariableDispatches.size() > 0) {
        for (auto &varDispatch : cooperativeKernelVariableDispatches) {
            if (varDispatch->getMaxCooperativeGroupCount() < varDispatch->getTotalGroupCount()) {
                return ZE_RESULT_ERROR_UNSUPPORTED_SIZE;
            }
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MutableCommandListImp::updateMutableCommandSignalEventExp(uint64_t commandId, ze_event_handle_t signalEvent) {
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL updateMutableCommandSignalEventExp cmdlist: %p commandId: %" PRIu64 " new signal event handle: %p\n", this, commandId, signalEvent);
    AppendEventMutation &selectedAppend = this->eventMutations[(commandId - 1)];
    if (selectedAppend.signalEvent.eventVariable == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    auto inputEvent = Event::fromHandle(signalEvent);
    if (selectedAppend.signalEvent.event == inputEvent) {
        return ZE_RESULT_SUCCESS;
    }

    auto ret = selectedAppend.signalEvent.eventVariable->setValue(0, 0, inputEvent);
    if (ret == ZE_RESULT_SUCCESS) {
        selectedAppend.signalEvent.event = inputEvent;
        this->updatedCommandList = true;
    }
    return ret;
}

ze_result_t MutableCommandListImp::updateMutableCommandWaitEventsExp(uint64_t commandId, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL updateMutableCommandWaitEventsExp cmdlist: %p commandId: %" PRIu64 " numWaitEvents: %u\n", this, commandId, numWaitEvents);
    AppendEventMutation &selectedAppend = this->eventMutations[(commandId - 1)];
    if (numWaitEvents > selectedAppend.waitEvents.size()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    for (uint32_t eventNum = 0; eventNum < numWaitEvents; eventNum++) {
        WaitEventVariableDescriptor &mutableWaitEventDesc = selectedAppend.waitEvents[eventNum];
        UNRECOVERABLE_IF(mutableWaitEventDesc.waitEventIndex != eventNum);
        auto waitEventHandle = toInternalType(phWaitEvents[eventNum]);
        auto inputEvent = Event::fromHandle(waitEventHandle);
        if (mutableWaitEventDesc.event == inputEvent) {
            continue;
        }

        auto retCode = mutableWaitEventDesc.eventVariable->setValue(0, 0, inputEvent);
        if (retCode == ZE_RESULT_SUCCESS) {
            mutableWaitEventDesc.event = inputEvent;
            this->updatedCommandList = true;
        } else {
            return retCode;
        }
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL event number: %u new wait event handle: %p\n", eventNum, waitEventHandle);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MutableCommandListImp::updateMutableCommandKernelsExp(uint32_t numKernels, uint64_t *pCommandId, ze_kernel_handle_t *phKernels) {
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL updateMutableCommandKernelsExp cmdlist: %p numKernels: %u\n", this, numKernels);
    for (uint32_t id = 0; id < numKernels; id++) {
        auto commandId = pCommandId[id];
        auto kernelHandle = toInternalType(phKernels[id]);
        auto kernel = Kernel::fromHandle(kernelHandle);
        auto kernelGroup = this->kernelMutations[(commandId - 1)].kernelGroup;
        if (kernelGroup == nullptr) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        auto oldMutableKernel = kernelGroup->getCurrentMutableKernel();
        // when old mutable kernel handle is the same as new, then skip mutation
        if (oldMutableKernel->getKernel() == kernel) {
            continue;
        }
        auto oldKernelComputeWalker = oldMutableKernel->getMutableComputeWalker();

        kernelGroup->setCurrentMutableKernel(kernel);
        auto newMutableKernel = kernelGroup->getCurrentMutableKernel();

        // remove old kernel from mutable residency
        {
            oldMutableKernel->updateResidencySnapshotContainer();
            auto &residencyContainer = oldMutableKernel->getKernelResidencySnapshotContainer();
            for (auto resource : residencyContainer) {
                removeFromResidencyContainer(resource);
            }
        }
        // remove old kernel arguments (buffers) from mutable residency and reset variables
        {
            auto &kernelVariableDescriptors = oldMutableKernel->getKernelVariables();
            for (auto &kernelArgVarDesc : kernelVariableDescriptors.kernelArguments) {
                if (kernelArgVarDesc.kernelArgumentVariable == nullptr) {
                    continue;
                }
                auto &varDescriptor = kernelArgVarDesc.kernelArgumentVariable->getDesc();
                if (varDescriptor.type == VariableType::buffer) {
                    if (varDescriptor.bufferAlloc != nullptr) {
                        removeFromResidencyContainer(varDescriptor.bufferAlloc);
                    }
                    kernelArgVarDesc.kernelArgumentVariable->resetBufferVariable();
                }
                if (varDescriptor.type == VariableType::slmBuffer) {
                    kernelArgVarDesc.kernelArgumentVariable->resetSlmVariable();
                }
            }
            kernelVariableDescriptors.groupCount->resetGroupCountVariable();
            if (kernelVariableDescriptors.groupSize != nullptr) {
                kernelVariableDescriptors.groupSize->resetGroupSizeVariable();
            }
            if (kernelVariableDescriptors.globalOffset != nullptr) {
                kernelVariableDescriptors.globalOffset->resetGlobalOffsetVariable();
            }
        }

        // copy const data from host view into heap payload
        newMutableKernel->copyTemplateViewIndirectData();

        {
            // copy noop patch indices
            auto oldKernelDispatch = oldMutableKernel->getKernelDispatch();
            auto newKernelDispatch = newMutableKernel->getKernelDispatch();

            newKernelDispatch->syncBufferNoopPatchIndex = oldKernelDispatch->syncBufferNoopPatchIndex;
            newKernelDispatch->regionBarrierNoopPatchIndex = oldKernelDispatch->regionBarrierNoopPatchIndex;

            if (newKernelDispatch->syncBufferNoopPatchIndex != undefined<size_t> &&
                newKernelDispatch->kernelData->usesSyncBuffer == false) {
                // disable noop patch index if sync buffer is not used
                disableAddressNoopPatch(newKernelDispatch->syncBufferNoopPatchIndex);
            }

            if (newKernelDispatch->regionBarrierNoopPatchIndex != undefined<size_t> &&
                newKernelDispatch->kernelData->usesRegionGroupBarrier == false) {
                // disable noop patch index if region barrier buffer is not used
                disableAddressNoopPatch(newKernelDispatch->regionBarrierNoopPatchIndex);
            }
        }

        // copy post sync and possible indirect/scratch pointers from old walker host view into new walker host view
        auto newKernelComputeWalker = newMutableKernel->getMutableComputeWalker();
        newKernelComputeWalker->copyWalkerDataToHostBuffer(oldKernelComputeWalker);

        if (kernelGroup->isScratchNeeded()) {
            // update walker with latest patched scratch address
            auto scratchAddressPatchIndex = kernelGroup->getScratchAddressPatchIndex();
            this->updateScratchAddress(scratchAddressPatchIndex, *oldKernelComputeWalker, *newKernelComputeWalker);
        }

        // save new host view inline data/post sync into command buffer
        newKernelComputeWalker->saveCpuBufferIntoGpuBuffer(false, true);

        // update reminder variables (signal event variable) with new compute walker to have correct reference for new post sync addresses
        if (kernelGroup->getSharedSignalVariable() != nullptr) {
            kernelGroup->getSharedSignalVariable()->updateMutableComputeWalker(newKernelComputeWalker);
        }

        // add new kernel to mutable residency
        {
            newMutableKernel->makeKernelResidencySnapshotContainer(false);
            auto &residencyContainer = newMutableKernel->getKernelResidencySnapshotContainer();
            for (auto resource : residencyContainer) {
                addToResidencyContainer(resource);
            }
        }

        updateKernelMemoryPrefetch(*kernel, kernelGroup->getIohForPrefetch(), kernelGroup->getPrefetchCmd(), commandId);

        this->updatedCommandList = true;
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL kernel number: %u commandId: %" PRIu64 " new kernel handle: %p\n", id, commandId, kernelHandle);
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace MCL
} // namespace L0
