/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/event/event.h"

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/utilities/cpuintrinsics.h"
#include "shared/source/utilities/wait_util.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/tools/source/metrics/metric.h"

#include <set>

//
#include "level_zero/core/source/event/event_impl.inl"

namespace L0 {
template Event *Event::create<uint64_t>(EventPool *, const ze_event_desc_t *, Device *);
template Event *Event::create<uint32_t>(EventPool *, const ze_event_desc_t *, Device *);

ze_result_t EventPoolImp::initialize(DriverHandle *driver, Context *context, uint32_t numDevices, ze_device_handle_t *phDevices) {
    this->context = static_cast<ContextImp *>(context);

    RootDeviceIndicesContainer rootDeviceIndices;
    uint32_t maxRootDeviceIndex = 0u;
    uint32_t currentNumDevices = numDevices;

    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(driver);
    bool useDevicesFromApi = true;
    this->isDeviceEventPoolAllocation = isEventPoolDeviceAllocationFlagSet();

    if (numDevices == 0) {
        currentNumDevices = static_cast<uint32_t>(driverHandleImp->devices.size());
        useDevicesFromApi = false;
    }

    for (uint32_t i = 0u; i < currentNumDevices; i++) {
        Device *eventDevice = nullptr;

        if (useDevicesFromApi) {
            eventDevice = Device::fromHandle(phDevices[i]);
        } else {
            eventDevice = driverHandleImp->devices[i];
        }

        if (!eventDevice) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        devices.push_back(eventDevice);
        rootDeviceIndices.push_back(eventDevice->getNEODevice()->getRootDeviceIndex());
        if (maxRootDeviceIndex < eventDevice->getNEODevice()->getRootDeviceIndex()) {
            maxRootDeviceIndex = eventDevice->getNEODevice()->getRootDeviceIndex();
        }
    }
    rootDeviceIndices.remove_duplicates();

    auto &rootDeviceEnvironment = getDevice()->getNEODevice()->getRootDeviceEnvironment();
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    this->isDeviceEventPoolAllocation |= l0GfxCoreHelper.alwaysAllocateEventInLocalMem();

    initializeSizeParameters(numDevices, phDevices, *driverHandleImp, rootDeviceEnvironment);

    NEO::AllocationType allocationType = isEventPoolTimestampFlagSet() ? NEO::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER
                                                                       : NEO::AllocationType::BUFFER_HOST_MEMORY;
    if (this->devices.size() > 1) {
        this->isDeviceEventPoolAllocation = false;
    }

    if (this->isDeviceEventPoolAllocation) {
        allocationType = NEO::AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER;
    }

    eventPoolAllocations = std::make_unique<NEO::MultiGraphicsAllocation>(maxRootDeviceIndex);

    bool allocatedMemory = false;

    this->isHostVisibleEventPoolAllocation = !(isEventPoolDeviceAllocationFlagSet());

    if (this->isDeviceEventPoolAllocation) {
        NEO::AllocationProperties allocationProperties{*rootDeviceIndices.begin(), this->eventPoolSize, allocationType, devices[0]->getNEODevice()->getDeviceBitfield()};
        allocationProperties.alignment = eventAlignment;

        auto memoryManager = driver->getMemoryManager();
        auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties);
        if (graphicsAllocation) {
            eventPoolAllocations->addAllocation(graphicsAllocation);
            allocatedMemory = true;
            if (eventPoolFlags & ZE_EVENT_POOL_FLAG_IPC) {
                uint64_t handle = 0;
                this->isShareableEventMemory = (graphicsAllocation->peekInternalHandle(memoryManager, handle) == 0);
            }
        }

    } else {
        NEO::AllocationProperties allocationProperties{*rootDeviceIndices.begin(), this->eventPoolSize, allocationType, systemMemoryBitfield};
        allocationProperties.alignment = eventAlignment;

        eventPoolPtr = driver->getMemoryManager()->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices,
                                                                                                   allocationProperties,
                                                                                                   *eventPoolAllocations);

        if (eventPoolFlags & ZE_EVENT_POOL_FLAG_IPC) {
            this->isShareableEventMemory = eventPoolAllocations->getDefaultGraphicsAllocation()->isShareableHostMemory;
        }

        allocatedMemory = (nullptr != eventPoolPtr);
    }

    if (!allocatedMemory) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    return ZE_RESULT_SUCCESS;
}

EventPoolImp::~EventPoolImp() {
    if (eventPoolAllocations) {
        auto graphicsAllocations = eventPoolAllocations->getGraphicsAllocations();
        auto memoryManager = devices[0]->getDriverHandle()->getMemoryManager();
        for (auto gpuAllocation : graphicsAllocations) {
            memoryManager->freeGraphicsMemory(gpuAllocation);
        }
    }
}

ze_result_t EventPoolImp::destroy() {
    delete this;

    return ZE_RESULT_SUCCESS;
}

ze_result_t EventPoolImp::createEvent(const ze_event_desc_t *desc, ze_event_handle_t *phEvent) {
    if (desc->index > (getNumEvents() - 1)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto &l0GfxCoreHelper = getDevice()->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    *phEvent = l0GfxCoreHelper.createEvent(this, desc, getDevice());

    return ZE_RESULT_SUCCESS;
}

void EventPoolImp::initializeSizeParameters(uint32_t numDevices, ze_device_handle_t *deviceHandles, DriverHandleImp &driver, const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {

    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<NEO::GfxCoreHelper>();

    setEventAlignment(static_cast<uint32_t>(gfxCoreHelper.getTimestampPacketAllocatorAlignment()));

    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    bool useDynamicEventPackets = l0GfxCoreHelper.useDynamicEventPacketsCount(hwInfo);
    eventPackets = EventPacketsCount::eventPackets;
    maxKernelCount = EventPacketsCount::maxKernelSplit;
    if (useDynamicEventPackets) {
        eventPackets = driver.getEventMaxPacketCount(numDevices, deviceHandles);
        maxKernelCount = driver.getEventMaxKernelCount(numDevices, deviceHandles);
    }
    setEventSize(static_cast<uint32_t>(alignUp(eventPackets * gfxCoreHelper.getSingleTimestampPacketSize(), eventAlignment)));

    eventPoolSize = alignUp<size_t>(this->numEvents * eventSize, MemoryConstants::pageSize64k);
}

ze_result_t Event::destroy() {
    delete this;
    return ZE_RESULT_SUCCESS;
}

EventPool *EventPool::create(DriverHandle *driver, Context *context, uint32_t numDevices, ze_device_handle_t *phDevices, const ze_event_pool_desc_t *desc, ze_result_t &result) {
    auto eventPool = std::make_unique<EventPoolImp>(desc);
    if (!eventPool) {
        result = ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        DEBUG_BREAK_IF(true);
        return nullptr;
    }

    result = eventPool->initialize(driver, context, numDevices, phDevices);
    if (result) {
        return nullptr;
    }
    return eventPool.release();
}

} // namespace L0
