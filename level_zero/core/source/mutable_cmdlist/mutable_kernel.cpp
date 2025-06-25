/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/mutable_cmdlist/mutable_kernel.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/string.h"

#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_dispatch.h"
#include <level_zero/ze_api.h>

namespace L0::MCL {

MutableKernel::MutableKernel(ze_kernel_handle_t kernelHandle, uint32_t inlineDataSize, uint32_t maxPerThreadDataSize)
    : inlineDataSize(inlineDataSize),
      maxPerThreadDataSize(maxPerThreadDataSize) {
    this->kernel = L0::Kernel::fromHandle(kernelHandle);
}

uint32_t MutableKernel::getKernelScratchSize(uint32_t slotId) const {
    return kernel->getKernelDescriptor().kernelAttributes.perThreadScratchSize[slotId];
}

void MutableKernel::allocateHostViewIndirectHeap() {
    hostViewIndirectHeap.reset(new uint8_t[kernel->getIndirectSize() + this->maxPerThreadDataSize]);
}

void MutableKernel::createHostViewIndirectData(bool copyInlineData) {
    if (hostViewIndirectHeap.get() == nullptr) {
        allocateHostViewIndirectHeap();
    }

    auto &kernelDescriptor = kernel->getKernelDescriptor();

    auto &dispatchTraits = kernelDescriptor.payloadMappings.dispatchTraits;
    auto offsets = std::make_unique<MutableIndirectData::Offsets>();
    offsets->globalWorkSize = dispatchTraits.globalWorkSize[0];
    offsets->localWorkSize = dispatchTraits.localWorkSize[0];
    offsets->localWorkSize2 = dispatchTraits.localWorkSize2[0];
    offsets->enqLocalWorkSize = dispatchTraits.enqueuedLocalWorkSize[0];
    offsets->numWorkGroups = dispatchTraits.numWorkGroups[0];
    offsets->workDimensions = dispatchTraits.workDim;
    offsets->globalWorkOffset = dispatchTraits.globalWorkOffset[0];

    ArrayRef<uint8_t> inlineData;

    size_t crossThreadDataSize = kernel->getCrossThreadDataSize();
    if (kernelDescriptor.kernelAttributes.flags.passInlineData) {
        crossThreadDataSize -= std::min(crossThreadDataSize, static_cast<size_t>(this->inlineDataSize));
        inlineData = {reinterpret_cast<uint8_t *>(this->computeWalker->getHostMemoryInlineDataPointer()), this->inlineDataSize};
    }
    ArrayRef<uint8_t> crossThreadData = {hostViewIndirectHeap.get(), crossThreadDataSize};

    ArrayRef<uint8_t> perThreadData;
    if (this->kernelDispatch->offsets.perThreadOffset != undefined<IndirectObjectHeapOffset>) {
        size_t perThreadDataSize = kernel->getIndirectSize() - crossThreadDataSize;
        perThreadData = {reinterpret_cast<uint8_t *>(ptrOffset(hostViewIndirectHeap.get(), crossThreadDataSize)), (perThreadDataSize + this->maxPerThreadDataSize)};
    }

    hostViewIndirectData = std::make_unique<MutableIndirectData>(std::move(offsets), crossThreadData, perThreadData, inlineData);

    auto srcCrossThreadData = kernel->getCrossThreadData();
    if (kernelDescriptor.kernelAttributes.flags.passInlineData) {
        if (copyInlineData) {
            memcpy_s(inlineData.begin(), this->inlineDataSize, srcCrossThreadData, this->inlineDataSize);
        }
        srcCrossThreadData = ptrOffset(srcCrossThreadData, this->inlineDataSize);
    }

    memcpy_s(crossThreadData.begin(), crossThreadDataSize, srcCrossThreadData, crossThreadDataSize);

    if (this->kernelDispatch->offsets.perThreadOffset != undefined<IndirectObjectHeapOffset>) {
        auto srcPerThreadData = kernel->getPerThreadData();
        memcpy_s(perThreadData.begin(), kernel->getPerThreadDataSizeForWholeThreadGroup(), srcPerThreadData, kernel->getPerThreadDataSizeForWholeThreadGroup());
    }
}

void MutableKernel::copyTemplateViewIndirectData() {
    hostViewIndirectData->copyIndirectData(kernelDispatch->varDispatch->getIndirectData());
}

void MutableKernel::makeKernelResidencySnapshotContainer(bool saveSyncAndRegionAllocsFromInternalResidency) {
    kernelResidencySnapshotContainer.clear();
    kernelResidencySnapshotContainer.reserve(kernel->getInternalResidencyContainer().size() + 1);

    if (saveSyncAndRegionAllocsFromInternalResidency) {
        // when selected kernel is appended to launch it, its allocations are actual part of residency, it was encoded to these allocations
        kernelResidencySnapshotContainer.insert(kernelResidencySnapshotContainer.end(), kernel->getInternalResidencyContainer().begin(), kernel->getInternalResidencyContainer().end());
    } else {
        // when kernel is mutated from the group, its allocations cannot be reused (were assigned to different append calls), so we need to copy them from kernel dispatch data, as these are associated with current append
        uint32_t idx = 0;
        auto kernelImp = static_cast<L0::KernelImp *>(this->kernel);
        for (const auto &allocation : kernel->getInternalResidencyContainer()) {
            if (idx != kernelImp->getSyncBufferIndex() && idx != kernelImp->getRegionGroupBarrierIndex()) {
                kernelResidencySnapshotContainer.emplace_back(allocation);
            }
            idx++;
        }
        if (this->kernelDispatch->syncBuffer != nullptr) {
            kernelResidencySnapshotContainer.emplace_back(this->kernelDispatch->syncBuffer);
            this->syncBufferSnapshotResidencyIndex = idx;
            idx++;
        }
        if (this->kernelDispatch->regionBarrier != nullptr) {
            kernelResidencySnapshotContainer.emplace_back(this->kernelDispatch->regionBarrier);
            this->regionBarrierSnapshotResidencyIndex = idx;
            idx++;
        }
    }

    // add isa allocation as last to save the indices of other allocations as the same during initial making of snapshot at saveResidencyAllocationIndices call
    kernelResidencySnapshotContainer.emplace_back(kernel->getImmutableData()->getIsaGraphicsAllocation());
}

void MutableKernel::updateResidencySnapshotContainer() {
    // check if mutation of group count/size or local region size not caused change of allocation
    if (this->kernelDispatch->syncBuffer != nullptr) {
        if (this->syncBufferSnapshotResidencyIndex != std::numeric_limits<size_t>::max()) {
            // allocation was already defined in snapshot, just update what is currently saved in snapshot
            kernelResidencySnapshotContainer[this->syncBufferSnapshotResidencyIndex] = this->kernelDispatch->syncBuffer;
        } else {
            // allocation was added during mutation - when not used kernel was selected and it did not had allocation assigned, then add it to snapshot
            this->syncBufferSnapshotResidencyIndex = kernelResidencySnapshotContainer.size();
            kernelResidencySnapshotContainer.emplace_back(this->kernelDispatch->syncBuffer);
        }
    }

    if (this->kernelDispatch->regionBarrier != nullptr) {
        if (this->regionBarrierSnapshotResidencyIndex != std::numeric_limits<size_t>::max()) {
            kernelResidencySnapshotContainer[this->regionBarrierSnapshotResidencyIndex] = this->kernelDispatch->regionBarrier;
        } else {
            this->regionBarrierSnapshotResidencyIndex = kernelResidencySnapshotContainer.size();
            kernelResidencySnapshotContainer.emplace_back(this->kernelDispatch->regionBarrier);
        }
    }
}

bool MutableKernel::checkKernelCompatible() {
    L0::KernelImp *kernelImp = static_cast<KernelImp *>(kernel);
    const auto &kernelAttributes = kernel->getKernelDescriptor().kernelAttributes;

    const bool requiresImplicitArgs = kernelAttributes.flags.requiresImplicitArgs;
    const bool usesPrintf = kernelAttributes.flags.usesPrintf;
    const bool useStackCalls = kernelAttributes.flags.useStackCalls;
    const bool privateMemoryPerDispatch = kernelImp->getParentModule().shouldAllocatePrivateMemoryPerDispatch();

    const bool usesUnsupportedFeature = requiresImplicitArgs ||
                                        usesPrintf ||
                                        useStackCalls ||
                                        privateMemoryPerDispatch;
    if (usesUnsupportedFeature) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL MutableKernel compatible property requiresImplicitArgs: %d, usesPrintf: %d, useStackCalls: %d, privateMemoryPerDispatch: %d\n",
                           requiresImplicitArgs, usesPrintf, useStackCalls, privateMemoryPerDispatch);
        return false;
    }
    return true;
}

} // namespace L0::MCL
