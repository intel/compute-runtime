/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_manager.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

namespace ULT {

struct ClAddCommentToAubTest : ApiTests {
    void SetUp() override {
        ApiTests::SetUp();
        pDevice = pContext->getDevice(0);
    }
    void TearDown() override {
        ApiTests::TearDown();
    }

    ClDevice *pDevice = nullptr;
};

TEST_F(ClAddCommentToAubTest, givenProperCommentNullptrAubCenterWhenAddCommentToAubThenSuccessIsReturned) {
    auto retVal = clAddCommentINTEL(pDevice, "comment");
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClAddCommentToAubTest, givenInvalidDeviceWhenAddCommentToAubThenErrorIsReturned) {
    auto retVal = clAddCommentINTEL(nullptr, "comment");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(ClAddCommentToAubTest, givenNullptrCommentWhenAddCommentToAubThenErrorIsReturned) {
    auto retVal = clAddCommentINTEL(pDevice, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClAddCommentToAubTest, givenAubCenterAndProperCommentButNullptrAubManagerWhenAddCommentToAubThenErrorIsReturned) {
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[testedRootDeviceIndex]->aubCenter.reset(new MockAubCenter());

    auto retVal = clAddCommentINTEL(pDevice, "comment");
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClAddCommentToAubTest, givenProperCommentAubCenterAndAubManagerWhenAddCommentToAubThenSuccessIsReturned) {
    struct AubManagerCommentMock : public MockAubManager {
        using MockAubManager::MockAubManager;
        void addComment(const char *message) override {
            addCommentCalled = true;
            EXPECT_STREQ("comment", message);
        }
        bool addCommentCalled = false;
    };
    auto mockAubCenter = new MockAubCenter();
    auto mockAubManager = new AubManagerCommentMock;
    mockAubCenter->aubManager.reset(mockAubManager);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[testedRootDeviceIndex]->aubCenter.reset(mockAubCenter);

    EXPECT_FALSE(mockAubManager->addCommentCalled);

    auto retVal = clAddCommentINTEL(pDevice, "comment");
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockAubManager->addCommentCalled);
}
} // namespace ULT
