/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleCreateTracing(ze_context_handle_t hContext,
                      ze_device_handle_t hDevice,
                      const ze_module_desc_t *desc,
                      ze_module_handle_t *phModule,
                      ze_module_build_log_handle_t *phBuildLog) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Module.pfnCreate,
                               hContext,
                               hDevice,
                               desc,
                               phModule,
                               phBuildLog);

    ze_module_create_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phDevice = &hDevice;
    tracerParams.pdesc = &desc;
    tracerParams.pphModule = &phModule;
    tracerParams.pphBuildLog = &phBuildLog;

    L0::APITracerCallbackDataImp<ze_pfnModuleCreateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnModuleCreateCb_t, Module, pfnCreateCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Module.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.pdesc,
                                   *tracerParams.pphModule,
                                   *tracerParams.pphBuildLog);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleDestroyTracing(ze_module_handle_t hModule) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Module.pfnDestroy,
                               hModule);

    ze_module_destroy_params_t tracerParams;
    tracerParams.phModule = &hModule;

    L0::APITracerCallbackDataImp<ze_pfnModuleDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnModuleDestroyCb_t, Module, pfnDestroyCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Module.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModule);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleBuildLogDestroyTracing(ze_module_build_log_handle_t hModuleBuildLog) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.ModuleBuildLog.pfnDestroy,
                               hModuleBuildLog);

    ze_module_build_log_destroy_params_t tracerParams;
    tracerParams.phModuleBuildLog = &hModuleBuildLog;

    L0::APITracerCallbackDataImp<ze_pfnModuleBuildLogDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnModuleBuildLogDestroyCb_t, ModuleBuildLog, pfnDestroyCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.ModuleBuildLog.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModuleBuildLog);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleBuildLogGetStringTracing(ze_module_build_log_handle_t hModuleBuildLog,
                                 size_t *pSize,
                                 char *pBuildLog) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.ModuleBuildLog.pfnGetString,
                               hModuleBuildLog,
                               pSize,
                               pBuildLog);

    ze_module_build_log_get_string_params_t tracerParams;
    tracerParams.phModuleBuildLog = &hModuleBuildLog;
    tracerParams.ppSize = &pSize;
    tracerParams.ppBuildLog = &pBuildLog;

    L0::APITracerCallbackDataImp<ze_pfnModuleBuildLogGetStringCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnModuleBuildLogGetStringCb_t, ModuleBuildLog, pfnGetStringCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.ModuleBuildLog.pfnGetString,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModuleBuildLog,
                                   *tracerParams.ppSize,
                                   *tracerParams.ppBuildLog);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetNativeBinaryTracing(ze_module_handle_t hModule,
                               size_t *pSize,
                               uint8_t *pModuleNativeBinary) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Module.pfnGetNativeBinary,
                               hModule,
                               pSize,
                               pModuleNativeBinary);

    ze_module_get_native_binary_params_t tracerParams;
    tracerParams.phModule = &hModule;
    tracerParams.ppSize = &pSize;
    tracerParams.ppModuleNativeBinary = &pModuleNativeBinary;

    L0::APITracerCallbackDataImp<ze_pfnModuleGetNativeBinaryCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnModuleGetNativeBinaryCb_t, Module, pfnGetNativeBinaryCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Module.pfnGetNativeBinary,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModule,
                                   *tracerParams.ppSize,
                                   *tracerParams.ppModuleNativeBinary);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetGlobalPointerTracing(ze_module_handle_t hModule,
                                const char *pGlobalName,
                                size_t *pSize,
                                void **pptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Module.pfnGetGlobalPointer,
                               hModule,
                               pGlobalName,
                               pSize,
                               pptr);

    ze_module_get_global_pointer_params_t tracerParams;
    tracerParams.phModule = &hModule;
    tracerParams.ppGlobalName = &pGlobalName;
    tracerParams.ppSize = &pSize;
    tracerParams.ppptr = &pptr;

    L0::APITracerCallbackDataImp<ze_pfnModuleGetGlobalPointerCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnModuleGetGlobalPointerCb_t, Module, pfnGetGlobalPointerCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Module.pfnGetGlobalPointer,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModule,
                                   *tracerParams.ppGlobalName,
                                   *tracerParams.ppSize,
                                   *tracerParams.ppptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleDynamicLinkTracing(uint32_t numModules,
                           ze_module_handle_t *phModules,
                           ze_module_build_log_handle_t *phLinkLog) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Module.pfnDynamicLink,
                               numModules,
                               phModules,
                               phLinkLog);

    ze_module_dynamic_link_params_t tracerParams;
    tracerParams.pnumModules = &numModules;
    tracerParams.pphModules = &phModules;
    tracerParams.pphLinkLog = &phLinkLog;

    L0::APITracerCallbackDataImp<ze_pfnModuleDynamicLinkCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnModuleDynamicLinkCb_t, Module, pfnDynamicLinkCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Module.pfnDynamicLink,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.pnumModules,
                                   *tracerParams.pphModules,
                                   *tracerParams.pphLinkLog);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetPropertiesTracing(ze_module_handle_t hModule,
                             ze_module_properties_t *pModuleProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Module.pfnGetProperties,
                               hModule,
                               pModuleProperties);

    ze_module_get_properties_params_t tracerParams;
    tracerParams.phModule = &hModule;
    tracerParams.ppModuleProperties = &pModuleProperties;

    L0::APITracerCallbackDataImp<ze_pfnModuleGetPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnModuleGetPropertiesCb_t, Module, pfnGetPropertiesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Module.pfnGetProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModule,
                                   *tracerParams.ppModuleProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelCreateTracing(ze_module_handle_t hModule,
                      const ze_kernel_desc_t *desc,
                      ze_kernel_handle_t *phKernel) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnCreate,
                               hModule,
                               desc,
                               phKernel);

    ze_kernel_create_params_t tracerParams;
    tracerParams.phModule = &hModule;
    tracerParams.pdesc = &desc;
    tracerParams.pphKernel = &phKernel;

    L0::APITracerCallbackDataImp<ze_pfnKernelCreateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnKernelCreateCb_t, Kernel, pfnCreateCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModule,
                                   *tracerParams.pdesc,
                                   *tracerParams.pphKernel);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelDestroyTracing(ze_kernel_handle_t hKernel) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnDestroy,
                               hKernel);

    ze_kernel_destroy_params_t tracerParams;
    tracerParams.phKernel = &hKernel;

    L0::APITracerCallbackDataImp<ze_pfnKernelDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnKernelDestroyCb_t, Kernel, pfnDestroyCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetFunctionPointerTracing(ze_module_handle_t hModule,
                                  const char *pKernelName,
                                  void **pfnFunction) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Module.pfnGetFunctionPointer,
                               hModule,
                               pKernelName,
                               pfnFunction);

    ze_module_get_function_pointer_params_t tracerParams;
    tracerParams.phModule = &hModule;
    tracerParams.ppFunctionName = &pKernelName;
    tracerParams.ppfnFunction = &pfnFunction;

    L0::APITracerCallbackDataImp<ze_pfnModuleGetFunctionPointerCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnModuleGetFunctionPointerCb_t, Module, pfnGetFunctionPointerCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Module.pfnGetFunctionPointer,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModule,
                                   *tracerParams.ppFunctionName,
                                   *tracerParams.ppfnFunction);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetGroupSizeTracing(ze_kernel_handle_t hKernel,
                            uint32_t groupSizeX,
                            uint32_t groupSizeY,
                            uint32_t groupSizeZ) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnSetGroupSize,
                               hKernel,
                               groupSizeX,
                               groupSizeY,
                               groupSizeZ);

    ze_kernel_set_group_size_params_t tracerParams;
    tracerParams.phKernel = &hKernel;
    tracerParams.pgroupSizeX = &groupSizeX;
    tracerParams.pgroupSizeY = &groupSizeY;
    tracerParams.pgroupSizeZ = &groupSizeZ;

    L0::APITracerCallbackDataImp<ze_pfnKernelSetGroupSizeCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnKernelSetGroupSizeCb_t, Kernel, pfnSetGroupSizeCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnSetGroupSize,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.pgroupSizeX,
                                   *tracerParams.pgroupSizeY,
                                   *tracerParams.pgroupSizeZ);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSuggestGroupSizeTracing(ze_kernel_handle_t hKernel,
                                uint32_t globalSizeX,
                                uint32_t globalSizeY,
                                uint32_t globalSizeZ,
                                uint32_t *groupSizeX,
                                uint32_t *groupSizeY,
                                uint32_t *groupSizeZ) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnSuggestGroupSize,
                               hKernel,
                               globalSizeX,
                               globalSizeY,
                               globalSizeZ,
                               groupSizeX,
                               groupSizeY,
                               groupSizeZ);

    ze_kernel_suggest_group_size_params_t tracerParams;
    tracerParams.phKernel = &hKernel;
    tracerParams.pglobalSizeX = &globalSizeX;
    tracerParams.pglobalSizeY = &globalSizeY;
    tracerParams.pglobalSizeZ = &globalSizeZ;
    tracerParams.pgroupSizeX = &groupSizeX;
    tracerParams.pgroupSizeY = &groupSizeY;
    tracerParams.pgroupSizeZ = &groupSizeZ;

    L0::APITracerCallbackDataImp<ze_pfnKernelSuggestGroupSizeCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnKernelSuggestGroupSizeCb_t, Kernel, pfnSuggestGroupSizeCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnSuggestGroupSize,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.pglobalSizeX,
                                   *tracerParams.pglobalSizeY,
                                   *tracerParams.pglobalSizeZ,
                                   *tracerParams.pgroupSizeX,
                                   *tracerParams.pgroupSizeY,
                                   *tracerParams.pgroupSizeZ);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetArgumentValueTracing(ze_kernel_handle_t hKernel,
                                uint32_t argIndex,
                                size_t argSize,
                                const void *pArgValue) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnSetArgumentValue,
                               hKernel,
                               argIndex,
                               argSize,
                               pArgValue);

    ze_kernel_set_argument_value_params_t tracerParams;
    tracerParams.phKernel = &hKernel;
    tracerParams.pargIndex = &argIndex;
    tracerParams.pargSize = &argSize;
    tracerParams.ppArgValue = &pArgValue;

    L0::APITracerCallbackDataImp<ze_pfnKernelSetArgumentValueCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnKernelSetArgumentValueCb_t, Kernel, pfnSetArgumentValueCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnSetArgumentValue,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.pargIndex,
                                   *tracerParams.pargSize,
                                   *tracerParams.ppArgValue);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetPropertiesTracing(ze_kernel_handle_t hKernel,
                             ze_kernel_properties_t *pKernelProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnGetProperties,
                               hKernel,
                               pKernelProperties);

    ze_kernel_get_properties_params_t tracerParams;
    tracerParams.phKernel = &hKernel;
    tracerParams.ppKernelProperties = &pKernelProperties;

    L0::APITracerCallbackDataImp<ze_pfnKernelGetPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnKernelGetPropertiesCb_t, Kernel, pfnGetPropertiesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnGetProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.ppKernelProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendLaunchKernelTracing(ze_command_list_handle_t hCommandList,
                                       ze_kernel_handle_t hKernel,
                                       const ze_group_count_t *pLaunchFuncArgs,
                                       ze_event_handle_t hSignalEvent,
                                       uint32_t numWaitEvents,
                                       ze_event_handle_t *phWaitEvents) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchKernel,
                               hCommandList,
                               hKernel,
                               pLaunchFuncArgs,
                               hSignalEvent,
                               numWaitEvents,
                               phWaitEvents);

    ze_command_list_append_launch_kernel_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.phKernel = &hKernel;
    tracerParams.ppLaunchFuncArgs = &pLaunchFuncArgs;
    tracerParams.phSignalEvent = &hSignalEvent;
    tracerParams.pnumWaitEvents = &numWaitEvents;
    tracerParams.pphWaitEvents = &phWaitEvents;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendLaunchKernelCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListAppendLaunchKernelCb_t,
                                  CommandList,
                                  pfnAppendLaunchKernelCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchKernel,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.phKernel,
                                   *tracerParams.ppLaunchFuncArgs,
                                   *tracerParams.phSignalEvent,
                                   *tracerParams.pnumWaitEvents,
                                   *tracerParams.pphWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendLaunchKernelIndirectTracing(ze_command_list_handle_t hCommandList,
                                               ze_kernel_handle_t hKernel,
                                               const ze_group_count_t *pLaunchArgumentsBuffer,
                                               ze_event_handle_t hSignalEvent,
                                               uint32_t numWaitEvents,
                                               ze_event_handle_t *phWaitEvents) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchKernelIndirect,
                               hCommandList,
                               hKernel,
                               pLaunchArgumentsBuffer,
                               hSignalEvent,
                               numWaitEvents,
                               phWaitEvents);

    ze_command_list_append_launch_kernel_indirect_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.phKernel = &hKernel;
    tracerParams.ppLaunchArgumentsBuffer = &pLaunchArgumentsBuffer;
    tracerParams.phSignalEvent = &hSignalEvent;
    tracerParams.pnumWaitEvents = &numWaitEvents;
    tracerParams.pphWaitEvents = &phWaitEvents;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendLaunchKernelIndirectCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListAppendLaunchKernelIndirectCb_t,
                                  CommandList,
                                  pfnAppendLaunchKernelIndirectCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchKernelIndirect,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.phKernel,
                                   *tracerParams.ppLaunchArgumentsBuffer,
                                   *tracerParams.phSignalEvent,
                                   *tracerParams.pnumWaitEvents,
                                   *tracerParams.pphWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendLaunchMultipleKernelsIndirectTracing(ze_command_list_handle_t hCommandList,
                                                        uint32_t numKernels,
                                                        ze_kernel_handle_t *phKernels,
                                                        const uint32_t *pCountBuffer,
                                                        const ze_group_count_t *pLaunchArgumentsBuffer,
                                                        ze_event_handle_t hSignalEvent,
                                                        uint32_t numWaitEvents,
                                                        ze_event_handle_t *phWaitEvents) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchMultipleKernelsIndirect,
                               hCommandList,
                               numKernels,
                               phKernels,
                               pCountBuffer,
                               pLaunchArgumentsBuffer,
                               hSignalEvent,
                               numWaitEvents,
                               phWaitEvents);

    ze_command_list_append_launch_multiple_kernels_indirect_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.pnumKernels = &numKernels;
    tracerParams.pphKernels = &phKernels;
    tracerParams.ppCountBuffer = &pCountBuffer;
    tracerParams.ppLaunchArgumentsBuffer = &pLaunchArgumentsBuffer;
    tracerParams.phSignalEvent = &hSignalEvent;
    tracerParams.pnumWaitEvents = &numWaitEvents;
    tracerParams.pphWaitEvents = &phWaitEvents;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendLaunchMultipleKernelsIndirectCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListAppendLaunchMultipleKernelsIndirectCb_t,
                                  CommandList,
                                  pfnAppendLaunchMultipleKernelsIndirectCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchMultipleKernelsIndirect,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.pnumKernels,
                                   *tracerParams.pphKernels,
                                   *tracerParams.ppCountBuffer,
                                   *tracerParams.ppLaunchArgumentsBuffer,
                                   *tracerParams.phSignalEvent,
                                   *tracerParams.pnumWaitEvents,
                                   *tracerParams.pphWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendLaunchCooperativeKernelTracing(ze_command_list_handle_t hCommandList,
                                                  ze_kernel_handle_t hKernel,
                                                  const ze_group_count_t *pLaunchFuncArgs,
                                                  ze_event_handle_t hSignalEvent,
                                                  uint32_t numWaitEvents,
                                                  ze_event_handle_t *phWaitEvents) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchCooperativeKernel,
                               hCommandList,
                               hKernel,
                               pLaunchFuncArgs,
                               hSignalEvent,
                               numWaitEvents,
                               phWaitEvents);

    ze_command_list_append_launch_cooperative_kernel_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.phKernel = &hKernel;
    tracerParams.ppLaunchFuncArgs = &pLaunchFuncArgs;
    tracerParams.phSignalEvent = &hSignalEvent;
    tracerParams.pnumWaitEvents = &numWaitEvents;
    tracerParams.pphWaitEvents = &phWaitEvents;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendLaunchCooperativeKernelCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListAppendLaunchCooperativeKernelCb_t,
                                  CommandList, pfnAppendLaunchCooperativeKernelCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchCooperativeKernel,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.phKernel,
                                   *tracerParams.ppLaunchFuncArgs,
                                   *tracerParams.phSignalEvent,
                                   *tracerParams.pnumWaitEvents,
                                   *tracerParams.pphWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeModuleGetKernelNamesTracing(ze_module_handle_t hModule,
                              uint32_t *pCount,
                              const char **pNames) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Module.pfnGetKernelNames,
                               hModule,
                               pCount,
                               pNames);

    ze_module_get_kernel_names_params_t tracerParams;
    tracerParams.phModule = &hModule;
    tracerParams.ppCount = &pCount;
    tracerParams.ppNames = &pNames;

    L0::APITracerCallbackDataImp<ze_pfnModuleGetKernelNamesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnModuleGetKernelNamesCb_t, Module, pfnGetKernelNamesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Module.pfnGetKernelNames,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModule,
                                   *tracerParams.ppCount,
                                   *tracerParams.ppNames);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSuggestMaxCooperativeGroupCountTracing(ze_kernel_handle_t hKernel,
                                               uint32_t *totalGroupCount) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnSuggestMaxCooperativeGroupCount,
                               hKernel,
                               totalGroupCount);

    ze_kernel_suggest_max_cooperative_group_count_params_t tracerParams;
    tracerParams.phKernel = &hKernel;
    tracerParams.ptotalGroupCount = &totalGroupCount;

    L0::APITracerCallbackDataImp<ze_pfnKernelSuggestMaxCooperativeGroupCountCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnKernelSuggestMaxCooperativeGroupCountCb_t,
                                  Kernel, pfnSuggestMaxCooperativeGroupCountCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnSuggestMaxCooperativeGroupCount,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.ptotalGroupCount);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetIndirectAccessTracing(ze_kernel_handle_t hKernel,
                                 ze_kernel_indirect_access_flags_t *pFlags) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnGetIndirectAccess,
                               hKernel,
                               pFlags);

    ze_kernel_get_indirect_access_params_t tracerParams;
    tracerParams.phKernel = &hKernel;
    tracerParams.ppFlags = &pFlags;

    L0::APITracerCallbackDataImp<ze_pfnKernelGetIndirectAccessCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnKernelGetIndirectAccessCb_t,
                                  Kernel, pfnGetIndirectAccessCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnGetIndirectAccess,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.ppFlags);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetNameTracing(ze_kernel_handle_t hKernel,
                       size_t *pSize,
                       char *pName) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnGetName,
                               hKernel,
                               pSize,
                               pName);

    ze_kernel_get_name_params_t tracerParams;
    tracerParams.phKernel = &hKernel;
    tracerParams.ppSize = &pSize;
    tracerParams.ppName = &pName;

    L0::APITracerCallbackDataImp<ze_pfnKernelGetNameCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnKernelGetNameCb_t,
                                  Kernel, pfnGetNameCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnGetName,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.ppSize,
                                   *tracerParams.ppName);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelGetSourceAttributesTracing(ze_kernel_handle_t hKernel,
                                   uint32_t *pSize,
                                   char **pString) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnGetSourceAttributes,
                               hKernel,
                               pSize,
                               pString);

    ze_kernel_get_source_attributes_params_t tracerParams;
    tracerParams.phKernel = &hKernel;
    tracerParams.ppSize = &pSize;
    tracerParams.ppString = &pString;

    L0::APITracerCallbackDataImp<ze_pfnKernelGetSourceAttributesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnKernelGetSourceAttributesCb_t,
                                  Kernel, pfnGetSourceAttributesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnGetSourceAttributes,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.ppSize,
                                   *tracerParams.ppString);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetIndirectAccessTracing(ze_kernel_handle_t hKernel,
                                 ze_kernel_indirect_access_flags_t flags) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnSetIndirectAccess,
                               hKernel,
                               flags);

    ze_kernel_set_indirect_access_params_t tracerParams;
    tracerParams.phKernel = &hKernel;
    tracerParams.pflags = &flags;

    L0::APITracerCallbackDataImp<ze_pfnKernelSetIndirectAccessCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnKernelSetIndirectAccessCb_t,
                                  Kernel, pfnSetIndirectAccessCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnSetIndirectAccess,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.pflags);
}
