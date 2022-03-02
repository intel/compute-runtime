/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/os_metric_ip_sampling.h"

namespace L0 {

class IpSamplingMetricSourceImp;

struct IpSamplingMetricStreamerImp : MetricStreamer {

    IpSamplingMetricStreamerImp(IpSamplingMetricSourceImp &ipSamplingSource) : ipSamplingSource(ipSamplingSource) {}
    ~IpSamplingMetricStreamerImp() override{};
    ze_result_t readData(uint32_t maxReportCount, size_t *pRawDataSize, uint8_t *pRawData) override;
    ze_result_t close() override;
    Event::State getNotificationState() override;
    ze_result_t appendStreamerMarker(CommandList &commandList, uint32_t value) override;
    void attachEvent(ze_event_handle_t hNotificationEvent);
    void detachEvent();

  protected:
    Event *pNotificationEvent = nullptr;
    IpSamplingMetricSourceImp &ipSamplingSource;
};

} // namespace L0
