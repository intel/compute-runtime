/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {

template <typename GfxFamily>
struct MockDirectSubmissionHw : public DirectSubmissionHw<GfxFamily> {
    using BaseClass = DirectSubmissionHw<GfxFamily>;
    using BaseClass::allocateResources;
    using BaseClass::cmdDispatcher;
    using BaseClass::completionRingBuffers;
    using BaseClass::cpuCachelineFlush;
    using BaseClass::currentQueueWorkCount;
    using BaseClass::currentRingBuffer;
    using BaseClass::deallocateResources;
    using BaseClass::device;
    using BaseClass::disableCpuCacheFlush;
    using BaseClass::dispatchEndingSection;
    using BaseClass::dispatchFlushSection;
    using BaseClass::dispatchSemaphoreSection;
    using BaseClass::dispatchStartSection;
    using BaseClass::dispatchSwitchRingBufferSection;
    using BaseClass::dispatchTagUpdateSection;
    using BaseClass::getCommandBufferPositionGpuAddress;
    using BaseClass::getSizeDispatch;
    using BaseClass::getSizeEnd;
    using BaseClass::getSizeEndingSection;
    using BaseClass::getSizeFlushSection;
    using BaseClass::getSizeSemaphoreSection;
    using BaseClass::getSizeStartSection;
    using BaseClass::getSizeSwitchRingBufferSection;
    using BaseClass::getSizeTagUpdateSection;
    using BaseClass::osContext;
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
    using typename BaseClass::RingBufferUse;

    MockDirectSubmissionHw(Device &device,
                           std::unique_ptr<Dispatcher> cmdDispatcher,
                           OsContext &osContext)
        : DirectSubmissionHw<GfxFamily>(device, std::move(cmdDispatcher), osContext) {
    }

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

    bool allocateOsResourcesReturn = true;
    bool submitReturn = true;
    bool handleResidencyReturn = true;
    uint32_t submitCount = 0u;
    uint32_t handleResidencyCount = 0u;
    size_t submitSize = 0u;
    uint64_t updateTagValueReturn = 1ull;
    uint64_t tagAddressSetValue = MemoryConstants::pageSize;
    uint64_t tagValueSetValue = 1ull;
    uint64_t submitGpuAddress = 0ull;
};
} // namespace NEO
