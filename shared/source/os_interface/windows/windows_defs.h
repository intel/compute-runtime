/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/d3dkmthk_wrapper.h"

#include <cstdint>

namespace NEO {

constexpr uintptr_t windowsMinAddress = 0x200000;

struct MonitoredFence {
    D3DKMT_HANDLE fenceHandle = 0;
    D3DGPU_VIRTUAL_ADDRESS gpuAddress = 0;
    volatile uint64_t *cpuAddress = nullptr;
    volatile uint64_t currentFenceValue = 0;
    uint64_t lastSubmittedFence = 0;
};

} // namespace NEO
