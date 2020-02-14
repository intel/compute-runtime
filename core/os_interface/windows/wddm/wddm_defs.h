/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/os_interface/windows/windows_defs.h"

#include <cstdint>

namespace NEO {

struct WddmSubmitArguments {
    MonitoredFence *monitorFence;
    D3DKMT_HANDLE contextHandle;
    D3DKMT_HANDLE hwQueueHandle;
};

enum class WddmVersion : uint32_t {
    WDDM_2_0 = 0,
    WDDM_2_3
};
} // namespace NEO
