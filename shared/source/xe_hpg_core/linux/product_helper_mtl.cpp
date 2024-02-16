/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"

constexpr static auto gfxProduct = IGFX_METEORLAKE;

#include "shared/source/xe_hpg_core/xe_lpg/linux/product_helper_xe_lpg_linux.inl"
#include "shared/source/xe_hpg_core/xe_lpg/os_agnostic_product_helper_xe_lpg.inl"
namespace NEO {
template <>
uint64_t ProductHelperHw<gfxProduct>::overridePatIndex(bool isUncachedType, uint64_t patIndex, AllocationType allocationType) const {
    switch (allocationType) {
    case NEO::AllocationType::buffer:
        return 0u;
    default:
        return 3u;
    }
}

template <>
bool ProductHelperHw<gfxProduct>::useGemCreateExtInAllocateMemoryByKMD() const {
    return true;
}

template class NEO::ProductHelperHw<gfxProduct>;

} // namespace NEO