/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"

namespace NEO {

template <>
void ProductHelperHw<gfxProduct>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) const {
}

template <>
size_t ProductHelperHw<gfxProduct>::getMaxFillPaternSizeForCopyEngine() const {
    return sizeof(uint32_t);
}

} // namespace NEO
