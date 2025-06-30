/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/bcs_split.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/os_context.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/driver_experimental/zex_api.h"

namespace L0 {

bool BcsSplit::setupDevice(uint32_t productFamily, bool internalUsage, const ze_command_queue_desc_t *desc, NEO::CommandStreamReceiver *csr) {
    auto &productHelper = this->device.getProductHelper();
    auto bcsSplitSettings = productHelper.getBcsSplitSettings();

    if (NEO::debugManager.flags.SplitBcsRequiredTileCount.get() != -1) {
        bcsSplitSettings.requiredTileCount = static_cast<uint32_t>(NEO::debugManager.flags.SplitBcsRequiredTileCount.get());
    }

    // If expectedTileCount==1, route root device to Tile0, otherwise use all Tiles
    bool tileCountMatch = (bcsSplitSettings.requiredTileCount == 1) || (this->device.getNEODevice()->getNumSubDevices() == bcsSplitSettings.requiredTileCount);

    auto initializeBcsSplit = this->device.getNEODevice()->isBcsSplitSupported() &&
                              (csr->getOsContext().getEngineType() == productHelper.getDefaultCopyEngine()) &&
                              !internalUsage && tileCountMatch;

    if (!initializeBcsSplit) {
        return false;
    }

    std::lock_guard<std::mutex> lock(this->mtx);

    this->clientCount++;

    if (!this->cmdQs.empty()) {
        return true;
    }

    setupEnginesMask(bcsSplitSettings);

    return setupQueues(bcsSplitSettings, productFamily);
}

bool BcsSplit::setupQueues(const NEO::BcsSplitSettings &settings, uint32_t productFamily) {
    CsrContainer csrs;

    for (uint32_t tileId = 0; tileId < settings.requiredTileCount; tileId++) {
        auto subDevice = this->device.getNEODevice()->getNearestGenericSubDevice(tileId);

        UNRECOVERABLE_IF(!subDevice);

        for (uint32_t engineId = 0; engineId < NEO::bcsInfoMaskSize; engineId++) {
            if (settings.allEngines.test(engineId)) {
                if (auto engine = subDevice->tryGetEngine(NEO::EngineHelpers::getBcsEngineAtIdx(engineId), NEO::EngineUsage::regular)) {
                    csrs.push_back(engine->commandStreamReceiver);
                }
            }
        }
    }

    if (csrs.size() < settings.minRequiredTotalCsrCount) {
        return false;
    }

    const ze_command_queue_desc_t splitDesc = {.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, .mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS};

    for (const auto &csr : csrs) {
        ze_result_t result;
        auto commandQueue = CommandQueue::create(productFamily, &device, csr, &splitDesc, true, false, true, result);
        UNRECOVERABLE_IF(result != ZE_RESULT_SUCCESS);

        this->cmdQs.push_back(commandQueue);

        auto engineType = csr->getOsContext().getEngineType();
        auto bcsId = NEO::EngineHelpers::getBcsIndex(engineType);

        if (settings.h2dEngines.test(bcsId)) {
            this->h2dCmdQs.push_back(commandQueue);
        }
        if (settings.d2hEngines.test(bcsId)) {
            this->d2hCmdQs.push_back(commandQueue);
        }
    }

    return true;
}

void BcsSplit::setupEnginesMask(NEO::BcsSplitSettings &settings) {
    if (NEO::debugManager.flags.SplitBcsMask.get() > 0) {
        settings.allEngines = NEO::debugManager.flags.SplitBcsMask.get();
    }
    if (NEO::debugManager.flags.SplitBcsMaskH2D.get() > 0) {
        settings.h2dEngines = NEO::debugManager.flags.SplitBcsMaskH2D.get();
    }
    if (NEO::debugManager.flags.SplitBcsMaskD2H.get() > 0) {
        settings.d2hEngines = NEO::debugManager.flags.SplitBcsMaskD2H.get();
    }

    if (NEO::debugManager.flags.SplitBcsRequiredEnginesCount.get() != -1) {
        settings.minRequiredTotalCsrCount = static_cast<uint32_t>(NEO::debugManager.flags.SplitBcsRequiredEnginesCount.get());
    }
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

BcsSplit::Events::Events(BcsSplit &bcsSplit) : bcsSplit(bcsSplit) {
    if (NEO::debugManager.flags.SplitBcsAggregatedEventsMode.get() != -1) {
        aggregatedEventsMode = !!NEO::debugManager.flags.SplitBcsAggregatedEventsMode.get();
    }
};

size_t BcsSplit::Events::obtainAggregatedEventsForSplit(Context *context) {
    for (size_t i = 0; i < this->marker.size(); i++) {
        if (this->marker[i]->queryStatus() == ZE_RESULT_SUCCESS) {
            resetAggregatedEventState(i, false);
            return i;
        }
    }

    return this->createAggregatedEvent(context);
}

std::optional<size_t> BcsSplit::Events::obtainForSplit(Context *context, size_t maxEventCountInPool) {
    std::lock_guard<std::mutex> lock(this->mtx);

    if (this->aggregatedEventsMode) {
        return obtainAggregatedEventsForSplit(context);
    }

    for (size_t i = 0; i < this->marker.size(); i++) {
        auto ret = this->marker[i]->queryStatus();
        if (ret == ZE_RESULT_SUCCESS) {
            this->resetEventPackage(i);
            return i;
        }
    }

    auto newEventIndex = this->createFromPool(context, maxEventCountInPool);
    if (newEventIndex.has_value() || this->marker.empty()) {
        return newEventIndex;
    }

    this->marker[0]->hostSynchronize(std::numeric_limits<uint64_t>::max());
    this->resetEventPackage(0);
    return 0;
}

uint64_t *BcsSplit::Events::getNextAllocationForAggregatedEvent() {
    constexpr size_t allocationSize = MemoryConstants::pageSize64k;

    if (!this->allocsForAggregatedEvents.empty() && (currentAggregatedAllocOffset + MemoryConstants::cacheLineSize) < allocationSize) {
        currentAggregatedAllocOffset += MemoryConstants::cacheLineSize;
    } else {
        ze_device_mem_alloc_desc_t desc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
        void *ptr = nullptr;

        auto context = Context::fromHandle(bcsSplit.device.getDriverHandle()->getDefaultContext());

        context->allocDeviceMem(bcsSplit.device.toHandle(), &desc, allocationSize, MemoryConstants::pageSize64k, &ptr);
        UNRECOVERABLE_IF(!ptr);
        currentAggregatedAllocOffset = 0;

        this->allocsForAggregatedEvents.push_back(ptr);
    }

    auto basePtr = reinterpret_cast<uint64_t *>(this->allocsForAggregatedEvents.back());

    return ptrOffset(basePtr, currentAggregatedAllocOffset);
}

size_t BcsSplit::Events::createAggregatedEvent(Context *context) {
    constexpr int preallocationCount = 8;
    size_t returnIndex = this->subcopy.size();

    zex_counter_based_event_external_storage_properties_t externalStorageAllocProperties = {.stype = ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_STORAGE_ALLOC_PROPERTIES,
                                                                                            .incrementValue = 1,
                                                                                            .completionValue = static_cast<uint64_t>(bcsSplit.cmdQs.size())};

    const zex_counter_based_event_desc_t counterBasedDesc = {.stype = ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC,
                                                             .pNext = &externalStorageAllocProperties,
                                                             .flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE,
                                                             .signalScope = ZE_EVENT_SCOPE_FLAG_DEVICE};

    const zex_counter_based_event_desc_t markerCounterBasedDesc = {.stype = ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC,
                                                                   .flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_HOST_VISIBLE,
                                                                   .signalScope = ZE_EVENT_SCOPE_FLAG_HOST};

    for (int i = 0; i < preallocationCount; i++) {
        externalStorageAllocProperties.deviceAddress = getNextAllocationForAggregatedEvent();

        ze_event_handle_t handle = nullptr;
        zexCounterBasedEventCreate2(context, bcsSplit.device.toHandle(), &counterBasedDesc, &handle);
        UNRECOVERABLE_IF(handle == nullptr);

        this->subcopy.push_back(Event::fromHandle(handle));

        ze_event_handle_t markerHandle = nullptr;
        zexCounterBasedEventCreate2(context, bcsSplit.device.toHandle(), &markerCounterBasedDesc, &markerHandle);
        UNRECOVERABLE_IF(markerHandle == nullptr);

        this->marker.push_back(Event::fromHandle(markerHandle));

        resetAggregatedEventState(this->subcopy.size() - 1, (i != 0));
    }

    return returnIndex;
}

bool BcsSplit::Events::allocatePool(Context *context, size_t maxEventCountInPool, size_t neededEvents) {
    if (this->pools.empty() ||
        this->createdFromLatestPool + neededEvents > maxEventCountInPool) {
        ze_result_t result;
        ze_event_pool_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
        desc.count = static_cast<uint32_t>(maxEventCountInPool);
        auto hDevice = this->bcsSplit.device.toHandle();
        auto pool = EventPool::create(this->bcsSplit.device.getDriverHandle(), context, 1, &hDevice, &desc, result);
        if (!pool) {
            return false;
        }
        this->pools.push_back(pool);
        this->createdFromLatestPool = 0u;
    }

    return true;
}

std::optional<size_t> BcsSplit::Events::createFromPool(Context *context, size_t maxEventCountInPool) {
    /* Internal events needed for split:
     *  - event per subcopy to signal completion of given subcopy (vector of subcopy events),
     *  - 1 event to signal completion of entire split (vector of marker events),
     *  - 1 event to handle barrier (vector of barrier events).
     */

    const size_t neededEvents = this->bcsSplit.cmdQs.size() + 2;

    if (!allocatePool(context, maxEventCountInPool, neededEvents)) {
        return std::nullopt;
    }

    auto pool = this->pools[this->pools.size() - 1];
    ze_event_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_DESC};

    for (size_t i = 0; i < neededEvents; i++) {
        // Marker event is the only one of internal split events that will be read from host, so create it at the end with appended scope flag.
        bool markerEvent = (i == neededEvents - 1);
        bool barrierEvent = (i == neededEvents - 2);

        desc.signal = markerEvent ? ZE_EVENT_SCOPE_FLAG_HOST : ZE_EVENT_SCOPE_FLAG_DEVICE;
        desc.index = static_cast<uint32_t>(this->createdFromLatestPool++);

        ze_event_handle_t hEvent = {};
        pool->createEvent(&desc, &hEvent);

        auto event = Event::fromHandle(hEvent);

        event->disableImplicitCounterBasedMode();

        // Last event, created with host scope flag, is marker event.
        if (markerEvent) {
            this->marker.push_back(event);

            // One event to handle barrier and others to handle subcopy completion.
        } else if (barrierEvent) {
            this->barrier.push_back(event);
        } else {
            this->subcopy.push_back(event);
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

void BcsSplit::Events::resetAggregatedEventState(size_t index, bool markerCompleted) {
    *this->subcopy[index]->getInOrderExecInfo()->getBaseHostAddress() = 0;

    auto markerEvent = this->marker[index];
    markerEvent->resetCompletionStatus();
    markerEvent->unsetInOrderExecInfo();
    markerEvent->setReportEmptyCbEventAsReady(markerCompleted);
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

    auto context = Context::fromHandle(bcsSplit.device.getDriverHandle()->getDefaultContext());
    for (auto &ptr : this->allocsForAggregatedEvents) {
        context->freeMem(ptr);
    }
    allocsForAggregatedEvents.clear();
}
} // namespace L0
