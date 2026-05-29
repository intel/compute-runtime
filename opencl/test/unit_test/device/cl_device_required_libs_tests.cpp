/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/program/program.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

#include <atomic>
#include <thread>

namespace NEO {

struct ClDeviceRequiredLibsTest : public ClDeviceFixture, public ::testing::Test {
    void SetUp() override {
        ClDeviceFixture::setUp();
    }

    void TearDown() override {
        ClDeviceFixture::tearDown();
    }
};

TEST_F(ClDeviceRequiredLibsTest, givenLibInRegistryWhenGetRequestedThenReturnCachedAndDoNotReload) {
    constexpr auto libName = "libCached.zebin";
    auto *fakeProgram = new MockProgram(nullptr, true, toClDeviceVector(*pClDevice));
    {
        auto lock = pClDevice->requiredLibsRegistry.lock();
        pClDevice->requiredLibsRegistry->emplace(libName, std::unique_ptr<Program>(fakeProgram));
    }

    auto *got = pClDevice->getRequiredLibProgram(libName);
    EXPECT_EQ(static_cast<Program *>(fakeProgram), got);
}

TEST_F(ClDeviceRequiredLibsTest, givenCustomSearchPathWithMissingFileWhenGettingThenReturnNullAndDoNotCache) {
    DebugManagerStateRestore restorer;
    debugManager.flags.RequiredLibsBinarySearchPath.set("/no/such/path/exists");

    constexpr auto libName = "libMissing.zebin";

    auto *got = pClDevice->getRequiredLibProgram(libName);
    EXPECT_EQ(nullptr, got);

    auto lock = pClDevice->requiredLibsRegistry.lock();
    EXPECT_FALSE(pClDevice->requiredLibsRegistry->contains(libName));
}

TEST_F(ClDeviceRequiredLibsTest, givenReentrantCreateRequiredLibProgramWhenGetRequiredLibProgramThenDoesNotSelfDeadlock) {
    struct ReentrantClDevice : MockClDevice {
        using MockClDevice::MockClDevice;
        std::atomic_bool innerCallReturned{false};
        Program *createRequiredLibProgram(const std::string &libName, cl_int &errcodeRet) override {
            if (libName == "outer.zebin") {
                (void)getRequiredLibProgram("inner.zebin");
                innerCallReturned = true;
            }
            errcodeRet = CL_INVALID_BINARY;
            return nullptr;
        }
    };

    auto device = std::make_unique<ReentrantClDevice>(
        MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    std::thread worker([&]() {
        (void)device->getRequiredLibProgram("outer.zebin");
    });
    worker.join();

    EXPECT_TRUE(device->innerCallReturned.load());
}

} // namespace NEO
