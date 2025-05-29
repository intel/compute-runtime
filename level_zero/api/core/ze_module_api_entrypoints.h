/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist_launch_params.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/source/module/module_build_log.h"
#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t zeModuleCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_module_desc_t *desc,
    ze_module_handle_t *phModule,
    ze_module_build_log_handle_t *phBuildLog) {
    return L0::Context::fromHandle(hContext)->createModule(hDevice, desc, phModule, phBuildLog);
}

ze_result_t zeModuleDestroy(
    ze_module_handle_t hModule) {
    return L0::Module::fromHandle(hModule)->destroy();
}

ze_result_t zeModuleBuildLogDestroy(
    ze_module_build_log_handle_t hModuleBuildLog) {
    return L0::ModuleBuildLog::fromHandle(hModuleBuildLog)->destroy();
}

ze_result_t zeModuleBuildLogGetString(
    ze_module_build_log_handle_t hModuleBuildLog,
    size_t *pSize,
    char *pBuildLog) {
    return L0::ModuleBuildLog::fromHandle(hModuleBuildLog)->getString(pSize, pBuildLog);
}

ze_result_t zeModuleGetNativeBinary(
    ze_module_handle_t hModule,
    size_t *pSize,
    uint8_t *pModuleNativeBinary) {
    return L0::Module::fromHandle(hModule)->getNativeBinary(pSize, pModuleNativeBinary);
}

ze_result_t zeModuleGetGlobalPointer(
    ze_module_handle_t hModule,
    const char *pGlobalName,
    size_t *pSize,
    void **pptr) {
    return L0::Module::fromHandle(hModule)->getGlobalPointer(pGlobalName, pSize, pptr);
}

ze_result_t zeModuleGetKernelNames(
    ze_module_handle_t hModule,
    uint32_t *pCount,
    const char **pNames) {
    return L0::Module::fromHandle(hModule)->getKernelNames(pCount, pNames);
}

ze_result_t zeKernelCreate(
    ze_module_handle_t hModule,
    const ze_kernel_desc_t *desc,
    ze_kernel_handle_t *kernelHandle) {
    return L0::Module::fromHandle(hModule)->createKernel(desc, kernelHandle);
}

ze_result_t zeKernelDestroy(
    ze_kernel_handle_t hKernel) {
    return L0::Kernel::fromHandle(hKernel)->destroy();
}

ze_result_t zeModuleGetFunctionPointer(
    ze_module_handle_t hModule,
    const char *pKernelName,
    void **pfnFunction) {
    return L0::Module::fromHandle(hModule)->getFunctionPointer(pKernelName, pfnFunction);
}

ze_result_t zeKernelSetGroupSize(
    ze_kernel_handle_t hKernel,
    uint32_t groupSizeX,
    uint32_t groupSizeY,
    uint32_t groupSizeZ) {
    return L0::Kernel::fromHandle(hKernel)->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);
}

ze_result_t zeKernelSuggestGroupSize(
    ze_kernel_handle_t hKernel,
    uint32_t globalSizeX,
    uint32_t globalSizeY,
    uint32_t globalSizeZ,
    uint32_t *groupSizeX,
    uint32_t *groupSizeY,
    uint32_t *groupSizeZ) {
    return L0::Kernel::fromHandle(hKernel)->suggestGroupSize(globalSizeX, globalSizeY, globalSizeZ, groupSizeX, groupSizeY, groupSizeZ);
}

ze_result_t zeKernelSuggestMaxCooperativeGroupCount(
    ze_kernel_handle_t hKernel,
    uint32_t *totalGroupCount) {
    *totalGroupCount = L0::Kernel::fromHandle(hKernel)->suggestMaxCooperativeGroupCount(NEO::EngineGroupType::compute, false);
    return ZE_RESULT_SUCCESS;
}

ze_result_t zeKernelSetArgumentValue(
    ze_kernel_handle_t hKernel,
    uint32_t argIndex,
    size_t argSize,
    const void *pArgValue) {
    return L0::Kernel::fromHandle(hKernel)->setArgumentValue(argIndex, argSize, pArgValue);
}

ze_result_t zeKernelSetIndirectAccess(
    ze_kernel_handle_t hKernel,
    ze_kernel_indirect_access_flags_t flags) {
    return L0::Kernel::fromHandle(hKernel)->setIndirectAccess(flags);
}

ze_result_t zeKernelGetIndirectAccess(
    ze_kernel_handle_t hKernel,
    ze_kernel_indirect_access_flags_t *pFlags) {
    return L0::Kernel::fromHandle(hKernel)->getIndirectAccess(pFlags);
}

ze_result_t zeKernelGetSourceAttributes(
    ze_kernel_handle_t hKernel,
    uint32_t *pSize,
    char **pString) {
    return L0::Kernel::fromHandle(hKernel)->getSourceAttributes(pSize, pString);
}

ze_result_t zeKernelGetProperties(
    ze_kernel_handle_t hKernel,
    ze_kernel_properties_t *pKernelProperties) {
    return L0::Kernel::fromHandle(hKernel)->getProperties(pKernelProperties);
}

ze_result_t zeKernelGetBinaryExp(
    ze_kernel_handle_t hKernel,
    size_t *pSize,
    uint8_t *pKernelBinary) {
    return L0::Kernel::fromHandle(hKernel)->getKernelProgramBinary(pSize, reinterpret_cast<char *>(pKernelBinary));
}

ze_result_t zeCommandListAppendLaunchKernel(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t kernelHandle,
    const ze_group_count_t *launchKernelArgs,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    auto cmdList = L0::CommandList::fromHandle(hCommandList);

    L0::CmdListKernelLaunchParams launchParams = {};
    launchParams.skipInOrderNonWalkerSignaling = cmdList->skipInOrderNonWalkerSignalingAllowed(hSignalEvent);

    return cmdList->appendLaunchKernel(kernelHandle, *launchKernelArgs, hSignalEvent, numWaitEvents, phWaitEvents, launchParams);
}

ze_result_t zeCommandListAppendLaunchCooperativeKernel(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t kernelHandle,
    const ze_group_count_t *launchKernelArgs,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    L0::CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = true;

    return L0::CommandList::fromHandle(hCommandList)->appendLaunchKernel(kernelHandle, *launchKernelArgs, hSignalEvent, numWaitEvents, phWaitEvents, launchParams);
}

ze_result_t zeCommandListAppendLaunchKernelIndirect(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t kernelHandle,
    const ze_group_count_t *pLaunchArgumentsBuffer,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendLaunchKernelIndirect(kernelHandle, *pLaunchArgumentsBuffer, hSignalEvent, numWaitEvents, phWaitEvents, false);
}

ze_result_t zeCommandListAppendLaunchMultipleKernelsIndirect(
    ze_command_list_handle_t hCommandList,
    uint32_t numKernels,
    ze_kernel_handle_t *kernelHandles,
    const uint32_t *pCountBuffer,
    const ze_group_count_t *pLaunchArgumentsBuffer,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendLaunchMultipleKernelsIndirect(numKernels, kernelHandles, pCountBuffer, pLaunchArgumentsBuffer, hSignalEvent, numWaitEvents, phWaitEvents, false);
}

ze_result_t zeKernelGetName(
    ze_kernel_handle_t hKernel,
    size_t *pSize,
    char *pName) {
    return L0::Kernel::fromHandle(hKernel)->getKernelName(pSize, pName);
}

ze_result_t zeModuleDynamicLink(
    uint32_t numModules,
    ze_module_handle_t *phModules,
    ze_module_build_log_handle_t *phLinkLog) {
    return L0::Module::fromHandle(phModules[0])->performDynamicLink(numModules, phModules, phLinkLog);
}

ze_result_t zeModuleGetProperties(
    ze_module_handle_t hModule,
    ze_module_properties_t *pModuleProperties) {
    return L0::Module::fromHandle(hModule)->getProperties(pModuleProperties);
}

ze_result_t zeModuleInspectLinkageExt(
    ze_linkage_inspection_ext_desc_t *pInspectDesc,
    uint32_t numModules,
    ze_module_handle_t *phModules,
    ze_module_build_log_handle_t *phLog) {
    return L0::Module::fromHandle(phModules[0])->inspectLinkage(pInspectDesc, numModules, phModules, phLog);
}

ze_result_t zeKernelSetCacheConfig(
    ze_kernel_handle_t hKernel,
    ze_cache_config_flags_t flags) {
    return L0::Kernel::fromHandle(hKernel)->setCacheConfig(flags);
}

ze_result_t zeKernelSchedulingHintExp(
    ze_kernel_handle_t hKernel,
    ze_scheduling_hint_exp_desc_t *pHint) {
    return L0::Kernel::fromHandle(hKernel)->setSchedulingHintExp(pHint);
}

} // namespace L0

extern "C" {
ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_module_desc_t *desc,
    ze_module_handle_t *phModule,
    ze_module_build_log_handle_t *phBuildLog) {
    return L0::zeModuleCreate(
        hContext,
        hDevice,
        desc,
        phModule,
        phBuildLog);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleDestroy(
    ze_module_handle_t hModule) {
    return L0::zeModuleDestroy(
        hModule);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleDynamicLink(
    uint32_t numModules,
    ze_module_handle_t *phModules,
    ze_module_build_log_handle_t *phLinkLog) {
    return L0::zeModuleDynamicLink(
        numModules,
        phModules,
        phLinkLog);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleBuildLogDestroy(
    ze_module_build_log_handle_t hModuleBuildLog) {
    return L0::zeModuleBuildLogDestroy(
        hModuleBuildLog);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleBuildLogGetString(
    ze_module_build_log_handle_t hModuleBuildLog,
    size_t *pSize,
    char *pBuildLog) {
    return L0::zeModuleBuildLogGetString(
        hModuleBuildLog,
        pSize,
        pBuildLog);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleGetNativeBinary(
    ze_module_handle_t hModule,
    size_t *pSize,
    uint8_t *pModuleNativeBinary) {
    return L0::zeModuleGetNativeBinary(
        hModule,
        pSize,
        pModuleNativeBinary);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleGetGlobalPointer(
    ze_module_handle_t hModule,
    const char *pGlobalName,
    size_t *pSize,
    void **pptr) {
    return L0::zeModuleGetGlobalPointer(
        hModule,
        pGlobalName,
        pSize,
        pptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleGetKernelNames(
    ze_module_handle_t hModule,
    uint32_t *pCount,
    const char **pNames) {
    return L0::zeModuleGetKernelNames(
        hModule,
        pCount,
        pNames);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleGetProperties(
    ze_module_handle_t hModule,
    ze_module_properties_t *pModuleProperties) {
    return L0::zeModuleGetProperties(
        hModule,
        pModuleProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelCreate(
    ze_module_handle_t hModule,
    const ze_kernel_desc_t *desc,
    ze_kernel_handle_t *phKernel) {
    return L0::zeKernelCreate(
        hModule,
        desc,
        phKernel);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelDestroy(
    ze_kernel_handle_t hKernel) {
    return L0::zeKernelDestroy(
        hKernel);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleGetFunctionPointer(
    ze_module_handle_t hModule,
    const char *pFunctionName,
    void **pfnFunction) {
    return L0::zeModuleGetFunctionPointer(
        hModule,
        pFunctionName,
        pfnFunction);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelSetGroupSize(
    ze_kernel_handle_t hKernel,
    uint32_t groupSizeX,
    uint32_t groupSizeY,
    uint32_t groupSizeZ) {
    return L0::zeKernelSetGroupSize(
        hKernel,
        groupSizeX,
        groupSizeY,
        groupSizeZ);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelSuggestGroupSize(
    ze_kernel_handle_t hKernel,
    uint32_t globalSizeX,
    uint32_t globalSizeY,
    uint32_t globalSizeZ,
    uint32_t *groupSizeX,
    uint32_t *groupSizeY,
    uint32_t *groupSizeZ) {
    return L0::zeKernelSuggestGroupSize(
        hKernel,
        globalSizeX,
        globalSizeY,
        globalSizeZ,
        groupSizeX,
        groupSizeY,
        groupSizeZ);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelSuggestMaxCooperativeGroupCount(
    ze_kernel_handle_t hKernel,
    uint32_t *totalGroupCount) {
    return L0::zeKernelSuggestMaxCooperativeGroupCount(
        hKernel,
        totalGroupCount);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelSetArgumentValue(
    ze_kernel_handle_t hKernel,
    uint32_t argIndex,
    size_t argSize,
    const void *pArgValue) {
    return L0::zeKernelSetArgumentValue(
        hKernel,
        argIndex,
        argSize,
        pArgValue);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelSetIndirectAccess(
    ze_kernel_handle_t hKernel,
    ze_kernel_indirect_access_flags_t flags) {
    return L0::zeKernelSetIndirectAccess(
        hKernel,
        flags);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelGetIndirectAccess(
    ze_kernel_handle_t hKernel,
    ze_kernel_indirect_access_flags_t *pFlags) {
    return L0::zeKernelGetIndirectAccess(
        hKernel,
        pFlags);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelGetSourceAttributes(
    ze_kernel_handle_t hKernel,
    uint32_t *pSize,
    char **pString) {
    return L0::zeKernelGetSourceAttributes(
        hKernel,
        pSize,
        pString);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelSetCacheConfig(
    ze_kernel_handle_t hKernel,
    ze_cache_config_flags_t flags) {
    return L0::zeKernelSetCacheConfig(
        hKernel,
        flags);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelGetProperties(
    ze_kernel_handle_t hKernel,
    ze_kernel_properties_t *pKernelProperties) {
    return L0::zeKernelGetProperties(
        hKernel,
        pKernelProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelGetBinaryExp(
    ze_kernel_handle_t hKernel,
    size_t *pSize,
    uint8_t *pKernelBinary) {
    return L0::zeKernelGetBinaryExp(
        hKernel,
        pSize,
        pKernelBinary);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelGetName(
    ze_kernel_handle_t hKernel,
    size_t *pSize,
    char *pName) {
    return L0::zeKernelGetName(
        hKernel,
        pSize,
        pName);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendLaunchKernel(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t kernelHandle,
    const ze_group_count_t *launchKernelArgs,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendLaunchKernel(
        hCommandList,
        kernelHandle,
        launchKernelArgs,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendLaunchCooperativeKernel(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t kernelHandle,
    const ze_group_count_t *launchKernelArgs,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendLaunchCooperativeKernel(
        hCommandList,
        kernelHandle,
        launchKernelArgs,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendLaunchKernelIndirect(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t kernelHandle,
    const ze_group_count_t *pLaunchArgumentsBuffer,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendLaunchKernelIndirect(
        hCommandList,
        kernelHandle,
        pLaunchArgumentsBuffer,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendLaunchMultipleKernelsIndirect(
    ze_command_list_handle_t hCommandList,
    uint32_t numKernels,
    ze_kernel_handle_t *kernelHandles,
    const uint32_t *pCountBuffer,
    const ze_group_count_t *pLaunchArgumentsBuffer,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendLaunchMultipleKernelsIndirect(
        hCommandList,
        numKernels,
        kernelHandles,
        pCountBuffer,
        pLaunchArgumentsBuffer,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelSchedulingHintExp(
    ze_kernel_handle_t hKernel,
    ze_scheduling_hint_exp_desc_t *pHint) {
    return L0::zeKernelSchedulingHintExp(hKernel, pHint);
}
}
