/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleCreateTracing(ze_context_handle_t hContext,
                      ze_device_handle_t hDevice,
                      const ze_module_desc_t *desc,
                      ze_module_handle_t *phModule,
                      ze_module_build_log_handle_t *phBuildLog);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleDestroyTracing(ze_module_handle_t hModule);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleBuildLogDestroyTracing(ze_module_build_log_handle_t hModuleBuildLog);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleBuildLogGetStringTracing(ze_module_build_log_handle_t hModuleBuildLog,
                                 size_t *pSize,
                                 char *pBuildLog);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetNativeBinaryTracing(ze_module_handle_t hModule,
                               size_t *pSize,
                               uint8_t *pModuleNativeBinary);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetGlobalPointerTracing(ze_module_handle_t hModule,
                                const char *pGlobalName,
                                size_t *pSize,
                                void **pptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleDynamicLinkTracing(uint32_t numModules,
                           ze_module_handle_t *phModules,
                           ze_module_build_log_handle_t *phLinkLog);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetPropertiesTracing(ze_module_handle_t hModule,
                             ze_module_properties_t *pModuleProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelCreateTracing(ze_module_handle_t hModule,
                      const ze_kernel_desc_t *desc,
                      ze_kernel_handle_t *phFunction);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelDestroyTracing(ze_kernel_handle_t hKernel);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetFunctionPointerTracing(ze_module_handle_t hModule,
                                  const char *pKernelName,
                                  void **pfnFunction);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetGroupSizeTracing(ze_kernel_handle_t hKernel,
                            uint32_t groupSizeX,
                            uint32_t groupSizeY,
                            uint32_t groupSizeZ);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSuggestGroupSizeTracing(ze_kernel_handle_t hKernel,
                                uint32_t globalSizeX,
                                uint32_t globalSizeY,
                                uint32_t globalSizeZ,
                                uint32_t *groupSizeX,
                                uint32_t *groupSizeY,
                                uint32_t *groupSizeZ);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetArgumentValueTracing(ze_kernel_handle_t hKernel,
                                uint32_t argIndex,
                                size_t argSize,
                                const void *pArgValue);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetPropertiesTracing(ze_kernel_handle_t hKernel,
                             ze_kernel_properties_t *pKernelProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendLaunchKernelTracing(ze_command_list_handle_t hCommandList,
                                       ze_kernel_handle_t hKernel,
                                       const ze_group_count_t *pLaunchFuncArgs,
                                       ze_event_handle_t hSignalEvent,
                                       uint32_t numWaitEvents,
                                       ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendLaunchKernelIndirectTracing(ze_command_list_handle_t hCommandList,
                                               ze_kernel_handle_t hKernel,
                                               const ze_group_count_t *pLaunchArgumentsBuffer,
                                               ze_event_handle_t hSignalEvent,
                                               uint32_t numWaitEvents,
                                               ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendLaunchMultipleKernelsIndirectTracing(ze_command_list_handle_t hCommandList,
                                                        uint32_t numKernels,
                                                        ze_kernel_handle_t *phKernels,
                                                        const uint32_t *pCountBuffer,
                                                        const ze_group_count_t *pLaunchArgumentsBuffer,
                                                        ze_event_handle_t hSignalEvent,
                                                        uint32_t numWaitEvents,
                                                        ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendLaunchCooperativeKernelTracing(ze_command_list_handle_t hCommandList,
                                                  ze_kernel_handle_t hKernel,
                                                  const ze_group_count_t *pLaunchFuncArgs,
                                                  ze_event_handle_t hSignalEvent,
                                                  uint32_t numWaitEvents,
                                                  ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetKernelNamesTracing(ze_module_handle_t hModule,
                              uint32_t *pCount,
                              const char **pNames);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSuggestMaxCooperativeGroupCountTracing(ze_kernel_handle_t hKernel,
                                               uint32_t *totalGroupCount);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetIndirectAccessTracing(ze_kernel_handle_t hKernel,
                                 ze_kernel_indirect_access_flags_t *pFlags);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetNameTracing(ze_kernel_handle_t hKernel,
                       size_t *pSize,
                       char *pName);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetSourceAttributesTracing(ze_kernel_handle_t hKernel,
                                   uint32_t *pSize,
                                   char **pString);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetIndirectAccessTracing(ze_kernel_handle_t hKernel,
                                 ze_kernel_indirect_access_flags_t flags);
}
