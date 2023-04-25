/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/test_macros/test.h"

#include "level_zero/tools/source/metrics/os_interface_metric.h"

namespace L0 {
namespace ult {

class MockMetricIpSamplingOsInterface : public MetricIpSamplingOsInterface {

  public:
    ze_result_t startMeasurementReturn = ZE_RESULT_SUCCESS;
    ze_result_t stopMeasurementReturn = ZE_RESULT_SUCCESS;
    ze_result_t readDataReturn = ZE_RESULT_SUCCESS;
    ze_result_t getMetricsTimerResolutionReturn = ZE_RESULT_SUCCESS;
    uint32_t getUnitReportSizeReturn = 64;
    bool isNReportsAvailableReturn = true;
    bool isDependencyAvailableReturn = true;
    bool isfillDataEnabled = false;
    uint8_t fillData = 1;
    size_t fillDataSize = 0;

    ~MockMetricIpSamplingOsInterface() override = default;
    ze_result_t startMeasurement(uint32_t &notifyEveryNReports, uint32_t &samplingPeriodNs) override {
        return startMeasurementReturn;
    }
    ze_result_t stopMeasurement() override {
        return stopMeasurementReturn;
    }
    ze_result_t readData(uint8_t *pRawData, size_t *pRawDataSize) override {
        if (isfillDataEnabled) {
            auto fillSize = std::min(fillDataSize, *pRawDataSize);
            memset(pRawData, fillData, fillSize);
            *pRawDataSize = fillSize;
        }
        return readDataReturn;
    }
    uint32_t getRequiredBufferSize(const uint32_t maxReportCount) override {
        return maxReportCount * getUnitReportSizeReturn;
    }
    uint32_t getUnitReportSize() override {
        return getUnitReportSizeReturn;
    }
    bool isNReportsAvailable() override {
        return isNReportsAvailableReturn;
    }
    bool isDependencyAvailable() override {
        return isDependencyAvailableReturn;
    }

    ze_result_t getMetricsTimerResolution(uint64_t &timerResolution) override {
        timerResolution = 1000;
        return getMetricsTimerResolutionReturn;
    }
};

} // namespace ult
} // namespace L0
