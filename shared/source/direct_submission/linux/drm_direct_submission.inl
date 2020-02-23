/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/direct_submission/linux/drm_direct_submission.h"
#include "shared/source/os_interface/linux/drm_allocation.h"

namespace NEO {

template <typename GfxFamily>
DrmDirectSubmission<GfxFamily>::DrmDirectSubmission(Device &device,
                                                    std::unique_ptr<Dispatcher> cmdDispatcher,
                                                    OsContext &osContext)
    : DirectSubmissionHw<GfxFamily>(device, std::move(cmdDispatcher), osContext) {
}

template <typename GfxFamily>
bool DrmDirectSubmission<GfxFamily>::allocateOsResources(DirectSubmissionAllocations &allocations) {
    return false;
}

template <typename GfxFamily>
bool DrmDirectSubmission<GfxFamily>::submit(uint64_t gpuAddress, size_t size) {
    return false;
}

template <typename GfxFamily>
bool DrmDirectSubmission<GfxFamily>::handleResidency() {
    return false;
}

template <typename GfxFamily>
uint64_t DrmDirectSubmission<GfxFamily>::switchRingBuffers() {
    return 0ull;
}

template <typename GfxFamily>
uint64_t DrmDirectSubmission<GfxFamily>::updateTagValue() {
    return 0ull;
}

template <typename GfxFamily>
void DrmDirectSubmission<GfxFamily>::getTagAddressValue(TagData &tagData) {
    tagData.tagAddress = 0ull;
    tagData.tagValue = 0ull;
}
} // namespace NEO
