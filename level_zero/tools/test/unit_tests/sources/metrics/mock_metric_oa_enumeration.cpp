/*
 * Copyright (C) 2020-2024 Intel Corporation
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

MockMetricsDiscoveryApi *Mock<MetricEnumeration>::globalMockApi = nullptr;

TCompletionCode MockMetricsDiscoveryApi::openAdapterGroup(IAdapterGroupLatest **group) {
    *group = Mock<MetricEnumeration>::globalMockApi->adapterGroup;
    return TCompletionCode::CC_OK;
}

void Mock<MetricEnumeration>::setMockedApi(MockMetricsDiscoveryApi *mockedApi) {

    if (mockedApi) {

        //  Mock class used to communicate with metrics library.
        metricEnumeration = &metricSource.getMetricEnumeration();
        auto &actualMetricEnumeration = metricSource.getMetricEnumerationObject();
        actualMetricEnumeration.release();
        actualMetricEnumeration.reset(this);

        // Mock metrics library api functions.
        openAdapterGroup = mockedApi->openAdapterGroup;

        // Mock metrics library api.
        Mock<MetricEnumeration>::globalMockApi = mockedApi;

    } else {

        // Restore an original class used to communicate with metrics library.
        auto &actualMetricEnumeration = metricSource.getMetricEnumerationObject();
        actualMetricEnumeration.release();
        actualMetricEnumeration.reset(metricEnumeration);
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

IEquation_1_0::~IEquation_1_0() {}
uint32_t IEquation_1_0::GetEquationElementsCount(void) {
    UNRECOVERABLE_IF(true);
    return 0;
}

TEquationElement_1_0 *IEquation_1_0::GetEquationElement(uint32_t index) {
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

IAdapter_1_10 *IAdapterGroup_1_10::GetAdapter(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IAdapter_1_11 *IAdapterGroup_1_11::GetAdapter(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IAdapter_1_13 *IAdapterGroup_1_13::GetAdapter(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TConcurrentGroupParams_1_13 *IConcurrentGroup_1_13::GetParams(void) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricSet_1_13 *IConcurrentGroup_1_13::GetMetricSet(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricEnumerator_1_13 *IConcurrentGroup_1_13::GetMetricEnumerator(void) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}
IMetricEnumerator_1_13 *IConcurrentGroup_1_13::GetMetricEnumeratorFromFile(const char *fileName) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}
IMetricSet_1_13 *IConcurrentGroup_1_13::AddMetricSet(const char *symbolName, const char *shortName) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}
TCompletionCode IConcurrentGroup_1_13::RemoveMetricSet(IMetricSet_1_13 *metricSet) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

IMetricSet_1_11 *IConcurrentGroup_1_11::GetMetricSet(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricEnumerator_1_13::~IMetricEnumerator_1_13() {}
uint32_t IMetricEnumerator_1_13::GetMetricPrototypeCount(void) {
    UNRECOVERABLE_IF(true);
    return 0;
}

IMetricPrototype_1_13 *IMetricEnumerator_1_13::GetMetricPrototype(const uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IMetricEnumerator_1_13::GetMetricPrototypes(const uint32_t index, uint32_t *count, IMetricPrototype_1_13 **metrics) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IMetricEnumerator_1_13::RemoveClonedMetricPrototype(IMetricPrototype_1_13 *clonedPrototype) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

IMetricPrototype_1_13::~IMetricPrototype_1_13() {}

const TMetricPrototypeParams_1_13 *IMetricPrototype_1_13::GetParams(void) const {
    UNRECOVERABLE_IF(true);
    return nullptr;
}
IMetricPrototype_1_13 *IMetricPrototype_1_13::Clone(void) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}
const TMetricPrototypeOptionDescriptor_1_13 *IMetricPrototype_1_13::GetOptionDescriptor(uint32_t index) const {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IMetricPrototype_1_13::SetOption(const TOptionDescriptorType optionType, const TTypedValue_1_0 *typedValue) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}
TCompletionCode IMetricPrototype_1_13::ChangeNames(const char *symbolName, const char *shortName, const char *longName, const char *resultUnits) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

IConcurrentGroup_1_11 *IMetricsDevice_1_11::GetConcurrentGroup(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IMetricsDevice_1_10::GetGpuCpuTimestamps(uint64_t *gpuTimestampNs, uint64_t *cpuTimestampNs, uint32_t *cpuId, uint64_t *correlationIndicatorNs) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

IConcurrentGroup_1_13 *IMetricsDevice_1_13::GetConcurrentGroup(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IAdapter_1_10::OpenMetricsDevice(IMetricsDevice_1_10 **metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}
TCompletionCode IAdapter_1_10::OpenMetricsDeviceFromFile(const char *fileName, void *openParams, IMetricsDevice_1_10 **metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}
TCompletionCode IAdapter_1_10::OpenMetricsSubDevice(const uint32_t subDeviceIndex, IMetricsDevice_1_10 **metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}
TCompletionCode IAdapter_1_10::OpenMetricsSubDeviceFromFile(const uint32_t subDeviceIndex, const char *fileName, void *openParams, IMetricsDevice_1_10 **metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IAdapter_1_11::OpenMetricsDevice(IMetricsDevice_1_11 **metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}
TCompletionCode IAdapter_1_11::OpenMetricsDeviceFromFile(const char *fileName, void *openParams, IMetricsDevice_1_11 **metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}
TCompletionCode IAdapter_1_11::OpenMetricsSubDevice(const uint32_t subDeviceIndex, IMetricsDevice_1_11 **metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}
TCompletionCode IAdapter_1_11::OpenMetricsSubDeviceFromFile(const uint32_t subDeviceIndex, const char *fileName, void *openParams, IMetricsDevice_1_11 **metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}
TCompletionCode IAdapter_1_11::SaveMetricsDeviceToFile(const char *fileName, void *saveParams, IMetricsDevice_1_11 *metricsDevice, const uint32_t minMajorApiVersion, const uint32_t minMinorApiVersion) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IAdapter_1_13::OpenMetricsDevice(IMetricsDevice_1_13 **metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IAdapter_1_13::OpenMetricsDeviceFromFile(const char *fileName, void *openParams, IMetricsDevice_1_13 **metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}
TCompletionCode IAdapter_1_13::OpenMetricsSubDevice(const uint32_t subDeviceIndex, IMetricsDevice_1_13 **metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}
TCompletionCode IAdapter_1_13::OpenMetricsSubDeviceFromFile(const uint32_t subDeviceIndex, const char *fileName, void *openParams, IMetricsDevice_1_13 **metricsDevice) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

const TEngineParams_1_13 *IAdapter_1_13::GetEngineParams(const uint32_t subDeviceIndex, const uint32_t engineIndex) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricSet_1_11::~IMetricSet_1_11() {
}
TMetricSetParams_1_11 *IMetricSet_1_11::GetParams(void) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricSet_1_13::~IMetricSet_1_13() {
}
TCompletionCode IMetricSet_1_13::Open() {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}
TCompletionCode IMetricSet_1_13::AddMetric(IMetricPrototype_1_13 *metricPrototype) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}
TCompletionCode IMetricSet_1_13::RemoveMetric(IMetricPrototype_1_13 *metricPrototype) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}
TCompletionCode IMetricSet_1_13::Finalize() {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

IMetric_1_13 *IMetricSet_1_13::AddCustomMetric(
    const char *symbolName,
    const char *shortName,
    const char *groupName,
    const char *longName,
    const char *dxToOglAlias,
    uint32_t usageFlagsMask,
    uint32_t apiMask,
    TMetricResultType resultType,
    const char *resultUnits,
    TMetricType metricType,
    int64_t loWatermark,
    int64_t hiWatermark,
    THwUnitType hwType,
    const char *ioReadEquation,
    const char *deltaFunction,
    const char *queryReadEquation,
    const char *normalizationEquation,
    const char *maxValueEquation,
    const char *signalName,
    uint32_t queryModeMask) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetric_1_13 *IMetricSet_1_13::GetMetric(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetric_1_13::~IMetric_1_13() {}

TMetricParams_1_13 *IMetric_1_13::GetParams() {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

} // namespace MetricsDiscovery
