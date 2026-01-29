/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/bcs_split.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_context.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/driver_experimental/zex_api.h"

namespace L0 {

bool BcsSplit::setupDevice(NEO::CommandStreamReceiver *csr, bool copyOffloadEnabled) {
    auto &productHelper = this->device.getProductHelper();
    this->splitSettings = productHelper.getBcsSplitSettings(this->device.getHwInfo());

    NEO::debugManager.flags.SplitBcsRequiredTileCount.assignIfNotDefault(splitSettings.requiredTileCount);

    // If expectedTileCount==1, route root device to Tile0, otherwise use all Tiles
    bool tileCountMatch = (splitSettings.requiredTileCount == 1) || (this->device.getNEODevice()->getNumSubDevices() == splitSettings.requiredTileCount);
    bool engineMatch = (csr->getOsContext().getEngineType() == productHelper.getDefaultCopyEngine());
    if (copyOffloadEnabled && NEO::debugManager.flags.SplitBcsForCopyOffload.get() != 0) {
        engineMatch = NEO::EngineHelpers::isComputeEngine(csr->getOsContext().getEngineType());
    }

    if (!(engineMatch && tileCountMatch)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(this->mtx);

    NEO::debugManager.flags.SplitBcsPerEngineMaxSize.assignIfNotDefault(splitSettings.perEngineMaxSize);
    UNRECOVERABLE_IF(splitSettings.perEngineMaxSize == 0);

    this->clientCount++;

    if (!this->cmdLists.empty()) {
        return true;
    }

    events.setAggregatedEventMode(NEO::debugManager.flags.SplitBcsAggregatedEventsMode.getIfNotDefault(device.getL0GfxCoreHelper().bcsSplitAggregatedModeEnabled()));

    setupEnginesMask();

    return setupQueues();
}

bool BcsSplit::setupQueues() {
    BcsSplitParams::CsrContainer csrs;

    for (uint32_t tileId = 0; tileId < splitSettings.requiredTileCount; tileId++) {
        auto subDevice = this->device.getNEODevice()->getNearestGenericSubDevice(tileId);

        UNRECOVERABLE_IF(!subDevice);

        for (uint32_t engineId = 0; engineId < NEO::bcsInfoMaskSize; engineId++) {
            if (splitSettings.allEngines.test(engineId)) {
                if (auto engine = subDevice->tryGetEngine(NEO::EngineHelpers::getBcsEngineAtIdx(engineId), NEO::EngineUsage::regular)) {
                    csrs.push_back(engine->commandStreamReceiver);
                }
            }

            if (csrs.size() >= splitSettings.minRequiredTotalCsrCount) {
                break;
            }
        }
    }

    if (csrs.size() < splitSettings.minRequiredTotalCsrCount) {
        return false;
    }

    ze_command_queue_flags_t flags = events.isAggregatedEventMode() ? static_cast<ze_command_queue_flags_t>(ZE_COMMAND_QUEUE_FLAG_IN_ORDER) : 0u;
    ze_command_queue_desc_t splitDesc = {.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, .flags = flags, .mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS};
    auto productFamily = this->device.getHwInfo().platform.eProductFamily;

    for (const auto &csr : csrs) {
        ze_result_t result;

        auto engineGroupType = NEO::EngineHelpers::engineTypeToEngineGroupType(csr->getOsContext().getEngineType());
        device.getNEODevice()->getProductHelper().adjustEngineGroupType(engineGroupType);

        const auto &regularEngines = device.getNEODevice()->getRegularEngineGroups();
        auto ordinalIt = std::find_if(regularEngines.cbegin(), regularEngines.cend(), [engineGroupType](const NEO::EngineGroupT &engineGroup) { return (engineGroup.engineGroupType == engineGroupType); });

        if (ordinalIt == regularEngines.cend()) {
            const auto &subDeviceCopyEngineGroups = device.getSubDeviceCopyEngineGroups();
            ordinalIt = std::find_if(subDeviceCopyEngineGroups.cbegin(), subDeviceCopyEngineGroups.cend(), [engineGroupType](const NEO::EngineGroupT &engineGroup) { return (engineGroup.engineGroupType == engineGroupType); });
            UNRECOVERABLE_IF(ordinalIt == subDeviceCopyEngineGroups.cend());

            splitDesc.ordinal = static_cast<uint32_t>(std::distance(subDeviceCopyEngineGroups.cbegin(), ordinalIt) + regularEngines.size());
        } else {
            splitDesc.ordinal = static_cast<uint32_t>(std::distance(regularEngines.cbegin(), ordinalIt));
        }

        auto cmdList = CommandList::createImmediate(productFamily, &device, &splitDesc, true, engineGroupType, csr, result);
        UNRECOVERABLE_IF(result != ZE_RESULT_SUCCESS);

        cmdList->forceDisableInOrderWaits();
        cmdList->setOrdinal(splitDesc.ordinal);

        this->cmdLists.push_back(cmdList);

        auto engineType = csr->getOsContext().getEngineType();
        auto bcsId = NEO::EngineHelpers::getBcsIndex(engineType);

        if (splitSettings.h2dEngines.test(bcsId)) {
            this->h2dCmdLists.push_back(cmdList);
        }
        if (splitSettings.d2hEngines.test(bcsId)) {
            this->d2hCmdLists.push_back(cmdList);
        }
    }

    return true;
}

void BcsSplit::setupEnginesMask() {
    NEO::debugManager.flags.SplitBcsMask.assignIfNotDefault(splitSettings.allEngines);
    NEO::debugManager.flags.SplitBcsMaskH2D.assignIfNotDefault(splitSettings.h2dEngines);
    NEO::debugManager.flags.SplitBcsMaskD2H.assignIfNotDefault(splitSettings.d2hEngines);
    NEO::debugManager.flags.SplitBcsRequiredEnginesCount.assignIfNotDefault(splitSettings.minRequiredTotalCsrCount);
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

std::vector<CommandList *> &BcsSplit::selectCmdLists(NEO::TransferDirection direction) {
    if (direction == NEO::TransferDirection::hostToLocal) {
        return h2dCmdLists;
    } else if (direction == NEO::TransferDirection::localToHost) {
        return d2hCmdLists;
    }

    return cmdLists;
}

BcsSplitParams::CmdListsForSplitContainer BcsSplit::getCmdListsForSplit(NEO::TransferDirection direction, size_t totalTransferSize) {
    auto &selectedCmdListType = selectCmdLists(direction);

    size_t maxEnginesToUse = std::max(totalTransferSize / splitSettings.perEngineMaxSize, size_t(1));
    auto engineCount = std::min(selectedCmdListType.size(), maxEnginesToUse);

    return {selectedCmdListType.begin(), selectedCmdListType.begin() + engineCount};
}

uint64_t BcsSplit::getAggregatedEventIncrementValForSplit(const Event *signalEvent, bool useSignalEventForSplit, size_t engineCountForSplit) {
    if (!events.isAggregatedEventMode()) {
        return 0;
    }

    uint64_t retVal = useSignalEventForSplit ? signalEvent->getInOrderIncrementValue(1)
                                             : static_cast<uint64_t>(this->getDevice().getAggregatedCopyOffloadIncrementValue());

    UNRECOVERABLE_IF(retVal % engineCountForSplit != 0);
    retVal /= engineCountForSplit;

    return retVal;
}

size_t BcsSplitEvents::obtainAggregatedEventsForSplit(Context *context) {
    for (size_t i = 0; i < this->eventResources.marker.size(); i++) {
        if (this->eventResources.marker[i]->queryStatus(0) == ZE_RESULT_SUCCESS) {
            resetAggregatedEventState(i, false);
            return i;
        }
    }

    return this->createAggregatedEvent(context);
}

std::optional<size_t> BcsSplitEvents::obtainForImmediateSplit(Context *context, size_t maxEventCountInPool) {
    auto lock = obtainLock();

    if (this->aggregatedEventsMode) {
        return obtainAggregatedEventsForSplit(context);
    }

    for (size_t i = 0; i < this->eventResources.marker.size(); i++) {
        auto ret = this->eventResources.marker[i]->queryStatus(0);
        if (ret == ZE_RESULT_SUCCESS) {
            this->resetEventPackage(i);
            return i;
        }
    }

    auto newEventIndex = this->createFromPool(context, maxEventCountInPool);
    if (newEventIndex.has_value() || this->eventResources.marker.empty()) {
        return newEventIndex;
    }

    this->eventResources.marker[0]->hostSynchronize(std::numeric_limits<uint64_t>::max());
    this->resetEventPackage(0);
    return 0;
}

uint64_t *BcsSplitEvents::getNextAllocationForAggregatedEvent() {
    constexpr size_t allocationSize = MemoryConstants::pageSize64k;

    if (!this->eventResources.allocsForAggregatedEvents.empty() && (eventResources.currentAggregatedAllocOffset + MemoryConstants::cacheLineSize) < allocationSize) {
        eventResources.currentAggregatedAllocOffset += MemoryConstants::cacheLineSize;
    } else {
        ze_device_mem_alloc_desc_t desc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
        void *ptr = nullptr;

        auto context = Context::fromHandle(bcsSplit.getDevice().getDriverHandle()->getDefaultContext());

        context->allocDeviceMem(bcsSplit.getDevice().toHandle(), &desc, allocationSize, MemoryConstants::pageSize64k, &ptr);
        UNRECOVERABLE_IF(!ptr);
        eventResources.currentAggregatedAllocOffset = 0;

        this->eventResources.allocsForAggregatedEvents.push_back(ptr);
    }

    auto basePtr = reinterpret_cast<uint64_t *>(this->eventResources.allocsForAggregatedEvents.back());

    return ptrOffset(basePtr, eventResources.currentAggregatedAllocOffset);
}

size_t BcsSplitEvents::createAggregatedEvent(Context *context) {
    constexpr int preallocationCount = 8;
    size_t returnIndex = this->eventResources.subcopy.size();

    const auto deviceIncValue = static_cast<uint64_t>(bcsSplit.getDevice().getAggregatedCopyOffloadIncrementValue());
    const auto engineCount = static_cast<uint64_t>(bcsSplit.cmdLists.size());
    UNRECOVERABLE_IF(deviceIncValue % engineCount != 0);

    // Use deviceIncValue as completion value. Its dividable by engine count to get increment value per subcopy event.
    zex_counter_based_event_external_storage_properties_t externalStorageAllocProperties = {.stype = ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_STORAGE_ALLOC_PROPERTIES,
                                                                                            .incrementValue = deviceIncValue / engineCount,
                                                                                            .completionValue = deviceIncValue};

    const zex_counter_based_event_desc_t counterBasedDesc = {.stype = ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC,
                                                             .pNext = &externalStorageAllocProperties,
                                                             .flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE,
                                                             .signalScope = ZE_EVENT_SCOPE_FLAG_DEVICE};

    const zex_counter_based_event_desc_t markerCounterBasedDesc = {.stype = ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC,
                                                                   .flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_HOST_VISIBLE | ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE,
                                                                   .signalScope = ZE_EVENT_SCOPE_FLAG_HOST};

    for (int i = 0; i < preallocationCount; i++) {
        externalStorageAllocProperties.deviceAddress = getNextAllocationForAggregatedEvent();

        ze_event_handle_t handle = nullptr;
        zexCounterBasedEventCreate2(context, bcsSplit.getDevice().toHandle(), &counterBasedDesc, &handle);
        UNRECOVERABLE_IF(handle == nullptr);

        this->eventResources.subcopy.push_back(Event::fromHandle(handle));

        ze_event_handle_t markerHandle = nullptr;
        zexCounterBasedEventCreate2(context, bcsSplit.getDevice().toHandle(), &markerCounterBasedDesc, &markerHandle);
        UNRECOVERABLE_IF(markerHandle == nullptr);

        this->eventResources.marker.push_back(Event::fromHandle(markerHandle));

        resetAggregatedEventState(this->eventResources.subcopy.size() - 1, (i != 0));
    }

    return returnIndex;
}

bool BcsSplitEvents::allocatePool(Context *context, size_t maxEventCountInPool, size_t neededEvents) {
    if (this->eventResources.pools.empty() ||
        this->eventResources.createdFromLatestPool + neededEvents > maxEventCountInPool) {
        ze_result_t result;
        ze_event_pool_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
        desc.count = static_cast<uint32_t>(maxEventCountInPool);
        auto hDevice = this->bcsSplit.getDevice().toHandle();
        auto pool = EventPool::create(this->bcsSplit.getDevice().getDriverHandle(), context, 1, &hDevice, &desc, result);
        if (!pool) {
            return false;
        }
        this->eventResources.pools.push_back(pool);
        this->eventResources.createdFromLatestPool = 0u;
    }

    return true;
}

std::optional<size_t> BcsSplitEvents::createFromPool(Context *context, size_t maxEventCountInPool) {
    /* Internal events needed for split:
     *  - event per subcopy to signal completion of given subcopy (vector of subcopy events),
     *  - 1 event to signal completion of entire split (vector of marker events),
     *  - 1 event to handle barrier (vector of barrier events).
     */

    const size_t neededEvents = this->bcsSplit.cmdLists.size() + 2;

    if (!allocatePool(context, maxEventCountInPool, neededEvents)) {
        return std::nullopt;
    }

    auto pool = this->eventResources.pools[this->eventResources.pools.size() - 1];
    ze_event_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_DESC};

    for (size_t i = 0; i < neededEvents; i++) {
        // Marker event is the only one of internal split events that will be read from host, so create it at the end with appended scope flag.
        bool markerEvent = (i == neededEvents - 1);
        bool barrierEvent = (i == neededEvents - 2);

        desc.signal = markerEvent ? ZE_EVENT_SCOPE_FLAG_HOST : ZE_EVENT_SCOPE_FLAG_DEVICE;
        desc.index = static_cast<uint32_t>(this->eventResources.createdFromLatestPool++);

        ze_event_handle_t hEvent = {};
        pool->createEvent(&desc, &hEvent);

        auto event = Event::fromHandle(hEvent);

        event->disableImplicitCounterBasedMode();

        // Last event, created with host scope flag, is marker event.
        if (markerEvent) {
            this->eventResources.marker.push_back(event);

            // One event to handle barrier and others to handle subcopy completion.
        } else if (barrierEvent) {
            this->eventResources.barrier.push_back(event);
        } else {
            this->eventResources.subcopy.push_back(event);
        }
    }

    return this->eventResources.marker.size() - 1;
}

void BcsSplitEvents::resetEventPackage(size_t index) {
    this->eventResources.marker[index]->reset();
    this->eventResources.barrier[index]->reset();
    for (size_t j = 0; j < this->bcsSplit.cmdLists.size(); j++) {
        this->eventResources.subcopy[index * this->bcsSplit.cmdLists.size() + j]->reset();
    }
}

void BcsSplitEvents::resetAggregatedEventState(size_t index, bool markerCompleted) {
    *this->eventResources.subcopy[index]->getInOrderExecInfo()->getBaseHostAddress() = 0;

    auto markerEvent = this->eventResources.marker[index];
    markerEvent->resetCompletionStatus();
    markerEvent->unsetInOrderExecInfo();
    markerEvent->setReportEmptyCbEventAsReady(markerCompleted);
}

void BcsSplitEvents::releaseResources() {
    for (auto &markerEvent : eventResources.marker) {
        markerEvent->destroy();
    }
    eventResources.marker.clear();
    for (auto &subcopyEvent : eventResources.subcopy) {
        subcopyEvent->destroy();
    }
    eventResources.subcopy.clear();
    for (auto &barrierEvent : eventResources.barrier) {
        barrierEvent->destroy();
    }
    eventResources.barrier.clear();
    for (auto &pool : eventResources.pools) {
        pool->destroy();
    }
    eventResources.pools.clear();
    auto context = Context::fromHandle(bcsSplit.getDevice().getDriverHandle()->getDefaultContext());
    for (auto &ptr : eventResources.allocsForAggregatedEvents) {
        context->freeMem(ptr);
    }
    eventResources.allocsForAggregatedEvents.clear();
}

} // namespace L0
