/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

using AILTestsIcllp = ::testing::Test;

HWTEST2_F(AILTestsIcllp, whenKernelSourceIsANGenDummyKernelThenDoEnforcePatchtokensFormat, IsICLLP) {
    std::string dummyKernelSource{"kernel void _(){}"};
    bool enforceRebuildToCTNI = false;

    AILConfigurationHw<IGFX_ICELAKE_LP> ail;
    ail.forceFallbackToPatchtokensIfRequired(dummyKernelSource, enforceRebuildToCTNI);

    EXPECT_TRUE(enforceRebuildToCTNI);
}

HWTEST2_F(AILTestsIcllp, whenKernelSourceIsNotANGenDummyKernelThenDoNotEnforcePatchtokensFormat, IsICLLP) {
    std::string dummyKernelSource{"kernel void copybuffer(__global int* a, __global int* b){ //some code }"};
    bool enforceRebuildToCTNI = false;

    AILConfigurationHw<IGFX_ICELAKE_LP> ail;
    ail.forceFallbackToPatchtokensIfRequired(dummyKernelSource, enforceRebuildToCTNI);

    EXPECT_FALSE(enforceRebuildToCTNI);
}

} // namespace NEO