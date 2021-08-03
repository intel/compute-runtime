/*
 * Copyright (C) 2016-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/os_context_linux.h"

#include "opencl/source/os_interface/linux/drm_command_stream.h"

namespace NEO {

template <typename GfxFamily>
void DrmCommandStreamReceiver<GfxFamily>::flushInternal(const BatchBuffer &batchBuffer, const ResidencyContainer &allocationsForResidency) {
    auto useImmediateExt = this->drm->isDirectSubmissionActive();

    if (DebugManager.flags.EnableImmediateVmBindExt.get() != -1) {
        useImmediateExt = DebugManager.flags.EnableImmediateVmBindExt.get();
    }

    if (useImmediateExt) {
        auto osContextLinux = static_cast<OsContextLinux *>(this->osContext);
        osContextLinux->waitForPagingFence();
    }

    auto &drmContextIds = static_cast<const OsContextLinux *>(osContext)->getDrmContextIds();

    uint32_t contextIndex = 0;
    for (auto tileIterator = 0u; tileIterator < this->osContext->getDeviceBitfield().size(); tileIterator++) {
        if (this->osContext->getDeviceBitfield().test(tileIterator)) {
            if (DebugManager.flags.ForceExecutionTile.get() != -1 && this->osContext->getDeviceBitfield().count() > 1) {
                tileIterator = contextIndex = DebugManager.flags.ForceExecutionTile.get();
            }

            this->processResidency(allocationsForResidency, tileIterator);
            if (DebugManager.flags.PrintDeviceAndEngineIdOnSubmission.get()) {
                printf("Drm Submission of contextIndex: %u, with context id %u\n", contextIndex, drmContextIds[contextIndex]);
            }

            this->exec(batchBuffer, tileIterator, drmContextIds[contextIndex]);

            contextIndex++;

            if (DebugManager.flags.EnableWalkerPartition.get() == 0 || batchBuffer.useSingleSubdevice) {
                return;
            }
        }
    }
}

template <typename GfxFamily>
int DrmCommandStreamReceiver<GfxFamily>::waitUserFence(uint32_t waitValue) {
    int ret = 0;
    uint64_t tagAddress = castToUint64(const_cast<uint32_t *>(getTagAddress()));
    if (useContextForUserFenceWait) {
        for (auto tileIterator = 0u; tileIterator < this->osContext->getDeviceBitfield().size(); tileIterator++) {
            uint32_t ctxId = 0u;
            if (this->osContext->getDeviceBitfield().test(tileIterator)) {
                ctxId = static_cast<const OsContextLinux *>(osContext)->getDrmContextIds()[tileIterator];
                ret |= this->drm->waitUserFence(ctxId, tagAddress, waitValue, Drm::ValueWidth::U32, kmdWaitTimeout, 0u);
            }
        }
    } else {
        ret = this->drm->waitUserFence(0u, tagAddress, waitValue, Drm::ValueWidth::U32, kmdWaitTimeout, 0u);
    }

    return ret;
}

} // namespace NEO
