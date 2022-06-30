/*
 * Copyright (C) 2020-2022 Intel Corporation
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
struct CommandList;
struct MetricStreamer;

class MetricSource {
  public:
    enum class SourceType {
        Undefined,
        Oa,
        IpSampling
    };
    virtual void enable() = 0;
    virtual bool isAvailable() = 0;
    virtual ze_result_t appendMetricMemoryBarrier(CommandList &commandList) = 0;
    virtual ze_result_t metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) = 0;
    virtual ~MetricSource() = default;
};

class MetricDeviceContext {

  public:
    MetricDeviceContext(Device &device);
    ze_result_t metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups);
    ze_result_t activateMetricGroupsDeferred(uint32_t count, zet_metric_group_handle_t *phMetricGroups);
    ze_result_t activateMetricGroups();
    ze_result_t appendMetricMemoryBarrier(CommandList &commandList);
    bool isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) const;
    bool isMetricGroupActivated() const;
    bool isImplicitScalingCapable() const;
    Device &getDevice() const;
    uint32_t getSubDeviceIndex() const;
    template <typename T>
    T &getMetricSource() const;
    void setSubDeviceIndex(uint32_t subDeviceIndex) { this->subDeviceIndex = subDeviceIndex; }

    static std::unique_ptr<MetricDeviceContext> create(Device &device);
    static ze_result_t enableMetricApi();

  private:
    bool enable();
    ze_result_t activateAllDomains();
    ze_result_t deActivateAllDomains();
    struct Device &device;
    std::map<uint32_t, std::pair<zet_metric_group_handle_t, bool>> domains;
    bool multiDeviceCapable = false;
    uint32_t subDeviceIndex = 0;
    std::map<MetricSource::SourceType, std::unique_ptr<MetricSource>> metricSources;
};

struct Metric : _zet_metric_handle_t {
    virtual ~Metric() = default;

    virtual ze_result_t getProperties(zet_metric_properties_t *pProperties) = 0;

    static Metric *fromHandle(zet_metric_handle_t handle) { return static_cast<Metric *>(handle); }
    inline zet_metric_handle_t toHandle() { return this; }
};

struct MetricGroup : _zet_metric_group_handle_t {
    virtual ~MetricGroup() = default;

    virtual ze_result_t getProperties(zet_metric_group_properties_t *pProperties) = 0;
    virtual ze_result_t metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) = 0;
    virtual ze_result_t calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                              const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                              zet_typed_value_t *pMetricValues) = 0;
    virtual ze_result_t calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                 const uint8_t *pRawData, uint32_t *pSetCount,
                                                 uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                                 zet_typed_value_t *pMetricValues) = 0;
    static MetricGroup *fromHandle(zet_metric_group_handle_t handle) {
        return static_cast<MetricGroup *>(handle);
    }
    zet_metric_group_handle_t toHandle() { return this; }
    virtual bool activate() = 0;
    virtual bool deactivate() = 0;
    virtual zet_metric_group_handle_t getMetricGroupForSubDevice(const uint32_t subDeviceIndex) = 0;
    virtual ze_result_t streamerOpen(
        zet_context_handle_t hContext,
        zet_device_handle_t hDevice,
        zet_metric_streamer_desc_t *desc,
        ze_event_handle_t hNotificationEvent,
        zet_metric_streamer_handle_t *phMetricStreamer) = 0;
    virtual ze_result_t metricQueryPoolCreate(
        zet_context_handle_t hContext,
        zet_device_handle_t hDevice,
        const zet_metric_query_pool_desc_t *desc,
        zet_metric_query_pool_handle_t *phMetricQueryPool) = 0;
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
    static MetricStreamer *fromHandle(zet_metric_streamer_handle_t handle) {
        return static_cast<MetricStreamer *>(handle);
    }
    virtual ze_result_t appendStreamerMarker(CommandList &commandList, uint32_t value) = 0;
    virtual Event::State getNotificationState() = 0;
    inline zet_metric_streamer_handle_t toHandle() { return this; }
};

struct MetricQueryPool : _zet_metric_query_pool_handle_t {
    virtual ~MetricQueryPool() = default;

    virtual ze_result_t destroy() = 0;
    virtual ze_result_t metricQueryCreate(uint32_t index,
                                          zet_metric_query_handle_t *phMetricQuery) = 0;

    static MetricQueryPool *fromHandle(zet_metric_query_pool_handle_t handle);

    zet_metric_query_pool_handle_t toHandle();
};

struct MetricQuery : _zet_metric_query_handle_t {
    virtual ~MetricQuery() = default;

    virtual ze_result_t appendBegin(CommandList &commandList) = 0;
    virtual ze_result_t appendEnd(CommandList &commandList, ze_event_handle_t hSignalEvent,
                                  uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) = 0;
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
