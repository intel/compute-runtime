/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/oclc_extensions.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/mock_source_level_debugger.h"
#include "shared/test/unit_test/fixtures/mock_aub_center_fixture.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/sharings/sharing_factory.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "gtest/gtest.h"

using namespace NEO;

struct PlatformTest : public ::testing::Test {
    void SetUp() override {
        MockSipData::clearUseFlags();
        backupSipInitType = std::make_unique<VariableBackup<bool>>(&MockSipData::useMockSip, true);

        pPlatform.reset(new MockPlatform());
    }
    void TearDown() override {
        MockSipData::clearUseFlags();
    }
    std::unique_ptr<MockPlatform> pPlatform;
    std::unique_ptr<VariableBackup<bool>> backupSipInitType;

    cl_int retVal = CL_SUCCESS;
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

TEST_F(PlatformTest, givenSupportingCl21WhenGettingExtensionsStringThenSubgroupsIsEnabled) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    pPlatform->initializeWithNewDevices();
    auto compilerExtensions = pPlatform->getClDevice(0)->peekCompilerExtensions();

    auto isIndependentForwardProgressSupported = pPlatform->getClDevice(0)->getDeviceInfo().independentForwardProgress;

    EXPECT_TRUE(hasSubstr(compilerExtensions, std::string(" -cl-ext=-all,+cl")));
    if (isIndependentForwardProgressSupported) {
        EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_khr_subgroups")));
    }
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

TEST(PlatformTestSimple, WhenConvertingCustomOclCFeaturesToCompilerInternalOptionsThenResultIsCorrect) {
    OpenClCFeaturesContainer customOpenclCFeatures;

    cl_name_version feature;
    strcpy_s(feature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "custom_feature");
    customOpenclCFeatures.push_back(feature);
    auto compilerOption = convertEnabledExtensionsToCompilerInternalOptions("", customOpenclCFeatures);
    EXPECT_STREQ(" -cl-ext=-all,+custom_feature ", compilerOption.c_str());

    strcpy_s(feature.name, CL_NAME_VERSION_MAX_NAME_SIZE, "other_extra_feature");
    customOpenclCFeatures.push_back(feature);
    compilerOption = convertEnabledExtensionsToCompilerInternalOptions("", customOpenclCFeatures);
    EXPECT_STREQ(" -cl-ext=-all,+custom_feature,+other_extra_feature ", compilerOption.c_str());
}

TEST(PlatformTestSimple, WhenConvertingOclCFeaturesToCompilerInternalOptionsThenResultIsCorrect) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];

    std::string expectedCompilerOption = " -cl-ext=-all,";
    for (auto &openclCFeature : pClDevice->deviceInfo.openclCFeatures) {
        expectedCompilerOption += "+";
        expectedCompilerOption += openclCFeature.name;
        expectedCompilerOption += ",";
    }
    expectedCompilerOption.erase(expectedCompilerOption.size() - 1, 1);
    expectedCompilerOption += " ";

    auto compilerOption = convertEnabledExtensionsToCompilerInternalOptions("", pClDevice->deviceInfo.openclCFeatures);
    EXPECT_STREQ(expectedCompilerOption.c_str(), compilerOption.c_str());
}

namespace NEO {
extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
}

CommandStreamReceiver *createMockCommandStreamReceiver(bool withAubDump,
                                                       ExecutionEnvironment &executionEnvironment,
                                                       uint32_t rootDeviceIndex,
                                                       const DeviceBitfield deviceBitfield) {
    return nullptr;
};

class PlatformFailingTest : public PlatformTest {
  public:
    PlatformFailingTest() {
        ultHwConfig.useHwCsr = true;
    }
    void SetUp() override {
        PlatformTest::SetUp();
        hwInfo = defaultHwInfo.get();
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
    hwInfo = defaultHwInfo.get();
    std::string extensionsList = getExtensionsList(*hwInfo);
    OpenClCFeaturesContainer features;
    getOpenclCFeaturesList(*hwInfo, features);

    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str(), features);
    EXPECT_TRUE(hasSubstr(compilerExtensions, std::string(" -cl-ext=-all,+cl")));

    if (hwInfo->capabilityTable.supportsOcl21Features) {
        EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_khr_subgroups")));
        if (hwInfo->capabilityTable.supportsVme) {
            EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_intel_spirv_device_side_avc_motion_estimation")));
        } else {
            EXPECT_FALSE(hasSubstr(compilerExtensions, std::string("cl_intel_spirv_device_side_avc_motion_estimation")));
        }
        if (hwInfo->capabilityTable.supportsMediaBlock) {
            EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_intel_spirv_media_block_io")));
        } else {
            EXPECT_FALSE(hasSubstr(compilerExtensions, std::string("cl_intel_spirv_media_block_io")));
        }

        EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_intel_spirv_subgroups")));
        EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_khr_spirv_no_integer_wrap_decoration")));
    }

    if (hwInfo->capabilityTable.ftrSupportsFP64) {
        EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_khr_fp64")));
    }

    if (hwInfo->capabilityTable.supportsImages) {
        EXPECT_TRUE(hasSubstr(extensionsList, std::string("cl_khr_3d_image_writes")));
    }
    EXPECT_TRUE(endsWith(compilerExtensions, std::string(" ")));
}

TEST_F(PlatformTest, givenNotSupportingCl21WhenPlatformNotSupportFp64ThenNotFillMatchingSubstringAndFillMandatoryTrailingSpace) {
    HardwareInfo TesthwInfo = *defaultHwInfo;
    TesthwInfo.capabilityTable.ftrSupportsFP64 = false;
    TesthwInfo.capabilityTable.clVersionSupport = 10;
    TesthwInfo.capabilityTable.supportsOcl21Features = false;

    std::string extensionsList = getExtensionsList(TesthwInfo);
    OpenClCFeaturesContainer features;
    getOpenclCFeaturesList(*defaultHwInfo, features);
    if (TesthwInfo.capabilityTable.supportsImages) {
        EXPECT_TRUE(hasSubstr(extensionsList, std::string("cl_khr_3d_image_writes")));
    }

    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str(), features);
    EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("-cl-ext=-all,+cl")));

    EXPECT_FALSE(hasSubstr(compilerExtensions, std::string("cl_khr_fp64")));
    EXPECT_FALSE(hasSubstr(compilerExtensions, std::string("cl_khr_subgroups")));

    EXPECT_TRUE(endsWith(compilerExtensions, std::string(" ")));
}

TEST_F(PlatformTest, givenFtrSupportAtomicsWhenCreateExtentionsListThenGetMatchingSubstrings) {
    const HardwareInfo *hwInfo;
    hwInfo = defaultHwInfo.get();
    std::string extensionsList = getExtensionsList(*hwInfo);
    OpenClCFeaturesContainer features;
    getOpenclCFeaturesList(*hwInfo, features);
    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str(), features);

    if (hwInfo->capabilityTable.ftrSupportsInteger64BitAtomics) {
        EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_khr_int64_base_atomics")));
        EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_khr_int64_extended_atomics")));
    } else {
        EXPECT_FALSE(hasSubstr(compilerExtensions, std::string("cl_khr_int64_base_atomics")));
        EXPECT_FALSE(hasSubstr(compilerExtensions, std::string("cl_khr_int64_extended_atomics")));
    }
}

TEST_F(PlatformTest, givenSupportedMediaBlockAndClVersion21WhenCreateExtentionsListThenDeviceReportsSpritvMediaBlockIoExtension) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsMediaBlock = true;
    hwInfo.capabilityTable.clVersionSupport = 21;
    hwInfo.capabilityTable.supportsOcl21Features = true;
    std::string extensionsList = getExtensionsList(hwInfo);
    OpenClCFeaturesContainer features;
    getOpenclCFeaturesList(*defaultHwInfo, features);
    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str(), features);

    EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_intel_spirv_media_block_io")));
}

TEST_F(PlatformTest, givenNotSupportedMediaBlockAndClVersion21WhenCreateExtentionsListThenDeviceNotReportsSpritvMediaBlockIoExtension) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsMediaBlock = false;
    hwInfo.capabilityTable.clVersionSupport = 21;
    std::string extensionsList = getExtensionsList(hwInfo);
    OpenClCFeaturesContainer features;
    getOpenclCFeaturesList(*defaultHwInfo, features);
    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str(), features);

    EXPECT_FALSE(hasSubstr(compilerExtensions, std::string("cl_intel_spirv_media_block_io")));
}

TEST_F(PlatformTest, givenSupportedImagesWhenCreateExtentionsListThenDeviceNotReportsKhr3DImageWritesExtension) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsImages = true;
    std::string extensionsList = getExtensionsList(hwInfo);
    OpenClCFeaturesContainer features;
    getOpenclCFeaturesList(*defaultHwInfo, features);
    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str(), features);

    EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_khr_3d_image_writes")));
}

TEST_F(PlatformTest, givenNotSupportedImagesWhenCreateExtentionsListThenDeviceNotReportsKhr3DImageWritesExtension) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsImages = false;
    std::string extensionsList = getExtensionsList(hwInfo);
    OpenClCFeaturesContainer features;
    getOpenclCFeaturesList(*defaultHwInfo, features);
    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str(), features);

    EXPECT_FALSE(hasSubstr(compilerExtensions, std::string("cl_khr_3d_image_writes")));
}

TEST(PlatformConstructionTest, givenPlatformConstructorWhenItIsCalledTwiceThenTheSamePlatformIsReturned) {
    platformsImpl->clear();
    auto platform1 = constructPlatform();
    EXPECT_EQ(platform1, platform());
    auto platform2 = constructPlatform();
    EXPECT_EQ(platform2, platform1);
    EXPECT_NE(platform1, nullptr);
}

TEST(PlatformConstructionTest, givenPlatformConstructorWhenItIsCalledAfterResetThenNewPlatformIsConstructed) {
    platformsImpl->clear();
    auto platform = constructPlatform();
    std::unique_ptr<Platform> temporaryOwnership(std::move((*platformsImpl)[0]));
    platformsImpl->clear();
    auto platform2 = constructPlatform();
    EXPECT_NE(platform2, platform);
    EXPECT_NE(platform, nullptr);
    EXPECT_NE(platform2, nullptr);
    platformsImpl->clear();
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
    auto executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, 3);
    devices.push_back(std::make_unique<MockDevice>(executionEnvironment, 2));
    auto status = platform()->initialize(std::move(devices));
    EXPECT_TRUE(status);
    size_t expectedNumDevices = 1u;
    EXPECT_EQ(expectedNumDevices, platform()->getNumDevices());
    EXPECT_EQ(2u, platform()->getClDevice(0)->getRootDeviceIndex());
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
