/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/linear_stream.h"
#include "runtime/helpers/gmm_callbacks.h"
#include "runtime/helpers/hw_helper.h"

#include <cstdint>

namespace NEO {

template <typename GfxFamily>
long __stdcall DeviceCallbacks<GfxFamily>::notifyAubCapture(void *csrHandle, uint64_t gfxAddress, size_t gfxSize, bool allocate) {
    return 0;
}

template <typename GfxFamily>
int __stdcall TTCallbacks<GfxFamily>::writeL3Address(void *queueHandle, uint64_t l3GfxAddress, uint64_t regOffset) {
    return 0;
}

} // namespace NEO
