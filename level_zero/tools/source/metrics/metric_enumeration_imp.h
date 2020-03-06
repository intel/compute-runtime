/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/metrics/metric.h"
// Ignore function-overload warning in clang
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#elif defined(__linux__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif
#include "common/instrumentation/api/metrics_discovery_api.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__linux__)
#pragma GCC diagnostic pop
#endif

#include "shared/source/os_interface/os_library.h"

#include <vector>

namespace L0 {

///////////////////////////////////////////////////////////////////////////////
/// @brief Metric enumeration object implementation.
struct MetricEnumeration {
    MetricEnumeration(MetricContext &metricContext);
    virtual ~MetricEnumeration();

    // API (called from static API functions).
    ze_result_t metricGroupGet(uint32_t &count, zet_metric_group_handle_t *phMetricGroups);

    // Non-API.
    virtual bool isInitialized();

    // Initialization.
    virtual ze_result_t loadMetricsDiscovery();
    static const char *getMetricsDiscoveryFilename();

  protected:
    // Initialization.
    ze_result_t initialize();

    // Metrics Discovery.
    virtual ze_result_t openMetricsDiscovery();
    virtual ze_result_t cleanupMetricsDiscovery();

    // Metric caching.
    ze_result_t cacheMetricInformation();
    ze_result_t cacheMetricGroup(MetricsDiscovery::IMetricSet_1_5 &metricSet,
                                 MetricsDiscovery::IConcurrentGroup_1_5 &pConcurrentGroup,
                                 const uint32_t domain,
                                 const zet_metric_group_sampling_type_t samplingType);
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
    MetricContext &metricContext;
    std::vector<MetricGroup *> metricGroups; // Cached metric groups
    ze_result_t initializationState = ZE_RESULT_ERROR_UNINITIALIZED;

    // Metrics Discovery API.
    std::unique_ptr<NEO::OsLibrary> hMetricsDiscovery = nullptr;
    MetricsDiscovery::OpenMetricsDevice_fn openMetricsDevice = nullptr;
    MetricsDiscovery::CloseMetricsDevice_fn closeMetricsDevice = nullptr;
    MetricsDiscovery::OpenMetricsDeviceFromFile_fn openMetricsDeviceFromFile = nullptr;
    MetricsDiscovery::IMetricsDevice_1_5 *pMetricsDevice = nullptr;

    // Public due to unit tests.
  public:
    // Metrics Discovery version should be at least 1.5.
    static const uint32_t requiredMetricsDiscoveryMajorVersion = 1;
    static const uint32_t requiredMetricsDiscoveryMinorVersion = 5;
    static const char *oaConcurrentGroupName;
};

///////////////////////////////////////////////////////////////////////////////
/// @brief Metric group object implementation.
struct MetricGroupImp : MetricGroup {
    ~MetricGroupImp() override;

    // API.
    ze_result_t getProperties(zet_metric_group_properties_t *pProperties) override;
    ze_result_t getMetric(uint32_t *pCount, zet_metric_handle_t *phMetrics) override;
    ze_result_t calculateMetricValues(size_t rawDataSize, const uint8_t *pRawData,
                                      uint32_t *pMetricValueCount,
                                      zet_typed_value_t *pCalculatedData) override;

    // Non-API.
    ze_result_t initialize(const zet_metric_group_properties_t &sourceProperties,
                           MetricsDiscovery::IMetricSet_1_5 &metricSet,
                           MetricsDiscovery::IConcurrentGroup_1_5 &concurrentGroup,
                           const std::vector<Metric *> &groupMetrics);

    uint32_t getRawReportSize() override;

    // Activation.
    bool activate() override;
    bool deactivate() override;

    static uint32_t getApiMask(const zet_metric_group_sampling_type_t samplingType);

    // Time based measurements.
    ze_result_t openIoStream(uint32_t &timerPeriodNs, uint32_t &oaBufferSize) override;
    ze_result_t waitForReports(const uint32_t timeoutMs) override;
    ze_result_t readIoStream(uint32_t &reportCount, uint8_t &reportData) override;
    ze_result_t closeIoStream() override;

  protected:
    void copyProperties(const zet_metric_group_properties_t &source,
                        zet_metric_group_properties_t &destination);

    void copyValue(const MetricsDiscovery::TTypedValue_1_0 &source,
                   zet_typed_value_t &destination) const;

    bool getCalculatedMetricCount(const size_t rawDataSize,
                                  uint32_t &metricValueCount);

    bool getCalculatedMetricValues(const size_t rawDataSize, const uint8_t *pRawData,
                                   uint32_t &metricValueCount,
                                   zet_typed_value_t *pCalculatedData);

    // Cached metrics.
    std::vector<Metric *> metrics;
    zet_metric_group_properties_t properties{
        ZET_METRIC_GROUP_PROPERTIES_VERSION_CURRENT,
    };
    MetricsDiscovery::IMetricSet_1_5 *pReferenceMetricSet = nullptr;
    MetricsDiscovery::IConcurrentGroup_1_5 *pReferenceConcurrentGroup = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
/// @brief Metric object implementation.
struct MetricImp : Metric {
    ~MetricImp() override{};

    // API.
    ze_result_t getProperties(zet_metric_properties_t *pProperties) override;

    // Non-API.
    ze_result_t initialize(const zet_metric_properties_t &sourceProperties);

  protected:
    void copyProperties(const zet_metric_properties_t &source,
                        zet_metric_properties_t &destination);

    zet_metric_properties_t properties{
        ZET_METRIC_PROPERTIES_VERSION_CURRENT,
    };
};

} // namespace L0
