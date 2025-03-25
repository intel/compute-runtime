/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_tgllp.h"
#include "shared/source/gen12lp/hw_info_tgllp.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper_hw.h"

constexpr static auto gfxProduct = IGFX_TIGERLAKE_LP;

#include "shared/source/gen12lp/os_agnostic_product_helper_gen12lp.inl"
#include "shared/source/gen12lp/tgllp/os_agnostic_product_helper_tgllp.inl"

namespace NEO {

template <>
void ProductHelperHw<gfxProduct>::setCapabilityCoherencyFlag(const HardwareInfo &hwInfo, bool &coherencyFlag) const {
    coherencyFlag = true;

    if (GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo, *this)) {
        // stepping A devices - turn off coherency
        coherencyFlag = false;
    }
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
