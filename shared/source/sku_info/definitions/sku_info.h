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

constexpr inline size_t bcsInfoMaskSize = 1u;
using BcsInfoMask = std::bitset<bcsInfoMaskSize>;

struct FeatureTable : FeatureTableBase {
    BcsInfoMask ftrBcsInfo = 0;
};

struct WorkaroundTable : WorkaroundTableBase {};
} // namespace NEO
