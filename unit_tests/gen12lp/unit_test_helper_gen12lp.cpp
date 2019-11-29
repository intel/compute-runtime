/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen12lp/hw_info.h"
#include "unit_tests/gen12lp/special_ult_helper_gen12lp.h"
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
    return SpecialUltHelperGen12lp::isPageTableManagerSupported(hwInfo);
}

template <>
bool UnitTestHelper<Family>::isPipeControlWArequired(const HardwareInfo &hwInfo) {
    return SpecialUltHelperGen12lp::isPipeControlWArequired(hwInfo.platform.eProductFamily);
}

template struct UnitTestHelper<Family>;
} // namespace NEO
