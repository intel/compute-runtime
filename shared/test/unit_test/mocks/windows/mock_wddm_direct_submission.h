/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/direct_submission/windows/wddm_direct_submission.h"

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
struct MockWddmDirectSubmission : public WddmDirectSubmission<GfxFamily, Dispatcher> {
    using BaseClass = WddmDirectSubmission<GfxFamily, Dispatcher>;
    using BaseClass::activeTiles;
    using BaseClass::allocateOsResources;
    using BaseClass::allocateResources;
    using BaseClass::commandBufferHeader;
    using BaseClass::completionFenceAllocation;
    using BaseClass::currentRingBuffer;
    using BaseClass::detectGpuHang;
    using BaseClass::dispatchMonitorFenceRequired;
    using BaseClass::getSizeDisablePrefetcher;
    using BaseClass::getSizeDispatch;
    using BaseClass::getSizeDispatchRelaxedOrderingQueueStall;
    using BaseClass::getSizeEnd;
    using BaseClass::getSizeNewResourceHandler;
    using BaseClass::getSizePrefetchMitigation;
    using BaseClass::getSizeSemaphoreSection;
    using BaseClass::getSizeSwitchRingBufferSection;
    using BaseClass::getSizeSystemMemoryFenceAddress;
    using BaseClass::getTagAddressValue;
    using BaseClass::globalFenceAllocation;
    using BaseClass::gpuVaForAdditionalSynchronizationWA;
    using BaseClass::handleCompletionFence;
    using BaseClass::handleNewResourcesSubmission;
    using BaseClass::handleResidency;
    using BaseClass::handleStopRingBuffer;
    using BaseClass::handleSwitchRingBuffers;
    using BaseClass::isCompleted;
    using BaseClass::isDisablePrefetcherRequired;
    using BaseClass::isNewResourceHandleNeeded;
    using BaseClass::lastSubmittedThrottle;
    using BaseClass::maxRingBufferCount;
    using BaseClass::miMemFenceRequired;
    using BaseClass::osContextWin;
    using BaseClass::pciBarrierPtr;
    using BaseClass::previousRingBuffer;
    using BaseClass::relaxedOrderingEnabled;
    using BaseClass::ringBufferEndCompletionTagData;
    using BaseClass::ringBuffers;
    using BaseClass::ringCommandStream;
    using BaseClass::ringFence;
    using BaseClass::ringStart;
    using BaseClass::rootDeviceEnvironment;
    using BaseClass::semaphoreData;
    using BaseClass::semaphoreGpuVa;
    using BaseClass::semaphorePtr;
    using BaseClass::semaphores;
    using BaseClass::submit;
    using BaseClass::switchRingBuffers;
    using BaseClass::tagAddress;
    using BaseClass::updateMonitorFenceValueForResidencyList;
    using BaseClass::updateTagValue;
    using BaseClass::wddm;
    using BaseClass::WddmDirectSubmission;
    using typename BaseClass::RingBufferUse;

    void updateMonitorFenceValueForResidencyList(ResidencyContainer *allocationsForResidency) override {
        updateMonitorFenceValueForResidencyListCalled++;
        BaseClass::updateMonitorFenceValueForResidencyList(allocationsForResidency);
    }
    uint64_t updateTagValueImpl(uint32_t completionBufferIndex) override {
        ringBufferForCompletionFence = completionBufferIndex;
        return BaseClass::updateTagValueImpl(completionBufferIndex);
    }
    void updateTagValueImplForSwitchRingBuffer(uint32_t completionBufferIndex) override {
        ringBufferForCompletionFence = completionBufferIndex;
        BaseClass::updateTagValueImplForSwitchRingBuffer(completionBufferIndex);
    }
    void getTagAddressValue(TagData &tagData) override {
        if (setTagAddressValue) {
            tagData.tagAddress = tagAddressSetValue;
            tagData.tagValue = tagValueSetValue;
        } else {
            BaseClass::getTagAddressValue(tagData);
        }
    }
    uint32_t updateMonitorFenceValueForResidencyListCalled = 0u;
    uint32_t ringBufferForCompletionFence = 0u;
    bool setTagAddressValue = false;
    uint64_t tagAddressSetValue = MemoryConstants::pageSize;
    uint64_t tagValueSetValue = 1ull;
};
} // namespace NEO
