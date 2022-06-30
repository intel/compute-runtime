/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/metrics/os_metric_ip_sampling.h"

namespace L0 {

class MetricIpSamplingWindowsImp : public MetricIpSamplingOsInterface {
  public:
    MetricIpSamplingWindowsImp() {}
    ze_result_t startMeasurement(uint32_t &notifyEveryNReports, uint32_t &samplingPeriodNs) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t stopMeasurement() override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t readData(uint8_t *pRawData, size_t *pRawDataSize) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    uint32_t getRequiredBufferSize(const uint32_t maxReportCount) override {
        return 0u;
    }
    uint32_t getUnitReportSize() override {
        return 0u;
    }
    bool isNReportsAvailable() override {
        return false;
    }
    bool isDependencyAvailable() override {
        return false;
    }
};

std::unique_ptr<MetricIpSamplingOsInterface> MetricIpSamplingOsInterface::create(Device &device) {
    return std::make_unique<MetricIpSamplingWindowsImp>();
}

} // namespace L0
