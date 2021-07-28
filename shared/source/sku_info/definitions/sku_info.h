/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/sku_info/sku_info_base.h"

#include <bitset>

namespace NEO {

using BcsInfoMask = std::bitset<1>;

struct FeatureTable : FeatureTableBase {
    BcsInfoMask ftrBcsInfo = 1;
};

struct WorkaroundTable : WorkaroundTableBase {};
} // namespace NEO
