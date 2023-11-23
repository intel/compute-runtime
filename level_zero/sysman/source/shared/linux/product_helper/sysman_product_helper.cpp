/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"

namespace L0 {
namespace Sysman {

SysmanProductHelperCreateFunctionType sysmanProductHelperFactory[IGFX_MAX_PRODUCT] = {};

}
} // namespace L0