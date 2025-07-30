/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/heap_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
namespace NEO {

CommandContainer::~CommandContainer() {
    if (!device) {
        DEBUG_BREAK_IF(device);
        return;
    }

    this->handleCmdBufferAllocations(0u);

    if (heapHelper) {
        for (auto allocationIndirectHeap : allocationIndirectHeaps) {
            heapHelper->storeHeapAllocation(allocationIndirectHeap);
        }
        for (auto deallocation : deallocationContainer) {
            if ((deallocation->getAllocationType() == AllocationType::internalHeap) || (deallocation->getAllocationType() == AllocationType::linearStream)) {
                getHeapHelper()->storeHeapAllocation(deallocation);
            }
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

    if (debugManager.flags.RemoveUserFenceInCmdlistResetAndDestroy.get() != -1) {
        isHandleFenceCompletionRequired = !static_cast<bool>(debugManager.flags.RemoveUserFenceInCmdlistResetAndDestroy.get());
    }
}

CommandContainer::CommandContainer(uint32_t maxNumAggregatedIdds) : CommandContainer() {
    numIddsPerBlock = maxNumAggregatedIdds;
}

CommandContainer::ErrorCode CommandContainer::initialize(Device *device, AllocationsList *reusableAllocationList, size_t defaultSshSize, bool requireHeaps, bool createSecondaryCmdBufferInHostMem) {
    this->device = device;
    this->reusableAllocationList = reusableAllocationList;
    size_t usableSize = getMaxUsableSpace();
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto &productHelper = device->getProductHelper();
    this->defaultSshSize = gfxCoreHelper.getDefaultSshSize(productHelper);
    if (this->stateBaseAddressTracking) {
        this->defaultSshSize = defaultSshSize;
    }

    globalBindlessHeapsEnabled = this->device->getExecutionEnvironment()->rootDeviceEnvironments[this->device->getRootDeviceIndex()]->getBindlessHeapsHelper() != nullptr;

    auto cmdBufferAllocation = this->obtainNextCommandBufferAllocation();

    if (!cmdBufferAllocation) {
        return ErrorCode::outOfDeviceMemory;
    }

    cmdBufferAllocations.push_back(cmdBufferAllocation);

    if (this->usingPrimaryBuffer) {
        this->selectedBbCmdSize = gfxCoreHelper.getBatchBufferStartSize();
    } else {
        this->selectedBbCmdSize = gfxCoreHelper.getBatchBufferEndSize();
        this->bbEndReference = gfxCoreHelper.getBatchBufferEndReference();
    }

    CommandContainer *cmdcontainer = this;
    if (this->immediateCmdListCsr) {
        cmdcontainer = nullptr;
    }
    commandStream = std::make_unique<LinearStream>(cmdBufferAllocation->getUnderlyingBuffer(),
                                                   usableSize, cmdcontainer, this->selectedBbCmdSize);

    commandStream->replaceGraphicsAllocation(cmdBufferAllocation);

    if (createSecondaryCmdBufferInHostMem) {
        this->useSecondaryCommandStream = true;

        auto cmdBufferAllocationHost = this->obtainNextCommandBufferAllocation(true);
        if (!cmdBufferAllocationHost) {
            return ErrorCode::outOfDeviceMemory;
        }
        secondaryCommandStreamForImmediateCmdList = std::make_unique<LinearStream>(cmdBufferAllocationHost->getUnderlyingBuffer(),
                                                                                   usableSize, cmdcontainer, this->selectedBbCmdSize);
        secondaryCommandStreamForImmediateCmdList->replaceGraphicsAllocation(cmdBufferAllocationHost);
        cmdBufferAllocations.push_back(cmdBufferAllocationHost);
        addToResidencyContainer(cmdBufferAllocationHost);
    }

    addToResidencyContainer(cmdBufferAllocation);
    if (requireHeaps) {
        heapHelper = std::unique_ptr<HeapHelper>(new HeapHelper(device->getMemoryManager(), device->getDefaultEngine().commandStreamReceiver->getInternalAllocationStorage(), device->getNumGenericSubDevices() > 1u));

        for (uint32_t i = 0; i < IndirectHeap::Type::numTypes; i++) {
            auto heapType = static_cast<HeapType>(i);
            if (skipHeapAllocationCreation(heapType)) {
                continue;
            }

            size_t heapSize = getHeapSize(heapType);

            allocationIndirectHeaps[i] = heapHelper->getHeapAllocation(i,
                                                                       heapSize,
                                                                       defaultHeapAllocationAlignment,
                                                                       device->getRootDeviceIndex());

            if (!allocationIndirectHeaps[i]) {
                return ErrorCode::outOfDeviceMemory;
            }
            residencyContainer.push_back(allocationIndirectHeaps[i]);

            bool requireInternalHeap = false;
            if (IndirectHeap::Type::indirectObject == heapType) {
                requireInternalHeap = true;
                indirectHeapInLocalMemory = allocationIndirectHeaps[i]->isAllocatedInLocalMemoryPool();
            }
            indirectHeaps[i] = std::make_unique<IndirectHeap>(allocationIndirectHeaps[i], requireInternalHeap);
            if (IndirectHeap::Type::surfaceState == heapType) {
                indirectHeaps[i]->getSpace(reservedSshSize);
            }
        }

        indirectObjectHeapBaseAddress = device->getMemoryManager()->getInternalHeapBaseAddress(device->getRootDeviceIndex(), indirectHeapInLocalMemory);

        instructionHeapBaseAddress = device->getMemoryManager()->getInternalHeapBaseAddress(device->getRootDeviceIndex(), device->getMemoryManager()->isLocalMemoryUsedForIsa(device->getRootDeviceIndex()));

        iddBlock = nullptr;
        nextIddInBlock = this->getNumIddPerBlock();
    }
    return ErrorCode::success;
}

void CommandContainer::addToResidencyContainer(GraphicsAllocation *alloc) {
    if (alloc == nullptr) {
        return;
    }

    this->residencyContainer.push_back(alloc);
}

bool CommandContainer::swapStreams() {
    if (this->useSecondaryCommandStream) {
        this->commandStream.swap(this->secondaryCommandStreamForImmediateCmdList);
        return true;
    }
    return false;
}

void CommandContainer::removeDuplicatesFromResidencyContainer() {
    std::sort(this->residencyContainer.begin(), this->residencyContainer.end());
    this->residencyContainer.erase(std::unique(this->residencyContainer.begin(), this->residencyContainer.end()), this->residencyContainer.end());
}

void CommandContainer::reset() {
    setDirtyStateForAllHeaps(true);
    slmSize = std::numeric_limits<uint32_t>::max();
    getResidencyContainer().clear();
    if (getHeapHelper()) {
        for (auto deallocation : deallocationContainer) {
            if ((deallocation->getAllocationType() == AllocationType::internalHeap) || (deallocation->getAllocationType() == AllocationType::linearStream)) {
                getHeapHelper()->storeHeapAllocation(deallocation);
            }
        }
    }
    getDeallocationContainer().clear();
    sshAllocations.clear();

    auto defaultCmdBuffersCnt = 1u + this->useSecondaryCommandStream;

    this->handleCmdBufferAllocations(defaultCmdBuffersCnt);
    cmdBufferAllocations.erase(cmdBufferAllocations.begin() + defaultCmdBuffersCnt, cmdBufferAllocations.end());

    if (this->useSecondaryCommandStream) {
        if (!NEO::MemoryPoolHelper::isSystemMemoryPool(this->getCommandStream()->getGraphicsAllocation()->getMemoryPool())) {
            this->swapStreams();
        }
        setCmdBuffer(cmdBufferAllocations[1]);
        this->swapStreams();
    }

    setCmdBuffer(cmdBufferAllocations[0]);

    for (uint32_t i = 0; i < IndirectHeap::Type::numTypes; i++) {
        if (indirectHeaps[i] != nullptr) {
            if (i == IndirectHeap::Type::indirectObject || !this->stateBaseAddressTracking) {
                indirectHeaps[i]->replaceBuffer(indirectHeaps[i]->getCpuBase(),
                                                indirectHeaps[i]->getMaxAvailableSpace());
                if (i == IndirectHeap::Type::surfaceState) {
                    indirectHeaps[i]->getSpace(reservedSshSize);
                }
            }
            addToResidencyContainer(indirectHeaps[i]->getGraphicsAllocation());
        }
    }

    iddBlock = nullptr;
    nextIddInBlock = this->getNumIddPerBlock();
    lastPipelineSelectModeRequired = false;
    endCmdPtr = nullptr;
    endCmdGpuAddress = 0;
    alignedPrimarySize = 0;
}

size_t CommandContainer::getAlignedCmdBufferSize() const {
    auto totalCommandBufferSize = totalCmdBufferSize;
    if (debugManager.flags.OverrideCmdListCmdBufferSizeInKb.get() > 0) {
        totalCommandBufferSize = static_cast<size_t>(debugManager.flags.OverrideCmdListCmdBufferSizeInKb.get()) * MemoryConstants::kiloByte;
        totalCommandBufferSize += cmdBufferReservedSize;
    }
    return alignUp<size_t>(totalCommandBufferSize, defaultCmdBufferAllocationAlignment);
}

void *CommandContainer::getHeapSpaceAllowGrow(HeapType heapType,
                                              size_t size) {
    return getHeapWithRequiredSize(heapType, size, 0, true)->getSpace(size);
}

IndirectHeap *CommandContainer::getHeapWithRequiredSizeAndAlignment(HeapType heapType, size_t sizeRequired, size_t alignment) {
    return getHeapWithRequiredSize(heapType, sizeRequired, alignment, false);
}

IndirectHeap *CommandContainer::getHeapWithRequiredSize(HeapType heapType, size_t sizeRequired, size_t alignment, bool allowGrow) {
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
            size_t newSize = indirectHeap->getMaxAvailableSpace();
            if (allowGrow) {
                newSize = std::max(newSize, indirectHeap->getAvailableSpace() + sizeRequested);
            }
            newSize = alignUp(newSize, MemoryConstants::pageSize);
            auto oldAlloc = getIndirectHeapAllocation(heapType);
            this->createAndAssignNewHeap(heapType, newSize);
            if (heapType == HeapType::surfaceState) {
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
    auto newAlloc = getHeapHelper()->getHeapAllocation(heapType, size, defaultHeapAllocationAlignment, device->getRootDeviceIndex());
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
    if (immediateReusableAllocationList != nullptr && !immediateReusableAllocationList->peekIsEmpty() && reusableAllocationList != nullptr) {
        reusableAllocationList->splice(*immediateReusableAllocationList->detachNodes());
    }
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
    return this->obtainNextCommandBufferAllocation(false);
}

GraphicsAllocation *CommandContainer::obtainNextCommandBufferAllocation(bool forceHostMemory) {
    forceHostMemory &= this->useSecondaryCommandStream;
    GraphicsAllocation *cmdBufferAllocation = nullptr;
    if (this->reusableAllocationList) {
        const size_t alignedSize = getAlignedCmdBufferSize();
        cmdBufferAllocation = this->reusableAllocationList->detachAllocation(alignedSize, nullptr, forceHostMemory, nullptr, AllocationType::commandBuffer).release();
    }
    if (!cmdBufferAllocation) {
        cmdBufferAllocation = this->allocateCommandBuffer(forceHostMemory);
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
    auto ptr = commandStream->getSpace(0u);
    size_t usedSize = commandStream->getUsed();
    allocateNextCommandBuffer();
    if (this->usingPrimaryBuffer) {
        auto nextChainedBuffer = commandStream->getGraphicsAllocation();
        auto &gfxCoreHelper = device->getGfxCoreHelper();
        gfxCoreHelper.encodeBatchBufferStart(ptr, nextChainedBuffer->getGpuAddress(), false, false, false);
        alignPrimaryEnding(ptr, usedSize);
    } else {
        memcpy_s(ptr, this->selectedBbCmdSize, this->bbEndReference, this->selectedBbCmdSize);
    }
    currentLinearStreamStartOffset = 0u;
}

void CommandContainer::alignPrimaryEnding(void *endPtr, size_t exactUsedSize) {
    exactUsedSize += this->selectedBbCmdSize;
    size_t alignedBufferSize = alignUp(exactUsedSize, minCmdBufferPtrAlign);
    if (alignedBufferSize > exactUsedSize) {
        endPtr = ptrOffset(endPtr, this->selectedBbCmdSize);
        memset(endPtr, 0, alignedBufferSize - exactUsedSize);
    }

    if (this->alignedPrimarySize == 0) {
        this->alignedPrimarySize = alignedBufferSize;
    }
}

void CommandContainer::endAlignedPrimaryBuffer() {
    this->endCmdPtr = commandStream->getSpace(0u);
    this->endCmdGpuAddress = commandStream->getCurrentGpuAddressPosition();
    alignPrimaryEnding(this->endCmdPtr, commandStream->getUsed());
}

void CommandContainer::prepareBindfulSsh() {
    bool globalBindlessSsh = this->device->getBindlessHeapsHelper() != nullptr;
    if (globalBindlessSsh) {
        if (allocationIndirectHeaps[IndirectHeap::Type::surfaceState] == nullptr) {
            constexpr size_t heapSize = MemoryConstants::pageSize64k;
            allocationIndirectHeaps[IndirectHeap::Type::surfaceState] = heapHelper->getHeapAllocation(IndirectHeap::Type::surfaceState,
                                                                                                      heapSize,
                                                                                                      defaultHeapAllocationAlignment,
                                                                                                      device->getRootDeviceIndex());
            UNRECOVERABLE_IF(!allocationIndirectHeaps[IndirectHeap::Type::surfaceState]);
            residencyContainer.push_back(allocationIndirectHeaps[IndirectHeap::Type::surfaceState]);

            indirectHeaps[IndirectHeap::Type::surfaceState] = std::make_unique<IndirectHeap>(allocationIndirectHeaps[IndirectHeap::Type::surfaceState], false);
            indirectHeaps[IndirectHeap::Type::surfaceState]->getSpace(reservedSshSize);
            setHeapDirty(IndirectHeap::Type::surfaceState);
        }
    }
}

IndirectHeap *CommandContainer::getIndirectHeap(HeapType heapType) {
    if (immediateCmdListSharedHeap(heapType)) {
        return heapType == HeapType::surfaceState ? sharedSshCsrHeap : sharedDshCsrHeap;
    } else {
        return indirectHeaps[heapType].get();
    }
}

IndirectHeap *CommandContainer::initIndirectHeapReservation(ReservedIndirectHeap *indirectHeapReservation, size_t size, size_t alignment, HeapType heapType) {
    void *currentHeap = immediateCmdListCsr->getIndirectHeapCurrentPtr(heapType);
    auto totalRequiredSize = size + ptrDiff(alignUp(currentHeap, alignment), currentHeap);

    auto baseHeap = &immediateCmdListCsr->getIndirectHeap(heapType, totalRequiredSize);

    auto usedSize = baseHeap->getUsed();
    void *heapCpuBase = baseHeap->getCpuBase();
    auto consumedSize = usedSize + totalRequiredSize;

    baseHeap->getSpace(totalRequiredSize);

    indirectHeapReservation->replaceGraphicsAllocation(baseHeap->getGraphicsAllocation());
    indirectHeapReservation->replaceBuffer(heapCpuBase, consumedSize);
    indirectHeapReservation->getSpace(usedSize);
    indirectHeapReservation->setHeapSizeInPages(baseHeap->getHeapSizeInPages());

    return baseHeap;
}

void CommandContainer::reserveSpaceForDispatch(HeapReserveArguments &sshReserveArg, HeapReserveArguments &dshReserveArg, bool getDsh) {
    size_t sshAlignment = sshReserveArg.alignment;
    size_t dshAlignment = dshReserveArg.alignment;
    if (sshReserveArg.size == 0) {
        sshAlignment = 1;
    }
    if (dshReserveArg.size == 0) {
        dshAlignment = 1;
    }
    if (immediateCmdListCsr) {
        auto lock = immediateCmdListCsr->obtainUniqueOwnership();
        sharedSshCsrHeap = this->initIndirectHeapReservation(sshReserveArg.indirectHeapReservation, sshReserveArg.size, sshAlignment, HeapType::surfaceState);

        if (getDsh) {
            sharedDshCsrHeap = this->initIndirectHeapReservation(dshReserveArg.indirectHeapReservation, dshReserveArg.size, dshAlignment, HeapType::dynamicState);
        }
    } else {
        if (sshReserveArg.size > 0) {
            prepareBindfulSsh();
            this->getHeapWithRequiredSizeAndAlignment(HeapType::surfaceState, sshReserveArg.size, sshAlignment);
        }

        if (getDsh) {
            this->getHeapWithRequiredSizeAndAlignment(HeapType::dynamicState, dshReserveArg.size, dshAlignment);
        }
        // private heaps can be accessed directly
        sshReserveArg.indirectHeapReservation = nullptr;
        dshReserveArg.indirectHeapReservation = nullptr;
    }
}

GraphicsAllocation *CommandContainer::reuseExistingCmdBuffer() {
    return this->reuseExistingCmdBuffer(false);
}

GraphicsAllocation *CommandContainer::reuseExistingCmdBuffer(bool forceHostMemory) {
    forceHostMemory &= this->useSecondaryCommandStream;
    size_t alignedSize = getAlignedCmdBufferSize();
    auto cmdBufferAllocation = this->immediateReusableAllocationList->detachAllocation(alignedSize, nullptr, forceHostMemory, this->immediateCmdListCsr, AllocationType::commandBuffer).release();
    if (!cmdBufferAllocation) {
        this->reusableAllocationList->detachAllocation(alignedSize, nullptr, forceHostMemory, this->immediateCmdListCsr, AllocationType::commandBuffer).release();
    }

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
    commandStream->replaceBuffer(cmdBuffer->getUnderlyingBuffer(), getMaxUsableSpace());
    commandStream->replaceGraphicsAllocation(cmdBuffer);

    if (!getFlushTaskUsedForImmediate()) {
        addToResidencyContainer(cmdBuffer);
    }
}

GraphicsAllocation *CommandContainer::allocateCommandBuffer(bool forceHostMemory) {
    size_t alignedSize = getAlignedCmdBufferSize();
    AllocationProperties properties{device->getRootDeviceIndex(),
                                    true /* allocateMemory*/,
                                    alignedSize,
                                    AllocationType::commandBuffer,
                                    (device->getNumGenericSubDevices() > 1u) /* multiOsContextCapable */,
                                    false,
                                    device->getDeviceBitfield()};
    properties.flags.forceSystemMemory = forceHostMemory && this->useSecondaryCommandStream;

    auto commandBufferAllocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    if (commandBufferAllocation) {
        commandBufferAllocation->storageInfo.systemMemoryForced = properties.flags.forceSystemMemory;
    }

    return commandBufferAllocation;
}

void CommandContainer::fillReusableAllocationLists() {
    if (this->immediateReusableAllocationList) {
        return;
    }

    this->immediateReusableAllocationList = std::make_unique<NEO::AllocationsList>();

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto amountToFill = gfxCoreHelper.getAmountOfAllocationsToFill();
    if (amountToFill == 0u) {
        return;
    }

    for (auto i = 0u; i < amountToFill; i++) {
        auto allocToReuse = obtainNextCommandBufferAllocation();
        this->immediateReusableAllocationList->pushTailOne(*allocToReuse);
        this->getResidencyContainer().push_back(allocToReuse);

        if (this->useSecondaryCommandStream) {
            auto hostAllocToReuse = obtainNextCommandBufferAllocation(true);
            this->immediateReusableAllocationList->pushTailOne(*hostAllocToReuse);
            this->getResidencyContainer().push_back(hostAllocToReuse);
        }
    }

    if (!this->heapHelper) {
        return;
    }

    for (auto i = 0u; i < amountToFill; i++) {
        for (auto heapType = 0u; heapType < IndirectHeap::Type::numTypes; heapType++) {
            if (skipHeapAllocationCreation(static_cast<HeapType>(heapType))) {
                continue;
            }
            size_t heapSize = getHeapSize(static_cast<HeapType>(heapType));
            auto heapToReuse = heapHelper->getHeapAllocation(heapType,
                                                             heapSize,
                                                             defaultHeapAllocationAlignment,
                                                             device->getRootDeviceIndex());
            if (heapToReuse != nullptr) {
                this->getResidencyContainer().push_back(heapToReuse);
            }
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
    if (allocation->getAllocationType() == AllocationType::commandBuffer) {
        this->immediateReusableAllocationList->pushTailOne(*allocation);
    } else {
        getHeapHelper()->storeHeapAllocation(allocation);
    }
    this->immediateCmdListCsr->flushTagUpdate();
}

HeapReserveData::HeapReserveData() {
    object = std::make_unique<NEO::ReservedIndirectHeap>(nullptr, false);
    indirectHeapReservation = object.get();
}

HeapReserveData::~HeapReserveData() {
}

bool CommandContainer::skipHeapAllocationCreation(HeapType heapType) {
    if (heapType == IndirectHeap::Type::indirectObject) {
        return false;
    }
    const auto &hardwareInfo = this->device->getHardwareInfo();

    bool skipCreation = (globalBindlessHeapsEnabled && IndirectHeap::Type::surfaceState == heapType) ||
                        this->immediateCmdListSharedHeap(heapType) ||
                        (!hardwareInfo.capabilityTable.supportsImages && IndirectHeap::Type::dynamicState == heapType) ||
                        (this->heapAddressModel != HeapAddressModel::privateHeaps);
    return skipCreation;
}

size_t CommandContainer::getHeapSize(HeapType heapType) {
    size_t defaultHeapSize = HeapSize::defaultHeapSize;
    if (HeapType::surfaceState == heapType) {
        defaultHeapSize = this->defaultSshSize;
    }
    return HeapSize::getDefaultHeapSize(defaultHeapSize);
}

void *CommandContainer::findCpuBaseForCmdBufferAddress(void *cmdBufferAddress) {
    uintptr_t cmdBufferAddressValue = reinterpret_cast<uintptr_t>(cmdBufferAddress);
    uintptr_t cpuBaseValue = reinterpret_cast<uintptr_t>(commandStream->getCpuBase());
    if ((cpuBaseValue <= cmdBufferAddressValue) &&
        ((cpuBaseValue + commandStream->getMaxAvailableSpace()) > cmdBufferAddressValue)) {
        return reinterpret_cast<void *>(cpuBaseValue);
    }
    // last cmd buffer allocation is assisgned to commandStream, no need to check it
    for (size_t i = 0; i < cmdBufferAllocations.size() - 1; i++) {
        cpuBaseValue = reinterpret_cast<uintptr_t>(cmdBufferAllocations[i]->getUnderlyingBuffer());
        if ((cpuBaseValue <= cmdBufferAddressValue) &&
            ((cpuBaseValue + getMaxUsableSpace()) > cmdBufferAddressValue)) {
            return reinterpret_cast<void *>(cpuBaseValue);
        }
    }
    return nullptr;
}

} // namespace NEO
