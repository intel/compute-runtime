/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"

namespace NEO {
template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isSpirSupported() const {
    return true;
}

} // namespace NEO
