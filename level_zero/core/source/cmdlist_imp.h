/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdlist.h"
#include "level_zero/core/source/device.h"

namespace L0 {

struct CommandListImp : CommandList {
    CommandListImp() {}
    CommandListImp(uint32_t numIddsPerBlock) : CommandList(numIddsPerBlock) {}
    ze_result_t destroy() override;

    ze_result_t appendMetricMemoryBarrier() override;
    ze_result_t appendMetricTracerMarker(zet_metric_tracer_handle_t hMetricTracer,
                                         uint32_t value) override;
    ze_result_t appendMetricQueryBegin(zet_metric_query_handle_t hMetricQuery) override;
    ze_result_t appendMetricQueryEnd(zet_metric_query_handle_t hMetricQuery,
                                     ze_event_handle_t hCompletionEvent) override;

  protected:
    ~CommandListImp() override = default;
};

} // namespace L0
