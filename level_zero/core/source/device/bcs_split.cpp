/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/bcs_split.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_context.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/driver_experimental/zex_api.h"

namespace L0 {

bool BcsSplit::setupDevice(NEO::CommandStreamReceiver *csr, bool copyOffloadEnabled) {
    auto &productHelper = this->device.getProductHelper();
    auto bcsSplitSettings = productHelper.getBcsSplitSettings(this->device.getHwInfo());

    if (NEO::debugManager.flags.SplitBcsRequiredTileCount.get() != -1) {
        bcsSplitSettings.requiredTileCount = static_cast<uint32_t>(NEO::debugManager.flags.SplitBcsRequiredTileCount.get());
    }

    // If expectedTileCount==1, route root device to Tile0, otherwise use all Tiles
    bool tileCountMatch = (bcsSplitSettings.requiredTileCount == 1) || (this->device.getNEODevice()->getNumSubDevices() == bcsSplitSettings.requiredTileCount);
    bool engineMatch = (csr->getOsContext().getEngineType() == productHelper.getDefaultCopyEngine());
    if (copyOffloadEnabled && NEO::debugManager.flags.SplitBcsForCopyOffload.get() != 0) {
        engineMatch = NEO::EngineHelpers::isComputeEngine(csr->getOsContext().getEngineType());
    }

    if (!(engineMatch && tileCountMatch)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(this->mtx);

    this->clientCount++;

    if (!this->cmdLists.empty()) {
        return true;
    }

    events.aggregatedEventsMode = device.getL0GfxCoreHelper().bcsSplitAggregatedModeEnabled();

    if (NEO::debugManager.flags.SplitBcsAggregatedEventsMode.get() != -1) {
        events.aggregatedEventsMode = !!NEO::debugManager.flags.SplitBcsAggregatedEventsMode.get();
    }

    setupEnginesMask(bcsSplitSettings);

    return setupQueues(bcsSplitSettings);
}

bool BcsSplit::setupQueues(const NEO::BcsSplitSettings &settings) {
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

            if (csrs.size() >= settings.minRequiredTotalCsrCount) {
                break;
            }
        }
    }

    if (csrs.size() < settings.minRequiredTotalCsrCount) {
        return false;
    }

    ze_command_queue_flags_t flags = events.aggregatedEventsMode ? static_cast<ze_command_queue_flags_t>(ZE_COMMAND_QUEUE_FLAG_IN_ORDER) : 0u;
    const ze_command_queue_desc_t splitDesc = {.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, .flags = flags, .mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS};
    auto productFamily = this->device.getHwInfo().platform.eProductFamily;

    for (const auto &csr : csrs) {
        ze_result_t result;
        auto cmdList = CommandList::createImmediate(productFamily, &device, &splitDesc, true, NEO::EngineHelpers::engineTypeToEngineGroupType(csr->getOsContext().getEngineType()), csr, result);
        UNRECOVERABLE_IF(result != ZE_RESULT_SUCCESS);

        cmdList->forceDisableInOrderWaits();

        this->cmdLists.push_back(cmdList);

        auto engineType = csr->getOsContext().getEngineType();
        auto bcsId = NEO::EngineHelpers::getBcsIndex(engineType);

        if (settings.h2dEngines.test(bcsId)) {
            this->h2dCmdLists.push_back(cmdList);
        }
        if (settings.d2hEngines.test(bcsId)) {
            this->d2hCmdLists.push_back(cmdList);
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
        for (auto cmdList : cmdLists) {
            cmdList->destroy();
        }
        cmdLists.clear();
        d2hCmdLists.clear();
        h2dCmdLists.clear();
        this->events.releaseResources();
    }
}

std::vector<CommandList *> &BcsSplit::getCmdListsForSplit(NEO::TransferDirection direction) {
    if (direction == NEO::TransferDirection::hostToLocal) {
        return this->h2dCmdLists;
    } else if (direction == NEO::TransferDirection::localToHost) {
        return this->d2hCmdLists;
    }

    return this->cmdLists;
}

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
                                                                                            .completionValue = static_cast<uint64_t>(bcsSplit.cmdLists.size())};

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

    const size_t neededEvents = this->bcsSplit.cmdLists.size() + 2;

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
    for (size_t j = 0; j < this->bcsSplit.cmdLists.size(); j++) {
        this->subcopy[index * this->bcsSplit.cmdLists.size() + j]->reset();
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
