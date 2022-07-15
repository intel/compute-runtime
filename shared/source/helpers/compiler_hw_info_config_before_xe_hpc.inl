/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/compiler_hw_info_config.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
bool CompilerHwInfoConfigHw<gfxProduct>::isForceToStatelessRequired() const {
    return false;
}

} // namespace NEO
