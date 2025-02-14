/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"
#include "level_zero/zet_intel_gpu_metric_export.h"

#include "gtest/gtest.h"
#include "metrics_discovery_api.h"

namespace L0 {
namespace ult {
static const char *equationName = "EquationElement";
static const char *testString = "TestString";
class MockIEquation10 : public MetricsDiscovery::IEquation_1_0 {
  public:
    MockIEquation10() {
        equationElement.Type = MetricsDiscovery::EQUATION_ELEM_IMM_UINT64;
        equationElement.ImmediateUInt64 = 0;
        equationElement.SymbolName = const_cast<char *>(equationName);
    }

    ~MockIEquation10() override = default;

    uint32_t GetEquationElementsCount(void) override { return 1; }
    MetricsDiscovery::TEquationElement_1_0 *GetEquationElement(uint32_t index) override {
        return &equationElement;
    };
    MetricsDiscovery::TEquationElement_1_0 equationElement;
};

class MetricExportDataOaTest : public Test<MetricMultiDeviceFixture> {
  public:
    uint32_t globalSymbolsCount = 1;

    template <typename T>
    T readUnaligned(T *unaligned) {
        T returnVal{};
        memcpy(reinterpret_cast<uint8_t *>(&returnVal), reinterpret_cast<uint8_t *>(unaligned), sizeof(T));
        return returnVal;
    }

    void setupMetricDeviceParams(MetricsDiscovery::TMetricsDeviceParams_1_2 &metricDeviceParams) {
        // Metrics Discovery device.
        metricsDeviceParams.ConcurrentGroupsCount = 1;
        metricsDeviceParams.GlobalSymbolsCount = globalSymbolsCount;
        metricsDeviceParams.Version.BuildNumber = 20;
        metricsDeviceParams.Version.MajorNumber = 1;
        metricsDeviceParams.Version.MinorNumber = 13;
    }

    void setupConcurrentGroupParams(TConcurrentGroupParams_1_0 &metricsConcurrentGroupParams) {
        metricsConcurrentGroupParams.MetricSetsCount = 1;
        metricsConcurrentGroupParams.SymbolName = "OA";
        metricsConcurrentGroupParams.Description = "OA description";
        metricsConcurrentGroupParams.IoMeasurementInformationCount = 1;
        metricsConcurrentGroupParams.IoGpuContextInformationCount = 1;
    }

    void setupMetricSetParams(MetricsDiscovery::TMetricSetParams_1_11 &metricsSetParams) {
        metricsSetParams.ApiMask = 50;
        metricsSetParams.SymbolName = "Metric set name";
        metricsSetParams.ShortName = "Metric set description";
        metricsSetParams.MetricsCount = 1;
        metricsSetParams.InformationCount = 1;
        metricsSetParams.AvailabilityEquation = testString;
    }

    void setupAdapterParams(TAdapterParams_1_9 &adapterParams) {
        adapterParams.BusNumber = 1;
        adapterParams.DeviceNumber = 2;
        adapterParams.FunctionNumber = 3;
        adapterParams.DomainNumber = 4;
        adapterParams.SystemId.Type = MetricsDiscovery::ADAPTER_ID_TYPE_MAJOR_MINOR;
        adapterParams.SystemId.MajorMinor.Major = 5;
        adapterParams.SystemId.MajorMinor.Minor = 6;
    }

    void setupGlobalSymbol(TGlobalSymbol_1_0 &globalSymbol) {
        valueByteArray.Size = 5;
        valueByteArray.Data = &byteArrayData[0];

        globalSymbol.SymbolName = "globalSymbol1";
        globalSymbol.SymbolTypedValue.ValueType = MetricsDiscovery::VALUE_TYPE_BYTEARRAY;
        globalSymbol.SymbolTypedValue.ValueByteArray = &valueByteArray;
    }

    void setupInformationParams(MetricsDiscovery::TInformationParams_1_0 &informationParams,
                                MockIEquation10 &equation,
                                MetricsDiscovery::TDeltaFunction_1_0 &deltaFunction) {
        informationParams.ApiMask = 20;
        informationParams.GroupName = "infoParamsGroupName";
        informationParams.IdInSet = 1;
        informationParams.InfoType = MetricsDiscovery::INFORMATION_TYPE_VALUE;
        informationParams.InfoUnits = "infoParamsUnits";
        informationParams.LongName = "infoParamsLongName";
        informationParams.ShortName = "infoParamsShortName";
        informationParams.SymbolName = "BufferOverflow";
        informationParams.IoReadEquation = &equation;
        informationParams.QueryReadEquation = &equation;
        informationParams.OverflowFunction = deltaFunction;
    }

    void setupMetricParams(TMetricParams_1_0 &metricParams,
                           MockIEquation10 &equation,
                           MetricsDiscovery::TDeltaFunction_1_0 &deltaFunction) {
        metricParams.IdInSet = 24;
        metricParams.SymbolName = "Metric symbol name";
        metricParams.ShortName = "Metric short name";
        metricParams.GroupName = "Metric group name";
        metricParams.LongName = "Metric long name";
        metricParams.ApiMask = 16;
        metricParams.UsageFlagsMask = 98;
        metricParams.MetricResultUnits = "MetricResultUnits";
        metricParams.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;
        metricParams.IoReadEquation = &equation;
        metricParams.QueryReadEquation = &equation;
        metricParams.DeltaFunction = deltaFunction;
        metricParams.NormEquation = &equation;
        metricParams.MaxValueEquation = &equation;
        metricParams.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    }

    void setupMdapiParameters() {
        setupConcurrentGroupParams(metricsConcurrentGroupParams);
        setupMetricDeviceParams(metricsDeviceParams);
        setupMetricSetParams(metricsSetParams);
        setupAdapterParams(adapterParams);
        setupGlobalSymbol(globalSymbol);
        deltaFunction.FunctionType = MetricsDiscovery::DELTA_N_BITS;
        deltaFunction.BitsCount = 5;
        setupInformationParams(informationParams, equation, deltaFunction);
        setupMetricParams(metricParams, equation, deltaFunction);
    }

    void validateDeviceParams(zet_intel_metric_df_gpu_export_data_format_t *data) {

        auto oaData = &data->format01.oaData;
        EXPECT_EQ(readUnaligned(&oaData->deviceParams.globalSymbolsCount), globalSymbolsCount);
        EXPECT_EQ(readUnaligned(&oaData->deviceParams.version.buildNumber), 20u);
        EXPECT_EQ(readUnaligned(&oaData->deviceParams.version.majorNumber), 1u);
        EXPECT_EQ(readUnaligned(&oaData->deviceParams.version.minorNumber), 13u);
    }

    void validateGlobalSymbols(zet_intel_metric_df_gpu_export_data_format_t *data) {
        auto oaData = &data->format01.oaData;
        auto globalSymbols = zet_intel_metric_df_gpu_offset_to_ptr(zet_intel_metric_df_gpu_global_symbol_0_1_offset_t, oaData->globalSymbols, data);
        auto symbolPtr = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, globalSymbols->symbolName, data);
        EXPECT_STREQ(symbolPtr, "globalSymbol1");
        EXPECT_EQ(readUnaligned(&globalSymbols->symbolTypedValue.valueType), ZET_INTEL_METRIC_DF_VALUE_TYPE_BYTEARRAY);
        EXPECT_EQ(readUnaligned(&globalSymbols->symbolTypedValue.valueByteArray.size), 5u);
        auto byteArray = zet_intel_metric_df_gpu_offset_to_ptr(uint8_offset_t, globalSymbols->symbolTypedValue.valueByteArray.data, data);
        EXPECT_EQ(memcmp(byteArray, &byteArrayData[0], 5), 0);
    }

    void validateAdapterParams(zet_intel_metric_df_gpu_export_data_format_t *data) {
        auto oaData = &data->format01.oaData;
        EXPECT_EQ(readUnaligned(&oaData->adapterParams.busNumber), 1u);
        EXPECT_EQ(readUnaligned(&oaData->adapterParams.deviceNumber), 2u);
        EXPECT_EQ(readUnaligned(&oaData->adapterParams.functionNumber), 3u);
        EXPECT_EQ(readUnaligned(&oaData->adapterParams.domainNumber), 4u);
        EXPECT_EQ(readUnaligned(&oaData->adapterParams.systemId.majorMinor.major), 5);
        EXPECT_EQ(readUnaligned(&oaData->adapterParams.systemId.majorMinor.minor), 6);
        EXPECT_EQ(readUnaligned(&oaData->adapterParams.systemId.type), ZET_INTEL_METRIC_DF_ADAPTER_ID_TYPE_MAJOR_MINOR);
    }

    void validateEquation(zet_intel_metric_df_gpu_export_data_format_t *data, zet_intel_metric_df_gpu_equation_0_1_t &equation) {
        EXPECT_EQ(readUnaligned(&equation.elementCount), 1u);
        auto elementsPtr = zet_intel_metric_df_gpu_offset_to_ptr(zet_intel_metric_df_gpu_equation_element_0_1_offset_t, equation.elements, data);
        auto elementSymbolPtr = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, elementsPtr->symbolName, data);
        EXPECT_STREQ(elementSymbolPtr, "EquationElement");
        EXPECT_EQ(readUnaligned(&elementsPtr->type), ZET_INTEL_METRIC_DF_EQUATION_ELEM_IMM_UINT64);
        EXPECT_EQ(readUnaligned(&elementsPtr->operation), ZET_INTEL_METRIC_DF_EQUATION_OPER_RSHIFT);
    }

    void validateInformationParams(zet_intel_metric_df_gpu_export_data_format_t *data, zet_intel_metric_df_gpu_information_params_0_1_offset_t infoOffset) {

        auto infoPtr = zet_intel_metric_df_gpu_offset_to_ptr(zet_intel_metric_df_gpu_information_params_0_1_offset_t, infoOffset, data);
        EXPECT_EQ(readUnaligned(&infoPtr->apiMask), 20u);
        auto groupNamePtr = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, infoPtr->groupName, data);
        EXPECT_STREQ(groupNamePtr, "infoParamsGroupName");
        EXPECT_EQ(readUnaligned(&infoPtr->idInSet), 1u);
        EXPECT_EQ(readUnaligned(&infoPtr->infoType), ZET_INTEL_METRIC_DF_INFORMATION_TYPE_VALUE);
        auto unitsPtr = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, infoPtr->infoUnits, data);
        EXPECT_STREQ(unitsPtr, "infoParamsUnits");
        validateEquation(data, infoPtr->ioReadEquation);
        auto longNamePtr = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, infoPtr->longName, data);
        EXPECT_STREQ(longNamePtr, "infoParamsLongName");
        EXPECT_EQ(readUnaligned(&infoPtr->overflowFunction.bitsCount), deltaFunction.BitsCount);
        EXPECT_EQ(readUnaligned(&infoPtr->overflowFunction.functionType), ZET_INTEL_METRIC_DF_DELTA_N_BITS);
        validateEquation(data, infoPtr->queryReadEquation);
        auto shortNamePtr = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, infoPtr->shortName, data);
        EXPECT_STREQ(shortNamePtr, "infoParamsShortName");
        auto symbolNamePtr = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, infoPtr->symbolName, data);
        EXPECT_STREQ(symbolNamePtr, "BufferOverflow");
    }

    void validateMetricSet(zet_intel_metric_df_gpu_export_data_format_t *data) {
        auto oaData = &data->format01.oaData;
        // Validate Metric Set Parameters
        EXPECT_EQ(readUnaligned(&oaData->metricSet.params.apiMask), 50u);
        EXPECT_EQ(readUnaligned(&oaData->metricSet.params.availabilityEquation), 1064u);
        EXPECT_EQ(readUnaligned(&oaData->metricSet.params.informationCount), 1u);
        EXPECT_EQ(readUnaligned(&oaData->metricSet.params.metricsCount), 1u);
        auto shortNamePtr = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, oaData->metricSet.params.shortName, data);
        EXPECT_STREQ(shortNamePtr, "Metric set description");
        auto symbolPtr = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, oaData->metricSet.params.symbolName, data);
        EXPECT_STREQ(symbolPtr, "Metric set name");

        // Validate Metric Parameters
        auto metricParamsPtr = zet_intel_metric_df_gpu_offset_to_ptr(zet_intel_metric_df_gpu_metric_params_0_1_offset_t, oaData->metricSet.metricParams, data);
        EXPECT_EQ(readUnaligned(&metricParamsPtr->apiMask), 16u);
        EXPECT_EQ(readUnaligned(&metricParamsPtr->deltaFunction.bitsCount), deltaFunction.BitsCount);
        EXPECT_EQ(readUnaligned(&metricParamsPtr->deltaFunction.functionType), ZET_INTEL_METRIC_DF_DELTA_N_BITS);

        auto groupNamePtr = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, metricParamsPtr->groupName, data);
        EXPECT_STREQ(groupNamePtr, "Metric group name");
        EXPECT_EQ(readUnaligned(&metricParamsPtr->idInSet), 24u);
        validateEquation(data, metricParamsPtr->ioReadEquation);
        auto longNamePtr = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, metricParamsPtr->longName, data);
        EXPECT_STREQ(longNamePtr, "Metric long name");
        validateEquation(data, metricParamsPtr->maxValueEquation);
        auto metricsResultUnits = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, metricParamsPtr->metricResultUnits, data);
        EXPECT_STREQ(metricsResultUnits, "MetricResultUnits");
        EXPECT_EQ(readUnaligned(&metricParamsPtr->metricType), ZET_INTEL_METRIC_DF_METRIC_TYPE_RATIO);
        validateEquation(data, metricParamsPtr->normEquation);
        validateEquation(data, metricParamsPtr->queryReadEquation);
        EXPECT_EQ(readUnaligned(&metricParamsPtr->resultType), ZET_INTEL_METRIC_DF_RESULT_UINT64);
        EXPECT_EQ(readUnaligned(&metricParamsPtr->usageFlagsMask), 98u);

        // Validate Information Parameters
        validateInformationParams(data, oaData->metricSet.informationParams);
    }

    void validateConcurrentGroup(zet_intel_metric_df_gpu_export_data_format_t *data) {
        auto oaData = &data->format01.oaData;
        validateInformationParams(data, oaData->concurrentGroup.ioGpuContextInformation);
        validateInformationParams(data, oaData->concurrentGroup.ioMeasurementInformation);
        auto descPtr = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, oaData->concurrentGroup.params.description, data);
        EXPECT_STREQ(descPtr, "OA description");
        EXPECT_EQ(readUnaligned(&oaData->concurrentGroup.params.ioGpuContextInformationCount), 1u);
        EXPECT_EQ(readUnaligned(&oaData->concurrentGroup.params.ioMeasurementInformationCount), 1u);
        auto symbolPtr = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, oaData->concurrentGroup.params.symbolName, data);
        EXPECT_STREQ(symbolPtr, "OA");
    }

    void validateParameters(zet_intel_metric_df_gpu_export_data_format_t *data) {
        validateConcurrentGroup(data);
        validateMetricSet(data);
        validateAdapterParams(data);
        validateDeviceParams(data);
    }

    void setupEquationElement(MetricsDiscovery::TEquationElement_1_0 &mdElement) {
        switch (mdElement.Type) {
        case MetricsDiscovery::EQUATION_ELEM_OPERATION:
            mdElement.Operation = MetricsDiscovery::EQUATION_OPER_RSHIFT;
            break;
        case MetricsDiscovery::EQUATION_ELEM_RD_BITFIELD:
            mdElement.ReadParams.ByteOffset = 100;
            mdElement.ReadParams.BitOffset = 101;
            mdElement.ReadParams.BitsCount = 102;
            break;
        case MetricsDiscovery::EQUATION_ELEM_RD_UINT8:
        case MetricsDiscovery::EQUATION_ELEM_RD_UINT16:
        case MetricsDiscovery::EQUATION_ELEM_RD_UINT32:
        case MetricsDiscovery::EQUATION_ELEM_RD_UINT64:
        case MetricsDiscovery::EQUATION_ELEM_RD_FLOAT:
            mdElement.ReadParams.ByteOffset = 103;
            break;
        case MetricsDiscovery::EQUATION_ELEM_RD_40BIT_CNTR:
            mdElement.ReadParams.ByteOffset = 104;
            mdElement.ReadParams.ByteOffsetExt = 105;
            break;
        case MetricsDiscovery::EQUATION_ELEM_IMM_UINT64:
            mdElement.ImmediateUInt64 = 106;
            break;
        case MetricsDiscovery::EQUATION_ELEM_IMM_FLOAT:
            mdElement.ImmediateFloat = 107.108f;
            break;
        case MetricsDiscovery::EQUATION_ELEM_SELF_COUNTER_VALUE:
            break;
        case MetricsDiscovery::EQUATION_ELEM_GLOBAL_SYMBOL:
        case MetricsDiscovery::EQUATION_ELEM_LOCAL_COUNTER_SYMBOL:
        case MetricsDiscovery::EQUATION_ELEM_OTHER_SET_COUNTER_SYMBOL:
            break;

        case MetricsDiscovery::EQUATION_ELEM_LOCAL_METRIC_SYMBOL:
        case MetricsDiscovery::EQUATION_ELEM_OTHER_SET_METRIC_SYMBOL:
        case MetricsDiscovery::EQUATION_ELEM_INFORMATION_SYMBOL:
        case MetricsDiscovery::EQUATION_ELEM_STD_NORM_GPU_DURATION:
        case MetricsDiscovery::EQUATION_ELEM_STD_NORM_EU_AGGR_DURATION:
            break;
        case MetricsDiscovery::EQUATION_ELEM_MASK:
            mdElement.Mask.Size = 3;
            mdElement.Mask.Data = &byteArrayData[0];
            break;
        default:
            EXPECT_TRUE(false);
            break;
        }
    }

    void validateEquationElement(zet_intel_metric_df_gpu_equation_element_0_1_t &element, void *base) {
        switch (element.type) {
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_OPERATION:
            EXPECT_EQ(readUnaligned(&element.operation), ZET_INTEL_METRIC_DF_EQUATION_OPER_RSHIFT);
            break;
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_BITFIELD:
            EXPECT_EQ(readUnaligned(&element.readParams.byteOffset), 100u);
            EXPECT_EQ(readUnaligned(&element.readParams.bitOffset), 101u);
            EXPECT_EQ(readUnaligned(&element.readParams.bitsCount), 102u);
            break;
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT8:
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT16:
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT32:
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT64:
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_FLOAT:
            EXPECT_EQ(readUnaligned(&element.readParams.byteOffset), 103u);
            break;
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_40BIT_CNTR:
            EXPECT_EQ(readUnaligned(&element.readParams.byteOffset), 104u);
            EXPECT_EQ(readUnaligned(&element.readParams.byteOffsetExt), 105u);
            break;
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_IMM_UINT64:
            EXPECT_EQ(readUnaligned(&element.immediateUInt64), 106u);
            break;
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_IMM_FLOAT:
            EXPECT_EQ(readUnaligned(&element.immediateFloat), 107.108f);
            break;
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_SELF_COUNTER_VALUE:
            break;
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_GLOBAL_SYMBOL:
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_LOCAL_COUNTER_SYMBOL:
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_OTHER_SET_COUNTER_SYMBOL:
            break;

        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_LOCAL_METRIC_SYMBOL:
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_OTHER_SET_METRIC_SYMBOL:
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_INFORMATION_SYMBOL:
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_STD_NORM_GPU_DURATION:
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_STD_NORM_EU_AGGR_DURATION:
            break;
        case ZET_INTEL_METRIC_DF_EQUATION_ELEM_MASK: {
            EXPECT_EQ(readUnaligned(&element.mask.size), 3u);
            auto byteArrayPtr = zet_intel_metric_df_gpu_offset_to_ptr(uint8_offset_t, element.mask.data, base);
            EXPECT_EQ(memcmp(byteArrayPtr, &byteArrayData[0], 3), 0);
        } break;
        default:
            EXPECT_TRUE(false);
            break;
        }
    }

    void setupTypedValue(MetricsDiscovery::TTypedValue_1_0 &value) {
        switch (value.ValueType) {
        case MetricsDiscovery::VALUE_TYPE_UINT32:
            value.ValueUInt32 = std::numeric_limits<uint32_t>::max() >> 1;
            break;
        case MetricsDiscovery::VALUE_TYPE_UINT64:
            value.ValueUInt64 = std::numeric_limits<uint64_t>::max() >> 1;
            break;
        case MetricsDiscovery::VALUE_TYPE_FLOAT:
            value.ValueFloat = 92.86f;
            break;
        case MetricsDiscovery::VALUE_TYPE_BOOL:
            value.ValueBool = true;
            break;
        case MetricsDiscovery::VALUE_TYPE_CSTRING:
            value.ValueCString = const_cast<char *>(testString);
            break;
        case MetricsDiscovery::VALUE_TYPE_BYTEARRAY:
            valueByteArray.Size = 5;
            valueByteArray.Data = &byteArrayData[0];
            value.ValueByteArray = &valueByteArray;
            break;
        default:
            EXPECT_TRUE(false);
            break;
        }
    }

    void validateTypedValue(zet_intel_metric_df_gpu_typed_value_0_1_t typedValue, void *base) {
        switch (typedValue.valueType) {
        case ZET_INTEL_METRIC_DF_VALUE_TYPE_UINT32:
            EXPECT_EQ(readUnaligned(&typedValue.valueUInt32), std::numeric_limits<uint32_t>::max() >> 1u);
            break;
        case ZET_INTEL_METRIC_DF_VALUE_TYPE_UINT64:
            EXPECT_EQ(readUnaligned(&typedValue.valueUInt64), std::numeric_limits<uint64_t>::max() >> 1u);
            break;
        case ZET_INTEL_METRIC_DF_VALUE_TYPE_FLOAT:
            EXPECT_EQ(readUnaligned(&typedValue.valueFloat), 92.86f);
            break;
        case ZET_INTEL_METRIC_DF_VALUE_TYPE_BOOL:
            EXPECT_EQ(readUnaligned(&typedValue.valueBool), true);
            break;
        case ZET_INTEL_METRIC_DF_VALUE_TYPE_CSTRING: {
            auto stringPtr = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, typedValue.valueCString, base);
            EXPECT_STREQ(stringPtr, const_cast<char *>(testString));
        } break;
        case ZET_INTEL_METRIC_DF_VALUE_TYPE_BYTEARRAY: {
            auto byteArrayPtr = zet_intel_metric_df_gpu_offset_to_ptr(uint8_offset_t, typedValue.valueByteArray.data, base);
            EXPECT_EQ(readUnaligned(&typedValue.valueByteArray.size), 5u);
            EXPECT_EQ(memcmp(byteArrayPtr, &byteArrayData[0], 5), 0);
        } break;
        default:
            EXPECT_TRUE(false);
            break;
        }
    }

    void setupMocks() {
        adapter.GetParamsResult = &adapterParams;
        metricsDevice.GetGlobalSymbolResult = &globalSymbol;
        metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);
        metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
        metricsConcurrentGroup.GetIoMeasurementInformationResult = &information;
        metricsConcurrentGroup.GetIoGpuContextInformationResult = &information;
        information.GetParamsResult = &informationParams;
        metricsConcurrentGroup.getMetricSetResult = &metricsSet;
        metricsSet.GetParamsResult = &metricsSetParams;
        metricsSet.GetInformationResult = &information;
        metricsSet.GetMetricResult = &metric;
        metric.GetParamsResult = &metricParams;
    }

    zet_metric_group_handle_t getMetricGroupHandle() {
        zet_metric_group_handle_t metricGroupHandle = {};
        uint32_t metricGroupCount = 0;
        EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricGroupCount, 1u);
        EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricGroupCount, 1u);
        EXPECT_NE(metricGroupHandle, nullptr);
        return metricGroupHandle;
    }

    uint8_t byteArrayData[5] = {10, 20, 30, 40, 50};
    MetricsDiscovery::TByteArray_1_0 valueByteArray;

    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    TAdapterParams_1_9 adapterParams{};
    TGlobalSymbol_1_0 globalSymbol{};
    MockIEquation10 equation{};
    MetricsDiscovery::TDeltaFunction_1_0 deltaFunction{};
    Mock<IInformation_1_0> information{};
    MetricsDiscovery::TInformationParams_1_0 informationParams{};
    TMetricParams_1_13 metricParams = {};
    Mock<IMetric_1_13> metric;
};

TEST_F(MetricExportDataOaTest, givenValidArgumentsWhenMetricGroupGetExportDataIsCalledThenReturnSuccess) {

    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();
    auto metricGroupHandle = getMetricGroupHandle();

    uint8_t dummyRawData = 0;
    size_t exportDataSize = 0;
    const auto dummyRawDataSize = 1u;
    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, dummyRawDataSize, &exportDataSize, nullptr),
              ZE_RESULT_SUCCESS);
    EXPECT_GT(exportDataSize, 0u);
    std::vector<uint8_t> exportData(exportDataSize + 8);
    for (auto unalignedOffset = 0; unalignedOffset < 8; unalignedOffset++) {
        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                                 &dummyRawData, dummyRawDataSize, &exportDataSize, exportData.data() + unalignedOffset),
                  ZE_RESULT_SUCCESS);
        auto exportedData = reinterpret_cast<zet_intel_metric_df_gpu_export_data_format_t *>(exportData.data() + unalignedOffset);
        validateParameters(exportedData);
        EXPECT_EQ(readUnaligned(&exportedData->header.type), ZET_INTEL_METRIC_DF_SOURCE_TYPE_OA);
        EXPECT_GT(readUnaligned(&exportedData->header.rawDataOffset), 0u);
        EXPECT_EQ(readUnaligned(&exportedData->header.rawDataSize), dummyRawDataSize);
    }
}

TEST_F(MetricExportDataOaTest, givenSystemIdTypeIsLuidWhenMetricGroupGetExportDataIsCalledThenCorrectValuesAreExported) {

    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();

    auto metricGroupHandle = getMetricGroupHandle();

    uint8_t dummyRawData = 0;
    size_t exportDataSize = 0;
    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, 1, &exportDataSize, nullptr),
              ZE_RESULT_SUCCESS);
    EXPECT_GT(exportDataSize, 0u);
    std::vector<uint8_t> exportData(exportDataSize);

    adapterParams.SystemId.Type = MetricsDiscovery::ADAPTER_ID_TYPE_LUID;
    adapterParams.SystemId.Luid.LowPart = 21;
    adapterParams.SystemId.Luid.HighPart = 22;

    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, 1, &exportDataSize, exportData.data()),
              ZE_RESULT_SUCCESS);
    auto oaData = &reinterpret_cast<zet_intel_metric_df_gpu_export_data_format_t *>(exportData.data())->format01.oaData;
    EXPECT_EQ(readUnaligned(&oaData->adapterParams.systemId.luid.lowPart), 21u);
    EXPECT_EQ(readUnaligned(&oaData->adapterParams.systemId.luid.highPart), 22);
    EXPECT_EQ(readUnaligned(&oaData->adapterParams.systemId.type), ZET_INTEL_METRIC_DF_ADAPTER_ID_TYPE_LUID);
}

TEST_F(MetricExportDataOaTest, givenInvalidSystemIdTypeWhenMetricGroupGetExportDataIsCalledThenErrorIsReturned) {

    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();

    auto metricGroupHandle = getMetricGroupHandle();

    uint8_t dummyRawData = 0;
    size_t exportDataSize = 0;
    adapterParams.SystemId.Type = MetricsDiscovery::ADAPTER_ID_TYPE_UNDEFINED;
    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, 1, &exportDataSize, nullptr),
              ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
    EXPECT_EQ(exportDataSize, 0u);
}

TEST_F(MetricExportDataOaTest, givenGlobalSymbolsWithSupportedValueTypesWhenMetricGroupGetExportDataIsCalledThenCorrectValuesAreExported) {
    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();

    auto metricGroupHandle = getMetricGroupHandle();
    std::vector<std::pair<zet_intel_metric_df_gpu_value_type_t, MetricsDiscovery::TValueType>> testTypeIds = {
        {
            ZET_INTEL_METRIC_DF_VALUE_TYPE_UINT32,
            MetricsDiscovery::VALUE_TYPE_UINT32,
        },
        {
            ZET_INTEL_METRIC_DF_VALUE_TYPE_UINT64,
            MetricsDiscovery::VALUE_TYPE_UINT64,
        },
        {
            ZET_INTEL_METRIC_DF_VALUE_TYPE_FLOAT,
            MetricsDiscovery::VALUE_TYPE_FLOAT,
        },
        {
            ZET_INTEL_METRIC_DF_VALUE_TYPE_BOOL,
            MetricsDiscovery::VALUE_TYPE_BOOL,
        },
        {
            ZET_INTEL_METRIC_DF_VALUE_TYPE_CSTRING,
            MetricsDiscovery::VALUE_TYPE_CSTRING,
        },
        {
            ZET_INTEL_METRIC_DF_VALUE_TYPE_BYTEARRAY,
            MetricsDiscovery::VALUE_TYPE_BYTEARRAY,
        },
    };

    for (const auto &[dfType, mdType] : testTypeIds) {
        uint8_t dummyRawData = 0;
        size_t exportDataSize = 0;
        globalSymbol.SymbolTypedValue.ValueType = mdType;
        setupTypedValue(globalSymbol.SymbolTypedValue);

        // Resetting the cache so that heap area is calculated everytime
        auto metricGroup = L0::MetricGroup::fromHandle(metricGroupHandle);
        auto oaMetricGroupImp = static_cast<OaMetricGroupImp *>(metricGroup);
        oaMetricGroupImp->setCachedExportDataHeapSize(0);

        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                                 &dummyRawData, 1, &exportDataSize, nullptr),
                  ZE_RESULT_SUCCESS);
        EXPECT_GT(exportDataSize, 0u);
        std::vector<uint8_t> exportData(exportDataSize);
        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                                 &dummyRawData, 1, &exportDataSize, exportData.data()),
                  ZE_RESULT_SUCCESS);
        auto base = exportData.data();
        auto oaData = &reinterpret_cast<zet_intel_metric_df_gpu_export_data_format_t *>(exportData.data())->format01.oaData;
        auto globalSymbolsPtr = zet_intel_metric_df_gpu_offset_to_ptr(zet_intel_metric_df_gpu_global_symbol_0_1_offset_t, oaData->globalSymbols, base);
        EXPECT_EQ(readUnaligned(&globalSymbolsPtr->symbolTypedValue.valueType), dfType);
        validateTypedValue(globalSymbolsPtr->symbolTypedValue, base);
    }
}

TEST_F(MetricExportDataOaTest, givenGlobalSymbolsWithSupportedValueTypeWhenMetricGroupGetExportDataIsCalledThenErrorIsReturned) {

    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();

    auto metricGroupHandle = getMetricGroupHandle();
    uint8_t dummyRawData = 0;
    size_t exportDataSize = 0;
    globalSymbol.SymbolTypedValue.ValueType = MetricsDiscovery::VALUE_TYPE_LAST;
    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, 1, &exportDataSize, nullptr),
              ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
    EXPECT_EQ(exportDataSize, 0u);
}

TEST_F(MetricExportDataOaTest, givenEquationsWithSupportedElementTypesWhenMetricGroupGetExportDataIsCalledThenCorrectValuesAreExported) {
    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();

    auto metricGroupHandle = getMetricGroupHandle();
    std::vector<std::pair<MetricsDiscovery::TEquationElementType, zet_intel_metric_df_gpu_equation_element_type_t>> elementTypes = {
        {
            MetricsDiscovery::EQUATION_ELEM_OPERATION,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_OPERATION,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_RD_BITFIELD,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_BITFIELD,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_RD_UINT8,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT8,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_RD_UINT16,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT16,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_RD_UINT32,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT32,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_RD_UINT64,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT64,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_RD_FLOAT,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_FLOAT,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_RD_40BIT_CNTR,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_40BIT_CNTR,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_IMM_UINT64,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_IMM_UINT64,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_IMM_FLOAT,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_IMM_FLOAT,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_SELF_COUNTER_VALUE,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_SELF_COUNTER_VALUE,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_GLOBAL_SYMBOL,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_GLOBAL_SYMBOL,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_LOCAL_COUNTER_SYMBOL,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_LOCAL_COUNTER_SYMBOL,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_OTHER_SET_COUNTER_SYMBOL,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_OTHER_SET_COUNTER_SYMBOL,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_LOCAL_METRIC_SYMBOL,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_LOCAL_METRIC_SYMBOL,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_OTHER_SET_METRIC_SYMBOL,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_OTHER_SET_METRIC_SYMBOL,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_INFORMATION_SYMBOL,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_INFORMATION_SYMBOL,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_STD_NORM_GPU_DURATION,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_STD_NORM_GPU_DURATION,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_STD_NORM_EU_AGGR_DURATION,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_STD_NORM_EU_AGGR_DURATION,
        },
        {
            MetricsDiscovery::EQUATION_ELEM_MASK,
            ZET_INTEL_METRIC_DF_EQUATION_ELEM_MASK,
        },
    };

    for (const auto &[mdType, dfType] : elementTypes) {
        uint8_t dummyRawData = 0;
        size_t exportDataSize = 0;
        equation.equationElement.Type = mdType;
        setupEquationElement(equation.equationElement);

        // Resetting the cache so that heap area is calculated everytime
        auto metricGroup = L0::MetricGroup::fromHandle(metricGroupHandle);
        auto oaMetricGroupImp = static_cast<OaMetricGroupImp *>(metricGroup);
        oaMetricGroupImp->setCachedExportDataHeapSize(0);

        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                                 &dummyRawData, 1, &exportDataSize, nullptr),
                  ZE_RESULT_SUCCESS);
        EXPECT_GT(exportDataSize, 0u);
        std::vector<uint8_t> exportData(exportDataSize);
        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                                 &dummyRawData, 1, &exportDataSize, exportData.data()),
                  ZE_RESULT_SUCCESS);
        auto base = exportData.data();
        auto oaData = &reinterpret_cast<zet_intel_metric_df_gpu_export_data_format_t *>(exportData.data())->format01.oaData;
        auto metricParamsPtr = zet_intel_metric_df_gpu_offset_to_ptr(zet_intel_metric_df_gpu_metric_params_0_1_offset_t, oaData->metricSet.metricParams, base);
        auto elementsPtr = zet_intel_metric_df_gpu_offset_to_ptr(zet_intel_metric_df_gpu_equation_element_0_1_offset_t, metricParamsPtr->ioReadEquation.elements, base);
        EXPECT_EQ(readUnaligned(&elementsPtr->type), dfType);
        validateEquationElement(*elementsPtr, base);
    }
}

TEST_F(MetricExportDataOaTest, givenEquationsWithUnSupportedElementTypeWhenMetricGroupGetExportDataIsCalledThenErrorIsReturned) {

    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();

    auto metricGroupHandle = getMetricGroupHandle();
    uint8_t dummyRawData = 0;
    size_t exportDataSize = 0;
    equation.equationElement.Type = MetricsDiscovery::EQUATION_ELEM_LAST_1_0;
    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, 1, &exportDataSize, nullptr),
              ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
    EXPECT_EQ(exportDataSize, 0u);
}

TEST_F(MetricExportDataOaTest, givenEquationsWithUnSupportedOperationWhenMetricGroupGetExportDataIsCalledThenErrorIsReturned) {

    MockIEquation10 errorEquation{};
    errorEquation.equationElement.Type = MetricsDiscovery::EQUATION_ELEM_OPERATION;
    errorEquation.equationElement.Operation = MetricsDiscovery::EQUATION_OPER_LAST_1_0;

    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();

    auto metricGroupHandle = getMetricGroupHandle();
    uint8_t dummyRawData = 0;
    size_t exportDataSize = 0;

    std::vector<MetricsDiscovery::IEquation_1_0 **> equationUsages = {
        &informationParams.IoReadEquation,
        &informationParams.QueryReadEquation,
        &metricParams.IoReadEquation,
        &metricParams.QueryReadEquation,
        &metricParams.NormEquation,
        &metricParams.MaxValueEquation,
    };

    for (auto &equation : equationUsages) {
        auto backupEquation = *equation;
        *equation = &errorEquation;
        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                                 &dummyRawData, 1, &exportDataSize, nullptr),
                  ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
        EXPECT_EQ(exportDataSize, 0u);
        *equation = backupEquation;
    }
}

TEST_F(MetricExportDataOaTest, givenDeltaFunctionsWithUnSupportedTypesWhenMetricGroupGetExportDataIsCalledThenErrorIsReturned) {

    MetricsDiscovery::TDeltaFunction_1_0 errorDeltaFunction{};
    errorDeltaFunction.FunctionType = MetricsDiscovery::DELTA_FUNCTION_LAST_1_0;
    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();

    auto metricGroupHandle = getMetricGroupHandle();
    uint8_t dummyRawData = 0;
    size_t exportDataSize = 0;

    std::vector<MetricsDiscovery::TDeltaFunction_1_0 *> deltaFunctionUsages = {
        &informationParams.OverflowFunction,
        &metricParams.DeltaFunction};

    for (auto &deltaFunction : deltaFunctionUsages) {
        auto backupDeltaFunction = *deltaFunction;
        *deltaFunction = errorDeltaFunction;
        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                                 &dummyRawData, 1, &exportDataSize, nullptr),
                  ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
        EXPECT_EQ(exportDataSize, 0u);
        *deltaFunction = backupDeltaFunction;
    }
}

TEST_F(MetricExportDataOaTest, givenUnSupportedEnumerationsWhenMetricGroupGetExportDataIsCalledThenErrorIsReturned) {

    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();

    auto metricGroupHandle = getMetricGroupHandle();
    uint8_t dummyRawData = 0;
    size_t exportDataSize = 0;

    std::vector<std::pair<int32_t *, int32_t>> enumUsages = {
        {reinterpret_cast<int32_t *>(&informationParams.InfoType), static_cast<int32_t>(MetricsDiscovery::INFORMATION_TYPE_LAST)},
        {reinterpret_cast<int32_t *>(&metricParams.MetricType), static_cast<int32_t>(MetricsDiscovery::TMetricType::METRIC_TYPE_LAST)},
        {reinterpret_cast<int32_t *>(&metricParams.ResultType), static_cast<int32_t>(MetricsDiscovery::TMetricResultType::RESULT_LAST)},
    };

    for (auto &[enumeration, errorValue] : enumUsages) {
        auto backupenumeration = *enumeration;
        *enumeration = errorValue;
        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                                 &dummyRawData, 1, &exportDataSize, nullptr),
                  ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
        EXPECT_EQ(exportDataSize, 0u);
        *enumeration = backupenumeration;
    }
}

TEST_F(MetricExportDataOaTest, givenUnsupportedInformationParamsWhenMetricGroupGetExportDataIsCalledThenErrorIsReturned) {

    MetricsDiscovery::TInformationParams_1_0 invalidInformationParams{};
    Mock<IInformation_1_0> invalidInformation{};
    invalidInformation.GetParamsResult = &invalidInformationParams;
    setupInformationParams(invalidInformationParams, equation, deltaFunction);
    invalidInformationParams.InfoType = MetricsDiscovery::INFORMATION_TYPE_LAST;

    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();

    auto metricGroupHandle = getMetricGroupHandle();
    uint8_t dummyRawData = 0;
    size_t exportDataSize = 0;

    {
        metricsConcurrentGroup.GetIoMeasurementInformationResult = &invalidInformation;
        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                                 &dummyRawData, 1, &exportDataSize, nullptr),
                  ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
        EXPECT_EQ(exportDataSize, 0u);
        metricsConcurrentGroup.GetIoMeasurementInformationResult = &information;
    }

    {
        metricsConcurrentGroup.GetIoGpuContextInformationResult = &invalidInformation;
        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                                 &dummyRawData, 1, &exportDataSize, nullptr),
                  ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
        EXPECT_EQ(exportDataSize, 0u);
        metricsConcurrentGroup.GetIoGpuContextInformationResult = &information;
    }

    {
        metricsSet.GetInformationResult = &invalidInformation;
        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                                 &dummyRawData, 1, &exportDataSize, nullptr),
                  ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
        EXPECT_EQ(exportDataSize, 0u);
        metricsSet.GetInformationResult = &information;
    }
}

TEST_F(MetricExportDataOaTest, givenEquationInNullWhenMetricGroupGetExportDataIsCalledThenErrorIsNotReturned) {
    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();
    auto metricGroupHandle = getMetricGroupHandle();

    uint8_t dummyRawData = 0;
    size_t exportDataSize = 0;
    const auto dummyRawDataSize = 1u;
    metricParams.IoReadEquation = nullptr;
    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, dummyRawDataSize, &exportDataSize, nullptr),
              ZE_RESULT_SUCCESS);
    EXPECT_GT(exportDataSize, 0u);
    std::vector<uint8_t> exportData(exportDataSize);
    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, dummyRawDataSize, &exportDataSize, exportData.data()),
              ZE_RESULT_SUCCESS);
}

TEST_F(MetricExportDataOaTest, givenSupportedVersionWhenMetricGroupGetExportDataIsCalledThenAvailabilityEquationIsNotInvalid) {
    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();

    auto verifyAvailabilityEquation = [&]() {
        uint8_t dummyRawData = 0;
        size_t exportDataSize = 0;
        const auto dummyRawDataSize = 1u;
        auto metricGroupHandle = getMetricGroupHandle();

        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                                 &dummyRawData, dummyRawDataSize, &exportDataSize, nullptr),
                  ZE_RESULT_SUCCESS);
        EXPECT_GT(exportDataSize, 0u);
        std::vector<uint8_t> exportData(exportDataSize);
        EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                                 &dummyRawData, dummyRawDataSize, &exportDataSize, exportData.data()),
                  ZE_RESULT_SUCCESS);
        auto base = reinterpret_cast<zet_intel_metric_df_gpu_export_data_format_t *>(exportData.data());
        auto oaData = &base->format01.oaData;
        auto availabilityStringPtr = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, oaData->metricSet.params.availabilityEquation, base);
        EXPECT_STREQ(availabilityStringPtr, testString);
    };

    metricsDeviceParams.Version.MajorNumber = 2;
    metricsDeviceParams.Version.MinorNumber = 0;
    verifyAvailabilityEquation();

    metricsDeviceParams.Version.MajorNumber = 1;
    metricsDeviceParams.Version.MinorNumber = 11;
    verifyAvailabilityEquation();

    metricsDeviceParams.Version.MajorNumber = 1;
    metricsDeviceParams.Version.MinorNumber = 12;
    verifyAvailabilityEquation();
}

TEST_F(MetricExportDataOaTest, givenUnSupportedVersionWhenMetricGroupGetExportDataIsCalledThenAvailabilityEquationIsInvalid) {
    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();

    uint8_t dummyRawData = 0;
    size_t exportDataSize = 0;
    const auto dummyRawDataSize = 1u;
    auto metricGroupHandle = getMetricGroupHandle();

    metricsDeviceParams.Version.MajorNumber = 0;
    metricsDeviceParams.Version.MinorNumber = 1;

    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, dummyRawDataSize, &exportDataSize, nullptr),
              ZE_RESULT_SUCCESS);
    EXPECT_GT(exportDataSize, 0u);
    std::vector<uint8_t> exportData(exportDataSize);
    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, dummyRawDataSize, &exportDataSize, exportData.data()),
              ZE_RESULT_SUCCESS);
    auto base = reinterpret_cast<zet_intel_metric_df_gpu_export_data_format_t *>(exportData.data());
    auto oaData = &base->format01.oaData;
    EXPECT_EQ(oaData->metricSet.params.availabilityEquation, ZET_INTEL_GPU_METRIC_INVALID_OFFSET);

    metricsDeviceParams.Version.MajorNumber = 1;
    metricsDeviceParams.Version.MinorNumber = 10;

    dummyRawData = 0;
    exportDataSize = 0;

    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, dummyRawDataSize, &exportDataSize, nullptr),
              ZE_RESULT_SUCCESS);
    EXPECT_GT(exportDataSize, 0u);
    exportData.resize(exportDataSize);
    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, dummyRawDataSize, &exportDataSize, exportData.data()),
              ZE_RESULT_SUCCESS);
    base = reinterpret_cast<zet_intel_metric_df_gpu_export_data_format_t *>(exportData.data());
    oaData = &base->format01.oaData;
    EXPECT_EQ(oaData->metricSet.params.availabilityEquation, ZET_INTEL_GPU_METRIC_INVALID_OFFSET);
}

TEST_F(MetricExportDataOaTest, givenIncorrectExportSizeIsPassedWhenMetricGroupGetExportDataIsCalledThenErrorIsReturned) {
    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();
    auto metricGroupHandle = getMetricGroupHandle();

    uint8_t dummyRawData = 0;
    size_t exportDataSize = 0;
    const auto dummyRawDataSize = 1u;
    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, dummyRawDataSize, &exportDataSize, nullptr),
              ZE_RESULT_SUCCESS);
    EXPECT_GT(exportDataSize, 0u);
    std::vector<uint8_t> exportData(exportDataSize);
    exportDataSize -= 1;
    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, dummyRawDataSize, &exportDataSize, exportData.data()),
              ZE_RESULT_ERROR_INVALID_SIZE);
}

TEST_F(MetricExportDataOaTest, givenMetricSetNameAsNullPtrWhenMetricGroupGetExportDataIsCalledNoErrorIsReturned) {
    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();
    auto metricGroupHandle = getMetricGroupHandle();

    uint8_t dummyRawData = 0;
    size_t exportDataSize = 0;
    const auto dummyRawDataSize = 1u;
    metricsSetParams.SymbolName = nullptr;
    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, dummyRawDataSize, &exportDataSize, nullptr),
              ZE_RESULT_SUCCESS);
    EXPECT_GT(exportDataSize, 0u);
    std::vector<uint8_t> exportData(exportDataSize);
    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, dummyRawDataSize, &exportDataSize, exportData.data()),
              ZE_RESULT_SUCCESS);
}

TEST_F(MetricExportDataOaTest, givenErrorDuringExportingPhaseWhenMetricGroupGetExportDataIsCalledThenErrorIsReturned) {

    MetricsDiscovery::TDeltaFunction_1_0 errorDeltaFunction{};
    errorDeltaFunction.FunctionType = MetricsDiscovery::DELTA_FUNCTION_LAST_1_0;
    setupMdapiParameters();
    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);
    setupMocks();

    auto metricGroupHandle = getMetricGroupHandle();
    uint8_t dummyRawData = 0;
    size_t exportDataSize = 0;
    const auto dummyRawDataSize = 1u;
    metricsSetParams.SymbolName = nullptr;
    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, dummyRawDataSize, &exportDataSize, nullptr),
              ZE_RESULT_SUCCESS);
    EXPECT_GT(exportDataSize, 0u);
    informationParams.OverflowFunction = errorDeltaFunction;
    std::vector<uint8_t> exportData(exportDataSize);
    EXPECT_EQ(zetMetricGroupGetExportDataExp(metricGroupHandle,
                                             &dummyRawData, dummyRawDataSize, &exportDataSize, exportData.data()),
              ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
}

} // namespace ult
} // namespace L0
