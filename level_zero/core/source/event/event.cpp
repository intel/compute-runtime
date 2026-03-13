/*
 * Copyright (C) 2020-2026 Intel Corporation
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
#include "shared/source/helpers/device_bitfield.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/utilities/pool_allocators.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/event/event_impl.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

static_assert(sizeof(NEO::InOrderExecEventData) <= ZE_MAX_IPC_HANDLE_SIZE, "InOrderExecEventData is bigger than ZE_MAX_IPC_HANDLE_SIZE");

namespace L0 {
template Event *Event::create<uint64_t>(EventPool *, const ze_event_desc_t *, Device *, ze_result_t &);
template Event *Event::create<uint32_t>(EventPool *, const ze_event_desc_t *, Device *, ze_result_t &);
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

    bool useDevicesFromApi = true;
    this->isDeviceEventPoolAllocation = isEventPoolDeviceAllocationFlagSet();

    if (numDevices == 0) {
        currentNumDevices = static_cast<uint32_t>(driver->devices.size());
        useDevicesFromApi = false;
    }

    for (uint32_t i = 0u; i < currentNumDevices; i++) {
        Device *eventDevice = nullptr;

        if (useDevicesFromApi) {
            eventDevice = Device::fromHandle(deviceHandles[i]);
        } else {
            eventDevice = driver->devices[i];
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

    initializeSizeParameters(numDevices, deviceHandles, *driver, rootDeviceEnvironment);

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

        const auto isTimestampPoolingEnabled = [neoDevice]() -> bool {
            if (NEO::debugManager.flags.EnableTimestampPoolAllocator.get() != -1) {
                return NEO::debugManager.flags.EnableTimestampPoolAllocator.get();
            }
            return NEO::DeviceFactory::isHwModeSelected() &&
                   neoDevice->getProductHelper().is2MBLocalMemAlignmentEnabled();
        }();

        if (isTimestampPoolingEnabled &&
            !isIpcPoolFlagSet()) {
            auto sharedTsAlloc = neoDevice->getDeviceTimestampPoolAllocator().allocate(this->eventPoolSize);
            if (sharedTsAlloc) {
                this->sharedTimestampAllocation.reset(sharedTsAlloc);
                eventPoolAllocations->addAllocation(this->sharedTimestampAllocation->getGraphicsAllocation());
                allocatedMemory = true;
            }
        }

        if (!allocatedMemory) {
            NEO::AllocationProperties allocationProperties{*rootDeviceIndices.begin(), this->eventPoolSize, allocationType, neoDevice->getDeviceBitfield()};
            allocationProperties.alignment = eventAlignment;
            allocationProperties.flags.uncacheable = neoDevice->getProductHelper().isDcFlushAllowed();

            auto memoryManager = driver->getMemoryManager();
            auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties);
            if (graphicsAllocation) {
                eventPoolAllocations->addAllocation(graphicsAllocation);
                allocatedMemory = true;
                if (isIpcPoolFlagSet()) {
                    uint64_t handle = 0;
                    this->isShareableEventMemory = (graphicsAllocation->peekInternalHandle(memoryManager, handle, nullptr) == 0);
                }
            }
        }
    } else {
        this->isHostVisibleEventPoolAllocation = true;
        NEO::AllocationProperties allocationProperties{*rootDeviceIndices.begin(), this->eventPoolSize, allocationType, NEO::systemMemoryBitfield};
        allocationProperties.alignment = eventAlignment;
        allocationProperties.flags.uncacheable = neoDevice->getProductHelper().isDcFlushAllowed();

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
        neoDevice->getDeviceTimestampPoolAllocator().free(this->sharedTimestampAllocation.release());
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

    ze_result_t result = ZE_RESULT_SUCCESS;
    *eventHandle = l0GfxCoreHelper.createEvent(this, desc, getDevice(), result);

    return result;
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

void EventPool::initializeSizeParameters(uint32_t numDevices, ze_device_handle_t *deviceHandles, DriverHandle &driver, const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {

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

ze_result_t Event::counterBasedCreate(ze_context_handle_t hContext, ze_device_handle_t hDevice, const ze_event_counter_based_desc_t *desc, ze_event_handle_t *phEvent) {
    constexpr uint32_t supportedBasedFlags = (ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE);

    auto device = Device::fromHandle(toInternalType(hDevice));
    auto counterBasedEventDesc = desc ? desc : &defaultIntelCounterBasedEventDesc;

    if (!hDevice || !phEvent) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    const bool ipcFlag = !!(counterBasedEventDesc->flags & ZE_EVENT_COUNTER_BASED_FLAG_IPC);
    const bool timestampFlag = !!(counterBasedEventDesc->flags & ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP);
    const bool mappedTimestampFlag = !!(counterBasedEventDesc->flags & ZE_EVENT_COUNTER_BASED_FLAG_HOST_TIMESTAMP);
    const bool externalEvent = !!(counterBasedEventDesc->flags & ZEX_COUNTER_BASED_EVENT_FLAG_EXTERNAL);

    uint32_t inputCbFlags = counterBasedEventDesc->flags & supportedBasedFlags;
    if (inputCbFlags == 0) {
        inputCbFlags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE;
    }

    if (ipcFlag && (timestampFlag || mappedTimestampFlag)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto signalScope = counterBasedEventDesc->signal;

    if (NEO::debugManager.flags.MitigateHostVisibleSignal.get()) {
        signalScope &= ~ZE_EVENT_SCOPE_FLAG_HOST;
    }

    EventDescriptor eventDescriptor = {
        .eventPoolAllocation = nullptr,
        .extensions = counterBasedEventDesc->pNext,
        .totalEventSize = 0,
        .maxKernelCount = device->getEventMaxKernelCount(),
        .maxPacketsCount = 1,
        .counterBasedFlags = inputCbFlags,
        .index = 0,
        .signalScope = signalScope,
        .waitScope = counterBasedEventDesc->wait,
        .timestampPool = timestampFlag,
        .kernelMappedTsPoolFlag = mappedTimestampFlag,
        .importedIpcPool = false,
        .ipcPool = ipcFlag,
        .externalEvent = externalEvent,
    };

    ze_result_t result = ZE_RESULT_SUCCESS;

    auto l0Event = device->getL0GfxCoreHelper().createStandaloneEvent(eventDescriptor, device, result);

    *phEvent = l0Event;

    return result;
}

ze_result_t Event::counterBasedGetDeviceAddress(ze_event_handle_t event, uint64_t *completionValue, uint64_t *address) {
    auto eventObj = Event::fromHandle(toInternalType(event));

    if (!eventObj || !completionValue || !address) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (eventObj->isCounterBased()) {
        if (!eventObj->getInOrderExecEventHelper().isDataAssigned()) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        *completionValue = eventObj->getInOrderExecBaseSignalValue();
        *address = eventObj->getInOrderExecEventHelper().getBaseDeviceAddress() + eventObj->getInOrderAllocationOffset();
    } else if (eventObj->isEventTimestampFlagSet()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    } else {
        *completionValue = Event::State::STATE_SIGNALED;
        *address = eventObj->getCompletionFieldGpuAddress(eventObj->peekEventPool()->getDevice());
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t Event::counterBasedGetIncrementValue(ze_device_handle_t hDevice, uint32_t *incrementValue) {
    auto device = Device::fromHandle(hDevice);
    if (!device || !incrementValue) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    *incrementValue = device->getAggregatedCopyOffloadIncrementValue();
    return ZE_RESULT_SUCCESS;
}

ze_result_t Event::counterBasedGetIpcHandle(ze_event_handle_t hEvent, ze_ipc_event_counter_based_handle_t *phIpc) {
    auto event = Event::fromHandle(hEvent);
    if (!event || !phIpc || !event->isCounterBasedExplicitlyEnabled()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto ipcData = reinterpret_cast<IpcCounterBasedEventData *>(phIpc->data);

    return event->getCounterBasedIpcHandle(*ipcData);
}

ze_result_t Event::counterBasedOpenIpcHandle(ze_context_handle_t hContext, ze_ipc_event_counter_based_handle_t hIpc, ze_event_handle_t *phEvent) {
    auto context = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    if (!context || !phEvent) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto ipcData = reinterpret_cast<IpcCounterBasedEventData *>(hIpc.data);

    return context->openCounterBasedIpcHandle(*ipcData, phEvent);
}

ze_result_t Event::openCounterBasedIpcHandle(const IpcCounterBasedEventData &ipcData, ze_event_handle_t *eventHandle,
                                             DriverHandle *driver, ContextImp *context, uint32_t numDevices, ze_device_handle_t *deviceHandles) {

    auto device = Device::fromHandle(*deviceHandles);
    auto neoDevice = device->getNEODevice();
    auto memoryManager = driver->getMemoryManager();

    NEO::MemoryManager::OsHandleData deviceOsHandleData{ipcData.deviceHandle};
    NEO::MemoryManager::OsHandleData hostOsHandleData{ipcData.hostHandle};

    NEO::AllocationProperties unifiedMemoryProperties{ipcData.rootDeviceIndex, MemoryConstants::pageSize64k, NEO::DeviceAllocNodeType<true>::getAllocationType(), NEO::systemMemoryBitfield};
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

    event->getInOrderExecEventHelper().assignData(ipcData.counterValue, ipcData.counterOffset, ipcData.devicePartitions, ipcData.hostPartitions, deviceAlloc, hostAlloc, deviceAlloc->getGpuAddress(),
                                                  static_cast<uint64_t *>(hostAlloc->getUnderlyingBuffer()), 0, 0, (deviceAlloc != hostAlloc), true);

    *eventHandle = event;

    return result;
}

ze_result_t Event::getCounterBasedIpcHandle(IpcCounterBasedEventData &ipcData) {
    if (!this->isSharableCounterBased) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (!isCounterBasedExplicitlyEnabled() || !inOrderExecHelper.isDataAssigned() || isEventTimestampFlagSet() || inOrderExecHelper.isFromExternalMemory()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    ipcData = {};
    ipcData.rootDeviceIndex = device->getRootDeviceIndex();
    ipcData.counterValue = inOrderExecHelper.getEventData()->counterValue;
    ipcData.counterBasedFlags = this->counterBasedFlags;
    ipcData.signalScopeFlags = this->signalScope;
    ipcData.waitScopeFlags = this->waitScope;

    auto memoryManager = device->getNEODevice()->getMemoryManager();
    auto deviceAlloc = inOrderExecHelper.getDeviceCounterAllocation();

    uint64_t handle = 0;
    if (int retCode = deviceAlloc->peekInternalHandle(memoryManager, handle, nullptr); retCode != 0) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    memoryManager->registerIpcExportedAllocation(deviceAlloc);

    ipcData.deviceHandle = handle;
    ipcData.devicePartitions = inOrderExecHelper.getEventData()->devicePartitions;
    ipcData.hostPartitions = ipcData.devicePartitions;

    ipcData.counterOffset = static_cast<uint32_t>(inOrderExecHelper.getBaseDeviceAddress() - deviceAlloc->getGpuAddress()) + inOrderExecHelper.getEventData()->counterOffset;

    if (inOrderExecHelper.isHostStorageDuplicated()) {
        auto hostAlloc = inOrderExecHelper.getHostCounterAllocation();
        if (int retCode = hostAlloc->peekInternalHandle(memoryManager, handle, nullptr); retCode != 0) {
            return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        }
        memoryManager->registerIpcExportedAllocation(hostAlloc);

        ipcData.hostHandle = handle;
        ipcData.hostPartitions = inOrderExecHelper.getEventData()->hostPartitions;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t EventPool::getIpcHandle(ze_ipc_event_pool_handle_t *ipcHandle) {
    if (!this->isShareableEventMemory) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    auto memoryManager = context->getDriverHandle()->getMemoryManager();
    auto allocation = eventPoolAllocations->getDefaultGraphicsAllocation();
    uint64_t handle{};
    if (allocation->peekInternalHandle(memoryManager, handle, nullptr) != 0) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }
    memoryManager->registerIpcExportedAllocation(allocation);

    IpcOpaqueEventPoolData &ipcData = *reinterpret_cast<IpcOpaqueEventPoolData *>(ipcHandle->data);
    ipcData = {};
    ipcData.handle.val = handle;
    ipcData.numEvents = numEvents;
    ipcData.rootDeviceIndex = getDevice()->getRootDeviceIndex();
    ipcData.maxEventPackets = getEventMaxPackets();
    ipcData.numDevices = static_cast<uint16_t>(devices.size());
    ipcData.isDeviceEventPoolAllocation = isDeviceEventPoolAllocation;
    ipcData.isHostVisibleEventPoolAllocation = isHostVisibleEventPoolAllocation;
    ipcData.isImplicitScalingCapable = isImplicitScalingCapable;
    ipcData.isEventPoolKernelMappedTsFlagSet = isEventPoolKernelMappedTsFlagSet();
    ipcData.isEventPoolTsFlagSet = isEventPoolTimestampFlagSet();
    if (context->settings.useOpaqueHandle) {
        ipcData.type = context->settings.handleType;
        ipcData.processId = NEO::SysCalls::getCurrentProcessId();
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t EventPool::openEventPoolIpcHandle(const ze_ipc_event_pool_handle_t &ipcEventPoolHandle, ze_event_pool_handle_t *eventPoolHandle,
                                              DriverHandle *driver, ContextImp *context, uint32_t numDevices, ze_device_handle_t *deviceHandles) {
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

    uint32_t parentID = 0;
    uint32_t shareWithNoNTHandle = 0;
    if (context->settings.useOpaqueHandle) {
        IpcOpaqueEventPoolData ipcData = *reinterpret_cast<const IpcOpaqueEventPoolData *>(ipcEventPoolHandle.data);
        parentID = ipcData.processId;
        if (NEO::debugManager.flags.EnableShareableWithoutNTHandle.get()) {
            auto &productHelper = neoDevice->getProductHelper();
            shareWithNoNTHandle = productHelper.canShareMemoryWithoutNTHandle();
        }
    }
    osHandleData.parentProcessId = parentID;

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
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(),
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
                                                      NEO::systemMemoryBitfield};

    unifiedMemoryProperties.flags.shareableWithoutNTHandle = shareWithNoNTHandle;
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

ze_result_t Event::destroy() {
    resetInOrderTimestampNode(nullptr, 0);
    resetAdditionalTimestampNode(nullptr, 0, true);
    inOrderExecHelper.releaseNotUsedTempTimestampNodes();

    if (isCounterBasedExplicitlyEnabled() && isFromIpcPool) {
        auto memoryManager = device->getNEODevice()->getMemoryManager();

        memoryManager->freeGraphicsMemory(inOrderExecHelper.getDeviceCounterAllocation());
        if (inOrderExecHelper.isHostStorageDuplicated()) {
            memoryManager->freeGraphicsMemory(inOrderExecHelper.getHostCounterAllocation());
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
    if (inOrderExecHelper.hasTimestampNodes()) {
        return inOrderExecHelper.getLatestTimestampNode()->getGpuAddress();
    }
    return getAllocation(device)->getGpuAddress() + this->eventPoolOffset + this->offsetInSharedAlloc;
}

void *Event::getHostAddress() const {
    if (inOrderExecHelper.hasTimestampNodes()) {
        return inOrderExecHelper.getLatestTimestampNode()->getCpuBase();
    }

    return this->hostAddressFromPool;
}

NEO::GraphicsAllocation *Event::getAllocation(Device *device) const {
    auto rootDeviceIndex = device->getNEODevice()->getRootDeviceIndex();

    if (inOrderExecHelper.hasTimestampNodes()) {
        return inOrderExecHelper.getLatestTimestampNode()->getBaseGraphicsAllocation()->getGraphicsAllocation(rootDeviceIndex);
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

    inOrderExecHelper.updateInOrderExecState(newInOrderExecInfo, signalValue, allocationOffset);
}

void Event::updateInOrdeState(NEO::InOrderExecEventHelper &input) {
    resetCompletionStatus();

    input.copyData(this->inOrderExecHelper);
}

uint64_t Event::getInOrderIncrementValue(uint32_t partitionCount) const {
    DEBUG_BREAK_IF(inOrderExecHelper.getIncrementValue() % partitionCount != 0);
    return (inOrderExecHelper.getIncrementValue() / partitionCount);
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
    inOrderExecHelper.unsetInOrderExecInfo();
}

void Event::resetInOrderTimestampNode(NEO::TagNodeBase *newNode, uint32_t partitionCount) {
    if (inOrderExecHelper.getIncrementValue() == 0 || !newNode) {
        inOrderExecHelper.moveTimestampNodeToReleaseList();
    }

    if (newNode) {
        inOrderExecHelper.addTimestampNode(newNode);
        if (NEO::debugManager.flags.ClearStandaloneInOrderTimestampAllocation.get() != 0) {
            clearTimestampTagData(partitionCount, inOrderExecHelper.getLatestTimestampNode());
        }
    }
}

void Event::resetAdditionalTimestampNode(NEO::TagNodeBase *newNode, uint32_t partitionCount, bool resetAggregatedEvent) {
    if (inOrderExecHelper.getIncrementValue() > 0) {
        if (newNode) {
            inOrderExecHelper.addAdditionalTimestampNode(newNode);
            if (NEO::debugManager.flags.ClearStandaloneInOrderTimestampAllocation.get() != 0) {
                clearTimestampTagData(partitionCount, newNode);
            }
        } else if (resetAggregatedEvent) {
            // If we are resetting aggregated event, we need to clear all additional timestamp nodes
            inOrderExecHelper.moveAdditionalTimestampNodesToReleaseList();
        }

        return;
    }

    inOrderExecHelper.moveAdditionalTimestampNodesToReleaseList();

    if (newNode) {
        inOrderExecHelper.addAdditionalTimestampNode(newNode);
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
        auto stype = static_cast<uint32_t>(extendedDesc->stype);
        if (stype == ZEX_INTEL_STRUCTURE_TYPE_EVENT_SYNC_MODE_EXP_DESC || stype == ZE_STRUCTURE_TYPE_EVENT_SYNC_MODE_DESC) {
            uint32_t externalInterruptId = 0;
            if (stype == ZE_STRUCTURE_TYPE_EVENT_SYNC_MODE_DESC) {
                auto eventSyncModeDesc = reinterpret_cast<const ze_event_sync_mode_desc_t *>(extendedDesc);
                interruptMode = (eventSyncModeDesc->syncModeFlags & ZE_EVENT_SYNC_MODE_FLAG_SIGNAL_INTERRUPT);
                kmdWaitMode = (eventSyncModeDesc->syncModeFlags & ZE_EVENT_SYNC_MODE_FLAG_LOW_POWER_WAIT);
                externalInterruptWait = (eventSyncModeDesc->syncModeFlags & ZE_EVENT_SYNC_MODE_FLAG_EXTERNAL_INTERRUPT_WAIT);
                externalInterruptId = eventSyncModeDesc->externalInterruptId;
            } else {
                auto eventSyncModeDesc = reinterpret_cast<const zex_intel_event_sync_mode_exp_desc_t *>(extendedDesc);
                interruptMode = (eventSyncModeDesc->syncModeFlags & ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_SIGNAL_INTERRUPT);
                kmdWaitMode = (eventSyncModeDesc->syncModeFlags & ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_LOW_POWER_WAIT);
                externalInterruptWait = (eventSyncModeDesc->syncModeFlags & ZEX_INTEL_EVENT_SYNC_MODE_EXP_FLAG_EXTERNAL_INTERRUPT_WAIT);
                externalInterruptId = eventSyncModeDesc->externalInterruptId;
            }

            if (interruptMode && !device->getProductHelper().isInterruptSupported(device->getNEODevice()->getRootDeviceEnvironment())) {
                return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }

            if (externalInterruptWait) {
                setExternalInterruptId(externalInterruptId);
                UNRECOVERABLE_IF(externalInterruptId > 0 && eventDescriptor.eventPoolAllocation);
            }
        } else if (stype == ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_SYNC_ALLOC_PROPERTIES || stype == ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_SYNC_ALLOCATION_DESC) {
            uint64_t *deviceAddress = nullptr;
            uint64_t *hostAddress = nullptr;
            uint64_t completionValue = 0;

            if (stype == ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_SYNC_ALLOCATION_DESC) {
                auto externalSyncAllocProperties = reinterpret_cast<const ze_event_counter_based_external_sync_allocation_desc_t *>(extendedDesc);
                deviceAddress = externalSyncAllocProperties->deviceAddress;
                hostAddress = externalSyncAllocProperties->hostAddress;
                completionValue = externalSyncAllocProperties->completionValue;
            } else {
                auto externalSyncAllocProperties = reinterpret_cast<const zex_counter_based_event_external_sync_alloc_properties_t *>(extendedDesc);
                deviceAddress = externalSyncAllocProperties->deviceAddress;
                hostAddress = externalSyncAllocProperties->hostAddress;
                completionValue = externalSyncAllocProperties->completionValue;
            }

            if (!deviceAddress || !hostAddress) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }

            auto deviceAlloc = getExternalCounterAllocationFromAddress(deviceAddress);
            auto hostAlloc = getExternalCounterAllocationFromAddress(hostAddress);

            if (!hostAlloc) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }

            inOrderExecHelper.assignData(completionValue, 0, 1, 1, deviceAlloc, hostAlloc, castToUint64(deviceAddress), hostAddress, 0, 0, (deviceAlloc != hostAlloc), true);

            disableHostCaching(true);
        } else if (stype == ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_STORAGE_ALLOC_PROPERTIES || stype == ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_AGGREGATE_STORAGE_DESC) {
            uint64_t *deviceAddress = nullptr;
            uint64_t completionValue = 0;
            uint64_t incrementValue = 0;

            if (stype == ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_AGGREGATE_STORAGE_DESC) {
                auto externalStorageProperties = reinterpret_cast<const ze_event_counter_based_external_aggregate_storage_desc_t *>(extendedDesc);
                deviceAddress = externalStorageProperties->deviceAddress;
                completionValue = externalStorageProperties->completionValue;
                incrementValue = externalStorageProperties->incrementValue;
            } else {
                auto externalStorageProperties = reinterpret_cast<const zex_counter_based_event_external_storage_properties_t *>(extendedDesc);
                deviceAddress = externalStorageProperties->deviceAddress;
                completionValue = externalStorageProperties->completionValue;
                incrementValue = externalStorageProperties->incrementValue;
            }
            auto deviceAlloc = getExternalCounterAllocationFromAddress(deviceAddress);

            if (!deviceAlloc || incrementValue == 0) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }

            auto offset = ptrDiff(deviceAddress, deviceAlloc->getGpuAddress());
            auto hostAddress = ptrOffset(device->getNEODevice()->getMemoryManager()->lockResource(deviceAlloc), offset);

            inOrderExecHelper.assignData(completionValue, 0, 1, 1, deviceAlloc, deviceAlloc, castToUint64(deviceAddress), reinterpret_cast<uint64_t *>(hostAddress), incrementValue, 0, false, true);
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

NEO::InOrderExecEventHelper &Event::getInOrderExecEventHelper() { return inOrderExecHelper; }

uint64_t Event::getInOrderExecBaseSignalValue() const { return inOrderExecHelper.getEventData()->counterValue; }

uint32_t Event::getInOrderAllocationOffset() const { return inOrderExecHelper.getEventData()->counterOffset; }

bool Event::hasInOrderTimestampNode() const { return inOrderExecHelper.hasTimestampNodes(); }

} // namespace L0
