/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"

#include "opencl/source/device/cl_device.h"
#include "opencl/source/platform/extensions.h"
#include "opencl/source/sharings/sharing_factory.h"
#include "opencl/test/unit_test/fixtures/mock_aub_center_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_async_event_handler.h"
#include "opencl/test/unit_test/mocks/mock_builtins.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/mock_source_level_debugger.h"

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
    EXPECT_EQ(nullptr, pPlatform->getClDevice(0));

    pPlatform->initializeWithNewDevices();

    EXPECT_NE(nullptr, pPlatform->getClDevice(0));

    auto numDevices = pPlatform->getNumDevices();
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
    if (std::string(pPlatform->getClDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
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
    auto executionEnvironment = pPlatform->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(builtIns);

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
    auto executionEnvironment = pPlatform->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(builtIns);

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
    auto executionEnvironment = pPlatform->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(builtIns);
    auto sourceLevelDebugger = new MockSourceLevelDebugger();
    sourceLevelDebugger->setActive(false);
    executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(sourceLevelDebugger);

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
    auto executionEnvironment = pPlatform->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(builtIns);
    executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(new MockActiveSourceLevelDebugger());

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

TEST(PlatformInitTest, givenSingleDeviceWithNonZeroRootDeviceIndexInPassedDeviceVectorWhenInitializePlatformThenCreateOnlyOneClDevice) {
    std::vector<std::unique_ptr<Device>> devices;
    auto executionEnvironment = new MockExecutionEnvironment(*platformDevices, false, 3);
    devices.push_back(std::make_unique<MockDevice>(executionEnvironment, 2));
    auto status = platform()->initialize(std::move(devices));
    EXPECT_TRUE(status);
    size_t expectedNumDevices = 1u;
    EXPECT_EQ(expectedNumDevices, platform()->getNumDevices());
    EXPECT_EQ(2u, platform()->getClDevice(0)->getRootDeviceIndex());
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

TEST(PlatformGroupDevicesTest, whenMultipleDevicesAreCreatedThenGroupDevicesCreatesVectorPerEachProductFamily) {
    DebugManagerStateRestore restorer;
    const size_t numRootDevices = 5u;

    DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    auto executionEnvironment = new ExecutionEnvironment();

    for (auto i = 0u; i < numRootDevices; i++) {
        executionEnvironment->rootDeviceEnvironments.push_back(std::make_unique<MockRootDeviceEnvironment>(*executionEnvironment));
    }
    auto inputDevices = DeviceFactory::createDevices(*executionEnvironment);
    EXPECT_EQ(numRootDevices, inputDevices.size());

    auto skl0Device = inputDevices[0].get();
    auto kbl0Device = inputDevices[1].get();
    auto skl1Device = inputDevices[2].get();
    auto skl2Device = inputDevices[3].get();
    auto cfl0Device = inputDevices[4].get();

    executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_SKYLAKE;
    executionEnvironment->rootDeviceEnvironments[1]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_KABYLAKE;
    executionEnvironment->rootDeviceEnvironments[2]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_SKYLAKE;
    executionEnvironment->rootDeviceEnvironments[3]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_SKYLAKE;
    executionEnvironment->rootDeviceEnvironments[4]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_COFFEELAKE;

    auto groupedDevices = Platform::groupDevices(std::move(inputDevices));

    EXPECT_EQ(3u, groupedDevices.size());
    EXPECT_EQ(1u, groupedDevices[0].size());
    EXPECT_EQ(1u, groupedDevices[1].size());
    EXPECT_EQ(3u, groupedDevices[2].size());

    EXPECT_EQ(skl0Device, groupedDevices[2][0].get());
    EXPECT_EQ(skl1Device, groupedDevices[2][1].get());
    EXPECT_EQ(skl2Device, groupedDevices[2][2].get());
    EXPECT_EQ(kbl0Device, groupedDevices[1][0].get());
    EXPECT_EQ(cfl0Device, groupedDevices[0][0].get());
}
