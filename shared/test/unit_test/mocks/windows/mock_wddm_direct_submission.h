/*
 * Copyright (C) 2020-2024 Intel Corporation
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
    using BaseClass::disableMonitorFence;
    using BaseClass::dispatchMonitorFenceRequired;
    using BaseClass::getSizeDispatch;
    using BaseClass::getSizeNewResourceHandler;
    using BaseClass::getSizeSemaphoreSection;
    using BaseClass::getSizeSwitchRingBufferSection;
    using BaseClass::getSizeSystemMemoryFenceAddress;
    using BaseClass::getTagAddressValue;
    using BaseClass::gpuVaForAdditionalSynchronizationWA;
    using BaseClass::handleCompletionFence;
    using BaseClass::handleNewResourcesSubmission;
    using BaseClass::handleResidency;
    using BaseClass::handleStopRingBuffer;
    using BaseClass::handleSwitchRingBuffers;
    using BaseClass::inputMonitorFenceDispatchRequirement;
    using BaseClass::isCompleted;
    using BaseClass::isNewResourceHandleNeeded;
    using BaseClass::lastSubmittedThrottle;
    using BaseClass::miMemFenceRequired;
    using BaseClass::osContextWin;
    using BaseClass::previousRingBuffer;
    using BaseClass::ringBuffers;
    using BaseClass::ringCommandStream;
    using BaseClass::ringFence;
    using BaseClass::ringStart;
    using BaseClass::semaphoreData;
    using BaseClass::semaphores;
    using BaseClass::submit;
    using BaseClass::switchRingBuffers;
    using BaseClass::updateMonitorFenceValueForResidencyList;
    using BaseClass::updateTagValue;
    using BaseClass::useNotifyForPostSync;
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
    uint32_t updateMonitorFenceValueForResidencyListCalled = 0u;
    uint32_t ringBufferForCompletionFence = 0u;
};
} // namespace NEO
