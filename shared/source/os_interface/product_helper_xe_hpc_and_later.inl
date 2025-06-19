/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"

namespace NEO {

template <>
size_t ProductHelperHw<gfxProduct>::getSvmCpuAlignment() const {
    return MemoryConstants::pageSize64k;
}

} // namespace NEO