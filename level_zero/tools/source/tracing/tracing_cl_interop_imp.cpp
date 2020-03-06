/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tracing/tracing_imp.h"

__zedllexport ze_result_t __zecall
zeDeviceRegisterCLMemory_Tracing(ze_device_handle_t hDevice,
                                 cl_context context,
                                 cl_mem mem,
                                 void **ptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnRegisterCLMemory,
                               hDevice,
                               context,
                               mem,
                               ptr);

    ze_device_register_cl_memory_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.pcontext = &context;
    tracerParams.pmem = &mem;
    tracerParams.pptr = &ptr;

    L0::APITracerCallbackDataImp<ze_pfnDeviceRegisterCLMemoryCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceRegisterCLMemoryCb_t, Device, pfnRegisterCLMemoryCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnRegisterCLMemory,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.pcontext,
                                   *tracerParams.pmem,
                                   *tracerParams.pptr);
}

__zedllexport ze_result_t __zecall
zeDeviceRegisterCLProgram_Tracing(ze_device_handle_t hDevice,
                                  cl_context context,
                                  cl_program program,
                                  ze_module_handle_t *phModule) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnRegisterCLProgram,
                               hDevice,
                               context,
                               program,
                               phModule);

    ze_device_register_cl_program_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.pcontext = &context;
    tracerParams.pprogram = &program;
    tracerParams.pphModule = &phModule;

    L0::APITracerCallbackDataImp<ze_pfnDeviceRegisterCLProgramCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceRegisterCLProgramCb_t, Device, pfnRegisterCLProgramCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnRegisterCLProgram,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.pcontext,
                                   *tracerParams.pprogram,
                                   *tracerParams.pphModule);
}

__zedllexport ze_result_t __zecall
zeDeviceRegisterCLCommandQueue_Tracing(ze_device_handle_t hDevice,
                                       cl_context context,
                                       cl_command_queue commandQueue,
                                       ze_command_queue_handle_t *phCommandQueue) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnRegisterCLCommandQueue,
                               hDevice,
                               context,
                               commandQueue,
                               phCommandQueue);

    ze_device_register_cl_command_queue_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.pcontext = &context;
    tracerParams.pcommand_queue = &commandQueue;
    tracerParams.pphCommandQueue = &phCommandQueue;

    L0::APITracerCallbackDataImp<ze_pfnDeviceRegisterCLCommandQueueCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceRegisterCLCommandQueueCb_t, Device, pfnRegisterCLCommandQueueCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnRegisterCLCommandQueue,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.pcontext,
                                   *tracerParams.pcommand_queue,
                                   *tracerParams.pphCommandQueue);
}
