/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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
#include "runtime/platform/extensions.h"
#include "runtime/sharings/sharing_factory.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/mocks/mock_async_event_handler.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/libult/create_command_stream.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using namespace OCLRT;

struct PlatformTest : public ::testing::Test {
    void SetUp() override { pPlatform.reset(new Platform()); }
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<Platform> pPlatform;
};

TEST_F(PlatformTest, getDevices) {
    size_t devNum = pPlatform->getNumDevices();
    EXPECT_EQ(0u, devNum);

    Device *device = pPlatform->getDevice(0);
    EXPECT_EQ(nullptr, device);

    bool ret = pPlatform->initialize();
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
}

TEST_F(PlatformTest, PlatformgetAsCompilerEnabledExtensionsString) {
    std::string compilerExtensions = pPlatform->peekCompilerExtensions();
    EXPECT_EQ(std::string(""), compilerExtensions);

    pPlatform->initialize();
    compilerExtensions = pPlatform->peekCompilerExtensions();

    EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string(" -cl-ext=-all,+cl")));
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
        EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("cl_khr_subgroups")));
    }
}

TEST_F(PlatformTest, hasAsyncEventsHandler) {
    EXPECT_NE(nullptr, pPlatform->getAsyncEventsHandler());
}

TEST(PlatformTestSimple, shutdownClosesAsyncEventHandlerThread) {
    Platform *platform = new Platform;

    MockHandler *mockAsyncHandler = new MockHandler;

    auto oldHandler = platform->setAsyncEventsHandler(std::unique_ptr<AsyncEventsHandler>(mockAsyncHandler));
    EXPECT_EQ(mockAsyncHandler, platform->getAsyncEventsHandler());

    mockAsyncHandler->openThread();
    delete platform;
    EXPECT_TRUE(MockAsyncEventHandlerGlobals::destructorCalled);
}

namespace OCLRT {
extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
}

CommandStreamReceiver *createMockCommandStreamReceiver(const HardwareInfo &hwInfoIn, bool withAubDump, ExecutionEnvironment &executionEnvironment) {
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
    bool ret = platform->initialize();
    EXPECT_FALSE(ret);
    EXPECT_FALSE(platform->isInitialized());
    delete platform;
}

TEST_F(PlatformTest, givenSupportingCl21WhenPlatformSupportsFp64ThenFillMatchingSubstringsAndMandatoryTrailingSpace) {
    const HardwareInfo *hwInfo;
    hwInfo = platformDevices[0];
    std::string extensionsList = getExtensionsList(*hwInfo);
    EXPECT_THAT(extensionsList, ::testing::HasSubstr(std::string("cl_khr_3d_image_writes")));

    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str());
    EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string(" -cl-ext=-all,+cl")));

    if (hwInfo->capabilityTable.clVersionSupport > 20) {
        EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("cl_khr_subgroups")));
        EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("cl_khr_il_program")));
    }

    if (hwInfo->capabilityTable.ftrSupportsFP64) {
        EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("cl_khr_fp64")));
    }

    EXPECT_THAT(compilerExtensions, ::testing::EndsWith(std::string(" ")));
}

TEST_F(PlatformTest, givenNotSupportingCl21WhenPlatformNotSupportFp64ThenNotFillMatchingSubstringAndFillMandatoryTrailingSpace) {
    HardwareInfo TesthwInfo = *platformDevices[0];
    TesthwInfo.capabilityTable.ftrSupportsFP64 = false;
    TesthwInfo.capabilityTable.clVersionSupport = 10;

    std::string extensionsList = getExtensionsList(TesthwInfo);
    EXPECT_THAT(extensionsList, ::testing::HasSubstr(std::string("cl_khr_3d_image_writes")));

    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str());
    EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("-cl-ext=-all,+cl")));

    EXPECT_THAT(compilerExtensions, ::testing::Not(::testing::HasSubstr(std::string("cl_khr_fp64"))));
    EXPECT_THAT(compilerExtensions, ::testing::Not(::testing::HasSubstr(std::string("cl_khr_subgroups"))));

    EXPECT_THAT(compilerExtensions, ::testing::EndsWith(std::string(" ")));
}

TEST_F(PlatformTest, testRemoveLastSpace) {
    std::string emptyString = "";
    removeLastSpace(emptyString);
    EXPECT_EQ(std::string(""), emptyString);

    std::string xString = "x";
    removeLastSpace(xString);
    EXPECT_EQ(std::string("x"), xString);

    std::string xSpaceString = "x ";
    removeLastSpace(xSpaceString);
    EXPECT_EQ(std::string("x"), xSpaceString);
}
TEST(PlatformConstructionTest, givenPlatformConstructorWhenItIsCalledTwiceThenTheSamePlatformIsReturned) {
    platformImpl.reset();
    ASSERT_EQ(nullptr, platformImpl);
    auto platform = constructPlatform();
    EXPECT_EQ(platform, platformImpl.get());
    auto platform2 = constructPlatform();
    EXPECT_EQ(platform2, platform);
    EXPECT_NE(platform, nullptr);
}

TEST(PlatformConstructionTest, givenPlatformConstructorWhenItIsCalledAfterResetThenNewPlatformIsConstructed) {
    platformImpl.reset();
    ASSERT_EQ(nullptr, platformImpl);
    auto platform = constructPlatform();
    std::unique_ptr<Platform> temporaryOwnership(std::move(platformImpl));
    EXPECT_EQ(nullptr, platformImpl.get());
    auto platform2 = constructPlatform();
    EXPECT_NE(platform2, platform);
    EXPECT_NE(platform, nullptr);
    EXPECT_NE(platform2, nullptr);
    platformImpl.reset(nullptr);
}

TEST(PlatformConstructionTest, givenPlatformThatIsNotInitializedWhenGetDevicesIsCalledThenNullptrIsReturned) {
    Platform platform;
    auto devices = platform.getDevices();
    EXPECT_EQ(nullptr, devices);
}

TEST(PlatformInitLoopTests, givenPlatformWhenInitLoopHelperIsCalledThenItDoesNothing) {
    struct mockPlatform : public Platform {
        using Platform::initializationLoopHelper;
    };
    mockPlatform platform;
    platform.initializationLoopHelper();
}

TEST(PlatformInitLoopTests, givenPlatformWithDebugSettingWhenInitIsCalledThenItEntersEndlessLoop) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.LoopAtPlatformInitialize.set(true);
    bool called = false;
    struct mockPlatform : public Platform {
        mockPlatform(bool &called) : called(called){};
        void initializationLoopHelper() override {
            DebugManager.flags.LoopAtPlatformInitialize.set(false);
            called = true;
        }
        bool &called;
    };
    mockPlatform platform(called);
    platform.initialize();
    EXPECT_TRUE(called);
}
