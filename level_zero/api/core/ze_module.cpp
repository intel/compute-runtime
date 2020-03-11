/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/module.h"
#include <level_zero/ze_api.h>

extern "C" {

__zedllexport ze_result_t __zecall
zeModuleCreate(
    ze_device_handle_t hDevice,
    const ze_module_desc_t *desc,
    ze_module_handle_t *phModule,
    ze_module_build_log_handle_t *phBuildLog) {
    return L0::Device::fromHandle(hDevice)->createModule(desc, phModule, phBuildLog);
}

__zedllexport ze_result_t __zecall
zeModuleDestroy(
    ze_module_handle_t hModule) {
    return L0::Module::fromHandle(hModule)->destroy();
}

__zedllexport ze_result_t __zecall
zeModuleBuildLogDestroy(
    ze_module_build_log_handle_t hModuleBuildLog) {
    return L0::ModuleBuildLog::fromHandle(hModuleBuildLog)->destroy();
}

__zedllexport ze_result_t __zecall
zeModuleBuildLogGetString(
    ze_module_build_log_handle_t hModuleBuildLog,
    size_t *pSize,
    char *pBuildLog) {
    return L0::ModuleBuildLog::fromHandle(hModuleBuildLog)->getString(pSize, pBuildLog);
}

__zedllexport ze_result_t __zecall
zeModuleGetNativeBinary(
    ze_module_handle_t hModule,
    size_t *pSize,
    uint8_t *pModuleNativeBinary) {
    return L0::Module::fromHandle(hModule)->getNativeBinary(pSize, pModuleNativeBinary);
}

__zedllexport ze_result_t __zecall
zeModuleGetGlobalPointer(
    ze_module_handle_t hModule,
    const char *pGlobalName,
    void **pptr) {
    return L0::Module::fromHandle(hModule)->getGlobalPointer(pGlobalName, pptr);
}

__zedllexport ze_result_t __zecall
zeModuleGetKernelNames(
    ze_module_handle_t hModule,
    uint32_t *pCount,
    const char **pNames) {
    return L0::Module::fromHandle(hModule)->getKernelNames(pCount, pNames);
}

__zedllexport ze_result_t __zecall
zeKernelCreate(
    ze_module_handle_t hModule,
    const ze_kernel_desc_t *desc,
    ze_kernel_handle_t *phFunction) {
    return L0::Module::fromHandle(hModule)->createKernel(desc, phFunction);
}

__zedllexport ze_result_t __zecall
zeKernelDestroy(
    ze_kernel_handle_t hKernel) {
    return L0::Kernel::fromHandle(hKernel)->destroy();
}

__zedllexport ze_result_t __zecall
zeModuleGetFunctionPointer(
    ze_module_handle_t hModule,
    const char *pKernelName,
    void **pfnFunction) {
    return L0::Module::fromHandle(hModule)->getFunctionPointer(pKernelName, pfnFunction);
}

__zedllexport ze_result_t __zecall
zeKernelSetGroupSize(
    ze_kernel_handle_t hFunction,
    uint32_t groupSizeX,
    uint32_t groupSizeY,
    uint32_t groupSizeZ) {
    return L0::Kernel::fromHandle(hFunction)->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);
}

__zedllexport ze_result_t __zecall
zeKernelSuggestGroupSize(
    ze_kernel_handle_t hFunction,
    uint32_t globalSizeX,
    uint32_t globalSizeY,
    uint32_t globalSizeZ,
    uint32_t *groupSizeX,
    uint32_t *groupSizeY,
    uint32_t *groupSizeZ) {
    return L0::Kernel::fromHandle(hFunction)->suggestGroupSize(globalSizeX, globalSizeY, globalSizeZ, groupSizeX, groupSizeY, groupSizeZ);
}

__zedllexport ze_result_t __zecall
zeKernelSuggestMaxCooperativeGroupCount(
    ze_kernel_handle_t hKernel,
    uint32_t *totalGroupCount) {
    return L0::Kernel::fromHandle(hKernel)->suggestMaxCooperativeGroupCount(totalGroupCount);
}

__zedllexport ze_result_t __zecall
zeKernelSetArgumentValue(
    ze_kernel_handle_t hFunction,
    uint32_t argIndex,
    size_t argSize,
    const void *pArgValue) {
    return L0::Kernel::fromHandle(hFunction)->setArgumentValue(argIndex, argSize, pArgValue);
}

__zedllexport ze_result_t __zecall
zeKernelSetAttribute(
    ze_kernel_handle_t hKernel,
    ze_kernel_attribute_t attr,
    uint32_t size,
    const void *pValue) {
    return L0::Kernel::fromHandle(hKernel)->setAttribute(attr, size, pValue);
}

__zedllexport ze_result_t __zecall
zeKernelGetAttribute(
    ze_kernel_handle_t hKernel,
    ze_kernel_attribute_t attr,
    uint32_t *pSize,
    void *pValue) {
    return L0::Kernel::fromHandle(hKernel)->getAttribute(attr, pSize, pValue);
}

__zedllexport ze_result_t __zecall
zeKernelSetIntermediateCacheConfig(
    ze_kernel_handle_t hKernel,
    ze_cache_config_t cacheConfig) {
    return L0::Kernel::fromHandle(hKernel)->setIntermediateCacheConfig(cacheConfig);
}

__zedllexport ze_result_t __zecall
zeKernelGetProperties(
    ze_kernel_handle_t hKernel,
    ze_kernel_properties_t *pKernelProperties) {
    return L0::Kernel::fromHandle(hKernel)->getProperties(pKernelProperties);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendLaunchKernel(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hFunction,
    const ze_group_count_t *pLaunchFuncArgs,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendLaunchFunction(hFunction, pLaunchFuncArgs, hSignalEvent, numWaitEvents, phWaitEvents);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendLaunchCooperativeKernel(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    const ze_group_count_t *pLaunchFuncArgs,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendLaunchCooperativeKernel(hKernel, pLaunchFuncArgs, hSignalEvent, numWaitEvents, phWaitEvents);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendLaunchKernelIndirect(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hFunction,
    const ze_group_count_t *pLaunchArgumentsBuffer,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendLaunchFunctionIndirect(hFunction, pLaunchArgumentsBuffer, hSignalEvent, numWaitEvents, phWaitEvents);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendLaunchMultipleKernelsIndirect(
    ze_command_list_handle_t hCommandList,
    uint32_t numFunctions,
    ze_kernel_handle_t *phFunctions,
    const uint32_t *pCountBuffer,
    const ze_group_count_t *pLaunchArgumentsBuffer,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendLaunchMultipleFunctionsIndirect(numFunctions, phFunctions, pCountBuffer, pLaunchArgumentsBuffer, hSignalEvent, numWaitEvents, phWaitEvents);
}

} // extern "C"
