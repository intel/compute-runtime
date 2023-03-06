/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/transfer_direction.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/sku_info/sku_info_base.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/event/event.h"

#include <functional>
#include <mutex>
#include <vector>

namespace NEO {
class CommandStreamReceiver;
}

namespace L0 {
struct CommandQueue;
struct DeviceImp;

struct BcsSplit {
    DeviceImp &device;
    uint32_t clientCount = 0u;

    std::mutex mtx;

    struct Events {
        BcsSplit &bcsSplit;

        std::mutex mtx;
        std::vector<EventPool *> pools;
        std::vector<Event *> barrier;
        std::vector<Event *> subcopy;
        std::vector<Event *> marker;
        size_t createdFromLatestPool = 0u;

        size_t obtainForSplit(Context *context, size_t maxEventCountInPool);
        size_t allocateNew(Context *context, size_t maxEventCountInPool);

        void releaseResources();

        Events(BcsSplit &bcsSplit) : bcsSplit(bcsSplit){};
    } events;

    std::vector<CommandQueue *> cmdQs;
    std::vector<CommandQueue *> h2dCmdQs;
    std::vector<CommandQueue *> d2hCmdQs;

    inline static constexpr size_t h2dEngineMask = 0b010001000;
    inline static constexpr size_t d2hEngineMask = 0b000100010;
    NEO::BcsInfoMask engines = NEO::EngineHelpers::oddLinkedCopyEnginesMask;
    NEO::BcsInfoMask h2dEngines = h2dEngineMask;
    NEO::BcsInfoMask d2hEngines = d2hEngineMask;

    template <GFXCORE_FAMILY gfxCoreFamily, typename T, typename K>
    ze_result_t appendSplitCall(CommandListCoreFamilyImmediate<gfxCoreFamily> *cmdList,
                                T dstptr,
                                K srcptr,
                                size_t size,
                                ze_event_handle_t hSignalEvent,
                                uint32_t numWaitEvents,
                                ze_event_handle_t *phWaitEvents,
                                bool performMigration,
                                bool hasRelaxedOrderingDependencies,
                                NEO::TransferDirection direction,
                                std::function<ze_result_t(T, K, size_t, ze_event_handle_t)> appendCall) {
        ze_result_t result = ZE_RESULT_SUCCESS;

        auto markerEventIndex = this->events.obtainForSplit(Context::fromHandle(cmdList->getCmdListContext()), MemoryConstants::pageSize64k / sizeof(typename CommandListCoreFamilyImmediate<gfxCoreFamily>::GfxFamily::TimestampPacketType));

        auto barrierRequired = cmdList->isBarrierRequired();
        if (barrierRequired) {
            cmdList->appendSignalEvent(this->events.barrier[markerEventIndex]->toHandle());
        }

        auto subcopyEventIndex = markerEventIndex * this->cmdQs.size();
        StackVec<ze_event_handle_t, 4> eventHandles;

        auto &cmdQsForSplit = this->getCmdQsForSplit(direction);

        auto totalSize = size;
        auto engineCount = cmdQsForSplit.size();
        for (size_t i = 0; i < cmdQsForSplit.size(); i++) {
            if (barrierRequired) {
                auto barrierEventHandle = this->events.barrier[markerEventIndex]->toHandle();
                cmdList->addEventsToCmdList(1u, &barrierEventHandle, hasRelaxedOrderingDependencies, false);
            }

            cmdList->addEventsToCmdList(numWaitEvents, phWaitEvents, hasRelaxedOrderingDependencies, false);
            if (hSignalEvent && i == 0u) {
                cmdList->appendEventForProfilingAllWalkers(Event::fromHandle(hSignalEvent), true, true);
            }

            auto localSize = totalSize / engineCount;
            auto localDstPtr = ptrOffset(dstptr, size - totalSize);
            auto localSrcPtr = ptrOffset(srcptr, size - totalSize);

            auto eventHandle = this->events.subcopy[subcopyEventIndex + i]->toHandle();
            result = appendCall(localDstPtr, localSrcPtr, localSize, eventHandle);

            if (cmdList->flushTaskSubmissionEnabled()) {
                cmdList->executeCommandListImmediateWithFlushTaskImpl(performMigration, false, hasRelaxedOrderingDependencies, cmdQsForSplit[i]);
            } else {
                cmdList->executeCommandListImmediateImpl(performMigration, cmdQsForSplit[i]);
            }

            eventHandles.push_back(eventHandle);

            totalSize -= localSize;
            engineCount--;
        }

        cmdList->addEventsToCmdList(static_cast<uint32_t>(cmdQsForSplit.size()), eventHandles.data(), hasRelaxedOrderingDependencies, false);
        if (hSignalEvent) {
            cmdList->appendEventForProfilingAllWalkers(Event::fromHandle(hSignalEvent), false, true);
        }
        cmdList->appendEventForProfilingAllWalkers(this->events.marker[markerEventIndex], false, true);

        return result;
    }

    bool setupDevice(uint32_t productFamily, bool internalUsage, const ze_command_queue_desc_t *desc, NEO::CommandStreamReceiver *csr);
    void releaseResources();
    std::vector<CommandQueue *> &getCmdQsForSplit(NEO::TransferDirection direction);

    BcsSplit(DeviceImp &device) : device(device), events(*this){};
};

} // namespace L0