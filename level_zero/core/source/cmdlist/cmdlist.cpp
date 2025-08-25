/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device_info.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/prefetch_manager.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/experimental/source/graph/graph.h"

#include "implicit_args.h"

namespace L0 {

CommandList::~CommandList() {
    if (cmdQImmediate) {
        cmdQImmediate->destroy();
    }
    if (cmdQImmediateCopyOffload) {
        cmdQImmediateCopyOffload->destroy();
    }
    removeDeallocationContainerData();
    if (!isImmediateType()) {
        removeHostPtrAllocations();
    }
    removeMemoryPrefetchAllocations();
    printfKernelContainer.clear();
    if (captureTarget && (false == captureTarget->wasPreallocated())) {
        delete captureTarget;
    }
}

void CommandList::storePrintfKernel(Kernel *kernel) {
    auto it = std::find_if(this->printfKernelContainer.begin(), this->printfKernelContainer.end(), [&kernel](const auto &kernelWeakPtr) { return kernelWeakPtr.lock().get() == kernel; });

    if (it == this->printfKernelContainer.end()) {
        auto module = static_cast<const ModuleImp *>(&static_cast<KernelImp *>(kernel)->getParentModule());
        this->printfKernelContainer.push_back(module->getPrintfKernelWeakPtr(kernel->toHandle()));
    }
}

void CommandList::removeHostPtrAllocations() {
    auto memoryManager = device ? device->getNEODevice()->getMemoryManager() : nullptr;

    bool restartDirectSubmission = !this->hostPtrMap.empty() && memoryManager && device->getNEODevice()->getRootDeviceEnvironment().getProductHelper().restartDirectSubmissionForHostptrFree();
    if (restartDirectSubmission) {
        const auto &engines = memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
        for (const auto &engine : engines) {
            engine.commandStreamReceiver->stopDirectSubmission(false, true);
        }
    }

    for (auto &allocation : hostPtrMap) {
        UNRECOVERABLE_IF(memoryManager == nullptr);
        memoryManager->freeGraphicsMemory(allocation.second);
    }

    if (restartDirectSubmission) {
        const auto &engines = memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
        for (const auto &engine : engines) {
            if (engine.commandStreamReceiver->isAnyDirectSubmissionEnabled()) {
                engine.commandStreamReceiver->flushTagUpdate();
            }
        }
    }

    hostPtrMap.clear();
}

void CommandList::removeMemoryPrefetchAllocations() {
    if (this->performMemoryPrefetch) {
        auto prefetchManager = this->device->getDriverHandle()->getMemoryManager()->getPrefetchManager();
        if (prefetchManager) {
            prefetchManager->removeAllocations(prefetchContext);
        }
        performMemoryPrefetch = false;
    }
}

void CommandList::storeFillPatternResourcesForReuse() {
    for (auto &patternAlloc : this->patternAllocations) {
        device->storeReusableAllocation(*patternAlloc);
    }
    this->patternAllocations.clear();
    for (auto &patternTag : this->patternTags) {
        patternTag->returnTag();
    }
    this->patternTags.clear();
}

NEO::GraphicsAllocation *CommandList::getAllocationFromHostPtrMap(const void *buffer, uint64_t bufferSize, bool copyOffload) {
    auto allocation = hostPtrMap.lower_bound(buffer);
    if (allocation != hostPtrMap.end()) {
        if (buffer == allocation->first && ptrOffset(allocation->first, allocation->second->getUnderlyingBufferSize()) >= ptrOffset(buffer, bufferSize)) {
            return allocation->second;
        }
    }
    if (allocation != hostPtrMap.begin()) {
        allocation--;
        if (ptrOffset(allocation->first, allocation->second->getUnderlyingBufferSize()) >= ptrOffset(buffer, bufferSize)) {
            return allocation->second;
        }
    }
    if (isImmediateType()) {
        auto csr = getCsr(copyOffload);
        auto allocation = csr->getInternalAllocationStorage()->obtainTemporaryAllocationWithPtr(bufferSize, buffer, NEO::AllocationType::externalHostPtr);
        if (allocation != nullptr) {
            auto alloc = allocation.get();
            alloc->hostPtrTaskCountAssignment++;
            csr->getInternalAllocationStorage()->storeAllocationWithTaskCount(std::move(allocation), NEO::AllocationUsage::TEMPORARY_ALLOCATION, csr->peekTaskCount());
            return alloc;
        }
    }
    return nullptr;
}

NEO::GraphicsAllocation *CommandList::getHostPtrAlloc(const void *buffer, uint64_t bufferSize, bool hostCopyAllowed, bool copyOffload) {
    NEO::GraphicsAllocation *alloc = getAllocationFromHostPtrMap(buffer, bufferSize, copyOffload);
    if (alloc) {
        return alloc;
    }
    alloc = device->allocateMemoryFromHostPtr(buffer, bufferSize, hostCopyAllowed);
    if (alloc == nullptr) {
        return nullptr;
    }
    if (isImmediateType()) {
        alloc->hostPtrTaskCountAssignment++;
        auto csr = getCsr(copyOffload);
        csr->getInternalAllocationStorage()->storeAllocationWithTaskCount(std::unique_ptr<NEO::GraphicsAllocation>(alloc), NEO::AllocationUsage::TEMPORARY_ALLOCATION, csr->peekTaskCount());
    } else if (alloc->getAllocationType() == NEO::AllocationType::externalHostPtr) {
        hostPtrMap.insert(std::make_pair(buffer, alloc));
    } else {
        commandContainer.getDeallocationContainer().push_back(alloc);
    }
    return alloc;
}

void CommandList::removeDeallocationContainerData() {
    auto memoryManager = device ? device->getNEODevice()->getMemoryManager() : nullptr;

    auto container = commandContainer.getDeallocationContainer();
    for (auto &deallocation : container) {
        DEBUG_BREAK_IF(deallocation == nullptr);
        UNRECOVERABLE_IF(memoryManager == nullptr);
        NEO::SvmAllocationData *allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(reinterpret_cast<void *>(deallocation->getGpuAddress()));
        if (allocData) {
            device->getDriverHandle()->getSvmAllocsManager()->removeSVMAlloc(*allocData);
        }
        if (!((deallocation->getAllocationType() == NEO::AllocationType::internalHeap) ||
              (deallocation->getAllocationType() == NEO::AllocationType::linearStream))) {
            memoryManager->freeGraphicsMemory(deallocation);
            eraseDeallocationContainerEntry(deallocation);
        }
    }
}

void CommandList::eraseDeallocationContainerEntry(NEO::GraphicsAllocation *allocation) {
    std::vector<NEO::GraphicsAllocation *>::iterator allocErase;
    auto container = &commandContainer.getDeallocationContainer();

    allocErase = std::find(container->begin(), container->end(), allocation);
    if (allocErase != container->end()) {
        container->erase(allocErase);
    }
}

void CommandList::eraseResidencyContainerEntry(NEO::GraphicsAllocation *allocation) {
    std::vector<NEO::GraphicsAllocation *>::iterator allocErase;
    auto container = &commandContainer.getResidencyContainer();

    allocErase = std::find(container->begin(), container->end(), allocation);
    if (allocErase != container->end()) {
        container->erase(allocErase);
    }
}

void CommandList::migrateSharedAllocations() {
    auto deviceImp = static_cast<DeviceImp *>(device);
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(deviceImp->getDriverHandle());
    std::lock_guard<std::mutex> lock(driverHandleImp->sharedMakeResidentAllocationsLock);
    auto pageFaultManager = device->getDriverHandle()->getMemoryManager()->getPageFaultManager();
    for (auto &alloc : driverHandleImp->sharedMakeResidentAllocations) {
        pageFaultManager->moveAllocationToGpuDomain(reinterpret_cast<void *>(alloc.second->getGpuAddress()));
    }
    if (this->unifiedMemoryControls.indirectSharedAllocationsAllowed) {
        auto pageFaultManager = device->getDriverHandle()->getMemoryManager()->getPageFaultManager();
        pageFaultManager->moveAllocationsWithinUMAllocsManagerToGpuDomain(this->device->getDriverHandle()->getSvmAllocsManager());
    }
}

bool CommandList::isTimestampEventForMultiTile(Event *signalEvent) {
    if (this->partitionCount > 1 && signalEvent && signalEvent->isEventTimestampFlagSet()) {
        return true;
    }

    return false;
}

bool CommandList::setupTimestampEventForMultiTile(Event *signalEvent) {
    if (isTimestampEventForMultiTile(signalEvent)) {
        signalEvent->setPacketsInUse(this->partitionCount);
        return true;
    }
    return false;
}

void CommandList::synchronizeEventList(uint32_t numWaitEvents, ze_event_handle_t *waitEventList) {
    for (uint32_t i = 0; i < numWaitEvents; i++) {
        Event *event = Event::fromHandle(waitEventList[i]);
        event->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }
}

NEO::CommandStreamReceiver *CommandList::getCsr(bool copyOffload) const {
    auto queue = isDualStreamCopyOffloadOperation(copyOffload) ? this->cmdQImmediateCopyOffload : this->cmdQImmediate;

    return static_cast<CommandQueueImp *>(queue)->getCsr();
}

void CommandList::registerWalkerWithProfilingEnqueued(Event *event) {
    if (this->shouldRegisterEnqueuedWalkerWithProfiling && event && event->isEventTimestampFlagSet()) {
        this->isWalkerWithProfilingEnqueued = true;
    }
}

ze_result_t CommandList::setKernelState(Kernel *kernel, const ze_group_size_t groupSizes, void **arguments) {
    if (kernel == nullptr) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    auto result = kernel->setGroupSize(groupSizes.groupSizeX, groupSizes.groupSizeY, groupSizes.groupSizeZ);

    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    auto &args = kernel->getImmutableData()->getDescriptor().payloadMappings.explicitArgs;

    if (args.size() > 0 && !arguments) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }
    for (auto i = 0u; i < args.size(); i++) {

        auto &arg = args[i];
        auto argSize = sizeof(void *);
        auto argValue = arguments[i];

        switch (arg.type) {
        case NEO::ArgDescriptor::argTPointer:
            if (arg.getTraits().getAddressQualifier() == NEO::KernelArgMetadata::AddrLocal) {
                argSize = *reinterpret_cast<const size_t *>(argValue);
                argValue = nullptr;
            }
            break;
        case NEO::ArgDescriptor::argTValue:
            argSize = std::numeric_limits<size_t>::max();
            break;
        default:
            break;
        }
        result = kernel->setArgumentValue(i, argSize, argValue);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
