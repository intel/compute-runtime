/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aubstream/product_family.h"

namespace NEO {
template <>
bool ProductHelperHw<gfxProduct>::isBufferPoolAllocatorSupported() const {
    return true;
}
} // namespace NEO