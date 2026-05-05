/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
std::optional<bool> ProductHelperHw<gfxProduct>::isCoherentAllocation(uint64_t patIndex) const {
    std::array<uint64_t, 12> listOfCoherentPatIndexes = {1, 2, 4, 5, 7, 19, 22, 23, 26, 27, 30, 31};
    if (std::find(listOfCoherentPatIndexes.begin(), listOfCoherentPatIndexes.end(), patIndex) != listOfCoherentPatIndexes.end()) {
        return true;
    }
    return false;
}

} // namespace NEO
