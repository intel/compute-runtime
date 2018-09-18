/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace OCLRT {

template <typename GfxFamily>
struct DeviceCallbacks {
    static long __stdcall notifyAubCapture(void *csrHandle, uint64_t gfxAddress, size_t gfxSize, bool allocate);
};

template <typename GfxFamily>
struct TTCallbacks {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;

    static int __stdcall writeL3Address(void *queueHandle, uint64_t l3GfxAddress, uint64_t regOffset);
};

} // namespace OCLRT
