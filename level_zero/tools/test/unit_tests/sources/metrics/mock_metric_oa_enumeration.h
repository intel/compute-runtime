/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_query_imp.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::MetricGroup> : public ::L0::MetricGroup {
    using BaseClass = ::L0::MetricGroup;
};

using MetricGroup = WhiteBox<::L0::MetricGroup>;

using MetricsDiscovery::IAdapter_1_6;
using MetricsDiscovery::IAdapter_1_8;
using MetricsDiscovery::IAdapter_1_9;
using MetricsDiscovery::IAdapterGroup_1_6;
using MetricsDiscovery::IAdapterGroup_1_8;
using MetricsDiscovery::IAdapterGroup_1_9;
using MetricsDiscovery::IAdapterGroupLatest;
using MetricsDiscovery::IConcurrentGroup_1_5;
using MetricsDiscovery::IInformation_1_0;
using MetricsDiscovery::IMetric_1_0;
using MetricsDiscovery::IMetricsDevice_1_5;
using MetricsDiscovery::IMetricsDeviceLatest;
using MetricsDiscovery::IMetricSet_1_0;
using MetricsDiscovery::IMetricSet_1_5;
using MetricsDiscovery::IOverride_1_2;
using MetricsDiscovery::TAdapterGroupParams_1_6;
using MetricsDiscovery::TAdapterParams_1_6;
using MetricsDiscovery::TAdapterParams_1_8;
using MetricsDiscovery::TAdapterParams_1_9;
using MetricsDiscovery::TCompletionCode;
using MetricsDiscovery::TConcurrentGroupParams_1_0;
using MetricsDiscovery::TEngineParams_1_9;
using MetricsDiscovery::TGlobalSymbol_1_0;
using MetricsDiscovery::TMetricParams_1_0;
using MetricsDiscovery::TMetricsDeviceParams_1_2;
using MetricsDiscovery::TMetricSetParams_1_4;
using MetricsDiscovery::TSamplingType;
using MetricsDiscovery::TSubDeviceParams_1_9;
using MetricsDiscovery::TTypedValue_1_0;

struct MockMetricsDiscoveryApi {

    // Original api functions.
    static TCompletionCode MD_STDCALL OpenMetricsDeviceFromFile(const char *fileName, void *openParams, IMetricsDeviceLatest **device);
    static TCompletionCode MD_STDCALL CloseMetricsDevice(IMetricsDeviceLatest *device);
    static TCompletionCode MD_STDCALL SaveMetricsDeviceToFile(const char *fileName, void *saveParams, IMetricsDeviceLatest *device);
    static TCompletionCode MD_STDCALL OpenAdapterGroup(IAdapterGroupLatest **adapterGroup);

    // Mocked api functions.
    MOCK_METHOD(TCompletionCode, MockOpenMetricsDeviceFromFile, (const char *, void *, IMetricsDevice_1_5 **));
    MOCK_METHOD(TCompletionCode, MockCloseMetricsDevice, (IMetricsDevice_1_5 *));
    MOCK_METHOD(TCompletionCode, MockSaveMetricsDeviceToFile, (const char *, void *, IMetricsDevice_1_5 *));
    MOCK_METHOD(TCompletionCode, MockOpenAdapterGroup, (IAdapterGroup_1_9 **));
};

template <>
class Mock<IAdapterGroup_1_9> : public IAdapterGroup_1_9 {
  public:
    Mock(){};

    MOCK_METHOD(IAdapter_1_9 *, GetAdapter, (uint32_t), (override));
    MOCK_METHOD(const TAdapterGroupParams_1_6 *, GetParams, (), (const, override));
    MOCK_METHOD(TCompletionCode, Close, (), (override));
};

template <>
class Mock<IAdapter_1_9> : public IAdapter_1_9 {
  public:
    Mock(){};

    // 1.9
    MOCK_METHOD(const TSubDeviceParams_1_9 *, GetSubDeviceParams, (const uint32_t subDeviceIndex), (override));
    MOCK_METHOD(const TEngineParams_1_9 *, GetEngineParams, (const uint32_t subDeviceIndex, const uint32_t engineIndex), (override));
    MOCK_METHOD(TCompletionCode, OpenMetricsSubDevice, (const uint32_t subDeviceIndex, IMetricsDevice_1_5 **metricsDevice), (override));
    MOCK_METHOD(TCompletionCode, OpenMetricsSubDeviceFromFile, (const uint32_t subDeviceIndex, const char *fileName, void *openParams, IMetricsDevice_1_5 **metricsDevice), (override));

    MOCK_METHOD(const TAdapterParams_1_9 *, GetParams, (), (const, override));
    MOCK_METHOD(TCompletionCode, Reset, (), (override));
    MOCK_METHOD(TCompletionCode, OpenMetricsDevice, (IMetricsDevice_1_5 **), (override));
    MOCK_METHOD(TCompletionCode, OpenMetricsDeviceFromFile, (const char *, void *, IMetricsDevice_1_5 **), (override));
    MOCK_METHOD(TCompletionCode, CloseMetricsDevice, (IMetricsDevice_1_5 *), (override));
    MOCK_METHOD(TCompletionCode, SaveMetricsDeviceToFile, (const char *, void *, IMetricsDevice_1_5 *), (override));
};

template <>
class Mock<IMetricsDevice_1_5> : public IMetricsDevice_1_5 {
  public:
    Mock(){};

    MOCK_METHOD(TMetricsDeviceParams_1_2 *, GetParams, (), (override));
    MOCK_METHOD(IOverride_1_2 *, GetOverride, (uint32_t index), (override));
    MOCK_METHOD(IOverride_1_2 *, GetOverrideByName, (const char *symbolName), (override));
    MOCK_METHOD(IConcurrentGroup_1_5 *, GetConcurrentGroup, (uint32_t index), (override));
    MOCK_METHOD(TGlobalSymbol_1_0 *, GetGlobalSymbol, (uint32_t index), (override));
    MOCK_METHOD(TTypedValue_1_0 *, GetGlobalSymbolValueByName, (const char *name), (override));
    MOCK_METHOD(TCompletionCode, GetLastError, (), (override));
    MOCK_METHOD(TCompletionCode, GetGpuCpuTimestamps, (uint64_t * gpuTimestampNs, uint64_t *cpuTimestampNs, uint32_t *cpuId), (override));
};

template <>
class Mock<IConcurrentGroup_1_5> : public IConcurrentGroup_1_5 {
  public:
    Mock(){};

    MOCK_METHOD(IMetricSet_1_5 *, GetMetricSet, (uint32_t index), (override));
    MOCK_METHOD(TConcurrentGroupParams_1_0 *, GetParams, (), (override));
    MOCK_METHOD(TCompletionCode, OpenIoStream, (IMetricSet_1_0 * metricSet, uint32_t processId, uint32_t *nsTimerPeriod, uint32_t *oaBufferSize), (override));
    MOCK_METHOD(TCompletionCode, ReadIoStream, (uint32_t * reportsCount, char *reportData, uint32_t readFlags), (override));
    MOCK_METHOD(TCompletionCode, CloseIoStream, (), (override));
    MOCK_METHOD(TCompletionCode, WaitForReports, (uint32_t milliseconds), (override));
    MOCK_METHOD(TCompletionCode, SetIoStreamSamplingType, (TSamplingType type), (override));
    MOCK_METHOD(IInformation_1_0 *, GetIoMeasurementInformation, (uint32_t index), (override));
    MOCK_METHOD(IInformation_1_0 *, GetIoGpuContextInformation, (uint32_t index), (override));
};

template <>
class Mock<IMetricSet_1_5> : public IMetricSet_1_5 {
  public:
    Mock(){};

    MOCK_METHOD(TMetricSetParams_1_4 *, GetParams, (), (override));
    MOCK_METHOD(IMetric_1_0 *, GetMetric, (uint32_t index), (override));
    MOCK_METHOD(IInformation_1_0 *, GetInformation, (uint32_t index), (override));
    MOCK_METHOD(TCompletionCode, Activate, (), (override));
    MOCK_METHOD(TCompletionCode, Deactivate, (), (override));
    MOCK_METHOD(TCompletionCode, SetApiFiltering, (uint32_t apiMask), (override));
    MOCK_METHOD(TCompletionCode, CalculateMetrics, (const unsigned char *rawData, uint32_t rawDataSize, TTypedValue_1_0 *out, uint32_t outSize, uint32_t *outReportCount, bool enableContextFiltering), (override));
    MOCK_METHOD(TCompletionCode, CalculateIoMeasurementInformation, (TTypedValue_1_0 * out, uint32_t outSize), (override));
    MOCK_METHOD(IMetricSet_1_5 *, GetComplementaryMetricSet, (uint32_t index), (override));
    MOCK_METHOD(TCompletionCode, CalculateMetrics, (const unsigned char *rawData, uint32_t rawDataSize, TTypedValue_1_0 *out, uint32_t outSize, uint32_t *outReportCount, TTypedValue_1_0 *outMaxValues, uint32_t outMaxValuesSize), (override));
};

template <>
class Mock<IMetric_1_0> : public IMetric_1_0 {
  public:
    Mock(){};

    MOCK_METHOD(TMetricParams_1_0 *, GetParams, (), (override));
};

template <>
class Mock<IInformation_1_0> : public IInformation_1_0 {
  public:
    Mock(){};

    MOCK_METHOD(MetricsDiscovery::TInformationParams_1_0 *, GetParams, (), (override));
};

template <>
struct Mock<MetricEnumeration> : public MetricEnumeration {
    Mock(::L0::OaMetricSourceImp &metricSource);
    ~Mock() override;

    using MetricEnumeration::cleanupMetricsDiscovery;
    using MetricEnumeration::hMetricsDiscovery;
    using MetricEnumeration::initializationState;
    using MetricEnumeration::openAdapterGroup;
    using MetricEnumeration::openMetricsDiscovery;

    // Api mock enable/disable.
    void setMockedApi(MockMetricsDiscoveryApi *mockedApi);

    // Mock metric enumeration functions.
    MOCK_METHOD(bool, isInitialized, (), (override));
    MOCK_METHOD(ze_result_t, loadMetricsDiscovery, (), (override));
    MOCK_METHOD(MetricsDiscovery::IAdapter_1_9 *, getMetricsAdapter, (), (override));
    MOCK_METHOD(bool, getAdapterId, (uint32_t & drmMajor, uint32_t &drmMinor), (override));

    // Not mocked metrics enumeration functions.
    bool baseIsInitialized() { return MetricEnumeration::isInitialized(); }
    IAdapter_1_9 *baseGetMetricsAdapter() { return MetricEnumeration::getMetricsAdapter(); }
    bool baseGetAdapterId(uint32_t &adapterMajor, uint32_t &adapterMinor) { return MetricEnumeration::getAdapterId(adapterMajor, adapterMinor); }
    ze_result_t baseLoadMetricsDiscovery() { return MetricEnumeration::loadMetricsDiscovery(); }

    // Mock metrics discovery api.
    static MockMetricsDiscoveryApi *g_mockApi;

    // Original metric enumeration obtained from metric context.
    ::L0::MetricEnumeration *metricEnumeration = nullptr;
};

template <>
struct Mock<MetricGroup> : public OaMetricGroupImp {
    Mock() {}

    MOCK_METHOD(ze_result_t, metricGet, (uint32_t *, zet_metric_handle_t *), (override));
    MOCK_METHOD(ze_result_t, calculateMetricValues, (const zet_metric_group_calculation_type_t, size_t, const uint8_t *, uint32_t *, zet_typed_value_t *), (override));
    MOCK_METHOD(ze_result_t, calculateMetricValuesExp, (const zet_metric_group_calculation_type_t, size_t, const uint8_t *, uint32_t *, uint32_t *, uint32_t *, zet_typed_value_t *), (override));
    MOCK_METHOD(ze_result_t, getProperties, (zet_metric_group_properties_t * properties), (override));
    MOCK_METHOD(bool, activate, (), (override));
    MOCK_METHOD(bool, deactivate, (), (override));
    zet_metric_group_handle_t getMetricGroupForSubDevice(const uint32_t subDeviceIndex) override {
        return nullptr;
    }
    MOCK_METHOD(ze_result_t, waitForReports, (const uint32_t));
    MOCK_METHOD(ze_result_t, openIoStream, (uint32_t &, uint32_t &));
    MOCK_METHOD(ze_result_t, readIoStream, (uint32_t &, uint8_t &));
    MOCK_METHOD(ze_result_t, closeIoStream, ());
};

struct MetricGroupImpTest : public OaMetricGroupImp {
    using OaMetricGroupImp::copyValue;
    using OaMetricGroupImp::pReferenceConcurrentGroup;
    using OaMetricGroupImp::pReferenceMetricSet;
};

} // namespace ult
} // namespace L0
