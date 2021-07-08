/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/metrics/metric.h"

#include <map>
#include <vector>

using MetricsLibraryApi::ClientCallbacks_1_0;
using MetricsLibraryApi::ClientGen;
using MetricsLibraryApi::ClientOptionsData_1_0;
using MetricsLibraryApi::CommandBufferData_1_0;
using MetricsLibraryApi::ConfigurationHandle_1_0;
using MetricsLibraryApi::ContextCreateData_1_0;
using MetricsLibraryApi::ContextCreateFunction_1_0;
using MetricsLibraryApi::ContextDeleteFunction_1_0;
using MetricsLibraryApi::ContextHandle_1_0;
using MetricsLibraryApi::Interface_1_0;
using MetricsLibraryApi::QueryHandle_1_0;

namespace L0 {
struct Device;
struct CommandList;
struct MetricGroup;
} // namespace L0

namespace NEO {
class OsLibrary;
class GraphicsAllocation;
} // namespace NEO

namespace L0 {

struct MetricsLibrary {
  public:
    MetricsLibrary(MetricContext &metricContext);
    virtual ~MetricsLibrary();

    // Initialization.
    virtual bool load();
    bool isInitialized();
    ze_result_t getInitializationState();
    void getSubDeviceClientOptions(NEO::Device &neoDevice,
                                   ClientOptionsData_1_0 &subDevice,
                                   ClientOptionsData_1_0 &subDeviceIndex,
                                   ClientOptionsData_1_0 &subDeviceCount);
    static const char *getFilename();

    // Deinitialization.
    void release();

    // Metric query.
    bool createMetricQuery(const uint32_t slotsCount, QueryHandle_1_0 &query,
                           NEO::GraphicsAllocation *&pAllocation);
    uint32_t getMetricQueryCount();
    bool getMetricQueryReport(QueryHandle_1_0 &query, const uint32_t slot, const size_t rawDataSize, uint8_t *pData);
    virtual bool getMetricQueryReportSize(size_t &rawDataSize);
    bool destroyMetricQuery(QueryHandle_1_0 &query);

    // Command buffer.
    bool getGpuCommands(CommandList &commandList, CommandBufferData_1_0 &commandBuffer);
    uint32_t getGpuCommandsSize(CommandBufferData_1_0 &commandBuffer);

    // Metric group configuration.
    ConfigurationHandle_1_0 getConfiguration(const zet_metric_group_handle_t metricGroup);
    bool activateConfiguration(const ConfigurationHandle_1_0 configurationHandle);
    bool deactivateConfiguration(const ConfigurationHandle_1_0 configurationHandle);
    void cacheConfiguration(zet_metric_group_handle_t metricGroup, ConfigurationHandle_1_0 configurationHandle);
    void deleteAllConfigurations();

  protected:
    void initialize();

    bool createContext();
    bool createContextForDevice(Device &device);
    virtual bool getContextData(Device &device, ContextCreateData_1_0 &contextData);

    ConfigurationHandle_1_0 createConfiguration(const zet_metric_group_handle_t metricGroup,
                                                const zet_metric_group_properties_t properties);
    ConfigurationHandle_1_0 addConfiguration(const zet_metric_group_handle_t metricGroup);

    ClientGen getGenType(const uint32_t gen) const;

  protected:
    NEO::OsLibrary *handle = nullptr;
    MetricContext &metricContext;
    ze_result_t initializationState = ZE_RESULT_ERROR_UNINITIALIZED;
    std::mutex mutex;

    // Metrics library types.
    Interface_1_0 api = {};
    ClientCallbacks_1_0 callbacks = {};
    ContextHandle_1_0 context = {};
    ContextCreateFunction_1_0 contextCreateFunction = nullptr;
    ContextDeleteFunction_1_0 contextDeleteFunction = nullptr;
    std::map<zet_metric_group_handle_t, ConfigurationHandle_1_0> configurations;
    std::vector<QueryHandle_1_0> queries;
};

struct MetricQueryImp : MetricQuery {
  public:
    MetricQueryImp(MetricContext &metricContext, struct MetricQueryPoolImp &pool,
                   const uint32_t slot);

    ze_result_t appendBegin(CommandList &commandList) override;
    ze_result_t appendEnd(CommandList &commandList, ze_event_handle_t hSignalEvent,
                          uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;

    ze_result_t getData(size_t *pRawDataSize, uint8_t *pRawData) override;

    ze_result_t reset() override;
    ze_result_t destroy() override;

  protected:
    ze_result_t writeMetricQuery(CommandList &commandList, ze_event_handle_t hSignalEvent,
                                 uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                 const bool begin);
    ze_result_t writeSkipExecutionQuery(CommandList &commandList, ze_event_handle_t hSignalEvent,
                                        uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                        const bool begin);

  protected:
    MetricContext &metricContext;
    MetricsLibrary &metricsLibrary;
    MetricQueryPoolImp &pool;
    uint32_t slot;
};

struct MetricQueryPoolImp : MetricQueryPool {
  public:
    MetricQueryPoolImp(MetricContext &metricContext, zet_metric_group_handle_t hEventMetricGroup, const zet_metric_query_pool_desc_t &poolDescription);

    bool create();
    ze_result_t destroy() override;

    ze_result_t createMetricQuery(uint32_t index, zet_metric_query_handle_t *phMetricQuery) override;

  protected:
    bool createMetricQueryPool();
    bool createSkipExecutionQueryPool();

  public:
    MetricContext &metricContext;
    MetricsLibrary &metricsLibrary;
    std::vector<MetricQueryImp> pool;
    NEO::GraphicsAllocation *pAllocation = nullptr;
    zet_metric_query_pool_desc_t description = {};
    zet_metric_group_handle_t hMetricGroup = nullptr;
    QueryHandle_1_0 query = {};
};
} // namespace L0
