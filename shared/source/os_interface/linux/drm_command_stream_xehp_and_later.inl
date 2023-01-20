/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/submission_status.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_command_stream.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/sys_calls_common.h"

namespace NEO {

template <typename GfxFamily>
SubmissionStatus DrmCommandStreamReceiver<GfxFamily>::flushInternal(const BatchBuffer &batchBuffer, const ResidencyContainer &allocationsForResidency) {
    if (drm->useVMBindImmediate()) {
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

            auto processResidencySuccess = this->processResidency(allocationsForResidency, tileIterator);
            if (processResidencySuccess != SubmissionStatus::SUCCESS) {
                return processResidencySuccess;
            }

            if (DebugManager.flags.PrintDeviceAndEngineIdOnSubmission.get()) {
                printf("%u: Drm Submission of contextIndex: %u, with context id %u\n", SysCalls::getProcessId(), contextIndex, drmContextIds[contextIndex]);
            }

            int ret = this->exec(batchBuffer, tileIterator, drmContextIds[contextIndex], contextIndex);
            if (ret) {
                return Drm::getSubmissionStatusFromReturnCode(ret);
            }

            contextIndex++;

            if (DebugManager.flags.EnableWalkerPartition.get() == 0) {
                return SubmissionStatus::SUCCESS;
            }
        }
    }

    return SubmissionStatus::SUCCESS;
}

template <typename GfxFamily>
int DrmCommandStreamReceiver<GfxFamily>::waitUserFence(TaskCountType waitValue) {
    int ret = 0;
    StackVec<uint32_t, 32> ctxIds;
    uint64_t tagAddress = castToUint64(const_cast<TagAddressType *>(getTagAddress()));
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
            ret |= this->drm->waitUserFence(ctxIds[i], tagAddress, waitValue, Drm::ValueWidth::U64, kmdWaitTimeout, 0u);
            tagAddress += this->postSyncWriteOffset;
        }
    } else {
        for (uint32_t i = 0; i < this->activePartitions; i++) {
            ret |= this->drm->waitUserFence(0u, tagAddress, waitValue, Drm::ValueWidth::U64, kmdWaitTimeout, 0u);
            tagAddress += this->postSyncWriteOffset;
        }
    }

    return ret;
}

} // namespace NEO
