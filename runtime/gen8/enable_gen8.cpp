/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gen8/hw_cmds.h"
#include "runtime/helpers/enable_product.inl"
#include "runtime/os_interface/hw_info_config.h"

namespace NEO {

#ifdef SUPPORT_BDW
static EnableGfxProductHw<IGFX_BROADWELL> enableGfxProductHwBDW;
#endif

} // namespace NEO
