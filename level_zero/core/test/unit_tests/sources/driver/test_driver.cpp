/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/driver_experimental/public/zex_api.h"
#include "level_zero/api/driver_experimental/public/zex_driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/host_pointer_manager_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include <bitset>

namespace L0 {
namespace ult {

TEST(zeInit, whenCallingZeInitThenInitializeOnDriverIsCalled) {
    Mock<Driver> driver;

    auto result = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, driver.initCalledCount);
}

TEST(zeInit, whenCallingZeInitThenLevelZeroDriverInitializedIsSetToTrue) {
    Mock<Driver> driver;

    auto result = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(LevelZeroDriverInitialized);
    EXPECT_EQ(1u, driver.initCalledCount);
}

TEST(zeInit, whenCallingZeInitWithFailureInIinitThenLevelZeroDriverInitializedIsSetToFalse) {
    Mock<Driver> driver;
    driver.failInitDriver = true;

    auto result = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
    EXPECT_FALSE(LevelZeroDriverInitialized);
}

TEST(zeInit, whenCallingZeInitWithVpuOnlyThenLevelZeroDriverInitializedIsSetToFalse) {
    Mock<Driver> driver;

    auto result = zeInit(ZE_INIT_FLAG_VPU_ONLY);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
    EXPECT_FALSE(LevelZeroDriverInitialized);
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
TEST_F(DriverVersionTest, givenSupportedExtensionsWhenCheckIfDeviceIpVersionIsSupportedThenCorrectResultsAreReturned) {
    auto &supportedExt = driverHandle->extensionsSupported;
    auto it = std::find_if(supportedExt.begin(), supportedExt.end(), [](const auto &supportedExt) { return supportedExt.first == ZE_DEVICE_IP_VERSION_EXT_NAME; });
    EXPECT_NE(it, supportedExt.end());
    EXPECT_EQ(it->second, ZE_DEVICE_IP_VERSION_VERSION_CURRENT);
}

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

    auto expectedDriverVersion = static_cast<uint32_t>(DriverHandleImp::initialDriverVersionValue + strtoul(NEO_VERSION_BUILD, NULL, 10));
    EXPECT_EQ(expectedDriverVersion, properties.driverVersion);
}

TEST_F(DriverVersionTest, GivenDebugOverrideWhenGettingDriverVersionThenExpectedOverrideDriverVersionIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.OverrideDriverVersion.set(0);

    ze_driver_properties_t properties;
    ze_result_t res = driverHandle->getProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint32_t expectedDriverVersion = 0;
    EXPECT_EQ(expectedDriverVersion, properties.driverVersion);

    NEO::DebugManager.flags.OverrideDriverVersion.set(10);

    res = driverHandle->getProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    expectedDriverVersion = 10;
    EXPECT_EQ(expectedDriverVersion, properties.driverVersion);

    NEO::DebugManager.flags.OverrideDriverVersion.set(DriverHandleImp::initialDriverVersionValue + 20);

    res = driverHandle->getProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    expectedDriverVersion = DriverHandleImp::initialDriverVersionValue + 20;
    EXPECT_EQ(expectedDriverVersion, properties.driverVersion);
}

TEST_F(DriverVersionTest, givenCallToGetDriverPropertiesThenUuidIsSet) {

    ze_driver_properties_t properties;
    ze_result_t res = driverHandle->getProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint64_t uuid = 0u;
    memcpy_s(&uuid, sizeof(uuid), properties.uuid.id, sizeof(uuid));

    auto uniqueId = static_cast<uint32_t>((uuid & 0xFFFFFFFF00000000) >> 32);
    EXPECT_NE(0u, uniqueId);

    auto driverVersion = static_cast<uint32_t>(uuid & 0xFFFFFFFF);
    auto expectedDriverVersion = static_cast<uint32_t>(DriverHandleImp::initialDriverVersionValue + strtoul(NEO_VERSION_BUILD, NULL, 10));
    EXPECT_EQ(expectedDriverVersion, driverVersion);
}

TEST_F(DriverVersionTest, whenCallingGetDriverPropertiesRepeatedlyThenTheSameUuidIsReturned) {

    ze_driver_properties_t properties;
    ze_result_t res = driverHandle->getProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

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
        GmmRequirements gmmRequirements{};
        gmmRequirements.allowLargePages = true;
        gmmRequirements.preferCompressed = false;
        graphicsAllocation->setDefaultGmm(new Gmm(executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
        return graphicsAllocation;
    }
};

HWTEST_F(ImportNTHandle, givenCallToImportNTHandleWithHostBufferMemoryAllocationTypeThenHostUnifiedMemoryIsSet) {
    delete driverHandle->svmAllocsManager;
    execEnv->memoryManager.reset(new MemoryManagerNTHandleMock(*execEnv));
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get(), false);

    uint64_t imageHandle = 0x1;
    NEO::AllocationType allocationType = NEO::AllocationType::BUFFER_HOST_MEMORY;
    void *ptr = driverHandle->importNTHandle(device->toHandle(), &imageHandle, allocationType);
    EXPECT_NE(ptr, nullptr);

    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);
    EXPECT_EQ(allocData->memoryType, InternalMemoryType::HOST_UNIFIED_MEMORY);

    ze_result_t result = context->freeMem(ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(ImportNTHandle, givenCallToImportNTHandleWithBufferMemoryAllocationTypeThenDeviceUnifiedMemoryIsSet) {
    delete driverHandle->svmAllocsManager;
    execEnv->memoryManager.reset(new MemoryManagerNTHandleMock(*execEnv));
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get(), false);

    uint64_t imageHandle = 0x1;
    NEO::AllocationType allocationType = NEO::AllocationType::BUFFER;
    void *ptr = driverHandle->importNTHandle(device->toHandle(), &imageHandle, allocationType);
    EXPECT_NE(ptr, nullptr);

    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);
    EXPECT_EQ(allocData->memoryType, InternalMemoryType::DEVICE_UNIFIED_MEMORY);

    ze_result_t result = context->freeMem(ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(ImportNTHandle, givenNTHandleWhenCreatingDeviceMemoryThenSuccessIsReturned) {
    ze_device_mem_alloc_desc_t devDesc = {};
    devDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;

    uint64_t imageHandle = 0x1;
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    devDesc.pNext = &importNTHandle;

    delete driverHandle->svmAllocsManager;
    execEnv->memoryManager.reset(new MemoryManagerNTHandleMock(*execEnv));
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get(), false);

    void *ptr;
    auto result = context->allocDeviceMem(device, &devDesc, 100, 1, &ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    auto alloc = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_EQ(alloc->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex())->peekSharedHandle(), NEO::toOsHandle(importNTHandle.handle));
    result = context->freeMem(ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(ImportNTHandle, givenNTHandleWhenCreatingHostMemoryThenSuccessIsReturned) {
    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;

    uint64_t imageHandle = 0x1;
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    hostDesc.pNext = &importNTHandle;

    delete driverHandle->svmAllocsManager;
    execEnv->memoryManager.reset(new MemoryManagerNTHandleMock(*execEnv));
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get(), false);

    void *ptr = nullptr;
    auto result = context->allocHostMem(&hostDesc, 100, 1, &ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    auto alloc = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_EQ(alloc->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex())->peekSharedHandle(), NEO::toOsHandle(importNTHandle.handle));
    result = context->freeMem(ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(ImportNTHandle, whenCallingCreateGraphicsAllocationFromMultipleSharedHandlesFromOsAgnosticMemoryManagerThenNullptrIsReturned) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    delete driverHandle->svmAllocsManager;
    execEnv->memoryManager.reset(new MemoryManagerNTHandleMock(*execEnv));
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get(), false);

    std::vector<osHandle> handles{6, 7};
    AllocationProperties properties = {device->getRootDeviceIndex(),
                                       true,
                                       MemoryConstants::pageSize,
                                       AllocationType::BUFFER,
                                       false,
                                       device->getNEODevice()->getDeviceBitfield()};
    bool requireSpecificBitness{};
    bool isHostIpcAllocation{};
    auto ptr = execEnv->memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, requireSpecificBitness, isHostIpcAllocation, true, nullptr);
    EXPECT_EQ(nullptr, ptr);
}

constexpr size_t invalidHandle = 0xffffffff;

HWTEST_F(ImportNTHandle, givenNotExistingNTHandleWhenCreatingDeviceMemoryThenErrorIsReturned) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_device_mem_alloc_desc_t devDesc = {};
    devDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;

    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = reinterpret_cast<void *>(invalidHandle);
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    devDesc.pNext = &importNTHandle;

    void *ptr = nullptr;

    auto result = context->allocDeviceMem(device, &devDesc, 100, 1, &ptr);

    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

HWTEST_F(ImportNTHandle, givenNotExistingNTHandleWhenCreatingHostMemoryThenErrorIsReturned) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;

    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = reinterpret_cast<void *>(invalidHandle);
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    hostDesc.pNext = &importNTHandle;

    void *ptr = nullptr;
    auto result = context->allocHostMem(&hostDesc, 100, 1, &ptr);

    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST(DriverTestFamilySupport, whenInitializingDriverOnSupportedFamilyThenDriverIsCreated) {

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

    ze_result_t returnValue;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    L0EnvVariables envVariables = {};
    envVariables.programDebugging = false;

    auto driverHandle = whiteboxCast(static_cast<::L0::DriverHandleImp *>(DriverHandle::create(std::move(devices), envVariables, &returnValue)));
    EXPECT_NE(nullptr, driverHandle);

    EXPECT_EQ(NEO::DebuggingMode::Disabled, driverHandle->enableProgramDebugging);

    delete driverHandle;
    L0::GlobalDriver = nullptr;
}

struct DriverImpTest : public ::testing::Test {
    VariableBackup<_ze_driver_handle_t *> globalDriverHandleBackup{&GlobalDriverHandle};
    VariableBackup<uint32_t> driverCountBackup{&driverCount};
};

TEST_F(DriverImpTest, givenDriverImpWhenInitializedThenEnvVariablesAreRead) {

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

TEST_F(DriverImpTest, givenMissingMetricApiDependenciesWhenInitializingDriverImpThenGlobalDriverHandleIsNull) {

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    NEO::MockExecutionEnvironment mockExecutionEnvironment;
    const auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();

    if (productHelper.isIpSamplingSupported(hwInfo)) {
        GTEST_SKIP();
    }

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

TEST_F(DriverImpTest, givenEnabledProgramDebuggingWhenCreatingExecutionEnvironmentThenDebuggingEnabledIsTrue) {

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

TEST_F(DriverImpTest, whenCreatingExecutionEnvironmentThenDefaultHierarchyIsEnabled) {

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);
    L0::DriverHandleImp *driverHandleImp = reinterpret_cast<L0::DriverHandleImp *>(L0::GlobalDriverHandle);
    auto &gfxCoreHelper = driverHandleImp->memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    if (strcmp(gfxCoreHelper.getDefaultDeviceHierarchy(), "COMPOSITE") == 0) {
        EXPECT_EQ(driverHandleImp->deviceHierarchyMode, L0::L0DeviceHierarchyMode::L0_DEVICE_HIERARCHY_COMPOSITE);
    } else {
        EXPECT_EQ(driverHandleImp->deviceHierarchyMode, L0::L0DeviceHierarchyMode::L0_DEVICE_HIERARCHY_FLAT);
    }
    ASSERT_NE(nullptr, L0::GlobalDriver);
    ASSERT_NE(0u, L0::GlobalDriver->numDevices);

    delete L0::GlobalDriver;
    L0::GlobalDriverHandle = nullptr;
    L0::GlobalDriver = nullptr;
}

TEST_F(DriverImpTest, givenFlatDeviceHierarchyWhenCreatingExecutionEnvironmentThenFlatHierarchyIsEnabled) {

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "FLAT"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);
    L0::DriverHandleImp *driverHandleImp = reinterpret_cast<L0::DriverHandleImp *>(L0::GlobalDriverHandle);
    EXPECT_EQ(driverHandleImp->deviceHierarchyMode, L0::L0DeviceHierarchyMode::L0_DEVICE_HIERARCHY_FLAT);
    ASSERT_NE(nullptr, L0::GlobalDriver);
    ASSERT_NE(0u, L0::GlobalDriver->numDevices);

    delete L0::GlobalDriver;
    L0::GlobalDriverHandle = nullptr;
    L0::GlobalDriver = nullptr;
}

TEST_F(DriverImpTest, givenCompositeDeviceHierarchyWhenCreatingExecutionEnvironmentThenCompositeHierarchyIsEnabled) {

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "COMPOSITE"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);
    L0::DriverHandleImp *driverHandleImp = reinterpret_cast<L0::DriverHandleImp *>(L0::GlobalDriverHandle);
    EXPECT_EQ(driverHandleImp->deviceHierarchyMode, L0::L0DeviceHierarchyMode::L0_DEVICE_HIERARCHY_COMPOSITE);
    ASSERT_NE(nullptr, L0::GlobalDriver);
    ASSERT_NE(0u, L0::GlobalDriver->numDevices);

    delete L0::GlobalDriver;
    L0::GlobalDriverHandle = nullptr;
    L0::GlobalDriver = nullptr;
}

TEST_F(DriverImpTest, givenCombinedDeviceHierarchyWhenCreatingExecutionEnvironmentThenCombinedHierarchyIsEnabled) {

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "COMBINED"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);
    L0::DriverHandleImp *driverHandleImp = reinterpret_cast<L0::DriverHandleImp *>(L0::GlobalDriverHandle);
    EXPECT_EQ(driverHandleImp->deviceHierarchyMode, L0::L0DeviceHierarchyMode::L0_DEVICE_HIERARCHY_COMBINED);
    ASSERT_NE(nullptr, L0::GlobalDriver);
    ASSERT_NE(0u, L0::GlobalDriver->numDevices);

    delete L0::GlobalDriver;
    L0::GlobalDriverHandle = nullptr;
    L0::GlobalDriver = nullptr;
}

TEST_F(DriverImpTest, givenEnableProgramDebuggingWithValue2WhenCreatingExecutionEnvironmentThenDebuggingEnabledIsTrue) {

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "2"}};
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

TEST_F(DriverImpTest, givenEnabledFP64EmulationWhenCreatingExecutionEnvironmentThenFP64EmulationIsEnabled) {

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"NEO_FP64_EMULATION", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_NE(nullptr, L0::GlobalDriver);
    ASSERT_NE(0u, L0::GlobalDriver->numDevices);
    EXPECT_TRUE(L0::GlobalDriver->devices[0]->getNEODevice()->getExecutionEnvironment()->isFP64EmulationEnabled());

    delete L0::GlobalDriver;
    L0::GlobalDriverHandle = nullptr;
    L0::GlobalDriver = nullptr;
}

TEST_F(DriverImpTest, givenEnabledProgramDebuggingAndEnabledExperimentalOpenCLWhenCreatingExecutionEnvironmentThenDebuggingEnabledIsFalse) {
    DebugManagerStateRestore restorer;

    VariableBackup<_ze_driver_handle_t *> globalDriverHandleBackup{&GlobalDriverHandle};
    VariableBackup<uint32_t> driverCountBackup{&driverCount};
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    NEO::DebugManager.flags.ExperimentalEnableL0DebuggerForOpenCL.set(true);

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(L0::GlobalDriverHandle)> mockableDriverHandle(&L0::GlobalDriverHandle);
    VariableBackup<decltype(L0::GlobalDriver)> mockableDriver(&L0::GlobalDriver);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_NE(nullptr, L0::GlobalDriver);
    ASSERT_NE(0u, L0::GlobalDriver->numDevices);
    EXPECT_FALSE(L0::GlobalDriver->devices[0]->getNEODevice()->getExecutionEnvironment()->isDebuggingEnabled());

    delete L0::GlobalDriver;
}

TEST_F(DriverImpTest, givenEnableProgramDebuggingWithValue2AndEnabledExperimentalOpenCLWhenCreatingExecutionEnvironmentThenDebuggingEnabledIsFalse) {
    DebugManagerStateRestore restorer;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    NEO::DebugManager.flags.ExperimentalEnableL0DebuggerForOpenCL.set(true);

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "2"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(L0::GlobalDriverHandle)> mockableDriverHandle(&L0::GlobalDriverHandle);
    VariableBackup<decltype(L0::GlobalDriver)> mockableDriver(&L0::GlobalDriver);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_NE(nullptr, L0::GlobalDriver);
    ASSERT_NE(0u, L0::GlobalDriver->numDevices);
    EXPECT_FALSE(L0::GlobalDriver->devices[0]->getNEODevice()->getExecutionEnvironment()->isDebuggingEnabled());

    delete L0::GlobalDriver;
}

TEST_F(DriverImpTest, givenNoProgramDebuggingEnvVarWhenCreatingExecutionEnvironmentThenDebuggingEnabledIsFalse) {

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

TEST(DriverTest, givenProgramDebuggingEnvVarValue1WhenCreatingDriverThenEnableProgramDebuggingIsSetToTrue) {

    ze_result_t returnValue;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    L0EnvVariables envVariables = {};
    envVariables.programDebugging = 1;

    auto driverHandle = whiteboxCast(static_cast<::L0::DriverHandleImp *>(DriverHandle::create(std::move(devices), envVariables, &returnValue)));
    EXPECT_NE(nullptr, driverHandle);

    EXPECT_TRUE(driverHandle->enableProgramDebugging == NEO::DebuggingMode::Online);

    delete driverHandle;
    L0::GlobalDriver = nullptr;
}

TEST(DriverTest, givenProgramDebuggingEnvVarValue2WhenCreatingDriverThenEnableProgramDebuggingIsSetToTrue) {

    ze_result_t returnValue;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    L0EnvVariables envVariables = {};
    envVariables.programDebugging = 2;

    auto driverHandle = whiteboxCast(static_cast<::L0::DriverHandleImp *>(DriverHandle::create(std::move(devices), envVariables, &returnValue)));
    EXPECT_NE(nullptr, driverHandle);

    EXPECT_TRUE(driverHandle->enableProgramDebugging == NEO::DebuggingMode::Offline);

    delete driverHandle;
    L0::GlobalDriver = nullptr;
}

TEST(DriverTest, givenInvalidCompilerEnvironmentThenDependencyUnavailableErrorIsReturned) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;
    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

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

TEST(DriverTest, givenInvalidCompilerEnvironmentAndEnableProgramDebuggingWithValue2ThenDependencyUnavailableErrorIsReturned) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;
    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "2"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

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

        VariableBackup<bool> mockDeviceFlagBackup(&NEO::MockDevice::createSingleDevice, false);

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

TEST(MultiRootDeviceDriverTest, whenInitializingDriverHandleWithMultipleDevicesThenOrderInRootDeviceIndicesMatchesOrderInDeviceVector) {
    Mock<DriverHandle> driverHandle;
    for (auto &device : driverHandle.devices) {
        delete device;
    }
    driverHandle.devices.clear();

    UltDeviceFactory deviceFactory{3, 0};
    NEO::DeviceVector inputDevices;
    inputDevices.push_back(std::unique_ptr<NEO::Device>(deviceFactory.rootDevices[2]));
    inputDevices.push_back(std::unique_ptr<NEO::Device>(deviceFactory.rootDevices[0]));
    inputDevices.push_back(std::unique_ptr<NEO::Device>(deviceFactory.rootDevices[1]));

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle.initialize(std::move(inputDevices)));

    EXPECT_EQ(3u, driverHandle.devices.size());
    EXPECT_EQ(3u, driverHandle.rootDeviceIndices.size());
    EXPECT_EQ(2u, driverHandle.rootDeviceIndices[0]);
    EXPECT_EQ(0u, driverHandle.rootDeviceIndices[1]);
    EXPECT_EQ(1u, driverHandle.rootDeviceIndices[2]);
}

TEST(MultiSubDeviceDriverTest, whenInitializingDriverHandleWithMultipleDevicesWithSameRootDeviceIndexThenIndicesInRootDeviceIndicesAreNotDuplicated) {
    Mock<DriverHandle> driverHandle;
    for (auto &device : driverHandle.devices) {
        delete device;
    }
    driverHandle.devices.clear();

    UltDeviceFactory deviceFactory{1, 3};
    NEO::DeviceVector inputDevices;
    inputDevices.push_back(std::unique_ptr<NEO::Device>(deviceFactory.subDevices[2]));
    inputDevices.push_back(std::unique_ptr<NEO::Device>(deviceFactory.subDevices[0]));
    inputDevices.push_back(std::unique_ptr<NEO::Device>(deviceFactory.subDevices[1]));

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle.initialize(std::move(inputDevices)));
    EXPECT_EQ(3u, driverHandle.devices.size());
    EXPECT_EQ(1u, driverHandle.rootDeviceIndices.size());
    EXPECT_EQ(0u, driverHandle.rootDeviceIndices[0]);
}

struct DriverTestMultipleFamilyNoSupport : public ::testing::Test {
    void SetUp() override {

        VariableBackup<bool> mockDeviceFlagBackup(&NEO::MockDevice::createSingleDevice, false);

        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
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

    ze_result_t returnValue;
    auto driverHandle = DriverHandle::create(std::move(devices), L0EnvVariables{}, &returnValue);
    EXPECT_EQ(nullptr, driverHandle);
}

struct MaskArray {
    const std::string masks[4] = {"0", "1", "2", "3"}; // fixture has 4 subDevices
};

struct DriverHandleTest : public ::testing::Test {
    void SetUp() override {

        ze_result_t returnValue;
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        hwInfo.capabilityTable.levelZeroSupported = true;

        NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

        L0EnvVariables envVariables = {};
        envVariables.programDebugging = true;

        driverHandle = whiteboxCast(DriverHandle::create(std::move(devices), envVariables, &returnValue));
        L0::GlobalDriverHandle = driverHandle;
        L0::driverCount = 1;
    }
    void TearDown() override {
        delete driverHandle;
        L0::GlobalDriver = nullptr;
        L0::GlobalDriverHandle = nullptr;
    }
    L0::DriverHandle *driverHandle;
    VariableBackup<_ze_driver_handle_t *> globalDriverHandleBackup{&GlobalDriverHandle};
    VariableBackup<uint32_t> driverCountBackup{&driverCount};
};

TEST(DriverHandleNegativeTest, givenNotInitializedDriverWhenZeDriverGetIsCalledThenReturnZeroCount) {
    uint32_t count = 0u;
    auto result = zeDriverGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0U, count);
}

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
    ze_result_t result = ZE_RESULT_SUCCESS;
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
    ze_result_t result = ZE_RESULT_SUCCESS;

    uint32_t count = 1;
    ze_driver_handle_t hDriverHandle = reinterpret_cast<ze_driver_handle_t>(&hDriverHandle);

    result = zeDriverGet(&count, &hDriverHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, hDriverHandle);
    EXPECT_EQ(hDriverHandle, GlobalDriver);
}

TEST_F(DriverHandleTest, givenInitializedDriverWhenGetDeviceIsCalledThenOneDeviceIsObtained) {
    ze_result_t result = ZE_RESULT_SUCCESS;
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

TEST_F(DriverHandleTest, givenValidDriverHandleWhenSetErrorDescriptionIsCalledThenGetErrorDecriptionGetsStringCorrectly) {
    std::string errorString = "we manually created error";
    std::string errorString2 = "here's the next string to pass with arguments: ";
    std::string errorString2Fmt = errorString2 + std::string("%d");
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverHandle->setErrorDescription(errorString.c_str());

    const char *pStr = nullptr;
    result = zeDriverGetLastErrorDescription(driverHandle->toHandle(), &pStr);
    EXPECT_EQ(0, strcmp(errorString.c_str(), pStr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    driverHandle->setErrorDescription(errorString2Fmt.c_str(), 4);
    result = zeDriverGetLastErrorDescription(driverHandle->toHandle(), &pStr);
    std::string expectedString = errorString2 + "4";
    EXPECT_EQ(0, strcmp(expectedString.c_str(), pStr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(DriverHandleTest, givenValidDriverHandleWhenGetErrorDescriptionIsCalledThenEmptyStringIsReturned) {
    std::string errorString = "";
    ze_result_t result = ZE_RESULT_SUCCESS;

    const char *pStr = nullptr;
    result = zeDriverGetLastErrorDescription(driverHandle->toHandle(), &pStr);
    EXPECT_EQ(0, strcmp(errorString.c_str(), pStr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    driverHandle->setErrorDescription(errorString.c_str());
    result = zeDriverGetLastErrorDescription(driverHandle->toHandle(), &pStr);
    EXPECT_EQ(0, strcmp(errorString.c_str(), pStr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(DriverHandleTest, givenValidDriverHandleWhenClearErrorDescriptionIsCalledThenEmptyStringIsReturned) {
    std::string errorString = "error string";
    std::string emptyString = "";
    const char *pStr = nullptr;
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverHandle->setErrorDescription(errorString.c_str());
    result = zeDriverGetLastErrorDescription(driverHandle->toHandle(), &pStr);
    EXPECT_EQ(0, strcmp(errorString.c_str(), pStr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = driverHandle->clearErrorDescription();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = zeDriverGetLastErrorDescription(driverHandle->toHandle(), &pStr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0, strcmp(emptyString.c_str(), pStr));
}

TEST(zeDriverHandleGetProperties, whenZeDriverGetPropertiesIsCalledThenGetPropertiesIsCalled) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    Mock<DriverHandle> driverHandle;
    ze_driver_properties_t properties;
    ze_result_t expectedResult = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;

    driverHandle.getPropertiesResult = expectedResult;
    result = zeDriverGetProperties(driverHandle.toHandle(), &properties);
    EXPECT_EQ(expectedResult, result);
    EXPECT_EQ(1u, driverHandle.getPropertiesCalled);
}

TEST(zeDriverHandleGetApiVersion, whenZeDriverGetApiIsCalledThenGetApiVersionIsCalled) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    Mock<DriverHandle> driverHandle;
    ze_api_version_t version;
    ze_result_t expectedResult = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;

    driverHandle.getApiVersionResult = expectedResult;
    result = zeDriverGetApiVersion(driverHandle.toHandle(), &version);
    EXPECT_EQ(expectedResult, result);
    EXPECT_EQ(1u, driverHandle.getApiVersionCalled);
}

TEST(zeDriverGetIpcProperties, whenZeDriverGetIpcPropertiesIsCalledThenGetIPCPropertiesIsCalled) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    Mock<DriverHandle> driverHandle;
    ze_driver_ipc_properties_t ipcProperties;
    ze_result_t expectedResult = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;

    driverHandle.getIPCPropertiesResult = expectedResult;
    result = zeDriverGetIpcProperties(driverHandle.toHandle(), &ipcProperties);
    EXPECT_EQ(expectedResult, result);
    EXPECT_EQ(1u, driverHandle.getIPCPropertiesCalled);
}

struct HostImportApiFixture : public HostPointerManagerFixure {
    void setUp() {
        HostPointerManagerFixure::setUp();

        driverHandle = hostDriverHandle->toHandle();
    }

    void tearDown() {
        HostPointerManagerFixure::tearDown();
    }

    ze_driver_handle_t driverHandle;
};

using DriverExperimentalApiTest = Test<HostImportApiFixture>;

TEST_F(DriverExperimentalApiTest, whenRetrievingApiFunctionThenExpectProperPointer) {
    decltype(&zexDriverImportExternalPointer) expectedImport = L0::zexDriverImportExternalPointer;
    decltype(&zexDriverReleaseImportedPointer) expectedRelease = L0::zexDriverReleaseImportedPointer;
    decltype(&zexDriverGetHostPointerBaseAddress) expectedGet = L0::zexDriverGetHostPointerBaseAddress;
    decltype(&zexKernelGetBaseAddress) expectedKernelGetBaseAddress = L0::zexKernelGetBaseAddress;

    void *funPtr = nullptr;

    auto result = zeDriverGetExtensionFunctionAddress(driverHandle, "zexDriverImportExternalPointer", &funPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(expectedImport, reinterpret_cast<decltype(&zexDriverImportExternalPointer)>(funPtr));

    result = zeDriverGetExtensionFunctionAddress(driverHandle, "zexDriverReleaseImportedPointer", &funPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(expectedRelease, reinterpret_cast<decltype(&zexDriverReleaseImportedPointer)>(funPtr));

    result = zeDriverGetExtensionFunctionAddress(driverHandle, "zexDriverGetHostPointerBaseAddress", &funPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(expectedGet, reinterpret_cast<decltype(&zexDriverGetHostPointerBaseAddress)>(funPtr));

    result = zeDriverGetExtensionFunctionAddress(driverHandle, "zexKernelGetBaseAddress", &funPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(expectedKernelGetBaseAddress, reinterpret_cast<decltype(&zexKernelGetBaseAddress)>(funPtr));
}

TEST_F(DriverExperimentalApiTest, givenHostPointerApiExistWhenImportingPtrThenExpectProperBehavior) {
    void *basePtr = nullptr;
    size_t offset = 0x20u;
    size_t size = 0x100;
    void *testPtr = ptrOffset(heapPointer, offset);

    auto result = zexDriverImportExternalPointer(driverHandle, testPtr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *offsetPtr = ptrOffset(testPtr, offset);

    result = zexDriverGetHostPointerBaseAddress(driverHandle, offsetPtr, &basePtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testPtr, basePtr);

    result = zexDriverReleaseImportedPointer(driverHandle, testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0
