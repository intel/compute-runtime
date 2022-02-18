/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_library.h"

#include "level_zero/tools/source/metrics/metric.h"

#include <vector>

namespace L0 {

class OaMetricSourceImp;

struct MetricEnumeration {
    MetricEnumeration(OaMetricSourceImp &metricSource);
    virtual ~MetricEnumeration();

    ze_result_t metricGroupGet(uint32_t &count, zet_metric_group_handle_t *phMetricGroups);
    MetricGroup *getMetricGroupByIndex(const uint32_t index);
    uint32_t getMetricGroupCount();

    virtual bool isInitialized();

    virtual ze_result_t loadMetricsDiscovery();
    static const char *getMetricsDiscoveryFilename();

  protected:
    ze_result_t initialize();

    virtual ze_result_t openMetricsDiscovery();
    virtual bool getAdapterId(uint32_t &major, uint32_t &minor);
    virtual MetricsDiscovery::IAdapter_1_9 *getMetricsAdapter();
    ze_result_t cleanupMetricsDiscovery();

    ze_result_t cacheMetricInformation();
    ze_result_t cacheMetricGroup(MetricsDiscovery::IMetricSet_1_5 &metricSet,
                                 MetricsDiscovery::IConcurrentGroup_1_5 &pConcurrentGroup,
                                 const uint32_t domain,
                                 const zet_metric_group_sampling_type_flag_t samplingType);
    ze_result_t createMetrics(MetricsDiscovery::IMetricSet_1_5 &metricSet,
                              std::vector<Metric *> &metrics);

    // Metrics Discovery types mapping.
    uint32_t getMetricTierNumber(const uint32_t sourceUsageFlagsMask) const;
    zet_metric_type_t getMetricType(const MetricsDiscovery::TMetricType sourceMetricType) const;
    zet_metric_type_t
    getMetricType(const MetricsDiscovery::TInformationType sourceInformationType) const;
    zet_value_type_t
    getMetricResultType(const MetricsDiscovery::TMetricResultType sourceMetricResultType) const;

  protected:
    OaMetricSourceImp &metricSource;
    std::vector<MetricGroup *> metricGroups; // Cached metric groups
    ze_result_t initializationState = ZE_RESULT_ERROR_UNINITIALIZED;

    // Metrics Discovery API.
    std::unique_ptr<NEO::OsLibrary> hMetricsDiscovery = nullptr;
    MetricsDiscovery::OpenAdapterGroup_fn openAdapterGroup = nullptr;
    MetricsDiscovery::IAdapterGroup_1_9 *pAdapterGroup = nullptr;
    MetricsDiscovery::IAdapter_1_9 *pAdapter = nullptr;
    MetricsDiscovery::IMetricsDevice_1_5 *pMetricsDevice = nullptr;

  public:
    // Metrics Discovery version should be at least 1.5.
    static const uint32_t requiredMetricsDiscoveryMajorVersion = 1;
    static const uint32_t requiredMetricsDiscoveryMinorVersion = 5;
    static const char *oaConcurrentGroupName;
};

struct OaMetricGroupImp : MetricGroup {
    ~OaMetricGroupImp() override;

    ze_result_t getProperties(zet_metric_group_properties_t *pProperties) override;
    ze_result_t metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) override;
    ze_result_t calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize, const uint8_t *pRawData,
                                      uint32_t *pMetricValueCount,
                                      zet_typed_value_t *pCalculatedData) override;
    ze_result_t calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                         const uint8_t *pRawData, uint32_t *pSetCount,
                                         uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                         zet_typed_value_t *pMetricValues) override;
    ze_result_t initialize(const zet_metric_group_properties_t &sourceProperties,
                           MetricsDiscovery::IMetricSet_1_5 &metricSet,
                           MetricsDiscovery::IConcurrentGroup_1_5 &concurrentGroup,
                           const std::vector<Metric *> &groupMetrics,
                           OaMetricSourceImp &metricSource);

    bool activate() override;
    bool deactivate() override;

    bool activateMetricSet();
    bool deactivateMetricSet();

    static uint32_t getApiMask(const zet_metric_group_sampling_type_flags_t samplingType);
    zet_metric_group_handle_t getMetricGroupForSubDevice(const uint32_t subDeviceIndex) override;

    // Time based measurements.
    ze_result_t openIoStream(uint32_t &timerPeriodNs, uint32_t &oaBufferSize);
    ze_result_t waitForReports(const uint32_t timeoutMs);
    ze_result_t readIoStream(uint32_t &reportCount, uint8_t &reportData);
    ze_result_t closeIoStream();

    std::vector<zet_metric_group_handle_t> &getMetricGroups();
    ze_result_t streamerOpen(
        zet_context_handle_t hContext,
        zet_device_handle_t hDevice,
        zet_metric_streamer_desc_t *desc,
        ze_event_handle_t hNotificationEvent,
        zet_metric_streamer_handle_t *phMetricStreamer) override;

    ze_result_t metricQueryPoolCreate(
        zet_context_handle_t hContext,
        zet_device_handle_t hDevice,
        const zet_metric_query_pool_desc_t *desc,
        zet_metric_query_pool_handle_t *phMetricQueryPool) override;
    static MetricGroup *create(zet_metric_group_properties_t &properties,
                               MetricsDiscovery::IMetricSet_1_5 &metricSet,
                               MetricsDiscovery::IConcurrentGroup_1_5 &concurrentGroup,
                               const std::vector<Metric *> &metrics,
                               MetricSource &metricSource);
    static zet_metric_group_properties_t getProperties(const zet_metric_group_handle_t handle);
    uint32_t getRawReportSize();

  protected:
    void copyProperties(const zet_metric_group_properties_t &source,
                        zet_metric_group_properties_t &destination);
    void copyValue(const MetricsDiscovery::TTypedValue_1_0 &source,
                   zet_typed_value_t &destination) const;

    bool getCalculatedMetricCount(const size_t rawDataSize,
                                  uint32_t &metricValueCount);

    bool getCalculatedMetricValues(const zet_metric_group_calculation_type_t, const size_t rawDataSize, const uint8_t *pRawData,
                                   uint32_t &metricValueCount,
                                   zet_typed_value_t *pCalculatedData);

    // Cached metrics.
    std::vector<Metric *> metrics;
    zet_metric_group_properties_t properties{
        ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES,
    };
    MetricsDiscovery::IMetricSet_1_5 *pReferenceMetricSet = nullptr;
    MetricsDiscovery::IConcurrentGroup_1_5 *pReferenceConcurrentGroup = nullptr;

    std::vector<zet_metric_group_handle_t> metricGroups;

    OaMetricSourceImp *metricSource;

  private:
    ze_result_t openForDevice(Device *pDevice, zet_metric_streamer_desc_t &desc,
                              zet_metric_streamer_handle_t *phMetricStreamer);
};

struct OaMetricImp : Metric {
    ~OaMetricImp() override{};

    ze_result_t getProperties(zet_metric_properties_t *pProperties) override;

    ze_result_t initialize(const zet_metric_properties_t &sourceProperties);

    static Metric *create(zet_metric_properties_t &properties);

  protected:
    void copyProperties(const zet_metric_properties_t &source,
                        zet_metric_properties_t &destination);

    zet_metric_properties_t properties{
        ZET_STRUCTURE_TYPE_METRIC_PROPERTIES};
};

} // namespace L0
