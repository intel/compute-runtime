/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/memory_operations_status.h"
#include "shared/source/os_interface/windows/wddm/wddm_defs.h"
#include "shared/source/os_interface/windows/windows_defs.h"

#include <vector>

namespace NEO {

namespace WddmMockHelpers {
struct CallResult {
    uint32_t called = 0;
    uint64_t uint64ParamPassed = -1;
    size_t alignment = 0;
    bool success = false;
    void *cpuPtrPassed = nullptr;
};
struct MakeResidentCall : CallResult {
    std::vector<D3DKMT_HANDLE> handlePack;
    uint32_t handleCount = 0;
    bool cantTrimFurther{};
    size_t totalSize{};
};
struct WaitFromCpuResult : CallResult {
    const MonitoredFence *monitoredFence = nullptr;
};
struct MonitoredFenceKmdWaitEventResult {
    uint32_t createCalled = 0;
    uint32_t resetCalled = 0;
    uint32_t waitCalled = 0;
    HANDLE eventHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(1));
    HANDLE resetEventHandle = nullptr;
    HANDLE waitEventHandle = nullptr;
    bool resetSuccess = true;
    bool waitResult = false;
    uint32_t timeoutMilliseconds = 0;
    volatile uint64_t *fenceAddressToSignal = nullptr;
    uint64_t fenceValueToSignal = 0;
};
struct FreeGpuVirtualAddressCall : CallResult {
    uint64_t sizePassed = -1;
};
struct MemoryOperationResult : CallResult {
    MemoryOperationsStatus operationSuccess = MemoryOperationsStatus::unsupported;
};

struct WaitOnPagingFenceFromCpuResult : CallResult {
    bool isKmdWaitNeededPassed = false;
};

struct SubmitResult : CallResult {
    uint64_t commandBufferSubmitted = 0ull;
    void *commandHeaderSubmitted = nullptr;
    size_t size = 0u;
    WddmSubmitArguments submitArgs = {0};
};

} // namespace WddmMockHelpers

} // namespace NEO
