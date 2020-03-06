/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tracing/tracing_imp.h"

__zedllexport ze_result_t __zecall
zeModuleCreate_Tracing(ze_device_handle_t hDevice,
                       const ze_module_desc_t *desc,
                       ze_module_handle_t *phModule,
                       ze_module_build_log_handle_t *phBuildLog) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Module.pfnCreate,
                               hDevice,
                               desc,
                               phModule,
                               phBuildLog);

    ze_module_create_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.pdesc = &desc;
    tracerParams.pphModule = &phModule;
    tracerParams.pphBuildLog = &phBuildLog;

    L0::APITracerCallbackDataImp<ze_pfnModuleCreateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnModuleCreateCb_t, Module, pfnCreateCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Module.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.pdesc,
                                   *tracerParams.pphModule,
                                   *tracerParams.pphBuildLog);
}

__zedllexport ze_result_t __zecall
zeModuleDestroy_Tracing(ze_module_handle_t hModule) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Module.pfnDestroy,
                               hModule);

    ze_module_destroy_params_t tracerParams;
    tracerParams.phModule = &hModule;

    L0::APITracerCallbackDataImp<ze_pfnModuleDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnModuleDestroyCb_t, Module, pfnDestroyCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Module.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModule);
}

__zedllexport ze_result_t __zecall
zeModuleBuildLogDestroy_Tracing(ze_module_build_log_handle_t hModuleBuildLog) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.ModuleBuildLog.pfnDestroy,
                               hModuleBuildLog);

    ze_module_build_log_destroy_params_t tracerParams;
    tracerParams.phModuleBuildLog = &hModuleBuildLog;

    L0::APITracerCallbackDataImp<ze_pfnModuleBuildLogDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnModuleBuildLogDestroyCb_t, ModuleBuildLog, pfnDestroyCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.ModuleBuildLog.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModuleBuildLog);
}

__zedllexport ze_result_t __zecall
zeModuleBuildLogGetString_Tracing(ze_module_build_log_handle_t hModuleBuildLog,
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

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.ModuleBuildLog.pfnGetString,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModuleBuildLog,
                                   *tracerParams.ppSize,
                                   *tracerParams.ppBuildLog);
}

__zedllexport ze_result_t __zecall
zeModuleGetNativeBinary_Tracing(ze_module_handle_t hModule,
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

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Module.pfnGetNativeBinary,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModule,
                                   *tracerParams.ppSize,
                                   *tracerParams.ppModuleNativeBinary);
}

__zedllexport ze_result_t __zecall
zeModuleGetGlobalPointer_Tracing(ze_module_handle_t hModule,
                                 const char *pGlobalName,
                                 void **pptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Module.pfnGetGlobalPointer,
                               hModule,
                               pGlobalName,
                               pptr);

    ze_module_get_global_pointer_params_t tracerParams;
    tracerParams.phModule = &hModule;
    tracerParams.ppGlobalName = &pGlobalName;
    tracerParams.ppptr = &pptr;

    L0::APITracerCallbackDataImp<ze_pfnModuleGetGlobalPointerCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnModuleGetGlobalPointerCb_t, Module, pfnGetGlobalPointerCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Module.pfnGetGlobalPointer,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModule,
                                   *tracerParams.ppGlobalName,
                                   *tracerParams.ppptr);
}

__zedllexport ze_result_t __zecall
zeKernelCreate_Tracing(ze_module_handle_t hModule,
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

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModule,
                                   *tracerParams.pdesc,
                                   *tracerParams.pphKernel);
}

__zedllexport ze_result_t __zecall
zeKernelDestroy_Tracing(ze_kernel_handle_t hKernel) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnDestroy,
                               hKernel);

    ze_kernel_destroy_params_t tracerParams;
    tracerParams.phKernel = &hKernel;

    L0::APITracerCallbackDataImp<ze_pfnKernelDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnKernelDestroyCb_t, Kernel, pfnDestroyCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel);
}

__zedllexport ze_result_t __zecall
zeModuleGetFunctionPointer_Tracing(ze_module_handle_t hModule,
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

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Module.pfnGetFunctionPointer,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModule,
                                   *tracerParams.ppFunctionName,
                                   *tracerParams.ppfnFunction);
}

__zedllexport ze_result_t __zecall
zeKernelSetGroupSize_Tracing(ze_kernel_handle_t hKernel,
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

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnSetGroupSize,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.pgroupSizeX,
                                   *tracerParams.pgroupSizeY,
                                   *tracerParams.pgroupSizeZ);
}

__zedllexport ze_result_t __zecall
zeKernelSuggestGroupSize_Tracing(ze_kernel_handle_t hKernel,
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

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnSuggestGroupSize,
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

__zedllexport ze_result_t __zecall
zeKernelSetArgumentValue_Tracing(ze_kernel_handle_t hKernel,
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

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnSetArgumentValue,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.pargIndex,
                                   *tracerParams.pargSize,
                                   *tracerParams.ppArgValue);
}

__zedllexport ze_result_t __zecall
zeKernelSetAttribute_Tracing(ze_kernel_handle_t hKernel,
                             ze_kernel_attribute_t attr,
                             uint32_t size,
                             const void *pValue) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnSetAttribute,
                               hKernel,
                               attr,
                               size,
                               pValue);

    ze_kernel_set_attribute_params_t tracerParams;
    tracerParams.phKernel = &hKernel;
    tracerParams.pattr = &attr;
    tracerParams.psize = &size;
    tracerParams.ppValue = &pValue;

    L0::APITracerCallbackDataImp<ze_pfnKernelSetAttributeCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnKernelSetAttributeCb_t, Kernel, pfnSetAttributeCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnSetAttribute,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.pattr,
                                   *tracerParams.psize,
                                   *tracerParams.ppValue);
}

__zedllexport ze_result_t __zecall
zeKernelGetProperties_Tracing(ze_kernel_handle_t hKernel,
                              ze_kernel_properties_t *pKernelProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnGetProperties,
                               hKernel,
                               pKernelProperties);

    ze_kernel_get_properties_params_t tracerParams;
    tracerParams.phKernel = &hKernel;
    tracerParams.ppKernelProperties = &pKernelProperties;

    L0::APITracerCallbackDataImp<ze_pfnKernelGetPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnKernelGetPropertiesCb_t, Kernel, pfnGetPropertiesCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnGetProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.ppKernelProperties);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendLaunchKernel_Tracing(ze_command_list_handle_t hCommandList,
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

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchKernel,
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

__zedllexport ze_result_t __zecall
zeCommandListAppendLaunchKernelIndirect_Tracing(ze_command_list_handle_t hCommandList,
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

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchKernelIndirect,
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

__zedllexport ze_result_t __zecall
zeCommandListAppendLaunchMultipleKernelsIndirect_Tracing(ze_command_list_handle_t hCommandList,
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

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchMultipleKernelsIndirect,
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

__zedllexport ze_result_t __zecall
zeCommandListAppendLaunchCooperativeKernel_Tracing(ze_command_list_handle_t hCommandList,
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

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchCooperativeKernel,
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

__zedllexport ze_result_t __zecall
zeModuleGetKernelNames_Tracing(ze_module_handle_t hModule,
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

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Module.pfnGetKernelNames,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phModule,
                                   *tracerParams.ppCount,
                                   *tracerParams.ppNames);
}

__zedllexport ze_result_t __zecall
zeKernelSuggestMaxCooperativeGroupCount_Tracing(ze_kernel_handle_t hKernel,
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

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnSuggestMaxCooperativeGroupCount,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.ptotalGroupCount);
}

__zedllexport ze_result_t __zecall
zeKernelGetAttribute_Tracing(ze_kernel_handle_t hKernel,
                             ze_kernel_attribute_t attr,
                             uint32_t *pSize,
                             void *pValue) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnGetAttribute,
                               hKernel,
                               attr,
                               pSize,
                               pValue);

    ze_kernel_get_attribute_params_t tracerParams;
    tracerParams.phKernel = &hKernel;
    tracerParams.pattr = &attr;
    tracerParams.ppSize = &pSize;
    tracerParams.ppValue = &pValue;

    L0::APITracerCallbackDataImp<ze_pfnKernelGetAttributeCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnKernelGetAttributeCb_t, Kernel, pfnGetAttributeCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnGetAttribute,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.pattr,
                                   *tracerParams.ppSize,
                                   *tracerParams.ppValue);
}
