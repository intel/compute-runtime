/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/os_interface/hw_info_config.h"

#include "opencl/source/helpers/enable_product.inl"

namespace NEO {

#ifdef SUPPORT_TGLLP
static EnableGfxProductHw<IGFX_TIGERLAKE_LP> enableGfxProductHwTGLLP;
#endif

} // namespace NEO
