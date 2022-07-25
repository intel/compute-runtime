/*
 * Copyright (C) 2022 Intel Corporation
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
        EXPECT_EQ(IGFX_SKYLAKE, defaultHwInfo->platform.eProductFamily);
    }
    void SetUp() override {
        expectCorrectPlatform();
    }
    void TearDown() override {
        expectCorrectPlatform();
    }
};

HWTEST2_F(TestMacrosIfNotMatchTearDownCall, givenNotMatchPlatformWhenUseHwTest2FThenSetUpAndTearDownAreNotCalled, IsSKL) {
    expectCorrectPlatform();
}
class TestMacrosWithParamIfNotMatchTearDownCall : public TestMacrosIfNotMatchTearDownCall, public ::testing::WithParamInterface<int> {};
HWTEST2_P(TestMacrosWithParamIfNotMatchTearDownCall, givenNotMatchPlatformWhenUseHwTest2PThenSetUpAndTearDownAreNotCalled, IsSKL) {
    expectCorrectPlatform();
}
INSTANTIATE_TEST_CASE_P(givenNotMatchPlatformWhenUseHwTest2PThenSetUpAndTearDownAreNotCalled,
                        TestMacrosWithParamIfNotMatchTearDownCall,
                        ::testing::Values(0));