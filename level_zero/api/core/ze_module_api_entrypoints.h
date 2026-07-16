/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t ZE_APICALL zeModuleCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_module_desc_t *desc,
    ze_module_handle_t *phModule,
    ze_module_build_log_handle_t *phBuildLog);

ze_result_t ZE_APICALL zeModuleDestroy(
    ze_module_handle_t hModule);

ze_result_t ZE_APICALL zeModuleBuildLogDestroy(
    ze_module_build_log_handle_t hModuleBuildLog);

ze_result_t ZE_APICALL zeModuleBuildLogGetString(
    ze_module_build_log_handle_t hModuleBuildLog,
    size_t *pSize,
    char *pBuildLog);

ze_result_t ZE_APICALL zeModuleGetNativeBinary(
    ze_module_handle_t hModule,
    size_t *pSize,
    uint8_t *pModuleNativeBinary);

ze_result_t ZE_APICALL zeModuleGetGlobalPointer(
    ze_module_handle_t hModule,
    const char *pGlobalName,
    size_t *pSize,
    void **pptr);

ze_result_t ZE_APICALL zeModuleGetKernelNames(
    ze_module_handle_t hModule,
    uint32_t *pCount,
    const char **pNames);

ze_result_t ZE_APICALL zeKernelCreate(
    ze_module_handle_t hModule,
    const ze_kernel_desc_t *desc,
    ze_kernel_handle_t *kernelHandle);

ze_result_t ZE_APICALL zeKernelDestroy(
    ze_kernel_handle_t hKernel);

ze_result_t ZE_APICALL zeModuleGetFunctionPointer(
    ze_module_handle_t hModule,
    const char *pKernelName,
    void **pfnFunction);

ze_result_t ZE_APICALL zeKernelSetGroupSize(
    ze_kernel_handle_t hKernel,
    uint32_t groupSizeX,
    uint32_t groupSizeY,
    uint32_t groupSizeZ);

ze_result_t ZE_APICALL zeKernelSuggestGroupSize(
    ze_kernel_handle_t hKernel,
    uint32_t globalSizeX,
    uint32_t globalSizeY,
    uint32_t globalSizeZ,
    uint32_t *groupSizeX,
    uint32_t *groupSizeY,
    uint32_t *groupSizeZ);

ze_result_t ZE_APICALL zeKernelSuggestMaxCooperativeGroupCount(
    ze_kernel_handle_t hKernel,
    uint32_t *totalGroupCount);

ze_result_t ZE_APICALL zeKernelSetArgumentValue(
    ze_kernel_handle_t hKernel,
    uint32_t argIndex,
    size_t argSize,
    const void *pArgValue);

ze_result_t ZE_APICALL zeKernelSetIndirectAccess(
    ze_kernel_handle_t hKernel,
    ze_kernel_indirect_access_flags_t flags);

ze_result_t ZE_APICALL zeKernelGetIndirectAccess(
    ze_kernel_handle_t hKernel,
    ze_kernel_indirect_access_flags_t *pFlags);

ze_result_t ZE_APICALL zeKernelGetSourceAttributes(
    ze_kernel_handle_t hKernel,
    uint32_t *pSize,
    char **pString);

ze_result_t ZE_APICALL zeKernelGetProperties(
    ze_kernel_handle_t hKernel,
    ze_kernel_properties_t *pKernelProperties);

ze_result_t ZE_APICALL zeKernelGetBinaryExp(
    ze_kernel_handle_t hKernel,
    size_t *pSize,
    uint8_t *pKernelBinary);

ze_result_t ZE_APICALL zeCommandListAppendLaunchKernel(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t kernelHandle,
    const ze_group_count_t *launchKernelArgs,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeCommandListAppendLaunchCooperativeKernel(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t kernelHandle,
    const ze_group_count_t *launchKernelArgs,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeCommandListAppendLaunchKernelIndirect(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t kernelHandle,
    const ze_group_count_t *pLaunchArgumentsBuffer,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeCommandListAppendLaunchMultipleKernelsIndirect(
    ze_command_list_handle_t hCommandList,
    uint32_t numKernels,
    ze_kernel_handle_t *kernelHandles,
    const uint32_t *pCountBuffer,
    const ze_group_count_t *pLaunchArgumentsBuffer,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeKernelGetName(
    ze_kernel_handle_t hKernel,
    size_t *pSize,
    char *pName);

ze_result_t ZE_APICALL zeModuleDynamicLink(
    uint32_t numModules,
    ze_module_handle_t *phModules,
    ze_module_build_log_handle_t *phLinkLog);

ze_result_t ZE_APICALL zeModuleGetProperties(
    ze_module_handle_t hModule,
    ze_module_properties_t *pModuleProperties);

ze_result_t ZE_APICALL zeModuleInspectLinkageExt(
    ze_linkage_inspection_ext_desc_t *pInspectDesc,
    uint32_t numModules,
    ze_module_handle_t *phModules,
    ze_module_build_log_handle_t *phLog);

ze_result_t ZE_APICALL zeKernelSetCacheConfig(
    ze_kernel_handle_t hKernel,
    ze_cache_config_flags_t flags);

ze_result_t ZE_APICALL zeKernelSchedulingHintExp(
    ze_kernel_handle_t hKernel,
    ze_scheduling_hint_exp_desc_t *pHint);

ze_result_t ZE_APICALL zeKernelGetAllocationPropertiesExp(
    ze_kernel_handle_t hKernel,
    uint32_t *pCount,
    ze_kernel_allocation_exp_properties_t *pAllocationProperties);

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_module_desc_t *desc,
    ze_module_handle_t *phModule,
    ze_module_build_log_handle_t *phBuildLog);

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleDestroy(
    ze_module_handle_t hModule);

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleDynamicLink(
    uint32_t numModules,
    ze_module_handle_t *phModules,
    ze_module_build_log_handle_t *phLinkLog);

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleBuildLogDestroy(
    ze_module_build_log_handle_t hModuleBuildLog);

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleBuildLogGetString(
    ze_module_build_log_handle_t hModuleBuildLog,
    size_t *pSize,
    char *pBuildLog);

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleGetNativeBinary(
    ze_module_handle_t hModule,
    size_t *pSize,
    uint8_t *pModuleNativeBinary);

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleGetGlobalPointer(
    ze_module_handle_t hModule,
    const char *pGlobalName,
    size_t *pSize,
    void **pptr);

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleGetKernelNames(
    ze_module_handle_t hModule,
    uint32_t *pCount,
    const char **pNames);

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleGetProperties(
    ze_module_handle_t hModule,
    ze_module_properties_t *pModuleProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelCreate(
    ze_module_handle_t hModule,
    const ze_kernel_desc_t *desc,
    ze_kernel_handle_t *phKernel);

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelDestroy(
    ze_kernel_handle_t hKernel);

ZE_APIEXPORT ze_result_t ZE_APICALL zeModuleGetFunctionPointer(
    ze_module_handle_t hModule,
    const char *pFunctionName,
    void **pfnFunction);

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelSetGroupSize(
    ze_kernel_handle_t hKernel,
    uint32_t groupSizeX,
    uint32_t groupSizeY,
    uint32_t groupSizeZ);

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelSuggestGroupSize(
    ze_kernel_handle_t hKernel,
    uint32_t globalSizeX,
    uint32_t globalSizeY,
    uint32_t globalSizeZ,
    uint32_t *groupSizeX,
    uint32_t *groupSizeY,
    uint32_t *groupSizeZ);

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelSuggestMaxCooperativeGroupCount(
    ze_kernel_handle_t hKernel,
    uint32_t *totalGroupCount);

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelSetArgumentValue(
    ze_kernel_handle_t hKernel,
    uint32_t argIndex,
    size_t argSize,
    const void *pArgValue);

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelSetIndirectAccess(
    ze_kernel_handle_t hKernel,
    ze_kernel_indirect_access_flags_t flags);

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelGetIndirectAccess(
    ze_kernel_handle_t hKernel,
    ze_kernel_indirect_access_flags_t *pFlags);

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelGetSourceAttributes(
    ze_kernel_handle_t hKernel,
    uint32_t *pSize,
    char **pString);

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelSetCacheConfig(
    ze_kernel_handle_t hKernel,
    ze_cache_config_flags_t flags);

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelGetProperties(
    ze_kernel_handle_t hKernel,
    ze_kernel_properties_t *pKernelProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelGetBinaryExp(
    ze_kernel_handle_t hKernel,
    size_t *pSize,
    uint8_t *pKernelBinary);

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelGetName(
    ze_kernel_handle_t hKernel,
    size_t *pSize,
    char *pName);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendLaunchKernel(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t kernelHandle,
    const ze_group_count_t *launchKernelArgs,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendLaunchCooperativeKernel(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t kernelHandle,
    const ze_group_count_t *launchKernelArgs,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendLaunchKernelIndirect(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t kernelHandle,
    const ze_group_count_t *pLaunchArgumentsBuffer,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendLaunchMultipleKernelsIndirect(
    ze_command_list_handle_t hCommandList,
    uint32_t numKernels,
    ze_kernel_handle_t *kernelHandles,
    const uint32_t *pCountBuffer,
    const ze_group_count_t *pLaunchArgumentsBuffer,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelSchedulingHintExp(
    ze_kernel_handle_t hKernel,
    ze_scheduling_hint_exp_desc_t *pHint);

ZE_APIEXPORT ze_result_t ZE_APICALL zeKernelGetAllocationPropertiesExp(
    ze_kernel_handle_t hKernel,
    uint32_t *pCount,
    ze_kernel_allocation_exp_properties_t *pAllocationProperties);

} // extern "C"
