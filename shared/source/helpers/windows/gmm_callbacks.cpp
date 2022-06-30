/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <cstdint>

namespace NEO {
long(__stdcall *notifyAubCaptureImpl)(void *csrHandle, uint64_t gfxAddress, size_t gfxSize, bool allocate) = nullptr;
} // namespace NEO
