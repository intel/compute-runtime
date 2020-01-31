/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen11/hw_cmds.h"
#include "core/os_interface/hw_info_config.h"
#include "runtime/helpers/enable_product.inl"

namespace NEO {

#ifdef SUPPORT_ICLLP
static EnableGfxProductHw<IGFX_ICELAKE_LP> enableGfxProductHwICLLP;
#endif
#ifdef SUPPORT_LKF
static EnableGfxProductHw<IGFX_LAKEFIELD> enableGfxProductHwLKF;
#endif
#ifdef SUPPORT_EHL
static EnableGfxProductHw<IGFX_ELKHARTLAKE> enableGfxProductHwEHL;
#endif

} // namespace NEO
