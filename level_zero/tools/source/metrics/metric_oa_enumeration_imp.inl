/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"

namespace L0 {

template <typename MdapiEntryType>
Metric *OaMetricImp::create(MetricSource &metricSource,
                            MdapiEntryType *mdapiObject,
                            zet_metric_properties_t &properties,
                            bool isPredefined) {
    std::vector<MetricScopeImp *> metricScopes{};
    auto pMetric = new OaMetricImp(metricSource, metricScopes);
    UNRECOVERABLE_IF(pMetric == nullptr);
    pMetric->initialize(properties);
    pMetric->isPredefined = isPredefined;
    if constexpr (std::is_same_v<MdapiEntryType, MetricsDiscovery::IMetric_1_0>) {
        pMetric->mdapiMetric = mdapiObject;
    } else if constexpr (std::is_same_v<MdapiEntryType, MetricsDiscovery::IInformation_1_0>) {
        pMetric->mdapiInformation = mdapiObject;
    }
    DEBUG_BREAK_IF(pMetric->mdapiMetric == nullptr && pMetric->mdapiInformation == nullptr);

    return pMetric;
}

} // namespace L0
