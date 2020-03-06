/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

__zedllexport ze_result_t __zecall
zeModuleCreate_Tracing(ze_device_handle_t hDevice,
                       const ze_module_desc_t *desc,
                       ze_module_handle_t *phModule,
                       ze_module_build_log_handle_t *phBuildLog);

__zedllexport ze_result_t __zecall
zeModuleDestroy_Tracing(ze_module_handle_t hModule);

__zedllexport ze_result_t __zecall
zeModuleBuildLogDestroy_Tracing(ze_module_build_log_handle_t hModuleBuildLog);

__zedllexport ze_result_t __zecall
zeModuleBuildLogGetString_Tracing(ze_module_build_log_handle_t hModuleBuildLog,
                                  size_t *pSize,
                                  char *pBuildLog);

__zedllexport ze_result_t __zecall
zeModuleGetNativeBinary_Tracing(ze_module_handle_t hModule,
                                size_t *pSize,
                                uint8_t *pModuleNativeBinary);

__zedllexport ze_result_t __zecall
zeModuleGetGlobalPointer_Tracing(ze_module_handle_t hModule,
                                 const char *pGlobalName,
                                 void **pptr);

__zedllexport ze_result_t __zecall
zeKernelCreate_Tracing(ze_module_handle_t hModule,
                       const ze_kernel_desc_t *desc,
                       ze_kernel_handle_t *phFunction);

__zedllexport ze_result_t __zecall
zeKernelDestroy_Tracing(ze_kernel_handle_t hFunction);

__zedllexport ze_result_t __zecall
zeModuleGetFunctionPointer_Tracing(ze_module_handle_t hModule,
                                   const char *pKernelName,
                                   void **pfnFunction);

__zedllexport ze_result_t __zecall
zeKernelSetGroupSize_Tracing(ze_kernel_handle_t hFunction,
                             uint32_t groupSizeX,
                             uint32_t groupSizeY,
                             uint32_t groupSizeZ);

__zedllexport ze_result_t __zecall
zeKernelSuggestGroupSize_Tracing(ze_kernel_handle_t hFunction,
                                 uint32_t globalSizeX,
                                 uint32_t globalSizeY,
                                 uint32_t globalSizeZ,
                                 uint32_t *groupSizeX,
                                 uint32_t *groupSizeY,
                                 uint32_t *groupSizeZ);

__zedllexport ze_result_t __zecall
zeKernelSetArgumentValue_Tracing(ze_kernel_handle_t hFunction,
                                 uint32_t argIndex,
                                 size_t argSize,
                                 const void *pArgValue);

__zedllexport ze_result_t __zecall
zeKernelSetAttribute_Tracing(ze_kernel_handle_t hKernel,
                             ze_kernel_attribute_t attr,
                             uint32_t size,
                             const void *pValue);

__zedllexport ze_result_t __zecall
zeKernelGetProperties_Tracing(ze_kernel_handle_t hKernel,
                              ze_kernel_properties_t *pKernelProperties);

__zedllexport ze_result_t __zecall
zeCommandListAppendLaunchKernel_Tracing(ze_command_list_handle_t hCommandList,
                                        ze_kernel_handle_t hFunction,
                                        const ze_group_count_t *pLaunchFuncArgs,
                                        ze_event_handle_t hSignalEvent,
                                        uint32_t numWaitEvents,
                                        ze_event_handle_t *phWaitEvents);

__zedllexport ze_result_t __zecall
zeCommandListAppendLaunchKernelIndirect_Tracing(ze_command_list_handle_t hCommandList,
                                                ze_kernel_handle_t hFunction,
                                                const ze_group_count_t *pLaunchArgumentsBuffer,
                                                ze_event_handle_t hSignalEvent,
                                                uint32_t numWaitEvents,
                                                ze_event_handle_t *phWaitEvents);

__zedllexport ze_result_t __zecall
zeCommandListAppendLaunchMultipleKernelsIndirect_Tracing(ze_command_list_handle_t hCommandList,
                                                         uint32_t numFunctions,
                                                         ze_kernel_handle_t *phFunctions,
                                                         const uint32_t *pCountBuffer,
                                                         const ze_group_count_t *pLaunchArgumentsBuffer,
                                                         ze_event_handle_t hSignalEvent,
                                                         uint32_t numWaitEvents,
                                                         ze_event_handle_t *phWaitEvents);

__zedllexport ze_result_t __zecall
zeCommandListAppendLaunchCooperativeKernel_Tracing(ze_command_list_handle_t hCommandList,
                                                   ze_kernel_handle_t hKernel,
                                                   const ze_group_count_t *pLaunchFuncArgs,
                                                   ze_event_handle_t hSignalEvent,
                                                   uint32_t numWaitEvents,
                                                   ze_event_handle_t *phWaitEvents);

__zedllexport ze_result_t __zecall
zeModuleGetKernelNames_Tracing(ze_module_handle_t hModule,
                               uint32_t *pCount,
                               const char **pNames);

__zedllexport ze_result_t __zecall
zeKernelSuggestMaxCooperativeGroupCount_Tracing(ze_kernel_handle_t hKernel,
                                                uint32_t *totalGroupCount);

__zedllexport ze_result_t __zecall
zeKernelGetAttribute_Tracing(ze_kernel_handle_t hKernel,
                             ze_kernel_attribute_t attr,
                             uint32_t *pSize,
                             void *pValue);
}
