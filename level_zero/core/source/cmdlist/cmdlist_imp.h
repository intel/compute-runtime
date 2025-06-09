/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/os_interface/os_time.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"

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

    ze_result_t getDeviceHandle(ze_device_handle_t *phDevice) override;
    ze_result_t getContextHandle(ze_context_handle_t *phContext) override;
    ze_result_t getOrdinal(uint32_t *pOrdinal) override;
    ze_result_t getImmediateIndex(uint32_t *pIndex) override;
    ze_result_t isImmediate(ze_bool_t *pIsImmediate) override;

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
    void enableSynchronizedDispatch(NEO::SynchronizedDispatchMode mode);
    NEO::SynchronizedDispatchMode getSynchronizedDispatchMode() const { return synchronizedDispatchMode; }
    void enableCopyOperationOffload();
    void setInterruptEventsCsr(NEO::CommandStreamReceiver &csr);
    virtual bool kernelMemoryPrefetchEnabled() const = 0;

  protected:
    std::shared_ptr<NEO::InOrderExecInfo> inOrderExecInfo;
    NEO::SynchronizedDispatchMode synchronizedDispatchMode = NEO::SynchronizedDispatchMode::disabled;
    uint32_t syncDispatchQueueId = std::numeric_limits<uint32_t>::max();

    ~CommandListImp() override = default;

    static constexpr bool cmdListDefaultCoherency = false;
    static constexpr bool cmdListDefaultDisableOverdispatch = true;
    static constexpr bool cmdListDefaultPipelineSelectModeSelected = true;
    static constexpr bool cmdListDefaultMediaSamplerClockGate = false;
    static constexpr bool cmdListDefaultGlobalAtomics = false;
    std::vector<Event *> mappedTsEventList;
    std::vector<Event *> interruptEvents;
};

} // namespace L0
