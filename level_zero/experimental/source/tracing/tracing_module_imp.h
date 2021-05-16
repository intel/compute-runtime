/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleCreate_Tracing(ze_context_handle_t hContext,
                       ze_device_handle_t hDevice,
                       const ze_module_desc_t *desc,
                       ze_module_handle_t *phModule,
                       ze_module_build_log_handle_t *phBuildLog);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleDestroy_Tracing(ze_module_handle_t hModule);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleBuildLogDestroy_Tracing(ze_module_build_log_handle_t hModuleBuildLog);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleBuildLogGetString_Tracing(ze_module_build_log_handle_t hModuleBuildLog,
                                  size_t *pSize,
                                  char *pBuildLog);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetNativeBinary_Tracing(ze_module_handle_t hModule,
                                size_t *pSize,
                                uint8_t *pModuleNativeBinary);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetGlobalPointer_Tracing(ze_module_handle_t hModule,
                                 const char *pGlobalName,
                                 size_t *pSize,
                                 void **pptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleDynamicLink_Tracing(uint32_t numModules,
                            ze_module_handle_t *phModules,
                            ze_module_build_log_handle_t *phLinkLog);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetProperties_Tracing(ze_module_handle_t hModule,
                              ze_module_properties_t *pModuleProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelCreate_Tracing(ze_module_handle_t hModule,
                       const ze_kernel_desc_t *desc,
                       ze_kernel_handle_t *phFunction);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelDestroy_Tracing(ze_kernel_handle_t hKernel);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetFunctionPointer_Tracing(ze_module_handle_t hModule,
                                   const char *pKernelName,
                                   void **pfnFunction);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetGroupSize_Tracing(ze_kernel_handle_t hKernel,
                             uint32_t groupSizeX,
                             uint32_t groupSizeY,
                             uint32_t groupSizeZ);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSuggestGroupSize_Tracing(ze_kernel_handle_t hKernel,
                                 uint32_t globalSizeX,
                                 uint32_t globalSizeY,
                                 uint32_t globalSizeZ,
                                 uint32_t *groupSizeX,
                                 uint32_t *groupSizeY,
                                 uint32_t *groupSizeZ);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetArgumentValue_Tracing(ze_kernel_handle_t hKernel,
                                 uint32_t argIndex,
                                 size_t argSize,
                                 const void *pArgValue);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetProperties_Tracing(ze_kernel_handle_t hKernel,
                              ze_kernel_properties_t *pKernelProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendLaunchKernel_Tracing(ze_command_list_handle_t hCommandList,
                                        ze_kernel_handle_t hKernel,
                                        const ze_group_count_t *pLaunchFuncArgs,
                                        ze_event_handle_t hSignalEvent,
                                        uint32_t numWaitEvents,
                                        ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendLaunchKernelIndirect_Tracing(ze_command_list_handle_t hCommandList,
                                                ze_kernel_handle_t hKernel,
                                                const ze_group_count_t *pLaunchArgumentsBuffer,
                                                ze_event_handle_t hSignalEvent,
                                                uint32_t numWaitEvents,
                                                ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendLaunchMultipleKernelsIndirect_Tracing(ze_command_list_handle_t hCommandList,
                                                         uint32_t numKernels,
                                                         ze_kernel_handle_t *phKernels,
                                                         const uint32_t *pCountBuffer,
                                                         const ze_group_count_t *pLaunchArgumentsBuffer,
                                                         ze_event_handle_t hSignalEvent,
                                                         uint32_t numWaitEvents,
                                                         ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendLaunchCooperativeKernel_Tracing(ze_command_list_handle_t hCommandList,
                                                   ze_kernel_handle_t hKernel,
                                                   const ze_group_count_t *pLaunchFuncArgs,
                                                   ze_event_handle_t hSignalEvent,
                                                   uint32_t numWaitEvents,
                                                   ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetKernelNames_Tracing(ze_module_handle_t hModule,
                               uint32_t *pCount,
                               const char **pNames);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSuggestMaxCooperativeGroupCount_Tracing(ze_kernel_handle_t hKernel,
                                                uint32_t *totalGroupCount);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetIndirectAccess_Tracing(ze_kernel_handle_t hKernel,
                                  ze_kernel_indirect_access_flags_t *pFlags);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetName_Tracing(ze_kernel_handle_t hKernel,
                        size_t *pSize,
                        char *pName);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetSourceAttributes_Tracing(ze_kernel_handle_t hKernel,
                                    uint32_t *pSize,
                                    char **pString);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetIndirectAccess_Tracing(ze_kernel_handle_t hKernel,
                                  ze_kernel_indirect_access_flags_t flags);
}
