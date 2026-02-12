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
    return false;
}
template <>
bool ProductHelperHw<gfxProduct>::isCompressionFormatFromGmmRequired() const {
    return true;
}
} // namespace NEO