/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/sku_info/sku_info_base.h"

namespace NEO {
struct alignas(4) FeatureTable : FeatureTableBase {};

struct alignas(4) WorkaroundTable : WorkaroundTableBase {};
} // namespace NEO
