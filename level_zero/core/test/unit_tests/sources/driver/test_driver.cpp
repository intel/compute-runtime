/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/mocks/mock_io_functions.h"
#include "test.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"

#include <bitset>

namespace L0 {
namespace ult {

TEST(zeInit, whenCallingZeInitThenInitializeOnDriverIsCalled) {
    Mock<Driver> driver;

    auto result = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, driver.initCalledCount);
}

using DriverVersionTest = Test<DeviceFixture>;

TEST_F(DriverVersionTest, givenCallToGetExtensionPropertiesThenUnsupportedIsReturned) {
    uint32_t count = 0;
    ze_driver_extension_properties_t properties;
    ze_result_t res = driverHandle->getExtensionProperties(&count, &properties);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);
}

TEST_F(DriverVersionTest, returnsExpectedDriverVersion) {
    ze_driver_properties_t properties;
    ze_result_t res = driverHandle->getProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint32_t versionMajor = static_cast<uint32_t>((properties.driverVersion & 0xFF000000) >> 24);
    uint32_t versionMinor = static_cast<uint32_t>((properties.driverVersion & 0x00FF0000) >> 16);
    uint32_t versionBuild = static_cast<uint32_t>(properties.driverVersion & 0x0000FFFF);

    EXPECT_EQ(static_cast<uint32_t>(strtoul(L0_PROJECT_VERSION_MAJOR, NULL, 10)), versionMajor);
    EXPECT_EQ(static_cast<uint32_t>(strtoul(L0_PROJECT_VERSION_MINOR, NULL, 10)), versionMinor);
    EXPECT_EQ(static_cast<uint32_t>(strtoul(NEO_VERSION_BUILD, NULL, 10)), versionBuild);
}

TEST(DriverTestFamilySupport, whenInitializingDriverOnSupportedFamilyThenDriverIsCreated) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    auto driverHandle = DriverHandle::create(std::move(devices), L0EnvVariables{});
    EXPECT_NE(nullptr, driverHandle);
    delete driverHandle;
    L0::GlobalDriver = nullptr;
}

TEST(DriverTestFamilySupport, whenInitializingDriverOnNotSupportedFamilyThenDriverIsNotCreated) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = false;

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    auto driverHandle = DriverHandle::create(std::move(devices), L0EnvVariables{});
    EXPECT_EQ(nullptr, driverHandle);
}

TEST(DriverTest, givenNullEnvVariableWhenCreatingDriverThenEnableProgramDebuggingIsFalse) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    L0EnvVariables envVariables = {};
    envVariables.programDebugging = false;

    auto driverHandle = whitebox_cast(DriverHandle::create(std::move(devices), envVariables));
    EXPECT_NE(nullptr, driverHandle);

    EXPECT_FALSE(driverHandle->enableProgramDebugging);

    delete driverHandle;
    L0::GlobalDriver = nullptr;
}

TEST(DriverImpTest, givenDriverImpWhenInitializedThenEnvVariablesAreRead) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_LE(3u, IoFunctions::mockGetenvCalled);

    delete L0::GlobalDriver;
    L0::GlobalDriverHandle = nullptr;
    L0::GlobalDriver = nullptr;
}

TEST(DriverImpTest, DISABLED_givenMissingMetricApiDependenciesWhenInitializingDriverImpThenGlobalDriverHandleIsNull) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_METRICS", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(nullptr, L0::GlobalDriverHandle);
    EXPECT_EQ(nullptr, L0::GlobalDriver);
}

TEST(DriverImpTest, givenEnabledProgramDebuggingWhenCreatingExecutionEnvironmentThenPerContextMemorySpaceIsTrue) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_NE(nullptr, L0::GlobalDriver);
    ASSERT_NE(0u, L0::GlobalDriver->numDevices);
    EXPECT_TRUE(L0::GlobalDriver->devices[0]->getNEODevice()->getExecutionEnvironment()->isPerContextMemorySpaceRequired());

    delete L0::GlobalDriver;
    L0::GlobalDriverHandle = nullptr;
    L0::GlobalDriver = nullptr;
}

TEST(DriverImpTest, givenNoProgramDebuggingEnvVarWhenCreatingExecutionEnvironmentThenPerContextMemorySpaceIsFalse) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_NE(nullptr, L0::GlobalDriver);
    ASSERT_NE(0u, L0::GlobalDriver->numDevices);
    EXPECT_FALSE(L0::GlobalDriver->devices[0]->getNEODevice()->getExecutionEnvironment()->isPerContextMemorySpaceRequired());

    delete L0::GlobalDriver;
    L0::GlobalDriverHandle = nullptr;
    L0::GlobalDriver = nullptr;
}

TEST(DriverTest, givenProgramDebuggingEnvVarNonZeroWhenCreatingDriverThenEnableProgramDebuggingIsSetTrue) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    L0EnvVariables envVariables = {};
    envVariables.programDebugging = true;

    auto driverHandle = whitebox_cast(DriverHandle::create(std::move(devices), envVariables));
    EXPECT_NE(nullptr, driverHandle);

    EXPECT_TRUE(driverHandle->enableProgramDebugging);

    delete driverHandle;
    L0::GlobalDriver = nullptr;
}

struct DriverTestMultipleFamilySupport : public ::testing::Test {
    void SetUp() override {
        VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
        }

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), executionEnvironment, i)));
            if (i < numSupportedRootDevices) {
                devices[i]->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.levelZeroSupported = true;
            } else {
                devices[i]->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.levelZeroSupported = false;
            }
        }
    }

    DebugManagerStateRestore restorer;
    std::vector<std::unique_ptr<NEO::Device>> devices;

    const uint32_t numRootDevices = 3u;
    const uint32_t numSupportedRootDevices = 2u;
};

TEST_F(DriverTestMultipleFamilySupport, whenInitializingDriverWithArrayOfDevicesThenDriverIsInitializedOnlyWithThoseSupported) {
    auto driverHandle = DriverHandle::create(std::move(devices), L0EnvVariables{});
    EXPECT_NE(nullptr, driverHandle);

    L0::DriverHandleImp *driverHandleImp = reinterpret_cast<L0::DriverHandleImp *>(driverHandle);
    EXPECT_EQ(numSupportedRootDevices, driverHandleImp->devices.size());

    for (auto d : driverHandleImp->devices) {
        EXPECT_TRUE(d->getNEODevice()->getHardwareInfo().capabilityTable.levelZeroSupported);
    }

    delete driverHandle;
    L0::GlobalDriver = nullptr;
}

struct DriverTestMultipleFamilyNoSupport : public ::testing::Test {
    void SetUp() override {
        VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
        }

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), executionEnvironment, i)));
            devices[i]->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.levelZeroSupported = false;
        }
    }

    DebugManagerStateRestore restorer;
    std::vector<std::unique_ptr<NEO::Device>> devices;

    const uint32_t numRootDevices = 3u;
};

TEST_F(DriverTestMultipleFamilyNoSupport, whenInitializingDriverWithArrayOfNotSupportedDevicesThenDriverIsNull) {
    auto driverHandle = DriverHandle::create(std::move(devices), L0EnvVariables{});
    EXPECT_EQ(nullptr, driverHandle);
}

struct LegacyMaskArray {
    const std::string masks[16] = {"0", "1", "2", "3", "4", "5", "6", "7",
                                   "8", "9", "A", "B", "C", "D", "E", "F"};
};
struct DriverTestMultipleDeviceWithLegacyAffinityMask : public ::testing::WithParamInterface<std::tuple<std::string, std::string>>,
                                                        public ::testing::Test {
    void SetUp() override {
        DebugManager.flags.UseLegacyLevelZeroAffinity.set(true);
        DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
        }

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(
                NEO::defaultHwInfo.get(),
                executionEnvironment, i)));
        }
    }

    void getNumOfExposedDevices(uint32_t mask, uint32_t &rootDeviceExposed, uint32_t &numOfSubDevicesExposed) {
        rootDeviceExposed = (((1UL << numSubDevices) - 1) & mask) ? 1 : 0;
        numOfSubDevicesExposed = static_cast<uint32_t>(static_cast<std::bitset<sizeof(uint32_t) * 8>>(mask).count());
    }

    DebugManagerStateRestore restorer;
    std::vector<std::unique_ptr<NEO::Device>> devices;

    const uint32_t numRootDevices = 2u;
    const uint32_t numSubDevices = 4u;
};

TEST_F(DriverTestMultipleDeviceWithLegacyAffinityMask,
       whenNotSettingAffinityThenAllRootDevicesAndSubDevicesAreExposed) {
    L0::DriverHandleImp *driverHandle = new DriverHandleImp;

    ze_result_t res = driverHandle->initialize(std::move(devices));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    uint32_t deviceCount = 0;
    res = zeDeviceGet(driverHandle->toHandle(), &deviceCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(deviceCount, numRootDevices);

    std::vector<ze_device_handle_t> phDevices(deviceCount);
    res = zeDeviceGet(driverHandle->toHandle(), &deviceCount, phDevices.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (uint32_t i = 0; i < numRootDevices; i++) {
        ze_device_handle_t hDevice = phDevices[i];
        EXPECT_NE(nullptr, hDevice);

        uint32_t subDeviceCount = 0;
        res = zeDeviceGetSubDevices(hDevice, &subDeviceCount, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        EXPECT_EQ(numSubDevices, subDeviceCount);
    }

    delete driverHandle;
    L0::GlobalDriver = nullptr;
}

TEST_P(DriverTestMultipleDeviceWithLegacyAffinityMask,
       whenSettingAffinityMaskToDifferentValuesThenCorrectNumberOfDevicesIsExposed) {
    L0::DriverHandleImp *driverHandle = new DriverHandleImp;

    std::string device0MaskString = std::get<0>(GetParam());
    std::string device1MaskString = std::get<1>(GetParam());

    uint32_t device0Mask = static_cast<uint32_t>(strtoul(device0MaskString.c_str(), nullptr, 16));
    uint32_t rootDevice0Exposed = 0;
    uint32_t numOfSubDevicesExposedInDevice0 = 0;
    getNumOfExposedDevices(device0Mask, rootDevice0Exposed, numOfSubDevicesExposedInDevice0);
    uint32_t device1Mask = static_cast<uint32_t>(strtoul(device1MaskString.c_str(), nullptr, 16));
    uint32_t rootDevice1Exposed = 0;
    uint32_t numOfSubDevicesExposedInDevice1 = 0;
    getNumOfExposedDevices(device1Mask, rootDevice1Exposed, numOfSubDevicesExposedInDevice1);

    driverHandle->affinityMaskString = device1MaskString + device0MaskString;

    uint32_t totalRootDevices = rootDevice0Exposed + rootDevice1Exposed;
    ze_result_t res = driverHandle->initialize(std::move(devices));
    if (0 == totalRootDevices) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, res);
    } else {
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    }
    uint32_t deviceCount = 0;
    res = zeDeviceGet(driverHandle->toHandle(), &deviceCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(deviceCount, totalRootDevices);

    if (deviceCount) {
        std::vector<ze_device_handle_t> phDevices(deviceCount);
        res = zeDeviceGet(driverHandle->toHandle(), &deviceCount, phDevices.data());
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        for (uint32_t i = 0; i < deviceCount; i++) {
            ze_device_handle_t hDevice = phDevices[i];
            EXPECT_NE(nullptr, hDevice);

            uint32_t subDeviceCount = 0;
            res = zeDeviceGetSubDevices(hDevice, &subDeviceCount, nullptr);
            EXPECT_EQ(ZE_RESULT_SUCCESS, res);
            if (rootDevice0Exposed && !rootDevice1Exposed) {
                EXPECT_EQ(numOfSubDevicesExposedInDevice0, subDeviceCount);
            } else if (!rootDevice0Exposed && rootDevice1Exposed) {
                EXPECT_EQ(numOfSubDevicesExposedInDevice1, subDeviceCount);
            } else {
                if (i == 0) {
                    EXPECT_EQ(numOfSubDevicesExposedInDevice0, subDeviceCount);
                } else {
                    EXPECT_EQ(numOfSubDevicesExposedInDevice1, subDeviceCount);
                }
            }
        }
    }

    delete driverHandle;
    L0::GlobalDriver = nullptr;
}

LegacyMaskArray legacyMaskArray;
INSTANTIATE_TEST_SUITE_P(DriverTestMultipleDeviceWithLegacyAffinityMaskTests,
                         DriverTestMultipleDeviceWithLegacyAffinityMask,
                         ::testing::Combine(
                             ::testing::ValuesIn(legacyMaskArray.masks),
                             ::testing::ValuesIn(legacyMaskArray.masks)));

struct MaskArray {
    const std::string masks[4] = {"0", "1", "2", "3"}; // fixture has 4 subDevices
};

struct DriverTestMultipleDeviceWithAffinityMask : public ::testing::WithParamInterface<std::tuple<std::string, std::string>>,
                                                  public ::testing::Test {
    void SetUp() override {
        DebugManager.flags.UseLegacyLevelZeroAffinity.set(false);
        DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
        }

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(
                NEO::defaultHwInfo.get(),
                executionEnvironment, i)));
        }
    }

    DebugManagerStateRestore restorer;
    std::vector<std::unique_ptr<NEO::Device>> devices;

    const uint32_t numRootDevices = 2u;
    const uint32_t numSubDevices = 4u;
};

TEST_P(DriverTestMultipleDeviceWithAffinityMask,
       whenSettingAffinityMaskToRetrieveOneSubDeviceOnEachDeviceThenCorrectDevicesAreExposed) {
    L0::DriverHandleImp *driverHandle = new DriverHandleImp;

    std::string subDevice0String = std::get<0>(GetParam());
    uint32_t subDevice0Index = std::stoi(subDevice0String, nullptr, 0);

    std::string subDevice1String = std::get<1>(GetParam());
    uint32_t subDevice1Index = std::stoi(subDevice1String, nullptr, 0);

    constexpr uint32_t totalRootDevices = 2;

    driverHandle->affinityMaskString = "0." + subDevice0String + "," + "1." + subDevice1String;

    ze_result_t res = driverHandle->initialize(std::move(devices));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint32_t deviceCount = 0;
    res = zeDeviceGet(driverHandle->toHandle(), &deviceCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(deviceCount, totalRootDevices);

    std::vector<ze_device_handle_t> phDevices(deviceCount);
    res = zeDeviceGet(driverHandle->toHandle(), &deviceCount, phDevices.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (uint32_t i = 0; i < deviceCount; i++) {
        ze_device_handle_t hDevice = phDevices[i];
        EXPECT_NE(nullptr, hDevice);

        DeviceImp *device = reinterpret_cast<DeviceImp *>(L0::Device::fromHandle(hDevice));

        uint32_t subDeviceCount = 0;
        res = zeDeviceGetSubDevices(hDevice, &subDeviceCount, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        EXPECT_EQ(1u, subDeviceCount);

        ze_device_handle_t hSubDevice;
        res = zeDeviceGetSubDevices(hDevice, &subDeviceCount, &hSubDevice);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        DeviceImp *subDevice = reinterpret_cast<DeviceImp *>(L0::Device::fromHandle(hSubDevice));

        if (i == 0) {
            EXPECT_EQ(subDevice->neoDevice, device->neoDevice->getDeviceById(subDevice0Index));
        } else {
            EXPECT_EQ(subDevice->neoDevice, device->neoDevice->getDeviceById(subDevice1Index));
        }
    }

    delete driverHandle;
    L0::GlobalDriver = nullptr;
}

TEST_P(DriverTestMultipleDeviceWithAffinityMask,
       whenSettingAffinityMaskToRetrieveAllDevicesInOneDeviceAndOneSubDeviceInOtherThenCorrectDevicesAreExposed) {
    L0::DriverHandleImp *driverHandle = new DriverHandleImp;

    std::string subDevice1String = std::get<1>(GetParam());
    uint32_t subDevice1Index = std::stoi(subDevice1String, nullptr, 0);

    constexpr uint32_t totalRootDevices = 2;

    driverHandle->affinityMaskString = "0,1." + subDevice1String;

    ze_result_t res = driverHandle->initialize(std::move(devices));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint32_t deviceCount = 0;
    res = zeDeviceGet(driverHandle->toHandle(), &deviceCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(deviceCount, totalRootDevices);

    std::vector<ze_device_handle_t> phDevices(deviceCount);
    res = zeDeviceGet(driverHandle->toHandle(), &deviceCount, phDevices.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (uint32_t i = 0; i < deviceCount; i++) {
        ze_device_handle_t hDevice = phDevices[i];
        EXPECT_NE(nullptr, hDevice);

        DeviceImp *device = reinterpret_cast<DeviceImp *>(L0::Device::fromHandle(hDevice));

        uint32_t subDeviceCount = 0;
        res = zeDeviceGetSubDevices(hDevice, &subDeviceCount, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        if (i == 0) {
            EXPECT_EQ(4u, subDeviceCount);
        } else {
            ze_device_handle_t hSubDevice;
            res = zeDeviceGetSubDevices(hDevice, &subDeviceCount, &hSubDevice);
            EXPECT_EQ(ZE_RESULT_SUCCESS, res);

            DeviceImp *subDevice = reinterpret_cast<DeviceImp *>(L0::Device::fromHandle(hSubDevice));

            EXPECT_EQ(1u, subDeviceCount);
            EXPECT_EQ(subDevice->neoDevice, device->neoDevice->getDeviceById(subDevice1Index));
        }
    }

    delete driverHandle;
    L0::GlobalDriver = nullptr;
}

MaskArray maskArray;
INSTANTIATE_TEST_SUITE_P(DriverTestMultipleDeviceWithAffinityMaskTests,
                         DriverTestMultipleDeviceWithAffinityMask,
                         ::testing::Combine(
                             ::testing::ValuesIn(maskArray.masks),
                             ::testing::ValuesIn(maskArray.masks)));

TEST_F(DriverTestMultipleDeviceWithAffinityMask,
       whenSettingAffinityMaskWithDeviceLargerThanAvailableDevicesThenRootDeviceValueIsIgnored) {
    L0::DriverHandleImp *driverHandle = new DriverHandleImp;

    constexpr uint32_t totalRootDevices = 2;
    uint32_t subDevice1Index = 0;
    driverHandle->affinityMaskString = "0,23,1." + std::to_string(subDevice1Index);

    ze_result_t res = driverHandle->initialize(std::move(devices));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint32_t deviceCount = 0;
    res = zeDeviceGet(driverHandle->toHandle(), &deviceCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(deviceCount, totalRootDevices);

    std::vector<ze_device_handle_t> phDevices(deviceCount);
    res = zeDeviceGet(driverHandle->toHandle(), &deviceCount, phDevices.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (uint32_t i = 0; i < deviceCount; i++) {
        ze_device_handle_t hDevice = phDevices[i];
        EXPECT_NE(nullptr, hDevice);

        DeviceImp *device = reinterpret_cast<DeviceImp *>(L0::Device::fromHandle(hDevice));

        uint32_t subDeviceCount = 0;
        res = zeDeviceGetSubDevices(hDevice, &subDeviceCount, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        if (i == 0) {
            EXPECT_EQ(4u, subDeviceCount);
        } else {
            ze_device_handle_t hSubDevice;
            res = zeDeviceGetSubDevices(hDevice, &subDeviceCount, &hSubDevice);
            EXPECT_EQ(ZE_RESULT_SUCCESS, res);

            DeviceImp *subDevice = reinterpret_cast<DeviceImp *>(L0::Device::fromHandle(hSubDevice));

            EXPECT_EQ(1u, subDeviceCount);
            EXPECT_EQ(subDevice->neoDevice, device->neoDevice->getDeviceById(subDevice1Index));
        }
    }

    delete driverHandle;
    L0::GlobalDriver = nullptr;
}

TEST_F(DriverTestMultipleDeviceWithAffinityMask,
       whenSettingAffinityMaskWithSubDeviceLargerThanAvailableSubDevicesThenSubDeviceValueIsIgnored) {
    L0::DriverHandleImp *driverHandle = new DriverHandleImp;

    driverHandle->affinityMaskString = "0,1.77";

    ze_result_t res = driverHandle->initialize(std::move(devices));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint32_t deviceCount = 0;
    res = zeDeviceGet(driverHandle->toHandle(), &deviceCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(deviceCount, 1u);

    std::vector<ze_device_handle_t> phDevices(deviceCount);
    res = zeDeviceGet(driverHandle->toHandle(), &deviceCount, phDevices.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_device_handle_t hDevice = phDevices[0];
    EXPECT_NE(nullptr, hDevice);

    uint32_t subDeviceCount = 0;
    res = zeDeviceGetSubDevices(hDevice, &subDeviceCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(4u, subDeviceCount);

    delete driverHandle;
    L0::GlobalDriver = nullptr;
}

} // namespace ult
} // namespace L0
