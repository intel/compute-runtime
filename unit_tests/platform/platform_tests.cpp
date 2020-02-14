/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device/device.h"
#include "core/helpers/hw_info.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "core/unit_tests/helpers/ult_hw_config.h"
#include "runtime/platform/extensions.h"
#include "runtime/sharings/sharing_factory.h"
#include "unit_tests/fixtures/mock_aub_center_fixture.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_async_event_handler.h"
#include "unit_tests/mocks/mock_builtins.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_platform.h"
#include "unit_tests/mocks/mock_source_level_debugger.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace NEO;

namespace NEO {
namespace MockSipData {
extern SipKernelType calledType;
extern bool called;
} // namespace MockSipData
} // namespace NEO

struct PlatformTest : public ::testing::Test {
    void SetUp() override {
        MockSipData::calledType = SipKernelType::COUNT;
        MockSipData::called = false;
        pPlatform.reset(new MockPlatform());
    }
    void TearDown() override {
        MockSipData::calledType = SipKernelType::COUNT;
        MockSipData::called = false;
    }
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<MockPlatform> pPlatform;
};

struct MockPlatformWithMockExecutionEnvironment : public MockPlatform {

    MockPlatformWithMockExecutionEnvironment() : MockPlatform(*(new MockExecutionEnvironment(nullptr, false, 1))) {
        MockAubCenterFixture::setMockAubCenter(*executionEnvironment.rootDeviceEnvironments[0]);
    }
};

TEST_F(PlatformTest, GivenUninitializedPlatformWhenInitializeIsCalledThenPlatformIsInitialized) {
    EXPECT_FALSE(pPlatform->isInitialized());

    pPlatform->initializeWithNewDevices();

    EXPECT_TRUE(pPlatform->isInitialized());
}

TEST_F(PlatformTest, WhenGetNumDevicesIsCalledThenExpectedValuesAreReturned) {
    EXPECT_EQ(0u, pPlatform->getNumDevices());

    pPlatform->initializeWithNewDevices();

    EXPECT_GT(pPlatform->getNumDevices(), 0u);
}

TEST_F(PlatformTest, WhenGetDeviceIsCalledThenExpectedValuesAreReturned) {
    EXPECT_EQ(nullptr, pPlatform->getDevice(0));
    EXPECT_EQ(nullptr, pPlatform->getClDevice(0));

    pPlatform->initializeWithNewDevices();

    EXPECT_NE(nullptr, pPlatform->getDevice(0));
    EXPECT_NE(nullptr, pPlatform->getClDevice(0));

    auto numDevices = pPlatform->getNumDevices();
    EXPECT_EQ(nullptr, pPlatform->getDevice(numDevices));
    EXPECT_EQ(nullptr, pPlatform->getClDevice(numDevices));
}

TEST_F(PlatformTest, WhenGetClDevicesIsCalledThenExpectedValuesAreReturned) {
    EXPECT_EQ(nullptr, pPlatform->getClDevices());

    pPlatform->initializeWithNewDevices();

    EXPECT_NE(nullptr, pPlatform->getClDevices());
}

TEST_F(PlatformTest, PlatformgetAsCompilerEnabledExtensionsString) {
    pPlatform->initializeWithNewDevices();
    auto compilerExtensions = pPlatform->getClDevice(0)->peekCompilerExtensions();

    EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string(" -cl-ext=-all,+cl")));
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
        EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("cl_khr_subgroups")));
    }
}

TEST_F(PlatformTest, hasAsyncEventsHandler) {
    EXPECT_NE(nullptr, pPlatform->getAsyncEventsHandler());
}

TEST_F(PlatformTest, givenMidThreadPreemptionWhenInitializingPlatformThenCallGetSipKernel) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));

    auto builtIns = new MockBuiltins();
    pPlatform->peekExecutionEnvironment()->builtins.reset(builtIns);

    EXPECT_EQ(SipKernelType::COUNT, MockSipData::calledType);
    EXPECT_FALSE(MockSipData::called);
    pPlatform->initializeWithNewDevices();
    EXPECT_EQ(SipKernelType::Csr, MockSipData::calledType);
    EXPECT_TRUE(MockSipData::called);
}

TEST_F(PlatformTest, givenDisabledPreemptionAndNoSourceLevelDebuggerWhenInitializingPlatformThenDoNotCallGetSipKernel) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));

    auto builtIns = new MockBuiltins();
    pPlatform->peekExecutionEnvironment()->builtins.reset(builtIns);

    EXPECT_EQ(SipKernelType::COUNT, MockSipData::calledType);
    EXPECT_FALSE(MockSipData::called);
    pPlatform->initializeWithNewDevices();
    EXPECT_EQ(SipKernelType::COUNT, MockSipData::calledType);
    EXPECT_FALSE(MockSipData::called);
}

TEST_F(PlatformTest, givenDisabledPreemptionInactiveSourceLevelDebuggerWhenInitializingPlatformThenDoNotCallGetSipKernel) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));

    auto builtIns = new MockBuiltins();
    pPlatform->peekExecutionEnvironment()->builtins.reset(builtIns);
    auto sourceLevelDebugger = new MockSourceLevelDebugger();
    sourceLevelDebugger->setActive(false);
    pPlatform->peekExecutionEnvironment()->debugger.reset(sourceLevelDebugger);

    EXPECT_EQ(SipKernelType::COUNT, MockSipData::calledType);
    EXPECT_FALSE(MockSipData::called);
    pPlatform->initializeWithNewDevices();
    EXPECT_EQ(SipKernelType::COUNT, MockSipData::calledType);
    EXPECT_FALSE(MockSipData::called);
}

TEST_F(PlatformTest, givenDisabledPreemptionActiveSourceLevelDebuggerWhenInitializingPlatformThenCallGetSipKernel) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));

    auto builtIns = new MockBuiltins();
    pPlatform->peekExecutionEnvironment()->builtins.reset(builtIns);
    pPlatform->peekExecutionEnvironment()->debugger.reset(new MockActiveSourceLevelDebugger());

    EXPECT_EQ(SipKernelType::COUNT, MockSipData::calledType);
    EXPECT_FALSE(MockSipData::called);
    pPlatform->initializeWithNewDevices();
    EXPECT_TRUE(MockSipData::called);
    EXPECT_LE(SipKernelType::DbgCsr, MockSipData::calledType);
    EXPECT_GE(SipKernelType::DbgCsrLocal, MockSipData::calledType);
}

TEST(PlatformTestSimple, givenCsrHwTypeWhenPlatformIsInitializedThenInitAubCenterIsNotCalled) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.SetCommandStreamReceiver.set(0);
    MockPlatformWithMockExecutionEnvironment platform;

    bool ret = platform.initializeWithNewDevices();
    EXPECT_TRUE(ret);
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(platform.peekExecutionEnvironment()->rootDeviceEnvironments[0].get());
    EXPECT_FALSE(rootDeviceEnvironment->initAubCenterCalled);
}

TEST(PlatformTestSimple, givenNotCsrHwTypeWhenPlatformIsInitializedThenInitAubCenterIsCalled) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.SetCommandStreamReceiver.set(1);
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;
    MockPlatformWithMockExecutionEnvironment platform;
    bool ret = platform.initializeWithNewDevices();
    EXPECT_TRUE(ret);
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(platform.peekExecutionEnvironment()->rootDeviceEnvironments[0].get());
    EXPECT_TRUE(rootDeviceEnvironment->initAubCenterCalled);
}

TEST(PlatformTestSimple, shutdownClosesAsyncEventHandlerThread) {
    Platform *platform = new MockPlatform();

    MockHandler *mockAsyncHandler = new MockHandler();

    auto oldHandler = platform->setAsyncEventsHandler(std::unique_ptr<AsyncEventsHandler>(mockAsyncHandler));
    EXPECT_EQ(mockAsyncHandler, platform->getAsyncEventsHandler());

    mockAsyncHandler->openThread();
    delete platform;
    EXPECT_TRUE(MockAsyncEventHandlerGlobals::destructorCalled);
}

namespace NEO {
extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[IGFX_MAX_CORE];
}

CommandStreamReceiver *createMockCommandStreamReceiver(bool withAubDump, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) {
    return nullptr;
};

class PlatformFailingTest : public PlatformTest {
  public:
    PlatformFailingTest() {
        ultHwConfig.useHwCsr = true;
    }
    void SetUp() override {
        PlatformTest::SetUp();
        hwInfo = platformDevices[0];
        commandStreamReceiverCreateFunc = commandStreamReceiverFactory[hwInfo->platform.eRenderCoreFamily];
        commandStreamReceiverFactory[hwInfo->platform.eRenderCoreFamily] = createMockCommandStreamReceiver;
    }

    void TearDown() override {
        commandStreamReceiverFactory[hwInfo->platform.eRenderCoreFamily] = commandStreamReceiverCreateFunc;
        PlatformTest::TearDown();
    }

    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    CommandStreamReceiverCreateFunc commandStreamReceiverCreateFunc;
    const HardwareInfo *hwInfo;
};

TEST_F(PlatformFailingTest, givenPlatformInitializationWhenIncorrectHwInfoThenInitializationFails) {
    auto platform = new MockPlatform();
    bool ret = platform->initializeWithNewDevices();
    EXPECT_FALSE(ret);
    EXPECT_FALSE(platform->isInitialized());
    delete platform;
}

TEST_F(PlatformTest, givenSupportingCl21WhenPlatformSupportsFp64ThenFillMatchingSubstringsAndMandatoryTrailingSpace) {
    const HardwareInfo *hwInfo;
    hwInfo = platformDevices[0];
    std::string extensionsList = getExtensionsList(*hwInfo);

    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str());
    EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string(" -cl-ext=-all,+cl")));

    if (hwInfo->capabilityTable.clVersionSupport > 20) {
        EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("cl_khr_subgroups")));
        EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("cl_khr_il_program")));
        if (hwInfo->capabilityTable.supportsVme) {
            EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("cl_intel_spirv_device_side_avc_motion_estimation")));
        } else {
            EXPECT_THAT(compilerExtensions, testing::Not(::testing::HasSubstr(std::string("cl_intel_spirv_device_side_avc_motion_estimation"))));
        }
        if (hwInfo->capabilityTable.supportsImages) {
            EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("cl_intel_spirv_media_block_io")));
        } else {
            EXPECT_THAT(compilerExtensions, testing::Not(::testing::HasSubstr(std::string("cl_intel_spirv_media_block_io"))));
        }

        EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("cl_intel_spirv_subgroups")));
        EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("cl_khr_spirv_no_integer_wrap_decoration")));
    }

    if (hwInfo->capabilityTable.ftrSupportsFP64) {
        EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("cl_khr_fp64")));
    }

    if (hwInfo->capabilityTable.supportsImages) {
        EXPECT_THAT(extensionsList, ::testing::HasSubstr(std::string("cl_khr_3d_image_writes")));
    }
    EXPECT_THAT(compilerExtensions, ::testing::EndsWith(std::string(" ")));
}

TEST_F(PlatformTest, givenNotSupportingCl21WhenPlatformNotSupportFp64ThenNotFillMatchingSubstringAndFillMandatoryTrailingSpace) {
    HardwareInfo TesthwInfo = *platformDevices[0];
    TesthwInfo.capabilityTable.ftrSupportsFP64 = false;
    TesthwInfo.capabilityTable.clVersionSupport = 10;

    std::string extensionsList = getExtensionsList(TesthwInfo);
    if (TesthwInfo.capabilityTable.supportsImages) {
        EXPECT_THAT(extensionsList, ::testing::HasSubstr(std::string("cl_khr_3d_image_writes")));
    }

    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str());
    EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("-cl-ext=-all,+cl")));

    EXPECT_THAT(compilerExtensions, ::testing::Not(::testing::HasSubstr(std::string("cl_khr_fp64"))));
    EXPECT_THAT(compilerExtensions, ::testing::Not(::testing::HasSubstr(std::string("cl_khr_subgroups"))));

    EXPECT_THAT(compilerExtensions, ::testing::EndsWith(std::string(" ")));
}

TEST_F(PlatformTest, givenFtrSupportAtomicsWhenCreateExtentionsListThenGetMatchingSubstrings) {
    const HardwareInfo *hwInfo;
    hwInfo = platformDevices[0];
    std::string extensionsList = getExtensionsList(*hwInfo);
    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str());

    if (hwInfo->capabilityTable.ftrSupportsInteger64BitAtomics) {
        EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("cl_khr_int64_base_atomics")));
        EXPECT_THAT(compilerExtensions, ::testing::HasSubstr(std::string("cl_khr_int64_extended_atomics")));
    } else {
        EXPECT_THAT(compilerExtensions, ::testing::Not(::testing::HasSubstr(std::string("cl_khr_int64_base_atomics"))));
        EXPECT_THAT(compilerExtensions, ::testing::Not(::testing::HasSubstr(std::string("cl_khr_int64_extended_atomics"))));
    }
}

TEST_F(PlatformTest, givenSupporteImagesAndClVersion21WhenCreateExtentionsListThenDeviceReportsSpritvMediaBlockIoExtension) {
    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsImages = true;
    hwInfo.capabilityTable.clVersionSupport = 21;
    std::string extensionsList = getExtensionsList(hwInfo);
    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str());

    EXPECT_THAT(compilerExtensions, testing::HasSubstr(std::string("cl_intel_spirv_media_block_io")));
}

TEST_F(PlatformTest, givenNotSupporteImagesAndClVersion21WhenCreateExtentionsListThenDeviceNotReportsSpritvMediaBlockIoExtension) {
    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsImages = false;
    hwInfo.capabilityTable.clVersionSupport = 21;
    std::string extensionsList = getExtensionsList(hwInfo);
    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str());

    EXPECT_THAT(compilerExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_spirv_media_block_io"))));
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
    platformsImpl.clear();
    auto platform1 = constructPlatform();
    EXPECT_EQ(platform1, platform());
    auto platform2 = constructPlatform();
    EXPECT_EQ(platform2, platform1);
    EXPECT_NE(platform1, nullptr);
}

TEST(PlatformConstructionTest, givenPlatformConstructorWhenItIsCalledAfterResetThenNewPlatformIsConstructed) {
    platformsImpl.clear();
    auto platform = constructPlatform();
    std::unique_ptr<Platform> temporaryOwnership(std::move(platformsImpl[0]));
    platformsImpl.clear();
    auto platform2 = constructPlatform();
    EXPECT_NE(platform2, platform);
    EXPECT_NE(platform, nullptr);
    EXPECT_NE(platform2, nullptr);
    platformsImpl.clear();
}

TEST(PlatformInitLoopTests, givenPlatformWhenInitLoopHelperIsCalledThenItDoesNothing) {
    MockPlatform platform;
    platform.initializationLoopHelper();
}

TEST(PlatformInitTest, givenNullptrDeviceInPassedDeviceVectorWhenInitializePlatformThenExceptionIsThrown) {
    std::vector<std::unique_ptr<Device>> devices;
    devices.push_back(nullptr);
    EXPECT_THROW(platform()->initialize(std::move(devices)), std::exception);
}

TEST(PlatformInitTest, givenInitializedPlatformWhenInitializeIsCalledOneMoreTimeWithNullptrDeviceThenSuccessIsEarlyReturned) {
    initPlatform();
    EXPECT_TRUE(platform()->isInitialized());
    std::vector<std::unique_ptr<Device>> devices;
    devices.push_back(nullptr);
    EXPECT_TRUE(platform()->initialize(std::move(devices)));
}

TEST(PlatformInitLoopTests, givenPlatformWithDebugSettingWhenInitIsCalledThenItEntersEndlessLoop) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.LoopAtPlatformInitialize.set(true);
    bool called = false;
    struct mockPlatform : public MockPlatform {
        mockPlatform(bool &called) : called(called){};
        void initializationLoopHelper() override {
            DebugManager.flags.LoopAtPlatformInitialize.set(false);
            called = true;
        }
        bool &called;
    };
    mockPlatform platform(called);
    platform.initializeWithNewDevices();
    EXPECT_TRUE(called);
}
