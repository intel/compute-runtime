/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "neo_igfxfmid.h"

#include <cstdint>
#include <stddef.h>

#ifndef _WIN32
#define __stdcall
#endif

namespace NEO {
using NotifyAubCaptureFunc = long(__stdcall *)(void *csrHandle, uint64_t gfxAddress, size_t gfxSize, bool allocate);
using WriteL3AddressFunc = int(__stdcall *)(void *queueHandle, uint64_t l3GfxAddress, uint64_t regOffset);

extern NotifyAubCaptureFunc notifyAubCaptureFuncFactory[NEO::maxCoreEnumValue];
extern WriteL3AddressFunc writeL3AddressFuncFactory[NEO::maxCoreEnumValue];

template <typename GfxFamily>
struct GmmCallbacks {
    static long __stdcall notifyAubCapture(void *csrHandle, uint64_t gfxAddress, size_t gfxSize, bool allocate);
    static int __stdcall writeL3Address(void *queueHandle, uint64_t l3GfxAddress, uint64_t regOffset);
};

} // namespace NEO
