/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/enable_product.inl"
#include "runtime/os_interface/hw_info_config.h"

#include "hw_cmds.h"

namespace NEO {

#ifdef SUPPORT_ICLLP
static EnableGfxProductHw<IGFX_ICELAKE_LP> enableGfxProductHwICLLP;
#endif
#ifdef SUPPORT_LKF
static EnableGfxProductHw<IGFX_LAKEFIELD> enableGfxProductHwLKF;
#endif

} // namespace NEO
