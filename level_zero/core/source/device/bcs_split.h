/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/transfer_direction.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/event/event.h"

#include <functional>
#include <mutex>
#include <vector>

namespace NEO {
class CommandStreamReceiver;
enum class TransferDirection;
} // namespace NEO

namespace L0 {
struct CommandQueue;
struct Device;
class BcsSplit;

class BcsSplitEvents {
  public:
    BcsSplitEvents(BcsSplit &bcsSplit) : bcsSplit(bcsSplit) {}

    bool isAggregatedEventMode() const { return aggregatedEventsMode; }
    void setAggregatedEventMode(bool mode) { aggregatedEventsMode = mode; }
    std::optional<size_t> obtainForImmediateSplit(Context *context, size_t maxEventCountInPool);
    void releaseResources();
    std::lock_guard<std::mutex> obtainLock() { return std::lock_guard<std::mutex>(mtx); }
    void resetAggregatedEventState(size_t index, bool markerCompleted);
    const BcsSplitParams::EventsResources &getEventResources() const { return eventResources; }

  protected:
    size_t obtainAggregatedEventsForSplit(Context *context);
    void resetEventPackage(size_t index);
    bool allocatePool(Context *context, size_t maxEventCountInPool, size_t neededEvents);
    std::optional<size_t> createFromPool(Context *context, size_t maxEventCountInPool);
    size_t createAggregatedEvent(Context *context);
    uint64_t *getNextAllocationForAggregatedEvent();

    BcsSplit &bcsSplit;

    BcsSplitParams::EventsResources eventResources;

    std::mutex mtx;
    bool aggregatedEventsMode = false;
};

class BcsSplit {
  public:
    template <GFXCORE_FAMILY gfxCoreFamily>
    using AppendCallFuncT = std::function<ze_result_t(CommandListCoreFamily<gfxCoreFamily> *, const BcsSplitParams::CopyParams &, size_t, ze_event_handle_t, uint64_t)>;

    template <typename GfxFamily>
    static constexpr size_t maxEventCountInPool = MemoryConstants::pageSize64k / sizeof(typename GfxFamily::TimestampPacketType);

    BcsSplitEvents events;

    std::vector<CommandList *> cmdLists;
    std::vector<CommandList *> h2dCmdLists;
    std::vector<CommandList *> d2hCmdLists;

    template <GFXCORE_FAMILY gfxCoreFamily>
    ze_result_t appendImmediateSplitCall(CommandListCoreFamilyImmediate<gfxCoreFamily> *cmdList,
                                         const BcsSplitParams::CopyParams &copyParams,
                                         size_t size,
                                         ze_event_handle_t hSignalEvent,
                                         uint32_t numWaitEvents,
                                         ze_event_handle_t *phWaitEvents,
                                         bool performMigration,
                                         bool hasRelaxedOrderingDependencies,
                                         NEO::TransferDirection direction,
                                         size_t estimatedCmdBufferSize,
                                         AppendCallFuncT<gfxCoreFamily> appendCall);

    bool setupDevice(NEO::CommandStreamReceiver *csr, bool copyOffloadEnabled);
    void releaseResources();
    Device &getDevice() const { return device; }

    BcsSplitParams::CmdListsForSplitContainer getCmdListsForSplit(NEO::TransferDirection direction, size_t totalTransferSize);

    BcsSplit(Device &device) : events(*this), device(device){};

  protected:
    std::vector<CommandList *> &selectCmdLists(NEO::TransferDirection direction);
    uint64_t getAggregatedEventIncrementValForSplit(const Event *signalEvent, bool useSignalEventForSplit, size_t engineCountForSplit);
    void setupEnginesMask();
    bool setupQueues();

    Device &device;
    NEO::BcsSplitSettings splitSettings = {};
    uint32_t clientCount = 0u;

    std::mutex mtx;
};

} // namespace L0

#include "level_zero/core/source/device/bcs_split.inl"
