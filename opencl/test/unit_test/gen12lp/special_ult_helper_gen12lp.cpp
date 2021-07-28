/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/gen12lp/special_ult_helper_gen12lp.h"

#include "shared/source/helpers/hw_info.h"

#include "test.h"

namespace NEO {

bool SpecialUltHelperGen12lp::additionalCoherencyCheck(PRODUCT_FAMILY productFamily, bool coherency) {
    if (productFamily == IGFX_DG1) {
        EXPECT_FALSE(coherency);
        return true;
    }
    return false;
}

bool SpecialUltHelperGen12lp::shouldPerformimagePitchAlignment(PRODUCT_FAMILY productFamily) {
    return productFamily == PRODUCT_FAMILY::IGFX_TIGERLAKE_LP || productFamily == PRODUCT_FAMILY::IGFX_DG1;
}

bool SpecialUltHelperGen12lp::shouldTestDefaultImplementationOfSetupHardwareCapabilities(PRODUCT_FAMILY productFamily) {
    return false;
}

bool SpecialUltHelperGen12lp::isPipeControlWArequired(PRODUCT_FAMILY productFamily) {
    return productFamily == IGFX_TIGERLAKE_LP || productFamily == IGFX_DG1;
}

} // namespace NEO
