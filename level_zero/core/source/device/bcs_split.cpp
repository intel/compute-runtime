/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/bcs_split.h"

#include "shared/source/command_stream/command_stream_receiver.h"

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

    if (NEO::DebugManager.flags.SplitBcsMask.get() > 0) {
        this->engines = NEO::DebugManager.flags.SplitBcsMask.get();
    }

    ze_command_queue_desc_t splitDesc;
    memcpy(&splitDesc, desc, sizeof(ze_command_queue_desc_t));
    splitDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    for (uint32_t i = 0; i < NEO::bcsInfoMaskSize; i++) {
        if (this->engines.test(i)) {
            auto engineType = (i == 0u ? aub_stream::EngineType::ENGINE_BCS : aub_stream::EngineType::ENGINE_BCS1 + i - 1);
            auto csr = this->device.getNEODevice()->getNearestGenericSubDevice(0u)->getEngine(static_cast<aub_stream::EngineType>(engineType), NEO::EngineUsage::Regular).commandStreamReceiver;

            ze_result_t result;
            auto commandQueue = CommandQueue::create(productFamily, &device, csr, &splitDesc, true, false, result);
            UNRECOVERABLE_IF(result != ZE_RESULT_SUCCESS);

            this->cmdQs.push_back(commandQueue);
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
        this->events.releaseResources();
    }
}

size_t BcsSplit::Events::obtainForSplit(Context *context, size_t maxEventCountInPool) {
    for (size_t i = 0; i < this->marker.size(); i++) {
        auto ret = this->marker[i]->queryStatus();
        if (ret == ZE_RESULT_SUCCESS) {
            this->marker[i]->reset();
            for (size_t j = 0; j < this->bcsSplit.cmdQs.size(); j++) {
                this->subcopy[i * this->bcsSplit.cmdQs.size() + j]->reset();
            }
            return i;
        }
    }

    return this->allocateNew(context, maxEventCountInPool);
}

size_t BcsSplit::Events::allocateNew(Context *context, size_t maxEventCountInPool) {
    const size_t neededEvents = this->bcsSplit.cmdQs.size() + 1;

    if (this->pools.empty() ||
        this->createdFromLatestPool + neededEvents > maxEventCountInPool) {
        ze_result_t result;
        ze_event_pool_desc_t desc{};
        desc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
        desc.count = static_cast<uint32_t>(maxEventCountInPool);
        auto hDevice = this->bcsSplit.device.toHandle();
        auto pool = EventPool::create(this->bcsSplit.device.getDriverHandle(), context, 1, &hDevice, &desc, result);
        this->pools.push_back(pool);
        this->createdFromLatestPool = 0u;
    }

    auto pool = this->pools[this->pools.size() - 1];
    ze_event_desc_t desc{};
    desc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    desc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    for (size_t i = 0; i < neededEvents; i++) {
        desc.index = static_cast<uint32_t>(this->createdFromLatestPool++);
        if (i == neededEvents - 1) {
            desc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
        }

        ze_event_handle_t hEvent;
        pool->createEvent(&desc, &hEvent);

        if (i == neededEvents - 1) {
            this->marker.push_back(Event::fromHandle(hEvent));
        } else {
            this->subcopy.push_back(Event::fromHandle(hEvent));
        }
    }

    return this->marker.size() - 1;
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
    for (auto &pool : this->pools) {
        pool->destroy();
    }
    pools.clear();
}
} // namespace L0