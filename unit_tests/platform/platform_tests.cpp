/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/options.h"
#include "runtime/device/device.h"
#include "runtime/sharings/sharing_factory.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/mocks/mock_async_event_handler.h"
#include "unit_tests/mocks/mock_csr.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using namespace OCLRT;

struct PlatformTest : public MemoryManagementFixture, public ::testing::Test {
    PlatformTest() {}

    void SetUp() override { pPlatform = platform(); }

    void TearDown() override {}

    cl_int retVal = CL_SUCCESS;
    Platform *pPlatform = nullptr;
};

TEST_F(PlatformTest, getDevices) {
    size_t devNum = pPlatform->getNumDevices();
    EXPECT_EQ(0u, devNum);

    Device *device = pPlatform->getDevice(0);
    EXPECT_EQ(nullptr, device);

    bool ret = pPlatform->initialize(numPlatformDevices, platformDevices);
    EXPECT_TRUE(ret);

    EXPECT_TRUE(pPlatform->isInitialized());

    devNum = pPlatform->getNumDevices();
    EXPECT_EQ(numPlatformDevices, devNum);

    device = pPlatform->getDevice(0);
    EXPECT_NE(nullptr, device);

    device = pPlatform->getDevice(numPlatformDevices);
    EXPECT_EQ(nullptr, device);

    auto allDevices = pPlatform->getDevices();
    EXPECT_NE(nullptr, allDevices);

    pPlatform->shutdown();
    devNum = pPlatform->getNumDevices();
    EXPECT_EQ(0u, devNum);

    allDevices = pPlatform->getDevices();
    EXPECT_EQ(nullptr, allDevices);
}

TEST_F(PlatformTest, PlatformGetCompilerExtensions) {
    std::string compilerExtensions = pPlatform->getCompilerExtensions();
    EXPECT_EQ(std::string(""), compilerExtensions);

    pPlatform->initialize(numPlatformDevices, platformDevices);
    compilerExtensions = pPlatform->getCompilerExtensions();

    EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("-cl-ext=+cl")));
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
        EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("cl_khr_subgroups")));
    }

    pPlatform->shutdown();
    compilerExtensions.shrink_to_fit();
}

TEST_F(PlatformTest, destructorCallsShutdownAndReleasesAllResources) {
    Platform *platform = new Platform;
    ASSERT_NE(nullptr, platform);
    platform->initialize(numPlatformDevices, platformDevices);
    delete platform;
}

TEST_F(PlatformTest, hasAsyncEventsHandler) {
    EXPECT_NE(nullptr, pPlatform->getAsyncEventsHandler());
}

TEST(PlatformTestSimple, shutdownClosesAsyncEventHandlerThread) {
    Platform *platform = new Platform;

    MockHandler *mockAsyncHandler = new MockHandler;

    platform->createAsyncEventsHandler(mockAsyncHandler);
    EXPECT_EQ(mockAsyncHandler, platform->getAsyncEventsHandler());

    mockAsyncHandler->openThread();

    platform->shutdown();
    EXPECT_EQ(nullptr, mockAsyncHandler->thread);
    delete platform;
}

namespace OCLRT {
extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
}

CommandStreamReceiver *createMockCommandStreamReceiver(const HardwareInfo &hwInfoIn) {
    return nullptr;
};

class PlatformFailingTest : public PlatformTest {
  public:
    void SetUp() override {
        PlatformTest::SetUp();
        hwInfo = platformDevices[0];
        commandStreamReceiverCreateFunc = commandStreamReceiverFactory[hwInfo->pPlatform->eRenderCoreFamily];
        commandStreamReceiverFactory[hwInfo->pPlatform->eRenderCoreFamily] = createMockCommandStreamReceiver;
        overrideCommandStreamReceiverCreation = true;
    }

    void TearDown() override {
        commandStreamReceiverFactory[hwInfo->pPlatform->eRenderCoreFamily] = commandStreamReceiverCreateFunc;
        PlatformTest::TearDown();
    }

    CommandStreamReceiverCreateFunc commandStreamReceiverCreateFunc;
    const HardwareInfo *hwInfo;
};

TEST_F(PlatformFailingTest, givenPlatformInitializationWhenIncorrectHwInfoThenInitializationFails) {
    Platform *platform = new Platform;
    bool ret = platform->initialize(numPlatformDevices, platformDevices);
    EXPECT_FALSE(ret);
    EXPECT_FALSE(platform->isInitialized());
    delete platform;
}
