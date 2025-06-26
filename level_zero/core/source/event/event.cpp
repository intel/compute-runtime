/*
 * Copyright (C) 2020-2025 Intel Corporation
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
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/utilities/cpuintrinsics.h"
#include "shared/source/utilities/timestamp_pool_allocator.h"
#include "shared/source/utilities/wait_util.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist_imp.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/event/event_impl.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

#include <set>

namespace L0 {
template Event *Event::create<uint64_t>(EventPool *, const ze_event_desc_t *, Device *);
template Event *Event::create<uint32_t>(EventPool *, const ze_event_desc_t *, Device *);
template Event *Event::create<uint64_t>(const EventDescriptor &, Device *, ze_result_t &);
template Event *Event::create<uint32_t>(const EventDescriptor &, Device *, ze_result_t &);

ze_result_t EventPool::initialize(DriverHandle *driver, Context *context, uint32_t numDevices, ze_device_handle_t *deviceHandles) {
    this->context = static_cast<ContextImp *>(context);

    const bool counterBased = (counterBasedFlags != 0);

    if (isIpcPoolFlagSet() && counterBased) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    constexpr uint32_t supportedCounterBasedFlags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE | ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE;
    if (counterBased && ((counterBasedFlags & supportedCounterBasedFlags) == 0)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

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
            eventDevice = Device::fromHandle(deviceHandles[i]);
        } else {
            eventDevice = driverHandleImp->devices[i];
        }

        if (!eventDevice) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        devices.push_back(eventDevice);
        rootDeviceIndices.pushUnique(eventDevice->getNEODevice()->getRootDeviceIndex());
        if (maxRootDeviceIndex < eventDevice->getNEODevice()->getRootDeviceIndex()) {
            maxRootDeviceIndex = eventDevice->getNEODevice()->getRootDeviceIndex();
        }

        isImplicitScalingCapable |= eventDevice->isImplicitScalingCapable();
    }

    auto &rootDeviceEnvironment = getDevice()->getNEODevice()->getRootDeviceEnvironment();
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    this->isDeviceEventPoolAllocation |= l0GfxCoreHelper.alwaysAllocateEventInLocalMem();

    initializeSizeParameters(numDevices, deviceHandles, *driverHandleImp, rootDeviceEnvironment);

    NEO::AllocationType allocationType = NEO::AllocationType::timestampPacketTagBuffer;
    if (this->devices.size() > 1) {
        this->isDeviceEventPoolAllocation = false;
    }

    if (this->isDeviceEventPoolAllocation) {
        allocationType = NEO::AllocationType::gpuTimestampDeviceBuffer;
    }

    eventPoolAllocations = std::make_unique<NEO::MultiGraphicsAllocation>(maxRootDeviceIndex);

    bool allocatedMemory = false;

    auto neoDevice = devices[0]->getNEODevice();
    if (this->isDeviceEventPoolAllocation) {
        this->isHostVisibleEventPoolAllocation = !(isEventPoolDeviceAllocationFlagSet());

        if (neoDevice->getDeviceTimestampPoolAllocator().isEnabled() &&
            !isIpcPoolFlagSet()) {
            auto sharedTsAlloc = neoDevice->getDeviceTimestampPoolAllocator().requestGraphicsAllocationForTimestamp(this->eventPoolSize);
            if (sharedTsAlloc) {
                this->sharedTimestampAllocation.reset(sharedTsAlloc);
                eventPoolAllocations->addAllocation(this->sharedTimestampAllocation->getGraphicsAllocation());
                allocatedMemory = true;
            }
        }

        if (!allocatedMemory) {
            NEO::AllocationProperties allocationProperties{*rootDeviceIndices.begin(), this->eventPoolSize, allocationType, neoDevice->getDeviceBitfield()};
            allocationProperties.alignment = eventAlignment;

            auto memoryManager = driver->getMemoryManager();
            auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties);
            if (graphicsAllocation) {
                eventPoolAllocations->addAllocation(graphicsAllocation);
                allocatedMemory = true;
                if (isIpcPoolFlagSet()) {
                    uint64_t handle = 0;
                    this->isShareableEventMemory = (graphicsAllocation->peekInternalHandle(memoryManager, handle) == 0);
                }
            }
        }
    } else {
        this->isHostVisibleEventPoolAllocation = true;
        NEO::AllocationProperties allocationProperties{*rootDeviceIndices.begin(), this->eventPoolSize, allocationType, systemMemoryBitfield};
        allocationProperties.alignment = eventAlignment;

        eventPoolPtr = driver->getMemoryManager()->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices,
                                                                                                   allocationProperties,
                                                                                                   *eventPoolAllocations);
        if (isIpcPoolFlagSet()) {
            this->isShareableEventMemory = eventPoolAllocations->getDefaultGraphicsAllocation()->isShareableHostMemory();
        }
        allocatedMemory = (nullptr != eventPoolPtr);
    }

    if (!allocatedMemory) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    if (neoDevice->getDefaultEngine().commandStreamReceiver->isTbxMode()) {
        eventPoolAllocations->getDefaultGraphicsAllocation()->setWriteMemoryOnly(true);
    }
    return ZE_RESULT_SUCCESS;
}

EventPool::~EventPool() {
    if (eventPoolAllocations) {
        auto graphicsAllocations = eventPoolAllocations->getGraphicsAllocations();
        auto memoryManager = devices[0]->getDriverHandle()->getMemoryManager();
        auto sharedTsAlloc = this->sharedTimestampAllocation ? this->sharedTimestampAllocation->getGraphicsAllocation() : nullptr;
        for (auto gpuAllocation : graphicsAllocations) {
            if (gpuAllocation != sharedTsAlloc) {
                memoryManager->freeGraphicsMemory(gpuAllocation);
            }
        }
    }
    if (this->sharedTimestampAllocation) {
        auto neoDevice = devices[0]->getNEODevice();
        neoDevice->getDeviceTimestampPoolAllocator().freeSharedTimestampAllocation(this->sharedTimestampAllocation.release());
    }
}

ze_result_t EventPool::destroy() {
    delete this;

    return ZE_RESULT_SUCCESS;
}

ze_result_t EventPool::createEvent(const ze_event_desc_t *desc, ze_event_handle_t *eventHandle) {
    if (desc->index > (getNumEvents() - 1)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto &l0GfxCoreHelper = getDevice()->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    *eventHandle = l0GfxCoreHelper.createEvent(this, desc, getDevice());

    return ZE_RESULT_SUCCESS;
}

ze_result_t EventPool::getContextHandle(ze_context_handle_t *phContext) {
    *phContext = context->toHandle();
    return ZE_RESULT_SUCCESS;
}

ze_result_t EventPool::getFlags(ze_event_pool_flags_t *pFlags) {
    *pFlags = eventPoolFlags;

    if (eventPoolFlags & ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP) {
        *pFlags &= ~ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    }
    return ZE_RESULT_SUCCESS;
}

void EventPool::initializeSizeParameters(uint32_t numDevices, ze_device_handle_t *deviceHandles, DriverHandleImp &driver, const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {

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

    auto eventSize = eventPackets * gfxCoreHelper.getSingleTimestampPacketSize();
    if (eventPoolFlags & ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP) {
        eventSize += sizeof(NEO::TimeStampData);
    }

    setEventSize(static_cast<uint32_t>(alignUp(eventSize, eventAlignment)));

    eventPoolSize = alignUp<size_t>(this->numEvents * this->eventSize, MemoryConstants::pageSize64k);
}

EventPool *EventPool::create(DriverHandle *driver, Context *context, uint32_t numDevices, ze_device_handle_t *deviceHandles, const ze_event_pool_desc_t *desc, ze_result_t &result) {
    auto eventPool = std::make_unique<EventPool>(desc);

    result = eventPool->initialize(driver, context, numDevices, deviceHandles);
    if (result) {
        return nullptr;
    }
    return eventPool.release();
}

void EventPool::setupDescriptorFlags(const ze_event_pool_desc_t *desc) {
    eventPoolFlags = desc->flags;

    if (eventPoolFlags & ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP) {
        eventPoolFlags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    }

    this->isIpcPoolFlag = !!(eventPoolFlags & ZE_EVENT_POOL_FLAG_IPC);

    auto pNext = reinterpret_cast<const ze_base_desc_t *>(desc->pNext);

    if (pNext && pNext->stype == ZE_STRUCTURE_TYPE_COUNTER_BASED_EVENT_POOL_EXP_DESC) {
        auto counterBasedDesc = reinterpret_cast<const ze_event_pool_counter_based_exp_desc_t *>(pNext);
        counterBasedFlags = counterBasedDesc->flags;
        if (counterBasedFlags == 0) {
            counterBasedFlags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE;
        }
    }
}

bool EventPool::isEventPoolTimestampFlagSet() const {
    if (NEO::debugManager.flags.OverrideTimestampEvents.get() != -1) {
        auto timestampOverride = !!NEO::debugManager.flags.OverrideTimestampEvents.get();
        return timestampOverride;
    }
    if (eventPoolFlags & ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP) {
        return true;
    }
    return false;
}

ze_result_t EventPool::closeIpcHandle() {
    return this->destroy();
}

ze_result_t Event::openCounterBasedIpcHandle(const IpcCounterBasedEventData &ipcData, ze_event_handle_t *eventHandle,
                                             DriverHandleImp *driver, ContextImp *context, uint32_t numDevices, ze_device_handle_t *deviceHandles) {

    auto device = Device::fromHandle(*deviceHandles);
    auto neoDevice = device->getNEODevice();
    auto memoryManager = driver->getMemoryManager();

    NEO::MemoryManager::OsHandleData deviceOsHandleData{ipcData.deviceHandle};
    NEO::MemoryManager::OsHandleData hostOsHandleData{ipcData.hostHandle};

    NEO::AllocationProperties unifiedMemoryProperties{ipcData.rootDeviceIndex, MemoryConstants::pageSize64k, NEO::DeviceAllocNodeType<true>::getAllocationType(), systemMemoryBitfield};
    unifiedMemoryProperties.subDevicesBitfield = neoDevice->getDeviceBitfield();
    auto *deviceAlloc = memoryManager->createGraphicsAllocationFromSharedHandle(deviceOsHandleData, unifiedMemoryProperties, false, (ipcData.hostHandle == 0), false, nullptr);

    if (!deviceAlloc) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    if (neoDevice->getDefaultEngine().commandStreamReceiver->isTbxMode()) {
        deviceAlloc->setWriteMemoryOnly(true);
    }

    NEO::GraphicsAllocation *hostAlloc = nullptr;
    if (ipcData.hostHandle != 0) {
        unifiedMemoryProperties.allocationType = NEO::DeviceAllocNodeType<false>::getAllocationType();
        hostAlloc = memoryManager->createGraphicsAllocationFromSharedHandle(hostOsHandleData, unifiedMemoryProperties, false, true, false, nullptr);
        if (!hostAlloc) {
            memoryManager->freeGraphicsMemory(deviceAlloc);
            return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        }
        if (neoDevice->getDefaultEngine().commandStreamReceiver->isTbxMode()) {
            hostAlloc->setWriteMemoryOnly(true);
        }
    } else {
        hostAlloc = deviceAlloc;
    }

    auto inOrderExecInfo = NEO::InOrderExecInfo::createFromExternalAllocation(*neoDevice, deviceAlloc, deviceAlloc->getGpuAddress(), hostAlloc, static_cast<uint64_t *>(hostAlloc->getUnderlyingBuffer()),
                                                                              ipcData.counterValue, ipcData.devicePartitions, ipcData.hostPartitions);

    const EventDescriptor eventDescriptor = {
        .eventPoolAllocation = nullptr,
        .extensions = nullptr,
        .totalEventSize = 0,
        .maxKernelCount = EventPacketsCount::maxKernelSplit,
        .maxPacketsCount = 1,
        .counterBasedFlags = ipcData.counterBasedFlags,
        .index = 0,
        .signalScope = ipcData.signalScopeFlags,
        .waitScope = ipcData.waitScopeFlags,
        .timestampPool = false,
        .kernelMappedTsPoolFlag = false,
        .importedIpcPool = true,
        .ipcPool = false,
    };

    ze_result_t result = ZE_RESULT_SUCCESS;

    auto event = Event::create<uint64_t>(eventDescriptor, device, result);
    event->updateInOrderExecState(inOrderExecInfo, ipcData.counterValue, ipcData.counterOffset);

    *eventHandle = event;

    return result;
}

ze_result_t Event::getCounterBasedIpcHandle(IpcCounterBasedEventData &ipcData) {
    if (!this->isSharableCounterBased) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (!isCounterBasedExplicitlyEnabled() || !this->inOrderExecInfo.get() || isEventTimestampFlagSet()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    ipcData = {};
    ipcData.rootDeviceIndex = device->getRootDeviceIndex();
    ipcData.counterValue = this->getInOrderExecSignalValueWithSubmissionCounter();
    ipcData.counterBasedFlags = this->counterBasedFlags;
    ipcData.signalScopeFlags = this->signalScope;
    ipcData.waitScopeFlags = this->waitScope;

    auto memoryManager = device->getNEODevice()->getMemoryManager();
    auto deviceAlloc = inOrderExecInfo->getDeviceCounterAllocation();

    uint64_t handle = 0;
    if (int retCode = deviceAlloc->peekInternalHandle(memoryManager, handle); retCode != 0) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    memoryManager->registerIpcExportedAllocation(deviceAlloc);

    ipcData.deviceHandle = handle;
    ipcData.devicePartitions = inOrderExecInfo->getNumDevicePartitionsToWait();
    ipcData.hostPartitions = ipcData.devicePartitions;

    ipcData.counterOffset = static_cast<uint32_t>(inOrderExecInfo->getBaseDeviceAddress() - deviceAlloc->getGpuAddress()) + inOrderAllocationOffset;

    if (inOrderExecInfo->isHostStorageDuplicated()) {
        auto hostAlloc = inOrderExecInfo->getHostCounterAllocation();
        if (int retCode = hostAlloc->peekInternalHandle(memoryManager, handle); retCode != 0) {
            return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        }
        memoryManager->registerIpcExportedAllocation(hostAlloc);

        ipcData.hostHandle = handle;
        ipcData.hostPartitions = inOrderExecInfo->getNumHostPartitionsToWait();
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t EventPool::getIpcHandle(ze_ipc_event_pool_handle_t *ipcHandle) {
    if (!this->isShareableEventMemory) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    IpcEventPoolData &poolData = *reinterpret_cast<IpcEventPoolData *>(ipcHandle->data);
    poolData = {};
    poolData.numEvents = this->numEvents;
    poolData.rootDeviceIndex = this->getDevice()->getRootDeviceIndex();
    poolData.isDeviceEventPoolAllocation = this->isDeviceEventPoolAllocation;
    poolData.isHostVisibleEventPoolAllocation = this->isHostVisibleEventPoolAllocation;
    poolData.isImplicitScalingCapable = this->isImplicitScalingCapable;
    poolData.maxEventPackets = this->getEventMaxPackets();
    poolData.numDevices = static_cast<uint32_t>(this->devices.size());
    poolData.isEventPoolKernelMappedTsFlagSet = this->isEventPoolKernelMappedTsFlagSet();
    poolData.isEventPoolTsFlagSet = this->isEventPoolTimestampFlagSet();

    auto memoryManager = this->context->getDriverHandle()->getMemoryManager();
    auto allocation = this->eventPoolAllocations->getDefaultGraphicsAllocation();
    uint64_t handle{};
    if (int retCode = allocation->peekInternalHandle(memoryManager, handle); retCode != 0) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }
    poolData.handle = handle;
    memoryManager->registerIpcExportedAllocation(allocation);
    return ZE_RESULT_SUCCESS;
}

ze_result_t EventPool::openEventPoolIpcHandle(const ze_ipc_event_pool_handle_t &ipcEventPoolHandle, ze_event_pool_handle_t *eventPoolHandle,
                                              DriverHandleImp *driver, ContextImp *context, uint32_t numDevices, ze_device_handle_t *deviceHandles) {
    const IpcEventPoolData &poolData = *reinterpret_cast<const IpcEventPoolData *>(ipcEventPoolHandle.data);

    ze_event_pool_desc_t desc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    if (poolData.isEventPoolKernelMappedTsFlagSet) {
        desc.flags |= ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP;
    }
    if (poolData.isEventPoolTsFlagSet) {
        desc.flags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    }
    desc.count = static_cast<uint32_t>(poolData.numEvents);
    auto eventPool = std::make_unique<EventPool>(&desc);
    eventPool->isDeviceEventPoolAllocation = poolData.isDeviceEventPoolAllocation;
    eventPool->isHostVisibleEventPoolAllocation = poolData.isHostVisibleEventPoolAllocation;
    eventPool->isImplicitScalingCapable = poolData.isImplicitScalingCapable;
    ze_device_handle_t *deviceHandlesUsed = deviceHandles;

    UNRECOVERABLE_IF(numDevices == 0);
    auto device = Device::fromHandle(*deviceHandles);
    auto neoDevice = device->getNEODevice();
    NEO::MemoryManager::OsHandleData osHandleData{poolData.handle};

    if (poolData.numDevices == 1) {
        for (uint32_t i = 0; i < numDevices; i++) {
            auto deviceStruct = Device::fromHandle(deviceHandles[i]);
            auto neoDeviceIteration = deviceStruct->getNEODevice();
            if (neoDeviceIteration->getRootDeviceIndex() == poolData.rootDeviceIndex) {
                *deviceHandlesUsed = deviceHandles[i];
                neoDevice = neoDeviceIteration;
                break;
            }
        }
        numDevices = 1;
    }

    eventPool->initializeSizeParameters(numDevices, deviceHandlesUsed, *driver, neoDevice->getRootDeviceEnvironment());
    if (eventPool->getEventMaxPackets() != poolData.maxEventPackets) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(),
                           stderr,
                           "IPC handle max event packets %u does not match context devices max event packet %u\n",
                           poolData.maxEventPackets, eventPool->getEventMaxPackets());
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    NEO::AllocationType allocationType = NEO::AllocationType::timestampPacketTagBuffer;
    if (eventPool->isDeviceEventPoolAllocation) {
        allocationType = NEO::AllocationType::gpuTimestampDeviceBuffer;
    }

    NEO::AllocationProperties unifiedMemoryProperties{poolData.rootDeviceIndex,
                                                      eventPool->getEventPoolSize(),
                                                      allocationType,
                                                      systemMemoryBitfield};

    unifiedMemoryProperties.subDevicesBitfield = neoDevice->getDeviceBitfield();
    auto memoryManager = driver->getMemoryManager();
    NEO::GraphicsAllocation *alloc = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData,
                                                                                             unifiedMemoryProperties,
                                                                                             false,
                                                                                             eventPool->isHostVisibleEventPoolAllocation,
                                                                                             false,
                                                                                             nullptr);

    if (alloc == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    if (neoDevice->getDefaultEngine().commandStreamReceiver->isTbxMode()) {
        alloc->setWriteMemoryOnly(true);
    }

    eventPool->context = context;
    eventPool->eventPoolAllocations =
        std::make_unique<NEO::MultiGraphicsAllocation>(static_cast<uint32_t>(context->rootDeviceIndices.size()));
    eventPool->eventPoolAllocations->addAllocation(alloc);
    eventPool->eventPoolPtr = reinterpret_cast<void *>(alloc->getUnderlyingBuffer());
    for (uint32_t i = 0; i < numDevices; i++) {
        eventPool->devices.push_back(Device::fromHandle(deviceHandlesUsed[i]));
    }
    eventPool->isImportedIpcPool = true;

    if (numDevices > 1) {
        for (auto currDeviceIndex : context->rootDeviceIndices) {
            if (currDeviceIndex == poolData.rootDeviceIndex) {
                continue;
            }

            unifiedMemoryProperties.rootDeviceIndex = currDeviceIndex;
            unifiedMemoryProperties.flags.isUSMHostAllocation = true;
            unifiedMemoryProperties.flags.forceSystemMemory = true;
            unifiedMemoryProperties.flags.allocateMemory = false;
            auto graphicsAllocation = memoryManager->createGraphicsAllocationFromExistingStorage(unifiedMemoryProperties,
                                                                                                 eventPool->eventPoolPtr,
                                                                                                 eventPool->getAllocation());
            if (!graphicsAllocation) {
                return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
            }
            eventPool->eventPoolAllocations->addAllocation(graphicsAllocation);
        }
    }

    *eventPoolHandle = eventPool.release();

    return ZE_RESULT_SUCCESS;
}

void Event::releaseTempInOrderTimestampNodes() {
    if (inOrderExecInfo) {
        inOrderExecInfo->releaseNotUsedTempTimestampNodes(false);
    }
}

ze_result_t Event::destroy() {
    resetInOrderTimestampNode(nullptr, 0);
    resetAdditionalTimestampNode(nullptr, 0, true);
    releaseTempInOrderTimestampNodes();

    if (isCounterBasedExplicitlyEnabled() && isFromIpcPool) {
        auto memoryManager = device->getNEODevice()->getMemoryManager();

        memoryManager->freeGraphicsMemory(inOrderExecInfo->getExternalDeviceAllocation());
        if (inOrderExecInfo->isHostStorageDuplicated()) {
            memoryManager->freeGraphicsMemory(inOrderExecInfo->getExternalHostAllocation());
        }
    }

    delete this;
    return ZE_RESULT_SUCCESS;
}

void Event::enableCounterBasedMode(bool apiRequest, uint32_t flags) {
    if (counterBasedMode == CounterBasedMode::initiallyDisabled) {
        counterBasedMode = apiRequest ? CounterBasedMode::explicitlyEnabled : CounterBasedMode::implicitlyEnabled;
        counterBasedFlags = flags;
    }
}

void Event::disableImplicitCounterBasedMode() {
    if (isCounterBasedExplicitlyEnabled()) {
        return;
    }

    if (counterBasedMode == CounterBasedMode::implicitlyEnabled || counterBasedMode == CounterBasedMode::initiallyDisabled) {
        counterBasedMode = CounterBasedMode::implicitlyDisabled;
        counterBasedFlags = 0;
        unsetInOrderExecInfo();
    }
}

uint64_t Event::getGpuAddress(Device *device) const {
    if (!inOrderTimestampNode.empty()) {
        return inOrderTimestampNode.back()->getGpuAddress();
    }
    return getAllocation(device)->getGpuAddress() + this->eventPoolOffset + this->offsetInSharedAlloc;
}

void *Event::getHostAddress() const {
    if (!inOrderTimestampNode.empty()) {
        return inOrderTimestampNode.back()->getCpuBase();
    }

    return this->hostAddressFromPool;
}

NEO::GraphicsAllocation *Event::getAllocation(Device *device) const {
    auto rootDeviceIndex = device->getNEODevice()->getRootDeviceIndex();

    if (!inOrderTimestampNode.empty()) {
        return inOrderTimestampNode.back()->getBaseGraphicsAllocation()->getGraphicsAllocation(rootDeviceIndex);
    } else if (eventPoolAllocation) {
        return eventPoolAllocation->getGraphicsAllocation(rootDeviceIndex);
    }

    return nullptr;
}

void Event::setGpuStartTimestamp() {
    if (isEventTimestampFlagSet()) {
        this->device->getGlobalTimestamps(&cpuStartTimestamp, &gpuStartTimestamp);
        cpuStartTimestamp = cpuStartTimestamp / this->device->getNEODevice()->getDeviceInfo().outProfilingTimerResolution;
    }
}

void Event::setGpuEndTimestamp() {
    if (isEventTimestampFlagSet()) {
        auto resolution = this->device->getNEODevice()->getDeviceInfo().outProfilingTimerResolution;
        uint64_t cpuEndTimestamp = 0;
        this->device->getNEODevice()->getOSTime()->getCpuTime(&cpuEndTimestamp);
        cpuEndTimestamp = cpuEndTimestamp / resolution;
        this->gpuEndTimestamp = gpuStartTimestamp + std::max<size_t>(1u, (cpuEndTimestamp - cpuStartTimestamp));
    }
}

void *Event::getCompletionFieldHostAddress() const {
    return ptrOffset(getHostAddress(), getCompletionFieldOffset());
}

void Event::increaseKernelCount() {
    kernelCount++;
    UNRECOVERABLE_IF(kernelCount > maxKernelCount);
}

void Event::resetPackets(bool resetAllPackets) {
    if (resetAllPackets) {
        resetKernelCountAndPacketUsedCount();
    }
    cpuStartTimestamp = 0;
    gpuStartTimestamp = 0;
    gpuEndTimestamp = 0;
    this->csrs.clear();
    this->csrs.push_back(this->device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
}

void Event::setIsCompleted() {
    if (this->isCompleted.load() == STATE_CLEARED) {
        this->isCompleted = STATE_SIGNALED;
    }
    unsetCmdQueue();
}

void Event::updateInOrderExecState(std::shared_ptr<NEO::InOrderExecInfo> &newInOrderExecInfo, uint64_t signalValue, uint32_t allocationOffset) {
    resetCompletionStatus();

    if (this->inOrderExecInfo.get() != newInOrderExecInfo.get()) {
        inOrderExecInfo = newInOrderExecInfo;
    }

    inOrderExecSignalValue = signalValue;
    inOrderAllocationOffset = allocationOffset;
}

uint64_t Event::getInOrderExecSignalValueWithSubmissionCounter() const {
    uint64_t appendCounter = inOrderExecInfo.get() ? NEO::InOrderPatchCommandHelpers::getAppendCounterValue(*inOrderExecInfo) : 0;
    return (inOrderExecSignalValue + appendCounter);
}

void Event::setLatestUsedCmdQueue(CommandQueue *newCmdQ) {
    this->latestUsedCmdQueue = newCmdQ;
}

void Event::unsetCmdQueue() {
    for (auto &csr : csrs) {
        csr->unregisterClient(latestUsedCmdQueue);
    }

    latestUsedCmdQueue = nullptr;
}

void Event::setReferenceTs(uint64_t currentCpuTimeStamp) {
    NEO::TimeStampData *referenceTs = static_cast<NEO::TimeStampData *>(ptrOffset(getHostAddress(), maxPacketCount * singlePacketSize));
    const auto recalculate =
        (currentCpuTimeStamp - referenceTs->cpuTimeinNS) > timestampRefreshIntervalInNanoSec;
    if (referenceTs->cpuTimeinNS == 0 || recalculate) {
        device->getNEODevice()->getOSTime()->getGpuCpuTime(referenceTs, true);
    }
}

void Event::unsetInOrderExecInfo() {
    resetInOrderTimestampNode(nullptr, 0);
    inOrderExecInfo.reset();
    inOrderAllocationOffset = 0;
    inOrderExecSignalValue = 0;
}

void Event::resetInOrderTimestampNode(NEO::TagNodeBase *newNode, uint32_t partitionCount) {
    if (inOrderIncrementValue == 0 || !newNode) {
        for (auto &node : inOrderTimestampNode) {
            inOrderExecInfo->pushTempTimestampNode(node, inOrderExecSignalValue);
        }

        inOrderTimestampNode.clear();
    }

    if (newNode) {
        inOrderTimestampNode.push_back(newNode);

        if (NEO::debugManager.flags.ClearStandaloneInOrderTimestampAllocation.get() != 0) {
            clearTimestampTagData(partitionCount, inOrderTimestampNode.back());
        }
    }
}

void Event::resetAdditionalTimestampNode(NEO::TagNodeBase *newNode, uint32_t partitionCount, bool resetAggregatedEvent) {
    if (inOrderIncrementValue > 0) {
        if (newNode) {
            additionalTimestampNode.push_back(newNode);
            if (NEO::debugManager.flags.ClearStandaloneInOrderTimestampAllocation.get() != 0) {
                clearTimestampTagData(partitionCount, newNode);
            }
        } else if (resetAggregatedEvent) {
            // If we are resetting aggregated event, we need to clear all additional timestamp nodes
            for (auto &node : additionalTimestampNode) {
                inOrderExecInfo->pushTempTimestampNode(node, inOrderExecSignalValue);
            }
            additionalTimestampNode.clear();
        }

        return;
    }

    for (auto &node : additionalTimestampNode) {
        if (inOrderExecInfo) {
            // Push to temp node vector and releaseNotUsedTempTimestampNodes will clear when needed
            inOrderExecInfo->pushTempTimestampNode(node, inOrderExecSignalValue);
        } else {
            node->returnTag();
        }
    }
    additionalTimestampNode.clear();

    if (newNode) {
        additionalTimestampNode.push_back(newNode);
        if (NEO::debugManager.flags.ClearStandaloneInOrderTimestampAllocation.get() != 0) {
            clearTimestampTagData(partitionCount, newNode);
        }
    }
}

NEO::GraphicsAllocation *Event::getExternalCounterAllocationFromAddress(uint64_t *address) const {
    NEO::SvmAllocationData *allocData = nullptr;
    if (!address || !device->getDriverHandle()->findAllocationDataForRange(address, sizeof(uint64_t), allocData)) {
        return nullptr;
    }

    return allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
}

ze_result_t Event::enableExtensions(const EventDescriptor &eventDescriptor) {
    bool interruptMode = false;
    bool kmdWaitMode = false;
    bool externalInterruptWait = false;

    auto extendedDesc = reinterpret_cast<const ze_base_desc_t *>(eventDescriptor.extensions);

    while (extendedDesc) {
        if (static_cast<uint32_t>(extendedDesc->stype) == ZEX_INTEL_STRUCTURE_TYPE_EVENT_SYNC_MODE_EXP_DESC) {
            auto eventSyncModeDesc = reinterpret_cast<const zex_intel_event_sync_mode_exp_desc_t *>(extendedDesc);

            interruptMode = (eventSyncModeDesc->syncModeFlags & ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_SIGNAL_INTERRUPT);
            kmdWaitMode = (eventSyncModeDesc->syncModeFlags & ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_LOW_POWER_WAIT);
            externalInterruptWait = (eventSyncModeDesc->syncModeFlags & ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_EXTERNAL_INTERRUPT_WAIT);

            if (externalInterruptWait) {
                setExternalInterruptId(eventSyncModeDesc->externalInterruptId);
                UNRECOVERABLE_IF(eventSyncModeDesc->externalInterruptId > 0 && eventDescriptor.eventPoolAllocation);
            }
        } else if (static_cast<uint32_t>(extendedDesc->stype) == ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_SYNC_ALLOC_PROPERTIES) {
            auto externalSyncAllocProperties = reinterpret_cast<const zex_counter_based_event_external_sync_alloc_properties_t *>(extendedDesc);

            if (!externalSyncAllocProperties->deviceAddress || !externalSyncAllocProperties->hostAddress) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }

            auto deviceAlloc = getExternalCounterAllocationFromAddress(externalSyncAllocProperties->deviceAddress);
            auto hostAlloc = getExternalCounterAllocationFromAddress(externalSyncAllocProperties->hostAddress);

            if (!hostAlloc) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }

            auto inOrderExecInfo = NEO::InOrderExecInfo::createFromExternalAllocation(*device->getNEODevice(), deviceAlloc, castToUint64(externalSyncAllocProperties->deviceAddress),
                                                                                      hostAlloc, externalSyncAllocProperties->hostAddress, externalSyncAllocProperties->completionValue, 1, 1);
            updateInOrderExecState(inOrderExecInfo, externalSyncAllocProperties->completionValue, 0);
            disableHostCaching(true);
        } else if (static_cast<uint32_t>(extendedDesc->stype) == ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_STORAGE_ALLOC_PROPERTIES) {
            auto externalStorageProperties = reinterpret_cast<const zex_counter_based_event_external_storage_properties_t *>(extendedDesc);

            auto deviceAlloc = getExternalCounterAllocationFromAddress(externalStorageProperties->deviceAddress);

            if (!deviceAlloc || externalStorageProperties->incrementValue == 0) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }

            auto offset = ptrDiff(externalStorageProperties->deviceAddress, deviceAlloc->getGpuAddress());

            auto hostAddress = ptrOffset(device->getNEODevice()->getMemoryManager()->lockResource(deviceAlloc), offset);

            auto inOrderExecInfo = NEO::InOrderExecInfo::createFromExternalAllocation(*device->getNEODevice(), deviceAlloc, castToUint64(externalStorageProperties->deviceAddress),
                                                                                      deviceAlloc, reinterpret_cast<uint64_t *>(hostAddress), externalStorageProperties->completionValue, 1, 1);

            updateInOrderExecState(inOrderExecInfo, externalStorageProperties->completionValue, 0);

            this->inOrderIncrementValue = externalStorageProperties->incrementValue;
            disableHostCaching(true);
        }

        extendedDesc = reinterpret_cast<const ze_base_desc_t *>(extendedDesc->pNext);
    }

    interruptMode |= (NEO::debugManager.flags.WaitForUserFenceOnEventHostSynchronize.get() == 1);
    kmdWaitMode |= (NEO::debugManager.flags.WaitForUserFenceOnEventHostSynchronize.get() == 1);

    if (interruptMode) {
        enableInterruptMode();
    }

    if (externalInterruptWait || (interruptMode && kmdWaitMode)) {
        enableKmdWaitMode();
    }

    return ZE_RESULT_SUCCESS;
}

} // namespace L0
