/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/platform/platform.h"
#include "unit_tests/api/cl_api_tests.h"
#include "unit_tests/mocks/mock_aub_center.h"
#include "unit_tests/mocks/mock_aub_manager.h"

using namespace NEO;

namespace ULT {

using clAddCommentToAubTest = api_tests;

TEST_F(clAddCommentToAubTest, givenProperCommentNullptrAubCenterWhenAddCommentToAubThenSuccessIsReturned) {
    auto retVal = clAddCommentINTEL(pPlatform, "comment");
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clAddCommentToAubTest, givenNullptrCommentWhenAddCommentToAubThenErrorIsReturned) {
    auto retVal = clAddCommentINTEL(pPlatform, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clAddCommentToAubTest, givenAubCenterAndProperCommentButNullptrAubManagerWhenAddCommentToAubThenErrorIsReturned) {
    pPlatform->peekExecutionEnvironment()->aubCenter.reset(new MockAubCenter());

    auto retVal = clAddCommentINTEL(pPlatform, "comment");
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
    pPlatform->peekExecutionEnvironment()->aubCenter.reset(mockAubCenter);

    EXPECT_FALSE(mockAubManager->addCommentCalled);

    auto retVal = clAddCommentINTEL(pPlatform, "comment");
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockAubManager->addCommentCalled);
}
} // namespace ULT
