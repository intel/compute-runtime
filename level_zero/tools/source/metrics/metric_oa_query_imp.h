/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/metrics/metric.h"

#include "metrics_library_api_1_0.h"

#include <map>
#include <mutex>
#include <vector>

using MetricsLibraryApi::ClientCallbacks_1_0;
using MetricsLibraryApi::ClientGen;
using MetricsLibraryApi::ClientHandle_1_0;
using MetricsLibraryApi::ClientOptionsData_1_0;
using MetricsLibraryApi::CommandBufferData_1_0;
using MetricsLibraryApi::ConfigurationHandle_1_0;
using MetricsLibraryApi::ContextCreateData_1_0;
using MetricsLibraryApi::ContextCreateFunction_1_0;
using MetricsLibraryApi::ContextDeleteFunction_1_0;
using MetricsLibraryApi::ContextHandle_1_0;
using MetricsLibraryApi::Interface_1_0;
using MetricsLibraryApi::QueryHandle_1_0;
using MetricsLibraryApi::StatusCode;

namespace L0 {
struct Device;
struct CommandList;
struct MetricGroup;
class OaMetricSourceImp;
} // namespace L0

namespace NEO {
class OsLibrary;
class GraphicsAllocation;
class GfxCoreHelper;
} // namespace NEO

namespace L0 {

struct MetricsLibrary {
  public:
    MetricsLibrary(OaMetricSourceImp &metricSource);
    virtual ~MetricsLibrary();

    // Initialization.
    virtual bool load();
    bool isInitialized();
    ze_result_t getInitializationState();
    void enableWorkloadPartition();
    void getSubDeviceClientOptions(ClientOptionsData_1_0 &subDevice,
                                   ClientOptionsData_1_0 &subDeviceIndex,
                                   ClientOptionsData_1_0 &subDeviceCount,
                                   ClientOptionsData_1_0 &workloadPartition);
    static const char *getFilename();

    // Deinitialization.
    void release();

    // Metric query.
    uint32_t getQueryReportGpuSize();
    bool createMetricQuery(const uint32_t slotsCount, QueryHandle_1_0 &query,
                           NEO::GraphicsAllocation *&pAllocation);
    uint32_t getMetricQueryCount();
    bool getMetricQueryReport(QueryHandle_1_0 &query, const uint32_t slot, const size_t rawDataSize, uint8_t *pData);
    virtual bool getMetricQueryReportSize(size_t &rawDataSize);
    bool destroyMetricQuery(QueryHandle_1_0 &query);

    // Command buffer.
    bool getGpuCommands(CommandList &commandList, CommandBufferData_1_0 &commandBuffer);
    bool getGpuCommands(CommandBufferData_1_0 &commandBuffer);
    uint32_t getGpuCommandsSize(CommandBufferData_1_0 &commandBuffer);
    static StatusCode ML_STDCALL flushCommandBufferCallback(ClientHandle_1_0 handle);

    // Metric group configuration.
    ConfigurationHandle_1_0 getConfiguration(const zet_metric_group_handle_t metricGroup);
    bool activateConfiguration(const ConfigurationHandle_1_0 configurationHandle);
    bool deactivateConfiguration(const ConfigurationHandle_1_0 configurationHandle);
    void cacheConfiguration(zet_metric_group_handle_t metricGroup, ConfigurationHandle_1_0 configurationHandle);
    void deleteAllConfigurations();

  protected:
    void initialize();

    bool createContext();
    virtual bool getContextData(Device &device, ContextCreateData_1_0 &contextData);

    ConfigurationHandle_1_0 createConfiguration(const zet_metric_group_handle_t metricGroup,
                                                const zet_metric_group_properties_t &properties);
    ConfigurationHandle_1_0 addConfiguration(const zet_metric_group_handle_t metricGroup);

    ClientGen getGenType(const NEO::GfxCoreHelper &gfxCoreHelper) const;

  protected:
    std::unique_ptr<NEO::OsLibrary> handle;
    OaMetricSourceImp &metricSource;
    ze_result_t initializationState = ZE_RESULT_ERROR_UNINITIALIZED;
    bool isWorkloadPartitionEnabled = false;
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

struct OaMetricQueryImp : MetricQuery {
  public:
    OaMetricQueryImp(OaMetricSourceImp &metricSource, struct OaMetricQueryPoolImp &pool,
                     const uint32_t slot);

    ze_result_t appendBegin(CommandList &commandList) override;
    ze_result_t appendEnd(CommandList &commandList, ze_event_handle_t hSignalEvent,
                          uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;

    ze_result_t getData(size_t *pRawDataSize, uint8_t *pRawData) override;

    ze_result_t reset() override;
    ze_result_t destroy() override;

    std::vector<zet_metric_query_handle_t> &getMetricQueries();

  protected:
    ze_result_t writeMetricQuery(CommandList &commandList, ze_event_handle_t hSignalEvent,
                                 uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                 const bool begin);
    ze_result_t writeSkipExecutionQuery(CommandList &commandList, ze_event_handle_t hSignalEvent,
                                        uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                        const bool begin);

  protected:
    OaMetricSourceImp &metricSource;
    MetricsLibrary &metricsLibrary;
    OaMetricQueryPoolImp &pool;
    uint32_t slot;
    std::vector<zet_metric_query_handle_t> metricQueries;
};

struct OaMetricQueryPoolImp : MetricQueryPool {
  public:
    OaMetricQueryPoolImp(OaMetricSourceImp &metricSource, zet_metric_group_handle_t hEventMetricGroup, const zet_metric_query_pool_desc_t &poolDescription);

    bool create();
    ze_result_t destroy() override;

    ze_result_t metricQueryCreate(uint32_t index, zet_metric_query_handle_t *phMetricQuery) override;

    bool allocateGpuMemory();

    std::vector<zet_metric_query_pool_handle_t> &getMetricQueryPools();

    static ze_result_t metricQueryPoolCreate(zet_context_handle_t hContext, zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                                             const zet_metric_query_pool_desc_t *pDesc, zet_metric_query_pool_handle_t *phMetricQueryPool);

  protected:
    bool createMetricQueryPool();
    bool createSkipExecutionQueryPool();

  public:
    OaMetricSourceImp &metricSource;
    MetricsLibrary &metricsLibrary;
    std::vector<OaMetricQueryImp> pool;
    NEO::GraphicsAllocation *pAllocation = nullptr;
    uint32_t allocationSize = 0;
    zet_metric_query_pool_desc_t description = {};
    zet_metric_group_handle_t hMetricGroup = nullptr;
    QueryHandle_1_0 query = {};

  protected:
    std::vector<zet_metric_query_pool_handle_t> metricQueryPools;
};
} // namespace L0
