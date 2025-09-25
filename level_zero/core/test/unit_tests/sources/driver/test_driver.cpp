/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/sip.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/driver_model_type.h"
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
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_os_library.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/source/driver/extension_injector.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/common/ult_helpers_l0.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/host_pointer_manager_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_builtin_functions_lib_impl.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
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

TEST_F(DriverHandleImpTest, givenDriverWhenFindAllocationDataForRangeWithDifferentAllocationsThenReturnFailure) {
    ze_device_mem_alloc_desc_t devDesc = {};
    devDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    void *ptr1 = nullptr;
    void *ptr2 = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(device, &devDesc, 100, 1, &ptr1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(device, &devDesc, 100, 1, &ptr2));
    NEO::SvmAllocationData *allocData{nullptr};
    auto uintPtr1 = castToUint64(ptr1);
    auto uintPtr2 = castToUint64(ptr2);
    bool ret = true;
    if (uintPtr1 > uintPtr2) {
        auto diff = ptrDiff(ptr1, ptr2) + 1;
        ret = driverHandle->findAllocationDataForRange(ptr2, diff, allocData);
    } else {
        auto diff = ptrDiff(ptr2, ptr1) + 1;
        ret = driverHandle->findAllocationDataForRange(ptr1, diff, allocData);
    }
    EXPECT_FALSE(ret);
    context->freeMem(ptr1);
    context->freeMem(ptr2);
}

using DriverVersionTest = Test<DeviceFixture>;
TEST_F(DriverVersionTest, givenCallToGetExtensionPropertiesThenSupportedExtensionsAreReturned) {
    std::vector<std::pair<std::string, uint32_t>> additionalExtensions;
    device->getL0GfxCoreHelper().appendPlatformSpecificExtensions(additionalExtensions, device->getProductHelper(), device->getHwInfo());

    if (device->getL0GfxCoreHelper().synchronizedDispatchSupported() && device->isImplicitScalingCapable()) {
        additionalExtensions.emplace_back(ZE_SYNCHRONIZED_DISPATCH_EXP_NAME, ZE_SYNCHRONIZED_DISPATCH_EXP_VERSION_CURRENT);
    }

    if (device->getNEODevice()->getRootDeviceEnvironment().getBindlessHeapsHelper()) {
        additionalExtensions.emplace_back(ZE_BINDLESS_IMAGE_EXP_NAME, ZE_BINDLESS_IMAGE_EXP_VERSION_CURRENT);
    }
    auto mockReleaseHelperVal = std::unique_ptr<MockReleaseHelper>(new MockReleaseHelper());
    mockReleaseHelperVal->bFloat16Support = true;
    auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironmentRef();
    rootDeviceEnvironment.releaseHelper.reset(mockReleaseHelperVal.release());
    if (device->getNEODevice()->getRootDeviceEnvironment().getReleaseHelper()->isBFloat16ConversionSupported()) {
        additionalExtensions.emplace_back(ZE_BFLOAT16_CONVERSIONS_EXT_NAME, ZE_BFLOAT16_CONVERSIONS_EXT_VERSION_1_0);
    }

    if (device->getProductHelper().isInterruptSupported()) {
        additionalExtensions.emplace_back(ZEX_INTEL_EVENT_SYNC_MODE_EXP_NAME, ZEX_INTEL_EVENT_SYNC_MODE_EXP_VERSION_CURRENT);
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

using DriverExtensionsTest = Test<ExtensionFixture>;

TEST_F(DriverExtensionsTest, whenAskingForExtensionsThenReturnCacheReservationExtBasedOnOs) {
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    uint32_t count = 0;
    ze_result_t res = driverHandle->getExtensionProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    extensionProperties.clear();
    extensionProperties.resize(count);
    res = driverHandle->getExtensionProperties(&count, extensionProperties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    verifyExtensionDefinition(ZE_CACHE_RESERVATION_EXT_NAME, ZE_CACHE_RESERVATION_EXT_VERSION_CURRENT);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());

    count = 0;
    res = driverHandle->getExtensionProperties(&count, nullptr);
    extensionProperties.clear();
    extensionProperties.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = driverHandle->getExtensionProperties(&count, extensionProperties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto it = std::find_if(extensionProperties.begin(), extensionProperties.end(), [](const auto &extension) { return (strcmp(extension.name, ZE_CACHE_RESERVATION_EXT_NAME) == 0); });
    EXPECT_EQ(it, extensionProperties.end());

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset();

    count = 0;
    res = driverHandle->getExtensionProperties(&count, nullptr);
    extensionProperties.clear();
    extensionProperties.resize(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = driverHandle->getExtensionProperties(&count, extensionProperties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    it = std::find_if(extensionProperties.begin(), extensionProperties.end(), [](const auto &extension) { return (strcmp(extension.name, ZE_CACHE_RESERVATION_EXT_NAME) == 0); });
    EXPECT_EQ(it, extensionProperties.end());
}

TEST_F(DriverVersionTest, givenExternalAllocatorWhenCallingGetExtensionPropertiesThenBindlessImageExtensionIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.UseBindlessMode.set(1);
    NEO::debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
    NEO::debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
    NEO::debugManager.flags.EnableHostUsmAllocationPool.set(0);

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
}

TEST_F(DriverVersionTest, WhenGettingDriverVersionThenExpectedDriverVersionIsReturned) {
    ze_driver_properties_t properties{ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES};
    ze_result_t res = driverHandle->getProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto expectedDriverVersion = static_cast<uint32_t>(DriverHandleImp::initialDriverVersionValue);
    expectedDriverVersion += static_cast<uint32_t>(NEO_VERSION_BUILD);
    EXPECT_EQ(expectedDriverVersion, properties.driverVersion);
}

TEST_F(DriverVersionTest, GivenDebugOverrideWhenGettingDriverVersionThenExpectedOverrideDriverVersionIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.OverrideDriverVersion.set(0);

    ze_driver_properties_t properties{ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES};
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

    ze_driver_properties_t properties{ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES};
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

    ze_driver_properties_t properties{ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES};
    ze_result_t res = driverHandle->getProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (uint32_t i = 0; i < 32; i++) {
        ze_driver_properties_t newProperties{ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES};
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
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get());

    uint64_t imageHandle = 0x1;
    NEO::AllocationType allocationType = NEO::AllocationType::bufferHostMemory;
    void *ptr = driverHandle->importNTHandle(device->toHandle(), &imageHandle, allocationType, 0u);
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
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get());

    uint64_t imageHandle = 0x1;
    NEO::AllocationType allocationType = NEO::AllocationType::buffer;
    void *ptr = driverHandle->importNTHandle(device->toHandle(), &imageHandle, allocationType, 0u);
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
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get());

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
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get());

    void *ptr = nullptr;
    auto result = context->allocHostMem(&hostDesc, 100, 1, &ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    auto alloc = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_EQ(alloc->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex())->peekSharedHandle(), NEO::toOsHandle(importNTHandle.handle));
    result = context->freeMem(ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(ImportNTHandleWithMockMemoryManager, whenCallingCreateGraphicsAllocationFromMultipleSharedHandlesFromOsAgnosticMemoryManagerThenNullptrIsReturned) {
    L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
    delete driverHandle->svmAllocsManager;
    driverHandle->setMemoryManager(execEnv->memoryManager.get());
    driverHandle->svmAllocsManager = new NEO::SVMAllocsManager(execEnv->memoryManager.get());
    L0UltHelper::initUsmAllocPools(driverHandle.get());

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

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    auto driverHandle = DriverHandle::create(std::move(devices), L0EnvVariables{}, &returnValue);
    EXPECT_NE(nullptr, driverHandle);
    delete driverHandle;
}

TEST(DriverTest, givenNullEnvVariableWhenCreatingDriverThenEnableProgramDebuggingIsFalse) {

    ze_result_t returnValue;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    L0EnvVariables envVariables = {};
    envVariables.programDebugging = false;

    auto driverHandle = whiteboxCast(static_cast<::L0::DriverHandleImp *>(DriverHandle::create(std::move(devices), envVariables, &returnValue)));
    EXPECT_NE(nullptr, driverHandle);

    EXPECT_EQ(NEO::DebuggingMode::disabled, driverHandle->enableProgramDebugging);

    delete driverHandle;
}

using DriverImpTest = ::testing::Test;

TEST_F(DriverImpTest, givenDriverImpWhenInitializedThenEnvVariablesAreRead) {

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_LE(3u, IoFunctions::mockGetenvCalled);

    auto driverHandle = static_cast<L0::DriverHandleImp *>((*globalDriverHandles)[0]);

    delete driverHandle;
    globalDriverHandles->clear();
}

TEST_F(DriverImpTest, givenMissingMetricApiDependenciesWhenInitializingDriverImpThenGlobalDriverHandleIsNull) {

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();

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
    EXPECT_TRUE(globalDriverHandles->empty());
}

TEST_F(DriverImpTest, givenOneApiPvcSendWarWaEnvWhenCreatingExecutionEnvironmentThenCorrectEnvValueIsStored) {
    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    {
        std::unordered_map<std::string, std::string> mockableEnvs = {{"ONEAPI_PVC_SEND_WAR_WA", "1"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
        DriverImp driverImp;
        driverImp.initialize(&result);

        ASSERT_FALSE(globalDriverHandles->empty());
        auto driverHandle = static_cast<L0::DriverHandleImp *>((*globalDriverHandles)[0]);
        EXPECT_TRUE(driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->isOneApiPvcWaEnv());

        delete driverHandle;
        globalDriverHandles->clear();
    }
    {
        std::unordered_map<std::string, std::string> mockableEnvs = {{"ONEAPI_PVC_SEND_WAR_WA", "0"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
        DriverImp driverImp;
        driverImp.initialize(&result);

        ASSERT_FALSE(globalDriverHandles->empty());
        auto driverHandle = static_cast<L0::DriverHandleImp *>((*globalDriverHandles)[0]);
        EXPECT_FALSE(driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->isOneApiPvcWaEnv());

        delete driverHandle;
        globalDriverHandles->clear();
    }
}

TEST_F(DriverImpTest, givenEnabledProgramDebuggingWhenCreatingExecutionEnvironmentThenDebuggingEnabledIsTrue) {

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_FALSE(globalDriverHandles->empty());
    auto driverHandle = static_cast<L0::DriverHandleImp *>((*globalDriverHandles)[0]);
    EXPECT_TRUE(driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->isDebuggingEnabled());

    delete driverHandle;
    globalDriverHandles->clear();
}

TEST_F(DriverImpTest, givenEnableProgramDebuggingWithValue2WhenCreatingExecutionEnvironmentThenDebuggingEnabledIsTrue) {

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "2"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_FALSE(globalDriverHandles->empty());
    auto driverHandle = static_cast<L0::DriverHandleImp *>((*globalDriverHandles)[0]);
    EXPECT_TRUE(driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->isDebuggingEnabled());

    delete driverHandle;
    globalDriverHandles->clear();
}

TEST_F(DriverImpTest, givenEnabledFP64EmulationWhenCreatingExecutionEnvironmentThenFP64EmulationIsEnabled) {

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"NEO_FP64_EMULATION", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_FALSE(globalDriverHandles->empty());
    auto driverHandle = static_cast<L0::DriverHandleImp *>((*globalDriverHandles)[0]);
    EXPECT_TRUE(driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->isFP64EmulationEnabled());

    delete driverHandle;
    globalDriverHandles->clear();
}

TEST_F(DriverImpTest, givenEnabledProgramDebuggingAndEnabledExperimentalOpenCLWhenCreatingExecutionEnvironmentThenDebuggingEnabledIsFalse) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ExperimentalEnableL0DebuggerForOpenCL.set(true);

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_FALSE(globalDriverHandles->empty());
    auto driverHandle = static_cast<L0::DriverHandleImp *>((*globalDriverHandles)[0]);
    EXPECT_FALSE(driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->isDebuggingEnabled());

    delete driverHandle;
    globalDriverHandles->clear();
}

TEST_F(DriverImpTest, givenEnableProgramDebuggingWithValue2AndEnabledExperimentalOpenCLWhenCreatingExecutionEnvironmentThenDebuggingEnabledIsFalse) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ExperimentalEnableL0DebuggerForOpenCL.set(true);

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "2"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_FALSE(globalDriverHandles->empty());
    auto driverHandle = static_cast<L0::DriverHandleImp *>((*globalDriverHandles)[0]);
    EXPECT_FALSE(driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->isDebuggingEnabled());

    delete driverHandle;
    globalDriverHandles->clear();
}

TEST_F(DriverImpTest, givenNoProgramDebuggingEnvVarWhenCreatingExecutionEnvironmentThenDebuggingEnabledIsFalse) {
    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    ASSERT_FALSE(globalDriverHandles->empty());

    auto driverHandle = static_cast<L0::DriverHandleImp *>((*globalDriverHandles)[0]);
    EXPECT_FALSE(driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->isDebuggingEnabled());

    delete driverHandle;
    globalDriverHandles->clear();
}

TEST(DriverTest, givenProgramDebuggingEnvVarValue1WhenCreatingDriverThenEnableProgramDebuggingIsSetToTrue) {

    ze_result_t returnValue;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    L0EnvVariables envVariables = {};
    envVariables.programDebugging = 1;

    auto driverHandle = whiteboxCast(static_cast<::L0::DriverHandleImp *>(DriverHandle::create(std::move(devices), envVariables, &returnValue)));
    EXPECT_NE(nullptr, driverHandle);

    EXPECT_TRUE(driverHandle->enableProgramDebugging == NEO::DebuggingMode::online);

    delete driverHandle;
}

TEST(DriverTest, givenProgramDebuggingEnvVarValue2WhenCreatingDriverThenEnableProgramDebuggingIsSetToTrue) {

    ze_result_t returnValue;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    L0EnvVariables envVariables = {};
    envVariables.programDebugging = 2;

    auto driverHandle = whiteboxCast(static_cast<::L0::DriverHandleImp *>(DriverHandle::create(std::move(devices), envVariables, &returnValue)));
    EXPECT_NE(nullptr, driverHandle);

    EXPECT_TRUE(driverHandle->enableProgramDebugging == NEO::DebuggingMode::offline);

    delete driverHandle;
}

TEST(DriverTest, whenCreatingDriverThenDefaultContextWithAllDevicesIsCreated) {

    ze_result_t returnValue;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    L0EnvVariables envVariables = {};

    auto driverHandle = whiteboxCast(static_cast<::L0::DriverHandleImp *>(DriverHandle::create(std::move(devices), envVariables, &returnValue)));
    EXPECT_NE(nullptr, driverHandle);

    auto defaultContext = driverHandle->getDefaultContext();
    ASSERT_NE(nullptr, defaultContext);

    auto context = static_cast<WhiteBox<::L0::ContextImp> *>(defaultContext);

    EXPECT_NE(0u, context->numDevices);
    EXPECT_EQ(context->numDevices, context->devices.size());
    EXPECT_EQ(context->devices.size(), driverHandle->devices.size());
    for (auto i = 0u; i < context->numDevices; i++) {
        EXPECT_EQ(context->devices[i], driverHandle->devices[i]);
    }

    delete driverHandle;
}

TEST(DriverTest, givenDriverWhenGetDefaultContextApiIsCalledThenProperHandleIsReturned) {

    ze_result_t returnValue;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    L0EnvVariables envVariables = {};

    auto driverHandle = whiteboxCast(static_cast<::L0::DriverHandleImp *>(DriverHandle::create(std::move(devices), envVariables, &returnValue)));
    EXPECT_NE(nullptr, driverHandle);

    auto defaultContext = zeDriverGetDefaultContext(driverHandle);

    EXPECT_NE(nullptr, defaultContext);

    EXPECT_EQ(defaultContext, driverHandle->getDefaultContext());

    delete driverHandle;
}

TEST(DriverTest, givenBuiltinsAsyncInitEnabledWhenCreatingDriverThenMakeSureBuiltinsInitIsCompletedOnExitOfDriverHandleInitialization) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useinitBuiltinsAsyncEnabled = true;

    ze_result_t returnValue;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();

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

    ASSERT_TRUE(globalDriverHandles->empty());
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

    ASSERT_TRUE(globalDriverHandles->empty());
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

struct MaskArray {
    const std::string masks[4] = {"0", "1", "2", "3"}; // fixture has 4 subDevices
};

struct DriverHandleTest : public ::testing::Test {
    void SetUp() override {
        globalDriverHandles = new std::vector<_ze_driver_handle_t *>;

        ze_result_t returnValue;
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();

        NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

        L0EnvVariables envVariables = {};
        envVariables.programDebugging = true;

        driverHandle = whiteboxCast(DriverHandle::create(std::move(devices), envVariables, &returnValue));
        globalDriverHandles->push_back(driverHandle);
    }
    void TearDown() override {
        delete driverHandle;
        delete globalDriverHandles;
    }
    L0::DriverHandle *driverHandle;
    VariableBackup<decltype(globalDriverHandles)> globalDriverHandleBackup{&globalDriverHandles, nullptr};
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
       givenInitializedDriverWhenZerGetDefaultContextIsCalledThenDefaultContextFromFirstDriverHandleIsReturned) {
    globalDriverHandles->push_back(nullptr);
    auto defaultContext = zerGetDefaultContext();

    EXPECT_EQ(defaultContext, driverHandle->getDefaultContext());
}

TEST_F(DriverHandleTest,
       whenTranslatingNullptrDeviceHandleToIdentifierThenErrorIsPropagated) {
    auto identifier = zerTranslateDeviceHandleToIdentifier(nullptr);

    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), identifier);

    const char *expectedError = "Invalid device handle";
    const char *errorDescription = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetLastErrorDescription(driverHandle->toHandle(), &errorDescription));

    EXPECT_EQ(0, strcmp(expectedError, errorDescription)) << errorDescription;

    errorDescription = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zerGetLastErrorDescription(&errorDescription));
    EXPECT_EQ(0, strcmp(expectedError, errorDescription)) << errorDescription;
}

TEST_F(DriverHandleTest,
       whenTranslatingIncorrectIdentifierToDeviceHandleThenErrorIsPropagated) {

    uint32_t invalidIdentifier = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(nullptr, zerTranslateIdentifierToDeviceHandle(invalidIdentifier));

    const char *expectedError = "Invalid device identifier";

    const char *errorDescription = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetLastErrorDescription(driverHandle->toHandle(), &errorDescription));
    EXPECT_EQ(0, strcmp(expectedError, errorDescription)) << errorDescription;

    errorDescription = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zerGetLastErrorDescription(&errorDescription));
    EXPECT_EQ(0, strcmp(expectedError, errorDescription)) << errorDescription;
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
    auto expectedDriverHandle = (*globalDriverHandles)[0];
    EXPECT_EQ(hDriverHandle, expectedDriverHandle);
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
    driverHandleImp->setupDevicesToExpose();

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
    EXPECT_EQ(ZE_API_VERSION_1_13, version);
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
    ze_driver_properties_t properties{ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES};
    ze_result_t expectedResult = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;

    driverHandle.getPropertiesResult = expectedResult;
    result = zeDriverGetProperties(driverHandle.toHandle(), &properties);
    EXPECT_EQ(expectedResult, result);
    EXPECT_EQ(1u, driverHandle.getPropertiesCalled);
}

using GetDriverPropertiesTest = Test<DeviceFixture>;

TEST_F(GetDriverPropertiesTest, whenGettingDdiHandlesExtensionPropertiesThenSupportCanBeDisabledViaDebugKey) {
    DebugManagerStateRestore restorer;

    ze_driver_properties_t driverProperties = {ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES};
    ze_driver_ddi_handles_ext_properties_t ddiHandlesExtProperties = {ZE_STRUCTURE_TYPE_DRIVER_DDI_HANDLES_EXT_PROPERTIES};
    driverProperties.pNext = &ddiHandlesExtProperties;

    ze_result_t result = driverHandle->getProperties(&driverProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ze_driver_ddi_handle_ext_flag_t::ZE_DRIVER_DDI_HANDLE_EXT_FLAG_DDI_HANDLE_EXT_SUPPORTED, ddiHandlesExtProperties.flags);

    ddiHandlesExtProperties = {ZE_STRUCTURE_TYPE_DRIVER_DDI_HANDLES_EXT_PROPERTIES};
    NEO::debugManager.flags.EnableDdiHandlesExtension.set(0);
    result = driverHandle->getProperties(&driverProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, ddiHandlesExtProperties.flags);

    ddiHandlesExtProperties = {ZE_STRUCTURE_TYPE_DRIVER_DDI_HANDLES_EXT_PROPERTIES};
    NEO::debugManager.flags.EnableDdiHandlesExtension.set(1);
    result = driverHandle->getProperties(&driverProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ze_driver_ddi_handle_ext_flag_t::ZE_DRIVER_DDI_HANDLE_EXT_FLAG_DDI_HANDLE_EXT_SUPPORTED, ddiHandlesExtProperties.flags);
}

TEST_F(GetDriverPropertiesTest, givenBaseStructStypeNotSetWhenGettingDdiHandlesExtensionPropertiesThenSupportIsNotExposedEvenIfDebugKeyIsSet) {
    DebugManagerStateRestore restorer;

    ze_driver_properties_t driverProperties{};
    ze_driver_ddi_handles_ext_properties_t ddiHandlesExtProperties = {ZE_STRUCTURE_TYPE_DRIVER_DDI_HANDLES_EXT_PROPERTIES};
    driverProperties.pNext = &ddiHandlesExtProperties;

    ze_result_t result = driverHandle->getProperties(&driverProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, ddiHandlesExtProperties.flags);

    ddiHandlesExtProperties = {ZE_STRUCTURE_TYPE_DRIVER_DDI_HANDLES_EXT_PROPERTIES};
    NEO::debugManager.flags.EnableDdiHandlesExtension.set(0);
    result = driverHandle->getProperties(&driverProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, ddiHandlesExtProperties.flags);

    ddiHandlesExtProperties = {ZE_STRUCTURE_TYPE_DRIVER_DDI_HANDLES_EXT_PROPERTIES};
    NEO::debugManager.flags.EnableDdiHandlesExtension.set(1);
    result = driverHandle->getProperties(&driverProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, ddiHandlesExtProperties.flags);
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
    decltype(&zexDriverImportExternalPointer) expectedImport = zexDriverImportExternalPointer;
    decltype(&zexDriverReleaseImportedPointer) expectedRelease = zexDriverReleaseImportedPointer;
    decltype(&zexDriverGetHostPointerBaseAddress) expectedGet = zexDriverGetHostPointerBaseAddress;
    decltype(&zeDriverGetDefaultContext) expectedZeDriverGetDefaultContext = zeDriverGetDefaultContext;
    decltype(&zerGetDefaultContext) expectedZerGetDefaultContext = zerGetDefaultContext;
    decltype(&zerGetLastErrorDescription) expectedZerGetLastErrorDescription = zerGetLastErrorDescription;

    decltype(&zerTranslateDeviceHandleToIdentifier) expectedZerTranslateDeviceHandleToIdentifier = zerTranslateDeviceHandleToIdentifier;
    decltype(&zerTranslateIdentifierToDeviceHandle) expectedZerTranslateIdentifierToDeviceHandle = zerTranslateIdentifierToDeviceHandle;
    decltype(&zeDeviceSynchronize) expectedZeDeviceSynchronize = zeDeviceSynchronize;

    decltype(&zeCommandListAppendLaunchKernelWithArguments) expectedZeCommandListAppendLaunchKernelWithArguments = zeCommandListAppendLaunchKernelWithArguments;
    decltype(&zeCommandListAppendLaunchKernelWithParameters) expectedZeCommandListAppendLaunchKernelWithParameters = zeCommandListAppendLaunchKernelWithParameters;

    decltype(&zexKernelGetBaseAddress) expectedKernelGetBaseAddress = zexKernelGetBaseAddress;
    decltype(&zeIntelGetDriverVersionString) expectedIntelGetDriverVersionString = zeIntelGetDriverVersionString;
    decltype(&zeIntelMediaCommunicationCreate) expectedIntelMediaCommunicationCreate = zeIntelMediaCommunicationCreate;
    decltype(&zeIntelMediaCommunicationDestroy) expectedIntelMediaCommunicationDestroy = zeIntelMediaCommunicationDestroy;
    decltype(&zexIntelAllocateNetworkInterrupt) expectedIntelAllocateNetworkInterrupt = zexIntelAllocateNetworkInterrupt;
    decltype(&zexIntelReleaseNetworkInterrupt) expectedIntelReleaseNetworkInterrupt = zexIntelReleaseNetworkInterrupt;
    decltype(&zexMemFreeRegisterCallbackExt) expectedIntelMemFreeRegisterCallbackExt = zexMemFreeRegisterCallbackExt;

    decltype(&zexCounterBasedEventCreate2) expectedCounterBasedEventCreate2 = zexCounterBasedEventCreate2;
    decltype(&zexCounterBasedEventGetIpcHandle) expectedCounterBasedEventGetIpcHandle = zexCounterBasedEventGetIpcHandle;
    decltype(&zexCounterBasedEventOpenIpcHandle) expectedCounterBasedEventOpenIpcHandle = zexCounterBasedEventOpenIpcHandle;
    decltype(&zexCounterBasedEventCloseIpcHandle) expectedCounterBasedEventCloseIpcHandle = zexCounterBasedEventCloseIpcHandle;
    decltype(&zexDeviceGetAggregatedCopyOffloadIncrementValue) expectedZexDeviceGetAggregatedCopyOffloadIncrementValueHandle = zexDeviceGetAggregatedCopyOffloadIncrementValue;

    decltype(&zexCommandListAppendHostFunction) expectedCommandListAppendHostFunction = zexCommandListAppendHostFunction;

    void *funPtr = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexDriverImportExternalPointer", &funPtr));
    EXPECT_EQ(expectedImport, reinterpret_cast<decltype(&zexDriverImportExternalPointer)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexDriverReleaseImportedPointer", &funPtr));
    EXPECT_EQ(expectedRelease, reinterpret_cast<decltype(&zexDriverReleaseImportedPointer)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexDriverGetHostPointerBaseAddress", &funPtr));
    EXPECT_EQ(expectedGet, reinterpret_cast<decltype(&zexDriverGetHostPointerBaseAddress)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zeDriverGetDefaultContext", &funPtr));
    EXPECT_EQ(expectedZeDriverGetDefaultContext, reinterpret_cast<decltype(&zeDriverGetDefaultContext)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zerGetDefaultContext", &funPtr));
    EXPECT_EQ(expectedZerGetDefaultContext, reinterpret_cast<decltype(&zerGetDefaultContext)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zerGetLastErrorDescription", &funPtr));
    EXPECT_EQ(expectedZerGetLastErrorDescription, reinterpret_cast<decltype(&zerGetLastErrorDescription)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zerTranslateDeviceHandleToIdentifier", &funPtr));
    EXPECT_EQ(expectedZerTranslateDeviceHandleToIdentifier, reinterpret_cast<decltype(&zerTranslateDeviceHandleToIdentifier)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zerTranslateIdentifierToDeviceHandle", &funPtr));
    EXPECT_EQ(expectedZerTranslateIdentifierToDeviceHandle, reinterpret_cast<decltype(&zerTranslateIdentifierToDeviceHandle)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zeDeviceSynchronize", &funPtr));
    EXPECT_EQ(expectedZeDeviceSynchronize, reinterpret_cast<decltype(&zeDeviceSynchronize)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zeCommandListAppendLaunchKernelWithArguments", &funPtr));
    EXPECT_EQ(expectedZeCommandListAppendLaunchKernelWithArguments, reinterpret_cast<decltype(&zeCommandListAppendLaunchKernelWithArguments)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zeCommandListAppendLaunchKernelWithParameters", &funPtr));
    EXPECT_EQ(expectedZeCommandListAppendLaunchKernelWithParameters, reinterpret_cast<decltype(&zeCommandListAppendLaunchKernelWithParameters)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexKernelGetBaseAddress", &funPtr));
    EXPECT_EQ(expectedKernelGetBaseAddress, reinterpret_cast<decltype(&zexKernelGetBaseAddress)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zeIntelGetDriverVersionString", &funPtr));
    EXPECT_EQ(expectedIntelGetDriverVersionString, reinterpret_cast<decltype(&zeIntelGetDriverVersionString)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zeIntelMediaCommunicationCreate", &funPtr));
    EXPECT_EQ(expectedIntelMediaCommunicationCreate, reinterpret_cast<decltype(&zeIntelMediaCommunicationCreate)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zeIntelMediaCommunicationDestroy", &funPtr));
    EXPECT_EQ(expectedIntelMediaCommunicationDestroy, reinterpret_cast<decltype(&zeIntelMediaCommunicationDestroy)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexMemFreeRegisterCallbackExt", &funPtr));
    EXPECT_EQ(expectedIntelMemFreeRegisterCallbackExt, reinterpret_cast<decltype(&zexMemFreeRegisterCallbackExt)>(funPtr));

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

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexDeviceGetAggregatedCopyOffloadIncrementValue", &funPtr));
    EXPECT_EQ(expectedZexDeviceGetAggregatedCopyOffloadIncrementValueHandle, reinterpret_cast<decltype(&zexDeviceGetAggregatedCopyOffloadIncrementValue)>(funPtr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListAppendHostFunction", &funPtr));
    EXPECT_EQ(expectedCommandListAppendHostFunction, reinterpret_cast<decltype(&zexCommandListAppendHostFunction)>(funPtr));
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
        globalDriverHandles = new std::vector<_ze_driver_handle_t *>;
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
            auto expectedDriverHandle = (*globalDriverHandles)[0];
            EXPECT_EQ(expectedDriverHandle, driverHandle);
            if (expectedDriverHandle != driverHandle) {
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
        for (auto &driverHandle : *globalDriverHandles) {
            delete static_cast<BaseDriver *>(driverHandle);
        }
        delete globalDriverHandles;
        globalDriverHandles = nullptr;
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
    VariableBackup<decltype(globalDriverHandles)> globalDriverHandleBackup{&globalDriverHandles, nullptr};
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
    auto expectedDriverHandle = (*globalDriverHandles)[0];
    EXPECT_EQ(expectedDriverHandle, driverHandle);

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
    auto expectedDriverHandle = (*globalDriverHandles)[0];
    EXPECT_EQ(expectedDriverHandle, driverHandle);
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
    auto expectedDriverHandle = (*globalDriverHandles)[0];
    EXPECT_EQ(expectedDriverHandle, driverHandle);

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
    auto expectedDriverHandle = (*globalDriverHandles)[0];
    EXPECT_EQ(expectedDriverHandle, driverHandle);
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

TEST(MultiDriverHandleTest, givenMultiplesDifferentDevicesWhenGetDriverHandleThenSeparateDriverHandleIsReturnedPerEachProductFamily) {
    if (!NEO::hardwareInfoSetup[IGFX_LUNARLAKE] ||
        !NEO::hardwareInfoSetup[IGFX_BMG] ||
        !NEO::hardwareInfoSetup[IGFX_PTL]) {
        GTEST_SKIP();
    }

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    const size_t numRootDevices = 5u;
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, numRootDevices);

    executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_LUNARLAKE;
    executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    executionEnvironment.rootDeviceEnvironments[1]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_BMG;
    executionEnvironment.rootDeviceEnvironments[1]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
    executionEnvironment.rootDeviceEnvironments[2]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_LUNARLAKE;
    executionEnvironment.rootDeviceEnvironments[2]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    executionEnvironment.rootDeviceEnvironments[3]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_LUNARLAKE;
    executionEnvironment.rootDeviceEnvironments[3]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    executionEnvironment.rootDeviceEnvironments[4]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_PTL;
    executionEnvironment.rootDeviceEnvironments[4]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;

    ultHwConfig.sourceExecutionEnvironment = &executionEnvironment;

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_FALSE(globalDriverHandles->empty());
    uint32_t numDrivers = 0;
    zeDriverGet(&numDrivers, nullptr);

    ASSERT_EQ(3u, numDrivers);

    ze_driver_handle_t drivers[3];
    zeDriverGet(&numDrivers, drivers);

    auto driver0 = static_cast<L0::DriverHandleImp *>(drivers[0]);
    auto driver1 = static_cast<L0::DriverHandleImp *>(drivers[1]);
    auto driver2 = static_cast<L0::DriverHandleImp *>(drivers[2]);

    EXPECT_EQ(1u, driver0->devices.size());
    EXPECT_EQ(IGFX_BMG, driver0->devices[0]->getHwInfo().platform.eProductFamily);

    EXPECT_EQ(1u, driver1->devices.size());
    EXPECT_EQ(IGFX_PTL, driver1->devices[0]->getHwInfo().platform.eProductFamily);

    EXPECT_EQ(3u, driver2->devices.size());
    EXPECT_EQ(IGFX_LUNARLAKE, driver2->devices[0]->getHwInfo().platform.eProductFamily);
    EXPECT_EQ(IGFX_LUNARLAKE, driver2->devices[1]->getHwInfo().platform.eProductFamily);
    EXPECT_EQ(IGFX_LUNARLAKE, driver2->devices[2]->getHwInfo().platform.eProductFamily);

    delete driver0;
    delete driver1;
    delete driver2;

    globalDriverHandles->clear();
}

TEST_F(DriverExtensionsTest, givenDriverHandleWhenAskingForExtensionsThenReturnCorrectVersions) {
    verifyExtensionDefinition(ZE_FLOAT_ATOMICS_EXT_NAME, ZE_FLOAT_ATOMICS_EXT_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_RELAXED_ALLOCATION_LIMITS_EXP_NAME, ZE_RELAXED_ALLOCATION_LIMITS_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_MODULE_PROGRAM_EXP_NAME, ZE_MODULE_PROGRAM_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_KERNEL_SCHEDULING_HINTS_EXP_NAME, ZE_SCHEDULING_HINTS_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_GLOBAL_OFFSET_EXP_NAME, ZE_GLOBAL_OFFSET_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_PCI_PROPERTIES_EXT_NAME, ZE_PCI_PROPERTIES_EXT_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_MEMORY_COMPRESSION_HINTS_EXT_NAME, ZE_MEMORY_COMPRESSION_HINTS_EXT_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_MEMORY_FREE_POLICIES_EXT_NAME, ZE_MEMORY_FREE_POLICIES_EXT_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_DEVICE_MEMORY_PROPERTIES_EXT_NAME, ZE_DEVICE_MEMORY_PROPERTIES_EXT_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_RAYTRACING_EXT_NAME, ZE_RAYTRACING_EXT_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_CONTEXT_POWER_SAVING_HINT_EXP_NAME, ZE_POWER_SAVING_HINT_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_DEVICE_LUID_EXT_NAME, ZE_DEVICE_LUID_EXT_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_DEVICE_IP_VERSION_EXT_NAME, ZE_DEVICE_IP_VERSION_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_IMAGE_COPY_EXT_NAME, ZE_IMAGE_COPY_EXT_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_IMAGE_VIEW_EXT_NAME, ZE_IMAGE_VIEW_EXT_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_IMAGE_VIEW_PLANAR_EXT_NAME, ZE_IMAGE_VIEW_PLANAR_EXT_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_EVENT_QUERY_KERNEL_TIMESTAMPS_EXT_NAME, ZE_EVENT_QUERY_KERNEL_TIMESTAMPS_EXT_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_RTAS_BUILDER_EXP_NAME, ZE_RTAS_BUILDER_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_KERNEL_MAX_GROUP_SIZE_PROPERTIES_EXT_NAME, ZE_KERNEL_MAX_GROUP_SIZE_PROPERTIES_EXT_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_LINKAGE_INSPECTION_EXT_NAME, ZE_LINKAGE_INSPECTION_EXT_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_IMMEDIATE_COMMAND_LIST_APPEND_EXP_NAME, ZE_IMMEDIATE_COMMAND_LIST_APPEND_EXP_VERSION_1_0);
    verifyExtensionDefinition(ZE_GET_KERNEL_BINARY_EXP_NAME, ZE_KERNEL_GET_BINARY_EXP_VERSION_1_0);
    verifyExtensionDefinition(ZE_EXTERNAL_SEMAPHORES_EXTENSION_NAME, ZE_EXTERNAL_SEMAPHORE_EXT_VERSION_1_0);
    verifyExtensionDefinition(ZE_CACHELINE_SIZE_EXT_NAME, ZE_DEVICE_CACHE_LINE_SIZE_EXT_VERSION_1_0);
    verifyExtensionDefinition(ZE_DEVICE_VECTOR_SIZES_EXT_NAME, ZE_DEVICE_VECTOR_SIZES_EXT_VERSION_1_0);
    verifyExtensionDefinition(ZE_MUTABLE_COMMAND_LIST_EXP_NAME, ZE_MUTABLE_COMMAND_LIST_EXP_VERSION_1_1);
    verifyExtensionDefinition(ZE_RTAS_EXT_NAME, ZE_RTAS_BUILDER_EXT_VERSION_1_0);
    verifyExtensionDefinition(ZE_DRIVER_DDI_HANDLES_EXT_NAME, ZE_DRIVER_DDI_HANDLES_EXT_VERSION_1_0);
    verifyExtensionDefinition(ZE_EU_COUNT_EXT_NAME, ZE_EU_COUNT_EXT_VERSION_1_0);
    verifyExtensionDefinition(ZE_RECORD_REPLAY_GRAPH_EXP_NAME, ZE_RECORD_REPLAY_GRAPH_EXP_VERSION_1_0);

    // Driver experimental extensions
    verifyExtensionDefinition(ZE_INTEL_DEVICE_MODULE_DP_PROPERTIES_EXP_NAME, ZE_INTEL_DEVICE_MODULE_DP_PROPERTIES_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_INTEL_GET_DRIVER_VERSION_STRING_EXP_NAME, ZE_INTEL_GET_DRIVER_VERSION_STRING_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_INTEL_DEVICE_BLOCK_ARRAY_EXP_NAME, ZE_INTEL_DEVICE_BLOCK_ARRAY_EXP_PROPERTIES_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_INTEL_KERNEL_GET_PROGRAM_BINARY_EXP_NAME, ZE_INTEL_KERNEL_GET_PROGRAM_BINARY_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_IMAGE_MEMORY_PROPERTIES_EXP_NAME, ZE_IMAGE_MEMORY_PROPERTIES_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_IMAGE_QUERY_ALLOC_PROPERTIES_EXT_NAME, ZE_IMAGE_QUERY_ALLOC_PROPERTIES_EXT_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_SUB_ALLOCATIONS_EXP_NAME, ZE_SUB_ALLOCATIONS_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZE_EVENT_QUERY_TIMESTAMPS_EXP_NAME, ZE_EVENT_QUERY_TIMESTAMPS_EXP_VERSION_CURRENT);
    verifyExtensionDefinition(ZEX_INTEL_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_NAME, ZEX_INTEL_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_VERSION_CURRENT);
}

} // namespace ult
} // namespace L0
