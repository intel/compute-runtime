/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gen12lp/hw_info.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/helpers/unit_test_helper.inl"

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
    if (hwInfo.platform.eProductFamily == IGFX_TIGERLAKE_LP) {
        return true;
    }
    return false;
}

template struct UnitTestHelper<Family>;
} // namespace NEO
