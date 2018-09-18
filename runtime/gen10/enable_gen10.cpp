/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/os_interface/hw_info_config.h"
#include "runtime/helpers/enable_product.inl"

namespace OCLRT {

#ifdef SUPPORT_CNL
static EnableGfxProductHw<IGFX_CANNONLAKE> enableGfxProductHwCNL;
#endif

} // namespace OCLRT
