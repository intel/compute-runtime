/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/test/common/fixtures/mock_execution_environment_gmm_fixture.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
using GmmCacheSettingTests = Test<MockExecutionEnvironmentGmmFixture>;
MTLTEST_F(GmmCacheSettingTests, givenDefaultFlagsWhenCreateGmmForMtlThenFlagCachcableIsSetProperly) {
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getProductHelper();
    for (auto resourceUsageType : {GMM_RESOURCE_USAGE_OCL_IMAGE,
                                   GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER,
                                   GMM_RESOURCE_USAGE_OCL_BUFFER_CONST,
                                   GMM_RESOURCE_USAGE_OCL_BUFFER,
                                   GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC,
                                   GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED,
                                   GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED}) {
        EXPECT_EQ(!CacheSettingsHelper::isUncachedType(resourceUsageType), CacheSettingsHelper::isResourceCacheableOnCpu(resourceUsageType, productHelper, true));
    }
}
} // namespace NEO