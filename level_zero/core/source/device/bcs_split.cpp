/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/bcs_split.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/os_context.h"

#include "level_zero/core/source/device/device_imp.h"

namespace L0 {

bool BcsSplit::setupDevice(uint32_t productFamily, bool internalUsage, const ze_command_queue_desc_t *desc, NEO::CommandStreamReceiver *csr) {
    auto initializeBcsSplit = this->device.getNEODevice()->isBcsSplitSupported() &&
                              csr->getOsContext().getEngineType() == aub_stream::EngineType::ENGINE_BCS &&
                              !internalUsage;

    if (!initializeBcsSplit) {
        return false;
    }

    std::lock_guard<std::mutex> lock(this->mtx);

    this->clientCount++;

    if (!this->cmdQs.empty()) {
        return true;
    }

    if (NEO::debugManager.flags.SplitBcsMask.get() > 0) {
        this->engines = NEO::debugManager.flags.SplitBcsMask.get();
    }

    StackVec<NEO::CommandStreamReceiver *, 4u> csrs;

    for (uint32_t i = 0; i < NEO::bcsInfoMaskSize; i++) {
        if (this->engines.test(i)) {
            auto engineType = (i == 0u ? aub_stream::EngineType::ENGINE_BCS : aub_stream::EngineType::ENGINE_BCS1 + i - 1);
            auto engine = this->device.getNEODevice()->getNearestGenericSubDevice(0u)->tryGetEngine(static_cast<aub_stream::EngineType>(engineType), NEO::EngineUsage::regular);
            if (!engine) {
                continue;
            }
            auto csr = engine->commandStreamReceiver;
            csrs.push_back(csr);
        }
    }

    if (csrs.size() != this->engines.count()) {
        return false;
    }

    ze_command_queue_desc_t splitDesc;
    memcpy(&splitDesc, desc, sizeof(ze_command_queue_desc_t));
    splitDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    for (const auto &csr : csrs) {
        ze_result_t result;
        auto commandQueue = CommandQueue::create(productFamily, &device, csr, &splitDesc, true, false, true, result);
        UNRECOVERABLE_IF(result != ZE_RESULT_SUCCESS);

        this->cmdQs.push_back(commandQueue);
    }

    if (NEO::debugManager.flags.SplitBcsMaskH2D.get() > 0) {
        this->h2dEngines = NEO::debugManager.flags.SplitBcsMaskH2D.get();
    }
    if (NEO::debugManager.flags.SplitBcsMaskD2H.get() > 0) {
        this->d2hEngines = NEO::debugManager.flags.SplitBcsMaskD2H.get();
    }

    uint32_t cmdQIndex = 0u;
    for (uint32_t i = 0; i < NEO::bcsInfoMaskSize; i++) {
        if (this->engines.test(i)) {
            if (this->h2dEngines.test(i)) {
                this->h2dCmdQs.push_back(this->cmdQs[cmdQIndex]);
            }
            if (this->d2hEngines.test(i)) {
                this->d2hCmdQs.push_back(this->cmdQs[cmdQIndex]);
            }
            cmdQIndex++;
        }
    }

    return true;
}

void BcsSplit::releaseResources() {
    std::lock_guard<std::mutex> lock(this->mtx);
    this->clientCount--;

    if (this->clientCount == 0u) {
        for (auto cmdQ : cmdQs) {
            cmdQ->destroy();
        }
        cmdQs.clear();
        d2hCmdQs.clear();
        h2dCmdQs.clear();
        this->events.releaseResources();
    }
}

std::vector<CommandQueue *> &BcsSplit::getCmdQsForSplit(NEO::TransferDirection direction) {
    if (direction == NEO::TransferDirection::hostToLocal) {
        return this->h2dCmdQs;
    } else if (direction == NEO::TransferDirection::localToHost) {
        return this->d2hCmdQs;
    }

    return this->cmdQs;
}

std::optional<size_t> BcsSplit::Events::obtainForSplit(Context *context, size_t maxEventCountInPool) {
    std::lock_guard<std::mutex> lock(this->mtx);
    for (size_t i = 0; i < this->marker.size(); i++) {
        auto ret = this->marker[i]->queryStatus();
        if (ret == ZE_RESULT_SUCCESS) {
            this->resetEventPackage(i);
            return i;
        }
    }

    auto newEventIndex = this->allocateNew(context, maxEventCountInPool);
    if (newEventIndex.has_value() || this->marker.empty()) {
        return newEventIndex;
    }

    this->marker[0]->hostSynchronize(std::numeric_limits<uint64_t>::max());
    this->resetEventPackage(0);
    return 0;
}

std::optional<size_t> BcsSplit::Events::allocateNew(Context *context, size_t maxEventCountInPool) {
    /* Internal events needed for split:
     *  - event per subcopy to signal completion of given subcopy (vector of subcopy events),
     *  - 1 event to signal completion of entire split (vector of marker events),
     *  - 1 event to handle barrier (vector of barrier events).
     */
    const size_t neededEvents = this->bcsSplit.cmdQs.size() + 2;

    if (this->pools.empty() ||
        this->createdFromLatestPool + neededEvents > maxEventCountInPool) {
        ze_result_t result;
        ze_event_pool_desc_t desc{};
        desc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
        desc.count = static_cast<uint32_t>(maxEventCountInPool);
        auto hDevice = this->bcsSplit.device.toHandle();
        auto pool = EventPool::create(this->bcsSplit.device.getDriverHandle(), context, 1, &hDevice, &desc, result);
        if (!pool) {
            return std::nullopt;
        }
        this->pools.push_back(pool);
        this->createdFromLatestPool = 0u;
    }

    auto pool = this->pools[this->pools.size() - 1];
    ze_event_desc_t desc{};
    desc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    desc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    for (size_t i = 0; i < neededEvents; i++) {
        desc.index = static_cast<uint32_t>(this->createdFromLatestPool++);

        // Marker event is the only one of internal split events that will be read from host, so create it at the end with appended scope flag.
        if (i == neededEvents - 1) {
            desc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
        }

        ze_event_handle_t hEvent{};
        pool->createEvent(&desc, &hEvent);
        Event::fromHandle(hEvent)->disableImplicitCounterBasedMode();

        // Last event, created with host scope flag, is marker event.
        if (i == neededEvents - 1) {
            this->marker.push_back(Event::fromHandle(hEvent));

            // One event to handle barrier and others to handle subcopy completion.
        } else if (i == neededEvents - 2) {
            this->barrier.push_back(Event::fromHandle(hEvent));
        } else {
            this->subcopy.push_back(Event::fromHandle(hEvent));
        }
    }

    return this->marker.size() - 1;
}

void BcsSplit::Events::resetEventPackage(size_t index) {
    this->marker[index]->reset();
    this->barrier[index]->reset();
    for (size_t j = 0; j < this->bcsSplit.cmdQs.size(); j++) {
        this->subcopy[index * this->bcsSplit.cmdQs.size() + j]->reset();
    }
}

void BcsSplit::Events::releaseResources() {
    for (auto &markerEvent : this->marker) {
        markerEvent->destroy();
    }
    marker.clear();
    for (auto &subcopyEvent : this->subcopy) {
        subcopyEvent->destroy();
    }
    subcopy.clear();
    for (auto &barrierEvent : this->barrier) {
        barrierEvent->destroy();
    }
    barrier.clear();
    for (auto &pool : this->pools) {
        pool->destroy();
    }
    pools.clear();
}
} // namespace L0
