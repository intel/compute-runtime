/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/cache_policy_from_xe_hpg_to_xe3.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/product_helper.h"

#include "hw_cmds_xe3_core.h"

namespace NEO {
#include "enable_xe3_core_products.inl"
} // namespace NEO
