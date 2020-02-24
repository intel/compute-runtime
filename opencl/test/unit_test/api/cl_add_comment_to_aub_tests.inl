/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"

#include "opencl/source/context/context.h"
#include "opencl/source/device/cl_device.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "opencl/test/unit_test/mocks/mock_aub_center.h"
#include "opencl/test/unit_test/mocks/mock_aub_manager.h"

using namespace NEO;

namespace ULT {

struct clAddCommentToAubTest : api_tests {
    void SetUp() override {
        api_tests::SetUp();
        pDevice = pContext->getDevice(0);
    }
    void TearDown() override {
        api_tests::TearDown();
    }

    ClDevice *pDevice = nullptr;
};

TEST_F(clAddCommentToAubTest, givenProperCommentNullptrAubCenterWhenAddCommentToAubThenSuccessIsReturned) {
    auto retVal = clAddCommentINTEL(pDevice, "comment");
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clAddCommentToAubTest, givenInvalidDeviceWhenAddCommentToAubThenErrorIsReturned) {
    auto retVal = clAddCommentINTEL(nullptr, "comment");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clAddCommentToAubTest, givenNullptrCommentWhenAddCommentToAubThenErrorIsReturned) {
    auto retVal = clAddCommentINTEL(pDevice, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clAddCommentToAubTest, givenAubCenterAndProperCommentButNullptrAubManagerWhenAddCommentToAubThenErrorIsReturned) {
    pPlatform->peekExecutionEnvironment()->rootDeviceEnvironments[testedRootDeviceIndex]->aubCenter.reset(new MockAubCenter());

    auto retVal = clAddCommentINTEL(pDevice, "comment");
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clAddCommentToAubTest, givenProperCommentAubCenterAndAubManagerWhenAddCommentToAubThenSuccessIsReturned) {
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
    pPlatform->peekExecutionEnvironment()->rootDeviceEnvironments[testedRootDeviceIndex]->aubCenter.reset(mockAubCenter);

    EXPECT_FALSE(mockAubManager->addCommentCalled);

    auto retVal = clAddCommentINTEL(pDevice, "comment");
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockAubManager->addCommentCalled);
}
} // namespace ULT
