/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/memory_manager/eviction_status.h"
#include "core/os_interface/windows/wddm/wddm_defs.h"
#include "core/os_interface/windows/windows_defs.h"

#include <vector>

namespace NEO {

namespace WddmMockHelpers {
struct CallResult {
    uint32_t called = 0;
    uint64_t uint64ParamPassed = -1;
    bool success = false;
    void *cpuPtrPassed = nullptr;
};
struct MakeResidentCall : CallResult {
    std::vector<D3DKMT_HANDLE> handlePack;
    uint32_t handleCount = 0;
};
struct KmDafLockCall : CallResult {
    std::vector<D3DKMT_HANDLE> lockedAllocations;
};
struct WaitFromCpuResult : CallResult {
    const MonitoredFence *monitoredFence = nullptr;
};
struct FreeGpuVirtualAddressCall : CallResult {
    uint64_t sizePassed = -1;
};
struct MemoryOperationResult : CallResult {
    MemoryOperationsStatus operationSuccess = MemoryOperationsStatus::UNSUPPORTED;
};

struct SubmitResult : CallResult {
    uint64_t commandBufferSubmitted = 0ull;
    void *commandHeaderSubmitted = nullptr;
    size_t size = 0u;
    WddmSubmitArguments submitArgs = {0};
};

} // namespace WddmMockHelpers

} // namespace NEO
