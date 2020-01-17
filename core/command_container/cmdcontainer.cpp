/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_container/cmdcontainer.h"

#include "core/command_container/command_encoder.h"
#include "core/command_stream/linear_stream.h"
#include "core/helpers/debug_helpers.h"
#include "core/helpers/heap_helper.h"
#include "core/indirect_heap/indirect_heap.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"
#include "runtime/memory_manager/memory_manager.h"

namespace NEO {

CommandContainer::~CommandContainer() {
    if (!device) {
        DEBUG_BREAK_IF(device);
        return;
    }

    auto memoryManager = device->getMemoryManager();

    for (auto *alloc : cmdBufferAllocations) {
        memoryManager->freeGraphicsMemory(alloc);
    }

    for (auto allocationIndirectHeap : allocationIndirectHeaps) {
        heapHelper->storeHeapAllocation(allocationIndirectHeap);
    }
    for (auto deallocation : deallocationContainer) {
        if (((deallocation->getAllocationType() == GraphicsAllocation::AllocationType::INTERNAL_HEAP) || (deallocation->getAllocationType() == GraphicsAllocation::AllocationType::LINEAR_STREAM))) {
            getHeapHelper()->storeHeapAllocation(deallocation);
        }
    }
    residencyContainer.clear();
    deallocationContainer.clear();
}

bool CommandContainer::initialize(Device *device) {
    if (!device) {
        DEBUG_BREAK_IF(device);
        return false;
    }
    this->device = device;

    heapHelper = std::unique_ptr<HeapHelper>(new HeapHelper(device->getMemoryManager(), device->getDefaultEngine().commandStreamReceiver->getInternalAllocationStorage(), device->getNumAvailableDevices() > 1u));

    size_t alignedSize = alignUp<size_t>(totalCmdBufferSize, MemoryConstants::pageSize64k);
    AllocationProperties properties{0u,
                                    true /* allocateMemory*/,
                                    alignedSize,
                                    GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                    (device->getNumAvailableDevices() > 1u) /* multiOsContextCapable */,
                                    false,
                                    {}};

    auto cmdBufferAllocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    UNRECOVERABLE_IF(!cmdBufferAllocation);

    cmdBufferAllocations.push_back(cmdBufferAllocation);

    commandStream = std::unique_ptr<LinearStream>(new LinearStream(cmdBufferAllocation->getUnderlyingBuffer(),
                                                                   defaultListCmdBufferSize));
    commandStream->replaceGraphicsAllocation(cmdBufferAllocation);

    addToResidencyContainer(cmdBufferAllocation);
    constexpr size_t heapSize = 65536u;

    for (uint32_t i = 0; i < IndirectHeap::Type::NUM_TYPES; i++) {
        allocationIndirectHeaps[i] = heapHelper->getHeapAllocation(i, heapSize, alignedSize, 0u);
        UNRECOVERABLE_IF(!allocationIndirectHeaps[i]);
        residencyContainer.push_back(allocationIndirectHeaps[i]);

        bool requireInternalHeap = (IndirectHeap::INDIRECT_OBJECT == i);
        indirectHeaps[i] = std::make_unique<IndirectHeap>(allocationIndirectHeaps[i], requireInternalHeap);
    }

    instructionHeapBaseAddress = device->getMemoryManager()->getInternalHeapBaseAddress(0);

    iddBlock = nullptr;
    nextIddInBlock = numIddsPerBlock;

    return true;
}

void CommandContainer::addToResidencyContainer(GraphicsAllocation *alloc) {
    if (alloc == nullptr) {
        return;
    }
    auto end = this->residencyContainer.end();
    bool isUnique = (end == std::find(this->residencyContainer.begin(), end, alloc));
    if (isUnique == false) {
        return;
    }

    this->residencyContainer.push_back(alloc);
}

void CommandContainer::reset() {
    setDirtyStateForAllHeaps(true);
    slmSize = std::numeric_limits<uint32_t>::max();
    getResidencyContainer().clear();
    getDeallocationContainer().clear();

    for (size_t i = 1; i < cmdBufferAllocations.size(); i++) {
        device->getMemoryManager()->freeGraphicsMemory(cmdBufferAllocations[i]);
    }
    cmdBufferAllocations.erase(cmdBufferAllocations.begin() + 1, cmdBufferAllocations.end());

    commandStream->replaceBuffer(cmdBufferAllocations[0]->getUnderlyingBuffer(),
                                 defaultListCmdBufferSize);
    addToResidencyContainer(commandStream->getGraphicsAllocation());

    for (auto &indirectHeap : indirectHeaps) {
        indirectHeap->replaceBuffer(indirectHeap->getCpuBase(),
                                    indirectHeap->getMaxAvailableSpace());
        addToResidencyContainer(indirectHeap->getGraphicsAllocation());
    }
}

void *CommandContainer::getHeapSpaceAllowGrow(HeapType heapType,
                                              size_t size) {
    auto indirectHeap = getIndirectHeap(heapType);

    if (indirectHeap->getAvailableSpace() < size) {
        size_t newSize = indirectHeap->getUsed() + indirectHeap->getAvailableSpace();
        newSize *= 2;
        newSize = std::max(newSize, indirectHeap->getAvailableSpace() + size);
        newSize = alignUp(newSize, MemoryConstants::pageSize);
        auto oldAlloc = getIndirectHeapAllocation(heapType);
        auto newAlloc = getHeapHelper()->getHeapAllocation(heapType, newSize, MemoryConstants::pageSize, device->getRootDeviceIndex());
        UNRECOVERABLE_IF(!oldAlloc);
        UNRECOVERABLE_IF(!newAlloc);
        indirectHeap->replaceGraphicsAllocation(newAlloc);
        indirectHeap->replaceBuffer(newAlloc->getUnderlyingBuffer(),
                                    newAlloc->getUnderlyingBufferSize());
        getResidencyContainer().push_back(newAlloc);
        getDeallocationContainer().push_back(oldAlloc);
        setIndirectHeapAllocation(heapType, newAlloc);
        setHeapDirty(heapType);
    }
    return indirectHeap->getSpace(size);
}

IndirectHeap *CommandContainer::getHeapWithRequiredSizeAndAlignment(HeapType heapType, size_t sizeRequired, size_t alignment) {
    auto indirectHeap = getIndirectHeap(heapType);
    auto sizeRequested = sizeRequired;

    auto heapBuffer = indirectHeap->getSpace(0);
    if (alignment && (heapBuffer != alignUp(heapBuffer, alignment))) {
        sizeRequested += alignment;
    }

    if (indirectHeap->getAvailableSpace() < sizeRequested) {
        size_t newSize = indirectHeap->getUsed() + indirectHeap->getAvailableSpace();
        newSize = alignUp(newSize, MemoryConstants::pageSize);
        auto oldAlloc = getIndirectHeapAllocation(heapType);
        auto newAlloc = getHeapHelper()->getHeapAllocation(heapType, newSize, MemoryConstants::pageSize, device->getRootDeviceIndex());
        UNRECOVERABLE_IF(!oldAlloc);
        UNRECOVERABLE_IF(!newAlloc);
        indirectHeap->replaceGraphicsAllocation(newAlloc);
        indirectHeap->replaceBuffer(newAlloc->getUnderlyingBuffer(),
                                    newAlloc->getUnderlyingBufferSize());
        getResidencyContainer().push_back(newAlloc);
        getDeallocationContainer().push_back(oldAlloc);
        setIndirectHeapAllocation(heapType, newAlloc);
        setHeapDirty(heapType);
    }

    if (alignment) {
        indirectHeap->align(alignment);
    }
    return indirectHeap;
}

void CommandContainer::allocateNextCommandBuffer() {
    size_t alignedSize = alignUp<size_t>(totalCmdBufferSize, MemoryConstants::pageSize64k);
    AllocationProperties properties{0u,
                                    true /* allocateMemory*/,
                                    alignedSize,
                                    GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                    (device->getNumAvailableDevices() > 1u) /* multiOsContextCapable */,
                                    false,
                                    {}};

    auto cmdBufferAllocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    UNRECOVERABLE_IF(!cmdBufferAllocation);

    cmdBufferAllocations.push_back(cmdBufferAllocation);

    commandStream->replaceBuffer(cmdBufferAllocation->getUnderlyingBuffer(), defaultListCmdBufferSize);
    commandStream->replaceGraphicsAllocation(cmdBufferAllocation);

    addToResidencyContainer(cmdBufferAllocation);
}

} // namespace NEO
