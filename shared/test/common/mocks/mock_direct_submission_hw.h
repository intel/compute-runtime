/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
namespace NEO {

template <typename GfxFamily, typename Dispatcher>
struct MockDirectSubmissionHw : public DirectSubmissionHw<GfxFamily, Dispatcher> {
    using BaseClass = DirectSubmissionHw<GfxFamily, Dispatcher>;
    using BaseClass::activeTiles;
    using BaseClass::allocateResources;
    using BaseClass::completionFenceAllocation;
    using BaseClass::copyCommandBufferIntoRing;
    using BaseClass::cpuCachelineFlush;
    using BaseClass::currentQueueWorkCount;
    using BaseClass::currentRelaxedOrderingQueueSize;
    using BaseClass::currentRingBuffer;
    using BaseClass::dcFlushRequired;
    using BaseClass::deallocateResources;
    using BaseClass::deferredTasksListAllocation;
    using BaseClass::detectGpuHang;
    using BaseClass::DirectSubmissionHw;
    using BaseClass::disableCacheFlush;
    using BaseClass::disableCpuCacheFlush;
    using BaseClass::disableMonitorFence;
    using BaseClass::dispatchDisablePrefetcher;
    using BaseClass::dispatchMonitorFenceRequired;
    using BaseClass::dispatchPartitionRegisterConfiguration;
    using BaseClass::dispatchPrefetchMitigation;
    using BaseClass::dispatchRelaxedOrderingReturnPtrRegs;
    using BaseClass::dispatchSemaphoreSection;
    using BaseClass::dispatchStartSection;
    using BaseClass::dispatchSwitchRingBufferSection;
    using BaseClass::dispatchUllsState;
    using BaseClass::dispatchWorkloadSection;
    using BaseClass::getSizeDisablePrefetcher;
    using BaseClass::getSizeDispatch;
    using BaseClass::getSizeDispatchRelaxedOrderingQueueStall;
    using BaseClass::getSizeEnd;
    using BaseClass::getSizeNewResourceHandler;
    using BaseClass::getSizePartitionRegisterConfigurationSection;
    using BaseClass::getSizePrefetchMitigation;
    using BaseClass::getSizeSemaphoreSection;
    using BaseClass::getSizeStartSection;
    using BaseClass::getSizeSwitchRingBufferSection;
    using BaseClass::getSizeSystemMemoryFenceAddress;
    using BaseClass::getTagAddressValueForRingSwitch;
    using BaseClass::globalFenceAllocation;
    using BaseClass::hwInfo;
    using BaseClass::immWritePostSyncOffset;
    using BaseClass::inputMonitorFenceDispatchRequirement;
    using BaseClass::isDisablePrefetcherRequired;
    using BaseClass::lastSubmittedThrottle;
    using BaseClass::miMemFenceRequired;
    using BaseClass::notifyKmdDuringMonitorFence;
    using BaseClass::osContext;
    using BaseClass::partitionConfigSet;
    using BaseClass::partitionedMode;
    using BaseClass::pciBarrierPtr;
    using BaseClass::preinitializedRelaxedOrderingScheduler;
    using BaseClass::preinitializedTaskStoreSection;
    using BaseClass::relaxedOrderingEnabled;
    using BaseClass::relaxedOrderingInitialized;
    using BaseClass::relaxedOrderingSchedulerAllocation;
    using BaseClass::relaxedOrderingSchedulerRequired;
    using BaseClass::reserved;
    using BaseClass::ringBuffers;
    using BaseClass::ringCommandStream;
    using BaseClass::ringStart;
    using BaseClass::rootDeviceEnvironment;
    using BaseClass::semaphoreData;
    using BaseClass::semaphoreGpuVa;
    using BaseClass::semaphorePtr;
    using BaseClass::semaphores;
    using BaseClass::setReturnAddress;
    using BaseClass::stopRingBuffer;
    using BaseClass::switchRingBuffersAllocations;
    using BaseClass::switchRingBuffersNeeded;
    using BaseClass::systemMemoryFenceAddressSet;
    using BaseClass::unblockGpu;
    using BaseClass::workPartitionAllocation;
    using typename BaseClass::RingBufferUse;

    ~MockDirectSubmissionHw() override {
        if (ringStart) {
            stopRingBuffer(false);
        }
        deallocateResources();
    }

    bool allocateOsResources() override {
        return allocateOsResourcesReturn;
    }

    void preinitializeRelaxedOrderingSections() override {
        preinitializeRelaxedOrderingSectionsCalled++;
        BaseClass::preinitializeRelaxedOrderingSections();
    }

    void dispatchStaticRelaxedOrderingScheduler() override {
        dispatchStaticRelaxedOrderingSchedulerCalled++;
        BaseClass::dispatchStaticRelaxedOrderingScheduler();
    }

    void dispatchRelaxedOrderingSchedulerSection(uint32_t value) override {
        dispatchRelaxedOrderingSchedulerSectionCalled++;
        BaseClass::dispatchRelaxedOrderingSchedulerSection(value);
    }

    void dispatchRelaxedOrderingQueueStall() override {
        dispatchRelaxedOrderingQueueStallCalled++;
        BaseClass::dispatchRelaxedOrderingQueueStall();
    }

    void dispatchTaskStoreSection(uint64_t taskStartSectionVa) override {
        dispatchTaskStoreSectionCalled++;
        BaseClass::dispatchTaskStoreSection(taskStartSectionVa);
    }

    void ensureRingCompletion() override {
        ensureRingCompletionCalled++;
        BaseClass::ensureRingCompletion();
    }

    bool makeResourcesResident(DirectSubmissionAllocations &allocations) override {
        makeResourcesResidentVectorSize = static_cast<uint32_t>(allocations.size());
        if (callBaseResident) {
            return BaseClass::makeResourcesResident(allocations);
        }
        return true;
    }

    bool submit(uint64_t gpuAddress, size_t size, const ResidencyContainer *allocationsForResidency) override {
        submitGpuAddress = gpuAddress;
        submitSize = size;
        submitCount++;
        return submitReturn;
    }

    bool handleResidency() override {
        handleResidencyCount++;
        return handleResidencyReturn;
    }

    void handleSwitchRingBuffers(ResidencyContainer *allocationsForResidency) override {}

    uint64_t updateTagValue(bool requireMonitorFence) override {
        return updateTagValueReturn;
    }

    void getTagAddressValue(TagData &tagData) override {
        tagData.tagAddress = tagAddressSetValue;
        tagData.tagValue = tagValueSetValue;
    }

    bool isCompleted(uint32_t ringBufferIndex) override {
        return this->isCompletedReturn;
    }

    void unblockPagingFenceSemaphore(uint64_t pagingFenceValue) override {
        this->pagingFenceValueToWait = pagingFenceValue;
    }
    void getTagAddressValueForRingSwitch(TagData &tagData) override {
        getTagAddressValueForRingSwitchCalled++;
        getTagAddressValue(tagData);
    }

    uint64_t updateTagValueReturn = 1ull;
    uint64_t tagAddressSetValue = MemoryConstants::pageSize;
    uint64_t tagValueSetValue = 1ull;
    uint64_t submitGpuAddress = 0ull;
    uint64_t pagingFenceValueToWait = 0ull;
    size_t submitSize = 0u;
    uint32_t submitCount = 0u;
    uint32_t handleResidencyCount = 0u;
    uint32_t preinitializeRelaxedOrderingSectionsCalled = 0;
    uint32_t dispatchStaticRelaxedOrderingSchedulerCalled = 0;
    uint32_t dispatchRelaxedOrderingSchedulerSectionCalled = 0;
    uint32_t dispatchRelaxedOrderingQueueStallCalled = 0;
    uint32_t dispatchTaskStoreSectionCalled = 0;
    uint32_t ensureRingCompletionCalled = 0;
    uint32_t getTagAddressValueForRingSwitchCalled = 0;
    uint32_t makeResourcesResidentVectorSize = 0u;
    bool allocateOsResourcesReturn = true;
    bool submitReturn = true;
    bool handleResidencyReturn = true;
    bool callBaseResident = false;
    bool isCompletedReturn = true;
};
} // namespace NEO
