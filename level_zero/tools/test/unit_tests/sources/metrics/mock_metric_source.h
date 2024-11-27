/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/tools/source/metrics/metric.h"

namespace L0 {
namespace ult {

class MockMetricSource : public L0::MetricSource {
  public:
    bool isAvailableReturn = false;
    void enable() override {}
    bool isAvailable() override { return isAvailableReturn; }
    ze_result_t appendMetricMemoryBarrier(L0::CommandList &commandList) override { return ZE_RESULT_ERROR_UNKNOWN; }
    ze_result_t metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) override { return ZE_RESULT_ERROR_UNKNOWN; }
    ze_result_t handleMetricGroupExtendedProperties(zet_metric_group_handle_t hMetricGroup, void *pNext) override { return ZE_RESULT_ERROR_UNKNOWN; }
    ze_result_t activateMetricGroupsPreferDeferred(uint32_t count, zet_metric_group_handle_t *phMetricGroups) override { return ZE_RESULT_ERROR_UNKNOWN; }
    ze_result_t activateMetricGroupsAlreadyDeferred() override { return ZE_RESULT_ERROR_UNKNOWN; }
    ze_result_t metricProgrammableGet(uint32_t *pCount, zet_metric_programmable_exp_handle_t *phMetricProgrammables) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t createMetricGroupsFromMetrics(std::vector<zet_metric_handle_t> &metricList,
                                              const char metricGroupNamePrefix[ZET_INTEL_MAX_METRIC_GROUP_NAME_PREFIX_EXP],
                                              const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                                              uint32_t *maxMetricGroupCount,
                                              std::vector<zet_metric_group_handle_t> &metricGroupList) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t getConcurrentMetricGroups(std::vector<zet_metric_group_handle_t> &hMetricGroups,
                                          uint32_t *pConcurrentGroupCount,
                                          uint32_t *pCountPerConcurrentGroup) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    void setType(uint32_t type) {
        this->type = type;
    }

    ~MockMetricSource() override = default;
};

class MockMetricGroup : public L0::MetricGroupImp {

  public:
    using L0::MetricGroupImp::isMultiDevice;

    ~MockMetricGroup() override = default;
    MockMetricGroup(MetricSource &metricSource) : L0::MetricGroupImp(metricSource) {}
    ze_result_t getProperties(zet_metric_group_properties_t *pProperties) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                      const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                      zet_typed_value_t *pMetricValues) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                         const uint8_t *pRawData, uint32_t *pSetCount,
                                         uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                         zet_typed_value_t *pMetricValues) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t getMetricTimestampsExp(const ze_bool_t synchronizedWithHost,
                                       uint64_t *globalTimestamp,
                                       uint64_t *metricTimestamp) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    bool activate() override {
        return false;
    }
    bool deactivate() override {
        return false;
    }
    zet_metric_group_handle_t getMetricGroupForSubDevice(const uint32_t subDeviceIndex) override {
        return nullptr;
    }
    ze_result_t streamerOpen(
        zet_context_handle_t hContext,
        zet_device_handle_t hDevice,
        zet_metric_streamer_desc_t *desc,
        ze_event_handle_t hNotificationEvent,
        zet_metric_streamer_handle_t *phMetricStreamer) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t metricQueryPoolCreate(
        zet_context_handle_t hContext,
        zet_device_handle_t hDevice,
        const zet_metric_query_pool_desc_t *desc,
        zet_metric_query_pool_handle_t *phMetricQueryPool) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    ze_result_t getExportData(const uint8_t *pRawData, size_t rawDataSize, size_t *pExportDataSize,
                              uint8_t *pExportData) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t addMetric(zet_metric_handle_t hMetric, size_t *errorStringSize, char *pErrorString) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t removeMetric(zet_metric_handle_t hMetric) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t close() override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t destroy() override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
};

class MockMetric : public L0::MetricImp {
  public:
    ze_result_t destroyReturn = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    ~MockMetric() override = default;
    MockMetric(MetricSource &metricSource) : L0::MetricImp(metricSource) {}
    ze_result_t getProperties(zet_metric_properties_t *pProperties) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t destroy() override {
        return destroyReturn;
    }

    void setPredefined(bool status) {
        isPredefined = status;
    }

    void setMultiDevice(bool status) {
        isMultiDevice = status;
    }
};

} // namespace ult
} // namespace L0
