/*
 * Copyright (C) 2018-2020 Intel Corporation
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
    BcsInfoMask ftrBcsInfo = 0;
};

struct WorkaroundTable : WorkaroundTableBase {};
} // namespace NEO
