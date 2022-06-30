/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zet_api.h>

#include <memory>

namespace L0 {

struct Device;

class MetricIpSamplingOsInterface {
  public:
    virtual ~MetricIpSamplingOsInterface() = default;
    virtual ze_result_t startMeasurement(uint32_t &notifyEveryNReports, uint32_t &samplingPeriodNs) = 0;
    virtual ze_result_t stopMeasurement() = 0;
    virtual ze_result_t readData(uint8_t *pRawData, size_t *pRawDataSize) = 0;
    virtual uint32_t getRequiredBufferSize(const uint32_t maxReportCount) = 0;
    virtual uint32_t getUnitReportSize() = 0;
    virtual bool isNReportsAvailable() = 0;
    virtual bool isDependencyAvailable() = 0;
    static std::unique_ptr<MetricIpSamplingOsInterface> create(Device &device);
};

} // namespace L0
