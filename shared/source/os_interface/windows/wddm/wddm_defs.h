/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/windows_defs.h"

#include <cstdint>

namespace NEO {

struct WddmSubmitArguments {
    MonitoredFence *monitorFence;
    D3DKMT_HANDLE contextHandle;
    D3DKMT_HANDLE hwQueueHandle;
};

enum class WddmVersion : uint32_t {
    wddm20 = 0,
    wddm23,
    wddm32
};
} // namespace NEO
