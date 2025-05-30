/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/fixtures/mock_aub_center_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_sip.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/platform/platform_info.h"
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
        compilerProductHelper = CompilerProductHelper::create(defaultHwInfo->platform.eProductFamily);
        releaseHelper = ReleaseHelper::create(defaultHwInfo->ipVersion);
    }
    void TearDown() override {
        MockSipData::clearUseFlags();
    }
    std::unique_ptr<MockPlatform> pPlatform;
    std::unique_ptr<VariableBackup<bool>> backupSipInitType;
    std::unique_ptr<CompilerProductHelper> compilerProductHelper;
    std::unique_ptr<ReleaseHelper> releaseHelper;

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

TEST_F(PlatformTest, WhenPlatformIsDestroyedThenDirectSubmissionIsTerminated) {
    VariableBackup<decltype(DeviceFactory::createRootDeviceFunc)> createFuncBackup{&DeviceFactory::createRootDeviceFunc};
    DeviceFactory::createRootDeviceFunc = [](ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) -> std::unique_ptr<Device> {
        return std::unique_ptr<Device>(MockDevice::create<MockDevice>(&executionEnvironment, rootDeviceIndex));
    };

    pPlatform->initializeWithNewDevices();

    auto clDevice = pPlatform->getClDevice(0);
    EXPECT_NE(nullptr, clDevice);

    auto mockDevice = static_cast<MockDevice *>(&clDevice->getDevice());
    EXPECT_FALSE(mockDevice->stopDirectSubmissionCalled);

    clDevice->incRefInternal();

    pPlatform = nullptr;
    EXPECT_TRUE(mockDevice->stopDirectSubmissionCalled);

    clDevice->decRefInternal();
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
    debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));

    auto builtIns = new MockBuiltins();
    auto executionEnvironment = pPlatform->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), builtIns);

    EXPECT_EQ(SipKernelType::count, MockSipData::calledType);
    EXPECT_FALSE(MockSipData::called);
    pPlatform->initializeWithNewDevices();
    EXPECT_EQ(SipKernelType::csr, MockSipData::calledType);
    EXPECT_TRUE(MockSipData::called);
}

TEST_F(PlatformTest, givenDisabledPreemptionAndNoDebuggerWhenInitializingPlatformThenDoNotCallGetSipKernel) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));

    auto builtIns = new MockBuiltins();
    auto executionEnvironment = pPlatform->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), builtIns);

    EXPECT_EQ(SipKernelType::count, MockSipData::calledType);
    EXPECT_FALSE(MockSipData::called);
    pPlatform->initializeWithNewDevices();
    EXPECT_EQ(SipKernelType::count, MockSipData::calledType);
    EXPECT_FALSE(MockSipData::called);
}

TEST(PlatformTestSimple, givenCsrHwTypeWhenPlatformIsInitializedThenInitAubCenterIsNotCalled) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetCommandStreamReceiver.set(0);
    MockPlatformWithMockExecutionEnvironment platform;

    bool ret = platform.initializeWithNewDevices();
    EXPECT_TRUE(ret);
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(platform.peekExecutionEnvironment()->rootDeviceEnvironments[0].get());
    EXPECT_FALSE(rootDeviceEnvironment->initAubCenterCalled);
}

TEST(PlatformTestSimple, givenNotCsrHwTypeWhenPlatformIsInitializedThenInitAubCenterIsCalled) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetCommandStreamReceiver.set(1);
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
    const HardwareInfo *hwInfo{};
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
    std::string extensionsList = compilerProductHelper->getDeviceExtensions(*hwInfo, releaseHelper.get());
    OpenClCFeaturesContainer features;
    getOpenclCFeaturesList(*hwInfo, features, *compilerProductHelper.get(), releaseHelper.get());

    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str(), features);
    EXPECT_TRUE(hasSubstr(compilerExtensions, std::string(" -cl-ext=-all,+cl")));

    if (hwInfo->capabilityTable.supportsOcl21Features) {
        if (hwInfo->capabilityTable.supportsMediaBlock) {
            EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_intel_spirv_media_block_io")));
        } else {
            EXPECT_FALSE(hasSubstr(compilerExtensions, std::string("cl_intel_spirv_media_block_io")));
        }

        EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_intel_spirv_subgroups")));
        EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_khr_spirv_linkonce_odr")));
        EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_khr_spirv_no_integer_wrap_decoration")));
        EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_khr_spirv_queries")));
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
    HardwareInfo testHwInfo = *defaultHwInfo;
    testHwInfo.capabilityTable.ftrSupportsFP64 = false;
    testHwInfo.capabilityTable.clVersionSupport = 10;
    testHwInfo.capabilityTable.supportsOcl21Features = false;

    std::string extensionsList = compilerProductHelper->getDeviceExtensions(testHwInfo, releaseHelper.get());
    OpenClCFeaturesContainer features;
    getOpenclCFeaturesList(*defaultHwInfo, features, *compilerProductHelper.get(), releaseHelper.get());
    if (testHwInfo.capabilityTable.supportsImages) {
        EXPECT_TRUE(hasSubstr(extensionsList, std::string("cl_khr_3d_image_writes")));
    }

    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str(), features);
    EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("-cl-ext=-all,+cl")));

    EXPECT_FALSE(hasSubstr(compilerExtensions, std::string("cl_khr_fp64")));

    EXPECT_TRUE(endsWith(compilerExtensions, std::string(" ")));
}

TEST_F(PlatformTest, givenFtrSupportAtomicsWhenCreateExtentionsListThenGetMatchingSubstrings) {
    const HardwareInfo *hwInfo;
    hwInfo = defaultHwInfo.get();
    std::string extensionsList = compilerProductHelper->getDeviceExtensions(*hwInfo, releaseHelper.get());
    OpenClCFeaturesContainer features;
    getOpenclCFeaturesList(*hwInfo, features, *compilerProductHelper.get(), releaseHelper.get());
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
    std::string extensionsList = compilerProductHelper->getDeviceExtensions(hwInfo, releaseHelper.get());
    OpenClCFeaturesContainer features;
    getOpenclCFeaturesList(*defaultHwInfo, features, *compilerProductHelper.get(), releaseHelper.get());
    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str(), features);

    EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_intel_spirv_media_block_io")));
}

TEST_F(PlatformTest, givenNotSupportedMediaBlockAndClVersion21WhenCreateExtentionsListThenDeviceNotReportsSpritvMediaBlockIoExtension) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsMediaBlock = false;
    hwInfo.capabilityTable.clVersionSupport = 21;
    std::string extensionsList = compilerProductHelper->getDeviceExtensions(hwInfo, releaseHelper.get());
    OpenClCFeaturesContainer features;
    getOpenclCFeaturesList(*defaultHwInfo, features, *compilerProductHelper.get(), releaseHelper.get());
    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str(), features);

    EXPECT_FALSE(hasSubstr(compilerExtensions, std::string("cl_intel_spirv_media_block_io")));
}

TEST_F(PlatformTest, givenSupportedImagesWhenCreateExtentionsListThenDeviceNotReportsKhr3DImageWritesExtension) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsImages = true;
    std::string extensionsList = compilerProductHelper->getDeviceExtensions(hwInfo, releaseHelper.get());
    OpenClCFeaturesContainer features;
    getOpenclCFeaturesList(*defaultHwInfo, features, *compilerProductHelper.get(), releaseHelper.get());
    std::string compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str(), features);

    EXPECT_TRUE(hasSubstr(compilerExtensions, std::string("cl_khr_3d_image_writes")));
}

TEST_F(PlatformTest, givenNotSupportedImagesWhenCreateExtentionsListThenDeviceNotReportsKhr3DImageWritesExtension) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsImages = false;
    std::string extensionsList = compilerProductHelper->getDeviceExtensions(hwInfo, releaseHelper.get());
    OpenClCFeaturesContainer features;
    getOpenclCFeaturesList(*defaultHwInfo, features, *compilerProductHelper.get(), releaseHelper.get());
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
    constructPlatform(executionEnvironment);
    devices.push_back(std::unique_ptr<Device>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 2)));
    auto status = platform(executionEnvironment)->initialize(std::move(devices));
    EXPECT_TRUE(status);
    size_t expectedNumDevices = 1u;
    EXPECT_EQ(expectedNumDevices, platform(executionEnvironment)->getNumDevices());
    EXPECT_EQ(2u, platform(executionEnvironment)->getClDevice(0)->getRootDeviceIndex());
    cleanupPlatform(executionEnvironment);
}

TEST(PlatformInitTest, GivenPreferredPlatformNameWhenPlatformIsInitializedThenOverridePlatformName) {
    std::vector<std::unique_ptr<Device>> devices;
    auto executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, 1);
    constructPlatform(executionEnvironment);
    devices.push_back(std::unique_ptr<Device>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0)));
    executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.preferredPlatformName = "Overridden Platform Name";
    auto status = platform(executionEnvironment)->initialize(std::move(devices));
    EXPECT_TRUE(status);
    EXPECT_STREQ("Overridden Platform Name", platform(executionEnvironment)->getPlatformInfo().name.c_str());
    cleanupPlatform(executionEnvironment);
}
