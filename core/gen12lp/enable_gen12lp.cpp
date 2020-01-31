/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen12lp/hw_cmds.h"
#include "core/os_interface/hw_info_config.h"
#include "runtime/helpers/enable_product.inl"

namespace NEO {

#ifdef SUPPORT_TGLLP
static EnableGfxProductHw<IGFX_TIGERLAKE_LP> enableGfxProductHwTGLLP;
#endif

} // namespace NEO
