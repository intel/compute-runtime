/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/metrics/metric_ip_sampling_fixture.h"
namespace L0 {
namespace ult {

using MetricIpSamplingCalcOpSingleDeviceTest = MetricIpSamplingCalculateSingleDevFixture;

HWTEST2_F(MetricIpSamplingCalcOpSingleDeviceTest, GivenIpSamplingCalcOpCallingMetricCalculateValuesOnSubDeviceThenSuccessIsReturned, EustallSupportedPlatforms) {

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));
    uint32_t metricsInReportCount = 10;
    std::vector<zet_metric_handle_t> metricsInReport(metricsInReportCount);
    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesInReport(metricsInReportCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation,
                                                                                      &metricsInReportCount,
                                                                                      metricsInReport.data(),
                                                                                      metricScopesInReport.data()));
    EXPECT_EQ(metricsInReportCount, 10u);
    std::vector<zet_metric_properties_t> metricProperties(metricsInReportCount);

    for (uint32_t i = 0; i < metricsInReportCount; i++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGetProperties(metricsInReport[i], &metricProperties[i]));
    }

    uint32_t totalMetricReportCount = 0;
    bool final = true;
    size_t usedSize = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(rawReportsBytesSize, reinterpret_cast<uint8_t *>(rawReports.data()),
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, nullptr));
    EXPECT_EQ(totalMetricReportCount, 3U);
    std::vector<zet_intel_metric_result_exp_t> metricResults(totalMetricReportCount * metricsInReportCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(rawReportsBytesSize, reinterpret_cast<uint8_t *>(rawReports.data()),
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, metricResults.data()));
    EXPECT_EQ(totalMetricReportCount, 3U);
    EXPECT_EQ(usedSize, rawReportsBytesSize);

    for (uint32_t i = 0; i < totalMetricReportCount; i++) {
        for (uint32_t j = 0; j < metricsInReportCount; j++) {
            uint32_t resultIndex = i * metricsInReportCount + j;
            EXPECT_TRUE(metricProperties[j].resultType == expectedMetricValues[resultIndex].type);
            EXPECT_TRUE(metricResults[resultIndex].value.ui64 == expectedMetricValues[resultIndex].value.ui64);
            EXPECT_TRUE(metricResults[resultIndex].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID);
        }
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

HWTEST2_F(MetricIpSamplingCalcOpSingleDeviceTest, GivenIpSamplingCalcOpCallingMetricCalculateValuesOnSubDeviceThenHandleErrorForRootDeviceData, EustallSupportedPlatforms) {

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));

    std::vector<uint8_t> rawDataWithHeader(rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader));
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 0);

    uint32_t totalMetricReportCount = 0;
    bool final = true;
    size_t usedSize = 0;
    // sub-device calcOp does not accept root device data
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculateValuesExp(rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                                 hCalculationOperation,
                                                                                 final, &usedSize,
                                                                                 &totalMetricReportCount, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

HWTEST2_F(MetricIpSamplingCalcOpSingleDeviceTest, GivenIpSamplingCalcOpCallingMetricCalculateValuesOnSubDeviceThenHandleInvalidDataSize, EustallSupportedPlatforms) {

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));

    std::vector<uint8_t> rawDataWithHeader(rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader));
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 0);

    uint32_t totalMetricReportCount = 0;
    bool final = true;
    size_t usedSize = 0;

    // Handle invalid input raw data size when getting size
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, zetIntelMetricCalculateValuesExp(rawReportsBytesSize / 30, reinterpret_cast<uint8_t *>(rawReports.data()),
                                                                             hCalculationOperation,
                                                                             final, &usedSize,
                                                                             &totalMetricReportCount, nullptr));
    // Handle invalid input raw data size when getting values
    std::vector<zet_intel_metric_result_exp_t> metricResults(1);
    totalMetricReportCount = 1;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, zetIntelMetricCalculateValuesExp(rawReportsBytesSize / 30, reinterpret_cast<uint8_t *>(rawReports.data()),
                                                                             hCalculationOperation,
                                                                             final, &usedSize,
                                                                             &totalMetricReportCount, metricResults.data()));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

HWTEST2_F(MetricIpSamplingCalcOpSingleDeviceTest, GivenIpSamplingCalcOpCallingMetricCalculateValuesOnSubDeviceFilterMetricsInReport, EustallSupportedPlatforms) {

    uint32_t metricCount = 0;
    EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCount, 10u);
    std::vector<zet_metric_handle_t> phMetrics(metricCount);
    EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, phMetrics.data()), ZE_RESULT_SUCCESS);

    std::vector<zet_metric_handle_t> metricsToCalculate;

    // Select only odd index metrics. According to expectedMetricNamesInReport there are:
    // "Active", "PipeStall" "DistStall",  "SyncStall", "OtherStall"
    for (uint32_t i = 0; i < metricCount; i++) {
        if (i % 2) {
            metricsToCalculate.push_back(phMetrics[i]);
        }
    }

    uint32_t metricsToCalculateCount = static_cast<uint32_t>(metricsToCalculate.size());
    EXPECT_EQ(metricsToCalculateCount, 5u);

    calculationDesc.metricGroupCount = 0;
    calculationDesc.phMetricGroups = nullptr;
    calculationDesc.metricCount = metricsToCalculateCount;
    calculationDesc.phMetrics = metricsToCalculate.data();

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));
    uint32_t metricsInReportCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation, &metricsInReportCount, nullptr, nullptr));
    EXPECT_EQ(metricsInReportCount, 5u);
    std::vector<zet_metric_handle_t> metricsInReport(metricsInReportCount);
    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesInReport(metricsInReportCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation, &metricsInReportCount,
                                                                                      metricsInReport.data(), metricScopesInReport.data()));

    EXPECT_EQ(metricsInReportCount, 5u);
    // Expect only odd index metrics in the result report
    zet_metric_properties_t ipSamplingMetricProperties = {};
    for (uint32_t i = 0; i < metricsInReportCount; i++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGetProperties(metricsInReport[i], &ipSamplingMetricProperties));
        EXPECT_EQ(strcmp(ipSamplingMetricProperties.name, expectedMetricNamesInReport[i * 2 + 1].c_str()), 0);
        EXPECT_EQ(static_cast<MockMetricScope *>(MetricScope::fromHandle(metricScopesInReport[i])), hMockScope);
    }

    // Only use the first raw reports, so can easily verify the results for each metric
    uint8_t *rawdata = reinterpret_cast<uint8_t *>(rawReports.data());
    size_t dataSize = IpSamplingCalculation::rawReportSize;

    uint32_t totalMetricReportCount = 0;
    bool final = true;
    size_t usedSize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataSize, rawdata,
                                                                  hCalculationOperation,
                                                                  final, &usedSize,
                                                                  &totalMetricReportCount, nullptr));
    EXPECT_EQ(totalMetricReportCount, 1U);
    EXPECT_EQ(usedSize, 0U); // query only, no data processed

    std::vector<zet_intel_metric_result_exp_t> metricResults(totalMetricReportCount * metricsInReportCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataSize, rawdata,
                                                                  hCalculationOperation,
                                                                  final, &usedSize,
                                                                  &totalMetricReportCount, metricResults.data()));
    EXPECT_EQ(totalMetricReportCount, 1U);
    EXPECT_EQ(usedSize, IpSamplingCalculation::rawReportSize);

    // Expect only odd index metrics results of the first report
    for (uint32_t j = 0; j < metricsInReportCount; j++) {
        EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesIP1FirstRawReport[j * 2 + 1].value.ui64);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

HWTEST2_F(MetricIpSamplingCalcOpSingleDeviceTest, GivenIpSamplingCallingMetricCalculateValuesOnSubDeviceCanRequestFewerValuesThanAvailable, EustallSupportedPlatforms) {

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));
    uint32_t metricsInReportCount = 10;
    std::vector<zet_metric_handle_t> metricsInReport(metricsInReportCount);
    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesInReport(metricsInReportCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation,
                                                                                      &metricsInReportCount,
                                                                                      metricsInReport.data(),
                                                                                      metricScopesInReport.data()));
    EXPECT_EQ(metricsInReportCount, 10u);

    uint32_t totalMetricReportCount = 0;
    bool final = false;
    size_t usedSize = 0;
    uint8_t *rawdata = reinterpret_cast<uint8_t *>(rawReports.data());
    size_t dataBytesSize = rawReportsBytesSize;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataBytesSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, nullptr));
    EXPECT_EQ(totalMetricReportCount, 3U);
    EXPECT_EQ(usedSize, 0U); // query only, no data processed

    // Request only one report even if three are available
    totalMetricReportCount = 1;
    std::vector<zet_intel_metric_result_exp_t> metricResults(metricsInReportCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataBytesSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, metricResults.data()));
    EXPECT_EQ(totalMetricReportCount, 1U);
    EXPECT_EQ(usedSize, IpSamplingCalculation::rawReportSize);

    for (uint32_t j = 0; j < expectedMetricValuesIP1.size(); j++) {
        EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesIP1[j].value.ui64);
    }

    dataBytesSize -= usedSize;
    rawdata += usedSize;
    totalMetricReportCount = 0; // query again available reports
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataBytesSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, nullptr));

    EXPECT_EQ(totalMetricReportCount, 2U); // two reports are available, cached

    // Request only one report
    totalMetricReportCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataBytesSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, metricResults.data()));
    EXPECT_EQ(totalMetricReportCount, 1U);
    EXPECT_EQ(usedSize, IpSamplingCalculation::rawReportSize);

    for (uint32_t j = 0; j < expectedMetricValuesIP10.size(); j++) {
        EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesIP10[j].value.ui64);
    }

    dataBytesSize -= usedSize;
    rawdata += usedSize;
    totalMetricReportCount = 0; // query again available reports
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataBytesSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, nullptr));
    EXPECT_EQ(totalMetricReportCount, 1U); // one report is available, cached

    // Request only one report
    totalMetricReportCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataBytesSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, metricResults.data()));
    EXPECT_EQ(totalMetricReportCount, 1U);
    EXPECT_EQ(usedSize, dataBytesSize);

    for (uint32_t j = 0; j < expectedMetricValuesIP100.size(); j++) {
        EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesIP100[j].value.ui64);
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

HWTEST2_F(MetricIpSamplingCalcOpSingleDeviceTest, GivenIpSamplingCallingMetricCalculateValuesOnSubDeviceCanRequestFewerValuesThanAvailableFinalTrue, EustallSupportedPlatforms) {

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));
    uint32_t metricsInReportCount = 10;
    std::vector<zet_metric_handle_t> metricsInReport(metricsInReportCount);
    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesInReport(metricsInReportCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation,
                                                                                      &metricsInReportCount,
                                                                                      metricsInReport.data(),
                                                                                      metricScopesInReport.data()));
    EXPECT_EQ(metricsInReportCount, 10u);

    uint32_t totalMetricReportCount = 0;
    bool final = true;
    size_t usedSize = 0;
    uint8_t *rawdata = reinterpret_cast<uint8_t *>(rawReports.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(rawReportsBytesSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, nullptr));
    EXPECT_EQ(totalMetricReportCount, 3U);
    EXPECT_EQ(usedSize, 0U); // query only, no data processed

    // Request only one report even if three are available
    totalMetricReportCount = 1;
    std::vector<zet_intel_metric_result_exp_t> metricResults(metricsInReportCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(rawReportsBytesSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, metricResults.data()));
    EXPECT_EQ(totalMetricReportCount, 1U);
    // Setting final to true processes all data available
    EXPECT_EQ(usedSize, rawReportsBytesSize);

    for (uint32_t j = 0; j < expectedMetricValuesIP1.size(); j++) {
        EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesIP1[j].value.ui64);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

HWTEST2_F(MetricIpSamplingCalcOpSingleDeviceTest, GivenIpSamplingCallingMetricCalculateValuesOnSubDeviceCanConcatenateData, EustallSupportedPlatforms) {

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));
    uint32_t metricsInReportCount = 10;
    std::vector<zet_metric_handle_t> metricsInReport(metricsInReportCount);
    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesInReport(metricsInReportCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation,
                                                                                      &metricsInReportCount,
                                                                                      metricsInReport.data(),
                                                                                      metricScopesInReport.data()));
    EXPECT_EQ(metricsInReportCount, 10u);

    uint32_t totalMetricReportCount = 0;
    bool final = false;
    size_t usedSize = 0;
    uint8_t *rawdata = reinterpret_cast<uint8_t *>(rawReports.data());
    size_t dataSize = 2 * IpSamplingCalculation::rawReportSize; // Only use the first two raw reports

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, nullptr));
    EXPECT_EQ(totalMetricReportCount, 2U); // IP1 first raw report, IP10 second raw report
    EXPECT_EQ(usedSize, 0U);               // query only, no data processed

    // Request only one report even if two are available, leave result for IP 10 (second raw report) cached
    totalMetricReportCount = 1;
    std::vector<zet_intel_metric_result_exp_t> metricResults(metricsInReportCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, metricResults.data()));
    EXPECT_EQ(totalMetricReportCount, 1U);
    EXPECT_EQ(usedSize, IpSamplingCalculation::rawReportSize); // If cached results remain, used size will be a single raw report

    for (uint32_t j = 0; j < expectedMetricValuesIP1FirstRawReport.size(); j++) {
        EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesIP1FirstRawReport[j].value.ui64);
    }

    // Advance already processed data and simulate appending
    rawdata += usedSize; // now points to second raw report
    // simulate appending one new raw report at the end. Third of rawReports
    dataSize = 2 * IpSamplingCalculation::rawReportSize;

    totalMetricReportCount = 0; // query again available reports
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, nullptr));

    EXPECT_EQ(totalMetricReportCount, 2U); // IP 10 cached plus new IP 1 from third raw report

    // Request only one report
    totalMetricReportCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, metricResults.data()));
    EXPECT_EQ(totalMetricReportCount, 1U);
    EXPECT_EQ(usedSize, IpSamplingCalculation::rawReportSize); // If cached results remain, used size will be a single raw report

    // Once again expect to find results for IP1, but this time from the third raw report.
    for (uint32_t j = 0; j < expectedMetricValuesIP1thirdRawReport.size(); j++) {
        EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesIP1thirdRawReport[j].value.ui64);
    }

    // Advance already processed data and simulate appending
    rawdata += usedSize; // now points to the third raw report
    // simulate appending two new raw reports at the end. Fourth and fifth of rawReports
    dataSize = 3 * IpSamplingCalculation::rawReportSize;

    totalMetricReportCount = 0; // query again available reports
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, nullptr));
    EXPECT_EQ(totalMetricReportCount, 2U); // IP 10 cached and also new from forth report and IP 100 from fifth raw report

    // Request only one report
    totalMetricReportCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, metricResults.data()));
    EXPECT_EQ(totalMetricReportCount, 1U);
    EXPECT_EQ(usedSize, IpSamplingCalculation::rawReportSize); // If cached results remain, used size will be a single raw report

    // Results from IP 10 should be the sum of the values cached plus new data found
    for (uint32_t j = 0; j < expectedMetricValuesIP10.size(); j++) {
        EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesIP10[j].value.ui64);
    }

    // Advance already processed data and simulate appending
    rawdata += usedSize; // // now points to the fourth raw report
    // simulate appending one new raw report at the end. sixth of rawReports
    // Note: since previously appended two but progressed only one, now the new size is 3 raw reports
    dataSize = 3 * IpSamplingCalculation::rawReportSize;

    totalMetricReportCount = 0; // query again available reports
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, nullptr));
    EXPECT_EQ(totalMetricReportCount, 1U); // IP100, cached and from the sixth raw report

    // Request only one report
    totalMetricReportCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, metricResults.data()));
    EXPECT_EQ(totalMetricReportCount, 1U);
    EXPECT_EQ(usedSize, dataSize); // all cached data consumed, usedSize should be equal to input size

    // Results from IP 100 should be the sum of the values cached plus new data found
    for (uint32_t j = 0; j < expectedMetricValuesIP100.size(); j++) {
        EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesIP100[j].value.ui64);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

HWTEST2_F(MetricIpSamplingCalcOpSingleDeviceTest, GivenIpSamplingCallingMetricCalculateValuesOnSubDeviceCachesareFreed, EustallSupportedPlatforms) {

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));
    uint32_t metricsInReportCount = 10;
    std::vector<zet_metric_handle_t> metricsInReport(metricsInReportCount);
    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesInReport(metricsInReportCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation,
                                                                                      &metricsInReportCount,
                                                                                      metricsInReport.data(),
                                                                                      metricScopesInReport.data()));
    EXPECT_EQ(metricsInReportCount, 10u);

    uint32_t totalMetricReportCount = 0;
    bool final = false;
    size_t usedSize = 0;
    uint8_t *rawdata = reinterpret_cast<uint8_t *>(rawReports.data());
    size_t dataBytesSize = rawReportsBytesSize;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataBytesSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, nullptr));
    EXPECT_EQ(totalMetricReportCount, 3U);

    // Request only one report even if three are available
    totalMetricReportCount = 1;
    std::vector<zet_intel_metric_result_exp_t> metricResults(metricsInReportCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(dataBytesSize, rawdata,
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, metricResults.data()));
    EXPECT_EQ(totalMetricReportCount, 1U);
    EXPECT_EQ(usedSize, IpSamplingCalculation::rawReportSize);

    // 2 IPs remain cached.
    // Destroying calcOp should free the cached IP results
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

using MetricIpSamplingCalcAggregationTest = MetricIpSamplingMetricsAggregationMultiDevFixture;

HWTEST2_F(MetricIpSamplingCalcAggregationTest, GivenIpSamplingCalcOpOnRootDeviceCalculatingOnSingleDataReadExpectSuccess, EustallSupportedPlatforms) {

    // Raw data for a single read with different data for sub-device 0 and 1
    size_t rawDataSize = sizeof(IpSamplingMultiDevDataHeader) + rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader) + rawReports2BytesSize;
    std::vector<uint8_t> rawDataWithHeader(rawDataSize);
    // sub device index 0
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 0);
    // sub device index 1
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data() + rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader),
                                            rawDataWithHeader.size() - (rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader)),
                                            reinterpret_cast<uint8_t *>(rawReports2.data()), rawReports2BytesSize, 1);
    /*
        uint32_t totalMetricReportCount = 0;
        bool final = false;
        size_t usedSize = 0;
        uint32_t metricsInReportCount = 0;
        zet_metric_properties_t ipSamplingMetricProperties = {};
        std::vector<zet_metric_handle_t> metricsInReport = {};
        std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesInReport = {};
        std::vector<zet_intel_metric_result_exp_t> metricResults = {};

        for (auto &calcOp : hCalcOps) {
            metricsInReportCount = 0;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(calcOp, &metricsInReportCount, nullptr, nullptr));
            // Expect 10 metrics per scope included in the calcOp
            if (calcOp == hCalcOpCompScope1 || calcOp == hCalcOpCompScope2 || calcOp == hCalcOpAggScope) {
                EXPECT_EQ(metricsInReportCount, 10u);
            } else if (calcOp == hCalcOpAllCompScopes) {
                EXPECT_EQ(metricsInReportCount, 20u);
            } else if (calcOp == hCalcOpAllScopes) {
                EXPECT_EQ(metricsInReportCount, 30u);
            }

            metricsInReport.resize(metricsInReportCount);
            metricScopesInReport.resize(metricsInReportCount);
            EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(calcOp,
                                                                                              &metricsInReportCount,
                                                                                              metricsInReport.data(),
                                                                                              metricScopesInReport.data()));
            for (uint32_t i = 0; i < metricsInReportCount; i++) {
                EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGetProperties(metricsInReport[i], &ipSamplingMetricProperties));
                EXPECT_EQ(strcmp(ipSamplingMetricProperties.name, expectedMetricNamesInReport[i % expectedMetricNamesInReport.size()].c_str()), 0);

                if (calcOp == hCalcOpCompScope1) {
                    EXPECT_EQ(static_cast<MockMetricScope *>(MetricScope::fromHandle(metricScopesInReport[i])), mockMetricScopeCompute0);
                } else if (calcOp == hCalcOpCompScope2) {
                    EXPECT_EQ(static_cast<MockMetricScope *>(MetricScope::fromHandle(metricScopesInReport[i])), mockMetricScopeCompute1);
                } else if (calcOp == hCalcOpAllCompScopes) {
                    if (i < 10) {
                        EXPECT_EQ(static_cast<MockMetricScope *>(MetricScope::fromHandle(metricScopesInReport[i])), mockMetricScopeCompute0);
                    } else {
                        EXPECT_EQ(static_cast<MockMetricScope *>(MetricScope::fromHandle(metricScopesInReport[i])), mockMetricScopeCompute1);
                    }
                } else if (calcOp == hCalcOpAggScope) {
                    EXPECT_EQ(static_cast<MockMetricScope *>(MetricScope::fromHandle(metricScopesInReport[i])), mockMetricScopeAggregated);
                } else if (calcOp == hCalcOpAllScopes) {
                    if (i < 10) {
                        // Expect aggregated scope first
                        EXPECT_EQ(static_cast<MockMetricScope *>(MetricScope::fromHandle(metricScopesInReport[i])), mockMetricScopeAggregated);
                    } else if (i >= 10 && i < 20) {
                        EXPECT_EQ(static_cast<MockMetricScope *>(MetricScope::fromHandle(metricScopesInReport[i])), mockMetricScopeCompute0);
                    } else {
                        EXPECT_EQ(static_cast<MockMetricScope *>(MetricScope::fromHandle(metricScopesInReport[i])), mockMetricScopeCompute1);
                    }
                }
            }

            totalMetricReportCount = 0;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(rawDataSize, reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                          calcOp, final, &usedSize,
                                                                          &totalMetricReportCount, nullptr));
            EXPECT_EQ(usedSize, 0U); // query only, no data processed
            if (calcOp == hCalcOpCompScope1 || calcOp == hCalcOpAllCompScopes) {
                // three IPs in raw data for sub-dev 0 (rawReports)
                EXPECT_EQ(totalMetricReportCount, 3U);
            } else if (calcOp == hCalcOpCompScope2) {
                // two IPs in raw data for sub-dev 1 (rawReports2)
                EXPECT_EQ(totalMetricReportCount, 2U);
            } else if (calcOp == hCalcOpAggScope || calcOp == hCalcOpAllScopes) {
                // When aggregated scope is included expect unique IPs from both sub-devices: IP1, IP2, IP10, IP100
                EXPECT_EQ(totalMetricReportCount, 4U);
            }

            EXPECT_EQ(usedSize, 0U); // query only, no data processed
            metricResults.resize(totalMetricReportCount * metricsInReportCount);
            EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(rawDataSize, reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                          calcOp, final, &usedSize,
                                                                          &totalMetricReportCount, metricResults.data()));
            EXPECT_EQ(usedSize, rawDataSize);
            if (calcOp == hCalcOpCompScope1 || calcOp == hCalcOpAllCompScopes) {
                // three IPs in raw data for sub-dev 0 (rawReports)
                EXPECT_EQ(totalMetricReportCount, 3U);
            } else if (calcOp == hCalcOpCompScope2) {
                // two IPs in raw data for sub-dev 1 (rawReports2)
                EXPECT_EQ(totalMetricReportCount, 2U);
            } else if (calcOp == hCalcOpAggScope || calcOp == hCalcOpAllScopes) {
                // When aggregated scope is included expect unique IPs from both sub-devices: IP1, IP2, IP10, IP100
                EXPECT_EQ(totalMetricReportCount, 4U);
            }

            // Data for sub dev 0 is from rawReports so expected results are from expectedMetricValues for IP1, IP10 and IP100
            for (uint32_t i = 0; i < totalMetricReportCount; i++) {
                for (uint32_t j = 0; j < metricsInReportCount; j++) {
                    uint32_t resultIndex = i * metricsInReportCount + j;
                    if (calcOp == hCalcOpCompScope1) {
                        EXPECT_TRUE(metricResults[resultIndex].value.ui64 == expectedMetricValues[resultIndex].value.ui64);
                        EXPECT_TRUE(metricResults[resultIndex].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID);
                    } else if (calcOp == hCalcOpCompScope2) {
                        EXPECT_TRUE(metricResults[resultIndex].value.ui64 == expectedMetricValues2[resultIndex].value.ui64);
                        EXPECT_TRUE(metricResults[resultIndex].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID);
                    } else if (calcOp == hCalcOpAllCompScopes) {
                        EXPECT_TRUE(metricResults[resultIndex].value.ui64 == expectedMetricValuesCalcOpAllCompScopes[resultIndex]);
                        if (resultIndex < 50) {
                            // First two result reports have valid entries in compute caches
                            EXPECT_TRUE(metricResults[resultIndex].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID);
                        } else {
                            // Third result report does not have IPs for sub-dev 1 in the cache: 3 IPs for subdev 0, 2 IPs for subdev 1
                            EXPECT_TRUE(metricResults[resultIndex].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID);
                        }

                    } else if (calcOp == hCalcOpAggScope) {
                        // Expected results are the aggregation of both sub-devices
                        EXPECT_TRUE(metricResults[resultIndex].value.ui64 == expectedMetricValuesCalcOpAggScope[resultIndex]);
                        EXPECT_TRUE(metricResults[resultIndex].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID);
                    } else if (calcOp == hCalcOpAllScopes) {
                        EXPECT_TRUE(metricResults[resultIndex].value.ui64 == expectedMetricValuesCalcOpAllScopes[resultIndex]);
                        // Sub-dev 0 has 3 IPs, sub-dev 1 has 2 IPs, aggregated scope has 4 IPs
                        if (resultIndex < 80) {
                            // First two reports: all results are valid.
                            // Third result report: aggregated scope cache has IPs, sub-dev 0 has IPs
                            EXPECT_TRUE(metricResults[resultIndex].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID);
                        } else if (resultIndex >= 80 && resultIndex < 90) {
                            // Third result report: sub-dev 1 cache ran out of IPs
                            EXPECT_TRUE(metricResults[resultIndex].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID);
                        } else if (resultIndex >= 90 && resultIndex < 100) {
                            // Fourth result report: only aggregated scope cache has IPs
                            EXPECT_TRUE(metricResults[resultIndex].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID);
                        } else {
                            // Fourth result report: both sub-devices caches ran out of IPs
                            EXPECT_TRUE(metricResults[resultIndex].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID);
                        }
                    }
                }
            }
        }
            */
}

HWTEST2_F(MetricIpSamplingCalcAggregationTest, GivenIpSamplingCalcOpOnRootDeviceCalculatingConcatenatedDataExpectSuccess, EustallSupportedPlatforms) {

    // Raw data for two reads of two sub-devices
    size_t rawDataSize = sizeof(IpSamplingMultiDevDataHeader) + rawReportsBytesSize +
                         sizeof(IpSamplingMultiDevDataHeader) + rawReports2BytesSize +
                         sizeof(IpSamplingMultiDevDataHeader) + rawReportsAppendBytesSize +
                         sizeof(IpSamplingMultiDevDataHeader) + rawReportsAppendBytesSize;

    std::vector<uint8_t> rawDataWithHeader(rawDataSize);
    uint8_t *readRawDataStart = reinterpret_cast<uint8_t *>(rawDataWithHeader.data());
    size_t availableSize = rawDataSize;
    // First read for both sub-devices
    // sub device index 0
    MockRawDataHelper::addMultiSubDevHeader(readRawDataStart, availableSize,
                                            reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 0);
    readRawDataStart += sizeof(IpSamplingMultiDevDataHeader) + rawReportsBytesSize;
    availableSize -= sizeof(IpSamplingMultiDevDataHeader) + rawReportsBytesSize;
    // sub device index 1
    MockRawDataHelper::addMultiSubDevHeader(readRawDataStart, availableSize,
                                            reinterpret_cast<uint8_t *>(rawReports2.data()), rawReports2BytesSize, 1);
    readRawDataStart += sizeof(IpSamplingMultiDevDataHeader) + rawReports2BytesSize;
    availableSize -= sizeof(IpSamplingMultiDevDataHeader) + rawReports2BytesSize;

    // Simulate appending second read for both sub-devices
    // sub device index 0
    MockRawDataHelper::addMultiSubDevHeader(readRawDataStart, availableSize,
                                            reinterpret_cast<uint8_t *>(rawReportsAppend.data()), rawReportsAppendBytesSize, 0);
    readRawDataStart += sizeof(IpSamplingMultiDevDataHeader) + rawReportsAppendBytesSize;
    availableSize -= sizeof(IpSamplingMultiDevDataHeader) + rawReportsAppendBytesSize;
    // sub device index 1
    MockRawDataHelper::addMultiSubDevHeader(readRawDataStart, availableSize,
                                            reinterpret_cast<uint8_t *>(rawReportsAppend.data()), rawReportsAppendBytesSize, 1);
    readRawDataStart += sizeof(IpSamplingMultiDevDataHeader) + rawReportsAppendBytesSize;
    availableSize -= sizeof(IpSamplingMultiDevDataHeader) + rawReportsAppendBytesSize;

    uint32_t totalMetricReportCount = 0;
    bool final = false;
    size_t usedSize = 0;
    uint32_t metricsInReportCount = 0;
    std::vector<zet_intel_metric_result_exp_t> metricResults = {};
    for (auto &calcOp : hCalcOps) {
        // Process first read
        uint8_t *computeRawDataStart = reinterpret_cast<uint8_t *>(rawDataWithHeader.data());
        size_t computeRawDataSize = sizeof(IpSamplingMultiDevDataHeader) + rawReportsBytesSize +
                                    sizeof(IpSamplingMultiDevDataHeader) + rawReports2BytesSize;

        metricsInReportCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(calcOp, &metricsInReportCount, nullptr, nullptr));
        // Expect 10 metrics per scope included in the calcOp
        if (calcOp == hCalcOpCompScope1 || calcOp == hCalcOpCompScope2 || calcOp == hCalcOpAggScope) {
            EXPECT_EQ(metricsInReportCount, 10u);
        } else if (calcOp == hCalcOpAllCompScopes) {
            EXPECT_EQ(metricsInReportCount, 20u);
        } else if (calcOp == hCalcOpAllScopes) {
            EXPECT_EQ(metricsInReportCount, 30u);
        }

        totalMetricReportCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(computeRawDataSize, computeRawDataStart,
                                                                      calcOp, final, &usedSize,
                                                                      &totalMetricReportCount, nullptr));
        EXPECT_EQ(usedSize, 0U); // query only, no data processed
        if (calcOp == hCalcOpCompScope1 || calcOp == hCalcOpAllCompScopes) {
            // three IPs in raw data for sub-dev 0 (rawReports)
            EXPECT_EQ(totalMetricReportCount, 3U);
        } else if (calcOp == hCalcOpCompScope2) {
            // two IPs in raw data for sub-dev 1 (rawReports2)
            EXPECT_EQ(totalMetricReportCount, 2U);
        } else if (calcOp == hCalcOpAggScope || calcOp == hCalcOpAllScopes) {
            // three IPs in raw data for sub-dev 0 (rawReports), one unique IP in raw data for sub-dev 1 (rawReports2)
            EXPECT_EQ(totalMetricReportCount, 4U);
        }

        // Request only one report even if three are available
        totalMetricReportCount = 1;
        metricResults.resize(metricsInReportCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(computeRawDataSize, computeRawDataStart,
                                                                      calcOp, final, &usedSize,
                                                                      &totalMetricReportCount, metricResults.data()));
        EXPECT_EQ(totalMetricReportCount, 1U);
        EXPECT_EQ(usedSize, IpSamplingCalculation::rawReportSize);

        for (uint32_t j = 0; j < metricsInReportCount; j++) {
            if (calcOp == hCalcOpCompScope1) {
                // Data for sub dev 0 is from rawReports so expected results are from expectedMetricValues for IP1
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValues[j].value.ui64);
            } else if (calcOp == hCalcOpCompScope2) {
                // Data for sub dev 1 is from rawReports2 so expected results are from expectedMetricValues2 for IP2
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValues2[j].value.ui64);
            } else if (calcOp == hCalcOpAllCompScopes) {
                // Data for both sub-devices
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesCalcOpAllCompScopes[j]);
            } else if (calcOp == hCalcOpAggScope) {
                // Data for aggregated scope
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesCalcOpAggScope[j]);
            } else if (calcOp == hCalcOpAllScopes) {
                // Data for aggregated and both compute scopes
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesCalcOpAllScopes[j]);
            }

            EXPECT_TRUE(metricResults[j].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID);
        }

        // Progress data after processing the first report
        computeRawDataStart += usedSize;
        computeRawDataSize -= usedSize;

        // Simulate data appending by adjusting the new size for the second read, including both sub-devices
        computeRawDataSize += sizeof(IpSamplingMultiDevDataHeader) + rawReportsAppendBytesSize +
                              sizeof(IpSamplingMultiDevDataHeader) + rawReportsAppendBytesSize;

        // Query again available reports after appending second read
        totalMetricReportCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(computeRawDataSize, computeRawDataStart,
                                                                      calcOp, final, &usedSize,
                                                                      &totalMetricReportCount, nullptr));
        EXPECT_EQ(usedSize, 0U); // query only, no data processed
        if (calcOp == hCalcOpCompScope1 || calcOp == hCalcOpAllCompScopes) {
            // Expect 3, 2 remaining from first (IP10 and IP100 cached) read and 1 new from second read (IP3)
            EXPECT_EQ(totalMetricReportCount, 3U);
        } else if (calcOp == hCalcOpCompScope2) {
            // Expect 2, 1 cached (IP100) and 1 new from second read (IP3)
            EXPECT_EQ(totalMetricReportCount, 2U);
        } else if (calcOp == hCalcOpAggScope || calcOp == hCalcOpAllScopes) {
            // Expect 4, 3 remaining from first read data (IP2, IP10, and IP100 cached) read and 1 new from second read (IP3)
            EXPECT_EQ(totalMetricReportCount, 4U);
        }

        // Request only one report after appending second read
        totalMetricReportCount = 1;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(computeRawDataSize, computeRawDataStart,
                                                                      calcOp, final, &usedSize,
                                                                      &totalMetricReportCount, metricResults.data()));

        EXPECT_EQ(totalMetricReportCount, 1U);
        EXPECT_EQ(usedSize, IpSamplingCalculation::rawReportSize);

        for (uint32_t j = 0; j < metricsInReportCount; j++) {
            if (calcOp == hCalcOpCompScope1 || calcOp == hCalcOpCompScope2 || calcOp == hCalcOpAllCompScopes) {
                // Check results for IP3 from second read. Since the appended data is the same for both sub-devices,
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesIP3[j % expectedMetricValuesIP3.size()]);
            } else if (calcOp == hCalcOpAggScope) {
                // when aggregated scope is included, the next IP to process is IP2
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValues2[j].value.ui64);
            } else if (calcOp == hCalcOpAllScopes) {
                // Expect to find IP 2 for aggregated scope and IP3 for both compute scopes
                if (j < 10) {
                    // Aggregated scope
                    EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValues2[j].value.ui64);
                } else {
                    // Compute scopes
                    EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesIP3[j % 10]);
                }
            }

            EXPECT_TRUE(metricResults[j].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID);
        }

        // Progress data after processing one IP
        computeRawDataStart += usedSize;
        computeRawDataSize -= usedSize;

        // Query again available reports
        totalMetricReportCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(computeRawDataSize, computeRawDataStart,
                                                                      calcOp, final, &usedSize,
                                                                      &totalMetricReportCount, nullptr));
        if (calcOp == hCalcOpCompScope1 || calcOp == hCalcOpAllCompScopes) {
            // Two IPs remaining in cache IP10 and IP100 cached
            EXPECT_EQ(totalMetricReportCount, 2U);
        } else if (calcOp == hCalcOpCompScope2) {
            // One IP remaining in cache IP100
            EXPECT_EQ(totalMetricReportCount, 1U);
        } else if (calcOp == hCalcOpAggScope || calcOp == hCalcOpAllScopes) {
            // Three IPs remaining in cache: IP3, IP10, IP100
            EXPECT_EQ(totalMetricReportCount, 3U);
        }

        // Calculate remaining IPs
        metricResults.resize(metricsInReportCount * totalMetricReportCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(computeRawDataSize, computeRawDataStart,
                                                                      calcOp, final, &usedSize,
                                                                      &totalMetricReportCount, metricResults.data()));

        for (uint32_t i = 0; i < totalMetricReportCount; i++) {
            for (uint32_t j = 0; j < metricsInReportCount; j++) {
                uint32_t resultIndex = i * metricsInReportCount + j;

                if (calcOp == hCalcOpCompScope1) {
                    EXPECT_TRUE(metricResults[resultIndex].value.ui64 == expectedMetricValuesIP10IP100AppendToRawReports[resultIndex]);
                    EXPECT_TRUE(metricResults[resultIndex].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID);
                } else if (calcOp == hCalcOpCompScope2) {
                    EXPECT_TRUE(metricResults[resultIndex].value.ui64 == expectedMetricValuesIP100AppendToRawReports2[resultIndex]);
                    EXPECT_TRUE(metricResults[resultIndex].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID);
                } else if (calcOp == hCalcOpAllCompScopes) {
                    EXPECT_TRUE(metricResults[resultIndex].value.ui64 == expectedMetricValuesAppendToAllComputeScopes[resultIndex]);
                    if (resultIndex < 30) {
                        EXPECT_TRUE(metricResults[resultIndex].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID);
                    } else {
                        // Scope for sub-dev 1 has run out of IPs
                        EXPECT_TRUE(metricResults[resultIndex].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID);
                    }
                } else if (calcOp == hCalcOpAggScope) {
                    EXPECT_TRUE(metricResults[resultIndex].value.ui64 == expectedMetricValuesAppendToAggregatedScope[resultIndex]);
                    EXPECT_TRUE(metricResults[resultIndex].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID);
                } else if (calcOp == hCalcOpAllScopes) {
                    EXPECT_TRUE(metricResults[resultIndex].value.ui64 == expectedMetricResultsAfterAppendForAllScopes[resultIndex].value.ui64);
                    EXPECT_TRUE(metricResults[resultIndex].resultStatus == expectedMetricResultsAfterAppendForAllScopes[resultIndex].resultStatus);
                }
            }
        }
    }
}

HWTEST2_F(MetricIpSamplingCalcAggregationTest, GivenIpSamplingCalcOpOnRootDeviceWhenDestroyingCalcOpsAndCachesHaveDataExpectCachedBeCleared, EustallSupportedPlatforms) {

    // Raw data for a single read with different data for sub-device 0 and 1
    size_t rawDataSize = sizeof(IpSamplingMultiDevDataHeader) + rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader) + rawReports2BytesSize;
    std::vector<uint8_t> rawDataWithHeader(rawDataSize);
    // sub device index 0
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 0);
    // sub device index 1
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data() + rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader),
                                            rawDataWithHeader.size() - (rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader)),
                                            reinterpret_cast<uint8_t *>(rawReports2.data()), rawReports2BytesSize, 1);

    uint32_t totalMetricReportCount = 0;
    bool final = false;
    size_t usedSize = 0;
    uint32_t metricsInReportCount = 0;
    std::vector<zet_metric_handle_t> metricsInReport = {};
    std::vector<zet_intel_metric_result_exp_t> metricResults = {};

    for (auto &calcOp : hCalcOps) {
        metricsInReportCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(calcOp, &metricsInReportCount, nullptr, nullptr));
        metricResults.resize(metricsInReportCount);
        // request only one report even if more are available, this leaves the caches with data
        totalMetricReportCount = 1;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(rawDataSize, reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                      calcOp, final, &usedSize,
                                                                      &totalMetricReportCount, metricResults.data()));
    }

    // Leave the MetricIpSamplingCalcAggregationTest fixture TearDown to destroy calcOps and verify caches are cleared
}

HWTEST2_F(MetricIpSamplingCalcAggregationTest, GivenIpSamplingCalcOpOnRootDeviceWhenUsingCorruptedDataErrorIsHandled, EustallSupportedPlatforms) {

    size_t rawDataSize = (sizeof(IpSamplingMultiDevDataHeader) + rawReportsBytesSize) * 2;
    std::vector<uint8_t> rawDataWithHeader(rawDataSize);
    uint8_t *readRawDataStart = reinterpret_cast<uint8_t *>(rawDataWithHeader.data());
    size_t availableSize = rawDataSize;
    // Raw data read for both sub-devices
    // sub device index 0
    MockRawDataHelper::addMultiSubDevHeader(readRawDataStart, availableSize,
                                            reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 0);
    auto header1 = reinterpret_cast<IpSamplingMultiDevDataHeader *>(readRawDataStart);
    header1->rawDataSize = 0x1; // invalid size

    readRawDataStart += sizeof(IpSamplingMultiDevDataHeader) + rawReportsBytesSize;
    availableSize -= sizeof(IpSamplingMultiDevDataHeader) + rawReportsBytesSize;

    // sub device index 1
    MockRawDataHelper::addMultiSubDevHeader(readRawDataStart, availableSize,
                                            reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 1);
    auto header2 = reinterpret_cast<IpSamplingMultiDevDataHeader *>(readRawDataStart);
    header2->rawDataSize = 0x2; // invalid size

    uint32_t totalMetricReportCount = 0;
    bool final = false;
    size_t usedSize = 0;
    uint32_t metricsInReportCount = 0;
    std::vector<zet_intel_metric_result_exp_t> metricResults = {};

    for (auto &calcOp : hCalcOps) {
        if (calcOp == hCalcOpCompScope2) { // sub-dev 1
            // Make sub-dev 0 have valid data such traversing data can reach sub-dev 1 data
            header1->rawDataSize = static_cast<uint32_t>(rawReportsBytesSize);
        }

        metricsInReportCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(calcOp, &metricsInReportCount, nullptr, nullptr));

        // check for failure getting the size
        totalMetricReportCount = 0;
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, zetIntelMetricCalculateValuesExp(rawDataSize, reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                                 calcOp, final, &usedSize,
                                                                                 &totalMetricReportCount, nullptr));
        EXPECT_EQ(usedSize, 0U);

        metricResults.resize(metricsInReportCount);
        // request only one report even if more are available, this leaves the caches with data. But failure should clear them
        totalMetricReportCount = 1;
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, zetIntelMetricCalculateValuesExp(rawDataSize, reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                                 calcOp, final, &usedSize,
                                                                                 &totalMetricReportCount, metricResults.data()));
        EXPECT_EQ(usedSize, 0U);
        if (calcOp == hCalcOpCompScope2) {
            header1->rawDataSize = 0x1; // restore invalid size for next iterations
        }
    }
}

HWTEST2_F(MetricIpSamplingCalcAggregationTest, GivenIpSamplingCalcOpOnRootDeviceCanCallFinalMidWayAndStartWithNewData, EustallSupportedPlatforms) {

    // Raw data for a single read with different data for sub-device 0 and 1
    size_t rawDataSize = sizeof(IpSamplingMultiDevDataHeader) + rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader) + rawReports2BytesSize;
    std::vector<uint8_t> rawDataWithHeader(rawDataSize);
    // sub device index 0
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 0);
    // sub device index 1
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data() + rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader),
                                            rawDataWithHeader.size() - (rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader)),
                                            reinterpret_cast<uint8_t *>(rawReports2.data()), rawReports2BytesSize, 1);
    uint32_t totalMetricReportCount = 0;
    size_t usedSize = 0;
    uint32_t metricsInReportCount = 0;
    std::vector<zet_metric_handle_t> metricsInReport = {};
    std::vector<zet_intel_metric_result_exp_t> metricResults = {};

    for (auto &calcOp : hCalcOps) {
        metricsInReportCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(calcOp, &metricsInReportCount, nullptr, nullptr));
        metricResults.resize(metricsInReportCount);
        // request only one report even if more are available, this leaves the caches with data
        totalMetricReportCount = 1;
        // set final to true mid-way to clear caches
        bool final = true;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(rawDataSize, reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                      calcOp, final, &usedSize,
                                                                      &totalMetricReportCount, metricResults.data()));
        // forcing final should have cleared the caches, so all data is assumed processed
        EXPECT_EQ(usedSize, rawDataSize);

        for (uint32_t j = 0; j < metricsInReportCount; j++) {
            if (calcOp == hCalcOpCompScope1) {
                // Data for sub dev 0 is from rawReports so expected results are from expectedMetricValues for IP1
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValues[j].value.ui64);
            } else if (calcOp == hCalcOpCompScope2) {
                // Data for sub dev 1 is from rawReports2 so expected results are from expectedMetricValues2 for IP2
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValues2[j].value.ui64);
            } else if (calcOp == hCalcOpAllCompScopes) {
                // Data for both sub-devices
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesCalcOpAllCompScopes[j]);
            } else if (calcOp == hCalcOpAggScope) {
                // Data for aggregated scope
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesCalcOpAggScope[j]);
            } else if (calcOp == hCalcOpAllScopes) {
                // Data for aggregated and both compute scopes
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesCalcOpAllScopes[j]);
            }

            EXPECT_TRUE(metricResults[j].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID);
        }

        // Call again with new data, caches should be empty so results are as if first time processing
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(rawDataSize, reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                      calcOp, final, &usedSize,
                                                                      &totalMetricReportCount, metricResults.data()));
        // forcing final should have cleared the caches, so all data is assumed processed
        EXPECT_EQ(usedSize, rawDataSize);

        for (uint32_t j = 0; j < metricsInReportCount; j++) {
            if (calcOp == hCalcOpCompScope1) {
                // Data for sub dev 0 is from rawReports so expected results are from expectedMetricValues for IP1
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValues[j].value.ui64);
            } else if (calcOp == hCalcOpCompScope2) {
                // Data for sub dev 1 is from rawReports2 so expected results are from expectedMetricValues2 for IP2
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValues2[j].value.ui64);
            } else if (calcOp == hCalcOpAllCompScopes) {
                // Data for both sub-devices
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesCalcOpAllCompScopes[j]);
            } else if (calcOp == hCalcOpAggScope) {
                // Data for aggregated scope
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesCalcOpAggScope[j]);
            } else if (calcOp == hCalcOpAllScopes) {
                // Data for aggregated and both compute scopes
                EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValuesCalcOpAllScopes[j]);
            }

            EXPECT_TRUE(metricResults[j].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID);
        }
    }
}

} // namespace ult
} // namespace L0
