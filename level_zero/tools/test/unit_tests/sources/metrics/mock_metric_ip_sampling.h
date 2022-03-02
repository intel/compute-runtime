/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/test_macros/test.h"

#include "level_zero/tools/source/metrics/os_metric_ip_sampling.h"

namespace L0 {
namespace ult {

class MockMetricIpSamplingOsInterface : public MetricIpSamplingOsInterface {

  public:
    ze_result_t startMeasurementReturn = ZE_RESULT_SUCCESS;
    ze_result_t stopMeasurementReturn = ZE_RESULT_SUCCESS;
    ze_result_t readDataReturn = ZE_RESULT_SUCCESS;
    uint32_t getRequiredBufferSizeReturn = 100;
    uint32_t getUnitReportSizeReturn = 64;
    bool isNReportsAvailableReturn = true;
    bool isDependencyAvailableReturn = true;

    ~MockMetricIpSamplingOsInterface() override = default;
    ze_result_t startMeasurement(uint32_t &notifyEveryNReports, uint32_t &samplingPeriodNs) override {
        return startMeasurementReturn;
    }
    ze_result_t stopMeasurement() override {
        return stopMeasurementReturn;
    }
    ze_result_t readData(uint8_t *pRawData, size_t *pRawDataSize) override {
        return readDataReturn;
    }
    uint32_t getRequiredBufferSize(const uint32_t maxReportCount) override {
        return getRequiredBufferSizeReturn;
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
};

} // namespace ult
} // namespace L0
