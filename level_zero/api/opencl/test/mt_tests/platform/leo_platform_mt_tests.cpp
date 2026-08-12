/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace NEO {
namespace LEO {
namespace ult {

struct GetPlatformIDsMtTests : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        platforms.push_back(std::unique_ptr<Platform>(platform));
        platformsImplBackup = std::make_unique<VariableBackup<decltype(platformsImpl)>>(&platformsImpl, &platforms);
    }
    void TearDown() override {
        platformsImplBackup.reset();
        platforms.back().release();
        platforms.clear();
        Test<OclFixture>::TearDown();
    }
    std::vector<std::unique_ptr<Platform>> platforms;
    std::unique_ptr<VariableBackup<decltype(platformsImpl)>> platformsImplBackup;
};

TEST_F(GetPlatformIDsMtTests, givenPlatformsMutexTakenWhenGetPlatformIDsIsCalledOnAnotherThreadThenItWaitsForTheMutex) {
    std::atomic<bool> threadStarted{false};
    std::atomic<bool> callCompleted{false};
    cl_uint numPlatforms = 0;
    cl_int retVal = CL_INVALID_VALUE;

    std::unique_lock<std::mutex> lock(Platform::platformsMutex);

    std::thread worker([&]() {
        threadStarted.store(true);
        retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);
        callCompleted.store(true);
    });

    constexpr auto waitIterations = 50;
    for (auto i = 0; i < waitIterations && (false == threadStarted.load()); i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    for (auto i = 0; i < waitIterations && (false == callCompleted.load()); i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_TRUE(threadStarted.load());
    EXPECT_FALSE(callCompleted.load());

    lock.unlock();
    worker.join();

    EXPECT_TRUE(callCompleted.load());
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numPlatforms);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
