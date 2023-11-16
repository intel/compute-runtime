/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/release_helper/release_helper.h"

namespace NEO {
template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isDotAccumulateSupported() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isCreateBufferWithPropertiesSupported() const {
    return true;
}

} // namespace NEO
