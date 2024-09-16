/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

class TestMacrosIfNotMatchTearDownCall : public ::testing::Test {
  public:
    void expectCorrectPlatform() {
        EXPECT_EQ(IGFX_BMG, defaultHwInfo->platform.eProductFamily);
    }
    void SetUp() override {
        expectCorrectPlatform();
    }
    void TearDown() override {
        expectCorrectPlatform();
    }
};

HWTEST2_F(TestMacrosIfNotMatchTearDownCall, givenNotMatchPlatformWhenUseHwTest2FThenSetUpAndTearDownAreNotCalled, IsBMG) {
    expectCorrectPlatform();
}
class TestMacrosWithParamIfNotMatchTearDownCall : public TestMacrosIfNotMatchTearDownCall, public ::testing::WithParamInterface<int> {};
HWTEST2_P(TestMacrosWithParamIfNotMatchTearDownCall, givenNotMatchPlatformWhenUseHwTest2PThenSetUpAndTearDownAreNotCalled, IsBMG) {
    expectCorrectPlatform();
}
INSTANTIATE_TEST_SUITE_P(givenNotMatchPlatformWhenUseHwTest2PThenSetUpAndTearDownAreNotCalled,
                         TestMacrosWithParamIfNotMatchTearDownCall,
                         ::testing::Values(0));
