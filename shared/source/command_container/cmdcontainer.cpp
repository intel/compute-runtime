/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/heap_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {

CommandContainer::~CommandContainer() {
    if (!device) {
        DEBUG_BREAK_IF(device);
        return;
    }

    this->handleCmdBufferAllocations(0u);

    for (auto allocationIndirectHeap : allocationIndirectHeaps) {
        if (heapHelper) {
            heapHelper->storeHeapAllocation(allocationIndirectHeap);
        }
    }
    for (auto deallocation : deallocationContainer) {
        if (((deallocation->getAllocationType() == AllocationType::INTERNAL_HEAP) || (deallocation->getAllocationType() == AllocationType::LINEAR_STREAM))) {
            getHeapHelper()->storeHeapAllocation(deallocation);
        }
    }
}

CommandContainer::CommandContainer() {
    for (auto &indirectHeap : indirectHeaps) {
        indirectHeap = nullptr;
    }

    for (auto &allocationIndirectHeap : allocationIndirectHeaps) {
        allocationIndirectHeap = nullptr;
    }

    residencyContainer.reserve(startingResidencyContainerSize);
}

CommandContainer::CommandContainer(uint32_t maxNumAggregatedIdds) : CommandContainer() {
    numIddsPerBlock = maxNumAggregatedIdds;
}

ErrorCode CommandContainer::initialize(Device *device, AllocationsList *reusableAllocationList, bool requireHeaps) {
    this->device = device;
    this->reusableAllocationList = reusableAllocationList;

    size_t alignedSize = alignUp<size_t>(totalCmdBufferSize, MemoryConstants::pageSize64k);

    auto cmdBufferAllocation = this->obtainNextCommandBufferAllocation();

    if (!cmdBufferAllocation) {
        return ErrorCode::OUT_OF_DEVICE_MEMORY;
    }

    cmdBufferAllocations.push_back(cmdBufferAllocation);

    const auto &hardwareInfo = device->getHardwareInfo();
    auto &hwHelper = NEO::HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    commandStream = std::make_unique<LinearStream>(cmdBufferAllocation->getUnderlyingBuffer(),
                                                   alignedSize - cmdBufferReservedSize, this, hwHelper.getBatchBufferEndSize());

    commandStream->replaceGraphicsAllocation(cmdBufferAllocation);

    if (!getFlushTaskUsedForImmediate()) {
        addToResidencyContainer(cmdBufferAllocation);
    }
    if (requireHeaps) {
        constexpr size_t heapSize = 65536u;
        heapHelper = std::unique_ptr<HeapHelper>(new HeapHelper(device->getMemoryManager(), device->getDefaultEngine().commandStreamReceiver->getInternalAllocationStorage(), device->getNumGenericSubDevices() > 1u));

        for (uint32_t i = 0; i < IndirectHeap::Type::NUM_TYPES; i++) {
            if (NEO::ApiSpecificConfig::getBindlessConfiguration() && i != IndirectHeap::Type::INDIRECT_OBJECT) {
                continue;
            }
            if (!hardwareInfo.capabilityTable.supportsImages && IndirectHeap::Type::DYNAMIC_STATE == i) {
                continue;
            }
            allocationIndirectHeaps[i] = heapHelper->getHeapAllocation(i,
                                                                       heapSize,
                                                                       alignedSize,
                                                                       device->getRootDeviceIndex());
            if (!allocationIndirectHeaps[i]) {
                return ErrorCode::OUT_OF_DEVICE_MEMORY;
            }
            residencyContainer.push_back(allocationIndirectHeaps[i]);

            bool requireInternalHeap = (IndirectHeap::Type::INDIRECT_OBJECT == i);
            indirectHeaps[i] = std::make_unique<IndirectHeap>(allocationIndirectHeaps[i], requireInternalHeap);
            if (i == IndirectHeap::Type::SURFACE_STATE) {
                indirectHeaps[i]->getSpace(reservedSshSize);
            }
        }

        indirectObjectHeapBaseAddress = device->getMemoryManager()->getInternalHeapBaseAddress(device->getRootDeviceIndex(), allocationIndirectHeaps[IndirectHeap::Type::INDIRECT_OBJECT]->isAllocatedInLocalMemoryPool());

        instructionHeapBaseAddress = device->getMemoryManager()->getInternalHeapBaseAddress(device->getRootDeviceIndex(), device->getMemoryManager()->isLocalMemoryUsedForIsa(device->getRootDeviceIndex()));

        iddBlock = nullptr;
        nextIddInBlock = this->getNumIddPerBlock();
    }

    return ErrorCode::SUCCESS;
}

void CommandContainer::addToResidencyContainer(GraphicsAllocation *alloc) {
    if (alloc == nullptr) {
        return;
    }

    this->residencyContainer.push_back(alloc);
}

void CommandContainer::removeDuplicatesFromResidencyContainer() {
    std::sort(this->residencyContainer.begin(), this->residencyContainer.end());
    this->residencyContainer.erase(std::unique(this->residencyContainer.begin(), this->residencyContainer.end()), this->residencyContainer.end());
}

void CommandContainer::reset() {
    setDirtyStateForAllHeaps(true);
    slmSize = std::numeric_limits<uint32_t>::max();
    getResidencyContainer().clear();
    getDeallocationContainer().clear();
    sshAllocations.clear();

    this->handleCmdBufferAllocations(1u);
    cmdBufferAllocations.erase(cmdBufferAllocations.begin() + 1, cmdBufferAllocations.end());

    commandStream->replaceBuffer(cmdBufferAllocations[0]->getUnderlyingBuffer(),
                                 defaultListCmdBufferSize);
    commandStream->replaceGraphicsAllocation(cmdBufferAllocations[0]);
    addToResidencyContainer(commandStream->getGraphicsAllocation());

    for (auto &indirectHeap : indirectHeaps) {
        if (indirectHeap != nullptr) {
            indirectHeap->replaceBuffer(indirectHeap->getCpuBase(),
                                        indirectHeap->getMaxAvailableSpace());
            addToResidencyContainer(indirectHeap->getGraphicsAllocation());
        }
    }
    if (indirectHeaps[IndirectHeap::Type::SURFACE_STATE] != nullptr) {
        indirectHeaps[IndirectHeap::Type::SURFACE_STATE]->getSpace(reservedSshSize);
    }

    iddBlock = nullptr;
    nextIddInBlock = this->getNumIddPerBlock();
    lastPipelineSelectModeRequired = false;
    lastSentUseGlobalAtomics = false;
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
        auto oldBase = indirectHeap->getHeapGpuBase();
        indirectHeap->replaceGraphicsAllocation(newAlloc);
        indirectHeap->replaceBuffer(newAlloc->getUnderlyingBuffer(),
                                    newAlloc->getUnderlyingBufferSize());
        auto newBase = indirectHeap->getHeapGpuBase();
        getResidencyContainer().push_back(newAlloc);
        getDeallocationContainer().push_back(oldAlloc);
        setIndirectHeapAllocation(heapType, newAlloc);
        if (oldBase != newBase) {
            setHeapDirty(heapType);
        }
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
        auto oldBase = indirectHeap->getHeapGpuBase();
        indirectHeap->replaceGraphicsAllocation(newAlloc);
        indirectHeap->replaceBuffer(newAlloc->getUnderlyingBuffer(),
                                    newAlloc->getUnderlyingBufferSize());
        auto newBase = indirectHeap->getHeapGpuBase();
        getResidencyContainer().push_back(newAlloc);
        getDeallocationContainer().push_back(oldAlloc);
        setIndirectHeapAllocation(heapType, newAlloc);
        if (oldBase != newBase) {
            setHeapDirty(heapType);
        }
        if (heapType == HeapType::SURFACE_STATE) {
            indirectHeap->getSpace(reservedSshSize);
            sshAllocations.push_back(oldAlloc);
        }
    }

    if (alignment) {
        indirectHeap->align(alignment);
    }

    return indirectHeap;
}

void CommandContainer::handleCmdBufferAllocations(size_t startIndex) {
    for (size_t i = startIndex; i < cmdBufferAllocations.size(); i++) {
        if (this->reusableAllocationList) {
            reusableAllocationList->pushFrontOne(*cmdBufferAllocations[i]);
        } else {
            this->device->getMemoryManager()->freeGraphicsMemory(cmdBufferAllocations[i]);
        }
    }
}

GraphicsAllocation *CommandContainer::obtainNextCommandBufferAllocation() {
    size_t alignedSize = alignUp<size_t>(totalCmdBufferSize, MemoryConstants::pageSize64k);

    GraphicsAllocation *cmdBufferAllocation = nullptr;
    if (this->reusableAllocationList) {
        cmdBufferAllocation = this->reusableAllocationList->detachAllocation(alignedSize, nullptr, nullptr, AllocationType::COMMAND_BUFFER).release();
    }
    if (!cmdBufferAllocation) {
        AllocationProperties properties{device->getRootDeviceIndex(),
                                        true /* allocateMemory*/,
                                        alignedSize,
                                        AllocationType::COMMAND_BUFFER,
                                        (device->getNumGenericSubDevices() > 1u) /* multiOsContextCapable */,
                                        false,
                                        device->getDeviceBitfield()};

        cmdBufferAllocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    }

    return cmdBufferAllocation;
}

void CommandContainer::allocateNextCommandBuffer() {
    auto cmdBufferAllocation = this->obtainNextCommandBufferAllocation();
    UNRECOVERABLE_IF(!cmdBufferAllocation);

    cmdBufferAllocations.push_back(cmdBufferAllocation);

    size_t alignedSize = alignUp<size_t>(totalCmdBufferSize, MemoryConstants::pageSize64k);
    commandStream->replaceBuffer(cmdBufferAllocation->getUnderlyingBuffer(), alignedSize - cmdBufferReservedSize);
    commandStream->replaceGraphicsAllocation(cmdBufferAllocation);

    if (!getFlushTaskUsedForImmediate()) {
        addToResidencyContainer(cmdBufferAllocation);
    }
}

void CommandContainer::closeAndAllocateNextCommandBuffer() {
    auto &hwHelper = NEO::HwHelper::get(device->getHardwareInfo().platform.eRenderCoreFamily);
    auto bbEndSize = hwHelper.getBatchBufferEndSize();
    auto ptr = commandStream->getSpace(0u);
    memcpy_s(ptr, bbEndSize, hwHelper.getBatchBufferEndReference(), bbEndSize);
    allocateNextCommandBuffer();
    currentLinearStreamStartOffset = 0u;
}

void CommandContainer::prepareBindfulSsh() {
    if (ApiSpecificConfig::getBindlessConfiguration()) {
        if (allocationIndirectHeaps[IndirectHeap::Type::SURFACE_STATE] == nullptr) {
            size_t alignedSize = alignUp<size_t>(totalCmdBufferSize, MemoryConstants::pageSize64k);
            constexpr size_t heapSize = 65536u;
            allocationIndirectHeaps[IndirectHeap::Type::SURFACE_STATE] = heapHelper->getHeapAllocation(IndirectHeap::Type::SURFACE_STATE,
                                                                                                       heapSize,
                                                                                                       alignedSize,
                                                                                                       device->getRootDeviceIndex());
            UNRECOVERABLE_IF(!allocationIndirectHeaps[IndirectHeap::Type::SURFACE_STATE]);
            residencyContainer.push_back(allocationIndirectHeaps[IndirectHeap::Type::SURFACE_STATE]);

            indirectHeaps[IndirectHeap::Type::SURFACE_STATE] = std::make_unique<IndirectHeap>(allocationIndirectHeaps[IndirectHeap::Type::SURFACE_STATE], false);
            indirectHeaps[IndirectHeap::Type::SURFACE_STATE]->getSpace(reservedSshSize);
        }
        setHeapDirty(IndirectHeap::Type::SURFACE_STATE);
    }
}

IndirectHeap *CommandContainer::getIndirectHeap(HeapType heapType) {
    return indirectHeaps[heapType].get();
}

} // namespace NEO
