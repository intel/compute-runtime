/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/sku_info/sku_info_base.h"

namespace NEO {

struct FeatureTable : FeatureTableBase {
    uint64_t asHash() const;
};

struct WorkaroundTable : WorkaroundTableBase {
    uint64_t asHash() const;
};

} // namespace NEO
