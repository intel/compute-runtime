/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_time.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/helpers/in_order_cmd_helpers.h"

#include <memory>

namespace L0 {

struct CommandListImp : public CommandList {
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

    void setStreamPropertiesDefaultSettings(NEO::StreamProperties &streamProperties);
    void enableInOrderExecution();
    bool isInOrderExecutionEnabled() const { return inOrderExecInfo.get(); }
    void storeReferenceTsToMappedEvents(bool clear);
    void addToMappedEventList(Event *event);
    const std::vector<Event *> &peekMappedEventList() { return mappedTsEventList; }
    void addRegularCmdListSubmissionCounter();
    virtual void patchInOrderCmds() = 0;

  protected:
    std::shared_ptr<InOrderExecInfo> inOrderExecInfo;
    uint32_t inOrderAllocationOffset = 0;

    ~CommandListImp() override = default;

    virtual bool inOrderAtomicSignallingEnabled() const = 0;
    bool duplicatedInOrderCounterStorageEnabled() const;

    static constexpr int32_t cmdListDefaultEngineInstancedDevice = NEO::StreamProperty::initValue;
    static constexpr bool cmdListDefaultCoherency = false;
    static constexpr bool cmdListDefaultDisableOverdispatch = true;
    static constexpr bool cmdListDefaultPipelineSelectModeSelected = true;
    static constexpr bool cmdListDefaultMediaSamplerClockGate = false;
    static constexpr bool cmdListDefaultGlobalAtomics = false;
    std::vector<Event *> mappedTsEventList{};
};

} // namespace L0
