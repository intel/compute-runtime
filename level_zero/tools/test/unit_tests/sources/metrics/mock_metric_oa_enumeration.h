/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
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

using MetricsDiscovery::IAdapter_1_13;
using MetricsDiscovery::IAdapter_1_6;
using MetricsDiscovery::IAdapter_1_8;
using MetricsDiscovery::IAdapter_1_9;
using MetricsDiscovery::IAdapterGroup_1_13;
using MetricsDiscovery::IAdapterGroup_1_6;
using MetricsDiscovery::IAdapterGroup_1_8;
using MetricsDiscovery::IAdapterGroup_1_9;
using MetricsDiscovery::IAdapterGroupLatest;
using MetricsDiscovery::IConcurrentGroup_1_13;
using MetricsDiscovery::IConcurrentGroup_1_5;
using MetricsDiscovery::IEquation_1_0;
using MetricsDiscovery::IInformation_1_0;
using MetricsDiscovery::IMetric_1_0;
using MetricsDiscovery::IMetric_1_13;
using MetricsDiscovery::IMetricsDevice_1_11;
using MetricsDiscovery::IMetricsDevice_1_13;
using MetricsDiscovery::IMetricsDevice_1_5;
using MetricsDiscovery::IMetricsDeviceLatest;
using MetricsDiscovery::IMetricSet_1_0;
using MetricsDiscovery::IMetricSet_1_13;
using MetricsDiscovery::IMetricSet_1_5;
using MetricsDiscovery::IOverride_1_2;
using MetricsDiscovery::TAdapterGroupParams_1_6;
using MetricsDiscovery::TAdapterParams_1_6;
using MetricsDiscovery::TAdapterParams_1_8;
using MetricsDiscovery::TAdapterParams_1_9;
using MetricsDiscovery::TCompletionCode;
using MetricsDiscovery::TConcurrentGroupParams_1_0;
using MetricsDiscovery::TConcurrentGroupParams_1_13;
using MetricsDiscovery::TEngineParams_1_13;
using MetricsDiscovery::TEngineParams_1_9;
using MetricsDiscovery::TEquationElement_1_0;
using MetricsDiscovery::TGlobalSymbol_1_0;
using MetricsDiscovery::TMetricParams_1_0;
using MetricsDiscovery::TMetricParams_1_13;
using MetricsDiscovery::TMetricsDeviceParams_1_2;
using MetricsDiscovery::TMetricSetParams_1_11;
using MetricsDiscovery::TMetricSetParams_1_4;
using MetricsDiscovery::TSamplingType;
using MetricsDiscovery::TSubDeviceParams_1_9;
using MetricsDiscovery::TTypedValue_1_0;

struct MockMetricsDiscoveryApi {

    // Original api functions.
    static TCompletionCode MD_STDCALL openAdapterGroup(IAdapterGroupLatest **adapterGroup);

    IAdapterGroupLatest *adapterGroup = nullptr;
};

template <>
class Mock<IAdapterGroup_1_13> : public IAdapterGroup_1_13 {
  public:
    ~Mock() override = default;

    ADDMETHOD_NOBASE(GetAdapter, IAdapter_1_13 *, nullptr, (uint32_t));
    ADDMETHOD_CONST_NOBASE(GetParams, const TAdapterGroupParams_1_6 *, nullptr, ());
    ADDMETHOD_NOBASE(Close, TCompletionCode, TCompletionCode::CC_OK, ());
};

template <>
class Mock<IAdapter_1_13> : public IAdapter_1_13 {
  public:
    ~Mock() override = default;

    // 1.13
    ADDMETHOD_NOBASE(GetSubDeviceParams, const TSubDeviceParams_1_9 *, nullptr, (const uint32_t subDeviceIndex));
    ADDMETHOD_NOBASE(GetEngineParams, const TEngineParams_1_13 *, nullptr, (const uint32_t subDeviceIndex, const uint32_t engineIndex));
    ADDMETHOD_NOBASE(OpenMetricsSubDeviceFromFile, TCompletionCode, TCompletionCode::CC_OK, (const uint32_t subDeviceIndex, const char *fileName, void *openParams, IMetricsDevice_1_13 **metricsDevice));
    ADDMETHOD_CONST_NOBASE(GetParams, const TAdapterParams_1_9 *, nullptr, ());
    ADDMETHOD_NOBASE(Reset, TCompletionCode, TCompletionCode::CC_OK, ());
    ADDMETHOD_NOBASE(OpenMetricsDeviceFromFile, TCompletionCode, TCompletionCode::CC_OK, (const char *, void *, IMetricsDevice_1_13 **));
    ADDMETHOD_NOBASE(CloseMetricsDevice, TCompletionCode, TCompletionCode::CC_OK, (IMetricsDevice_1_5 *));
    ADDMETHOD_NOBASE(SaveMetricsDeviceToFile, TCompletionCode, TCompletionCode::CC_OK, (const char *, void *, IMetricsDevice_1_11 *, const uint32_t, const uint32_t));

    TCompletionCode OpenMetricsDevice(IMetricsDevice_1_13 **metricsDevice) override {
        *metricsDevice = openMetricsDeviceOutDevice;
        return openMetricsDeviceResult;
    }

    TCompletionCode OpenMetricsSubDevice(const uint32_t subDeviceIndex, IMetricsDevice_1_13 **metricsDevice) override {
        *metricsDevice = openMetricsSubDeviceOutDevice;
        return openMetricsSubDeviceResult;
    }

    IMetricsDevice_1_13 *openMetricsSubDeviceOutDevice = nullptr;
    IMetricsDevice_1_13 *openMetricsDeviceOutDevice = nullptr;
    TCompletionCode openMetricsSubDeviceResult = TCompletionCode::CC_OK;
    TCompletionCode openMetricsDeviceResult = TCompletionCode::CC_OK;
};

template <>
class Mock<IMetricsDevice_1_13> : public IMetricsDevice_1_13 {
  public:
    ~Mock() override = default;

    ADDMETHOD_NOBASE(GetParams, TMetricsDeviceParams_1_2 *, nullptr, ());
    ADDMETHOD_NOBASE(GetOverride, IOverride_1_2 *, nullptr, (uint32_t index));
    ADDMETHOD_NOBASE(GetOverrideByName, IOverride_1_2 *, nullptr, (const char *symbolName));
    ADDMETHOD_NOBASE(GetGlobalSymbol, TGlobalSymbol_1_0 *, nullptr, (uint32_t index));
    ADDMETHOD_NOBASE(GetLastError, TCompletionCode, TCompletionCode::CC_OK, ());
    ADDMETHOD_NOBASE(GetGpuCpuTimestamps, TCompletionCode, TCompletionCode::CC_OK, (uint64_t *, uint64_t *, uint32_t *, uint64_t *));

    IConcurrentGroup_1_13 *GetConcurrentGroup(uint32_t index) override {
        return getConcurrentGroupResults[index];
    }

    MetricsDiscovery::TTypedValue_1_0 symbolValue = {};
    bool forceGetSymbolByNameFail = false;
    bool forceGetGpuCpuTimestampsFail = false;
    TTypedValue_1_0 *GetGlobalSymbolValueByName(const char *name) override {
        bool found = false;
        if (forceGetSymbolByNameFail) {
            return nullptr;
        } else if (std::strcmp(name, "OABufferMaxSize") == 0) {
            symbolValue.ValueType = MetricsDiscovery::TValueType::VALUE_TYPE_UINT32;
            symbolValue.ValueUInt32 = 1024;
            found = true;
        } else if (std::strcmp(name, "MaxTimestamp") == 0) {
            symbolValue.ValueType = MetricsDiscovery::TValueType::VALUE_TYPE_UINT64;
            symbolValue.ValueUInt64 = 171798691800UL; // PVC as reference
            found = true;
        } else if (std::strcmp(name, "GpuTimestampFrequency") == 0) {
            symbolValue.ValueType = MetricsDiscovery::TValueType::VALUE_TYPE_UINT64;
            symbolValue.ValueUInt64 = 25000000UL; // PVC as reference
            found = true;
        }
        if (found) {
            return &symbolValue;
        }

        return nullptr;
    }

    TCompletionCode GetGpuCpuTimestamps(uint64_t *gpuTimestampNs, uint64_t *cpuTimestampNs, uint32_t *cpuId) override {
        if (forceGetGpuCpuTimestampsFail) {
            *gpuTimestampNs = 0;
            *cpuTimestampNs = 0;
            *cpuId = 0;
            return MetricsDiscovery::CC_ERROR_GENERAL;
        }

        *gpuTimestampNs += 10;
        *cpuTimestampNs += 10;
        *cpuId = 0;
        return MetricsDiscovery::CC_OK;
    }

    std::vector<IConcurrentGroup_1_13 *> getConcurrentGroupResults;
};

template <>
class Mock<IConcurrentGroup_1_13> : public IConcurrentGroup_1_13 {
  public:
    ~Mock() override = default;

    ADDMETHOD_NOBASE(GetParams, TConcurrentGroupParams_1_13 *, nullptr, ());
    ADDMETHOD_NOBASE(CloseIoStream, TCompletionCode, TCompletionCode::CC_OK, ());
    ADDMETHOD_NOBASE(WaitForReports, TCompletionCode, TCompletionCode::CC_OK, (uint32_t milliseconds));
    ADDMETHOD_NOBASE(SetIoStreamSamplingType, TCompletionCode, TCompletionCode::CC_OK, (TSamplingType type));
    ADDMETHOD_NOBASE(GetIoMeasurementInformation, IInformation_1_0 *, nullptr, (uint32_t index));
    ADDMETHOD_NOBASE(GetIoGpuContextInformation, IInformation_1_0 *, nullptr, (uint32_t index));

    IMetricSet_1_13 *GetMetricSet(uint32_t index) override {
        if (!getMetricSetResults.empty()) {
            return getMetricSetResults[index];
        }
        return getMetricSetResult;
    }
    TCompletionCode OpenIoStream(IMetricSet_1_0 *metricSet, uint32_t processId, uint32_t *nsTimerPeriod, uint32_t *oaBufferSize) override {
        if (openIoStreamOutOaBufferSize) {
            *oaBufferSize = *openIoStreamOutOaBufferSize;
        }
        if (!openIoStreamResults.empty()) {
            auto retVal = openIoStreamResults.back();
            openIoStreamResults.pop_back();
            return retVal;
        }
        return openIoStreamResult;
    }
    TCompletionCode ReadIoStream(uint32_t *reportsCount, char *reportData, uint32_t readFlags) override {
        if (!readIoStreamOutReportsCount.empty()) {
            *reportsCount = readIoStreamOutReportsCount.back();
            readIoStreamOutReportsCount.pop_back();
        }

        return readIoStreamResult;
    }

    IMetricSet_1_13 *getMetricSetResult = nullptr;
    std::vector<IMetricSet_1_13 *> getMetricSetResults;
    std::vector<uint32_t> readIoStreamOutReportsCount{};
    uint32_t *openIoStreamOutOaBufferSize = nullptr;
    TCompletionCode openIoStreamResult = TCompletionCode::CC_OK;
    TCompletionCode readIoStreamResult = TCompletionCode::CC_OK;
    std::vector<TCompletionCode> openIoStreamResults{};
};

template <>
class Mock<IEquation_1_0> : public IEquation_1_0 {
  public:
    ~Mock() override = default;

    uint32_t GetEquationElementsCount() override {
        return getEquationElementsCount;
    }

    TEquationElement_1_0 *GetEquationElement(uint32_t index) override {
        if (!getEquationElement.empty()) {
            return getEquationElement[index];
        }
        return nullptr;
    }

    std::vector<TEquationElement_1_0 *> getEquationElement;
    uint32_t getEquationElementsCount = 1u;
};

template <>
class Mock<IMetricSet_1_13> : public IMetricSet_1_13 {
  public:
    ~Mock() override = default;

    ADDMETHOD_NOBASE(GetParams, TMetricSetParams_1_11 *, nullptr, ());
    ADDMETHOD_NOBASE(GetMetric, IMetric_1_13 *, nullptr, (uint32_t index));
    ADDMETHOD_NOBASE(GetInformation, IInformation_1_0 *, nullptr, (uint32_t index));
    ADDMETHOD_NOBASE(Activate, TCompletionCode, TCompletionCode::CC_OK, ());
    ADDMETHOD_NOBASE(Deactivate, TCompletionCode, TCompletionCode::CC_OK, ());
    ADDMETHOD_NOBASE(SetApiFiltering, TCompletionCode, TCompletionCode::CC_OK, (uint32_t apiMask));
    ADDMETHOD_NOBASE(CalculateMetrics, TCompletionCode, TCompletionCode::CC_OK, (const unsigned char *rawData, uint32_t rawDataSize, TTypedValue_1_0 *out, uint32_t outSize, uint32_t *outReportCount, bool enableContextFiltering));
    ADDMETHOD_NOBASE(CalculateIoMeasurementInformation, TCompletionCode, TCompletionCode::CC_OK, (TTypedValue_1_0 * out, uint32_t outSize));
    ADDMETHOD_NOBASE(GetComplementaryMetricSet, IMetricSet_1_5 *, nullptr, (uint32_t index));

    TCompletionCode CalculateMetrics(const unsigned char *rawData, uint32_t rawDataSize, TTypedValue_1_0 *out, uint32_t outSize, uint32_t *outReportCount, TTypedValue_1_0 *outMaxValues, uint32_t outMaxValuesSize) override {
        if (calculateMetricsOutReportCount) {
            *outReportCount = *calculateMetricsOutReportCount;
        }
        return calculateMetricsResult;
    }

    uint32_t *calculateMetricsOutReportCount = nullptr;
    TCompletionCode calculateMetricsResult = TCompletionCode::CC_OK;
};

template <>
class Mock<IMetric_1_13> : public IMetric_1_13 {
  public:
    ~Mock() override = default;

    ADDMETHOD_NOBASE(GetParams, TMetricParams_1_13 *, nullptr, ());
};

template <>
class Mock<IInformation_1_0> : public IInformation_1_0 {
  public:
    Mock(){};

    ADDMETHOD_NOBASE(GetParams, MetricsDiscovery::TInformationParams_1_0 *, nullptr, ());
};

template <>
struct Mock<MetricEnumeration> : public MetricEnumeration {
    Mock(::L0::OaMetricSourceImp &metricSource);
    ~Mock() override = default;

    using BaseClass = MetricEnumeration;

    using MetricEnumeration::cleanupMetricsDiscovery;
    using MetricEnumeration::hMetricsDiscovery;
    using MetricEnumeration::initializationState;
    using MetricEnumeration::openAdapterGroup;
    using MetricEnumeration::openMetricsDiscovery;

    // Api mock enable/disable.
    void setMockedApi(MockMetricsDiscoveryApi *mockedApi);

    // Mock metric enumeration functions.
    ADDMETHOD(isInitialized, bool, false, true, (), ());
    ADDMETHOD(loadMetricsDiscovery, ze_result_t, false, ZE_RESULT_SUCCESS, (), ());

    MetricsDiscovery::IAdapter_1_13 *getMetricsAdapterResult = nullptr;
    MetricsDiscovery::IAdapter_1_13 *getMetricsAdapter() override {
        return getMetricsAdapterResult;
    }

    bool getAdapterId(uint32_t &drmMajor, uint32_t &drmMinor) override {
        if (getAdapterIdCallBase) {
            return MetricEnumeration::getAdapterId(drmMajor, drmMinor);
        }
        drmMajor = getAdapterIdOutMajor;
        drmMinor = getAdapterIdOutMinor;
        return getAdapterIdResult;
    }

    // Not mocked metrics enumeration functions.
    ze_result_t baseLoadMetricsDiscovery() { return MetricEnumeration::loadMetricsDiscovery(); }

    // Mock metrics discovery api.
    static MockMetricsDiscoveryApi *globalMockApi;

    // Original metric enumeration obtained from metric context.
    ::L0::MetricEnumeration *metricEnumeration = nullptr;

    uint32_t getAdapterIdOutMajor = 0u;
    uint32_t getAdapterIdOutMinor = 0u;
    bool getAdapterIdResult = true;
    bool getAdapterIdCallBase = false;
};

template <>
struct Mock<MetricGroup> : public OaMetricGroupImp {
    ~Mock() override = default;
    Mock(MetricSource &metricSource) : OaMetricGroupImp(metricSource) {}

    ADDMETHOD_NOBASE(metricGet, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t *, zet_metric_handle_t *));
    ADDMETHOD_NOBASE(calculateMetricValues, ze_result_t, ZE_RESULT_SUCCESS, (const zet_metric_group_calculation_type_t, size_t, const uint8_t *, uint32_t *, zet_typed_value_t *));
    ADDMETHOD_NOBASE(calculateMetricValuesExp, ze_result_t, ZE_RESULT_SUCCESS, (const zet_metric_group_calculation_type_t, size_t, const uint8_t *, uint32_t *, uint32_t *, uint32_t *, zet_typed_value_t *));
    ADDMETHOD_NOBASE(activate, bool, true, ());
    ADDMETHOD_NOBASE(deactivate, bool, true, ());

    zet_metric_group_handle_t getMetricGroupForSubDevice(const uint32_t subDeviceIndex) override {
        return nullptr;
    }

    ze_result_t getProperties(zet_metric_group_properties_t *properties) override {
        if (getPropertiesOutProperties) {
            *properties = *getPropertiesOutProperties;
        }
        return getPropertiesResult;
    }

    zet_metric_group_properties_t *getPropertiesOutProperties = nullptr;
    ze_result_t getPropertiesResult = ZE_RESULT_SUCCESS;
};

struct MetricGroupImpTest : public OaMetricGroupImp {
    MetricGroupImpTest(MetricSource &metricSource) : OaMetricGroupImp(metricSource) {}
    using OaMetricGroupImp::copyValue;
    using OaMetricGroupImp::pReferenceConcurrentGroup;
    using OaMetricGroupImp::pReferenceMetricSet;
};

} // namespace ult
} // namespace L0
