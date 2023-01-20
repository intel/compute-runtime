/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
struct TemplatedFixtureTests : public ::testing::Test {
    void SetUp() override {
        baseSetUpCallId = callsOrder++;
    }

    void TearDown() override {
        EXPECT_EQ(idForBaseTearDown, callsOrder);
        EXPECT_EQ((idForBaseTearDown - 1), templateBaseTearDownCallId);
    }

    template <typename T>
    void setUpT() {
        templateBaseSetUpCallId = callsOrder++;
    }

    template <typename T>
    void tearDownT() {
        templateBaseTearDownCallId = callsOrder++;
    }

    uint32_t callsOrder = 0;
    uint32_t baseSetUpCallId = -1;
    uint32_t templateBaseSetUpCallId = -1;
    uint32_t templateBaseTearDownCallId = -1;

    uint32_t idForBaseTearDown = -1;
};

HWTEST_TEMPLATED_F(TemplatedFixtureTests, whenExecutingTemplatedTestThenCallTemplatedSetupAndTeardown) {
    EXPECT_EQ(2u, callsOrder);

    EXPECT_EQ(0u, baseSetUpCallId);
    EXPECT_EQ(1u, templateBaseSetUpCallId);

    idForBaseTearDown = callsOrder + 1;
}

struct DerivedTemplatedFixtureTests : public TemplatedFixtureTests {
    template <typename T>
    void setUpT() {
        TemplatedFixtureTests::setUpT<T>();
        templateDerivedSetUpCallId = callsOrder++;
    }

    template <typename T>
    void tearDownT() {
        templateDerivedTearDownCallId = callsOrder++;
        TemplatedFixtureTests::tearDownT<T>();
    }

    uint32_t templateDerivedSetUpCallId = -1;
    uint32_t templateDerivedTearDownCallId = -1;
};

HWTEST_TEMPLATED_F(DerivedTemplatedFixtureTests, whenExecutingTemplatedTestThenCallTemplatedSetupAndTeardown) {
    EXPECT_EQ(3u, callsOrder);

    EXPECT_EQ(0u, baseSetUpCallId);
    EXPECT_EQ(1u, templateBaseSetUpCallId);
    EXPECT_EQ(2u, templateDerivedSetUpCallId);

    idForBaseTearDown = callsOrder + 2;
}

struct TemplatedFixtureBaseTests : public ::testing::Test {
    template <typename T>
    void setUpT() {
        capturedPipeControlWaRequiredInSetUp = MemorySynchronizationCommands<T>::isBarrierWaRequired(*defaultHwInfo);
    }

    template <typename T>
    void tearDownT() {}

    bool capturedPipeControlWaRequiredInSetUp = false;
};

HWTEST_TEMPLATED_F(TemplatedFixtureBaseTests, whenExecutingTemplatedSetupThenTemplateTargetsCorrectPlatform) {
    bool capturedPipeControlWaRequiredInTestBody = MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(*defaultHwInfo);

    EXPECT_EQ(capturedPipeControlWaRequiredInTestBody, capturedPipeControlWaRequiredInSetUp);
}
} // namespace NEO
