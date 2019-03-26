/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/options.h"
#include "unit_tests/fixtures/platform_fixture.h"

#include "gtest/gtest.h"

using namespace NEO;

struct PlatformTestMt : public ::testing::Test {
    void SetUp() override { pPlatform.reset(new Platform); }

    static void initThreadFunc(Platform *pP) {
        pP->initialize();
    }

    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<Platform> pPlatform;
};

static void callinitPlatform(Platform *plt, bool *ret) {
    *ret = plt->initialize();
}

TEST_F(PlatformTestMt, initialize) {
    std::thread threads[10];
    bool ret[10];
    for (int i = 0; i < 10; ++i) {
        threads[i] = std::thread(callinitPlatform, pPlatform.get(), &ret[i]);
    }

    for (auto &th : threads)
        th.join();

    for (int i = 0; i < 10; ++i)
        EXPECT_TRUE(ret[i]);

    EXPECT_TRUE(pPlatform->isInitialized());

    pPlatform.reset(new Platform());

    for (int i = 0; i < 10; ++i) {
        threads[i] = std::thread(callinitPlatform, pPlatform.get(), &ret[i]);
    }

    for (auto &th : threads)
        th.join();

    for (int i = 0; i < 10; ++i)
        EXPECT_TRUE(ret[i]);
}

TEST_F(PlatformTestMt, mtSafeTest) {
    size_t devNum = pPlatform->getNumDevices();
    EXPECT_EQ(0u, devNum);

    std::thread t1(PlatformTestMt::initThreadFunc, pPlatform.get());
    std::thread t2(PlatformTestMt::initThreadFunc, pPlatform.get());

    t1.join();
    t2.join();

    devNum = pPlatform->getNumDevices();
    EXPECT_EQ(numPlatformDevices, devNum);
}
