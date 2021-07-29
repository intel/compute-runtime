/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/linux/drm_direct_submission.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_context_linux.h"

#include <memory>

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
DrmDirectSubmission<GfxFamily, Dispatcher>::DrmDirectSubmission(Device &device,
                                                                OsContext &osContext)
    : DirectSubmissionHw<GfxFamily, Dispatcher>(device, osContext) {

    this->disableMonitorFence = true;

    if (DebugManager.flags.DirectSubmissionDisableMonitorFence.get() != -1) {
        this->disableMonitorFence = DebugManager.flags.DirectSubmissionDisableMonitorFence.get();
    }

    auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);
    osContextLinux->getDrm().setDirectSubmissionActive(true);
};

template <typename GfxFamily, typename Dispatcher>
inline DrmDirectSubmission<GfxFamily, Dispatcher>::~DrmDirectSubmission() {
    if (this->ringStart) {
        this->stopRingBuffer();
        if (this->disableMonitorFence) {
            this->currentTagData.tagValue++;
        }
        this->wait(static_cast<uint32_t>(this->currentTagData.tagValue));
        auto bb = static_cast<DrmAllocation *>(this->ringBuffer)->getBO();
        bb->wait(-1);
    }
    this->deallocateResources();
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::allocateOsResources() {
    this->currentTagData.tagAddress = this->semaphoreGpuVa + MemoryConstants::cacheLineSize;
    this->currentTagData.tagValue = 0u;
    this->tagAddress = reinterpret_cast<volatile uint32_t *>(reinterpret_cast<uint8_t *>(this->semaphorePtr) + MemoryConstants::cacheLineSize);
    return true;
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::submit(uint64_t gpuAddress, size_t size) {
    auto bb = static_cast<DrmAllocation *>(this->ringCommandStream.getGraphicsAllocation())->getBO();

    auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);
    auto execFlags = osContextLinux->getEngineFlag() | I915_EXEC_NO_RELOC;
    auto &drmContextIds = osContextLinux->getDrmContextIds();

    drm_i915_gem_exec_object2 execObject{};

    this->handleResidency();

    bool ret = false;
    uint32_t drmContextId = 0u;
    for (auto drmIterator = 0u; drmIterator < osContextLinux->getDeviceBitfield().size(); drmIterator++) {
        if (osContextLinux->getDeviceBitfield().test(drmIterator)) {
            ret |= !!bb->exec(static_cast<uint32_t>(size),
                              0,
                              execFlags,
                              false,
                              &this->osContext,
                              drmIterator,
                              drmContextIds[drmContextId],
                              nullptr,
                              0,
                              &execObject);
            drmContextId++;
        }
    }

    return !ret;
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::handleResidency() {
    auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);
    osContextLinux->waitForPagingFence();
    return true;
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::isNewResourceHandleNeeded() {
    auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);
    auto newResourcesBound = osContextLinux->getDrm().getNewResourceBound();

    if (DebugManager.flags.DirectSubmissionNewResourceTlbFlush.get() != -1) {
        newResourcesBound = DebugManager.flags.DirectSubmissionNewResourceTlbFlush.get();
    }

    return newResourcesBound;
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::handleNewResourcesSubmission() {
    if (isNewResourceHandleNeeded()) {
        Dispatcher::dispatchTlbFlush(this->ringCommandStream, this->gpuVaForMiFlush);
    }

    auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);
    if (!EngineHelpers::isBcs(osContextLinux->getEngineType())) {
        osContextLinux->getDrm().setNewResourceBound(false);
    }
}

template <typename GfxFamily, typename Dispatcher>
size_t DrmDirectSubmission<GfxFamily, Dispatcher>::getSizeNewResourceHandler() {
    size_t size = 0u;

    if (isNewResourceHandleNeeded()) {
        size += Dispatcher::getSizeTlbFlush();
    }

    return size;
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::handleSwitchRingBuffers() {
    if (this->disableMonitorFence) {
        auto previousRingBuffer = this->currentRingBuffer == DirectSubmissionHw<GfxFamily, Dispatcher>::RingBufferUse::FirstBuffer ? DirectSubmissionHw<GfxFamily, Dispatcher>::RingBufferUse::SecondBuffer : DirectSubmissionHw<GfxFamily, Dispatcher>::RingBufferUse::FirstBuffer;
        this->currentTagData.tagValue++;
        this->completionRingBuffers[previousRingBuffer] = this->currentTagData.tagValue;
    }

    if (this->ringStart) {
        if (this->completionRingBuffers[this->currentRingBuffer] != 0) {
            this->wait(static_cast<uint32_t>(this->completionRingBuffers[this->currentRingBuffer]));
        }
    }
}

template <typename GfxFamily, typename Dispatcher>
uint64_t DrmDirectSubmission<GfxFamily, Dispatcher>::updateTagValue() {
    if (!this->disableMonitorFence) {
        this->currentTagData.tagValue++;
        this->completionRingBuffers[this->currentRingBuffer] = this->currentTagData.tagValue;
    }
    return 0ull;
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::getTagAddressValue(TagData &tagData) {
    tagData.tagAddress = this->currentTagData.tagAddress;
    tagData.tagValue = this->currentTagData.tagValue + 1;
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::wait(uint32_t taskCountToWait) {
    while (taskCountToWait > *this->tagAddress) {
    }
}

} // namespace NEO
