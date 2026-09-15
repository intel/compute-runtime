/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/string.h"

#include <cstddef>

namespace NEO {

template <bool emitSfenceAfterCopy = true>
inline void streamCopy(void *dst, const void *src, size_t bytes) noexcept {
    memcpy_s(dst, bytes, src, bytes);
}

} // namespace NEO
