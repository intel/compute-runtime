/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/metrics/metric.h"

struct Event;

namespace L0 {

struct MetricTracerImp : MetricTracer {
    ~MetricTracerImp() override{};

    ze_result_t readData(uint32_t maxReportCount, size_t *pRawDataSize, uint8_t *pRawData) override;
    ze_result_t close() override;

    ze_result_t initialize(ze_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup);
    ze_result_t startMeasurements(uint32_t &notifyEveryNReports, uint32_t &samplingPeriodNs, ze_event_handle_t hNotificationEvent);
    Event::State getNotificationState() override;

  protected:
    ze_result_t stopMeasurements();
    uint32_t getOaBufferSize(const uint32_t notifyEveryNReports) const;
    uint32_t getNotifyEveryNReports(const uint32_t oaBufferSize) const;
    uint32_t getRequiredBufferSize(const uint32_t maxReportCount) const;

    ze_device_handle_t hDevice = nullptr;
    zet_metric_group_handle_t hMetricGroup = nullptr;
    Event *pNotificationEvent = nullptr;
    uint32_t rawReportSize = 0;
    uint32_t oaBufferSize = 0;
};

} // namespace L0