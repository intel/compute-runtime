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

    if (DebugManager.flags.RemoveUserFenceInCmdlistResetAndDestroy.get() != -1) {
        isHandleFenceCompletionRequired = !static_cast<bool>(DebugManager.flags.RemoveUserFenceInCmdlistResetAndDestroy.get());
    }
}

CommandContainer::CommandContainer(uint32_t maxNumAggregatedIdds) : CommandContainer() {
    numIddsPerBlock = maxNumAggregatedIdds;
}

CommandContainer::ErrorCode CommandContainer::initialize(Device *device, AllocationsList *reusableAllocationList, bool requireHeaps) {
    this->device = device;
    this->reusableAllocationList = reusableAllocationList;
    size_t alignedSize = alignUp<size_t>(this->getTotalCmdBufferSize(), MemoryConstants::pageSize64k);

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
        size_t heapSize = 65536u;
        if (DebugManager.flags.ForceDefaultHeapSize.get() != -1) {
            heapSize = DebugManager.flags.ForceDefaultHeapSize.get() * MemoryConstants::kiloByte;
        }
        heapHelper = std::unique_ptr<HeapHelper>(new HeapHelper(device->getMemoryManager(), device->getDefaultEngine().commandStreamReceiver->getInternalAllocationStorage(), device->getNumGenericSubDevices() > 1u));

        for (uint32_t i = 0; i < IndirectHeap::Type::NUM_TYPES; i++) {
            if (NEO::ApiSpecificConfig::getBindlessConfiguration() && i != IndirectHeap::Type::INDIRECT_OBJECT) {
                continue;
            }
            if (!hardwareInfo.capabilityTable.supportsImages && IndirectHeap::Type::DYNAMIC_STATE == i) {
                continue;
            }
            if (immediateCmdListSharedHeap(static_cast<HeapType>(i))) {
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

    auto cmdlistCmdBufferSize = defaultListCmdBufferSize;
    if (DebugManager.flags.OverrideCmdListCmdBufferSizeInKb.get() > 0) {
        cmdlistCmdBufferSize = static_cast<size_t>(DebugManager.flags.OverrideCmdListCmdBufferSizeInKb.get()) * MemoryConstants::kiloByte;
    }

    commandStream->replaceBuffer(cmdBufferAllocations[0]->getUnderlyingBuffer(), cmdlistCmdBufferSize);
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

size_t CommandContainer::getTotalCmdBufferSize() {
    auto totalCommandBufferSize = totalCmdBufferSize;
    if (DebugManager.flags.OverrideCmdListCmdBufferSizeInKb.get() > 0) {
        totalCommandBufferSize = static_cast<size_t>(DebugManager.flags.OverrideCmdListCmdBufferSizeInKb.get()) * MemoryConstants::kiloByte;
        totalCommandBufferSize += cmdBufferReservedSize;
    }
    return totalCommandBufferSize;
}

void *CommandContainer::getHeapSpaceAllowGrow(HeapType heapType,
                                              size_t size) {
    return getHeapWithRequiredSizeAndAlignment(heapType, size, 0)->getSpace(size);
}

IndirectHeap *CommandContainer::getHeapWithRequiredSizeAndAlignment(HeapType heapType, size_t sizeRequired, size_t alignment) {
    auto indirectHeap = getIndirectHeap(heapType);
    UNRECOVERABLE_IF(indirectHeap == nullptr);
    auto sizeRequested = sizeRequired;

    auto heapBuffer = indirectHeap->getSpace(0);
    if (alignment && (heapBuffer != alignUp(heapBuffer, alignment))) {
        sizeRequested += alignment;
    }

    if (immediateCmdListSharedHeap(heapType)) {
        UNRECOVERABLE_IF(indirectHeap->getAvailableSpace() < sizeRequested);
    } else {
        if (indirectHeap->getAvailableSpace() < sizeRequested) {
            size_t newSize = indirectHeap->getUsed() + indirectHeap->getAvailableSpace();
            newSize = std::max(newSize, indirectHeap->getAvailableSpace() + sizeRequested);
            newSize = alignUp(newSize, MemoryConstants::pageSize);
            auto oldAlloc = getIndirectHeapAllocation(heapType);
            this->createAndAssignNewHeap(heapType, newSize);
            if (heapType == HeapType::SURFACE_STATE) {
                indirectHeap->getSpace(reservedSshSize);
                sshAllocations.push_back(oldAlloc);
            }
        }
    }

    if (alignment) {
        indirectHeap->align(alignment);
    }

    return indirectHeap;
}

void CommandContainer::createAndAssignNewHeap(HeapType heapType, size_t size) {
    auto indirectHeap = getIndirectHeap(heapType);
    auto oldAlloc = getIndirectHeapAllocation(heapType);
    auto newAlloc = getHeapHelper()->getHeapAllocation(heapType, size, MemoryConstants::pageSize, device->getRootDeviceIndex());
    UNRECOVERABLE_IF(!oldAlloc);
    UNRECOVERABLE_IF(!newAlloc);
    auto oldBase = indirectHeap->getHeapGpuBase();
    indirectHeap->replaceGraphicsAllocation(newAlloc);
    indirectHeap->replaceBuffer(newAlloc->getUnderlyingBuffer(),
                                newAlloc->getUnderlyingBufferSize());
    auto newBase = indirectHeap->getHeapGpuBase();
    getResidencyContainer().push_back(newAlloc);
    if (this->immediateCmdListCsr) {
        this->storeAllocationAndFlushTagUpdate(oldAlloc);
    } else {
        getDeallocationContainer().push_back(oldAlloc);
    }
    setIndirectHeapAllocation(heapType, newAlloc);
    if (oldBase != newBase) {
        setHeapDirty(heapType);
    }
}

void CommandContainer::handleCmdBufferAllocations(size_t startIndex) {
    for (size_t i = startIndex; i < cmdBufferAllocations.size(); i++) {
        if (this->reusableAllocationList) {

            if (isHandleFenceCompletionRequired) {
                this->device->getMemoryManager()->handleFenceCompletion(cmdBufferAllocations[i]);
            }

            reusableAllocationList->pushFrontOne(*cmdBufferAllocations[i]);
        } else {
            this->device->getMemoryManager()->freeGraphicsMemory(cmdBufferAllocations[i]);
        }
    }
}

GraphicsAllocation *CommandContainer::obtainNextCommandBufferAllocation() {

    GraphicsAllocation *cmdBufferAllocation = nullptr;
    if (this->reusableAllocationList) {
        size_t alignedSize = alignUp<size_t>(this->getTotalCmdBufferSize(), MemoryConstants::pageSize64k);
        cmdBufferAllocation = this->reusableAllocationList->detachAllocation(alignedSize, nullptr, nullptr, AllocationType::COMMAND_BUFFER).release();
    }
    if (!cmdBufferAllocation) {
        cmdBufferAllocation = this->allocateCommandBuffer();
    }

    return cmdBufferAllocation;
}

void CommandContainer::allocateNextCommandBuffer() {
    auto cmdBufferAllocation = this->obtainNextCommandBufferAllocation();
    UNRECOVERABLE_IF(!cmdBufferAllocation);

    cmdBufferAllocations.push_back(cmdBufferAllocation);

    setCmdBuffer(cmdBufferAllocation);
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
            constexpr size_t heapSize = MemoryConstants::pageSize64k;
            allocationIndirectHeaps[IndirectHeap::Type::SURFACE_STATE] = heapHelper->getHeapAllocation(IndirectHeap::Type::SURFACE_STATE,
                                                                                                       heapSize,
                                                                                                       MemoryConstants::pageSize64k,
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
    if (immediateCmdListSharedHeap(heapType)) {
        return heapType == HeapType::SURFACE_STATE ? sharedSshCsrHeap : sharedDshCsrHeap;
    } else {
        return indirectHeaps[heapType].get();
    }
}

void CommandContainer::ensureHeapSizePrepared(size_t sshRequiredSize, size_t dshRequiredSize) {
    auto lock = immediateCmdListCsr->obtainUniqueOwnership();
    sharedSshCsrHeap = &immediateCmdListCsr->getIndirectHeap(HeapType::SURFACE_STATE, sshRequiredSize);

    if (dshRequiredSize > 0) {
        sharedDshCsrHeap = &immediateCmdListCsr->getIndirectHeap(HeapType::DYNAMIC_STATE, dshRequiredSize);
    }
}

GraphicsAllocation *CommandContainer::reuseExistingCmdBuffer() {
    size_t alignedSize = alignUp<size_t>(this->getTotalCmdBufferSize(), MemoryConstants::pageSize64k);
    auto cmdBufferAllocation = this->reusableAllocationList->detachAllocation(alignedSize, nullptr, this->immediateCmdListCsr, AllocationType::COMMAND_BUFFER).release();
    if (cmdBufferAllocation) {
        this->cmdBufferAllocations.push_back(cmdBufferAllocation);
    }
    return cmdBufferAllocation;
}

void CommandContainer::addCurrentCommandBufferToReusableAllocationList() {
    this->cmdBufferAllocations.erase(std::find(this->cmdBufferAllocations.begin(), this->cmdBufferAllocations.end(), this->commandStream->getGraphicsAllocation()));
    this->storeAllocationAndFlushTagUpdate(this->commandStream->getGraphicsAllocation());
}

void CommandContainer::setCmdBuffer(GraphicsAllocation *cmdBuffer) {
    size_t alignedSize = alignUp<size_t>(this->getTotalCmdBufferSize(), MemoryConstants::pageSize64k);
    commandStream->replaceBuffer(cmdBuffer->getUnderlyingBuffer(), alignedSize - cmdBufferReservedSize);
    commandStream->replaceGraphicsAllocation(cmdBuffer);

    if (!getFlushTaskUsedForImmediate()) {
        addToResidencyContainer(cmdBuffer);
    }
}

GraphicsAllocation *CommandContainer::allocateCommandBuffer() {
    size_t alignedSize = alignUp<size_t>(this->getTotalCmdBufferSize(), MemoryConstants::pageSize64k);
    AllocationProperties properties{device->getRootDeviceIndex(),
                                    true /* allocateMemory*/,
                                    alignedSize,
                                    AllocationType::COMMAND_BUFFER,
                                    (device->getNumGenericSubDevices() > 1u) /* multiOsContextCapable */,
                                    false,
                                    device->getDeviceBitfield()};

    return device->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
}

void CommandContainer::fillReusableAllocationLists() {
    const auto &hardwareInfo = device->getHardwareInfo();
    auto &hwHelper = NEO::HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    auto amountToFill = hwHelper.getAmountOfAllocationsToFill();
    if (amountToFill == 0u) {
        return;
    }

    for (auto i = 0u; i < amountToFill; i++) {
        auto allocToReuse = this->allocateCommandBuffer();
        this->reusableAllocationList->pushTailOne(*allocToReuse);
        this->getResidencyContainer().push_back(allocToReuse);
    }

    if (!this->heapHelper) {
        return;
    }

    constexpr size_t heapSize = 65536u;
    size_t alignedSize = alignUp<size_t>(this->getTotalCmdBufferSize(), MemoryConstants::pageSize64k);
    for (auto i = 0u; i < amountToFill; i++) {
        for (auto heapType = 0u; heapType < IndirectHeap::Type::NUM_TYPES; heapType++) {
            if (NEO::ApiSpecificConfig::getBindlessConfiguration() && heapType != IndirectHeap::Type::INDIRECT_OBJECT) {
                continue;
            }
            if (!hardwareInfo.capabilityTable.supportsImages && IndirectHeap::Type::DYNAMIC_STATE == heapType) {
                continue;
            }
            if (immediateCmdListSharedHeap(static_cast<HeapType>(heapType))) {
                continue;
            }
            auto heapToReuse = heapHelper->getHeapAllocation(heapType,
                                                             heapSize,
                                                             alignedSize,
                                                             device->getRootDeviceIndex());
            this->immediateCmdListCsr->makeResident(*heapToReuse);
            this->heapHelper->storeHeapAllocation(heapToReuse);
        }
    }
}

void CommandContainer::storeAllocationAndFlushTagUpdate(GraphicsAllocation *allocation) {
    auto lock = this->immediateCmdListCsr->obtainUniqueOwnership();
    auto taskCount = this->immediateCmdListCsr->peekTaskCount() + 1;
    auto osContextId = this->immediateCmdListCsr->getOsContext().getContextId();
    allocation->updateTaskCount(taskCount, osContextId);
    allocation->updateResidencyTaskCount(taskCount, osContextId);
    if (allocation->getAllocationType() == AllocationType::COMMAND_BUFFER) {
        this->reusableAllocationList->pushTailOne(*allocation);
    } else {
        getHeapHelper()->storeHeapAllocation(allocation);
    }
    this->immediateCmdListCsr->flushTagUpdate();
}

} // namespace NEO
