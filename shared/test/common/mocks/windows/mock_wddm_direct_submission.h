/*
 * Copyright (C) 2020-2021 Intel Corporation
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
    using BaseClass::completionRingBuffers;
    using BaseClass::currentRingBuffer;
    using BaseClass::getSizeDispatch;
    using BaseClass::getSizeSemaphoreSection;
    using BaseClass::getSizeSwitchRingBufferSection;
    using BaseClass::getTagAddressValue;
    using BaseClass::handleCompletionRingBuffer;
    using BaseClass::handleResidency;
    using BaseClass::osContextWin;
    using BaseClass::ringBuffer;
    using BaseClass::ringBuffer2;
    using BaseClass::ringCommandStream;
    using BaseClass::ringFence;
    using BaseClass::ringStart;
    using BaseClass::semaphores;
    using BaseClass::submit;
    using BaseClass::switchRingBuffers;
    using BaseClass::updateTagValue;
    using BaseClass::useNotifyForPostSync;
    using BaseClass::wddm;
    using BaseClass::WddmDirectSubmission;
    using typename BaseClass::RingBufferUse;
};
} // namespace NEO
