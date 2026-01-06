/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/metrics/metric.h"

struct Event;

namespace L0 {

struct OaMetricStreamerImp : MetricStreamer {
    ~OaMetricStreamerImp() override{};

    ze_result_t readData(uint32_t maxReportCount, size_t *pRawDataSize, uint8_t *pRawData) override;
    ze_result_t close() override;

    ze_result_t initialize(ze_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup);
    ze_result_t updateStreamerDescriptionAndStartMeasurements(zet_metric_streamer_desc_t &desc, const bool isNotificationEnabled);
    Event::State getNotificationState() override;

    ze_result_t appendStreamerMarker(CommandList &commandList, uint32_t value) override;
    std::vector<zet_metric_streamer_handle_t> &getMetricStreamers();

  protected:
    ze_result_t stopMeasurements();
    uint32_t getOaBufferSizeForNotification(const uint32_t notifyEveryNReports) const;
    uint32_t getOaBufferSizeForReports(const uint32_t maxReportCount) const;
    uint32_t getNotifyEveryNReports(const uint32_t oaBufferSize) const;
    uint32_t getRequiredBufferSize(const uint32_t maxReportCount) const;

    ze_device_handle_t hDevice = nullptr;
    zet_metric_group_handle_t hMetricGroup = nullptr;
    Event *pNotificationEvent = nullptr;
    uint32_t rawReportSize = 0;
    uint32_t oaBufferSize = 0;
    std::vector<zet_metric_streamer_handle_t> metricStreamers;
};

} // namespace L0
