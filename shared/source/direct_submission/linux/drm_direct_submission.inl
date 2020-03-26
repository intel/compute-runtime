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

template <typename GfxFamily, typename Dispatcher>
DrmDirectSubmission<GfxFamily, Dispatcher>::DrmDirectSubmission(Device &device,
                                                                OsContext &osContext)
    : DirectSubmissionHw<GfxFamily, Dispatcher>(device, osContext) {
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::allocateOsResources(DirectSubmissionAllocations &allocations) {
    return false;
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::submit(uint64_t gpuAddress, size_t size) {
    return false;
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::handleResidency() {
    return false;
}

template <typename GfxFamily, typename Dispatcher>
uint64_t DrmDirectSubmission<GfxFamily, Dispatcher>::switchRingBuffers() {
    return 0ull;
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
