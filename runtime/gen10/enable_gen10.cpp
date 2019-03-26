/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/enable_product.inl"
#include "runtime/os_interface/hw_info_config.h"

#include "hw_cmds.h"

namespace NEO {

#ifdef SUPPORT_CNL
static EnableGfxProductHw<IGFX_CANNONLAKE> enableGfxProductHwCNL;
#endif

} // namespace NEO
