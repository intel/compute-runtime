/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "neo_igfxfmid.h"

namespace NEO {

struct SpecialUltHelperGen12lp {
    static bool additionalCoherencyCheck(PRODUCT_FAMILY productFamily, bool coherency);
    static bool isAdditionalCapabilityCoherencyFlagSettingRequired(PRODUCT_FAMILY productFamily);
    static bool shouldPerformimagePitchAlignment(PRODUCT_FAMILY productFamily);
    static bool isPipeControlWArequired(PRODUCT_FAMILY productFamily);
};

} // namespace NEO
