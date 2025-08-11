/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"

#include "gtest/gtest.h"

using namespace NEO;

class GmmResourceInfoDrmTest : public ::testing::Test {
  public:
    void SetUp() override {
        hwInfo = *defaultHwInfo;
        executionEnvironment = std::make_unique<MockExecutionEnvironment>(&hwInfo);
        gmmClientContext = std::make_unique<NEO::MockGmmClientContext>(*executionEnvironment->rootDeviceEnvironments[0]);

        GMM_RESCREATE_PARAMS createParams = {};
        createParams.Type = RESOURCE_BUFFER;
        createParams.Format = GMM_FORMAT_R8G8B8A8_SINT;
        createParams.Usage = GMM_RESOURCE_USAGE_OCL_BUFFER;
        createParams.BaseWidth = 1024;
        createParams.BaseHeight = 1;
        createParams.Depth = 1;

        gmmResourceInfo = std::unique_ptr<GmmResourceInfo>(GmmResourceInfo::create(gmmClientContext.get(), &createParams));
    }

    HardwareInfo hwInfo;
    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<NEO::MockGmmClientContext> gmmClientContext;
    std::unique_ptr<GmmResourceInfo> gmmResourceInfo;
};

TEST_F(GmmResourceInfoDrmTest, givenDrmImplementationWhenGetDriverProtectionBitsIsCalledThenReturnsZero) {
    // Test DRM implementation which always returns 0
    uint64_t result = gmmResourceInfo->getDriverProtectionBits(GMM_RESOURCE_USAGE_OCL_BUFFER, false);
    EXPECT_EQ(0u, result);
}

TEST_F(GmmResourceInfoDrmTest, givenDrmImplementationWhenGetDriverProtectionBitsIsCalledWithCompressionDeniedThenReturnsZero) {
    // Test DRM implementation with compression denied - should still return 0
    uint64_t result = gmmResourceInfo->getDriverProtectionBits(GMM_RESOURCE_USAGE_OCL_BUFFER, true);
    EXPECT_EQ(0u, result);
}

TEST_F(GmmResourceInfoDrmTest, givenDrmImplementationWhenGetDriverProtectionBitsIsCalledWithDifferentUsageTypeThenReturnsZero) {
    // Test DRM implementation with different usage type - should still return 0
    uint64_t result = gmmResourceInfo->getDriverProtectionBits(GMM_RESOURCE_USAGE_RENDER_TARGET, false);
    EXPECT_EQ(0u, result);
}
