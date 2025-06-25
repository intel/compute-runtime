/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist.h"
#include "level_zero/core/source/mutable_cmdlist/variable.h"
#include "level_zero/core/source/mutable_cmdlist/variable_dispatch.h"
#include "level_zero/driver_experimental/mcl_ext/zex_mutable_cmdlist_ext.h"
#include "level_zero/experimental/source/mutable_cmdlist/label.h"

#include <exception>
#include <new>

namespace L0::MCL {

MclAluReg getMclAluReg(zex_mcl_alu_reg_t reg) {
    return static_cast<MclAluReg>(reg);
}

zex_variable_state_t getZeVariableState(const VariableDescriptor &varDesc) {
    switch (varDesc.state) {
    default:
        DEBUG_BREAK_IF(true);
        return ZEX_VARIABLE_STATE_DECLARED;
    case VariableDescriptor::State::declared:
        return ZEX_VARIABLE_STATE_DECLARED;
    case VariableDescriptor::State::defined:
        return ZEX_VARIABLE_STATE_DEFINED;
    case VariableDescriptor::State::initialized:
        return ZEX_VARIABLE_STATE_INITIALIZED;
    }
}

zex_variable_type_t getZeVariableType(const VariableDescriptor &varDesc) {
    switch (varDesc.type) {
    default:
        DEBUG_BREAK_IF(true);
        return ZEX_VARIABLE_TYPE_NONE;
    case VariableType::none:
        return ZEX_VARIABLE_TYPE_NONE;
    case VariableType::buffer:
        return ZEX_VARIABLE_TYPE_BUFFER;
    case VariableType::value:
        return ZEX_VARIABLE_TYPE_VALUE;
    case VariableType::groupSize:
        return ZEX_VARIABLE_TYPE_GROUP_SIZE;
    case VariableType::groupCount:
        return ZEX_VARIABLE_TYPE_GROUP_COUNT;
    case VariableType::globalOffset:
        return ZEX_VARIABLE_TYPE_GLOBAL_OFFSET;
    case VariableType::signalEvent:
        return ZEX_VARIABLE_TYPE_SIGNAL_EVENT;
    case VariableType::waitEvent:
        return ZEX_VARIABLE_TYPE_WAIT_EVENT;
    case VariableType::slmBuffer:
        return ZEX_VARIABLE_TYPE_SLM_BUFFER;
    }
}

zex_variable_flags_t getZeVariableFlags(const VariableDescriptor &varDesc) {
    zex_variable_flags_t flags = 0U;
    auto setFlagIfTrue = [&flags](zex_variable_flag_t flag, bool isSet) {
        if (isSet) {
            flags |= flag;
        }
    };
    setFlagIfTrue(ZEX_VARIABLE_FLAGS_TEMPORARY, varDesc.isTemporary);
    setFlagIfTrue(ZEX_VARIABLE_FLAGS_SCALABLE, varDesc.isScalable);
    return flags;
}

ze_result_t getVariableInfoFromDesc(const VariableDescriptor &varDesc, Variable *variable, zex_variable_info_t &info) {
    info.handle = variable->toHandle();
    info.name = varDesc.name.c_str();
    info.size = varDesc.size;
    info.state = getZeVariableState(varDesc);
    info.type = getZeVariableType(varDesc);
    info.flags = getZeVariableFlags(varDesc);

    return ZE_RESULT_SUCCESS;
}

ze_result_t copyVariableInfoFromVariableList(MutableCommandList *mcl, uint32_t *pVariablesCount, const zex_variable_info_t **pVariablesInfos) {

    mcl->getVariablesList(pVariablesCount, nullptr);

    if (pVariablesInfos != nullptr) {
        std::vector<Variable *> variables(*pVariablesCount);
        mcl->getVariablesList(pVariablesCount, variables.data());

        for (uint32_t i = 0; i < *pVariablesCount; i++) {
            getVariableInfoFromDesc(variables[i]->getDesc(), variables[i], variables[i]->info);
            pVariablesInfos[i] = &variables[i]->info;
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL
zexCommandListGetVariable(
    ze_command_list_handle_t hCmdList,
    const zex_variable_desc_t *pVariableDescriptor,
    zex_variable_handle_t *phVariable) {
    hCmdList = toInternalType(hCmdList);
    if (nullptr == hCmdList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (nullptr == pVariableDescriptor) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (nullptr == phVariable) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto outVariable = reinterpret_cast<Variable **>(phVariable);
    InterfaceVariableDescriptor varDesc = {};
    varDesc.name = pVariableDescriptor->name;

    if (pVariableDescriptor->pNext != nullptr) {
        auto tempVarDesc = reinterpret_cast<const zex_temp_variable_desc_t *>(pVariableDescriptor->pNext);
        if (static_cast<uint32_t>(tempVarDesc->stype) == ZEX_STRUCTURE_TYPE_TEMP_VARIABLE_DESCRIPTOR) {
            varDesc.isTemporary = true;
            varDesc.size = tempVarDesc->size;
            varDesc.isScalable = (tempVarDesc->flags & ZEX_TEMP_VARIABLE_FLAGS_IS_SCALABLE) != 0;
            varDesc.isConstSize = (tempVarDesc->flags & ZEX_TEMP_VARIABLE_FLAGS_IS_CONST_SIZE) != 0;
        }
    }
    return MutableCommandList::fromHandle(hCmdList)->getVariable(&varDesc, outVariable);
}

ze_result_t ZE_APICALL
zexKernelSetArgumentVariable(
    ze_kernel_handle_t hKernel,
    uint32_t argIndex,
    zex_variable_handle_t hVariable) {
    hKernel = toInternalType(hKernel);
    if (nullptr == hKernel) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (nullptr == hVariable) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return Variable::fromHandle(hVariable)->setAsKernelArg(hKernel, argIndex);
}

ze_result_t ZE_APICALL
zexVariableSetValue(
    zex_variable_handle_t hVariable,
    uint32_t flags,
    size_t valueSize,
    const void *pValue) {
    if (nullptr == hVariable) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    flags |= Variable::directFlag;
    return Variable::fromHandle(hVariable)->setValue(valueSize, flags, pValue);
}

ze_result_t ZE_APICALL
zexCommandListGetLabel(
    ze_command_list_handle_t hCommandList,
    const zex_label_desc_t *pLabelDesc,
    zex_label_handle_t *phLabel) {
    hCommandList = toInternalType(hCommandList);
    if (nullptr == hCommandList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (nullptr == pLabelDesc) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (nullptr == phLabel) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto outLabel = reinterpret_cast<Label **>(phLabel);

    InterfaceLabelDescriptor labelDesc = {pLabelDesc->name, pLabelDesc->alignment};

    return MutableCommandList::fromHandle(hCommandList)->getLabel(&labelDesc, outLabel);
}

ze_result_t ZE_APICALL
zexCommandListSetLabel(
    ze_command_list_handle_t hCommandList,
    zex_label_handle_t hLabel) {
    hCommandList = toInternalType(hCommandList);
    if (nullptr == hCommandList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (nullptr == hLabel) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return Label::fromHandle(hLabel)->setLabel(hCommandList);
}

ze_result_t ZE_APICALL
zexCommandListAppendJump(
    ze_command_list_handle_t hCommandList,
    zex_label_handle_t hLabel,
    zex_operand_desc_t *pCondition) {
    hCommandList = toInternalType(hCommandList);
    if (nullptr == hCommandList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (nullptr == hLabel) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    InterfaceOperandDescriptor conditionData = {};
    InterfaceOperandDescriptor *condition = nullptr;

    if (pCondition != nullptr) {
        conditionData.memory = pCondition->memory;
        conditionData.offset = pCondition->offset;
        conditionData.size = pCondition->size;
        conditionData.flags = pCondition->flags;

        condition = &conditionData;
    }

    auto label = Label::fromHandle(hLabel);
    return MutableCommandList::fromHandle(hCommandList)->appendJump(label, condition);
}

ze_result_t ZE_APICALL
zexCommandListAppendLoadRegVariable(
    ze_command_list_handle_t hCommandList,
    zex_mcl_alu_reg_t reg,
    zex_variable_handle_t hVariable) {
    hCommandList = toInternalType(hCommandList);
    if (nullptr == hCommandList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (nullptr == hVariable) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto variable = Variable::fromHandle(hVariable);
    return MutableCommandList::fromHandle(hCommandList)->appendMILoadRegVariable(getMclAluReg(reg), variable);
}

ze_result_t ZE_APICALL
zexCommandListAppendStoreRegVariable(
    ze_command_list_handle_t hCommandList,
    zex_mcl_alu_reg_t reg,
    zex_variable_handle_t hVariable) {
    hCommandList = toInternalType(hCommandList);
    if (nullptr == hCommandList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (nullptr == hVariable) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto variable = Variable::fromHandle(hVariable);
    return MutableCommandList::fromHandle(hCommandList)->appendMIStoreRegVariable(getMclAluReg(reg), variable);
}

ze_result_t ZE_APICALL
zexCommandListTempMemSetEleCount(
    ze_command_list_handle_t hCommandList,
    size_t eleCount) {
    hCommandList = toInternalType(hCommandList);
    if (nullptr == hCommandList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return MutableCommandList::fromHandle(hCommandList)->tempMemSetElementCount(eleCount);
}

ze_result_t ZE_APICALL
zexCommandListTempMemGetSize(
    ze_command_list_handle_t hCommandList,
    size_t *pTempMemSize) {
    if (nullptr == hCommandList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return MutableCommandList::fromHandle(hCommandList)->tempMemGetSize(pTempMemSize);
}

ze_result_t ZE_APICALL
zexCommandListTempMemSet(
    ze_command_list_handle_t hCommandList,
    const void *pTempMem) {
    hCommandList = toInternalType(hCommandList);
    if (nullptr == hCommandList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return MutableCommandList::fromHandle(hCommandList)->tempMemSet(pTempMem);
}

ze_result_t ZE_APICALL
zexCommandListGetNativeBinary(
    ze_command_list_handle_t hCommandList,
    void *pBinary,
    size_t *pBinarySize,
    const void *pModule,
    size_t moduleSize) {
    hCommandList = toInternalType(hCommandList);
    if (nullptr == hCommandList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return MutableCommandList::fromHandle(hCommandList)->getNativeBinary((uint8_t *)pBinary, pBinarySize, (const uint8_t *)pModule, moduleSize);
}

ze_result_t ZE_APICALL
zexCommandListLoadNativeBinary(
    ze_command_list_handle_t hCommandList,
    const void *pBinary,
    size_t binarySize) {
    hCommandList = toInternalType(hCommandList);
    if (nullptr == hCommandList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return MutableCommandList::fromHandle(hCommandList)->loadFromBinary((const uint8_t *)pBinary, binarySize);
}

ze_result_t ZE_APICALL
zexVariableGetInfo(
    zex_variable_handle_t hVariable,
    const zex_variable_info_t **pTypeInfo) {
    if (nullptr == hVariable) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (nullptr == pTypeInfo) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto variable = Variable::fromHandle(hVariable);
    const auto &varDesc = variable->getDesc();

    return getVariableInfoFromDesc(varDesc, variable, hVariable->info);
}

ze_result_t ZE_APICALL
zexCommandListGetVariablesList(
    ze_command_list_handle_t hCommandList,
    uint32_t *pVariablesCount,
    const zex_variable_info_t **pVariablesInfos) {
    hCommandList = toInternalType(hCommandList);
    if (nullptr == hCommandList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (nullptr == pVariablesCount) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return copyVariableInfoFromVariableList(L0::MCL::MutableCommandList::fromHandle(hCommandList), pVariablesCount, pVariablesInfos);
}

ze_result_t ZE_APICALL
zexKernelSetVariableGroupSize(
    ze_kernel_handle_t hKernel,
    zex_variable_handle_t hGroupSizeVariable) {
    hKernel = toInternalType(hKernel);
    if (nullptr == hKernel) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (nullptr == hGroupSizeVariable) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return Variable::fromHandle(hGroupSizeVariable)->setAsKernelGroupSize(hKernel);
}

ze_result_t ZE_APICALL
zexCommandListAppendVariableLaunchKernel(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    zex_variable_handle_t hGroupCountVariable,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    hCommandList = toInternalType(hCommandList);
    if (nullptr == hCommandList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    hKernel = toInternalType(hKernel);
    if (nullptr == hKernel) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (nullptr == hGroupCountVariable) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    std::vector<ze_event_handle_t> translatedEvents{};
    for (auto i = 0u; i < numWaitEvents; i++) {
        translatedEvents.push_back(toInternalType(phWaitEvents[i]));
    }
    hSignalEvent = toInternalType(hSignalEvent);
    auto kernel = Kernel::fromHandle(hKernel);
    auto groupCount = Variable::fromHandle(hGroupCountVariable);
    auto signalEvent = Event::fromHandle(hSignalEvent);
    return MutableCommandList::fromHandle(hCommandList)->appendVariableLaunchKernel(kernel, groupCount, signalEvent, numWaitEvents, translatedEvents.data());
}

} // namespace L0::MCL

#if defined(__cplusplus)
extern "C" {
#endif

ze_result_t ZE_APICALL
zexCommandListGetVariable(
    ze_command_list_handle_t hCmdList,
    const zex_variable_desc_t *pVariableDescriptor,
    zex_variable_handle_t *phVariable) {

    return L0::MCL::zexCommandListGetVariable(hCmdList, pVariableDescriptor, phVariable);
}

ze_result_t ZE_APICALL
zexKernelSetArgumentVariable(
    ze_kernel_handle_t hKernel,
    uint32_t argIndex,
    zex_variable_handle_t hVariable) {

    return L0::MCL::zexKernelSetArgumentVariable(hKernel, argIndex, hVariable);
}

ze_result_t ZE_APICALL
zexVariableSetValue(
    zex_variable_handle_t hVariable,
    uint32_t flags,
    size_t valueSize,
    const void *pValue) {

    return L0::MCL::zexVariableSetValue(hVariable, flags, valueSize, pValue);
}

ze_result_t ZE_APICALL
zexCommandListGetLabel(
    ze_command_list_handle_t hCommandList,
    const zex_label_desc_t *pLabelDesc,
    zex_label_handle_t *phLabel) {

    return L0::MCL::zexCommandListGetLabel(hCommandList, pLabelDesc, phLabel);
}

ze_result_t ZE_APICALL
zexCommandListSetLabel(
    ze_command_list_handle_t hCommandList,
    zex_label_handle_t hLabel) {

    return L0::MCL::zexCommandListSetLabel(hCommandList, hLabel);
}

ze_result_t ZE_APICALL
zexCommandListAppendJump(
    ze_command_list_handle_t hCommandList,
    zex_label_handle_t hLabel,
    zex_operand_desc_t *pCondition) {

    return L0::MCL::zexCommandListAppendJump(hCommandList, hLabel, pCondition);
}

ze_result_t ZE_APICALL
zexCommandListAppendLoadRegVariable(
    ze_command_list_handle_t hCommandList,
    zex_mcl_alu_reg_t reg,
    zex_variable_handle_t hVariable) {

    return L0::MCL::zexCommandListAppendLoadRegVariable(hCommandList, reg, hVariable);
}

ze_result_t ZE_APICALL
zexCommandListAppendStoreRegVariable(
    ze_command_list_handle_t hCommandList,
    zex_mcl_alu_reg_t reg,
    zex_variable_handle_t hVariable) {

    return L0::MCL::zexCommandListAppendStoreRegVariable(hCommandList, reg, hVariable);
}

ze_result_t ZE_APICALL
zexCommandListTempMemSetEleCount(
    ze_command_list_handle_t hCommandList,
    size_t eleCount) {

    return L0::MCL::zexCommandListTempMemSetEleCount(hCommandList, eleCount);
}

ze_result_t ZE_APICALL
zexCommandListTempMemGetSize(
    ze_command_list_handle_t hCommandList,
    size_t *pTempMemSize) {

    return L0::MCL::zexCommandListTempMemGetSize(hCommandList, pTempMemSize);
}

ze_result_t ZE_APICALL
zexCommandListTempMemSet(
    ze_command_list_handle_t hCommandList,
    const void *pTempMem) {

    return L0::MCL::zexCommandListTempMemSet(hCommandList, pTempMem);
}

ze_result_t ZE_APICALL
zexCommandListGetNativeBinary(
    ze_command_list_handle_t hCommandList,
    void *pBinary,
    size_t *pBinarySize,
    const void *pModule,
    size_t moduleSize) {

    return L0::MCL::zexCommandListGetNativeBinary(hCommandList, pBinary, pBinarySize, pModule, moduleSize);
}

ze_result_t ZE_APICALL
zexCommandListLoadNativeBinary(
    ze_command_list_handle_t hCommandList,
    const void *pBinary,
    size_t binarySize) {

    return L0::MCL::zexCommandListLoadNativeBinary(hCommandList, pBinary, binarySize);
}

ze_result_t ZE_APICALL
zexVariableGetInfo(
    zex_variable_handle_t hVariable,
    const zex_variable_info_t **pTypeInfo) {

    return L0::MCL::zexVariableGetInfo(hVariable, pTypeInfo);
}

ze_result_t ZE_APICALL
zexCommandListGetVariablesList(
    ze_command_list_handle_t hCommandList,
    uint32_t *pVariablesCount,
    const zex_variable_info_t **pVariablesInfos) {

    return L0::MCL::zexCommandListGetVariablesList(hCommandList, pVariablesCount, pVariablesInfos);
}

ze_result_t ZE_APICALL
zexKernelSetVariableGroupSize(
    ze_kernel_handle_t hKernel,
    zex_variable_handle_t hGroupSizeVariable) {

    return L0::MCL::zexKernelSetVariableGroupSize(hKernel, hGroupSizeVariable);
}

ze_result_t ZE_APICALL
zexCommandListAppendVariableLaunchKernel(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    zex_variable_handle_t hGroupCountVariable,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    return L0::MCL::zexCommandListAppendVariableLaunchKernel(hCommandList, hKernel, hGroupCountVariable, hSignalEvent, numWaitEvents, phWaitEvents);
}

#if defined(__cplusplus)
} // extern "C"
#endif
