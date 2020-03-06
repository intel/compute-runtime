/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/event.h"
#include <level_zero/zet_api.h>

#include <vector>

struct _zet_metric_group_handle_t {};
struct _zet_metric_handle_t {};
struct _zet_metric_tracer_handle_t {};
struct _zet_metric_query_pool_handle_t {};
struct _zet_metric_query_handle_t {};

///////////////////////////////////////////////////////////////////////////////
/// @brief Metrics Discovery forward declarations.
namespace MetricsDiscovery {
class IMetricSet_1_5;
class IConcurrentGroup_1_5;
} // namespace MetricsDiscovery

namespace L0 {
///////////////////////////////////////////////////////////////////////////////
/// @brief Level zero forward declarations.
struct MetricsLibrary;
struct CommandList;
struct MetricEnumeration;
struct MetricTracer;

///////////////////////////////////////////////////////////////////////////////
/// @brief Metric context base object.
struct MetricContext {
    virtual ~MetricContext() = default;
    static std::unique_ptr<MetricContext> create(struct Device &device);
    static bool isMetricApiAvailable();
    virtual bool loadDependencies() = 0;
    virtual bool isInitialized() = 0;
    virtual void setInitializationState(const ze_result_t state) = 0;
    virtual Device &getDevice() = 0;
    virtual MetricsLibrary &getMetricsLibrary() = 0;
    virtual MetricEnumeration &getMetricEnumeration() = 0;
    virtual MetricTracer *getMetricTracer() = 0;
    virtual void setMetricTracer(MetricTracer *pMetricTracer) = 0;
    virtual void setMetricsLibrary(MetricsLibrary &metricsLibrary) = 0;
    virtual void setMetricEnumeration(MetricEnumeration &metricEnumeration) = 0;

    // Called by zetInit.
    static void enableMetricApi(ze_result_t &result);

    // Metric groups activation.
    virtual ze_result_t activateMetricGroups() = 0;
    virtual ze_result_t activateMetricGroupsDeferred(const uint32_t count,
                                                     zet_metric_group_handle_t *phMetricGroups) = 0;
    virtual bool isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) = 0;

    virtual void setUseCcs(const bool useCcs) = 0;
    virtual bool isCcsUsed() = 0;
};

///////////////////////////////////////////////////////////////////////////////
/// @brief Metric base object.
struct Metric : _zet_metric_handle_t {
    virtual ~Metric() = default;

    // API.
    virtual ze_result_t getProperties(zet_metric_properties_t *pProperties) = 0;

    // Non-API.
    static Metric *create(zet_metric_properties_t &properties);
    static Metric *fromHandle(zet_metric_handle_t handle) { return static_cast<Metric *>(handle); }
    inline zet_metric_handle_t toHandle() { return this; }
};

///////////////////////////////////////////////////////////////////////////////
/// @brief Metric group base object.
struct MetricGroup : _zet_metric_group_handle_t {
    virtual ~MetricGroup() = default;

    // API.
    virtual ze_result_t getProperties(zet_metric_group_properties_t *pProperties) = 0;
    virtual ze_result_t getMetric(uint32_t *pCount, zet_metric_handle_t *phMetrics) = 0;
    virtual ze_result_t calculateMetricValues(size_t rawDataSize,
                                              const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                              zet_typed_value_t *pMetricValues) = 0;

    // Non-API static.
    static MetricGroup *create(zet_metric_group_properties_t &properties,
                               MetricsDiscovery::IMetricSet_1_5 &metricSet,
                               MetricsDiscovery::IConcurrentGroup_1_5 &concurrentGroup,
                               const std::vector<Metric *> &metrics);
    static MetricGroup *fromHandle(zet_metric_group_handle_t handle) {
        return static_cast<MetricGroup *>(handle);
    }
    static zet_metric_group_properties_t getProperties(const zet_metric_group_handle_t handle);

    // Non-API.
    zet_metric_group_handle_t toHandle() { return this; }

    virtual uint32_t getRawReportSize() = 0;

    virtual bool activate() = 0;
    virtual bool deactivate() = 0;

    virtual ze_result_t openIoStream(uint32_t &timerPeriodNs, uint32_t &oaBufferSize) = 0;
    virtual ze_result_t waitForReports(const uint32_t timeoutMs) = 0;
    virtual ze_result_t readIoStream(uint32_t &reportCount, uint8_t &reportData) = 0;
    virtual ze_result_t closeIoStream() = 0;
};

///////////////////////////////////////////////////////////////////////////////
/// @brief Metric tracer base object.
struct MetricTracer : _zet_metric_tracer_handle_t {
    virtual ~MetricTracer() = default;

    // API.
    virtual ze_result_t readData(uint32_t maxReportCount, size_t *pRawDataSize,
                                 uint8_t *pRawData) = 0;
    virtual ze_result_t close() = 0;

    // Non-API static.
    static MetricTracer *open(zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                              zet_metric_tracer_desc_t &desc, ze_event_handle_t hNotificationEvent);
    static MetricTracer *fromHandle(zet_metric_tracer_handle_t handle) {
        return static_cast<MetricTracer *>(handle);
    }

    // Non-API.
    virtual Event::State getNotificationState() = 0;
    inline zet_metric_tracer_handle_t toHandle() { return this; }
};

///////////////////////////////////////////////////////////////////////////////
/// @brief Metric query pool base object.
struct MetricQueryPool : _zet_metric_query_pool_handle_t {
    virtual ~MetricQueryPool() = default;

    // API.
    virtual bool destroy() = 0;
    virtual ze_result_t createMetricQuery(uint32_t index,
                                          zet_metric_query_handle_t *phMetricQuery) = 0;

    // Non-API static.
    static MetricQueryPool *create(zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup, const zet_metric_query_pool_desc_t &desc);
    static MetricQueryPool *fromHandle(zet_metric_query_pool_handle_t handle);

    // Non-API.
    zet_metric_query_pool_handle_t toHandle();
};

///////////////////////////////////////////////////////////////////////////////
/// @brief Metric query base object.
struct MetricQuery : _zet_metric_query_handle_t {
    virtual ~MetricQuery() = default;

    // API.
    virtual ze_result_t appendBegin(CommandList &commandList) = 0;
    virtual ze_result_t appendEnd(CommandList &commandList, ze_event_handle_t hCompletionEvent) = 0;

    static ze_result_t appendMemoryBarrier(CommandList &commandList);
    static ze_result_t appendTracerMarker(CommandList &commandList,
                                          zet_metric_tracer_handle_t hMetricTracer, uint32_t value);

    virtual ze_result_t getData(size_t *pRawDataSize, uint8_t *pRawData) = 0;

    virtual ze_result_t reset() = 0;
    virtual ze_result_t destroy() = 0;

    // Non-API static.
    static MetricQuery *fromHandle(zet_metric_query_handle_t handle);

    // Non-API.
    zet_metric_query_handle_t toHandle();
};

// MetricGroup.
ze_result_t metricGroupGet(zet_device_handle_t hDevice, uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups);

// Metric.
ze_result_t metricGet(zet_metric_group_handle_t hMetricGroup, uint32_t *pCount, zet_metric_handle_t *phMetrics);

// MetricTracer.
ze_result_t metricTracerOpen(zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                             zet_metric_tracer_desc_t *pDesc, ze_event_handle_t hNotificationEvent,
                             zet_metric_tracer_handle_t *phMetricTracer);

// MetricQueryPool.
ze_result_t metricQueryPoolCreate(zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup, const zet_metric_query_pool_desc_t *pDesc,
                                  zet_metric_query_pool_handle_t *phMetricQueryPool);
ze_result_t metricQueryPoolDestroy(zet_metric_query_pool_handle_t hMetricQueryPool);

} // namespace L0
