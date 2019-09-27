/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "igfxfmid.h"

namespace NEO {
struct HardwareInfo;

struct SpecialUltHelperGen12lp {
    static bool shouldEnableHdcFlush(PRODUCT_FAMILY productFamily);
    static bool additionalCoherencyCheck(PRODUCT_FAMILY productFamily, bool coherency);
    static bool shouldPerformimagePitchAlignment(PRODUCT_FAMILY productFamily);
    static bool shouldTestDefaultImplementationOfSetupHardwareCapabilities(PRODUCT_FAMILY productFamily);
    static bool isPipeControlWArequired(PRODUCT_FAMILY productFamily);
};
} // namespace NEO
