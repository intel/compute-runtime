/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_interface.h"

#if defined(__aarch64__) || defined(_M_ARM64)
#include "shared/source/helpers/aarch64/stream_copy.h"
#else
#include "shared/source/helpers/x86_64/stream_copy.h"
#endif

namespace NEO {
bool isSmallBarConfigPresent(const OSInterface *osIface);
} // namespace NEO
