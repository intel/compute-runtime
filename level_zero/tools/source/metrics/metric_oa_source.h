/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_query_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_streamer_imp.h"

namespace NEO {
class OsLibrary;
} // namespace NEO

namespace L0 {

class OaMetricSourceImp : public MetricSource {

  public:
    OaMetricSourceImp(const MetricDeviceContext &metricDeviceContext);
    virtual ~OaMetricSourceImp();
    void enable() override;
    bool isAvailable() override;
    ze_result_t metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) override;
    ze_result_t appendMetricMemoryBarrier(CommandList &commandList) override;
    bool loadDependencies();
    bool isInitialized();
    void setInitializationState(const ze_result_t state);
    Device &getDevice();
    MetricsLibrary &getMetricsLibrary();
    MetricEnumeration &getMetricEnumeration();
    MetricStreamer *getMetricStreamer();
    void setMetricStreamer(MetricStreamer *pMetricStreamer);
    void setMetricsLibrary(MetricsLibrary &metricsLibrary);
    void setMetricEnumeration(MetricEnumeration &metricEnumeration);

    ze_result_t activateMetricGroups();
    ze_result_t activateMetricGroupsDeferred(const uint32_t count,
                                             zet_metric_group_handle_t *phMetricGroups);
    bool isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) const;
    bool isMetricGroupActivated() const;
    void setUseCompute(const bool useCompute);
    bool isComputeUsed() const;
    uint32_t getSubDeviceIndex();
    bool isImplicitScalingCapable() const;
    const MetricDeviceContext &getMetricDeviceContext() const { return metricDeviceContext; }
    static bool checkDependencies();
    static std::unique_ptr<OaMetricSourceImp> create(const MetricDeviceContext &metricDeviceContext);
    using OsLibraryLoadPtr = std::add_pointer<NEO::OsLibrary *(const std::string &)>::type;
    static OsLibraryLoadPtr osLibraryLoadFunction;

  protected:
    ze_result_t initializationState = ZE_RESULT_ERROR_UNINITIALIZED;
    const MetricDeviceContext &metricDeviceContext;
    std::unique_ptr<MetricEnumeration> metricEnumeration = nullptr;
    std::unique_ptr<MetricsLibrary> metricsLibrary = nullptr;
    MetricStreamer *pMetricStreamer = nullptr;
    bool useCompute = false;
};

template <>
OaMetricSourceImp &MetricDeviceContext::getMetricSource<OaMetricSourceImp>() const;

} // namespace L0
