/*
 * Copyright (C) 2016-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_command_stream.h"
#include "shared/source/os_interface/linux/os_context_linux.h"

namespace NEO {

template <typename GfxFamily>
int DrmCommandStreamReceiver<GfxFamily>::flushInternal(const BatchBuffer &batchBuffer, const ResidencyContainer &allocationsForResidency) {
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

            int ret = this->exec(batchBuffer, tileIterator, drmContextIds[contextIndex]);
            if (ret) {
                return ret;
            }

            contextIndex++;

            if (DebugManager.flags.EnableWalkerPartition.get() == 0 || batchBuffer.useSingleSubdevice) {
                return 0;
            }
        }
    }

    return 0;
}

template <typename GfxFamily>
int DrmCommandStreamReceiver<GfxFamily>::waitUserFence(uint32_t waitValue) {
    int ret = 0;
    StackVec<uint32_t, 32> ctxIds;
    uint64_t tagAddress = castToUint64(const_cast<uint32_t *>(getTagAddress()));
    if (useContextForUserFenceWait) {
        for (auto tileIterator = 0u; tileIterator < this->osContext->getDeviceBitfield().size(); tileIterator++) {
            uint32_t ctxId = 0u;
            if (this->osContext->getDeviceBitfield().test(tileIterator)) {
                ctxId = static_cast<const OsContextLinux *>(osContext)->getDrmContextIds()[tileIterator];
                ctxIds.push_back(ctxId);
            }
        }
        UNRECOVERABLE_IF(ctxIds.size() != this->activePartitions);
        for (uint32_t i = 0; i < this->activePartitions; i++) {
            ret |= this->drm->waitUserFence(ctxIds[i], tagAddress, waitValue, Drm::ValueWidth::U32, kmdWaitTimeout, 0u);
            tagAddress += CommonConstants::partitionAddressOffset;
        }
    } else {
        for (uint32_t i = 0; i < this->activePartitions; i++) {
            ret |= this->drm->waitUserFence(0u, tagAddress, waitValue, Drm::ValueWidth::U32, kmdWaitTimeout, 0u);
            tagAddress += CommonConstants::partitionAddressOffset;
        }
    }

    return ret;
}

} // namespace NEO
