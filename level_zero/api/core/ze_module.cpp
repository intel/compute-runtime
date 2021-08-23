/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/module/module.h"
#include <level_zero/ze_api.h>

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_module_desc_t *desc,
    ze_module_handle_t *phModule,
    ze_module_build_log_handle_t *phBuildLog) {
    return L0::Context::fromHandle(hContext)->createModule(hDevice, desc, phModule, phBuildLog);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleDestroy(
    ze_module_handle_t hModule) {
    return L0::Module::fromHandle(hModule)->destroy();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleBuildLogDestroy(
    ze_module_build_log_handle_t hModuleBuildLog) {
    return L0::ModuleBuildLog::fromHandle(hModuleBuildLog)->destroy();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleBuildLogGetString(
    ze_module_build_log_handle_t hModuleBuildLog,
    size_t *pSize,
    char *pBuildLog) {
    return L0::ModuleBuildLog::fromHandle(hModuleBuildLog)->getString(pSize, pBuildLog);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetNativeBinary(
    ze_module_handle_t hModule,
    size_t *pSize,
    uint8_t *pModuleNativeBinary) {
    return L0::Module::fromHandle(hModule)->getNativeBinary(pSize, pModuleNativeBinary);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetGlobalPointer(
    ze_module_handle_t hModule,
    const char *pGlobalName,
    size_t *pSize,
    void **pptr) {
    return L0::Module::fromHandle(hModule)->getGlobalPointer(pGlobalName, pSize, pptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetKernelNames(
    ze_module_handle_t hModule,
    uint32_t *pCount,
    const char **pNames) {
    return L0::Module::fromHandle(hModule)->getKernelNames(pCount, pNames);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelCreate(
    ze_module_handle_t hModule,
    const ze_kernel_desc_t *desc,
    ze_kernel_handle_t *phFunction) {
    return L0::Module::fromHandle(hModule)->createKernel(desc, phFunction);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelDestroy(
    ze_kernel_handle_t hKernel) {
    return L0::Kernel::fromHandle(hKernel)->destroy();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetFunctionPointer(
    ze_module_handle_t hModule,
    const char *pKernelName,
    void **pfnFunction) {
    return L0::Module::fromHandle(hModule)->getFunctionPointer(pKernelName, pfnFunction);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetGroupSize(
    ze_kernel_handle_t hKernel,
    uint32_t groupSizeX,
    uint32_t groupSizeY,
    uint32_t groupSizeZ) {
    return L0::Kernel::fromHandle(hKernel)->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSuggestGroupSize(
    ze_kernel_handle_t hKernel,
    uint32_t globalSizeX,
    uint32_t globalSizeY,
    uint32_t globalSizeZ,
    uint32_t *groupSizeX,
    uint32_t *groupSizeY,
    uint32_t *groupSizeZ) {
    return L0::Kernel::fromHandle(hKernel)->suggestGroupSize(globalSizeX, globalSizeY, globalSizeZ, groupSizeX, groupSizeY, groupSizeZ);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSuggestMaxCooperativeGroupCount(
    ze_kernel_handle_t hKernel,
    uint32_t *totalGroupCount) {
    return L0::Kernel::fromHandle(hKernel)->suggestMaxCooperativeGroupCount(totalGroupCount, NEO::EngineGroupType::Compute, false);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetArgumentValue(
    ze_kernel_handle_t hKernel,
    uint32_t argIndex,
    size_t argSize,
    const void *pArgValue) {
    return L0::Kernel::fromHandle(hKernel)->setArgumentValue(argIndex, argSize, pArgValue);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetIndirectAccess(
    ze_kernel_handle_t hKernel,
    ze_kernel_indirect_access_flags_t flags) {
    return L0::Kernel::fromHandle(hKernel)->setIndirectAccess(flags);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetIndirectAccess(
    ze_kernel_handle_t hKernel,
    ze_kernel_indirect_access_flags_t *pFlags) {
    return L0::Kernel::fromHandle(hKernel)->getIndirectAccess(pFlags);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetSourceAttributes(
    ze_kernel_handle_t hKernel,
    uint32_t *pSize,
    char **pString) {
    return L0::Kernel::fromHandle(hKernel)->getSourceAttributes(pSize, pString);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetProperties(
    ze_kernel_handle_t hKernel,
    ze_kernel_properties_t *pKernelProperties) {
    return L0::Kernel::fromHandle(hKernel)->getProperties(pKernelProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendLaunchKernel(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    const ze_group_count_t *pLaunchFuncArgs,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendLaunchKernel(hKernel, pLaunchFuncArgs, hSignalEvent, numWaitEvents, phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendLaunchCooperativeKernel(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    const ze_group_count_t *pLaunchFuncArgs,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendLaunchCooperativeKernel(hKernel, pLaunchFuncArgs, hSignalEvent, numWaitEvents, phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendLaunchKernelIndirect(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    const ze_group_count_t *pLaunchArgumentsBuffer,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendLaunchKernelIndirect(hKernel, pLaunchArgumentsBuffer, hSignalEvent, numWaitEvents, phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendLaunchMultipleKernelsIndirect(
    ze_command_list_handle_t hCommandList,
    uint32_t numKernels,
    ze_kernel_handle_t *phKernels,
    const uint32_t *pCountBuffer,
    const ze_group_count_t *pLaunchArgumentsBuffer,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendLaunchMultipleKernelsIndirect(numKernels, phKernels, pCountBuffer, pLaunchArgumentsBuffer, hSignalEvent, numWaitEvents, phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetName(
    ze_kernel_handle_t hKernel,
    size_t *pSize,
    char *pName) {
    return L0::Kernel::fromHandle(hKernel)->getKernelName(pSize, pName);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleDynamicLink(
    uint32_t numModules,
    ze_module_handle_t *phModules,
    ze_module_build_log_handle_t *phLinkLog) {
    return L0::Module::fromHandle(phModules[0])->performDynamicLink(numModules, phModules, phLinkLog);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetProperties(
    ze_module_handle_t hModule,
    ze_module_properties_t *pModuleProperties) {
    return L0::Module::fromHandle(hModule)->getProperties(pModuleProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetCacheConfig(
    ze_kernel_handle_t hKernel,
    ze_cache_config_flags_t flags) {
    return L0::Kernel::fromHandle(hKernel)->setCacheConfig(flags);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSchedulingHintExp(
    ze_kernel_handle_t hKernel,
    ze_scheduling_hint_exp_desc_t *pHint) {
    return L0::Kernel::fromHandle(hKernel)->setSchedulingHintExp(pHint);
}
