/*
 * Copyright (C) 2020-2021 Intel Corporation
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
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/utilities/cpuintrinsics.h"
#include "shared/source/utilities/wait_util.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/event/event_impl.inl"
#include "level_zero/tools/source/metrics/metric.h"

#include <set>

namespace L0 {

ze_result_t EventPoolImp::initialize(DriverHandle *driver, Context *context, uint32_t numDevices, ze_device_handle_t *phDevices, uint32_t numEvents) {
    std::set<uint32_t> rootDeviceIndices;
    uint32_t maxRootDeviceIndex = 0u;

    void *eventPoolPtr = nullptr;

    size_t alignedSize = alignUp<size_t>(numEvents * eventSize, MemoryConstants::pageSize64k);
    NEO::GraphicsAllocation::AllocationType allocationType = isEventPoolUsedForTimestamp ? NEO::GraphicsAllocation::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER
                                                                                         : NEO::GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY;

    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(driver);
    bool useDevicesFromApi = true;

    if (numDevices == 0) {
        numDevices = static_cast<uint32_t>(driverHandleImp->devices.size());
        useDevicesFromApi = false;
    }

    for (uint32_t i = 0u; i < numDevices; i++) {
        Device *eventDevice = nullptr;

        if (useDevicesFromApi) {
            eventDevice = Device::fromHandle(phDevices[i]);
        } else {
            eventDevice = driverHandleImp->devices[i];
        }

        if (!eventDevice) {
            continue;
        }

        devices.push_back(eventDevice);
        rootDeviceIndices.insert(eventDevice->getNEODevice()->getRootDeviceIndex());
        if (maxRootDeviceIndex < eventDevice->getNEODevice()->getRootDeviceIndex()) {
            maxRootDeviceIndex = eventDevice->getNEODevice()->getRootDeviceIndex();
        }
    }

    eventPoolAllocations = std::make_unique<NEO::MultiGraphicsAllocation>(maxRootDeviceIndex);

    NEO::AllocationProperties allocationProperties{*rootDeviceIndices.begin(), alignedSize, allocationType, systemMemoryBitfield};
    allocationProperties.alignment = eventAlignment;

    std::vector<uint32_t> rootDeviceIndicesVector = {rootDeviceIndices.begin(), rootDeviceIndices.end()};
    eventPoolPtr = driver->getMemoryManager()->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndicesVector,
                                                                                               allocationProperties,
                                                                                               *eventPoolAllocations);

    if (!eventPoolPtr) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }
    return ZE_RESULT_SUCCESS;
}

EventPoolImp::~EventPoolImp() {
    auto graphicsAllocations = eventPoolAllocations->getGraphicsAllocations();
    auto memoryManager = devices[0]->getDriverHandle()->getMemoryManager();
    for (auto gpuAllocation : graphicsAllocations) {
        memoryManager->freeGraphicsMemory(gpuAllocation);
    }
}

ze_result_t EventPoolImp::getIpcHandle(ze_ipc_event_pool_handle_t *pIpcHandle) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t EventPoolImp::closeIpcHandle() {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t EventPoolImp::destroy() {
    delete this;

    return ZE_RESULT_SUCCESS;
}

ze_result_t EventPoolImp::createEvent(const ze_event_desc_t *desc, ze_event_handle_t *phEvent) {
    if (desc->index > (getNumEvents() - 1)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    *phEvent = Event::create<uint32_t>(this, desc, this->getDevice());

    return ZE_RESULT_SUCCESS;
}

ze_result_t Event::destroy() {
    delete this;
    return ZE_RESULT_SUCCESS;
}

EventPool *EventPool::create(DriverHandle *driver, Context *context, uint32_t numDevices,
                             ze_device_handle_t *phDevices,
                             const ze_event_pool_desc_t *desc) {
    auto eventPool = new (std::nothrow) EventPoolImp(driver, numDevices, phDevices, desc->count, desc->flags);
    if (!eventPool) {
        DEBUG_BREAK_IF(true);
        return nullptr;
    }

    ze_result_t result = eventPool->initialize(driver, context, numDevices, phDevices, desc->count);
    if (result) {
        delete eventPool;
        return nullptr;
    }
    return eventPool;
}

} // namespace L0
