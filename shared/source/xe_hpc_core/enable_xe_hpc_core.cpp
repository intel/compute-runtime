/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/enable_product.h"

namespace NEO {

#ifdef SUPPORT_PVC
static EnableGfxProductHw<IGFX_PVC> enableGfxProductHwPVC;
#endif

} // namespace NEO
