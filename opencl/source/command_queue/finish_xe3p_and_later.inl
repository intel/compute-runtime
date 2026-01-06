/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void NEO::CommandQueueHw<NEO::Family>::programPendingL3Flushes(NEO::CommandStreamReceiver &csr, bool &waitForTaskCountRequired, bool resolvePendingL3Flushes) {
    if (resolvePendingL3Flushes == false) {
        return;
    }

    if (getPendingL3FlushForHostVisibleResources()) {
        TakeOwnershipWrapper<CommandQueueHw<NEO::Family>> queueOwnership(*this);
        if (getPendingL3FlushForHostVisibleResources()) {

            csr.flushTagUpdate();

            NEO::CompletionStamp completionStamp = {
                csr.peekTaskCount(),
                std::max(this->taskLevel, csr.peekTaskLevel()),
                csr.obtainCurrentFlushStamp()};

            this->updateFromCompletionStamp(completionStamp, nullptr);
            this->setPendingL3FlushForHostVisibleResources(false);
            waitForTaskCountRequired = true;
        }
    }
}
