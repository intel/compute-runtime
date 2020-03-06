/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tracing/tracing_imp.h"

__zedllexport ze_result_t __zecall
zeDriverGet_Tracing(uint32_t *pCount,
                    ze_driver_handle_t *phDrivers) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnGet,
                               pCount,
                               phDrivers);

    ze_driver_get_params_t tracerParams;
    tracerParams.ppCount = &pCount;
    tracerParams.pphDrivers = &phDrivers;

    L0::APITracerCallbackDataImp<ze_pfnDriverGetCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverGetCb_t, Driver, pfnGetCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnGet,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.ppCount,
                                   *tracerParams.pphDrivers);
}

__zedllexport ze_result_t __zecall
zeDriverGetProperties_Tracing(ze_driver_handle_t hDriver,
                              ze_driver_properties_t *properties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnGetProperties,
                               hDriver,
                               properties);
    ze_driver_get_properties_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.ppDriverProperties = &properties;

    L0::APITracerCallbackDataImp<ze_pfnDriverGetPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverGetPropertiesCb_t, Driver, pfnGetPropertiesCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnGetProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.ppDriverProperties);
}

__zedllexport ze_result_t __zecall
zeDriverGetApiVersion_Tracing(ze_driver_handle_t hDrivers,
                              ze_api_version_t *version) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnGetApiVersion, hDrivers, version);

    ze_driver_get_api_version_params_t tracerParams;
    tracerParams.phDriver = &hDrivers;
    tracerParams.pversion = &version;

    L0::APITracerCallbackDataImp<ze_pfnDriverGetApiVersionCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverGetApiVersionCb_t, Driver, pfnGetApiVersionCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnGetApiVersion,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.pversion);
}

__zedllexport ze_result_t __zecall
zeDriverGetIPCProperties_Tracing(ze_driver_handle_t hDriver,
                                 ze_driver_ipc_properties_t *pIPCProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnGetIPCProperties,
                               hDriver,
                               pIPCProperties);

    ze_driver_get_ipc_properties_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.ppIPCProperties = &pIPCProperties;

    L0::APITracerCallbackDataImp<ze_pfnDriverGetIPCPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverGetIPCPropertiesCb_t, Driver, pfnGetIPCPropertiesCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnGetIPCProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.ppIPCProperties);
}

__zedllexport ze_result_t __zecall
zeDriverGetExtensionFunctionAddress_Tracing(ze_driver_handle_t hDriver,
                                            const char *pFuncName,
                                            void **pfunc) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnGetExtensionFunctionAddress,
                               hDriver,
                               pFuncName,
                               pfunc);

    ze_driver_get_extension_function_address_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.ppFuncName = &pFuncName;
    tracerParams.ppfunc = &pfunc;

    L0::APITracerCallbackDataImp<ze_pfnDriverGetExtensionFunctionAddressCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverGetExtensionFunctionAddressCb_t, Driver, pfnGetExtensionFunctionAddressCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnGetExtensionFunctionAddress,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.ppFuncName,
                                   *tracerParams.ppfunc);
}
