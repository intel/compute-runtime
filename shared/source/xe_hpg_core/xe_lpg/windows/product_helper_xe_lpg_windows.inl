/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::isStagingBuffersEnabled() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isCompressionForbidden(const HardwareInfo &hwInfo) const {
    return isCompressionForbiddenCommon(false);
}

} // namespace NEO
