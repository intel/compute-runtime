/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "igfxfmid.h"

#include <cstddef>

namespace NEO {
struct HardwareInfo;

struct SpecialUltHelperGen12lp {
    static bool shouldCompressionBeEnabledAfterConfigureHardwareCustom(const HardwareInfo &hwInfo);
    static bool shouldEnableHdcFlush(PRODUCT_FAMILY productFamily);
    static bool additionalCoherencyCheck(PRODUCT_FAMILY productFamily, bool coherency);
    static bool shouldPerformimagePitchAlignment(PRODUCT_FAMILY productFamily);
    static bool shouldTestDefaultImplementationOfSetupHardwareCapabilities(PRODUCT_FAMILY productFamily);
    static bool isPipeControlWArequired(PRODUCT_FAMILY productFamily);
    static bool isPageTableManagerSupported(const HardwareInfo &hwInfo);
    static bool isRenderBufferCompressionPreferred(const HardwareInfo &hwInfo, const std::size_t size);
    static bool isAdditionalSurfaceStateParamForCompressionRequired(const HardwareInfo &hwInfo);
};
} // namespace NEO
