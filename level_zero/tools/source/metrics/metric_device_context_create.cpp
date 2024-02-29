/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
namespace L0 {

std::unique_ptr<MetricDeviceContext> MetricDeviceContext::create(Device &device) {
    return std::make_unique<MetricDeviceContext>(device);
}

ze_result_t MetricDeviceContext::metricProgrammableGet(
    uint32_t *pCount, zet_metric_programmable_exp_handle_t *phMetricProgrammables) {

    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t MetricDeviceContext::metricGroupCreate(const char name[ZET_MAX_METRIC_GROUP_NAME],
                                                   const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                                                   zet_metric_group_sampling_type_flag_t samplingType,
                                                   zet_metric_group_handle_t *pMetricGroupHandle) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t OaMetricSourceImp::metricProgrammableGet(uint32_t *pCount, zet_metric_programmable_exp_handle_t *phMetricProgrammables) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
ze_result_t OaMetricSourceImp::metricGroupCreate(const char name[ZET_MAX_METRIC_GROUP_NAME],
                                                 const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                                                 zet_metric_group_sampling_type_flag_t samplingType,
                                                 zet_metric_group_handle_t *pMetricGroupHandle) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t metricProgrammableGetProperties(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    zet_metric_programmable_exp_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t metricProgrammableGetParamInfo(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    uint32_t *pParameterCount,
    zet_metric_programmable_param_info_exp_t *pParameterInfo) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t metricProgrammableGetParamValueInfo(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    uint32_t parameterOrdinal,
    uint32_t *pValueInfoCount,
    zet_metric_programmable_param_value_info_exp_t *pValueInfo) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t metricCreateFromProgrammable(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    zet_metric_programmable_param_value_exp_t *pParameterValues,
    uint32_t parameterCount,
    const char name[ZET_MAX_METRIC_NAME],
    const char description[ZET_MAX_METRIC_DESCRIPTION],
    uint32_t *pMetricHandleCount,
    zet_metric_handle_t *phMetricHandles) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0