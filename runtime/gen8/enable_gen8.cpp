/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/os_interface/hw_info_config.h"
#include "runtime/helpers/enable_product.inl"

namespace OCLRT {

#ifdef SUPPORT_BDW
static EnableGfxProductHw<IGFX_BROADWELL> enableGfxProductHwBDW;
#endif

} // namespace OCLRT