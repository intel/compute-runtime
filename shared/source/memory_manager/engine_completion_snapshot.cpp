/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/engine_completion_snapshot.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/ptr_math.h"

namespace NEO {

bool isEngineCompletionSnapshotReady(const EngineCompletionSnapshot &snapshot) {
    for (const auto &[commandStreamReceiver, taskCount] : snapshot) {
        volatile TagAddressType *pollAddress = commandStreamReceiver->getTagAddress();
        if (nullptr == pollAddress) {
            // nothing to poll, so nothing to wait for - same as MemoryManager::allocInUse
            // and MemoryManager::waitForEnginesCompletion
            continue;
        }
        for (uint32_t partition = 0; partition < commandStreamReceiver->getActivePartitions(); partition++) {
            if (*pollAddress < taskCount) {
                return false;
            }
            pollAddress = ptrOffset(pollAddress, commandStreamReceiver->getImmWritePostSyncWriteOffset());
        }
    }
    return true;
}

} // namespace NEO
