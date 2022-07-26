/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/cmdlist/cmdlist.h"

#include <memory>

namespace NEO {
class LogicalStateHelper;
}

namespace L0 {

struct CommandListImp : CommandList {
    using CommandList::CommandList;

    ze_result_t destroy() override;

    ze_result_t appendMetricMemoryBarrier() override;
    ze_result_t appendMetricStreamerMarker(zet_metric_streamer_handle_t hMetricStreamer,
                                           uint32_t value) override;
    ze_result_t appendMetricQueryBegin(zet_metric_query_handle_t hMetricQuery) override;
    ze_result_t appendMetricQueryEnd(zet_metric_query_handle_t hMetricQuery, ze_event_handle_t hSignalEvent,
                                     uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;
    virtual void appendMultiPartitionPrologue(uint32_t partitionDataSize) = 0;
    virtual void appendMultiPartitionEpilogue() = 0;

    virtual NEO::LogicalStateHelper *getLogicalStateHelper() const { return nonImmediateLogicalStateHelper.get(); }

  protected:
    std::unique_ptr<NEO::LogicalStateHelper> nonImmediateLogicalStateHelper;

    ~CommandListImp() override = default;
};

} // namespace L0
