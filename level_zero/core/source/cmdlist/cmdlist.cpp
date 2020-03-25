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
void CommandList::storePrintfFunction(Kernel *function) {
    auto it = std::find(this->printfFunctionContainer.begin(), this->printfFunctionContainer.end(),
                        function);

    if (it == this->printfFunctionContainer.end()) {
        this->printfFunctionContainer.push_back(function);
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

void CommandList::removeDeallocationContainerData() {
    auto memoryManager = device ? device->getNEODevice()->getMemoryManager() : nullptr;

    auto container = commandContainer.getDeallocationContainer();
    for (auto deallocation : container) {
        DEBUG_BREAK_IF(deallocation == nullptr);
        UNRECOVERABLE_IF(memoryManager == nullptr);
        NEO::SvmAllocationData *allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(reinterpret_cast<void *>(deallocation->getGpuAddress()));
        if (allocData) {
            device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->remove(*allocData);
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

NEO::PreemptionMode CommandList::obtainFunctionPreemptionMode(Kernel *function) {
    auto functionAttributes = function->getImmutableData()->getDescriptor().kernelAttributes;

    NEO::PreemptionFlags flags = {};
    flags.flags.disabledMidThreadPreemptionKernel = functionAttributes.flags.requiresDisabledMidThreadPreemption;
    flags.flags.usesFencesForReadWriteImages = functionAttributes.flags.usesFencesForReadWriteImages;
    flags.flags.deviceSupportsVmePreemption = device->getDeviceInfo().vmeAvcSupportsPreemption;
    flags.flags.disablePerCtxtPreemptionGranularityControl = device->getHwInfo().workaroundTable.waDisablePerCtxtPreemptionGranularityControl;
    flags.flags.disableLSQCROPERFforOCL = device->getHwInfo().workaroundTable.waDisableLSQCROPERFforOCL;

    return NEO::PreemptionHelper::taskPreemptionMode(device->getDevicePreemptionMode(), flags);
}

} // namespace L0
