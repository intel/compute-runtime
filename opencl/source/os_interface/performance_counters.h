/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/event/perf_counter.h"
#include "opencl/source/os_interface/metrics_library.h"

#include <mutex>

namespace NEO {

//////////////////////////////////////////////////////
// Forward declaration.
//////////////////////////////////////////////////////
template <typename Node>
struct TagNode;

//////////////////////////////////////////////////////
// Performance counters implementation.
//////////////////////////////////////////////////////
class PerformanceCounters {
  public:
    //////////////////////////////////////////////////////
    // Constructor/destructor.
    //////////////////////////////////////////////////////
    PerformanceCounters();
    virtual ~PerformanceCounters() = default;

    //////////////////////////////////////////////////////
    // Performance counters creation.
    //////////////////////////////////////////////////////
    static std::unique_ptr<PerformanceCounters> create(class Device *device);
    bool enable(bool ccsEngine);
    void shutdown();
    uint32_t getReferenceNumber();

    /////////////////////////////////////////////////////
    // Gpu oa/mmio configuration.
    /////////////////////////////////////////////////////
    virtual bool enableCountersConfiguration() = 0;
    virtual void releaseCountersConfiguration() = 0;

    //////////////////////////////////////////////////////
    // Gpu commands.
    //////////////////////////////////////////////////////
    uint32_t getGpuCommandsSize(const MetricsLibraryApi::GpuCommandBufferType commandBufferType, const bool begin);
    bool getGpuCommands(const MetricsLibraryApi::GpuCommandBufferType commandBufferType, TagNode<HwPerfCounter> &performanceCounters, const bool begin, const uint32_t bufferSize, void *pBuffer);

    /////////////////////////////////////////////////////
    // Gpu/Api reports.
    /////////////////////////////////////////////////////
    uint32_t getApiReportSize();
    uint32_t getGpuReportSize();
    bool getApiReport(const size_t inputParamSize, void *pClientData, size_t *pOutputSize, bool isEventComplete);

    /////////////////////////////////////////////////////
    // Metrics Library interface.
    /////////////////////////////////////////////////////
    MetricsLibrary *getMetricsLibraryInterface();
    void setMetricsLibraryInterface(std::unique_ptr<MetricsLibrary> newMetricsLibrary);
    bool openMetricsLibrary();
    void closeMetricsLibrary();

    /////////////////////////////////////////////////////
    // Metrics Library context/query handles.
    /////////////////////////////////////////////////////
    ContextHandle_1_0 getMetricsLibraryContext();
    QueryHandle_1_0 getQueryHandle();

  protected:
    /////////////////////////////////////////////////////
    // Common members.
    /////////////////////////////////////////////////////
    std::mutex mutex;
    uint32_t referenceCounter = 0;
    bool available = false;
    bool usingCcsEngine = false;

    /////////////////////////////////////////////////////
    // Metrics Library interface.
    /////////////////////////////////////////////////////
    std::unique_ptr<MetricsLibrary> metricsLibrary = {};

    /////////////////////////////////////////////////////
    // Metrics Library client data.
    /////////////////////////////////////////////////////
    ClientData_1_0 clientData = {};
    ClientType_1_0 clientType = {ClientApi::OpenCL, ClientGen::Unknown};

    /////////////////////////////////////////////////////
    // Metrics Library context.
    /////////////////////////////////////////////////////
    ContextCreateData_1_0 contextData = {};
    ContextHandle_1_0 context = {};

    /////////////////////////////////////////////////////
    // Metrics Library oa/mmio counters configuration.
    /////////////////////////////////////////////////////
    ConfigurationHandle_1_0 oaConfiguration = {};
    ConfigurationHandle_1_0 userConfiguration = {};

    /////////////////////////////////////////////////////
    // Metrics Library query object.
    /////////////////////////////////////////////////////
    QueryHandle_1_0 query = {};
};
} // namespace NEO
