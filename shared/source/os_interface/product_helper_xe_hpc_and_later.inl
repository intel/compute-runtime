/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::supports2DBlockLoad() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::supports2DBlockStore() const {
    return true;
}

template <>
size_t ProductHelperHw<gfxProduct>::getSvmCpuAlignment() const {
    return MemoryConstants::pageSize64k;
}

} // namespace NEO
