/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

using AILTestsTgllp = ::testing::Test;

HWTEST2_F(AILTestsTgllp, whenKernelSourceIsANGenDummyKernelThenDoEnforcePatchtokensFormat, IsTGLLP) {
    std::string dummyKernelSource{"kernel void _(){}"};
    bool enforceRebuildToCTNI = false;

    AILConfigurationHw<IGFX_TIGERLAKE_LP> ail;
    ail.forceFallbackToPatchtokensIfRequired(dummyKernelSource, enforceRebuildToCTNI);

    EXPECT_TRUE(enforceRebuildToCTNI);
}

HWTEST2_F(AILTestsTgllp, whenKernelSourceIsNotANGenDummyKernelThenDoNotEnforcePatchtokensFormat, IsTGLLP) {
    std::string dummyKernelSource{"kernel void copybuffer(__global int* a, __global int* b){ //some code }"};
    bool enforceRebuildToCTNI = false;

    AILConfigurationHw<IGFX_TIGERLAKE_LP> ail;
    ail.forceFallbackToPatchtokensIfRequired(dummyKernelSource, enforceRebuildToCTNI);

    EXPECT_FALSE(enforceRebuildToCTNI);
}

} // namespace NEO