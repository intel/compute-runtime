/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
std::optional<bool> ProductHelperHw<gfxProduct>::isCoherentAllocation(uint64_t patIndex) const {
    std::array<uint64_t, 11> listOfCoherentPatIndexes = {1, 2, 4, 5, 7, 22, 23, 26, 27, 30, 31};
    if (std::find(listOfCoherentPatIndexes.begin(), listOfCoherentPatIndexes.end(), patIndex) != listOfCoherentPatIndexes.end()) {
        return true;
    }
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getIsaPrefetchSize(uint32_t isaSize) const {
    constexpr size_t defaultLimitValue = MemoryConstants::kiloByte;

    uint32_t limitValue = debugManager.flags.LimitIsaPrefetchSize.getIfNotDefault(static_cast<uint32_t>(defaultLimitValue));
    return std::min(isaSize, limitValue);
}

} // namespace NEO
