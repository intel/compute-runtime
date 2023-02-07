/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "igfxfmid.h"

namespace NEO {

template <PRODUCT_FAMILY product>
struct HwMapper {};

// Utility conversion
template <PRODUCT_FAMILY productFamily>
struct ToGfxCoreFamily {
    static const GFXCORE_FAMILY gfxCoreFamily =
        static_cast<GFXCORE_FAMILY>(NEO::HwMapper<productFamily>::gfxFamily);
    static constexpr GFXCORE_FAMILY get() { return gfxCoreFamily; }
};

template <GFXCORE_FAMILY gfxFamily>
struct GfxFamilyMapper {};

} // namespace NEO