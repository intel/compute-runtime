/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/unit_test_helper.inl"

#include "opencl/test/unit_test/gen12lp/special_ult_helper_gen12lp.h"

namespace NEO {

using Family = TGLLPFamily;

template <>
bool UnitTestHelper<Family>::isL3ConfigProgrammable() {
    return false;
};

template <>
bool UnitTestHelper<Family>::isPageTableManagerSupported(const HardwareInfo &hwInfo) {
    return hwInfo.capabilityTable.ftrRenderCompressedBuffers || hwInfo.capabilityTable.ftrRenderCompressedImages;
}

template <>
bool UnitTestHelper<Family>::isPipeControlWArequired(const HardwareInfo &hwInfo) {
    return SpecialUltHelperGen12lp::isPipeControlWArequired(hwInfo.platform.eProductFamily);
}

template struct UnitTestHelper<Family>;
} // namespace NEO
