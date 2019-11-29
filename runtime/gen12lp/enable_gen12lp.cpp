/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen12lp/hw_cmds.h"
#include "runtime/helpers/enable_product.inl"
#include "runtime/os_interface/hw_info_config.h"

namespace NEO {

#ifdef SUPPORT_TGLLP
static EnableGfxProductHw<IGFX_TIGERLAKE_LP> enableGfxProductHwTGLLP;
#endif

} // namespace NEO
