/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
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
#include "shared/test/common/helpers/memory_management.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_os_library.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/host_pointer_manager_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_builtin_functions_lib_impl.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/driver_experimental/zex_api.h"
#include "level_zero/driver_experimental/zex_context.h"
#include "level_zero/driver_experimental/zex_driver.h"
#include "level_zero/ze_intel_gpu.h"

#include "driver_version.h"

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
    EXPECT_TRUE(levelZeroDriverInitialized);
    EXPECT_EQ(1u, driver.initCalledCount);
}

TEST(zeInit, whenCallingZeInitWithFailureInIinitThenLevelZeroDriverInitializedIsSetToFalse) {
    Mock<Driver> driver;
    driver.failInitDriver = true;

    auto result = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
    EXPECT_FALSE(levelZeroDriverInitialized);
}

TEST(zeInit, whenCallingZeInitWithVpuOnlyThenLevelZeroDriverInitializedIsSetToFalse) {
    Mock<Driver> driver;

    auto result = zeInit(ZE_INIT_FLAG_VPU_ONLY);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
    EXPECT_FALSE(levelZeroDriverInitialized);
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

TEST(zeInit, givenZeInitCalledWhenCallingZeInitInForkedProcessThenNewDriverIsInitialized) {
    Mock<Driver> driver;

    driver.pid = NEO::SysCalls::getCurrentProcessId();

    auto result = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // change pid in driver
    driver.pid = NEO::SysCalls::getCurrentProcessId() - 1;

    result = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(levelZeroDriverInitialized);
    EXPECT_EQ(2u, driver.initCalledCount);

    // pid updated to current pid
    auto expectedPid = NEO::SysCalls::getCurrentProcessId();
    EXPECT_EQ(expectedPid, driver.pid);
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
    std::vector<std::pair<std::string, uint32_t>> additionalExtensions;
    device->getL0GfxCoreHelper().appendPlatformSpecificExtensions(additionalExtensions, device->getProductHelper(), device->getHwInfo());

    if (device->getL0GfxCoreHelper().synchronizedDispatchSupported() && device->isImplicitScalingCapable()) {
        additionalExtensions.emplace_back(ZE_SYNCHRONIZED_DISPATCH_EXP_NAME, ZE_SYNCHRONIZED_DISPATCH_EXP_VERSION_CURRENT);
    }

    if (device->getNEODevice()->getRootDeviceEnvironment().getBindlessHeapsHelper()) {
        additionalExtensions.emplace_back(ZE_BINDLESS_IMAGE_EXP_NAME, ZE_BINDLESS_IMAGE_EXP_VERSION_CURRENT);
    }
    if (!device->getProductHelper().isDcFlushAllowed()) {
        additionalExtensions.emplace_back(ZEX_INTEL_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_NAME, ZEX_INTEL_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_VERSION_CURRENT);
    }

    uint32_t count = 0;
    ze_result_t res = driverHandle->getExtensionProperties(&count, nullptr);
    EXPECT_EQ(count, static_cast<uint32_t>(driverHandle->extensionsSupported.size() + additionalExtensions.size()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_driver_extension_properties_t *extensionProperties = new ze_driver_extension_properties_t[count];
    count++;
    res = driverHandle->getExtensionProperties(&count, extensionProperties);
    EXPECT_EQ(count, static_cast<uint32_t>(driverHandle->extensionsSupported.size() + additionalExtensions.size()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(driverHandle.get());

    size_t i = 0;
    for (; i < driverHandleImp->extensionsSupported.size(); i++) {
        auto extension = extensionProperties[i];
        EXPECT_EQ(0, strcmp(extension.name, driverHandleImp->extensionsSupported[i].first.c_str()));
        EXPECT_EQ(extension.version, driverHandleImp->extensionsSupported[i].second);
    }

    for (size_t j = 0; j < additionalExtensions.size(); j++) {
        auto extension = extensionProperties[i];
        EXPECT_EQ(0, strcmp(extension.name, additionalExtensions[j].first.c_str()));
        EXPECT_EQ(extension.version, additionalExtensions[j].second);

        i++;
    }

    delete[] extensionProperties;
}

TEST_F(DriverVersionTest, givenExternalAllocatorWhenCallingGetExtensionPropertiesThenBindlessImageExtensionIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.UseBindlessMode.set(1);
    NEO::debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);

    auto hwInfo = *NEO::defaultHwInfo;
    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    NEO::MockDevice *neoDevice2 = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);

    NEO::debugManager.flags.UseBindlessMode.set(0);
    NEO::debugManager.flags.UseExternalAllocatorForSshAndDsh.set(0);
    NEO::MockDevice *neoDevice3 = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice3));
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice2));

    ze_result_t res;
    auto driverHandle = DriverHandle::create(std::move(devices), L0EnvVariables{}, &res);
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(driverHandle);

    uint32_t count = 0;
    res = driverHandle->getExtensionProperties(&count, nullptr);
    EXPECT_GT(count, static_cast<uint32_t>(driverHandleImp->extensionsSupported.size()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_driver_extension_properties_t *extensionProperties = new ze_driver_extension_properties_t[count];
    res = driverHandle->getExtensionProperties(&count, extensionProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    bool extensionFound = false;
    for (uint32_t i = 0; i < count; i++) {
        auto extension = extensionProperties[i];
        if (strcmp(extension.name, ZE_BINDLESS_IMAGE_EXP_NAME) == 0) {
            EXPECT_FALSE(extensionFound);
            extensionFound = true;
        }
    }
    EXPECT_TRUE(extensionFound);

    delete[] extensionProperties;
    delete driverHandle;
    L0::globalDriver = nullptr;
}

TEST_F(DriverVersionTest, WhenGettingDriverVersionThenExpectedDriverVersionIsReturned) {
    ze_driver_properties_t properties;
    ze_result_t res = driverHandle->getProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto expectedDriverVersion = static_cast<uint32_t>(DriverHandleImp::initialDriverVersionValue);
    expectedDriverVersion += static_cast<uint32_t>(NEO_VERSION_BUILD);
    EXPECT_EQ(expectedDriverVersion, properties.driverVersion);
}

TEST_F(DriverVersionTest, GivenDebugOverrideWhenGettingDriverVersionThenExpectedOverrideDriverVersionIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.OverrideDriverVersion.set(0);

    ze_driver_properties_t properties;
    ze_result_t res = driverHandle->getProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint32_t expectedDriverVersion = 0;
    EXPECT_EQ(expectedDriverVersion, properties.driverVersion);

    NEO::debugManager.flags.OverrideDriverVersion.set(10);

    res = driverHandle->getProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    expectedDriverVersion = 10;
    EXPECT_EQ(expectedDriverVersion, properties.driverVersion);

    NEO::debugManager.flags.OverrideDriverVersion.set(DriverHandleImp::initialDriverVersionValue + 20);

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
    auto expectedDriverVersion = static_cast<uint32_t>(DriverHandleImp::initialDriverVersionValue);
    expectedDriverVersion += static_cast<uint32_t>(NEO_VERSION_BUILD);
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

    NEO::GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
        auto graphicsAllocation = createMemoryAllocation(properties.allocationType, nullptr, reinterpret_cast<void *>(1), 1,
                                                         4096u, osHandleData.handle, MemoryPool::systemCpuInaccessible,
                                                         properties.rootDeviceIndex, false, false, false);
        graphicsAllocation->setSharedHandle(osHandleData.handle);
        graphicsAllocation->set32BitAllocation(false);
        GmmRequirements gmmRequirements{};
        gmmRequirements.allowLargePages = true;
        gmmRequirements.preferCompressed = false;
        graphicsAllocation->setDefaultGmm(new Gmm(executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
        return graphicsAllocation;
    }
};

using ImportNTHandleWithMockMemoryManager = Test<DeviceFixtureWithCustomMemoryManager<MemoryManagerNTHandleMock>>;

HWTEST_F(ImportNTHandleWithMockMemoryManager, givenCallToImportNTHandleWithHostBufferMemoryAllocationTypeThenHostUnifiedMemoryIsSet) {
    delete driverHandle->svmAllocsManager;
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get(), false);

    uint64_t imageHandle = 0x1;
    NEO::AllocationType allocationType = NEO::AllocationType::bufferHostMemory;
    void *ptr = driverHandle->importNTHandle(device->toHandle(), &imageHandle, allocationType);
    EXPECT_NE(ptr, nullptr);

    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);
    EXPECT_EQ(allocData->memoryType, InternalMemoryType::hostUnifiedMemory);

    ze_result_t result = context->freeMem(ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(ImportNTHandleWithMockMemoryManager, givenCallToImportNTHandleWithBufferMemoryAllocationTypeThenDeviceUnifiedMemoryIsSet) {
    delete driverHandle->svmAllocsManager;
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get(), false);

    uint64_t imageHandle = 0x1;
    NEO::AllocationType allocationType = NEO::AllocationType::buffer;
    void *ptr = driverHandle->importNTHandle(device->toHandle(), &imageHandle, allocationType);
    EXPECT_NE(ptr, nullptr);

    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);
    EXPECT_EQ(allocData->memoryType, InternalMemoryType::deviceUnifiedMemory);

    ze_result_t result = context->freeMem(ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(ImportNTHandleWithMockMemoryManager, givenNTHandleWhenCreatingDeviceMemoryThenSuccessIsReturned) {
    ze_device_mem_alloc_desc_t devDesc = {};
    devDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;

    uint64_t imageHandle = 0x1;
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    devDesc.pNext = &importNTHandle;

    delete driverHandle->svmAllocsManager;
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

HWTEST_F(ImportNTHandleWithMockMemoryManager, givenNTHandleWhenCreatingHostMemoryThenSuccessIsReturned) {
    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;

    uint64_t imageHandle = 0x1;
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.handle = &imageHandle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    hostDesc.pNext = &importNTHandle;

    delete driverHandle->svmAllocsManager;
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

HWTEST_F(ImportNTHandleWithMockMemoryManager, whenCallingCreateGraphicsAllocationFromMultipleSharedHandlesFromOsAgnosticMemoryManagerThenNullptrIsReturned) {
    delete driverHandle->svmAllocsManager;
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get(), false);

    std::vector<osHandle> handles{6, 7};
    AllocationProperties properties = {device->getRootDeviceIndex(),
                                       true,
                                       MemoryConstants::pageSize,
                                       AllocationType::buffer,
                                       false,
                                       device->getNEODevice()->getDeviceBitfield()};
    bool requireSpecificBitness{};
    bool isHostIpcAllocation{};
    auto ptr = execEnv->memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, requireSpecificBitness, isHostIpcAllocation, true, nullptr);
    EXPECT_EQ(nullptr, ptr);
}

constexpr size_t invalidHandle = 0xffffffff;

HWTEST_F(ImportNTHandle, givenNotExistingNTHandleWhenCreatingDeviceMemoryThenErrorIsReturned) {
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
    L0::globalDriver = nullptr;
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

    EXPECT_EQ(NEO::DebuggingMode::disabled, driverHandle->enableProgramDebugging);

    delete driverHandle;
    L0::globalDriver = nullptr;
}

struct DriverImpTest : public ::testing::Test {
    VariableBackup<_ze_driver_handle_t *> globalDriverHandleBackup{&globalDriverHandle};
    VariableBackup<uint32_t> driverCountBackup{&driverCount};
};

TEST_F(DriverImpTest, givenDriverImpWhenInitializedThenEnvVariablesAreRead) {

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_LE(3u, IoFunctions::mockGetenvCalled);

    delete L0::globalDriver;
    L0::globalDriverHandle = nullptr;
    L0::globalDriver = nullptr;
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
    EXPECT_EQ(nullptr, L0::globalDriverHandle);
    EXPECT_EQ(nullptr, L0::globalDriver);
}

TEST_F(DriverImpTest, givenEnabledProgramDebuggingWhenCreatingExecutionEnvironmentThenDebuggingEnabledIsTrue) {

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_NE(nullptr, L0::globalDriver);
    ASSERT_NE(0u, L0::globalDriver->numDevices);
    EXPECT_TRUE(L0::globalDriver->devices[0]->getNEODevice()->getExecutionEnvironment()->isDebuggingEnabled());

    delete L0::globalDriver;
    L0::globalDriverHandle = nullptr;
    L0::globalDriver = nullptr;
}

TEST_F(DriverImpTest, whenCreatingExecutionEnvironmentThenDefaultHierarchyIsEnabled) {

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);
    L0::DriverHandleImp *driverHandleImp = reinterpret_cast<L0::DriverHandleImp *>(L0::globalDriverHandle);
    auto &gfxCoreHelper = driverHandleImp->memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    if (strcmp(gfxCoreHelper.getDefaultDeviceHierarchy(), "COMPOSITE") == 0) {
        EXPECT_EQ(driverHandleImp->deviceHierarchyMode, L0::L0DeviceHierarchyMode::L0_DEVICE_HIERARCHY_COMPOSITE);
    } else {
        EXPECT_EQ(driverHandleImp->deviceHierarchyMode, L0::L0DeviceHierarchyMode::L0_DEVICE_HIERARCHY_FLAT);
    }
    ASSERT_NE(nullptr, L0::globalDriver);
    ASSERT_NE(0u, L0::globalDriver->numDevices);

    delete L0::globalDriver;
    L0::globalDriverHandle = nullptr;
    L0::globalDriver = nullptr;
}

TEST_F(DriverImpTest, givenFlatDeviceHierarchyWhenCreatingExecutionEnvironmentThenFlatHierarchyIsEnabled) {

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "FLAT"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);
    L0::DriverHandleImp *driverHandleImp = reinterpret_cast<L0::DriverHandleImp *>(L0::globalDriverHandle);
    EXPECT_EQ(driverHandleImp->deviceHierarchyMode, L0::L0DeviceHierarchyMode::L0_DEVICE_HIERARCHY_FLAT);
    ASSERT_NE(nullptr, L0::globalDriver);
    ASSERT_NE(0u, L0::globalDriver->numDevices);

    delete L0::globalDriver;
    L0::globalDriverHandle = nullptr;
    L0::globalDriver = nullptr;
}

TEST_F(DriverImpTest, givenCompositeDeviceHierarchyWhenCreatingExecutionEnvironmentThenCompositeHierarchyIsEnabled) {

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "COMPOSITE"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);
    L0::DriverHandleImp *driverHandleImp = reinterpret_cast<L0::DriverHandleImp *>(L0::globalDriverHandle);
    EXPECT_EQ(driverHandleImp->deviceHierarchyMode, L0::L0DeviceHierarchyMode::L0_DEVICE_HIERARCHY_COMPOSITE);
    ASSERT_NE(nullptr, L0::globalDriver);
    ASSERT_NE(0u, L0::globalDriver->numDevices);

    delete L0::globalDriver;
    L0::globalDriverHandle = nullptr;
    L0::globalDriver = nullptr;
}

TEST_F(DriverImpTest, givenCombinedDeviceHierarchyWhenCreatingExecutionEnvironmentThenCombinedHierarchyIsEnabled) {

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "COMBINED"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);
    L0::DriverHandleImp *driverHandleImp = reinterpret_cast<L0::DriverHandleImp *>(L0::globalDriverHandle);
    EXPECT_EQ(driverHandleImp->deviceHierarchyMode, L0::L0DeviceHierarchyMode::L0_DEVICE_HIERARCHY_COMBINED);
    ASSERT_NE(nullptr, L0::globalDriver);
    ASSERT_NE(0u, L0::globalDriver->numDevices);

    delete L0::globalDriver;
    L0::globalDriverHandle = nullptr;
    L0::globalDriver = nullptr;
}

TEST_F(DriverImpTest, givenErrantDeviceHierarchyWhenCreatingExecutionEnvironmentThenDefaultHierarchyIsEnabled) {

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "Flat"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);
    L0::DriverHandleImp *driverHandleImp = reinterpret_cast<L0::DriverHandleImp *>(L0::globalDriverHandle);
    auto &gfxCoreHelper = driverHandleImp->memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    if (strcmp(gfxCoreHelper.getDefaultDeviceHierarchy(), "COMPOSITE") == 0) {
        EXPECT_EQ(driverHandleImp->deviceHierarchyMode, L0::L0DeviceHierarchyMode::L0_DEVICE_HIERARCHY_COMPOSITE);
    } else {
        EXPECT_EQ(driverHandleImp->deviceHierarchyMode, L0::L0DeviceHierarchyMode::L0_DEVICE_HIERARCHY_FLAT);
    }
    ASSERT_NE(nullptr, L0::globalDriver);
    ASSERT_NE(0u, L0::globalDriver->numDevices);

    delete L0::globalDriver;
    L0::globalDriverHandle = nullptr;
    L0::globalDriver = nullptr;
}

TEST_F(DriverImpTest, givenEnableProgramDebuggingWithValue2WhenCreatingExecutionEnvironmentThenDebuggingEnabledIsTrue) {

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "2"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_NE(nullptr, L0::globalDriver);
    ASSERT_NE(0u, L0::globalDriver->numDevices);
    EXPECT_TRUE(L0::globalDriver->devices[0]->getNEODevice()->getExecutionEnvironment()->isDebuggingEnabled());

    delete L0::globalDriver;
    L0::globalDriverHandle = nullptr;
    L0::globalDriver = nullptr;
}

TEST_F(DriverImpTest, givenEnabledFP64EmulationWhenCreatingExecutionEnvironmentThenFP64EmulationIsEnabled) {

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"NEO_FP64_EMULATION", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_NE(nullptr, L0::globalDriver);
    ASSERT_NE(0u, L0::globalDriver->numDevices);
    EXPECT_TRUE(L0::globalDriver->devices[0]->getNEODevice()->getExecutionEnvironment()->isFP64EmulationEnabled());

    delete L0::globalDriver;
    L0::globalDriverHandle = nullptr;
    L0::globalDriver = nullptr;
}

TEST_F(DriverImpTest, givenEnabledProgramDebuggingAndEnabledExperimentalOpenCLWhenCreatingExecutionEnvironmentThenDebuggingEnabledIsFalse) {
    DebugManagerStateRestore restorer;

    VariableBackup<_ze_driver_handle_t *> globalDriverHandleBackup{&globalDriverHandle};
    VariableBackup<uint32_t> driverCountBackup{&driverCount};
    NEO::debugManager.flags.ExperimentalEnableL0DebuggerForOpenCL.set(true);

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(L0::globalDriverHandle)> mockableDriverHandle(&L0::globalDriverHandle);
    VariableBackup<decltype(L0::globalDriver)> mockableDriver(&L0::globalDriver);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_NE(nullptr, L0::globalDriver);
    ASSERT_NE(0u, L0::globalDriver->numDevices);
    EXPECT_FALSE(L0::globalDriver->devices[0]->getNEODevice()->getExecutionEnvironment()->isDebuggingEnabled());

    delete L0::globalDriver;
}

TEST_F(DriverImpTest, givenEnableProgramDebuggingWithValue2AndEnabledExperimentalOpenCLWhenCreatingExecutionEnvironmentThenDebuggingEnabledIsFalse) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ExperimentalEnableL0DebuggerForOpenCL.set(true);

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "2"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(L0::globalDriverHandle)> mockableDriverHandle(&L0::globalDriverHandle);
    VariableBackup<decltype(L0::globalDriver)> mockableDriver(&L0::globalDriver);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_NE(nullptr, L0::globalDriver);
    ASSERT_NE(0u, L0::globalDriver->numDevices);
    EXPECT_FALSE(L0::globalDriver->devices[0]->getNEODevice()->getExecutionEnvironment()->isDebuggingEnabled());

    delete L0::globalDriver;
}

TEST_F(DriverImpTest, givenNoProgramDebuggingEnvVarWhenCreatingExecutionEnvironmentThenDebuggingEnabledIsFalse) {
    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_NE(nullptr, L0::globalDriver);
    ASSERT_NE(0u, L0::globalDriver->numDevices);
    EXPECT_FALSE(L0::globalDriver->devices[0]->getNEODevice()->getExecutionEnvironment()->isDebuggingEnabled());

    delete L0::globalDriver;
    L0::globalDriverHandle = nullptr;
    L0::globalDriver = nullptr;
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

    EXPECT_TRUE(driverHandle->enableProgramDebugging == NEO::DebuggingMode::online);

    delete driverHandle;
    L0::globalDriver = nullptr;
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

    EXPECT_TRUE(driverHandle->enableProgramDebugging == NEO::DebuggingMode::offline);

    delete driverHandle;
    L0::globalDriver = nullptr;
}

TEST(DriverTest, givenBuiltinsAsyncInitEnabledWhenCreatingDriverThenMakeSureBuiltinsInitIsCompletedOnExitOfDriverHandleInitialization) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useinitBuiltinsAsyncEnabled = true;

    ze_result_t returnValue;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    L0EnvVariables envVariables = {};

    auto driverHandle = whiteboxCast(static_cast<::L0::DriverHandleImp *>(DriverHandle::create(std::move(devices), envVariables, &returnValue)));
    EXPECT_NE(nullptr, driverHandle);

    L0::Device *device = driverHandle->devices[0];
    auto builtinFunctionsLib = device->getBuiltinFunctionsLib();

    if (builtinFunctionsLib) {
        auto builtinsLibIpl = static_cast<MockBuiltinFunctionsLibImpl *>(builtinFunctionsLib);
        EXPECT_TRUE(builtinsLibIpl->initAsyncComplete);
    }

    delete driverHandle;
    L0::globalDriver = nullptr;

    /* std::async may create a detached thread - completion of the scheduled task can be ensured,
       but there is no way to ensure that actual OS thread exited and its resources are freed */
    MemoryManagement::fastLeaksDetectionMode = MemoryManagement::LeakDetectionMode::TURN_OFF_LEAK_DETECTION;
}

TEST(DriverTest, givenInvalidCompilerEnvironmentThenDependencyUnavailableErrorIsReturned) {
    VariableBackup<bool> backupUseMockSip{&MockSipData::useMockSip};
    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    backupUseMockSip = true;

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    auto oldFclDllName = Os::frontEndDllName;
    Os::frontEndDllName = "_invalidFCL";
    auto igcNameGuard = NEO::pushIgcDllName("_invalidIGC");
    driverImp.initialize(&result);
    EXPECT_EQ(result, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);

    Os::frontEndDllName = oldFclDllName;

    ASSERT_EQ(nullptr, L0::globalDriver);
}

TEST(DriverTest, givenInvalidCompilerEnvironmentAndEnableProgramDebuggingWithValue2ThenDependencyUnavailableErrorIsReturned) {
    VariableBackup<bool> backupUseMockSip{&MockSipData::useMockSip};
    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "2"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    backupUseMockSip = true;

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    auto oldFclDllName = Os::frontEndDllName;
    Os::frontEndDllName = "_invalidFCL";
    auto igcNameGuard = NEO::pushIgcDllName("_invalidIGC");
    driverImp.initialize(&result);
    EXPECT_EQ(result, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);

    Os::frontEndDllName = oldFclDllName;

    ASSERT_EQ(nullptr, L0::globalDriver);
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
    L0::globalDriver = nullptr;
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
        L0::globalDriverHandle = driverHandle;
        L0::driverCount = 1;
    }
    void TearDown() override {
        delete driverHandle;
        L0::globalDriver = nullptr;
        L0::globalDriverHandle = nullptr;
    }
    L0::DriverHandle *driverHandle;
    VariableBackup<_ze_driver_handle_t *> globalDriverHandleBackup{&globalDriverHandle};
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
    EXPECT_EQ(hDriverHandle, globalDriver);
}

TEST_F(DriverHandleTest, givenInitializedDriverWhenGetDeviceIsCalledThenOneDeviceIsObtained) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t count = 1;

    ze_device_handle_t device;
    result = driverHandle->getDevice(&count, &device);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, &device);
}

TEST_F(DriverHandleTest, givenInitializedDriverWithTwoDevicesWhenGetDeviceIsCalledWithLessCountThenOnlySpecifiedNumberOfDevicesAreObtainedAndCountIsNotUpdated) {
    auto hwInfo = *NEO::defaultHwInfo;
    NEO::MockDevice *testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    ze_result_t result = ZE_RESULT_SUCCESS;
    L0::DriverHandleImp *driverHandleImp = static_cast<L0::DriverHandleImp *>(driverHandle);
    auto newNeoDevice = L0::Device::create(driverHandleImp, std::move(testNeoDevice), false, &result);
    driverHandleImp->devices.push_back(newNeoDevice);
    driverHandleImp->numDevices++;

    uint32_t count = 0U;
    result = driverHandle->getDevice(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2U, count);

    count = 1U;
    std::vector<ze_device_handle_t> devices(2, nullptr);
    result = driverHandle->getDevice(&count, devices.data());
    EXPECT_EQ(1U, count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, devices[0]);
    EXPECT_EQ(nullptr, devices[1]);
}

TEST_F(DriverHandleTest, givenInitializedDriverWhenGetDeviceIsCalledThenOneDeviceIsObtainedAndCountIsUpdated) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t count = 3;

    ze_device_handle_t device;
    result = driverHandle->getDevice(&count, &device);
    EXPECT_EQ(1U, count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, &device);
}

TEST_F(DriverHandleTest, whenQueryingForApiVersionThenExpectedVersionIsReturned) {
    ze_api_version_t version = {};
    ze_result_t result = driverHandle->getApiVersion(&version);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZE_API_VERSION_1_6, version);
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
    const char *errorString = "we manually created error";
    std::string errorString2 = "here's the next string to pass with arguments: ";
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverHandle->setErrorDescription(std::string(errorString));

    const char *pStr = nullptr;
    result = zeDriverGetLastErrorDescription(driverHandle->toHandle(), &pStr);
    EXPECT_EQ(0, strcmp(errorString, pStr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::string expectedString = errorString2;
    driverHandle->setErrorDescription(errorString2);
    result = zeDriverGetLastErrorDescription(driverHandle->toHandle(), &pStr);
    EXPECT_EQ(0, strcmp(expectedString.c_str(), pStr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(DriverHandleTest, givenValidDriverHandleWhenGetErrorDescriptionIsCalledThenEmptyStringIsReturned) {
    const char *errorString = "";
    ze_result_t result = ZE_RESULT_SUCCESS;

    const char *pStr = nullptr;
    result = zeDriverGetLastErrorDescription(driverHandle->toHandle(), &pStr);
    EXPECT_EQ(0, strcmp(errorString, pStr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    driverHandle->setErrorDescription(std::string(errorString));
    result = zeDriverGetLastErrorDescription(driverHandle->toHandle(), &pStr);
    EXPECT_EQ(0, strcmp(errorString, pStr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(DriverHandleTest, givenValidDriverHandleWhenClearErrorDescriptionIsCalledThenEmptyStringIsReturned) {
    const char *errorString = "error string";
    std::string emptyString = "";
    const char *pStr = nullptr;
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverHandle->setErrorDescription(std::string(errorString));
    result = zeDriverGetLastErrorDescription(driverHandle->toHandle(), &pStr);
    EXPECT_EQ(0, strcmp(errorString, pStr));
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
    decltype(&zeIntelGetDriverVersionString) expectedIntelGetDriverVersionString = zeIntelGetDriverVersionString;
    decltype(&zeIntelMediaCommunicationCreate) expectedIntelMediaCommunicationCreate = L0::zeIntelMediaCommunicationCreate;
    decltype(&zeIntelMediaCommunicationDestroy) expectedIntelMediaCommunicationDestroy = L0::zeIntelMediaCommunicationDestroy;
    decltype(&zexIntelAllocateNetworkInterrupt) expectedIntelAllocateNetworkInterrupt = L0::zexIntelAllocateNetworkInterrupt;
    decltype(&zexIntelReleaseNetworkInterrupt) expectedIntelReleaseNetworkInterrupt = L0::zexIntelReleaseNetworkInterrupt;

    decltype(&zexCounterBasedEventCreate2) expectedCounterBasedEventCreate2 = L0::zexCounterBasedEventCreate2;
    decltype(&zexCounterBasedEventGetIpcHandle) expectedCounterBasedEventGetIpcHandle = L0::zexCounterBasedEventGetIpcHandle;
    decltype(&zexCounterBasedEventOpenIpcHandle) expectedCounterBasedEventOpenIpcHandle = L0::zexCounterBasedEventOpenIpcHandle;
    decltype(&zexCounterBasedEventCloseIpcHandle) expectedCounterBasedEventCloseIpcHandle = L0::zexCounterBasedEventCloseIpcHandle;

    void *funPtr = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexDriverImportExternalPointer", &funPtr));
    EXPECT_EQ(expectedImport, reinterpret_cast<decltype(&zexDriverImportExternalPointer)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexDriverReleaseImportedPointer", &funPtr));
    EXPECT_EQ(expectedRelease, reinterpret_cast<decltype(&zexDriverReleaseImportedPointer)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexDriverGetHostPointerBaseAddress", &funPtr));
    EXPECT_EQ(expectedGet, reinterpret_cast<decltype(&zexDriverGetHostPointerBaseAddress)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexKernelGetBaseAddress", &funPtr));
    EXPECT_EQ(expectedKernelGetBaseAddress, reinterpret_cast<decltype(&zexKernelGetBaseAddress)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zeIntelGetDriverVersionString", &funPtr));
    EXPECT_EQ(expectedIntelGetDriverVersionString, reinterpret_cast<decltype(&zeIntelGetDriverVersionString)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zeIntelMediaCommunicationCreate", &funPtr));
    EXPECT_EQ(expectedIntelMediaCommunicationCreate, reinterpret_cast<decltype(&zeIntelMediaCommunicationCreate)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zeIntelMediaCommunicationDestroy", &funPtr));
    EXPECT_EQ(expectedIntelMediaCommunicationDestroy, reinterpret_cast<decltype(&zeIntelMediaCommunicationDestroy)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexIntelAllocateNetworkInterrupt", &funPtr));
    EXPECT_EQ(expectedIntelAllocateNetworkInterrupt, reinterpret_cast<decltype(&zexIntelAllocateNetworkInterrupt)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexIntelReleaseNetworkInterrupt", &funPtr));
    EXPECT_EQ(expectedIntelReleaseNetworkInterrupt, reinterpret_cast<decltype(&zexIntelReleaseNetworkInterrupt)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexCounterBasedEventCreate2", &funPtr));
    EXPECT_EQ(expectedCounterBasedEventCreate2, reinterpret_cast<decltype(&zexCounterBasedEventCreate2)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexCounterBasedEventGetIpcHandle", &funPtr));
    EXPECT_EQ(expectedCounterBasedEventGetIpcHandle, reinterpret_cast<decltype(&zexCounterBasedEventGetIpcHandle)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexCounterBasedEventOpenIpcHandle", &funPtr));
    EXPECT_EQ(expectedCounterBasedEventOpenIpcHandle, reinterpret_cast<decltype(&zexCounterBasedEventOpenIpcHandle)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexCounterBasedEventCloseIpcHandle", &funPtr));
    EXPECT_EQ(expectedCounterBasedEventCloseIpcHandle, reinterpret_cast<decltype(&zexCounterBasedEventCloseIpcHandle)>(funPtr));
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

TEST_F(DriverExperimentalApiTest, givenGetVersionStringAPIExistsThenGetCurrentVersionString) {
    size_t sizeOfDriverString = 0;
    auto result = zeIntelGetDriverVersionString(driverHandle, nullptr, &sizeOfDriverString);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(sizeOfDriverString, 0u);
    char *driverVersionString = reinterpret_cast<char *>(malloc(sizeOfDriverString * sizeof(char)));
    result = zeIntelGetDriverVersionString(driverHandle, driverVersionString, &sizeOfDriverString);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE("", driverVersionString);
    free(driverVersionString);
}

struct GtPinInitTest : public ::testing::Test {
    void SetUp() override {
        gtpinInitTimesCalled = 0u;
        driver.driverInitCallBase = true;
        driver.initializeCallBase = true;

        uint32_t (*openPinHandler)(void *) = [](void *arg) -> uint32_t {
            gtpinInitTimesCalled++;
            uint32_t driverCount = 0;
            ze_driver_handle_t driverHandle{};
            auto result = zeDriverGet(&driverCount, nullptr);
            EXPECT_EQ(ZE_RESULT_SUCCESS, result);
            EXPECT_EQ(1u, driverCount);
            if (result != ZE_RESULT_SUCCESS || driverCount != 1) {
                return 1;
            }
            result = zeDriverGet(&driverCount, &driverHandle);
            EXPECT_EQ(1u, driverCount);
            if (result != ZE_RESULT_SUCCESS || driverCount != 1) {
                return 1;
            }
            EXPECT_EQ(globalDriverHandle, driverHandle);
            if (globalDriverHandle != driverHandle) {
                return 1;
            }

            return 0;
        };
        MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, false);

        auto osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);

        osLibrary->procMap["OpenGTPin"] = reinterpret_cast<void *>(openPinHandler);
    }
    void TearDown() override {
        delete MockOsLibrary::loadLibraryNewObject;
        delete globalDriverHandle;
        gtpinInitTimesCalled = 0u;
    }

    Mock<Driver> driver;
    static uint32_t gtpinInitTimesCalled;
    VariableBackup<uint32_t> gtpinCounterBackup{&gtpinInitTimesCalled, 0};
    VariableBackup<uint32_t> mockGetenvCalledBackup{&IoFunctions::mockGetenvCalled, 0};
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_INSTRUMENTATION", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup{&IoFunctions::mockableEnvValues, &mockableEnvs};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};
    VariableBackup<NEO::OsLibrary *> osLibraryBackup{&MockOsLibrary::loadLibraryNewObject, nullptr};
    VariableBackup<_ze_driver_handle_t *> globalDriverHandleBackup{&globalDriverHandle, nullptr};
    VariableBackup<decltype(L0::globalDriver)> globalDriverBackup{&L0::globalDriver, nullptr};
    VariableBackup<uint32_t> driverCountBackup{&driverCount};
};
uint32_t GtPinInitTest::gtpinInitTimesCalled = 0u;

TEST_F(GtPinInitTest, givenRequirementForGtpinWhenCallingZeInitMultipleTimesThenGtPinIsNotInitialized) {
    EXPECT_FALSE(driver.gtPinInitializationNeeded.load());
    auto result = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, driver.initCalledCount);
    EXPECT_EQ(1u, driver.initializeCalledCount);
    EXPECT_EQ(0u, gtpinInitTimesCalled);
    EXPECT_TRUE(driver.gtPinInitializationNeeded.load());
    driver.gtPinInitializationNeeded = false;
    result = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, driver.initCalledCount);
    EXPECT_EQ(1u, driver.initializeCalledCount);
    EXPECT_EQ(0u, gtpinInitTimesCalled);
    EXPECT_FALSE(driver.gtPinInitializationNeeded.load());
}

TEST_F(GtPinInitTest, givenRequirementForGtpinWhenCallingZeDriverGetMultipleTimesThenGtPinIsInitializedOnlyOnce) {
    auto result = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, driver.initCalledCount);
    EXPECT_EQ(1u, driver.initializeCalledCount);
    EXPECT_EQ(0u, gtpinInitTimesCalled);
    uint32_t driverCount = 0;
    ze_driver_handle_t driverHandle{};
    result = zeDriverGet(&driverCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(1u, driverCount);
    result = zeDriverGet(&driverCount, &driverHandle);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(1u, driverCount);
    EXPECT_EQ(globalDriverHandle, driverHandle);

    EXPECT_EQ(1u, driver.initCalledCount);
    EXPECT_EQ(1u, driver.initializeCalledCount);
    EXPECT_EQ(1u, gtpinInitTimesCalled);
}

TEST_F(GtPinInitTest, givenFailureWhenInitializingGtpinThenTheErrorIsNotExposedInZeDriverGetFunction) {

    uint32_t (*gtPinInit)(void *) = [](void *arg) -> uint32_t {
        return 1; // failure
    };

    auto osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap["OpenGTPin"] = reinterpret_cast<void *>(gtPinInit);
    auto result = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, driver.initCalledCount);
    EXPECT_EQ(1u, driver.initializeCalledCount);
    uint32_t driverCount = 0;
    ze_driver_handle_t driverHandle{};
    result = zeDriverGet(&driverCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(1u, driverCount);
    result = zeDriverGet(&driverCount, &driverHandle);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(1u, driverCount);
    EXPECT_EQ(globalDriverHandle, driverHandle);
}

TEST_F(GtPinInitTest, givenRequirementForGtpinWhenCallingZeInitDriversWithoutDriverHandlesRequestedMultipleTimesThenGtPinIsNotInitialized) {
    ze_init_driver_type_desc_t desc = {ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC};
    desc.flags = ZE_INIT_DRIVER_TYPE_FLAG_GPU;
    desc.pNext = nullptr;
    uint32_t driverCount = 0;
    EXPECT_FALSE(driver.gtPinInitializationNeeded.load());
    auto result = zeInitDrivers(&driverCount, nullptr, &desc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, driver.initCalledCount);
    EXPECT_EQ(1u, driver.initializeCalledCount);
    EXPECT_EQ(0u, gtpinInitTimesCalled);
    EXPECT_TRUE(driver.gtPinInitializationNeeded.load());
    driver.gtPinInitializationNeeded = false;
    driverCount = 0;
    result = zeInitDrivers(&driverCount, nullptr, &desc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, driver.initCalledCount);
    EXPECT_EQ(1u, driver.initializeCalledCount);
    EXPECT_EQ(0u, gtpinInitTimesCalled);
    EXPECT_FALSE(driver.gtPinInitializationNeeded.load());
}

TEST_F(GtPinInitTest, givenRequirementForGtpinWhenCallingZeInitDriversWithoutDriverHandlesRequestedAndCountZeroMultipleTimesThenGtPinIsNotInitialized) {
    ze_init_driver_type_desc_t desc = {ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC};
    desc.flags = ZE_INIT_DRIVER_TYPE_FLAG_GPU;
    desc.pNext = nullptr;
    uint32_t driverCount = 0;
    EXPECT_FALSE(driver.gtPinInitializationNeeded.load());
    auto result = zeInitDrivers(&driverCount, nullptr, &desc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, driver.initCalledCount);
    EXPECT_EQ(1u, driver.initializeCalledCount);
    EXPECT_EQ(0u, gtpinInitTimesCalled);
    EXPECT_TRUE(driver.gtPinInitializationNeeded.load());
    driver.gtPinInitializationNeeded = false;
    driverCount = 0;
    ze_driver_handle_t driverHandle{};
    result = zeInitDrivers(&driverCount, &driverHandle, &desc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, driver.initCalledCount);
    EXPECT_EQ(1u, driver.initializeCalledCount);
    EXPECT_EQ(0u, gtpinInitTimesCalled);
    EXPECT_FALSE(driver.gtPinInitializationNeeded.load());
}

TEST_F(GtPinInitTest, givenRequirementForGtpinWhenCallingZeInitDriversMultipleTimesThenGtPinIsInitializedOnlyOnce) {
    ze_init_driver_type_desc_t desc = {ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC};
    desc.flags = ZE_INIT_DRIVER_TYPE_FLAG_GPU;
    desc.pNext = nullptr;
    uint32_t driverCount = 0;
    ze_driver_handle_t driverHandle{};
    auto result = zeInitDrivers(&driverCount, nullptr, &desc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, driver.initCalledCount);
    EXPECT_EQ(1u, driver.initializeCalledCount);
    EXPECT_EQ(0u, gtpinInitTimesCalled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(1u, driverCount);
    result = zeInitDrivers(&driverCount, &driverHandle, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(1u, driverCount);
    EXPECT_EQ(globalDriverHandle, driverHandle);

    EXPECT_EQ(2u, driver.initCalledCount);
    EXPECT_EQ(1u, driver.initializeCalledCount);
    EXPECT_EQ(1u, gtpinInitTimesCalled);
}

TEST_F(GtPinInitTest, givenFailureWhenInitializingGtpinThenTheErrorIsNotExposedInZeInitDriversFunction) {

    uint32_t (*gtPinInit)(void *) = [](void *arg) -> uint32_t {
        return 1; // failure
    };

    auto osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap["OpenGTPin"] = reinterpret_cast<void *>(gtPinInit);
    ze_init_driver_type_desc_t desc = {ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC};
    desc.flags = ZE_INIT_DRIVER_TYPE_FLAG_GPU;
    desc.pNext = nullptr;
    uint32_t driverCount = 0;
    ze_driver_handle_t driverHandle{};
    auto result = zeInitDrivers(&driverCount, nullptr, &desc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, driver.initCalledCount);
    EXPECT_EQ(1u, driver.initializeCalledCount);
    ASSERT_EQ(1u, driverCount);
    result = zeInitDrivers(&driverCount, &driverHandle, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(1u, driverCount);
    EXPECT_EQ(globalDriverHandle, driverHandle);
}

TEST(InitDriversTest, givenZeInitDriversCalledWhenCallingZeInitDriversInForkedProcessThenNewDriverIsInitialized) {
    Mock<Driver> driver;
    uint32_t driverCount = 0;
    ze_init_driver_type_desc_t desc = {ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC};
    desc.flags = ZE_INIT_DRIVER_TYPE_FLAG_GPU;
    desc.pNext = nullptr;
    driver.pid = NEO::SysCalls::getCurrentProcessId();

    ze_result_t result = zeInitDrivers(&driverCount, nullptr, &desc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // change pid in driver
    driver.pid = NEO::SysCalls::getCurrentProcessId() - 1;

    result = zeInitDrivers(&driverCount, nullptr, &desc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(levelZeroDriverInitialized);
    EXPECT_EQ(2u, driver.initCalledCount);

    // pid updated to current pid
    auto expectedPid = NEO::SysCalls::getCurrentProcessId();
    EXPECT_EQ(expectedPid, driver.pid);
}

TEST(InitDriversTest, givenNullDriverHandlePointerWhenInitDriversIsCalledWithACountThenErrorInvalidNullPointerIsReturned) {
    Mock<Driver> driver;
    driver.driverGetCallBase = false;
    ze_init_driver_type_desc_t desc = {ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC};
    desc.flags = ZE_INIT_DRIVER_TYPE_FLAG_GPU;
    desc.pNext = nullptr;
    uint32_t driverCount = 0;
    auto result = zeInitDrivers(&driverCount, nullptr, &desc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, driver.initCalledCount);
    ASSERT_EQ(1u, driverCount);
    result = zeInitDrivers(&driverCount, nullptr, &desc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, result);
}

TEST(InitDriversTest, givennInitDriversIsCalledWhenDriverinitFailsThenUninitializedDriverIsReturned) {
    Mock<Driver> driver;
    driver.failInitDriver = true;
    ze_init_driver_type_desc_t desc = {ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC};
    desc.flags = ZE_INIT_DRIVER_TYPE_FLAG_GPU;
    desc.pNext = nullptr;
    uint32_t driverCount = 0;
    auto result = zeInitDrivers(&driverCount, nullptr, &desc);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
}

TEST(InitDriversTest, givenAllPossibleFlagCombinationsWhenInitDriversIsCalledThenCorrectResultsAreReturned) {
    struct TestCase {
        uint32_t flags;
        ze_result_t expectedResult;
        bool expectDriverHandle;
    };

    std::vector<TestCase> testCases = {
        {0, ZE_RESULT_ERROR_UNINITIALIZED, false},
        {ZE_INIT_DRIVER_TYPE_FLAG_GPU, ZE_RESULT_SUCCESS, true},
        {ZE_INIT_DRIVER_TYPE_FLAG_GPU | ZE_INIT_DRIVER_TYPE_FLAG_NPU, ZE_RESULT_SUCCESS, true},
        {UINT32_MAX, ZE_RESULT_SUCCESS, true},
        {ZE_INIT_DRIVER_TYPE_FLAG_NPU, ZE_RESULT_ERROR_UNINITIALIZED, false},
        {UINT32_MAX & !ZE_INIT_DRIVER_TYPE_FLAG_GPU, ZE_RESULT_ERROR_UNINITIALIZED, false}};

    for (const auto &testCase : testCases) {
        Mock<Driver> driver;
        driver.driverGetCallBase = false;
        uint32_t count = 0;
        ze_driver_handle_t driverHandle = nullptr;
        ze_init_driver_type_desc_t desc = {ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC};
        desc.flags = testCase.flags;
        desc.pNext = nullptr;

        ze_result_t result = zeInitDrivers(&count, nullptr, &desc);
        EXPECT_EQ(testCase.expectedResult, result);
        if (testCase.expectDriverHandle) {
            EXPECT_GT(count, 0u);
            result = zeInitDrivers(&count, &driverHandle, &desc);
            EXPECT_EQ(testCase.expectedResult, result);
            EXPECT_NE(nullptr, driverHandle);
            EXPECT_TRUE(levelZeroDriverInitialized);
        } else {
            EXPECT_EQ(0U, count);
            EXPECT_EQ(nullptr, driverHandle);
        }
    }
}

} // namespace ult
} // namespace L0
