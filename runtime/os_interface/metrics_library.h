/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/os_interface/os_library.h"

#include "instrumentation.h"

#include <memory>

namespace NEO {

using MetricsLibraryApi::ClientApi;
using MetricsLibraryApi::ClientCallbacks_1_0;
using MetricsLibraryApi::ClientData_1_0;
using MetricsLibraryApi::ClientGen;
using MetricsLibraryApi::ClientType_1_0;
using MetricsLibraryApi::CommandBufferData_1_0;
using MetricsLibraryApi::CommandBufferSize_1_0;
using MetricsLibraryApi::ConfigurationActivateData_1_0;
using MetricsLibraryApi::ConfigurationCreateData_1_0;
using MetricsLibraryApi::ConfigurationHandle_1_0;
using MetricsLibraryApi::ContextCreateData_1_0;
using MetricsLibraryApi::ContextCreateFunction_1_0;
using MetricsLibraryApi::ContextDeleteFunction_1_0;
using MetricsLibraryApi::ContextHandle_1_0;
using MetricsLibraryApi::GetReportData_1_0;
using MetricsLibraryApi::GpuConfigurationActivationType;
using MetricsLibraryApi::GpuMemory_1_0;
using MetricsLibraryApi::Interface_1_0;
using MetricsLibraryApi::ObjectType;
using MetricsLibraryApi::ParameterType;
using MetricsLibraryApi::QueryCreateData_1_0;
using MetricsLibraryApi::QueryHandle_1_0;
using MetricsLibraryApi::StatusCode;
using MetricsLibraryApi::TypedValue_1_0;
using MetricsLibraryApi::ValueType;

class MetricsLibraryInterface {
  public:
    ContextCreateFunction_1_0 contextCreate = nullptr;
    ContextDeleteFunction_1_0 contextDelete = nullptr;
    Interface_1_0 functions = {};
    ClientCallbacks_1_0 callbacks = {};
};

class MetricsLibrary {
  public:
    MetricsLibrary();
    MOCKABLE_VIRTUAL ~MetricsLibrary(){};

    // Library open function.
    MOCKABLE_VIRTUAL bool open();

    // Context create / destroy functions.
    MOCKABLE_VIRTUAL bool contextCreate(const ClientType_1_0 &client, ClientData_1_0 &clientData, ContextCreateData_1_0 &createData, ContextHandle_1_0 &handle);
    MOCKABLE_VIRTUAL bool contextDelete(const ContextHandle_1_0 &handle);

    // HwCounters functions.
    MOCKABLE_VIRTUAL bool hwCountersCreate(const ContextHandle_1_0 &context, const uint32_t slots, const ConfigurationHandle_1_0 mmio, QueryHandle_1_0 &handle);
    MOCKABLE_VIRTUAL bool hwCountersDelete(const QueryHandle_1_0 &handle);
    MOCKABLE_VIRTUAL bool hwCountersGetReport(const QueryHandle_1_0 &handle, const uint32_t slot, const uint32_t slotsCount, const uint32_t dataSize, void *data);
    MOCKABLE_VIRTUAL uint32_t hwCountersGetApiReportSize();
    MOCKABLE_VIRTUAL uint32_t hwCountersGetGpuReportSize();

    // Oa configuration functions.
    MOCKABLE_VIRTUAL bool oaConfigurationCreate(const ContextHandle_1_0 &context, ConfigurationHandle_1_0 &handle);
    MOCKABLE_VIRTUAL bool oaConfigurationDelete(const ConfigurationHandle_1_0 &handle);
    MOCKABLE_VIRTUAL bool oaConfigurationActivate(const ConfigurationHandle_1_0 &handle);
    MOCKABLE_VIRTUAL bool oaConfigurationDeactivate(const ConfigurationHandle_1_0 &handle);

    // User mmio configuration functions.
    MOCKABLE_VIRTUAL bool userConfigurationCreate(const ContextHandle_1_0 &context, ConfigurationHandle_1_0 &handle);
    MOCKABLE_VIRTUAL bool userConfigurationDelete(const ConfigurationHandle_1_0 &handle);

    // Command buffer functions.
    MOCKABLE_VIRTUAL bool commandBufferGet(CommandBufferData_1_0 &data);
    MOCKABLE_VIRTUAL bool commandBufferGetSize(const CommandBufferData_1_0 &commandBufferData, CommandBufferSize_1_0 &commandBufferSize);

  public:
    std::unique_ptr<OsLibrary> osLibrary;
    std::unique_ptr<MetricsLibraryInterface> api;
};
} // namespace NEO