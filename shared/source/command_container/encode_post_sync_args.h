/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>

namespace NEO {

class Device;
class InOrderExecInfo;
class TagNodeBase;

struct EncodePostSyncArgs {
    size_t eventPacketSize = 0;
    uint32_t eventPacketsCount = 0;
    uint64_t eventAddress = 0;
    uint64_t postSyncImmValue = 0;
    uint64_t inOrderCounterValue = 0;
    uint64_t inOrderAtomicSignallingValue = 1;
    uint64_t inOrderIncrementGpuAddress = 0;
    uint64_t inOrderIncrementValue = 0;
    Device *device = nullptr;
    NEO::InOrderExecInfo *inOrderExecInfo = nullptr;
    NEO::TagNodeBase *tsNode = nullptr;
    bool dcFlushEnable = false;
    bool interruptEvent = false;
    bool isCounterBasedEvent = false;
    bool isFlushL3ForExternalAllocationRequired = false;
    bool isFlushL3ForHostUsmRequired = false;
    bool isHostScopeSignalEvent = false;
    bool isTimestampEvent = false;
    bool isUsingSystemAllocation = false;

    bool requiresSystemMemoryFence() const;

    bool isRegularEvent() const {
        return (eventAddress != 0) && (inOrderExecInfo == nullptr);
    }

    bool hasHostWaitablePostSync() const {
        return (isHostScopeSignalEvent && (eventAddress != 0)) || (inOrderExecInfo != nullptr);
    }
};

} // namespace NEO
