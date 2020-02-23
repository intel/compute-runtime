/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/os_interface/metrics_library.h"

#include "shared/source/helpers/hw_helper.h"
#include "opencl/source/os_interface/os_inc_base.h"

namespace NEO {
///////////////////////////////////////////////////////
// FUNCTION: MetricsLibrary::MetricsLibrary
///////////////////////////////////////////////////////
MetricsLibrary::MetricsLibrary() {
    api = std::make_unique<MetricsLibraryInterface>();
    osLibrary.reset(OsLibrary::load(Os::metricsLibraryDllName));
}

//////////////////////////////////////////////////////
// FUNCTION: MetricsLibrary::open
//////////////////////////////////////////////////////
bool MetricsLibrary::open() {

    UNRECOVERABLE_IF(osLibrary.get() == nullptr);

    if (osLibrary->isLoaded()) {
        api->contextCreate = reinterpret_cast<ContextCreateFunction_1_0>(osLibrary->getProcAddress(METRICS_LIBRARY_CONTEXT_CREATE_1_0));
        api->contextDelete = reinterpret_cast<ContextDeleteFunction_1_0>(osLibrary->getProcAddress(METRICS_LIBRARY_CONTEXT_DELETE_1_0));
    } else {
        api->contextCreate = nullptr;
        api->contextDelete = nullptr;
    }

    if (!api->contextCreate) {
        return false;
    }

    if (!api->contextDelete) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////
// MetricsLibrary::createContext
//////////////////////////////////////////////////////
bool MetricsLibrary::contextCreate(
    const ClientType_1_0 &clientType,
    ClientData_1_0 &clientData,
    ContextCreateData_1_0 &createData,
    ContextHandle_1_0 &handle) {

    MetricsLibraryApi::ClientOptionsData_1_0 clientOptions[1] = {};
    clientOptions[0].Type = MetricsLibraryApi::ClientOptionsType::Compute;
    clientOptions[0].Compute.Asynchronous = true;
    clientData.ClientOptionsCount = 1;
    clientData.ClientOptions = clientOptions;

    createData.Api = &api->functions;
    createData.ClientCallbacks = &api->callbacks;
    createData.ClientData = &clientData;

    return api->contextCreate(
               clientType,
               &createData,
               &handle) == StatusCode::Success;
}

//////////////////////////////////////////////////////
// MetricsLibrary::contextDelete
//////////////////////////////////////////////////////
bool MetricsLibrary::contextDelete(
    const ContextHandle_1_0 &handle) {
    return api->contextDelete(handle) == StatusCode::Success;
}

//////////////////////////////////////////////////////
// MetricsLibrary::hwCountersCreate
//////////////////////////////////////////////////////
bool MetricsLibrary::hwCountersCreate(
    const ContextHandle_1_0 &context,
    const uint32_t slots,
    const ConfigurationHandle_1_0 user,
    QueryHandle_1_0 &query) {
    QueryCreateData_1_0 data = {};
    data.HandleContext = context;
    data.Type = ObjectType::QueryHwCounters;
    data.Slots = slots;

    return api->functions.QueryCreate(
               &data,
               &query) == StatusCode::Success;
}

//////////////////////////////////////////////////////
//  MetricsLibrary::hwCountersDelete
//////////////////////////////////////////////////////
bool MetricsLibrary::hwCountersDelete(
    const QueryHandle_1_0 &query) {
    return api->functions.QueryDelete(query) == StatusCode::Success;
}

//////////////////////////////////////////////////////
// MetricsLibrary::hwCountersGetReport
//////////////////////////////////////////////////////
bool MetricsLibrary::hwCountersGetReport(
    const QueryHandle_1_0 &handle,
    const uint32_t slot,
    const uint32_t slotsCount,
    const uint32_t dataSize,
    void *data) {
    GetReportData_1_0 report = {};
    report.Type = ObjectType::QueryHwCounters;
    report.Query.Handle = handle;
    report.Query.Slot = slot;
    report.Query.SlotsCount = slotsCount;
    report.Query.Data = data;
    report.Query.DataSize = dataSize;

    return api->functions.GetData(&report) == StatusCode::Success;
}

//////////////////////////////////////////////////////
// MetricsLibrary::hwCountersGetApiReportSize
//////////////////////////////////////////////////////
uint32_t MetricsLibrary::hwCountersGetApiReportSize() {
    ValueType type = ValueType::Uint32;
    TypedValue_1_0 value = {};

    return api->functions.GetParameter(ParameterType::QueryHwCountersReportApiSize, &type, &value) == StatusCode::Success
               ? value.ValueUInt32
               : 0;
}

//////////////////////////////////////////////////////
// MetricsLibrary::hwCountersGetGpuReportSize
//////////////////////////////////////////////////////
uint32_t MetricsLibrary::hwCountersGetGpuReportSize() {
    ValueType type = ValueType::Uint32;
    TypedValue_1_0 value = {};

    return api->functions.GetParameter(ParameterType::QueryHwCountersReportGpuSize, &type, &value) == StatusCode::Success
               ? value.ValueUInt32
               : 0;
}

//////////////////////////////////////////////////////
// MetricsLibrary::commandBufferGet
//////////////////////////////////////////////////////
bool MetricsLibrary::commandBufferGet(
    CommandBufferData_1_0 &data) {
    return api->functions.CommandBufferGet(
               &data) == StatusCode::Success;
}

//////////////////////////////////////////////////////
// MetricsLibrary::commandBufferGetSize
//////////////////////////////////////////////////////
bool MetricsLibrary::commandBufferGetSize(
    const CommandBufferData_1_0 &commandBufferData,
    CommandBufferSize_1_0 &commandBufferSize) {
    return api->functions.CommandBufferGetSize(
               &commandBufferData,
               &commandBufferSize) == StatusCode::Success;
}

//////////////////////////////////////////////////////
// MetricsLibrary::oaConfigurationCreate
//////////////////////////////////////////////////////
bool MetricsLibrary::oaConfigurationCreate(
    const ContextHandle_1_0 &context,
    ConfigurationHandle_1_0 &handle) {
    ConfigurationCreateData_1_0 data = {};
    data.HandleContext = context;
    data.Type = ObjectType::ConfigurationHwCountersOa;

    return api->functions.ConfigurationCreate(
               &data,
               &handle) == StatusCode::Success;
}

//////////////////////////////////////////////////////
// MetricsLibrary::oaConfigurationDelete
//////////////////////////////////////////////////////
bool MetricsLibrary::oaConfigurationDelete(
    const ConfigurationHandle_1_0 &handle) {

    return api->functions.ConfigurationDelete(handle) == StatusCode::Success;
}
} // namespace NEO