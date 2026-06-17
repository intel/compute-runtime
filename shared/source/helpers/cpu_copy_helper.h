/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_interface.h"

namespace NEO {
bool isSmallBarConfigPresent(const OSInterface *osIface);
void streamCopy(void *dst, const void *src, size_t bytes) noexcept;
} // namespace NEO
