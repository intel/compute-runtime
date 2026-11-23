/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "neo_igfxfmid.h"

namespace NEO {
template <PRODUCT_FAMILY gfxProduct>
struct EnableGfxProductHw {
    // Explicitly instantiated in hw_info_<product>.cpp.
    EnableGfxProductHw();
};
} // namespace NEO
