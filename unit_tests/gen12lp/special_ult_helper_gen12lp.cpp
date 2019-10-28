/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/gen12lp/special_ult_helper_gen12lp.h"

#include "runtime/helpers/hw_info.h"

namespace NEO {

bool SpecialUltHelperGen12lp::shouldCompressionBeEnabledAfterConfigureHardwareCustom(const HardwareInfo &hwInfo) {
    return hwInfo.featureTable.ftrE2ECompression;
}

bool SpecialUltHelperGen12lp::shouldEnableHdcFlush(PRODUCT_FAMILY productFamily) {
    return true;
}

bool SpecialUltHelperGen12lp::additionalCoherencyCheck(PRODUCT_FAMILY productFamily, bool coherency) {
    return false;
}

bool SpecialUltHelperGen12lp::shouldPerformimagePitchAlignment(PRODUCT_FAMILY productFamily) {
    return true;
}

bool SpecialUltHelperGen12lp::shouldTestDefaultImplementationOfSetupHardwareCapabilities(PRODUCT_FAMILY productFamily) {
    return true;
}

bool SpecialUltHelperGen12lp::isPipeControlWArequired(PRODUCT_FAMILY productFamily) {
    return true;
}

bool SpecialUltHelperGen12lp::isPageTableManagerSupported(const HardwareInfo &hwInfo) {
    return hwInfo.capabilityTable.ftrRenderCompressedBuffers || hwInfo.capabilityTable.ftrRenderCompressedImages;
}

bool SpecialUltHelperGen12lp::isRenderBufferCompressionPreferred(const HardwareInfo &hwInfo, const std::size_t size) {
    return false;
}

bool SpecialUltHelperGen12lp::isAdditionalSurfaceStateParamForCompressionRequired(const HardwareInfo &hwInfo) {
    return false;
}

} // namespace NEO
