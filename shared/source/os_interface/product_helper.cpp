/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"

namespace NEO {

ProductHelperCreateFunctionType productHelperFactory[IGFX_MAX_PRODUCT] = {};

} // namespace NEO
