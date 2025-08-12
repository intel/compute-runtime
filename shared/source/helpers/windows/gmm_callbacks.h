/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include "neo_igfxfmid.h"

#include <cstdint>

namespace NEO {
using NotifyAubCaptureFunc = long(__stdcall *)(void *csrHandle, uint64_t gfxAddress, size_t gfxSize, bool allocate);
using WriteL3AddressFunc = int(__stdcall *)(void *queueHandle, uint64_t l3GfxAddress, uint64_t regOffset);

extern NotifyAubCaptureFunc notifyAubCaptureFuncFactory[IGFX_MAX_CORE];
extern WriteL3AddressFunc writeL3AddressFuncFactory[IGFX_MAX_CORE];

template <typename GfxFamily>
struct DeviceCallbacks {
    static long __stdcall notifyAubCapture(void *csrHandle, uint64_t gfxAddress, size_t gfxSize, bool allocate);
};
template <typename GfxFamily>
struct TTCallbacks {
    static int __stdcall writeL3Address(void *queueHandle, uint64_t l3GfxAddress, uint64_t regOffset);
};

template <typename GfxFamily>
struct GmmCallbacksFactory {
    GmmCallbacksFactory() noexcept;
};

} // namespace NEO
