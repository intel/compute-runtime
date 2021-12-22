/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/direct_submission/direct_submission_hw_diagnostic_mode.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
namespace NEO {

template <typename GfxFamily, typename Dispatcher>
struct MockDirectSubmissionHw : public DirectSubmissionHw<GfxFamily, Dispatcher> {
    using BaseClass = DirectSubmissionHw<GfxFamily, Dispatcher>;
    using BaseClass::activeTiles;
    using BaseClass::allocateResources;
    using BaseClass::completionRingBuffers;
    using BaseClass::cpuCachelineFlush;
    using BaseClass::currentQueueWorkCount;
    using BaseClass::currentRingBuffer;
    using BaseClass::deallocateResources;
    using BaseClass::device;
    using BaseClass::diagnostic;
    using BaseClass::DirectSubmissionHw;
    using BaseClass::disableCacheFlush;
    using BaseClass::disableCpuCacheFlush;
    using BaseClass::disableMonitorFence;
    using BaseClass::dispatchDisablePrefetcher;
    using BaseClass::dispatchPartitionRegisterConfiguration;
    using BaseClass::dispatchPrefetchMitigation;
    using BaseClass::dispatchSemaphoreSection;
    using BaseClass::dispatchStartSection;
    using BaseClass::dispatchSwitchRingBufferSection;
    using BaseClass::dispatchWorkloadSection;
    using BaseClass::getCommandBufferPositionGpuAddress;
    using BaseClass::getDiagnosticModeSection;
    using BaseClass::getSizeDisablePrefetcher;
    using BaseClass::getSizeDispatch;
    using BaseClass::getSizeEnd;
    using BaseClass::getSizePartitionRegisterConfigurationSection;
    using BaseClass::getSizePrefetchMitigation;
    using BaseClass::getSizeSemaphoreSection;
    using BaseClass::getSizeStartSection;
    using BaseClass::getSizeSwitchRingBufferSection;
    using BaseClass::hwInfo;
    using BaseClass::osContext;
    using BaseClass::partitionConfigSet;
    using BaseClass::partitionedMode;
    using BaseClass::performDiagnosticMode;
    using BaseClass::postSyncOffset;
    using BaseClass::ringBuffer;
    using BaseClass::ringBuffer2;
    using BaseClass::ringCommandStream;
    using BaseClass::ringStart;
    using BaseClass::semaphoreData;
    using BaseClass::semaphoreGpuVa;
    using BaseClass::semaphorePtr;
    using BaseClass::semaphores;
    using BaseClass::setReturnAddress;
    using BaseClass::startRingBuffer;
    using BaseClass::stopRingBuffer;
    using BaseClass::switchRingBuffersAllocations;
    using BaseClass::useNotifyForPostSync;
    using BaseClass::workloadMode;
    using BaseClass::workloadModeOneExpectedValue;
    using BaseClass::workloadModeOneStoreAddress;
    using BaseClass::workPartitionAllocation;
    using typename BaseClass::RingBufferUse;

    ~MockDirectSubmissionHw() override {
        if (ringStart) {
            stopRingBuffer();
        }
        deallocateResources();
    }

    bool allocateOsResources() override {
        return allocateOsResourcesReturn;
    }

    bool makeResourcesResident(DirectSubmissionAllocations &allocations) override {
        makeResourcesResidentVectorSize = static_cast<uint32_t>(allocations.size());
        if (callBaseResident) {
            return BaseClass::makeResourcesResident(allocations);
        }
        return true;
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

    void handleSwitchRingBuffers() override {}

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
    uint32_t makeResourcesResidentVectorSize = 0u;
    bool allocateOsResourcesReturn = true;
    bool submitReturn = true;
    bool handleResidencyReturn = true;
    bool callBaseResident = false;
};
} // namespace NEO
