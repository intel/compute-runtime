/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_command_stream.h"

namespace NEO {

template <typename GfxFamily>
int DrmCommandStreamReceiver<GfxFamily>::flushInternal(const BatchBuffer &batchBuffer, const ResidencyContainer &allocationsForResidency) {
    this->processResidency(allocationsForResidency, 0u);
    int ret = this->exec(batchBuffer, 0u, static_cast<const OsContextLinux *>(osContext)->getDrmContextIds()[0]);

    return ret;
}

template <typename GfxFamily>
int DrmCommandStreamReceiver<GfxFamily>::waitUserFence(uint32_t waitValue) {
    uint32_t ctxId = 0u;
    uint64_t tagAddress = castToUint64(const_cast<uint32_t *>(getTagAddress()));
    if (useContextForUserFenceWait) {
        ctxId = static_cast<const OsContextLinux *>(osContext)->getDrmContextIds()[0];
    }
    return this->drm->waitUserFence(ctxId, tagAddress, waitValue, Drm::ValueWidth::U32, kmdWaitTimeout, 0u);
}

} // namespace NEO
