/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"

#include "gtest/gtest.h"

using namespace NEO;

class GmmResourceInfoDrmOrWddmTest : public ::testing::Test {
  public:
    void SetUp() override {
        hwInfo = *defaultHwInfo;
        executionEnvironment = std::make_unique<MockExecutionEnvironment>(&hwInfo);
        gmmClientContext = std::make_unique<MockGmmClientContext>();
        gmmClientContext->initialize(*executionEnvironment->rootDeviceEnvironments[0]);

        GMM_RESCREATE_PARAMS createParams = {};
        createParams.Type = RESOURCE_BUFFER;
        createParams.Format = GMM_FORMAT_R8G8B8A8_SINT;
        createParams.Usage = GMM_RESOURCE_USAGE_OCL_BUFFER;
        createParams.BaseWidth = 1024;
        createParams.BaseHeight = 1;
        createParams.Depth = 1;

        gmmResourceInfo.reset(GmmResourceInfo::create(gmmClientContext.get(), &createParams));
    }

    HardwareInfo hwInfo;
    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MockGmmClientContext> gmmClientContext;
    std::unique_ptr<GmmResourceInfo> gmmResourceInfo;
};

TEST_F(GmmResourceInfoDrmOrWddmTest, givenDrmOrWddmImplementationWhenIsResourceDenyCompressionEnabledIsCalledThenReturnsFalse) {
    EXPECT_FALSE(gmmResourceInfo->isResourceDenyCompressionEnabled());
}

TEST_F(GmmResourceInfoDrmOrWddmTest, givenDrmOrWddmImplementationWhenIsDisplayableIsCalledThenReturnsFalse) {
    EXPECT_FALSE(gmmResourceInfo->isDisplayable());
}
