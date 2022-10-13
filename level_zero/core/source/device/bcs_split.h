/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

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

        std::vector<EventPool *> pools;
        std::vector<Event *> subcopy;
        std::vector<Event *> marker;
        size_t createdFromLatestPool = 0u;

        size_t obtainForSplit(Context *context, size_t maxEventCountInPool);
        size_t allocateNew(Context *context, size_t maxEventCountInPool);

        void releaseResources();

        Events(BcsSplit &bcsSplit) : bcsSplit(bcsSplit){};
    } events;

    std::vector<CommandQueue *> cmdQs;
    NEO::BcsInfoMask engines = NEO::EngineHelpers::oddLinkedCopyEnginesMask;

    template <GFXCORE_FAMILY gfxCoreFamily, typename T, typename K>
    ze_result_t appendSplitCall(CommandListCoreFamilyImmediate<gfxCoreFamily> *cmdList,
                                T dstptr,
                                K srcptr,
                                size_t size,
                                ze_event_handle_t hSignalEvent,
                                std::function<ze_result_t(T, K, size_t, ze_event_handle_t)> appendCall) {
        ze_result_t result = ZE_RESULT_SUCCESS;

        if (hSignalEvent) {
            cmdList->appendEventForProfilingAllWalkers(Event::fromHandle(hSignalEvent), true);
        }

        auto markerEventIndex = this->events.obtainForSplit(Context::fromHandle(cmdList->hContext), MemoryConstants::pageSize64k / sizeof(typename CommandListCoreFamilyImmediate<gfxCoreFamily>::GfxFamily::TimestampPacketType));
        auto subcopyEventIndex = markerEventIndex * this->cmdQs.size();
        StackVec<ze_event_handle_t, 4> eventHandles;

        auto totalSize = size;
        auto engineCount = this->cmdQs.size();
        for (size_t i = 0; i < this->cmdQs.size(); i++) {
            auto localSize = totalSize / engineCount;
            auto localDstPtr = ptrOffset(dstptr, size - totalSize);
            auto localSrcPtr = ptrOffset(srcptr, size - totalSize);

            auto eventHandle = this->events.subcopy[subcopyEventIndex + i]->toHandle();
            result = appendCall(localDstPtr, localSrcPtr, localSize, eventHandle);
            cmdList->executeCommandListImmediateImpl(true, this->cmdQs[i]);
            eventHandles.push_back(eventHandle);

            totalSize -= localSize;
            engineCount--;
        }

        cmdList->addEventsToCmdList(static_cast<uint32_t>(this->cmdQs.size()), eventHandles.data());
        cmdList->appendEventForProfilingAllWalkers(this->events.marker[markerEventIndex], false);

        if (hSignalEvent) {
            cmdList->appendEventForProfilingAllWalkers(Event::fromHandle(hSignalEvent), false);
        }

        return result;
    }

    bool setupDevice(uint32_t productFamily, bool internalUsage, const ze_command_queue_desc_t *desc, NEO::CommandStreamReceiver *csr);
    void releaseResources();

    BcsSplit(DeviceImp &device) : device(device), events(*this){};
};

} // namespace L0