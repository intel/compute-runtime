/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/mutable_cmdlist/mutable_kernel.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/ptr_math.h"

#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_dispatch.h"
#include <level_zero/ze_api.h>

#include <iterator>

namespace L0::MCL {

MutableKernel::MutableKernel(ze_kernel_handle_t kernelHandle, uint32_t inlineDataSize, uint32_t maxPerThreadDataSize)
    : inlineDataSize(inlineDataSize),
      maxPerThreadDataSize(maxPerThreadDataSize) {
    this->kernel = L0::Kernel::fromHandle(kernelHandle);
    this->kernelVariables.kernelArguments.reserve(this->kernel->getKernelDescriptor().payloadMappings.explicitArgs.size());
    // space for internal allocations like ISA, private, const, global buffers, etc.
    constexpr size_t estimatedInternalResidencyCount = 10;
    this->kernelResidencySnapshotContainer.reserve(estimatedInternalResidencyCount);
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
    std::copy(std::begin(dispatchTraits.globalWorkSize), std::end(dispatchTraits.globalWorkSize), std::begin(offsets->globalWorkSize));
    std::copy(std::begin(dispatchTraits.localWorkSize), std::end(dispatchTraits.localWorkSize), std::begin(offsets->localWorkSize));
    std::copy(std::begin(dispatchTraits.localWorkSize2), std::end(dispatchTraits.localWorkSize2), std::begin(offsets->localWorkSize2));
    std::copy(std::begin(dispatchTraits.enqueuedLocalWorkSize), std::end(dispatchTraits.enqueuedLocalWorkSize), std::begin(offsets->enqLocalWorkSize));
    std::copy(std::begin(dispatchTraits.numWorkGroups), std::end(dispatchTraits.numWorkGroups), std::begin(offsets->numWorkGroups));
    offsets->workDimensions = dispatchTraits.workDim;
    std::copy(std::begin(dispatchTraits.globalWorkOffset), std::end(dispatchTraits.globalWorkOffset), std::begin(offsets->globalWorkOffset));

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
            if (idx != kernelImp->getSyncBufferIndex()) {
                kernelResidencySnapshotContainer.emplace_back(allocation);
            }
            idx++;
        }
        // sync buffer and region barrier allcocations are taken from current use, not from kernel internal residency
        if (this->kernelDispatch->syncBuffer != nullptr) {
            this->syncBufferSnapshotResidencyIndex = kernelResidencySnapshotContainer.size();
            kernelResidencySnapshotContainer.emplace_back(this->kernelDispatch->syncBuffer);
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
        PRINT_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL MutableKernel compatible property requiresImplicitArgs: %d, usesPrintf: %d, useStackCalls: %d, privateMemoryPerDispatch: %d\n",
                     requiresImplicitArgs, usesPrintf, useStackCalls, privateMemoryPerDispatch);
        return false;
    }
    return true;
}

} // namespace L0::MCL
