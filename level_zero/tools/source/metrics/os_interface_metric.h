/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"

#include <level_zero/zet_api.h>

#include <memory>

namespace L0 {

struct Device;
class MetricOsInterface {
  public:
    virtual ~MetricOsInterface() = default;
    virtual ze_result_t getMetricsTimerResolution(uint64_t &timerResolution) = 0;
    static std::unique_ptr<MetricOsInterface> create(Device &device);
};

class MetricIpSamplingOsInterface : public MetricOsInterface {
  public:
    ~MetricIpSamplingOsInterface() override = default;
    virtual ze_result_t startMeasurement(uint32_t &notifyEveryNReports, uint32_t &samplingPeriodNs) = 0;
    virtual ze_result_t stopMeasurement() = 0;
    virtual ze_result_t readData(uint8_t *pRawData, size_t *pRawDataSize) = 0;
    virtual uint32_t getRequiredBufferSize(const uint32_t maxReportCount) = 0;
    virtual uint32_t getUnitReportSize() = 0;
    virtual bool isNReportsAvailable() = 0;
    virtual bool isDependencyAvailable() = 0;
    static std::unique_ptr<MetricIpSamplingOsInterface> create(Device &device);

    uint32_t maxDssBufferSize = 512 * MemoryConstants::kiloByte;
    uint32_t defaultPollPeriodNs = 10000000u;
    uint32_t unitReportSize = 64u;
};

} // namespace L0
