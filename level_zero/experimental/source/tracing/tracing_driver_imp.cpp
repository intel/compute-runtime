/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetTracing(uint32_t *pCount,
                   ze_driver_handle_t *phDrivers) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnGet,
                               pCount,
                               phDrivers);

    ze_driver_get_params_t tracerParams;
    tracerParams.ppCount = &pCount;
    tracerParams.pphDrivers = &phDrivers;

    L0::APITracerCallbackDataImp<ze_pfnDriverGetCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverGetCb_t, Driver, pfnGetCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnGet,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.ppCount,
                                   *tracerParams.pphDrivers);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetPropertiesTracing(ze_driver_handle_t hDriver,
                             ze_driver_properties_t *properties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnGetProperties,
                               hDriver,
                               properties);
    ze_driver_get_properties_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.ppDriverProperties = &properties;

    L0::APITracerCallbackDataImp<ze_pfnDriverGetPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverGetPropertiesCb_t, Driver, pfnGetPropertiesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnGetProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.ppDriverProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetApiVersionTracing(ze_driver_handle_t hDrivers,
                             ze_api_version_t *version) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnGetApiVersion, hDrivers, version);

    ze_driver_get_api_version_params_t tracerParams;
    tracerParams.phDriver = &hDrivers;
    tracerParams.pversion = &version;

    L0::APITracerCallbackDataImp<ze_pfnDriverGetApiVersionCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverGetApiVersionCb_t, Driver, pfnGetApiVersionCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnGetApiVersion,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.pversion);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetIpcPropertiesTracing(ze_driver_handle_t hDriver,
                                ze_driver_ipc_properties_t *pIpcProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnGetIpcProperties,
                               hDriver,
                               pIpcProperties);

    ze_driver_get_ipc_properties_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.ppIpcProperties = &pIpcProperties;

    L0::APITracerCallbackDataImp<ze_pfnDriverGetIpcPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverGetIpcPropertiesCb_t, Driver, pfnGetIpcPropertiesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnGetIpcProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.ppIpcProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetExtensionPropertiesTracing(ze_driver_handle_t hDriver,
                                      uint32_t *pCount,
                                      ze_driver_extension_properties_t *pExtensionProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnGetExtensionProperties,
                               hDriver,
                               pCount,
                               pExtensionProperties);

    ze_driver_get_extension_properties_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.ppCount = &pCount;
    tracerParams.ppExtensionProperties = &pExtensionProperties;

    L0::APITracerCallbackDataImp<ze_pfnDriverGetExtensionPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverGetExtensionPropertiesCb_t, Driver, pfnGetExtensionPropertiesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnGetExtensionProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.ppCount,
                                   *tracerParams.ppExtensionProperties);
}
