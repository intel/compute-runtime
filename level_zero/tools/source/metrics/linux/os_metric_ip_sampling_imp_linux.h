/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/os_interface_metric.h"

namespace L0 {

class MetricIpSamplingLinuxImp : public MetricIpSamplingOsInterface {
  public:
    MetricIpSamplingLinuxImp(Device &device);
    ~MetricIpSamplingLinuxImp() override = default;
    ze_result_t startMeasurement(uint32_t &notifyEveryNReports, uint32_t &samplingPeriodNs) override;
    ze_result_t stopMeasurement() override;
    ze_result_t readData(uint8_t *pRawData, size_t *pRawDataSize) override;
    uint32_t getRequiredBufferSize(const uint32_t maxReportCount) override;
    uint32_t clampNReports(uint32_t notifyEveryNReports);
    uint32_t getUnitReportSize() override;
    bool isNReportsAvailable() override;
    bool isDependencyAvailable() override;
    ze_result_t getMetricsTimerResolution(uint64_t &timerResolution) override;

  private:
    int32_t stream = -1;
    Device &device;
};

} // namespace L0
