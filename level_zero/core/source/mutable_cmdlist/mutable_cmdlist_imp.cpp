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
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/mutable_cmdlist/helper.h"
#include "level_zero/core/source/mutable_cmdlist/mcl_kernel_ext.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_dispatch.h"

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

ze_result_t MutableCommandListImp::getVariable(const InterfaceVariableDescriptor *varDesc, Variable **outVariable) {
    *outVariable = nullptr;

    std::string varName = varDesc->name == nullptr ? "" : std::string(varDesc->name);

    if (false == varName.empty()) {
        auto it = variableMap.find(varName);
        if (it != variableMap.end()) {
            *outVariable = it->second;
            return ZE_RESULT_SUCCESS;
        }
    }

    auto var = std::unique_ptr<Variable>(Variable::create(this->base, varDesc));
    if (false == varName.empty()) {
        variableMap.insert(std::make_pair(varName, var.get()));
    }

    if (var->getDesc().isTemporary) {
        tempMem.variables.push_back(var.get());
    }

    *outVariable = var.get();
    variableStorage.push_back(std::move(var));

    this->hasStageCommitVariables |= varDesc->isStageCommit;

    return ZE_RESULT_SUCCESS;
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

    bool calcRegion = base->isHeaplessModeEnabled() && base->getLocalDispatchSupport();
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

    const auto kernelName = kernelDescriptor.kernelMetadata.kernelName;
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
    dispatchs.emplace_back(std::move(kdPtr));
    auto &dispatch = *(*dispatchs.rbegin()).get();
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
    AppendMutation nextAppend;
    if (desc->flags == 0) {
        nextAppend.mutationFlags = ZE_MUTABLE_COMMAND_EXP_FLAG_FORCE_UINT32 & (~ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION);
    } else {
        nextAppend.mutationFlags = desc->flags;
    }
    if (kernelInstructionMutationEnabled(nextAppend.mutationFlags)) {
        if (numKernels == 0) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        auto mutableKernelGroup = std::make_unique<MutableKernelGroup>(numKernels, phKernels, this->inlineDataSize, this->maxPerThreadDataSize);
        this->mutableKernelGroups.emplace_back(std::move(mutableKernelGroup));
        nextAppend.kernelGroup = (*this->mutableKernelGroups.rbegin()).get();

        // prepare kernel data for the whole kernel group - next append call will have them all ready
        bool allocateSyncBufferHandler = false;
        auto &mutableKernels = nextAppend.kernelGroup->getKernelsInGroup();
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

    mutations.push_back(nextAppend);

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
        MutationVariables *currentVariables = nullptr;
        const ze_base_desc_t *extendedDesc = reinterpret_cast<const ze_base_desc_t *>(next);
        if (extendedDesc->stype == ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC) {
            const ze_mutable_kernel_argument_exp_desc_t *kernelArgumentDesc = reinterpret_cast<const ze_mutable_kernel_argument_exp_desc_t *>(next);
            AppendMutation &selectedAppend = this->mutations[(kernelArgumentDesc->commandId - 1)];
            if ((selectedAppend.mutationFlags & ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS) == 0) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            currentVariables = getVariableDescriptorContainer(selectedAppend);
            MutableVariableDescriptor *mutableKernelArgumentDesc = nullptr;
            for (auto &mutableTypeDescriptor : *currentVariables) {
                if (mutableTypeDescriptor.varType != ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS ||
                    mutableTypeDescriptor.kernelArguments.argIndex != kernelArgumentDesc->argIndex) {
                    continue;
                }
                mutableKernelArgumentDesc = &mutableTypeDescriptor;
                break;
            }
            if (mutableKernelArgumentDesc == nullptr) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            if (mutableKernelArgumentDesc->var->getType() == VariableType::buffer) {
                auto argValue = kernelArgumentDesc->pArgValue == nullptr ? nullptr : *reinterpret_cast<void *const *>(kernelArgumentDesc->pArgValue);
                if (mutableKernelArgumentDesc->var->getDesc().argValue == argValue) {
                    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL update kernel arg commandId: %" PRIu64 " argument idx: %u, buffer - same value: %p\n",
                                       kernelArgumentDesc->commandId, kernelArgumentDesc->argIndex, argValue);
                    next = extendedDesc->pNext;
                    continue;
                }
            }
            result = mutableKernelArgumentDesc->var->setValue(kernelArgumentDesc->argSize, 0, kernelArgumentDesc->pArgValue);
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
            this->updatedCommandList = true;
            if (mutableKernelArgumentDesc->var->getType() == VariableType::slmBuffer && mutableKernelArgumentDesc->var->isCooperativeVariable()) {
                auto varDispatch = mutableKernelArgumentDesc->var->getInitialVariableDispatch();
                cooperativeKernelVariableDispatches.insert(varDispatch);
            }
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL update kernel arg commandId: %" PRIu64 " argument idx: %u, size: %zu, val: %p\n",
                               kernelArgumentDesc->commandId, kernelArgumentDesc->argIndex, kernelArgumentDesc->argSize, mutableKernelArgumentDesc->var->getDesc().argValue);
        }
        if (extendedDesc->stype == ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC) {
            const ze_mutable_group_count_exp_desc_t *groupCountDesc = reinterpret_cast<const ze_mutable_group_count_exp_desc_t *>(next);
            AppendMutation &selectedAppend = this->mutations[(groupCountDesc->commandId - 1)];
            if ((selectedAppend.mutationFlags & ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT) == 0) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            currentVariables = getVariableDescriptorContainer(selectedAppend);
            MutableVariableDescriptor *mutableGroupCountDesc = nullptr;
            for (auto &mutableTypeDescriptor : *currentVariables) {
                if (mutableTypeDescriptor.varType != ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT) {
                    continue;
                }
                mutableGroupCountDesc = &mutableTypeDescriptor;
                break;
            }
            if (mutableGroupCountDesc == nullptr) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            result = mutableGroupCountDesc->var->setValue(sizeof(ze_group_count_t), 0, groupCountDesc->pGroupCount);
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
            this->updatedCommandList = true;
            if (mutableGroupCountDesc->var->isCooperativeVariable()) {
                auto varDispatch = mutableGroupCountDesc->var->getInitialVariableDispatch();
                cooperativeKernelVariableDispatches.insert(varDispatch);
            }
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL update group count commandId: %" PRIu64 " x: %u y: %u z: %u\n",
                               groupCountDesc->commandId, groupCountDesc->pGroupCount->groupCountX, groupCountDesc->pGroupCount->groupCountY, groupCountDesc->pGroupCount->groupCountZ);
        }
        if (extendedDesc->stype == ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC) {
            const ze_mutable_group_size_exp_desc_t *groupSizeDesc = reinterpret_cast<const ze_mutable_group_size_exp_desc_t *>(next);
            AppendMutation &selectedAppend = this->mutations[(groupSizeDesc->commandId - 1)];
            if ((selectedAppend.mutationFlags & ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE) == 0) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            currentVariables = getVariableDescriptorContainer(selectedAppend);
            MutableVariableDescriptor *mutableGroupSizeDesc = nullptr;
            for (auto &mutableTypeDescriptor : *currentVariables) {
                if (mutableTypeDescriptor.varType != ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE) {
                    continue;
                }
                mutableGroupSizeDesc = &mutableTypeDescriptor;
                break;
            }
            if (mutableGroupSizeDesc == nullptr) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            uint32_t groupSize[3] = {groupSizeDesc->groupSizeX, groupSizeDesc->groupSizeY, groupSizeDesc->groupSizeZ};
            result = mutableGroupSizeDesc->var->setValue(sizeof(groupSize), 0, groupSize);
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
            this->updatedCommandList = true;
            if (mutableGroupSizeDesc->var->isCooperativeVariable()) {
                auto varDispatch = mutableGroupSizeDesc->var->getInitialVariableDispatch();
                cooperativeKernelVariableDispatches.insert(varDispatch);
            }
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL update group size commandId: %" PRIu64 " x: %u y: %u z: %u\n",
                               groupSizeDesc->commandId, groupSizeDesc->groupSizeX, groupSizeDesc->groupSizeY, groupSizeDesc->groupSizeZ);
        }
        if (extendedDesc->stype == ZE_STRUCTURE_TYPE_MUTABLE_GLOBAL_OFFSET_EXP_DESC) {
            const ze_mutable_global_offset_exp_desc_t *globalOffsetDesc = reinterpret_cast<const ze_mutable_global_offset_exp_desc_t *>(next);
            AppendMutation &selectedAppend = this->mutations[(globalOffsetDesc->commandId - 1)];
            if ((selectedAppend.mutationFlags & ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET) == 0) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            currentVariables = getVariableDescriptorContainer(selectedAppend);
            MutableVariableDescriptor *mutableGlobalOffsetDesc = nullptr;
            for (auto &mutableTypeDescriptor : *currentVariables) {
                if (mutableTypeDescriptor.varType != ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET) {
                    continue;
                }
                mutableGlobalOffsetDesc = &mutableTypeDescriptor;
                break;
            }
            if (mutableGlobalOffsetDesc == nullptr) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            uint32_t globalOffset[3] = {globalOffsetDesc->offsetX, globalOffsetDesc->offsetY, globalOffsetDesc->offsetZ};
            result = mutableGlobalOffsetDesc->var->setValue(sizeof(globalOffset), 0, globalOffset);
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
    AppendMutation &selectedAppend = this->mutations[(commandId - 1)];
    if ((selectedAppend.mutationFlags & ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT) == 0) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    MutableVariableDescriptor *mutableSignalEventDesc = nullptr;
    for (auto &mutableTypeDescriptor : selectedAppend.variables) {
        if (mutableTypeDescriptor.varType != ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT) {
            continue;
        }
        mutableSignalEventDesc = &mutableTypeDescriptor;
        break;
    }
    if (mutableSignalEventDesc == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    auto inputEvent = Event::fromHandle(signalEvent);
    if (mutableSignalEventDesc->signalEvent.event == inputEvent) {
        return ZE_RESULT_SUCCESS;
    }

    auto ret = mutableSignalEventDesc->var->setValue(0, 0, inputEvent);
    if (ret == ZE_RESULT_SUCCESS) {
        mutableSignalEventDesc->signalEvent.event = inputEvent;
        this->updatedCommandList = true;
    }
    return ret;
}

ze_result_t MutableCommandListImp::updateMutableCommandWaitEventsExp(uint64_t commandId, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL updateMutableCommandWaitEventsExp cmdlist: %p commandId: %" PRIu64 " numWaitEvents: %u\n", this, commandId, numWaitEvents);
    AppendMutation &selectedAppend = this->mutations[(commandId - 1)];
    if ((selectedAppend.mutationFlags & ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS) == 0) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    MutableVariableDescriptor *mutableWaitEventDesc = nullptr;
    for (uint32_t eventNum = 0; eventNum < numWaitEvents; eventNum++) {
        for (auto &mutableTypeDescriptor : selectedAppend.variables) {
            if (mutableTypeDescriptor.varType != ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS || mutableTypeDescriptor.waitEvents.waitEventIndex != eventNum) {
                continue;
            }
            mutableWaitEventDesc = &mutableTypeDescriptor;
            break;
        }
        if (mutableWaitEventDesc == nullptr) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        auto waitEventHandle = phWaitEvents[eventNum];
        auto inputEvent = Event::fromHandle(waitEventHandle);
        if (mutableWaitEventDesc->waitEvents.event == inputEvent) {
            continue;
        }

        auto retCode = mutableWaitEventDesc->var->setValue(0, 0, inputEvent);
        if (retCode == ZE_RESULT_SUCCESS) {
            mutableWaitEventDesc->waitEvents.event = inputEvent;
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
        auto kernelHandle = phKernels[id];
        auto kernel = Kernel::fromHandle(kernelHandle);
        AppendMutation &selectedAppend = this->mutations[(commandId - 1)];
        if (!kernelInstructionMutationEnabled(selectedAppend.mutationFlags)) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        auto kernelGroup = selectedAppend.kernelGroup;

        UNRECOVERABLE_IF(kernelGroup == nullptr);
        auto oldMutableKernel = kernelGroup->getCurrentMutableKernel();
        if (oldMutableKernel->getKernel() == kernel) {
            continue;
        }
        auto oldKernelComputeWalker = oldMutableKernel->getMutableComputeWalker();

        // select new mutable kernel, when not found - old kernel remains as current - error
        kernelGroup->setCurrentMutableKernel(kernel);
        auto newMutableKernel = kernelGroup->getCurrentMutableKernel();
        if (newMutableKernel == oldMutableKernel) {
            return ZE_RESULT_ERROR_INVALID_KERNEL_HANDLE;
        }

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
            for (auto &kernelVariableDescriptor : oldMutableKernel->getKernelVariables()) {
                auto &varDescriptor = kernelVariableDescriptor.var->getDesc();
                if (varDescriptor.type == VariableType::buffer) {
                    if (varDescriptor.bufferAlloc != nullptr) {
                        removeFromResidencyContainer(varDescriptor.bufferAlloc);
                    }
                    kernelVariableDescriptor.var->resetBufferVariable();
                }
                if (varDescriptor.type == VariableType::slmBuffer) {
                    kernelVariableDescriptor.var->resetSlmVariable();
                }
                if (varDescriptor.type == VariableType::groupCount) {
                    kernelVariableDescriptor.var->resetGroupCountVariable();
                }
                if (varDescriptor.type == VariableType::groupSize) {
                    kernelVariableDescriptor.var->resetGroupSizeVariable();
                }
                if (varDescriptor.type == VariableType::globalOffset) {
                    kernelVariableDescriptor.var->resetGlobalOffsetVariable();
                }
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
        }

        // copy post sync and payload from old walker host view into new walker host view
        auto newKernelComputeWalker = newMutableKernel->getMutableComputeWalker();
        newKernelComputeWalker->copyWalkerDataToHostBuffer(oldKernelComputeWalker);

        if (kernelGroup->isScratchNeeded()) {
            // update walker with latest patched scratch address
            auto scratchAddressPatchIndex = kernelGroup->getScratchAddressPatchIndex();
            this->updateScratchAddress(scratchAddressPatchIndex, *oldKernelComputeWalker, *newKernelComputeWalker);
        }

        // save new host view into command buffer
        newKernelComputeWalker->saveCpuBufferIntoGpuBuffer(false);

        // update reminder variables (signal/wait events variables) with new compute walker to have correct reference for new post sync addresses
        for (auto &mutableVariableDescriptor : selectedAppend.variables) {
            mutableVariableDescriptor.var->updateMutableComputeWalker(newKernelComputeWalker);
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
