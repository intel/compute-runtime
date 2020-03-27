/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/direct_submission/direct_submission_hw_diagnostic_mode.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
struct MockDirectSubmissionHw : public DirectSubmissionHw<GfxFamily, Dispatcher> {
    using BaseClass = DirectSubmissionHw<GfxFamily, Dispatcher>;
    using BaseClass::allocateResources;
    using BaseClass::completionRingBuffers;
    using BaseClass::cpuCachelineFlush;
    using BaseClass::currentQueueWorkCount;
    using BaseClass::currentRingBuffer;
    using BaseClass::deallocateResources;
    using BaseClass::defaultDisableCacheFlush;
    using BaseClass::defaultDisableMonitorFence;
    using BaseClass::device;
    using BaseClass::diagnostic;
    using BaseClass::DirectSubmissionHw;
    using BaseClass::disableCacheFlush;
    using BaseClass::disableCpuCacheFlush;
    using BaseClass::disableMonitorFence;
    using BaseClass::dispatchSemaphoreSection;
    using BaseClass::dispatchStartSection;
    using BaseClass::dispatchSwitchRingBufferSection;
    using BaseClass::dispatchWorkloadSection;
    using BaseClass::getCommandBufferPositionGpuAddress;
    using BaseClass::getSizeDispatch;
    using BaseClass::getSizeEnd;
    using BaseClass::getSizeSemaphoreSection;
    using BaseClass::getSizeStartSection;
    using BaseClass::getSizeSwitchRingBufferSection;
    using BaseClass::hwInfo;
    using BaseClass::osContext;
    using BaseClass::performDiagnosticMode;
    using BaseClass::ringBuffer;
    using BaseClass::ringBuffer2;
    using BaseClass::ringCommandStream;
    using BaseClass::ringStart;
    using BaseClass::semaphoreData;
    using BaseClass::semaphoreGpuVa;
    using BaseClass::semaphorePtr;
    using BaseClass::semaphores;
    using BaseClass::setReturnAddress;
    using BaseClass::stopRingBuffer;
    using BaseClass::switchRingBuffersAllocations;
    using BaseClass::workloadMode;
    using BaseClass::workloadModeOneExpectedValue;
    using BaseClass::workloadModeOneStoreAddress;
    using typename BaseClass::RingBufferUse;

    ~MockDirectSubmissionHw() override {
        if (ringStart) {
            stopRingBuffer();
        }
        deallocateResources();
    }

    bool allocateOsResources(DirectSubmissionAllocations &allocations) override {
        return allocateOsResourcesReturn;
    }

    bool submit(uint64_t gpuAddress, size_t size) override {
        submitGpuAddress = gpuAddress;
        submitSize = size;
        submitCount++;
        return submitReturn;
    }

    bool handleResidency() override {
        handleResidencyCount++;
        return handleResidencyReturn;
    }

    uint64_t switchRingBuffers() override {
        GraphicsAllocation *nextRingBuffer = switchRingBuffersAllocations();
        uint64_t currentBufferGpuVa = getCommandBufferPositionGpuAddress(ringCommandStream.getSpace(0));

        ringCommandStream.replaceBuffer(nextRingBuffer->getUnderlyingBuffer(), ringCommandStream.getMaxAvailableSpace());
        ringCommandStream.replaceGraphicsAllocation(nextRingBuffer);

        return currentBufferGpuVa;
    }

    uint64_t updateTagValue() override {
        return updateTagValueReturn;
    }

    void getTagAddressValue(TagData &tagData) override {
        tagData.tagAddress = tagAddressSetValue;
        tagData.tagValue = tagValueSetValue;
    }

    void performDiagnosticMode() override {
        if (!NEO::directSubmissionDiagnosticAvailable) {
            disabledDiagnosticCalled++;
        }
        uint32_t add = 1;
        if (diagnostic.get()) {
            add += diagnostic->getExecutionsCount();
        }
        *static_cast<volatile uint32_t *>(workloadModeOneStoreAddress) = workloadModeOneExpectedValue + add;
        BaseClass::performDiagnosticMode();
    }

    uint64_t updateTagValueReturn = 1ull;
    uint64_t tagAddressSetValue = MemoryConstants::pageSize;
    uint64_t tagValueSetValue = 1ull;
    uint64_t submitGpuAddress = 0ull;
    size_t submitSize = 0u;
    uint32_t submitCount = 0u;
    uint32_t handleResidencyCount = 0u;
    uint32_t disabledDiagnosticCalled = 0u;
    bool allocateOsResourcesReturn = true;
    bool submitReturn = true;
    bool handleResidencyReturn = true;
};
} // namespace NEO
