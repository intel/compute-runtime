/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/kernel_arg_descriptor.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist.h"
#include "level_zero/core/source/mutable_cmdlist/usage.h"
#include "level_zero/core/source/mutable_cmdlist/variable.h"
#include "level_zero/experimental/source/mutable_cmdlist/program/mcl_decoder.h"

namespace L0::MCL {
using State = VariableDescriptor::State;

Variable *Variable::createFromInfo(ze_command_list_handle_t hCmdList, Program::Decoder::VarInfo &varInfo) {
    auto var = new Variable(MutableCommandList::fromHandle(hCmdList));

    auto &desc = var->getDesc();
    desc.type = varInfo.type;
    desc.size = varInfo.size;
    desc.isTemporary = varInfo.tmp;
    desc.isScalable = varInfo.scalable;
    desc.name = varInfo.name;
    if (varInfo.type == VariableType::buffer) {
        var->setBufferUsages(std::move(varInfo.bufferUsages));
    } else if (varInfo.type == VariableType::value) {
        var->setValueUsages(std::move(varInfo.valueUsages));
    }
    desc.apiVariable = true;

    return var;
}

ze_result_t Variable::addKernelArgUsageImmediateAsContinuous(const NEO::ArgDescriptor &kernelArg, IndirectObjectHeapOffset iohOffset, IndirectObjectHeapOffset iohFullOffset,
                                                             CommandBufferOffset walkerCmdOffset, MutableComputeWalker *mutableComputeWalker, bool inlineData) {
    const auto &arg = kernelArg.as<NEO::ArgDescValue>();

    size_t fullSize = 0U;
    size_t startOffset = arg.elements[0].offset;
    bool isContinuous = true;
    for (auto &ele : arg.elements) {
        isContinuous &= (ele.sourceOffset == fullSize);
        isContinuous &= ((ele.offset - startOffset) == fullSize);
        fullSize += ele.size;
    }

    if (false == isContinuous) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (inlineData) {
        auto inlineSize = mutableComputeWalker->getInlineDataSize();
        if (startOffset < inlineSize) {
            valueUsages.commandBufferOffsets.push_back(walkerCmdOffset + mutableComputeWalker->getInlineDataOffset() + startOffset);
            auto walkerInlineFullOffset = reinterpret_cast<CommandBufferOffset>(mutableComputeWalker->getInlineDataPointer()) + startOffset;
            valueUsages.commandBufferWithoutOffset.push_back(walkerInlineFullOffset);
            // check immediate variable fits only in inline
            if (startOffset + fullSize <= inlineSize) {
                // full size immediate fits in inline
                valueUsages.commandBufferPatchSize.push_back(fullSize);
            } else {
                size_t csSize = inlineSize - startOffset;
                valueUsages.commandBufferPatchSize.push_back(csSize);

                // cross-thread continues at the starts of heap chunk
                valueUsages.statelessIndirect.push_back(iohOffset);
                valueUsages.statelessWithoutOffset.push_back(iohFullOffset);
                size_t heapSize = (startOffset + fullSize) - inlineSize;
                valueUsages.statelessIndirectPatchSize.push_back(heapSize);
            }
        } else {
            // immediate fits only in cross-thread, just decrease start offset by inline size
            startOffset -= inlineSize;
            valueUsages.statelessIndirect.push_back(iohOffset + startOffset);
            valueUsages.statelessWithoutOffset.push_back(iohFullOffset + startOffset);
            valueUsages.statelessIndirectPatchSize.push_back(fullSize);
        }
    } else {
        valueUsages.statelessIndirect.push_back(iohOffset + startOffset);
        valueUsages.statelessWithoutOffset.push_back(iohFullOffset + startOffset);
        valueUsages.statelessIndirectPatchSize.push_back(fullSize);
    }
    desc.size = fullSize;
    return ZE_RESULT_SUCCESS;
}

ze_result_t Variable::addCsUsage(CommandBufferOffset csOffset, CommandBufferOffset csFullOffset) {
    if (false == isType(VariableType::buffer)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    bufferUsages.commandBufferOffsets.push_back(csOffset);
    bufferUsages.commandBufferWithoutOffset.push_back(csFullOffset);
    return ZE_RESULT_SUCCESS;
}

ze_result_t Variable::setValueVariableContinuous(size_t size, const void *argVal) {
    if (size != desc.size) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (valueUsages.statelessWithoutOffset.size() > 0) {
        size_t statelessSizeIndex = 0;
        for (const auto &statelessPatch : valueUsages.statelessWithoutOffset) {
            auto statelessSize = valueUsages.statelessIndirectPatchSize[statelessSizeIndex];

            const void *newValue = argVal;
            if (statelessSize < desc.size) {
                newValue = reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(newValue) + (desc.size - statelessSize));
            }

            memcpy_s(reinterpret_cast<void *>(statelessPatch), statelessSize,
                     newValue, statelessSize);
            statelessSizeIndex++;
        }
    } else {
        size_t statelessSizeIndex = 0;
        auto iohCpuBase = cmdList->getBase()->getCmdContainer().getIndirectHeap(NEO::HeapType::indirectObject)->getCpuBase();
        for (const auto &statelessOffset : valueUsages.statelessIndirect) {
            auto statelessSize = valueUsages.statelessIndirectPatchSize[statelessSizeIndex];

            const void *newValue = argVal;
            if (statelessSize < desc.size) {
                newValue = reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(newValue) + (desc.size - statelessSize));
            }

            memcpy_s(reinterpret_cast<void *>(ptrOffset(iohCpuBase, statelessOffset)), statelessSize,
                     newValue, statelessSize);
            statelessSizeIndex++;
        }
    }

    if (valueUsages.commandBufferWithoutOffset.size() > 0) {
        size_t csSizeIndex = 0;
        for (const auto &csInlinePatch : valueUsages.commandBufferWithoutOffset) {
            auto csSize = valueUsages.commandBufferPatchSize[csSizeIndex];

            memcpy_s(reinterpret_cast<void *>(csInlinePatch), csSize,
                     argVal, csSize);
            csSizeIndex++;
        }
    } else {
        size_t csSizeIndex = 0;
        auto csCpuBase = cmdList->getBase()->getCmdContainer().getCommandStream()->getCpuBase();
        for (const auto csOffset : valueUsages.commandBufferOffsets) {
            auto csSize = valueUsages.commandBufferPatchSize[csSizeIndex];

            memcpy_s(reinterpret_cast<void *>(ptrOffset(csCpuBase, csOffset)), csSize,
                     argVal, csSize);
            csSizeIndex++;
        }
    }

    desc.state = State::initialized;
    return ZE_RESULT_SUCCESS;
}

void Variable::handleFlags(uint32_t flags) {
    if ((flags & Variable::directFlag) != 0) {
        cmdList->toggleCommandListUpdated();
    }
}

ze_result_t Variable::selectImmediateSetValueHandler(size_t size, const void *argVal) {
    if (desc.immediateValueChunks) {
        return setValueVariableInChunks(size, argVal);
    } else {
        return setValueVariableContinuous(size, argVal);
    }
}

ze_result_t Variable::selectImmediateAddKernelArgUsageHandler(const NEO::ArgDescriptor &kernelArg, IndirectObjectHeapOffset iohOffset, IndirectObjectHeapOffset iohFullOffset,
                                                              CommandBufferOffset walkerCmdOffset, MutableComputeWalker *mutableComputeWalker, bool inlineData) {
    if (desc.immediateValueChunks) {
        return addKernelArgUsageImmediateAsChunk(kernelArg, iohOffset, iohFullOffset, walkerCmdOffset, mutableComputeWalker, inlineData);
    } else {
        return addKernelArgUsageImmediateAsContinuous(kernelArg, iohOffset, iohFullOffset, walkerCmdOffset, mutableComputeWalker, inlineData);
    }
}

void Variable::setDescExperimentalValues(const InterfaceVariableDescriptor *ifaceVarDesc) {
    desc.name = ifaceVarDesc->name == nullptr ? "" : std::string(ifaceVarDesc->name);
    desc.apiVariable = ifaceVarDesc->api;

    if (ifaceVarDesc->isTemporary) {
        desc.isTemporary = true;
        if (ifaceVarDesc->isConstSize) {
            desc.size = ifaceVarDesc->size;
        } else if (ifaceVarDesc->isScalable) {
            desc.isScalable = true;
            desc.eleSize = ifaceVarDesc->size;
        }
    }
}

ze_result_t Variable::addKernelArgUsageStatefulBuffer(const NEO::ArgDescriptor &kernelArg, IndirectObjectHeapOffset iohOffset, SurfaceStateHeapOffset sshOffset) {
    if (desc.apiVariable) {
        const auto &arg = kernelArg.as<NEO::ArgDescPointer>();
        if (sshOffset != undefined<SurfaceStateHeapOffset>) {
            if (NEO::isValidOffset(arg.bufferOffset)) {
                if (NEO::isValidOffset(arg.bindful)) {
                    bufferUsages.bindful.push_back(sshOffset + arg.bindful);
                } else if (NEO::isValidOffset(arg.bindless)) {
                    bufferUsages.bindless.push_back(iohOffset + arg.bindless);
                } else {
                    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
                }
                bufferUsages.bufferOffset.push_back(iohOffset + arg.bufferOffset);
            } else {
                if (NEO::isValidOffset(arg.bindful)) {
                    bufferUsages.bindfulWithoutOffset.push_back(sshOffset + arg.bindful);
                } else if (NEO::isValidOffset(arg.bindless)) {
                    bufferUsages.bindlessWithoutOffset.push_back(iohOffset + arg.bindless);
                } else {
                    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
                }
            }
        }
    }
    return ZE_RESULT_SUCCESS;
}

void Variable::mutateStatefulBufferArg(GpuAddress bufferGpuAddress, NEO::GraphicsAllocation *bufferAllocation) {
    if (desc.apiVariable) {
        cmdList->setBufferSurfaceState(reinterpret_cast<void *>(bufferGpuAddress), bufferAllocation, this);
    }
}

} // namespace L0::MCL
