/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hash.h"
#include "shared/source/sku_info/sku_info_base.h"

namespace NEO {

struct FeatureTable : FeatureTableBase {
    BcsInfoMask ftrBcsInfo = 0;

    uint64_t asHash() const {
        Hash hash;

        hash.update(reinterpret_cast<const char *>(&packed), sizeof(packed));

        return hash.finish();
    }
};

struct WorkaroundTable : WorkaroundTableBase {
    uint64_t asHash() const {
        Hash hash;

        hash.update(reinterpret_cast<const char *>(&packed), sizeof(packed));

        return hash.finish();
    }
};
} // namespace NEO
