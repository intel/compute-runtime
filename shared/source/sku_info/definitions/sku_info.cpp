/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sku_info.h"

#include "shared/source/helpers/hash.h"

namespace NEO {

uint64_t FeatureTable::asHash() const {
    Hash hash;

    hash.update(reinterpret_cast<const char *>(&packed), sizeof(packed));

    return hash.finish();
}

uint64_t WorkaroundTable::asHash() const {
    Hash hash;

    hash.update(reinterpret_cast<const char *>(&packed), sizeof(packed));

    return hash.finish();
}

} // namespace NEO