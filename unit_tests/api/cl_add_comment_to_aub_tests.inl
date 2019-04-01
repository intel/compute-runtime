/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
#include "unit_tests/api/cl_api_tests.h"
#include "unit_tests/mocks/mock_aub_center.h"
#include "unit_tests/mocks/mock_aub_manager.h"

using namespace NEO;

namespace ULT {

TEST(clAddCommentToAubTest, givenNullptrCommentWhenAddCommentToAubThenErrorIsReturned) {
    auto retVal = clAddCommentINTEL(nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(clAddCommentToAubTest, givenProperCommentAndNullptrAubCenterWhenAddCommentToAubThenErrorIsReturned) {
    auto retVal = clAddCommentINTEL("comment");
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(clAddCommentToAubTest, givenProperCommentAndAubCenterButNullptrAubManagerWhenAddCommentToAubThenErrorIsReturned) {
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->aubCenter.reset(new MockAubCenter());

    auto retVal = clAddCommentINTEL("comment");
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    executionEnvironment->aubCenter.reset(nullptr);
}

TEST(clAddCommentToAubTest, givenProperCommentAubCenterAndAubManagerWhenAddCommentToAubThenSuccessIsReturned) {
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
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->aubCenter.reset(mockAubCenter);

    EXPECT_FALSE(mockAubManager->addCommentCalled);

    auto retVal = clAddCommentINTEL("comment");
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockAubManager->addCommentCalled);

    executionEnvironment->aubCenter.reset(nullptr);
}
} // namespace ULT
