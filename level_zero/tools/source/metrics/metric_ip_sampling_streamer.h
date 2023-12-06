/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"

namespace L0 {

class IpSamplingMetricSourceImp;

struct IpSamplingMetricStreamerBase : public MetricStreamer {
    ze_result_t appendStreamerMarker(CommandList &commandList, uint32_t value) override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }
};

struct IpSamplingMetricStreamerImp : public IpSamplingMetricStreamerBase {

    IpSamplingMetricStreamerImp(IpSamplingMetricSourceImp &ipSamplingSource) : ipSamplingSource(ipSamplingSource) {}
    ~IpSamplingMetricStreamerImp() override{};
    ze_result_t readData(uint32_t maxReportCount, size_t *pRawDataSize, uint8_t *pRawData) override;
    ze_result_t close() override;
    Event::State getNotificationState() override;
    uint32_t getMaxSupportedReportCount();

  protected:
    IpSamplingMetricSourceImp &ipSamplingSource;
};

struct MultiDeviceIpSamplingMetricStreamerImp : public IpSamplingMetricStreamerBase {

    MultiDeviceIpSamplingMetricStreamerImp(std::vector<IpSamplingMetricStreamerImp *> subDeviceStreamers) : subDeviceStreamers(subDeviceStreamers) {}
    ~MultiDeviceIpSamplingMetricStreamerImp() override = default;
    ze_result_t readData(uint32_t maxReportCount, size_t *pRawDataSize, uint8_t *pRawData) override;
    ze_result_t close() override;
    Event::State getNotificationState() override;

  protected:
    std::vector<IpSamplingMetricStreamerImp *> subDeviceStreamers = {};
};

} // namespace L0
