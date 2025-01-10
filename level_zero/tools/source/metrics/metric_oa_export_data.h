/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/zet_intel_gpu_metric_export.h"
#include <level_zero/zet_api.h>

#include "metrics_discovery_api.h"

#include <utility>

namespace L0 {
class HeapUsageTracker {

  public:
    enum OperationMode {
        OperationModeTrackOnly = 0u,
        OperationModeTrackAndAllocate
    };

    HeapUsageTracker() = delete;
    HeapUsageTracker(uintptr_t startAddress,
                     uintptr_t endAddress,
                     OperationMode mode = OperationModeTrackAndAllocate);

    template <typename T>
    std::pair<T *, ptrdiff_t> allocate(uint64_t count);
    size_t getUsedBytes();
    OperationMode getOperationMode() const { return operationMode; }

  protected:
    uintptr_t currentAddress = reinterpret_cast<uintptr_t>(nullptr);
    const uintptr_t endAddress;
    ptrdiff_t currentOffset = 0;
    const OperationMode operationMode;
};

class MetricOaExporter01 {
  public:
    MetricOaExporter01() = delete;
    MetricOaExporter01(MetricsDiscovery::IMetricsDevice_1_5 &mdDevice,
                       MetricsDiscovery::IAdapter_1_9 &mdAdapter,
                       MetricsDiscovery::IMetricSet_1_5 &mdMetricSet,
                       MetricsDiscovery::IConcurrentGroup_1_5 &mdConcurrentGroup,
                       HeapUsageTracker &heapUsageTracker);
    ze_result_t getExportData(zet_intel_metric_df_gpu_metric_oa_calc_0_1_t *oaCalcData);

  protected:
    enum OperationMode : uint32_t {
        OperationModeGetHeapSize = 0u,
        OperationModeGetExportData
    };

    void updateMetricApiVersion(zet_intel_metric_df_gpu_apiversion_0_1_t *apiVersion);
    void assignCstringOffset(cstring_offset_t *cStringOffset, const char *stringValue);
    void assignByteArray(zet_intel_metric_df_gpu_byte_array_0_1_t *byteArray, MetricsDiscovery::TByteArray_1_0 *mdByteArray);
    ze_result_t assignTypedValue(zet_intel_metric_df_gpu_typed_value_0_1_t *typedValue, MetricsDiscovery::TTypedValue_1_0 &mdTypedValue);
    ze_result_t assignAdapterId(zet_intel_metric_df_gpu_adapter_id_0_1_t *adapterId, MetricsDiscovery::TAdapterId_1_6 *mAdapterId);
    ze_result_t getEquationOperation(zet_intel_metric_df_gpu_equation_operation_t &equationOperation,
                                     const MetricsDiscovery::TEquationOperation mdEquationOperation);
    ze_result_t assignEquation(zet_intel_metric_df_gpu_equation_0_1_t &equation, MetricsDiscovery::IEquation_1_0 *mdEquation);
    ze_result_t getDeltaFunction(zet_intel_metric_df_gpu_delta_function_0_1_t &deltaFunction, MetricsDiscovery::TDeltaFunction_1_0 &mdDeltaFunction);
    ze_result_t getInformationType(zet_intel_metric_df_gpu_information_type_t &infoType, const MetricsDiscovery::TInformationType mdInfoType);
    ze_result_t assignInformationParams(zet_intel_metric_df_gpu_information_params_0_1_t *infoParams, MetricsDiscovery::TInformationParams_1_0 *mdInfoParams);
    ze_result_t assignIoMeaurementInformationOffset(zet_intel_metric_df_gpu_information_params_0_1_offset_t *ioMeasurementInformationOffset);
    ze_result_t assignIoGpuContextInformationOffset(zet_intel_metric_df_gpu_information_params_0_1_offset_t *ioGpuContextInformationOffset);
    ze_result_t assignMetricSetInformationOffset(zet_intel_metric_df_gpu_information_params_0_1_offset_t *metricSetInfoParamsOffset);
    ze_result_t getMetricType(zet_intel_metric_df_gpu_metric_type_t &metricType, const MetricsDiscovery::TMetricType mdMetricType);
    ze_result_t assignMetricParamsOffset(zet_intel_metric_df_gpu_metric_params_0_1_offset_t *metricParamsOffset);
    void updateConcurrentGroupParams(zet_intel_metric_df_gpu_concurrent_group_params_0_1_t *concGroupParams);
    ze_result_t updateConcurrentGroup(zet_intel_metric_df_gpu_concurrent_group_0_1_t *concGroup);
    void updateMetricSetParams(zet_intel_metric_df_gpu_metric_set_params_0_1_t *params);
    ze_result_t updateMetricSet(zet_intel_metric_df_gpu_metric_set_0_1_t *metricSet);
    ze_result_t updateAdapterParams(zet_intel_metric_df_gpu_adapter_params_0_1_t *adapterParams);
    ze_result_t updateGlobalSymbolOffsetAndValues(zet_intel_metric_df_gpu_global_symbol_0_1_offset_t *globalSymbolsOffset);
    ze_result_t getMetricResultType(zet_intel_metric_df_gpu_metric_result_type_t &resltType, const MetricsDiscovery::TMetricResultType mdResultType);
    void updateMetricsDeviceParams(zet_intel_metric_df_gpu_metrics_device_params_0_1_t *deviceParams);
    template <typename T>
    void assignUnaligned(T *unAlignedData, T *alignedData);

    MetricsDiscovery::IMetricsDevice_1_5 &mdDevice;
    MetricsDiscovery::IAdapter_1_9 &adapter;
    MetricsDiscovery::IMetricSet_1_5 &mdMetricSet;
    MetricsDiscovery::IConcurrentGroup_1_5 &concurrentGroup;
    HeapUsageTracker &heapUsageTracker;
    OperationMode currOperationMode = OperationModeGetExportData;
};

} // namespace L0
