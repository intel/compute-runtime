/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"

namespace L0 {
namespace ult {

Mock<MetricEnumeration>::Mock(::L0::OaMetricSourceImp &metricSource) : MetricEnumeration(metricSource) {
}

Mock<MetricEnumeration>::~Mock() {
}

MockMetricsDiscoveryApi *Mock<MetricEnumeration>::g_mockApi = nullptr;

TCompletionCode MockMetricsDiscoveryApi::OpenAdapterGroup(IAdapterGroupLatest **group) {
    return Mock<MetricEnumeration>::g_mockApi->MockOpenAdapterGroup((IAdapterGroup_1_9 **)group);
}

TCompletionCode MockMetricsDiscoveryApi::OpenMetricsDeviceFromFile(const char *fileName, void *openParams, IMetricsDeviceLatest **device) {
    return Mock<MetricEnumeration>::g_mockApi->MockOpenMetricsDeviceFromFile(fileName, openParams, (IMetricsDevice_1_5 **)device);
}

TCompletionCode MockMetricsDiscoveryApi::CloseMetricsDevice(IMetricsDeviceLatest *device) {
    return Mock<MetricEnumeration>::g_mockApi->MockCloseMetricsDevice((IMetricsDevice_1_5 *)device);
}

TCompletionCode MockMetricsDiscoveryApi::SaveMetricsDeviceToFile(const char *fileName, void *saveParams, IMetricsDeviceLatest *device) {
    return Mock<MetricEnumeration>::g_mockApi->MockSaveMetricsDeviceToFile(fileName, saveParams, (IMetricsDevice_1_5 *)device);
}

void Mock<MetricEnumeration>::setMockedApi(MockMetricsDiscoveryApi *mockedApi) {

    if (mockedApi) {

        //  Mock class used to communicate with metrics library.
        metricEnumeration = &metricSource.getMetricEnumeration();
        metricSource.setMetricEnumeration(*this);

        // Mock metrics library api functions.
        openAdapterGroup = mockedApi->OpenAdapterGroup;

        // Mock metrics library api.
        Mock<MetricEnumeration>::g_mockApi = mockedApi;

    } else {

        // Restore an original class used to communicate with metrics library.
        metricSource.setMetricEnumeration(*metricEnumeration);
    }
}

} // namespace ult
} // namespace L0

namespace MetricsDiscovery {

IMetricsDevice_1_0::~IMetricsDevice_1_0() {}

TMetricsDeviceParams_1_0 *IMetricsDevice_1_0::GetParams(void) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IConcurrentGroup_1_0 *IMetricsDevice_1_0::GetConcurrentGroup(uint32_t) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TGlobalSymbol_1_0 *IMetricsDevice_1_0::GetGlobalSymbol(uint32_t) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TTypedValue_1_0 *IMetricsDevice_1_0::GetGlobalSymbolValueByName(const char *name) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IMetricsDevice_1_0::GetLastError(void) {
    UNRECOVERABLE_IF(true);
    return TCompletionCode::CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IMetricsDevice_1_0::GetGpuCpuTimestamps(uint64_t *, uint64_t *,
                                                        uint32_t *) {
    UNRECOVERABLE_IF(true);
    return TCompletionCode::CC_ERROR_NOT_SUPPORTED;
}

IConcurrentGroup_1_1 *IMetricsDevice_1_1::GetConcurrentGroup(uint32_t) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TMetricsDeviceParams_1_2 *MetricsDiscovery::IMetricsDevice_1_2::GetParams(void) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IOverride_1_2 *IMetricsDevice_1_2::GetOverride(unsigned int) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IOverride_1_2 *IMetricsDevice_1_2::GetOverrideByName(char const *) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IConcurrentGroup_1_5 *IMetricsDevice_1_5::GetConcurrentGroup(uint32_t) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IConcurrentGroup_1_0::~IConcurrentGroup_1_0() {}

TConcurrentGroupParams_1_0 *IConcurrentGroup_1_0::GetParams(void) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricSet_1_0 *IConcurrentGroup_1_0::GetMetricSet(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IConcurrentGroup_1_0::OpenIoStream(IMetricSet_1_0 *metricSet, uint32_t processId, uint32_t *nsTimerPeriod, uint32_t *oaBufferSize) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IConcurrentGroup_1_0::ReadIoStream(uint32_t *reportsCount, char *reportData, uint32_t readFlags) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IConcurrentGroup_1_0::CloseIoStream(void) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IConcurrentGroup_1_0::WaitForReports(uint32_t milliseconds) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

IInformation_1_0 *IConcurrentGroup_1_0::GetIoMeasurementInformation(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IInformation_1_0 *IConcurrentGroup_1_0::GetIoGpuContextInformation(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricSet_1_1 *IConcurrentGroup_1_1::GetMetricSet(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IConcurrentGroup_1_3::SetIoStreamSamplingType(TSamplingType type) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

IMetricSet_1_5 *IConcurrentGroup_1_5::GetMetricSet(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricSet_1_0::~IMetricSet_1_0() {}

TMetricSetParams_1_0 *IMetricSet_1_0::GetParams(void) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetric_1_0 *IMetricSet_1_0::GetMetric(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IInformation_1_0 *IMetricSet_1_0::GetInformation(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricSet_1_0 *IMetricSet_1_0::GetComplementaryMetricSet(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IMetricSet_1_0::Activate(void) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IMetricSet_1_0::Deactivate(void) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

IMetric_1_0 *IMetricSet_1_0::AddCustomMetric(
    const char *symbolName, const char *shortName, const char *groupName, const char *longName, const char *dxToOglAlias,
    uint32_t usageFlagsMask, uint32_t apiMask, TMetricResultType resultType, const char *resultUnits, TMetricType metricType,
    int64_t loWatermark, int64_t hiWatermark, THwUnitType hwType, const char *ioReadEquation, const char *deltaFunction,
    const char *queryReadEquation, const char *normalizationEquation, const char *maxValueEquation, const char *signalName) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricSet_1_1 ::~IMetricSet_1_1() {}

TCompletionCode IMetricSet_1_1::SetApiFiltering(uint32_t apiMask) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IMetricSet_1_1::CalculateMetrics(const unsigned char *rawData, uint32_t rawDataSize, TTypedValue_1_0 *out,
                                                 uint32_t outSize, uint32_t *outReportCount, bool enableContextFiltering) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IMetricSet_1_1::CalculateIoMeasurementInformation(TTypedValue_1_0 *out, uint32_t outSize) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

IMetricSet_1_4 ::~IMetricSet_1_4() {}

TMetricSetParams_1_4 *IMetricSet_1_4::GetParams(void) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricSet_1_5 *IMetricSet_1_5::GetComplementaryMetricSet(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IMetricSet_1_5::CalculateMetrics(const unsigned char *rawData, uint32_t rawDataSize, TTypedValue_1_0 *out,
                                                 uint32_t outSize, uint32_t *outReportCount, TTypedValue_1_0 *outMaxValues, uint32_t outMaxValuesSize) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

IMetric_1_0 ::~IMetric_1_0() {}

TMetricParams_1_0 *IMetric_1_0::GetParams() {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IInformation_1_0 ::~IInformation_1_0() {}

TInformationParams_1_0 *IInformation_1_0::GetParams() {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IAdapter_1_8 *IAdapterGroup_1_8::GetAdapter(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IAdapter_1_6 *IAdapterGroup_1_6::GetAdapter(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IAdapterGroup_1_6 ::~IAdapterGroup_1_6() {
}

const TAdapterGroupParams_1_6 *IAdapterGroup_1_6::GetParams(void) const {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IAdapter_1_9 *IAdapterGroup_1_9::GetAdapter(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IAdapterGroup_1_6::Close() {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

IAdapter_1_6 ::~IAdapter_1_6() {}

const TAdapterParams_1_6 *IAdapter_1_6 ::GetParams(void) const {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

const TAdapterParams_1_8 *IAdapter_1_8::GetParams(void) const {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

const TAdapterParams_1_9 *IAdapter_1_9::GetParams(void) const {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IAdapter_1_6 ::Reset() {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IAdapter_1_6 ::OpenMetricsDevice(IMetricsDevice_1_5 **metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IAdapter_1_6 ::OpenMetricsDeviceFromFile(const char *fileName, void *openParams, IMetricsDevice_1_5 **metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IAdapter_1_6 ::CloseMetricsDevice(IMetricsDevice_1_5 *metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IAdapter_1_6 ::SaveMetricsDeviceToFile(const char *fileName, void *saveParams, IMetricsDevice_1_5 *metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

const TSubDeviceParams_1_9 *IAdapter_1_9::GetSubDeviceParams(const uint32_t subDeviceIndex) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

const TEngineParams_1_9 *IAdapter_1_9::GetEngineParams(const uint32_t subDeviceIndex, const uint32_t engineIndex) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IAdapter_1_9::OpenMetricsSubDevice(const uint32_t subDeviceIndex, IMetricsDevice_1_5 **metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IAdapter_1_9::OpenMetricsSubDeviceFromFile(const uint32_t subDeviceIndex, const char *fileName, void *openParams, IMetricsDevice_1_5 **metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

} // namespace MetricsDiscovery
