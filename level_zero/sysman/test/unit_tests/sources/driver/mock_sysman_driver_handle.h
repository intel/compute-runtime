/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"

namespace L0 {
namespace Sysman {
namespace ult {

struct MockSysmanDriverHandleImp : public SysmanDriverHandleImp {
    // Expose protected deferred-discovery state so tests can set it directly
    using SysmanDriverHandleImp::deferredDiscoveryMode;
    using SysmanDriverHandleImp::devicesDiscovered;
    using SysmanDriverHandleImp::savedExecutionEnvironment;

    // Inheritance-based mock: override the virtual discovery seam so no OS syscalls are made
    // during deferred discovery. The base performDeferredDiscovery() will receive an empty
    // hwDeviceIds vector, find 0 devices, and return ZE_RESULT_ERROR_UNINITIALIZED.
    HwDeviceIds discoverHwDevices(NEO::ExecutionEnvironment &) override {
        return {};
    }

    // Track how many times deferred discovery is triggered (used by idempotency tests)
    uint32_t deferredDiscoveryCallCount = 0;

    ze_result_t performDeferredDiscovery() override {
        deferredDiscoveryCallCount++;
        return SysmanDriverHandleImp::performDeferredDiscovery();
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
