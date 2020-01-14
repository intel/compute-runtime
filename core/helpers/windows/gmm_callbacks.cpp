/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {
long(__stdcall *notifyAubCaptureImpl)(void *csrHandle, uint64_t gfxAddress, size_t gfxSize, bool allocate) = nullptr;
} // namespace NEO
