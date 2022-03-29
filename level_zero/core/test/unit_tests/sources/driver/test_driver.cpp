/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

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

TEST(zeInit, whenCallingZeInitWithNoFlagsThenInitializeOnDriverIsCalled) {
    Mock<Driver> driver;

    auto result = zeInit(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, driver.initCalledCount);
}

TEST(zeInit, whenCallingZeInitWithoutGpuOnlyFlagThenInitializeOnDriverIsNotCalled) {
    Mock<Driver> driver;

    auto result = zeInit(ZE_INIT_FLAG_VPU_ONLY);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
    EXPECT_EQ(0u, driver.initCalledCount);
}

using DriverHandleImpTest = Test<DeviceFixture>;
TEST_F(DriverHandleImpTest, givenDriverImpWhenCallingupdateRootDeviceBitFieldsThendeviceBitfieldsAreUpdatedInAccordanceWithNeoDevice) {
    auto hwInfo = *NEO::defaultHwInfo;
    auto newNeoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    driverHandle->updateRootDeviceBitFields(newNeoDevice);
    const auto rootDeviceIndex = neoDevice->getRootDeviceIndex();
    auto entry = driverHandle->deviceBitfields.find(rootDeviceIndex);
    EXPECT_EQ(newNeoDevice->getDeviceBitfield(), entry->second);
}

using DriverVersionTest = Test<DeviceFixture>;

TEST_F(DriverVersionTest, givenCallToGetExtensionPropertiesThenSupportedExtensionsAreReturned) {
    uint32_t count = 0;
    ze_result_t res = driverHandle->getExtensionProperties(&count, nullptr);
    EXPECT_EQ(count, static_cast<uint32_t>(driverHandle->extensionsSupported.size()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_driver_extension_properties_t *extensionProperties = new ze_driver_extension_properties_t[count];
    count++;
    res = driverHandle->getExtensionProperties(&count, extensionProperties);
    EXPECT_EQ(count, static_cast<uint32_t>(driverHandle->extensionsSupported.size()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(driverHandle.get());
    for (uint32_t i = 0; i < count; i++) {
        auto extension = extensionProperties[i];
        EXPECT_EQ(0, strcmp(extension.name, driverHandleImp->extensionsSupported[i].first.c_str()));
        EXPECT_EQ(extension.version, driverHandleImp->extensionsSupported[i].second);
    }

    delete[] extensionProperties;
}

TEST_F(DriverVersionTest, WhenGettingDriverVersionThenExpectedDriverVersionIsReturned) {
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

TEST_F(DriverVersionTest, givenCallToGetDriverPropertiesThenUuidIsSet) {
    NEO::MockCompilerEnableGuard mock(true);
    ze_driver_properties_t properties;
    ze_result_t res = driverHandle->getProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint64_t uuid = 0u;
    memcpy_s(&uuid, sizeof(uuid), properties.uuid.id, sizeof(uuid));

    uint32_t uniqueId = static_cast<uint32_t>((uuid & 0xFFFFFFFF00000000) >> 32);
    uint32_t versionMajor = static_cast<uint32_t>((uuid & 0xFF000000) >> 24);
    uint32_t versionMinor = static_cast<uint32_t>((uuid & 0x00FF0000) >> 16);
    uint32_t versionBuild = static_cast<uint32_t>(uuid & 0x0000FFFF);

    EXPECT_NE(0u, uniqueId);
    EXPECT_EQ(static_cast<uint32_t>(strtoul(L0_PROJECT_VERSION_MAJOR, NULL, 10)), versionMajor);
    EXPECT_EQ(static_cast<uint32_t>(strtoul(L0_PROJECT_VERSION_MINOR, NULL, 10)), versionMinor);
    EXPECT_EQ(static_cast<uint32_t>(strtoul(NEO_VERSION_BUILD, NULL, 10)), versionBuild);
}

TEST_F(DriverVersionTest, whenCallingGetDriverPropertiesRepeatedlyThenTheSameUuidIsReturned) {
    NEO::MockCompilerEnableGuard mock(true);
    ze_driver_properties_t properties;
    ze_result_t res = driverHandle->getProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint64_t uuid = 0u;
    memcpy_s(&uuid, sizeof(uuid), properties.uuid.id, sizeof(uuid));

    uint32_t uniqueId = static_cast<uint32_t>((uuid & 0xFFFFFFFF00000000) >> 32);
    uint32_t versionMajor = static_cast<uint32_t>((uuid & 0xFF000000) >> 24);
    uint32_t versionMinor = static_cast<uint32_t>((uuid & 0x00FF0000) >> 16);
    uint32_t versionBuild = static_cast<uint32_t>(uuid & 0x0000FFFF);

    EXPECT_NE(0u, uniqueId);
    EXPECT_EQ(static_cast<uint32_t>(strtoul(L0_PROJECT_VERSION_MAJOR, NULL, 10)), versionMajor);
    EXPECT_EQ(static_cast<uint32_t>(strtoul(L0_PROJECT_VERSION_MINOR, NULL, 10)), versionMinor);
    EXPECT_EQ(static_cast<uint32_t>(strtoul(NEO_VERSION_BUILD, NULL, 10)), versionBuild);

    for (uint32_t i = 0; i < 32; i++) {
        ze_driver_properties_t newProperties;
        res = driverHandle->getProperties(&newProperties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        EXPECT_EQ(0, memcmp(properties.uuid.id, newProperties.uuid.id, sizeof(uint64_t)));
    }
}

using ImportNTHandle = Test<DeviceFixture>;

class MemoryManagerNTHandleMock : public NEO::OsAgnosticMemoryManager {
  public:
    MemoryManagerNTHandleMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::OsAgnosticMemoryManager(executionEnvironment) {}

    NEO::GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) override {
        auto graphicsAllocation = createMemoryAllocation(allocType, nullptr, reinterpret_cast<void *>(1), 1,
                                                         4096u, reinterpret_cast<uint64_t>(handle), MemoryPool::SystemCpuInaccessible,
                                                         rootDeviceIndex, false, false, false);
        graphicsAllocation->setSharedHandle(static_cast<osHandle>(reinterpret_cast<uint64_t>(handle)));
        graphicsAllocation->set32BitAllocation(false);
        graphicsAllocation->setDefaultGmm(new Gmm(executionEnvironment.rootDeviceEnvironments[0]->getGmmClientContext(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
        return graphicsAllocation;
    }
};

HWTEST_F(ImportNTHandle, givenNTHandleWhenCreatingDeviceMemoryThenSuccessIsReturned) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_device_mem_alloc_desc_t devProperties = {};
    devProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES;

    uint64_t imageHandle = 0x1;
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    devProperties.pNext = &importNTHandle;

    NEO::MockDevice *neoDevice = nullptr;
    auto executionEnvironment = NEO::MockDevice::prepareExecutionEnvironment(NEO::defaultHwInfo.get(), 0);
    executionEnvironment->memoryManager.reset(new MemoryManagerNTHandleMock(*executionEnvironment));

    neoDevice = NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), executionEnvironment, 0);

    driverHandle->setMemoryManager(executionEnvironment->memoryManager.get());

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto device = L0::Device::create(driverHandle.get(), neoDevice, false, &result);

    context->addDeviceAndSubDevices(device);

    void *ptr;

    result = context->allocDeviceMem(device, &devProperties, 100, 1, &ptr);

    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    auto alloc = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);

    ASSERT_EQ(alloc->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex())->peekSharedHandle(), NEO::toOsHandle(importNTHandle.handle));
    result = context->freeMem(ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    delete device;
}

HWTEST_F(ImportNTHandle, givenNotExistingNTHandleWhenCreatingDeviceMemoryThenErrorIsReturned) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_device_mem_alloc_desc_t devProperties = {};
    devProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES;

    uint64_t imageHandle = 0x1;
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    devProperties.pNext = &importNTHandle;

    void *ptr;

    auto result = context->allocDeviceMem(device, &devProperties, 100, 1, &ptr);

    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST(DriverTestFamilySupport, whenInitializingDriverOnSupportedFamilyThenDriverIsCreated) {
    NEO::MockCompilerEnableGuard mock(true);
    ze_result_t returnValue;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    auto driverHandle = DriverHandle::create(std::move(devices), L0EnvVariables{}, &returnValue);
    EXPECT_NE(nullptr, driverHandle);
    delete driverHandle;
    L0::GlobalDriver = nullptr;
}

TEST(DriverTestFamilySupport, whenInitializingDriverOnNotSupportedFamilyThenDriverIsNotCreated) {
    ze_result_t returnValue;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = false;

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    auto driverHandle = DriverHandle::create(std::move(devices), L0EnvVariables{}, &returnValue);
    EXPECT_EQ(nullptr, driverHandle);
}

TEST(DriverTest, givenNullEnvVariableWhenCreatingDriverThenEnableProgramDebuggingIsFalse) {
    NEO::MockCompilerEnableGuard mock(true);
    ze_result_t returnValue;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    L0EnvVariables envVariables = {};
    envVariables.programDebugging = false;

    auto driverHandle = whitebox_cast(DriverHandle::create(std::move(devices), envVariables, &returnValue));
    EXPECT_NE(nullptr, driverHandle);

    EXPECT_FALSE(driverHandle->enableProgramDebugging);

    delete driverHandle;
    L0::GlobalDriver = nullptr;
}

TEST(DriverImpTest, givenDriverImpWhenInitializedThenEnvVariablesAreRead) {
    NEO::MockCompilerEnableGuard mock(true);
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

TEST(DriverImpTest, givenMissingMetricApiDependenciesWhenInitializingDriverImpThenGlobalDriverHandleIsNull) {
    NEO::MockCompilerEnableGuard mock(true);
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

TEST(DriverImpTest, givenEnabledProgramDebuggingWhenCreatingExecutionEnvironmentThenDebuggingEnabledIsTrue) {
    NEO::MockCompilerEnableGuard mock(true);
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
    EXPECT_TRUE(L0::GlobalDriver->devices[0]->getNEODevice()->getExecutionEnvironment()->isDebuggingEnabled());

    delete L0::GlobalDriver;
    L0::GlobalDriverHandle = nullptr;
    L0::GlobalDriver = nullptr;
}

TEST(DriverImpTest, givenNoProgramDebuggingEnvVarWhenCreatingExecutionEnvironmentThenDebuggingEnabledIsFalse) {
    NEO::MockCompilerEnableGuard mock(true);
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_NE(nullptr, L0::GlobalDriver);
    ASSERT_NE(0u, L0::GlobalDriver->numDevices);
    EXPECT_FALSE(L0::GlobalDriver->devices[0]->getNEODevice()->getExecutionEnvironment()->isDebuggingEnabled());

    delete L0::GlobalDriver;
    L0::GlobalDriverHandle = nullptr;
    L0::GlobalDriver = nullptr;
}

TEST(DriverTest, givenProgramDebuggingEnvVarNonZeroWhenCreatingDriverThenEnableProgramDebuggingIsSetTrue) {
    NEO::MockCompilerEnableGuard mock(true);
    ze_result_t returnValue;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    L0EnvVariables envVariables = {};
    envVariables.programDebugging = true;

    auto driverHandle = whitebox_cast(DriverHandle::create(std::move(devices), envVariables, &returnValue));
    EXPECT_NE(nullptr, driverHandle);

    EXPECT_TRUE(driverHandle->enableProgramDebugging);

    delete driverHandle;
    L0::GlobalDriver = nullptr;
}

TEST(DriverTest, givenInvalidCompilerEnvironmentThenDependencyUnavailableErrorIsReturned) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    auto oldFclDllName = Os::frontEndDllName;
    auto oldIgcDllName = Os::igcDllName;
    Os::frontEndDllName = "_invalidFCL";
    Os::igcDllName = "_invalidIGC";
    driverImp.initialize(&result);
    EXPECT_EQ(result, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);

    Os::igcDllName = oldIgcDllName;
    Os::frontEndDllName = oldFclDllName;

    ASSERT_EQ(nullptr, L0::GlobalDriver);
}

struct DriverTestMultipleFamilySupport : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices);
        for (auto i = 0u; i < numRootDevices; i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
            if (i < numSupportedRootDevices) {
                devices[i]->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.levelZeroSupported = true;
            } else {
                deviceFactory->rootDevices.erase(deviceFactory->rootDevices.begin() + i);
                devices[i]->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.levelZeroSupported = false;
            }
        }
    }
    DebugManagerStateRestore restorer;
    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    const uint32_t numRootDevices = 3u;
    const uint32_t numSubDevices = 2u;
    const uint32_t numSupportedRootDevices = 2u;
};

TEST_F(DriverTestMultipleFamilySupport, whenInitializingDriverWithArrayOfDevicesThenDriverIsInitializedOnlyWithThoseSupported) {
    NEO::MockCompilerEnableGuard mock(true);
    ze_result_t returnValue;
    auto driverHandle = DriverHandle::create(std::move(devices), L0EnvVariables{}, &returnValue);
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
        NEO::MockCompilerEnableGuard mock(true);
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
    NEO::MockCompilerEnableGuard mock(true);
    ze_result_t returnValue;
    auto driverHandle = DriverHandle::create(std::move(devices), L0EnvVariables{}, &returnValue);
    EXPECT_EQ(nullptr, driverHandle);
}

struct MaskArray {
    const std::string masks[4] = {"0", "1", "2", "3"}; // fixture has 4 subDevices
};

struct DriverHandleTest : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        ze_result_t returnValue;
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        hwInfo.capabilityTable.levelZeroSupported = true;

        NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

        L0EnvVariables envVariables = {};
        envVariables.programDebugging = true;

        driverHandle = whitebox_cast(DriverHandle::create(std::move(devices), envVariables, &returnValue));
        L0::GlobalDriverHandle = driverHandle;
    }
    void TearDown() override {
        delete driverHandle;
        L0::GlobalDriver = nullptr;
        L0::GlobalDriverHandle = nullptr;
    }
    L0::DriverHandle *driverHandle;
};

TEST_F(DriverHandleTest, givenInitializedDriverWhenZeDriverGetIsCalledThenDriverHandleCountIsObtained) {
    uint32_t count = 0;
    auto result = zeDriverGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1U, count);
}

TEST_F(DriverHandleTest,
       givenInitializedDriverWhenZeDriverGetIsCalledWithGreaterThanCountAvailableThenCorrectCountIsReturned) {
    uint32_t count = 0;
    ze_result_t result = zeDriverGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1U, count);

    count++;
    ze_driver_handle_t driverHandle = {};
    result = zeDriverGet(&count, &driverHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1U, count);
    EXPECT_NE(nullptr, driverHandle);
}

TEST_F(DriverHandleTest,
       givenInitializedDriverWhenZeDriverGetIsCalledWithGreaterThanZeroCountAndNullDriverHandleThenInvalidNullPointerIsReturned) {
    uint32_t count = 0;
    ze_result_t result = zeDriverGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1U, count);

    result = zeDriverGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, result);
}

TEST_F(DriverHandleTest, givenInitializedDriverWhenZeDriverGetIsCalledThenDriverHandleIsObtained) {
    ze_result_t result;
    uint32_t count = 0;
    result = zeDriverGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1U, count);

    ze_driver_handle_t *phDriverHandles = new ze_driver_handle_t[count];

    result = zeDriverGet(&count, phDriverHandles);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(driverHandle->toHandle(), phDriverHandles[0]);
    delete[] phDriverHandles;
}

TEST_F(DriverHandleTest, givenInitializedDriverWhenZeDriverGetIsCalledThenGlobalDriverHandleIsObtained) {
    ze_result_t result;

    uint32_t count = 1;
    ze_driver_handle_t hDriverHandle = reinterpret_cast<ze_driver_handle_t>(&hDriverHandle);

    result = zeDriverGet(&count, &hDriverHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, hDriverHandle);
    EXPECT_EQ(hDriverHandle, GlobalDriver);
}

TEST_F(DriverHandleTest, givenInitializedDriverWhenGetDeviceIsCalledThenOneDeviceIsObtained) {
    ze_result_t result;
    uint32_t count = 1;

    ze_device_handle_t device;
    result = driverHandle->getDevice(&count, &device);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, &device);
}

TEST_F(DriverHandleTest, whenQueryingForApiVersionThenExpectedVersionIsReturned) {
    ze_api_version_t version = {};
    ze_result_t result = driverHandle->getApiVersion(&version);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZE_API_VERSION_1_3, version);
}

TEST_F(DriverHandleTest, whenQueryingForDevicesWithCountGreaterThanZeroAndNullDevicePointerThenNullHandleIsReturned) {
    uint32_t count = 1;
    ze_result_t result = driverHandle->getDevice(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_HANDLE, result);
}

TEST_F(DriverHandleTest, givenValidDriverHandleWhenGetSvmAllocManagerIsCalledThenSvmAllocsManagerIsObtained) {
    auto svmAllocsManager = driverHandle->getSvmAllocsManager();
    EXPECT_NE(nullptr, svmAllocsManager);
}

TEST(zeDriverHandleGetProperties, whenZeDriverGetPropertiesIsCalledThenGetPropertiesIsCalled) {
    ze_result_t result;
    Mock<DriverHandle> driverHandle;
    ze_driver_properties_t properties;
    ze_result_t expectedResult = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;

    driverHandle.getPropertiesResult = expectedResult;
    result = zeDriverGetProperties(driverHandle.toHandle(), &properties);
    EXPECT_EQ(expectedResult, result);
    EXPECT_EQ(1u, driverHandle.getPropertiesCalled);
}

TEST(zeDriverHandleGetApiVersion, whenZeDriverGetApiIsCalledThenGetApiVersionIsCalled) {
    ze_result_t result;
    Mock<DriverHandle> driverHandle;
    ze_api_version_t version;
    ze_result_t expectedResult = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;

    driverHandle.getApiVersionResult = expectedResult;
    result = zeDriverGetApiVersion(driverHandle.toHandle(), &version);
    EXPECT_EQ(expectedResult, result);
    EXPECT_EQ(1u, driverHandle.getApiVersionCalled);
}

TEST(zeDriverGetIpcProperties, whenZeDriverGetIpcPropertiesIsCalledThenGetIPCPropertiesIsCalled) {
    ze_result_t result;
    Mock<DriverHandle> driverHandle;
    ze_driver_ipc_properties_t ipcProperties;
    ze_result_t expectedResult = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;

    driverHandle.getIPCPropertiesResult = expectedResult;
    result = zeDriverGetIpcProperties(driverHandle.toHandle(), &ipcProperties);
    EXPECT_EQ(expectedResult, result);
    EXPECT_EQ(1u, driverHandle.getIPCPropertiesCalled);
}
} // namespace ult
} // namespace L0
