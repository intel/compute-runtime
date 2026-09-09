/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cpu_copy_helper.h"
#include "shared/source/helpers/string.h"

namespace NEO {

void streamCopy(void *dst, const void *src, size_t bytes) noexcept {
    memcpy_s(dst, bytes, src, bytes);
}

} // namespace NEO
