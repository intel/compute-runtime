/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/gen12lp/special_ult_helper_gen12lp.h"

namespace NEO {

bool SpecialUltHelperGen12lp::additionalCoherencyCheck(PRODUCT_FAMILY productFamily, bool coherency) {
    return productFamily == IGFX_DG1;
}

bool SpecialUltHelperGen12lp::isAdditionalCapabilityCoherencyFlagSettingRequired(PRODUCT_FAMILY productFamily) {
    return productFamily == IGFX_TIGERLAKE_LP;
}

bool SpecialUltHelperGen12lp::shouldPerformimagePitchAlignment(PRODUCT_FAMILY productFamily) {
    return productFamily == IGFX_TIGERLAKE_LP || productFamily == IGFX_DG1;
}

bool SpecialUltHelperGen12lp::isPipeControlWArequired(PRODUCT_FAMILY productFamily) {
    return productFamily == IGFX_TIGERLAKE_LP || productFamily == IGFX_DG1;
}

} // namespace NEO
