/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/indirect_heap/indirect_heap.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/mutable_cmdlist/helper.h"
#include "level_zero/core/source/mutable_cmdlist/mcl_kernel_ext.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_imp.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_dispatch.h"
#include "level_zero/core/source/mutable_cmdlist/variable.h"
#include "level_zero/experimental/source/mutable_cmdlist/program/mcl_decoder.h"
#include "level_zero/experimental/source/mutable_cmdlist/program/mcl_encoder.h"
#include "level_zero/experimental/source/mutable_cmdlist/program/mcl_program.h"

#include "implicit_args.h"

#include <algorithm>
#include <memory>

namespace L0 {
namespace MCL {

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

ze_result_t MutableCommandListImp::getVariablesList(uint32_t *pVarCount, Variable **outVariables) {
    *pVarCount = static_cast<uint32_t>(variableStorage.size());
    if (outVariables != nullptr) {
        for (size_t i = 0; i < variableStorage.size(); i++) {
            outVariables[i] = variableStorage[i].get();
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t MutableCommandListImp::getLabel(const InterfaceLabelDescriptor *labelDesc, Label **outLabel) {
    *outLabel = nullptr;

    auto alignment = labelDesc->alignment;
    if ((alignment & (alignment - 1)) != 0U) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    std::string labelName = labelDesc->name == nullptr ? "" : std::string(labelDesc->name);
    auto it = labelMap.find(labelName);
    if (it != labelMap.end()) {
        *outLabel = it->second;
        return ZE_RESULT_SUCCESS;
    }

    auto label = std::unique_ptr<Label>(Label::create(this->base, labelDesc));
    if (false == labelName.empty()) {
        labelMap.insert(std::make_pair(labelName, label.get()));
    }

    *outLabel = label.get();
    labelStorage.push_back(std::move(label));

    return ZE_RESULT_SUCCESS;
}

ze_result_t MutableCommandListImp::tempMemSetElementCount(size_t elementCount) {
    for (auto &var : tempMem.variables) {
        auto &desc = var->getDesc();
        if (desc.isScalable) {
            desc.size = desc.eleSize * elementCount;
        }
    }
    tempMem.eleCount = elementCount;

    return ZE_RESULT_SUCCESS;
}

ze_result_t MutableCommandListImp::tempMemGetSize(size_t *tempMemSize) {
    size_t size = 0U;
    for (auto &var : tempMem.variables) {
        var->getDesc().offset = size;
        size += var->getDesc().size;
    }
    *tempMemSize = size;

    return ZE_RESULT_SUCCESS;
}

ze_result_t MutableCommandListImp::tempMemSet(const void *pTempMem) {
    auto tempMemBuffor = *reinterpret_cast<void *const *>(pTempMem);

    NEO::GraphicsAllocation *alloc = nullptr;
    GpuAddress gpuAddress = undefined<GpuAddress>;
    auto result = getBufferGpuAddress(tempMemBuffor, base->getDevice(), alloc, gpuAddress);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    addToResidencyContainer(alloc);

    OffsetT allocOffset = 0U;
    for (auto &var : tempMem.variables) {
        GpuAddress bufferAddr = gpuAddress + allocOffset;

        auto &desc = var->getDesc();
        auto iohCpuBase = base->getCmdContainer().getIndirectHeap(NEO::HeapType::indirectObject)->getCpuBase();
        for (const auto statelessOffset : var->getBufferUsages().statelessIndirect) {
            *reinterpret_cast<CpuAddress *>(ptrOffset(iohCpuBase, statelessOffset)) = bufferAddr;
        }

        auto csCpuBase = base->getCmdContainer().getCommandStream()->getCpuBase();
        for (const auto csOffset : var->getBufferUsages().commandBufferOffsets) {
            *reinterpret_cast<CpuAddress *>(ptrOffset(csCpuBase, csOffset)) = bufferAddr;
        }

        setBufferSurfaceState(reinterpret_cast<void *>(bufferAddr), alloc, var);

        allocOffset += desc.size;
    }

    return ZE_RESULT_SUCCESS;
}

void MutableCommandListImp::createNativeBinary(ArrayRef<const uint8_t> module) {
    using namespace Program::Encoder;
    auto cs = base->getCmdContainer().getCommandStream();
    ArrayRef<const uint8_t> cmdBuffer{reinterpret_cast<const uint8_t *>(cs->getCpuBase()), cs->getUsed()};

    MclEncoder::MclEncoderArgs args;
    if (base->getCmdListStateBaseAddressTracking()) {
        args.explicitIh = true;
        if (base->getCmdListHeapAddressModel() == NEO::HeapAddressModel::privateHeaps) {
            args.explicitSsh = true;
        }
    }
    args.cmdBuffer = cmdBuffer;
    args.module = module;
    args.kernelData = &kernelData;
    args.sba = &sbaVec;
    args.dispatches = &dispatches;
    args.vars = &variableStorage;
    nativeBinary = MclEncoder::encode(args);
}

ze_result_t MutableCommandListImp::getNativeBinary(uint8_t *pBinary, size_t *pBinarySize, const uint8_t *pModule, size_t moduleSize) {
    if (nativeBinary.empty()) {
        ArrayRef<const uint8_t> module = {pModule, moduleSize};
        createNativeBinary(module);
    }

    if (pBinary != nullptr && *pBinarySize >= nativeBinary.size()) {
        memcpy_s(pBinary, *pBinarySize, nativeBinary.data(), nativeBinary.size());
    }

    *pBinarySize = nativeBinary.size();
    return ZE_RESULT_SUCCESS;
}

ze_result_t MutableCommandListImp::loadFromBinary(const uint8_t *pBinary, const size_t binarySize) {
    Program::Decoder::MclDecoderArgs args;
    args.binary = ArrayRef<const uint8_t>{pBinary, binarySize};
    args.variableStorage = &variableStorage;
    args.variableMap = &variableMap;
    args.tempVariables = &tempMem.variables;
    args.kernelData = &kernelData;
    args.kernelDispatches = &dispatches;
    args.sbaVec = &sbaVec;
    args.device = base->getDevice();
    args.cmdContainer = &base->getCmdContainer();
    args.allocs = &allocs;
    args.mcl = this;
    args.cmdListEngine = base->getEngineGroupType();
    args.partitionCount = base->getPartitionCount();
    args.heapless = this->getBase()->isHeaplessModeEnabled();
    args.mutableWalkerCmds = &mutableWalkerCmds;
    Program::Decoder::MclDecoder::decode(args);
    if (base->getCmdListStateBaseAddressTracking()) {
        auto &sbaRequiredProperties = base->getRequiredStreamState().stateBaseAddress;
        if (base->getCmdListHeapAddressModel() == NEO::HeapAddressModel::privateHeaps) {
            sbaRequiredProperties.setPropertyStatelessMocs(base->getDevice()->getMOCS(true, false) >> 1);

            auto ssh = base->getCmdContainer().getIndirectHeap(NEO::HeapType::surfaceState);
            if (ssh) {
                sbaRequiredProperties.setPropertiesBindingTableSurfaceState(ssh->getHeapGpuBase(), ssh->getHeapSizeInPages(),
                                                                            ssh->getHeapGpuBase(), ssh->getHeapSizeInPages());
            }
        }
        auto ioh = base->getCmdContainer().getIndirectHeap(NEO::HeapType::indirectObject);
        sbaRequiredProperties.setPropertiesIndirectState(ioh->getHeapGpuBase(), ioh->getHeapSizeInPages());

        base->getFinalStreamState().stateBaseAddress = sbaRequiredProperties;
    }
    if (this->base->getCmdListBatchBufferFlag()) {
        this->base->close();
        this->baseCmdListClosed = true;
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace MCL
} // namespace L0
