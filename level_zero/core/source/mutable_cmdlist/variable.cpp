/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/mutable_cmdlist/variable.h"

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/kernel_arg_descriptor.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/mutable_cmdlist/helper.h"
#include "level_zero/core/source/mutable_cmdlist/mcl_kernel_ext.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_load_register_imm.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_pipe_control.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_semaphore_wait.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_store_data_imm.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_store_register_mem.h"
#include "level_zero/core/source/mutable_cmdlist/usage.h"
#include "level_zero/core/source/mutable_cmdlist/variable_dispatch.h"

#include "implicit_args.h"

namespace L0::MCL {
using State = VariableDescriptor::State;

Variable::Variable(MutableCommandList *cmdList) {
    this->cmdList = cmdList;
    this->desc = {};
}

Variable *Variable::create(ze_command_list_handle_t hCmdList, const InterfaceVariableDescriptor *ifaceVarDesc) {
    auto var = new Variable(MutableCommandList::fromHandle(hCmdList));
    auto &desc = var->getDesc();

    desc.isStageCommit = ifaceVarDesc->isStageCommit;
    desc.immediateValueChunks = ifaceVarDesc->immediateValueChunks;

    var->setDescExperimentalValues(ifaceVarDesc);

    return var;
}

bool Variable::hasKernelArgCorrectType(const NEO::ArgDescriptor &arg) {
    using ArgType = NEO::ArgDescriptor::ArgType;
    switch (arg.type) {
    default:
        return false;
    case ArgType::argTPointer:
        if (arg.getTraits().getAddressQualifier() == NEO::KernelArgMetadata::AddrLocal) {
            return isType(VariableType::slmBuffer);
        }
        return isType(VariableType::buffer);
    case ArgType::argTValue:
        return isType(VariableType::value);
    }
}

bool Variable::isType(VariableType type) {
    if (desc.state == State::declared) {
        desc.type = type;
        desc.state = State::defined;
        if (type == VariableType::groupSize ||
            type == VariableType::groupCount ||
            type == VariableType::globalOffset) {
            desc.size = 3 * sizeof(uint32_t);
        } else {
            desc.size = 8U;
        }
        return true;
    }
    return desc.type == type && desc.state == State::defined;
}

ze_result_t Variable::setAsKernelArg(ze_kernel_handle_t hKernel, uint32_t argIndex) {
    auto kernel = Kernel::fromHandle(hKernel);
    const auto &kernelArgs = kernel->getKernelDescriptor().payloadMappings.explicitArgs;

    if (argIndex >= kernelArgs.size()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (false == hasKernelArgCorrectType(kernelArgs[argIndex])) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto &kernelExt = MclKernelExt::get(kernel);
    kernelExt.setArgumentVariable(argIndex, this);

    return ZE_RESULT_SUCCESS;
}

ze_result_t Variable::setAsKernelGroupSize(ze_kernel_handle_t hKernel) {
    if (false == isType(VariableType::groupSize)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto kernel = Kernel::fromHandle(hKernel);
    auto &kernelExt = MclKernelExt::get(kernel);
    kernelExt.setGroupSizeVariable(this);

    return ZE_RESULT_SUCCESS;
}

ze_result_t Variable::setAsSignalEvent(Event *event, MutableComputeWalker *walkerCmd, MutablePipeControl *postSyncCmd) {
    if (false == isType(VariableType::signalEvent)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    this->eventValue.event = event;
    this->eventValue.eventPoolAllocation = event->getAllocation(cmdList->getBase()->getDevice());
    this->eventValue.counterBasedEvent = event->isCounterBased();
    this->eventValue.inOrderIncrementEvent = event->getInOrderIncrementValue(cmdList->getBase()->getPartitionCount()) > 0;
    this->eventValue.walkerCmd = walkerCmd;
    this->eventValue.postSyncCmd = postSyncCmd;
    this->eventValue.kernelCount = event->getKernelCount();
    this->eventValue.packetCount = event->getPacketsInUse();
    this->eventValue.waitPackets = event->getPacketsToWait();
    this->eventValue.hasStandaloneProfilingNode = event->hasInOrderTimestampNode();
    if (this->eventValue.counterBasedEvent) {
        this->eventValue.inOrderExecBaseSignalValue = event->getInOrderExecBaseSignalValue();
        this->eventValue.inOrderAllocationOffset = event->getInOrderAllocationOffset();
    }
    this->desc.size = 0;
    return ZE_RESULT_SUCCESS;
}

ze_result_t Variable::setAsWaitEvent(Event *event) {
    if (false == isType(VariableType::waitEvent)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    this->eventValue.event = event;
    this->eventValue.eventPoolAllocation = event->getAllocation(cmdList->getBase()->getDevice());
    this->eventValue.counterBasedEvent = event->isCounterBased();
    this->eventValue.kernelCount = event->getKernelCount();
    this->eventValue.packetCount = event->getPacketsInUse();
    if (this->eventValue.counterBasedEvent) {
        this->eventValue.waitPackets = event->getInOrderExecInfo()->getNumDevicePartitionsToWait();
        this->eventValue.noopState = cmdList->isCbEventBoundToCmdList(event);
        if (cmdList->isQwordInOrderCounter()) {
            this->eventValue.loadRegImmCmds.reserve(2 * this->eventValue.waitPackets);
        }
        this->eventValue.isCbEventBoundToCmdList = cmdList->isCbEventBoundToCmdList(event);
        auto deviceCounterAlloc = event->getInOrderExecInfo()->getDeviceCounterAllocation();
        this->eventValue.cbEventDeviceCounterAllocation = cmdList->getDeviceCounterAllocForResidency(deviceCounterAlloc);
    } else {
        this->eventValue.waitPackets = event->getPacketsToWait();
    }
    this->eventValue.semWaitCmds.reserve(this->eventValue.waitPackets);
    this->desc.size = 0;
    return ZE_RESULT_SUCCESS;
}

ze_result_t Variable::addKernelArgUsage(const NEO::ArgDescriptor &kernelArg, IndirectObjectHeapOffset iohOffset, IndirectObjectHeapOffset iohFullOffset, SurfaceStateHeapOffset sshOffset,
                                        uint32_t slmArgSize, uint32_t slmArgOffsetValue,
                                        CommandBufferOffset walkerCmdOffset, MutableComputeWalker *mutableComputeWalker, bool inlineData) {
    if (desc.state != VariableDescriptor::State::defined) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    switch (desc.type) {
    default:
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    case VariableType::buffer: {
        const auto &arg = kernelArg.as<NEO::ArgDescPointer>();
        if (NEO::isValidOffset(arg.stateless)) {
            if (inlineData) {
                if (arg.stateless < mutableComputeWalker->getInlineDataSize()) {
                    bufferUsages.commandBufferOffsets.push_back(walkerCmdOffset + mutableComputeWalker->getInlineDataOffset() + arg.stateless);
                    auto walkerInlineFullOffset = reinterpret_cast<CommandBufferOffset>(mutableComputeWalker->getInlineDataPointer()) + arg.stateless;
                    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL captured buffer kernel arg patchlist inline %zx stateless offset %" PRIu16 "\n",
                                       walkerInlineFullOffset, arg.stateless);
                    bufferUsages.commandBufferWithoutOffset.push_back(walkerInlineFullOffset);
                } else {
                    auto statelessOffset = arg.stateless - mutableComputeWalker->getInlineDataSize();
                    bufferUsages.statelessIndirect.push_back(iohOffset + statelessOffset);
                    IndirectObjectHeapOffset iohFullStatelessOffset = iohFullOffset + statelessOffset;
                    bufferUsages.statelessWithoutOffset.push_back(iohFullStatelessOffset);

                    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL captured buffer kernel arg patchlist heap decreased by inline %zx stateless offset %" PRIu16 " decreased %zu\n",
                                       iohFullStatelessOffset, arg.stateless, statelessOffset);
                }
            } else {
                bufferUsages.statelessIndirect.push_back(iohOffset + arg.stateless);
                IndirectObjectHeapOffset iohFullStatelessOffset = iohFullOffset + arg.stateless;
                bufferUsages.statelessWithoutOffset.push_back(iohFullStatelessOffset);

                PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL captured buffer kernel arg patchlist heap %zx stateless offset %" PRIu16 "\n",
                                   iohFullStatelessOffset, arg.stateless);
            }
        }

        auto ret = addKernelArgUsageStatefulBuffer(kernelArg, iohOffset, sshOffset);
        if (ret != ZE_RESULT_SUCCESS) {
            return ret;
        }

        desc.size = sizeof(void *);
    } break;
    case VariableType::value: {
        return selectImmediateAddKernelArgUsageHandler(kernelArg, iohOffset, iohFullOffset, walkerCmdOffset, mutableComputeWalker, inlineData);
    } break;
    case VariableType::slmBuffer: {
        this->slmValue.slmSize = slmArgSize;
        this->slmValue.slmOffsetValue = slmArgOffsetValue;

        const auto &arg = kernelArg.as<NEO::ArgDescPointer>();
        if (NEO::isValidOffset(arg.slmOffset)) {
            this->slmValue.slmAlignment = arg.requiredSlmAlignment;
            if (inlineData) {
                if (arg.slmOffset < mutableComputeWalker->getInlineDataSize()) {
                    bufferUsages.commandBufferOffsets.push_back(walkerCmdOffset + mutableComputeWalker->getInlineDataOffset() + arg.slmOffset);
                    auto walkerInlineFullOffset = reinterpret_cast<CommandBufferOffset>(mutableComputeWalker->getInlineDataPointer()) + arg.slmOffset;
                    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL captured slm kernel arg patchlist inline %zx slm offset %" PRIu16 "\n",
                                       walkerInlineFullOffset, arg.slmOffset);
                    bufferUsages.commandBufferWithoutOffset.push_back(walkerInlineFullOffset);
                } else {
                    auto slmOffset = arg.slmOffset - mutableComputeWalker->getInlineDataSize();
                    bufferUsages.statelessIndirect.push_back(iohOffset + slmOffset);
                    IndirectObjectHeapOffset iohFullSlmOffset = iohFullOffset + slmOffset;
                    bufferUsages.statelessWithoutOffset.push_back(iohFullSlmOffset);

                    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL captured slm kernel arg patchlist heap decreased by inline %zx slm offset %" PRIu16 " decreased %zu\n",
                                       iohFullSlmOffset, arg.slmOffset, slmOffset);
                }
            } else {
                bufferUsages.statelessIndirect.push_back(iohOffset + arg.slmOffset);
                IndirectObjectHeapOffset iohFullSlmOffset = iohFullOffset + arg.slmOffset;
                bufferUsages.statelessWithoutOffset.push_back(iohFullSlmOffset);

                PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL captured buffer slm kernel arg patchlist heap %zx slm offset %" PRIu16 "\n",
                                   iohFullSlmOffset, arg.slmOffset);
            }
        }
    } break;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t Variable::addKernelArgUsageImmediateAsChunk(const NEO::ArgDescriptor &kernelArg, IndirectObjectHeapOffset iohOffset, IndirectObjectHeapOffset iohFullOffset,
                                                        CommandBufferOffset walkerCmdOffset, MutableComputeWalker *mutableComputeWalker, bool inlineData) {
    const auto &arg = kernelArg.as<NEO::ArgDescValue>();

    size_t fullSize = 0U;

    for (auto &ele : arg.elements) {
        auto &valueChunk = this->immediateValueChunks.emplace_back();
        size_t startOffset = ele.offset;
        size_t chunkSize = ele.size;

        valueChunk.size = chunkSize;
        valueChunk.sourceOffset = ele.sourceOffset;

        fullSize = valueChunk.sourceOffset + chunkSize;

        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL captured value kernel arg chunk source offset %zu size %zu start offset %zu\n", valueChunk.sourceOffset, chunkSize, startOffset);

        if (inlineData) {
            auto inlineSize = mutableComputeWalker->getInlineDataSize();
            // start of variable begins in CW inline data
            if (startOffset < inlineSize) {
                valueChunk.commandBufferUsageIndex = valueUsages.commandBufferWithoutOffset.size();
                auto walkerInlineFullOffset = reinterpret_cast<CommandBufferOffset>(mutableComputeWalker->getInlineDataPointer()) + startOffset;
                valueUsages.commandBufferWithoutOffset.push_back(walkerInlineFullOffset);
                PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL captured value kernel arg cmdbuffer patchlist full offset %zx stateless offset %zu\n",
                                   walkerInlineFullOffset, startOffset);

                valueUsages.commandBufferOffsets.push_back(walkerCmdOffset + mutableComputeWalker->getInlineDataOffset() + startOffset);
                // check immediate variable fits fully in inline
                if (startOffset + chunkSize <= inlineSize) {
                    // full size immediate fits in inline
                    valueUsages.commandBufferPatchSize.push_back(chunkSize);
                } else {
                    // first part fits in inline
                    size_t csSize = inlineSize - startOffset;
                    valueUsages.commandBufferPatchSize.push_back(csSize);

                    // cross-thread part of variable continues at the starts of heap
                    valueChunk.heapStatelessUsageIndex = valueUsages.statelessWithoutOffset.size();
                    valueUsages.statelessWithoutOffset.push_back(iohFullOffset);

                    valueUsages.statelessIndirect.push_back(iohOffset);
                    size_t heapSize = (startOffset + chunkSize) - inlineSize;
                    valueUsages.statelessIndirectPatchSize.push_back(heapSize);

                    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL captured value kernel arg partial heap patchlist full offset %zx\n",
                                       iohFullOffset);
                }
            } else {
                // immediate fits only in cross-thread, just decrease start offset by inline size
                startOffset -= inlineSize;

                valueChunk.heapStatelessUsageIndex = valueUsages.statelessWithoutOffset.size();
                valueUsages.statelessWithoutOffset.push_back(iohFullOffset + startOffset);

                valueUsages.statelessIndirect.push_back(iohOffset + startOffset);
                valueUsages.statelessIndirectPatchSize.push_back(chunkSize);

                PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL captured value kernel arg - inline - heap patchlist full offset %zx stateless offset %zu\n",
                                   (iohFullOffset + startOffset), startOffset);
            }
        } else {
            // no inline in kernel, no need to decrease start offset
            valueChunk.heapStatelessUsageIndex = valueUsages.statelessWithoutOffset.size();
            valueUsages.statelessWithoutOffset.push_back(iohFullOffset + startOffset);

            valueUsages.statelessIndirect.push_back(iohOffset + startOffset);
            valueUsages.statelessIndirectPatchSize.push_back(chunkSize);

            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL captured value kernel arg - clean - heap patchlist full offset %zx stateless offset %zu\n",
                               (iohFullOffset + startOffset), startOffset);
        }
    }

    desc.size = fullSize;
    return ZE_RESULT_SUCCESS;
}

ze_result_t Variable::setValue(size_t size, uint32_t flags, const void *argVal) {
    if (desc.isTemporary) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    handleFlags(flags);

    // size of value variable is evaluated in chunks
    // size of slm variable is changing dynamically
    if ((desc.type != VariableType::value && desc.type != VariableType::slmBuffer) && size != desc.size) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    switch (desc.type) {
    default:
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    case VariableType::buffer:
        return setBufferVariable(size, argVal);
    case VariableType::value:
        return setValueVariable(size, argVal);
    case VariableType::groupSize:
        return setGroupSizeVariable(size, argVal);
    case VariableType::groupCount:
        return setGroupCountVariable(size, argVal);
    case VariableType::signalEvent:
        return setSignalEventVariable(size, argVal);
    case VariableType::waitEvent:
        return setWaitEventVariable(size, argVal);
    case VariableType::globalOffset:
        return setGlobalOffsetVariable(size, argVal);
    case VariableType::slmBuffer:
        return setSlmBufferVariable(size, argVal);
    }
}

ze_result_t Variable::setBufferVariable(size_t size, const void *argVal) {
    DEBUG_BREAK_IF(size != sizeof(void *));
    auto argValue = argVal != nullptr ? *reinterpret_cast<void *const *>(argVal) : nullptr;
    auto device = cmdList->getBase()->getDevice();
    auto &commandContainer = cmdList->getBase()->getCmdContainer();

    auto oldBufferAlloc = desc.bufferAlloc;

    if (desc.state == State::initialized) {
        DEBUG_BREAK_IF(desc.bufferAlloc == nullptr);
        desc.bufferAlloc = nullptr;
        desc.state = State::defined;
    }
    DEBUG_BREAK_IF(desc.bufferAlloc != nullptr);

    GpuAddress gpuAddress = 0u;
    NEO::GraphicsAllocation *newBufferAlloc = nullptr;

    if (argValue != nullptr) {
        auto retVal = getBufferGpuAddress(argValue, device, newBufferAlloc, gpuAddress);
        if (retVal != ZE_RESULT_SUCCESS) {
            return retVal;
        }
    }
    desc.bufferAlloc = newBufferAlloc;
    desc.bufferGpuAddress = gpuAddress;
    desc.argValue = argValue;

    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL mutate kernel argument variable %p buffer gpuva %" PRIx64 " arg value %p from allocation %p\n",
                       this, gpuAddress, argValue, newBufferAlloc);

    if (bufferUsages.statelessWithoutOffset.size() > 0) {
        for (const auto statelessPatch : bufferUsages.statelessWithoutOffset) {
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL patching kernel argument buffer into heap offset %zx\n", statelessPatch);
            memcpy_s(reinterpret_cast<void *>(statelessPatch), sizeof(GpuAddress), &gpuAddress, sizeof(GpuAddress));
        }
    } else {
        auto iohCpuBase = commandContainer.getIndirectHeap(NEO::HeapType::indirectObject)->getCpuBase();
        for (const auto statelessOffset : bufferUsages.statelessIndirect) {
            memcpy_s(ptrOffset(iohCpuBase, statelessOffset), sizeof(GpuAddress), &gpuAddress, sizeof(GpuAddress));
        }
    }

    if (bufferUsages.commandBufferWithoutOffset.size() > 0) {
        for (const auto csPatch : bufferUsages.commandBufferWithoutOffset) {
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL patching kernel argument buffer into inline data cmd buffer offset %zx\n", csPatch);
            memcpy_s(reinterpret_cast<GpuAddress *>(csPatch), sizeof(GpuAddress), &gpuAddress, sizeof(GpuAddress));
        }
    } else {
        auto csCpuBase = commandContainer.getCommandStream()->getCpuBase();
        for (const auto csOffset : bufferUsages.commandBufferOffsets) {
            memcpy_s(ptrOffset(csCpuBase, csOffset), sizeof(GpuAddress), &gpuAddress, sizeof(GpuAddress));
        }
    }

    if (argValue != nullptr) {
        mutateStatefulBufferArg(gpuAddress, newBufferAlloc);
        desc.state = State::initialized;
    }
    updateAllocationResidency(oldBufferAlloc, newBufferAlloc);

    return ZE_RESULT_SUCCESS;
}
ze_result_t Variable::setValueVariable(size_t size, const void *argVal) {
    return selectImmediateSetValueHandler(size, argVal);
}

ze_result_t Variable::setValueVariableInChunks(size_t size, const void *argVal) {
    for (const auto &chunk : this->immediateValueChunks) {
        if (chunk.sourceOffset >= size) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        size_t maxBytesToCopy = size - chunk.sourceOffset;
        size_t bytesToCopy = std::min(chunk.size, maxBytesToCopy);

        const void *newValue = ptrOffset(argVal, chunk.sourceOffset);

        if (chunk.heapStatelessUsageIndex != std::numeric_limits<size_t>::max()) {
            size_t statelessSize = valueUsages.statelessIndirectPatchSize[chunk.heapStatelessUsageIndex];
            const bool splitBetween = chunk.commandBufferUsageIndex != std::numeric_limits<size_t>::max();
            const void *newHeapValue = newValue;
            if (splitBetween) {
                DEBUG_BREAK_IF(statelessSize >= bytesToCopy);
                // handle immediate value split between CW Inline Data and cross-thread indirect heap - here second part that landed in heap
                newHeapValue = reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(newHeapValue) + (bytesToCopy - statelessSize));
            } else {
                statelessSize = std::min(statelessSize, bytesToCopy);
            }

            void *destinationPtr = nullptr;

            if (valueUsages.statelessWithoutOffset.size() > 0) {
                destinationPtr = reinterpret_cast<void *>(valueUsages.statelessWithoutOffset[chunk.heapStatelessUsageIndex]);
            } else {
                auto iohCpuBase = cmdList->getBase()->getCmdContainer().getIndirectHeap(NEO::HeapType::indirectObject)->getCpuBase();

                const auto &statelessOffset = valueUsages.statelessIndirect[chunk.heapStatelessUsageIndex];
                destinationPtr = reinterpret_cast<void *>(ptrOffset(iohCpuBase, statelessOffset));
            }
            memcpy_s(destinationPtr, statelessSize,
                     newHeapValue, statelessSize);
        }

        if (chunk.commandBufferUsageIndex != std::numeric_limits<size_t>::max()) {
            size_t csSize = valueUsages.commandBufferPatchSize[chunk.commandBufferUsageIndex];
            csSize = std::min(csSize, bytesToCopy);

            void *destinationPtr = nullptr;
            if (valueUsages.commandBufferWithoutOffset.size() > 0) {
                destinationPtr = reinterpret_cast<void *>(valueUsages.commandBufferWithoutOffset[chunk.commandBufferUsageIndex]);
            } else {
                auto csCpuBase = cmdList->getBase()->getCmdContainer().getCommandStream()->getCpuBase();

                auto &csOffset = bufferUsages.commandBufferOffsets[chunk.commandBufferUsageIndex];
                destinationPtr = reinterpret_cast<void *>(ptrOffset(csCpuBase, csOffset));
            }
            memcpy_s(destinationPtr, csSize,
                     newValue, csSize);
        }
    }

    desc.state = State::initialized;
    return ZE_RESULT_SUCCESS;
}

ze_result_t Variable::setGroupSizeVariable(size_t size, const void *argVal) {
    const uint32_t *groupSize = reinterpret_cast<const uint32_t *>(argVal);
    if (groupSize[0] == kernelDispatch.groupSize[0] &&
        groupSize[1] == kernelDispatch.groupSize[1] &&
        groupSize[2] == kernelDispatch.groupSize[2]) {
        return ZE_RESULT_SUCCESS;
    }
    for (auto &vd : usedInDispatch) {
        vd->setGroupSize(groupSize, *cmdList->getBase()->getDevice()->getNEODevice(), desc.isStageCommit);
    }
    setCommitVariable();
    desc.state = State::initialized;
    setGroupSizeProperty(groupSize);
    return ZE_RESULT_SUCCESS;
}

ze_result_t Variable::setGroupCountVariable(size_t size, const void *argVal) {
    const uint32_t *groupCount = reinterpret_cast<const uint32_t *>(argVal);
    if (groupCount[0] == kernelDispatch.groupCount[0] &&
        groupCount[1] == kernelDispatch.groupCount[1] &&
        groupCount[2] == kernelDispatch.groupCount[2]) {
        return ZE_RESULT_SUCCESS;
    }
    for (auto &vd : usedInDispatch) {
        vd->setGroupCount(groupCount, *cmdList->getBase()->getDevice()->getNEODevice(), desc.isStageCommit);
    }
    setCommitVariable();
    desc.state = State::initialized;
    setGroupCountProperty(groupCount);
    return ZE_RESULT_SUCCESS;
}

ze_result_t Variable::setGlobalOffsetVariable(size_t size, const void *argVal) {
    const uint32_t *globalOffset = reinterpret_cast<const uint32_t *>(argVal);
    if (globalOffset[0] == kernelDispatch.globalOffset[0] &&
        globalOffset[1] == kernelDispatch.globalOffset[1] &&
        globalOffset[2] == kernelDispatch.globalOffset[2]) {
        return ZE_RESULT_SUCCESS;
    }
    for (auto &vd : usedInDispatch) {
        vd->setGlobalOffset(globalOffset);
    }

    desc.state = State::initialized;
    setGlobalOffsetProperty(globalOffset);
    return ZE_RESULT_SUCCESS;
}

ze_result_t Variable::setSignalEventVariable(size_t size, const void *argVal) {
    Event *newEvent = const_cast<Event *>(reinterpret_cast<const Event *>(argVal));
    if (newEvent == this->eventValue.event) {
        return ZE_RESULT_SUCCESS;
    }
    auto device = cmdList->getBase()->getDevice();

    if (this->eventValue.hasStandaloneProfilingNode) {
        newEvent->resetInOrderTimestampNode(device->getInOrderTimestampAllocator()->getTag(), cmdList->getBase()->getPartitionCount());
    }

    auto newEventAllocation = newEvent->getAllocation(device);
    auto oldEventAllocation = this->eventValue.eventPoolAllocation;

    updateAllocationResidency(oldEventAllocation, newEventAllocation);

    if (this->eventValue.counterBasedEvent && !this->eventValue.inOrderIncrementEvent) {
        this->cmdList->switchCounterBasedEvents(this->eventValue.inOrderExecBaseSignalValue, this->eventValue.inOrderAllocationOffset, newEvent);
    }

    uint64_t postSyncAddress = 0;
    uint64_t inOrderIncrementAddress = 0;

    if (newEventAllocation != nullptr) {
        postSyncAddress = newEvent->getGpuAddress(device);

        for (auto &mutableStoreDataImm : this->eventValue.storeDataImmCmds) {
            mutableStoreDataImm->setAddress(postSyncAddress);
        }
        for (auto &mutableSemaphoreWait : this->eventValue.semWaitCmds) {
            mutableSemaphoreWait->setSemaphoreAddress(postSyncAddress);
        }
        for (auto &mutableStoreRegMem : this->eventValue.storeRegMemCmds) {
            mutableStoreRegMem->setMemoryAddress(postSyncAddress);
        }
        if (this->eventValue.postSyncCmd) {
            this->eventValue.postSyncCmd->setPostSyncAddress(postSyncAddress);
        }
    }

    if (this->eventValue.inOrderIncrementEvent) {
        inOrderIncrementAddress = newEvent->getInOrderExecInfo()->getBaseDeviceAddress();
    }

    if (postSyncAddress != 0 || inOrderIncrementAddress != 0) {
        if (this->eventValue.walkerCmd) {
            this->eventValue.walkerCmd->setPostSyncAddress(postSyncAddress, inOrderIncrementAddress);
        }
    }

    newEvent->setKernelCount(this->eventValue.kernelCount);
    newEvent->setPacketsInUse(this->eventValue.packetCount);

    this->eventValue.event = newEvent;
    this->eventValue.eventPoolAllocation = newEventAllocation;
    if (this->eventValue.counterBasedEvent) {
        this->eventValue.inOrderExecBaseSignalValue = newEvent->getInOrderExecBaseSignalValue();
        this->eventValue.inOrderAllocationOffset = newEvent->getInOrderAllocationOffset();
    }
    desc.state = State::initialized;
    return ZE_RESULT_SUCCESS;
}

ze_result_t Variable::setWaitEventVariable(size_t size, const void *argVal) {
    Event *newEvent = const_cast<Event *>(reinterpret_cast<const Event *>(argVal));
    if (newEvent == this->eventValue.event) {
        return ZE_RESULT_SUCCESS;
    }
    auto device = cmdList->getBase()->getDevice();

    NEO::GraphicsAllocation *newEventAllocation = nullptr;
    NEO::GraphicsAllocation *oldEventAllocation = nullptr;

    NEO::GraphicsAllocation *newInOrderAllocation = nullptr;
    NEO::GraphicsAllocation *oldInOrderAllocation = nullptr;

    std::shared_ptr<NEO::InOrderExecInfo> *newInOrderInfo = nullptr;
    bool newCbEventBoundToCmdList = false;
    bool newNooped = true;
    if (newEvent != nullptr) {
        newInOrderInfo = &newEvent->getInOrderExecInfo();
        newNooped = false;
        newEventAllocation = newEvent->getAllocation(device);
        if (newEvent->isCounterBased()) {
            newCbEventBoundToCmdList = cmdList->isCbEventBoundToCmdList(newEvent);
            if (newCbEventBoundToCmdList) {
                newNooped = true;
            } else {
                auto deviceCounterAlloc = (*newInOrderInfo)->getDeviceCounterAllocation();
                newInOrderAllocation = cmdList->getDeviceCounterAllocForResidency(deviceCounterAlloc);
            }
        }
    }

    bool oldNooped = this->eventValue.noopState;
    if (this->eventValue.event != nullptr) {
        oldEventAllocation = this->eventValue.eventPoolAllocation;
        if (this->eventValue.counterBasedEvent) {
            if (!this->eventValue.isCbEventBoundToCmdList) {
                oldInOrderAllocation = this->eventValue.cbEventDeviceCounterAllocation;
            }
        }
    }

    updateAllocationResidency(oldEventAllocation, newEventAllocation);
    updateAllocationResidency(oldInOrderAllocation, newInOrderAllocation);

    if (this->eventValue.counterBasedEvent && (this->cmdList->getBase()->isHeaplessModeEnabled() || !(newEvent ? newEvent->hasInOrderTimestampNode() : false))) {
        if (oldNooped) {
            if (!newNooped) {
                // was nooped, needs programming - restore
                this->eventValue.noopState = false;
                // update in order info with patching, restore commands with new address
                auto waitAddress = (*newInOrderInfo)->getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset();
                setCbWaitEventUpdateOperation(CbWaitEventOperationType::restore, waitAddress, newInOrderInfo);
            }
        } else {
            if (newNooped) {
                // was programed, needs noop
                this->eventValue.noopState = true;
                setCbWaitEventUpdateOperation(CbWaitEventOperationType::noop, 0, newInOrderInfo);
            } else {
                // was programed, needs update address and new in order info
                auto waitAddress = (*newInOrderInfo)->getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset();
                setCbWaitEventUpdateOperation(CbWaitEventOperationType::set, waitAddress, newInOrderInfo);
            }
        }
    } else {
        if (newEventAllocation != nullptr) {
            auto waitAddress = newEvent->getGpuAddress(device);
            for (auto &mutableSemWait : this->eventValue.semWaitCmds) {
                if (oldNooped) {
                    mutableSemWait->restoreWithSemaphoreAddress(waitAddress);
                } else {
                    mutableSemWait->setSemaphoreAddress(waitAddress);
                }
            }
        }
        if (newNooped) {
            this->eventValue.noopState = true;
            for (auto &mutableSemWait : this->eventValue.semWaitCmds) {
                mutableSemWait->noop();
            }
        } else {
            this->eventValue.noopState = false;
        }
    }

    this->eventValue.event = newEvent;
    this->eventValue.eventPoolAllocation = newEventAllocation;
    if (this->eventValue.counterBasedEvent) {
        this->eventValue.isCbEventBoundToCmdList = newCbEventBoundToCmdList;
        this->eventValue.cbEventDeviceCounterAllocation = newInOrderAllocation;
    }
    desc.state = State::initialized;
    return ZE_RESULT_SUCCESS;
}

void Variable::setCbWaitEventUpdateOperation(CbWaitEventOperationType operation, uint64_t waitAddress, std::shared_ptr<NEO::InOrderExecInfo> *eventInOrderInfo) {
    bool qwordInUse = cmdList->isQwordInOrderCounter();

    for (auto &mutableSemWait : this->eventValue.semWaitCmds) {
        if (operation == CbWaitEventOperationType::set) {
            mutableSemWait->setSemaphoreAddress(waitAddress);
        } else if (operation == CbWaitEventOperationType::noop) {
            mutableSemWait->noop();
        } else if (operation == CbWaitEventOperationType::restore) {
            mutableSemWait->restoreWithSemaphoreAddress(waitAddress);
        }

        if (!qwordInUse && eventInOrderInfo && eventInOrderInfo->get()->isExternalMemoryExecInfo()) {
            if (operation == CbWaitEventOperationType::set || operation == CbWaitEventOperationType::restore) {
                mutableSemWait->setSemaphoreValue(eventInOrderInfo->get()->getCounterValue());
            }
        }

        auto semWaitIndex = mutableSemWait->getInOrderPatchListIndex();
        if (semWaitIndex != std::numeric_limits<size_t>::max()) {
            if (operation == CbWaitEventOperationType::set || operation == CbWaitEventOperationType::restore) {
                cmdList->updateInOrderExecInfo(semWaitIndex, eventInOrderInfo, false);
            } else if (operation == CbWaitEventOperationType::noop) {
                cmdList->disablePatching(semWaitIndex);
            }
        }
    }
    if (qwordInUse) {
        size_t lastIndex = std::numeric_limits<size_t>::max();
        uint32_t cmdIndex = 0;
        for (auto &mutableLoadRegImm : this->eventValue.loadRegImmCmds) {
            if (operation == CbWaitEventOperationType::noop) {
                mutableLoadRegImm->noop();
            } else if (operation == CbWaitEventOperationType::restore) {
                mutableLoadRegImm->restore();
            }

            if (eventInOrderInfo && eventInOrderInfo->get()->isExternalMemoryExecInfo()) {
                if (operation == CbWaitEventOperationType::set || operation == CbWaitEventOperationType::restore) {
                    uint32_t waitValue = cmdIndex == 0 ? getLowPart(eventInOrderInfo->get()->getCounterValue()) : getHighPart(eventInOrderInfo->get()->getCounterValue());
                    mutableLoadRegImm->setValue(waitValue);
                }
            }
            cmdIndex++;

            auto lriIndex = mutableLoadRegImm->getInOrderPatchListIndex();
            // there are 2 LRI commands for the same in order patchlist index, skip second update of in order exec info
            if (lriIndex == lastIndex) {
                continue;
            }
            if (operation == CbWaitEventOperationType::set || operation == CbWaitEventOperationType::restore) {
                cmdList->updateInOrderExecInfo(lriIndex, eventInOrderInfo, false);
            } else if (operation == CbWaitEventOperationType::noop) {
                cmdList->disablePatching(lriIndex);
            }
            lastIndex = lriIndex;
        }
    }
}

void Variable::commitVariable() {
    for (auto &vd : usedInDispatch) {
        vd->commitChanges(*cmdList->getBase()->getDevice()->getNEODevice());
    }
    desc.commitRequired = false;
}

void Variable::updateAllocationResidency(NEO::GraphicsAllocation *oldAllocation, NEO::GraphicsAllocation *newAllocation) {
    if (oldAllocation != newAllocation) {
        cmdList->addToResidencyContainer(newAllocation);
        cmdList->removeFromResidencyContainer(oldAllocation);
    }
}

void Variable::updateCmdListNoopPatchData(size_t noopPatchIndex, void *newCpuPtr, size_t newPatchSize, size_t newOffset, uint64_t newGpuAddress) {
    cmdList->updateCmdListNoopPatchData(noopPatchIndex, newCpuPtr, newPatchSize, newOffset, newGpuAddress);
}

size_t Variable::createNewCmdListNoopPatchData(void *newCpuPtr, size_t newPatchSize, size_t newOffset, uint64_t newGpuAddress) {
    return cmdList->createNewCmdListNoopPatchData(newCpuPtr, newPatchSize, newOffset, newGpuAddress);
}

void Variable::fillCmdListNoopPatchData(size_t noopPatchIndex, void *&cpuPtr, size_t &patchSize, size_t &offset, uint64_t &gpuAddress) {
    cmdList->fillCmdListNoopPatchData(noopPatchIndex, cpuPtr, patchSize, offset, gpuAddress);
}

bool Variable::isCooperativeVariable() const {
    if (usedInDispatch.empty()) {
        return false;
    }
    return usedInDispatch[0]->isCooperativeDispatch();
}

ze_result_t Variable::setSlmBufferVariable(size_t size, const void *argVal) {
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL mutate kernel slm argument variable %p new size %u, old size %u\n",
                       this, size, this->slmValue.slmSize);
    if (this->slmValue.slmSize != static_cast<SlmOffset>(size)) {
        this->slmValue.slmSize = static_cast<SlmOffset>(size);

        processVariableDispatchForSlm();
    }

    desc.state = State::initialized;
    return ZE_RESULT_SUCCESS;
}

void Variable::setNextSlmVariableOffset(SlmOffset nextSlmOffset) {
    SlmOffset alignedNewOffset = alignUp<SlmOffset>(nextSlmOffset, this->slmValue.slmAlignment);
    bool patchSlmOffset = alignedNewOffset != this->slmValue.slmOffsetValue;

    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL mutate kernel slm argument variable %p new slm offset %u, old slm offset %u\n",
                       this, alignedNewOffset, this->slmValue.slmOffsetValue);

    if (patchSlmOffset) {
        auto &commandContainer = cmdList->getBase()->getCmdContainer();

        if (bufferUsages.statelessWithoutOffset.size() > 0) {
            for (const auto statelessPatch : bufferUsages.statelessWithoutOffset) {
                PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL patching kernel slm argument buffer into heap offset %zx\n", statelessPatch);
                *reinterpret_cast<SlmOffset *>(statelessPatch) = alignedNewOffset;
            }
        } else {
            auto iohCpuBase = commandContainer.getIndirectHeap(NEO::HeapType::indirectObject)->getCpuBase();
            for (const auto statelessOffset : bufferUsages.statelessIndirect) {
                *reinterpret_cast<SlmOffset *>(ptrOffset(iohCpuBase, statelessOffset)) = alignedNewOffset;
            }
        }

        if (bufferUsages.commandBufferWithoutOffset.size() > 0) {
            for (const auto csPatch : bufferUsages.commandBufferWithoutOffset) {
                PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL patching kernel slm argument into inline data cmd buffer offset %zx\n", csPatch);
                *reinterpret_cast<SlmOffset *>(csPatch) = alignedNewOffset;
            }
        } else {
            auto csCpuBase = commandContainer.getCommandStream()->getCpuBase();
            for (const auto csOffset : bufferUsages.commandBufferOffsets) {
                *reinterpret_cast<SlmOffset *>(ptrOffset(csCpuBase, csOffset)) = alignedNewOffset;
            }
        }

        this->slmValue.slmOffsetValue = alignedNewOffset;
        processVariableDispatchForSlm();
    }
}

void Variable::processVariableDispatchForSlm() {
    SlmOffset nextSlmOffset = this->slmValue.slmOffsetValue + this->slmValue.slmSize;
    if (this->slmValue.nextSlmVariable != nullptr) {
        this->slmValue.nextSlmVariable->setNextSlmVariableOffset(nextSlmOffset);
    } else {
        SlmOffset slmArgsTotalSize = static_cast<SlmOffset>(alignUp(nextSlmOffset, MemoryConstants::kiloByte));
        for (auto &vd : usedInDispatch) {
            vd->setSlmSize(slmArgsTotalSize, *cmdList->getBase()->getDevice()->getNEODevice(), desc.isStageCommit);
        }
        setCommitVariable();
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL mutate kernel slm argument variable %p aligned total arg size %u\n",
                           this, slmArgsTotalSize);
    }
}

uint32_t Variable::getAlignedSlmSize(uint32_t slmSize) {
    return cmdList->getBase()->getDevice()->getGfxCoreHelper().alignSlmSize(slmSize);
}

void Variable::addCommitVariableToBaseCmdList() {
    cmdList->addVariableToCommitList(this);
}

} // namespace L0::MCL
