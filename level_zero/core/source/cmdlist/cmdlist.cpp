/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist.h"

#include "shared/source/command_stream/preemption.h"
#include "shared/source/device/device_info.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace L0 {

CommandList::~CommandList() {
    if (cmdQImmediate) {
        cmdQImmediate->destroy();
    }
    removeDeallocationContainerData();
    removeHostPtrAllocations();
    printfFunctionContainer.clear();
}
void CommandList::storePrintfFunction(Kernel *kernel) {
    auto it = std::find(this->printfFunctionContainer.begin(), this->printfFunctionContainer.end(),
                        kernel);

    if (it == this->printfFunctionContainer.end()) {
        this->printfFunctionContainer.push_back(kernel);
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
    return nullptr;
}

NEO::GraphicsAllocation *CommandList::getHostPtrAlloc(const void *buffer, uint64_t bufferSize, size_t *offset) {
    NEO::GraphicsAllocation *alloc = getAllocationFromHostPtrMap(buffer, bufferSize);
    if (alloc) {
        *offset += ptrDiff(buffer, alloc->getUnderlyingBuffer());
        return alloc;
    }
    alloc = device->allocateMemoryFromHostPtr(buffer, bufferSize);
    hostPtrMap.insert(std::make_pair(buffer, alloc));
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
        if (!((deallocation->getAllocationType() == NEO::GraphicsAllocation::AllocationType::INTERNAL_HEAP) ||
              (deallocation->getAllocationType() == NEO::GraphicsAllocation::AllocationType::LINEAR_STREAM))) {
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

bool CommandList::isCopyOnly() const {
    return isCopyOnlyCmdList;
}

NEO::PreemptionMode CommandList::obtainFunctionPreemptionMode(Kernel *kernel) {
    auto functionAttributes = kernel->getImmutableData()->getDescriptor().kernelAttributes;
    NEO::PreemptionFlags flags = {};
    flags.flags.disabledMidThreadPreemptionKernel = functionAttributes.flags.requiresDisabledMidThreadPreemption;
    flags.flags.usesFencesForReadWriteImages = functionAttributes.flags.usesFencesForReadWriteImages;
    flags.flags.deviceSupportsVmePreemption = device->getDeviceInfo().vmeAvcSupportsPreemption;
    flags.flags.disablePerCtxtPreemptionGranularityControl = device->getHwInfo().workaroundTable.waDisablePerCtxtPreemptionGranularityControl;
    flags.flags.disableLSQCROPERFforOCL = device->getHwInfo().workaroundTable.waDisableLSQCROPERFforOCL;

    return NEO::PreemptionHelper::taskPreemptionMode(device->getDevicePreemptionMode(), flags);
}

} // namespace L0
