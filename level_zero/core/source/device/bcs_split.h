/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/transfer_direction.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/event/event.h"

#include <functional>
#include <mutex>
#include <variant>
#include <vector>

namespace NEO {
class CommandStreamReceiver;
enum class TransferDirection;
} // namespace NEO

namespace L0 {
struct CommandQueue;
struct DeviceImp;
class BcsSplit;

namespace BcsSplitParams {
struct MemCopy {
    void *dst = nullptr;
    const void *src = nullptr;
};

struct RegionCopy {
    // originXParams
    uint32_t dst = 0;
    uint32_t src = 0;
};

using CopyParams = std::variant<MemCopy, RegionCopy>;
} // namespace BcsSplitParams

struct BcsSplitEvents {
    BcsSplit &bcsSplit;

    std::mutex mtx;
    std::vector<EventPool *> pools;
    std::vector<Event *> barrier;
    std::vector<Event *> subcopy;
    std::vector<Event *> marker;
    std::vector<void *> allocsForAggregatedEvents;
    size_t currentAggregatedAllocOffset = 0;
    size_t createdFromLatestPool = 0u;
    bool aggregatedEventsMode = false;

    std::optional<size_t> obtainForSplit(Context *context, size_t maxEventCountInPool);
    size_t obtainAggregatedEventsForSplit(Context *context);
    void resetEventPackage(size_t index);
    void resetAggregatedEventState(size_t index, bool markerCompleted);
    void releaseResources();
    bool allocatePool(Context *context, size_t maxEventCountInPool, size_t neededEvents);
    std::optional<size_t> createFromPool(Context *context, size_t maxEventCountInPool);
    size_t createAggregatedEvent(Context *context);
    uint64_t *getNextAllocationForAggregatedEvent();

    BcsSplitEvents(BcsSplit &bcsSplit) : bcsSplit(bcsSplit) {}
};

class BcsSplit {
  public:
    template <GFXCORE_FAMILY gfxCoreFamily>
    using AppendCallFuncT = std::function<ze_result_t(CommandListCoreFamilyImmediate<gfxCoreFamily> *, const BcsSplitParams::CopyParams &, size_t, ze_event_handle_t, uint64_t)>;

    template <typename GfxFamily>
    static constexpr size_t maxEventCountInPool = MemoryConstants::pageSize64k / sizeof(typename GfxFamily::TimestampPacketType);

    static constexpr size_t csrContainerSize = 12;

    using CsrContainer = StackVec<NEO::CommandStreamReceiver *, csrContainerSize>;
    using CmdListsForSplitContainer = StackVec<L0::CommandList *, csrContainerSize>;

    BcsSplitEvents events;

    std::vector<CommandList *> cmdLists;
    std::vector<CommandList *> h2dCmdLists;
    std::vector<CommandList *> d2hCmdLists;

    template <GFXCORE_FAMILY gfxCoreFamily>
    ze_result_t appendSplitCall(CommandListCoreFamilyImmediate<gfxCoreFamily> *cmdList,
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
    DeviceImp &getDevice() const { return device; }

    CmdListsForSplitContainer getCmdListsForSplit(NEO::TransferDirection direction, size_t totalTransferSize);

    BcsSplit(DeviceImp &device) : events(*this), device(device){};

  protected:
    std::vector<CommandList *> &selectCmdLists(NEO::TransferDirection direction);
    uint64_t getAggregatedEventIncrementValForSplit(const Event *signalEvent, bool useSignalEventForSplit, size_t engineCountForSplit);
    void setupEnginesMask();
    bool setupQueues();

    DeviceImp &device;
    NEO::BcsSplitSettings splitSettings = {};
    uint32_t clientCount = 0u;

    std::mutex mtx;
};

} // namespace L0

#include "level_zero/core/source/device/bcs_split.inl"
