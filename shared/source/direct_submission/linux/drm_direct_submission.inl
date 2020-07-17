/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/direct_submission/linux/drm_direct_submission.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_context_linux.h"

#include <memory>

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
inline std::unique_ptr<DirectSubmissionHw<GfxFamily, Dispatcher>> DirectSubmissionHw<GfxFamily, Dispatcher>::create(Device &device, OsContext &osContext) {
    return std::make_unique<DrmDirectSubmission<GfxFamily, Dispatcher>>(device, osContext);
}

template <typename GfxFamily, typename Dispatcher>
DrmDirectSubmission<GfxFamily, Dispatcher>::DrmDirectSubmission(Device &device,
                                                                OsContext &osContext)
    : DirectSubmissionHw<GfxFamily, Dispatcher>(device, osContext) {
    this->disableMonitorFence = true;
}

template <typename GfxFamily, typename Dispatcher>
inline DrmDirectSubmission<GfxFamily, Dispatcher>::~DrmDirectSubmission() {
    if (this->ringStart) {
        this->stopRingBuffer();
        auto bb = static_cast<DrmAllocation *>(this->ringCommandStream.getGraphicsAllocation())->getBO();
        bb->wait(-1);
    }
    this->deallocateResources();
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::allocateOsResources() {
    return true;
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::submit(uint64_t gpuAddress, size_t size) {
    auto bb = static_cast<DrmAllocation *>(this->ringCommandStream.getGraphicsAllocation())->getBO();

    auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);
    auto execFlags = osContextLinux->getEngineFlag() | I915_EXEC_NO_RELOC;
    auto &drmContextIds = osContextLinux->getDrmContextIds();

    drm_i915_gem_exec_object2 execObject{};

    bool ret = false;
    for (const auto &drmContextId : drmContextIds) {
        ret |= bb->exec(static_cast<uint32_t>(size),
                        0,
                        execFlags,
                        false,
                        drmContextId,
                        nullptr,
                        0,
                        &execObject);
    }

    return !ret;
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::handleResidency() {
    return true;
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::handleSwitchRingBuffers() {
}

template <typename GfxFamily, typename Dispatcher>
uint64_t DrmDirectSubmission<GfxFamily, Dispatcher>::updateTagValue() {
    return 0ull;
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::getTagAddressValue(TagData &tagData) {
    tagData.tagAddress = 0ull;
    tagData.tagValue = 0ull;
}
} // namespace NEO
