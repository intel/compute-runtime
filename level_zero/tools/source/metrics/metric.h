/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/event/event.h"
#include <level_zero/zet_api.h>

#include "metrics_discovery_api.h"

#include <vector>

struct _zet_metric_group_handle_t {};
struct _zet_metric_handle_t {};
struct _zet_metric_streamer_handle_t {};
struct _zet_metric_query_pool_handle_t {};
struct _zet_metric_query_handle_t {};

namespace L0 {
struct MetricsLibrary;
struct CommandList;
struct MetricEnumeration;
struct MetricStreamer;

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
    virtual MetricStreamer *getMetricStreamer() = 0;
    virtual void setMetricStreamer(MetricStreamer *pMetricStreamer) = 0;
    virtual void setMetricsLibrary(MetricsLibrary &metricsLibrary) = 0;
    virtual void setMetricEnumeration(MetricEnumeration &metricEnumeration) = 0;

    // Called by zeInit.
    static ze_result_t enableMetricApi();

    // Metric groups activation.
    virtual ze_result_t activateMetricGroups() = 0;
    virtual ze_result_t activateMetricGroupsDeferred(const uint32_t count,
                                                     zet_metric_group_handle_t *phMetricGroups) = 0;
    virtual bool isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) = 0;
    virtual bool isMetricGroupActivated() = 0;

    virtual void setUseCompute(const bool useCompute) = 0;
    virtual bool isComputeUsed() = 0;
    virtual uint32_t getSubDeviceIndex() = 0;
    virtual void setSubDeviceIndex(const uint32_t index) = 0;
    virtual bool isImplicitScalingCapable() = 0;
};

struct Metric : _zet_metric_handle_t {
    virtual ~Metric() = default;

    virtual ze_result_t getProperties(zet_metric_properties_t *pProperties) = 0;

    static Metric *create(zet_metric_properties_t &properties);
    static Metric *fromHandle(zet_metric_handle_t handle) { return static_cast<Metric *>(handle); }
    inline zet_metric_handle_t toHandle() { return this; }
};

struct MetricGroup : _zet_metric_group_handle_t {
    virtual ~MetricGroup() = default;

    virtual ze_result_t getProperties(zet_metric_group_properties_t *pProperties) = 0;
    virtual ze_result_t getMetric(uint32_t *pCount, zet_metric_handle_t *phMetrics) = 0;
    virtual ze_result_t calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                              const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                              zet_typed_value_t *pMetricValues) = 0;
    virtual ze_result_t calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                 const uint8_t *pRawData, uint32_t *pSetCount,
                                                 uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                                 zet_typed_value_t *pMetricValues) = 0;

    static MetricGroup *create(zet_metric_group_properties_t &properties,
                               MetricsDiscovery::IMetricSet_1_5 &metricSet,
                               MetricsDiscovery::IConcurrentGroup_1_5 &concurrentGroup,
                               const std::vector<Metric *> &metrics);
    static MetricGroup *fromHandle(zet_metric_group_handle_t handle) {
        return static_cast<MetricGroup *>(handle);
    }
    static zet_metric_group_properties_t getProperties(const zet_metric_group_handle_t handle);

    zet_metric_group_handle_t toHandle() { return this; }

    virtual uint32_t getRawReportSize() = 0;

    virtual bool activate() = 0;
    virtual bool deactivate() = 0;

    virtual ze_result_t openIoStream(uint32_t &timerPeriodNs, uint32_t &oaBufferSize) = 0;
    virtual ze_result_t waitForReports(const uint32_t timeoutMs) = 0;
    virtual ze_result_t readIoStream(uint32_t &reportCount, uint8_t &reportData) = 0;
    virtual ze_result_t closeIoStream() = 0;
};

struct MetricGroupCalculateHeader {
    static constexpr uint32_t magicValue = 0xFFFEDCBA;

    uint32_t magic;
    uint32_t dataCount;
    uint32_t rawDataOffsets;
    uint32_t rawDataSizes;
    uint32_t rawDataOffset;
};

struct MetricStreamer : _zet_metric_streamer_handle_t {
    virtual ~MetricStreamer() = default;

    virtual ze_result_t readData(uint32_t maxReportCount, size_t *pRawDataSize,
                                 uint8_t *pRawData) = 0;
    virtual ze_result_t close() = 0;
    static ze_result_t openForDevice(Device *pDevice, zet_metric_group_handle_t hMetricGroup,
                                     zet_metric_streamer_desc_t &desc,
                                     zet_metric_streamer_handle_t *phMetricStreamer);
    static ze_result_t open(zet_context_handle_t hContext, zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                            zet_metric_streamer_desc_t &desc, ze_event_handle_t hNotificationEvent, zet_metric_streamer_handle_t *phMetricStreamer);
    static MetricStreamer *fromHandle(zet_metric_streamer_handle_t handle) {
        return static_cast<MetricStreamer *>(handle);
    }

    virtual Event::State getNotificationState() = 0;
    inline zet_metric_streamer_handle_t toHandle() { return this; }
};

struct MetricQueryPool : _zet_metric_query_pool_handle_t {
    virtual ~MetricQueryPool() = default;

    virtual ze_result_t destroy() = 0;
    virtual ze_result_t createMetricQuery(uint32_t index,
                                          zet_metric_query_handle_t *phMetricQuery) = 0;

    static MetricQueryPool *fromHandle(zet_metric_query_pool_handle_t handle);

    zet_metric_query_pool_handle_t toHandle();
};

struct MetricQuery : _zet_metric_query_handle_t {
    virtual ~MetricQuery() = default;

    virtual ze_result_t appendBegin(CommandList &commandList) = 0;
    virtual ze_result_t appendEnd(CommandList &commandList, ze_event_handle_t hSignalEvent,
                                  uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) = 0;

    static ze_result_t appendMemoryBarrier(CommandList &commandList);
    static ze_result_t appendStreamerMarker(CommandList &commandList,
                                            zet_metric_streamer_handle_t hMetricStreamer, uint32_t value);

    virtual ze_result_t getData(size_t *pRawDataSize, uint8_t *pRawData) = 0;

    virtual ze_result_t reset() = 0;
    virtual ze_result_t destroy() = 0;

    static MetricQuery *fromHandle(zet_metric_query_handle_t handle);

    zet_metric_query_handle_t toHandle();
};

// MetricGroup.
ze_result_t metricGroupGet(zet_device_handle_t hDevice, uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups);

// MetricStreamer.
ze_result_t metricStreamerOpen(zet_context_handle_t hContext, zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                               zet_metric_streamer_desc_t *pDesc, ze_event_handle_t hNotificationEvent,
                               zet_metric_streamer_handle_t *phMetricStreamer);

// MetricQueryPool.
ze_result_t metricQueryPoolCreate(zet_context_handle_t hContext, zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                                  const zet_metric_query_pool_desc_t *pDesc, zet_metric_query_pool_handle_t *phMetricQueryPool);

} // namespace L0
