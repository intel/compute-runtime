/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/device/device_info.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/kernel/kernel.h"

namespace L0 {

CommandList::~CommandList() {
    if (cmdQImmediate) {
        cmdQImmediate->destroy();
    }
    removeDeallocationContainerData();
    if (this->cmdListType == CommandListType::TYPE_REGULAR || !this->isFlushTaskSubmissionEnabled) {
        removeHostPtrAllocations();
    }
    printfKernelContainer.clear();
}

void CommandList::storePrintfKernel(Kernel *kernel) {
    auto it = std::find(this->printfKernelContainer.begin(), this->printfKernelContainer.end(),
                        kernel);

    if (it == this->printfKernelContainer.end()) {
        this->printfKernelContainer.push_back(kernel);
    }
}

void CommandList::removeHostPtrAllocations() {
    auto memoryManager = device ? device->getNEODevice()->getMemoryManager() : nullptr;
    for (auto &allocation : hostPtrMap) {
        UNRECOVERABLE_IF(memoryManager == nullptr);
        memoryManager->freeGraphicsMemory(allocation.second);
    }
    hostPtrMap.clear();
}

NEO::GraphicsAllocation *CommandList::getAllocationFromHostPtrMap(const void *buffer, uint64_t bufferSize) {
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
    if (this->storeExternalPtrAsTemporary()) {
        auto allocation = this->csr->getInternalAllocationStorage()->obtainTemporaryAllocationWithPtr(bufferSize, buffer, NEO::AllocationType::EXTERNAL_HOST_PTR);
        if (allocation != nullptr) {
            auto alloc = allocation.get();
            this->csr->getInternalAllocationStorage()->storeAllocation(std::move(allocation), NEO::AllocationUsage::TEMPORARY_ALLOCATION);
            return alloc;
        }
    }
    return nullptr;
}

bool CommandList::isWaitForEventsFromHostEnabled() {
    bool waitForEventsFromHostEnabled = false;
    if (NEO::DebugManager.flags.EventWaitOnHost.get() != -1) {
        waitForEventsFromHostEnabled = NEO::DebugManager.flags.EventWaitOnHost.get();
    }
    return waitForEventsFromHostEnabled;
}

NEO::GraphicsAllocation *CommandList::getHostPtrAlloc(const void *buffer, uint64_t bufferSize, bool hostCopyAllowed) {
    NEO::GraphicsAllocation *alloc = getAllocationFromHostPtrMap(buffer, bufferSize);
    if (alloc) {
        return alloc;
    }
    alloc = device->allocateMemoryFromHostPtr(buffer, bufferSize, hostCopyAllowed);
    UNRECOVERABLE_IF(alloc == nullptr);
    if (this->storeExternalPtrAsTemporary()) {
        this->csr->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<NEO::GraphicsAllocation>(alloc), NEO::AllocationUsage::TEMPORARY_ALLOCATION);
    } else if (alloc->getAllocationType() == NEO::AllocationType::EXTERNAL_HOST_PTR) {
        hostPtrMap.insert(std::make_pair(buffer, alloc));
    } else {
        commandContainer.getDeallocationContainer().push_back(alloc);
    }
    return alloc;
}

void CommandList::removeDeallocationContainerData() {
    auto memoryManager = device ? device->getNEODevice()->getMemoryManager() : nullptr;

    auto container = commandContainer.getDeallocationContainer();
    for (auto deallocation : container) {
        DEBUG_BREAK_IF(deallocation == nullptr);
        UNRECOVERABLE_IF(memoryManager == nullptr);
        NEO::SvmAllocationData *allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(reinterpret_cast<void *>(deallocation->getGpuAddress()));
        if (allocData) {
            device->getDriverHandle()->getSvmAllocsManager()->removeSVMAlloc(*allocData);
        }
        if (!((deallocation->getAllocationType() == NEO::AllocationType::INTERNAL_HEAP) ||
              (deallocation->getAllocationType() == NEO::AllocationType::LINEAR_STREAM))) {
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

NEO::PreemptionMode CommandList::obtainKernelPreemptionMode(Kernel *kernel) {
    NEO::PreemptionFlags flags = NEO::PreemptionHelper::createPreemptionLevelFlags(*device->getNEODevice(), &kernel->getImmutableData()->getDescriptor());
    return NEO::PreemptionHelper::taskPreemptionMode(device->getDevicePreemptionMode(), flags);
}

void CommandList::makeResidentAndMigrate(bool performMigration) {
    for (auto alloc : commandContainer.getResidencyContainer()) {
        csr->makeResident(*alloc);

        if (performMigration &&
            (alloc->getAllocationType() == NEO::AllocationType::SVM_GPU ||
             alloc->getAllocationType() == NEO::AllocationType::SVM_CPU)) {
            auto pageFaultManager = device->getDriverHandle()->getMemoryManager()->getPageFaultManager();
            pageFaultManager->moveAllocationToGpuDomain(reinterpret_cast<void *>(alloc->getGpuAddress()));
        }
    }
}

void CommandList::migrateSharedAllocations() {
    auto deviceImp = static_cast<DeviceImp *>(device);
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(deviceImp->getDriverHandle());
    std::lock_guard<std::mutex> lock(driverHandleImp->sharedMakeResidentAllocationsLock);
    auto pageFaultManager = device->getDriverHandle()->getMemoryManager()->getPageFaultManager();
    for (auto alloc : driverHandleImp->sharedMakeResidentAllocations) {
        pageFaultManager->moveAllocationToGpuDomain(reinterpret_cast<void *>(alloc.second->getGpuAddress()));
    }
    if (this->unifiedMemoryControls.indirectSharedAllocationsAllowed) {
        auto pageFaultManager = device->getDriverHandle()->getMemoryManager()->getPageFaultManager();
        pageFaultManager->moveAllocationsWithinUMAllocsManagerToGpuDomain(this->device->getDriverHandle()->getSvmAllocsManager());
    }
}

bool CommandList::setupTimestampEventForMultiTile(Event *signalEvent) {
    if (this->partitionCount > 1 &&
        signalEvent) {
        if (signalEvent->isEventTimestampFlagSet()) {
            signalEvent->setPacketsInUse(this->partitionCount);
            return true;
        }
    }
    return false;
}

} // namespace L0
