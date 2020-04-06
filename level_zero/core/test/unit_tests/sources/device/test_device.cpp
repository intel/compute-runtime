/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/mocks/mock_device.h"

#include "test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"

#include <memory>

namespace L0 {
namespace ult {

struct DeviceTest : public ::testing::Test {
    void SetUp() override {
        DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), rootDeviceIndex);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::Device *neoDevice = nullptr;
    L0::Device *device = nullptr;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
};

TEST_F(DeviceTest, givenEmptySVmAllocStorageWhenAllocateManagedMemoryFromHostPtrThenBufferHostAllocationIsCreated) {
    int data;
    auto allocation = device->allocateManagedMemoryFromHostPtr(&data, sizeof(data), nullptr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(NEO::GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, allocation->getAllocationType());
    EXPECT_EQ(rootDeviceIndex, allocation->getRootDeviceIndex());
    neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
}

TEST_F(DeviceTest, givenEmptySVmAllocStorageWhenAllocateMemoryFromHostPtrThenValidExternalHostPtrAllocationIsCreated) {
    DebugManager.flags.EnableHostPtrTracking.set(0);
    constexpr auto dataSize = 1024u;
    auto data = std::make_unique<int[]>(dataSize);

    constexpr auto allocationSize = sizeof(int) * dataSize;

    auto allocation = device->allocateMemoryFromHostPtr(data.get(), allocationSize);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(NEO::GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR, allocation->getAllocationType());
    EXPECT_EQ(rootDeviceIndex, allocation->getRootDeviceIndex());

    auto alignedPtr = alignDown(data.get(), MemoryConstants::pageSize);
    auto offsetInPage = ptrDiff(data.get(), alignedPtr);

    EXPECT_EQ(allocation->getAllocationOffset(), offsetInPage);
    EXPECT_EQ(allocation->getUnderlyingBufferSize(), allocationSize);
    EXPECT_EQ(allocation->isFlushL3Required(), true);

    neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
}

TEST_F(DeviceTest, givenKernelPropertiesStructureWhenKernelPropertiesCalledThenAllPropertiesAreAssigned) {
    ze_device_kernel_properties_t kernelProperties, kernelPropertiesBefore;
    memset(&kernelProperties, std::numeric_limits<int>::max(), sizeof(ze_device_kernel_properties_t));
    kernelPropertiesBefore = kernelProperties;
    device->getKernelProperties(&kernelProperties);

    EXPECT_NE(kernelPropertiesBefore.spirvVersionSupported, kernelProperties.spirvVersionSupported);
    EXPECT_NE(kernelPropertiesBefore.nativeKernelSupported.id, kernelProperties.nativeKernelSupported.id);
    EXPECT_NE(kernelPropertiesBefore.fp16Supported, kernelProperties.fp16Supported);
    EXPECT_NE(kernelPropertiesBefore.fp64Supported, kernelProperties.fp64Supported);
    EXPECT_NE(kernelPropertiesBefore.int64AtomicsSupported, kernelProperties.int64AtomicsSupported);
    EXPECT_NE(kernelPropertiesBefore.maxArgumentsSize, kernelProperties.maxArgumentsSize);
    EXPECT_NE(kernelPropertiesBefore.printfBufferSize, kernelProperties.printfBufferSize);
}

TEST_F(DeviceTest, givenDeviceWithCopyEngineThenNumAsyncCopyEnginesDevicePropertyIsCorrectlyReturned) {
    ze_device_properties_t deviceProperties;
    deviceProperties.version = ZE_DEVICE_PROPERTIES_VERSION_CURRENT;
    deviceProperties.numAsyncCopyEngines = std::numeric_limits<int>::max();
    device->getProperties(&deviceProperties);

    uint32_t expecteNumOfCopyEngines = device->getNEODevice()->getHardwareInfo().capabilityTable.blitterOperationsSupported ? 1 : 0;
    EXPECT_EQ(expecteNumOfCopyEngines, deviceProperties.numAsyncCopyEngines);
}

TEST_F(DeviceTest, givenDevicePropertiesStructureWhenDevicePropertiesCalledThenAllPropertiesAreAssigned) {
    ze_device_properties_t deviceProperties, devicePropertiesBefore;

    deviceProperties.type = ZE_DEVICE_TYPE_FPGA;
    memset(&deviceProperties.vendorId, std::numeric_limits<int>::max(), sizeof(deviceProperties.vendorId));
    memset(&deviceProperties.deviceId, std::numeric_limits<int>::max(), sizeof(deviceProperties.deviceId));
    memset(&deviceProperties.uuid, std::numeric_limits<int>::max(), sizeof(deviceProperties.uuid));
    memset(&deviceProperties.isSubdevice, std::numeric_limits<int>::max(), sizeof(deviceProperties.isSubdevice));
    memset(&deviceProperties.subdeviceId, std::numeric_limits<int>::max(), sizeof(deviceProperties.subdeviceId));
    memset(&deviceProperties.coreClockRate, std::numeric_limits<int>::max(), sizeof(deviceProperties.coreClockRate));
    memset(&deviceProperties.unifiedMemorySupported, std::numeric_limits<int>::max(), sizeof(deviceProperties.unifiedMemorySupported));
    memset(&deviceProperties.eccMemorySupported, std::numeric_limits<int>::max(), sizeof(deviceProperties.eccMemorySupported));
    memset(&deviceProperties.onDemandPageFaultsSupported, std::numeric_limits<int>::max(), sizeof(deviceProperties.onDemandPageFaultsSupported));
    memset(&deviceProperties.maxCommandQueues, std::numeric_limits<int>::max(), sizeof(deviceProperties.maxCommandQueues));
    memset(&deviceProperties.numAsyncComputeEngines, std::numeric_limits<int>::max(), sizeof(deviceProperties.numAsyncComputeEngines));
    memset(&deviceProperties.numAsyncCopyEngines, std::numeric_limits<int>::max(), sizeof(deviceProperties.numAsyncCopyEngines));
    memset(&deviceProperties.maxCommandQueuePriority, std::numeric_limits<int>::max(), sizeof(deviceProperties.maxCommandQueuePriority));
    memset(&deviceProperties.numThreadsPerEU, std::numeric_limits<int>::max(), sizeof(deviceProperties.numThreadsPerEU));
    memset(&deviceProperties.physicalEUSimdWidth, std::numeric_limits<int>::max(), sizeof(deviceProperties.physicalEUSimdWidth));
    memset(&deviceProperties.numEUsPerSubslice, std::numeric_limits<int>::max(), sizeof(deviceProperties.numEUsPerSubslice));
    memset(&deviceProperties.numSubslicesPerSlice, std::numeric_limits<int>::max(), sizeof(deviceProperties.numSubslicesPerSlice));
    memset(&deviceProperties.numSlices, std::numeric_limits<int>::max(), sizeof(deviceProperties.numSlices));
    memset(&deviceProperties.timerResolution, std::numeric_limits<int>::max(), sizeof(deviceProperties.timerResolution));
    memset(&deviceProperties.name, std::numeric_limits<int>::max(), sizeof(deviceProperties.name));

    devicePropertiesBefore = deviceProperties;
    device->getProperties(&deviceProperties);

    EXPECT_NE(deviceProperties.type, devicePropertiesBefore.type);
    EXPECT_NE(deviceProperties.vendorId, devicePropertiesBefore.vendorId);
    EXPECT_NE(deviceProperties.deviceId, devicePropertiesBefore.deviceId);
    EXPECT_NE(0, memcmp(&deviceProperties.uuid, &devicePropertiesBefore.uuid, sizeof(devicePropertiesBefore.uuid)));
    EXPECT_NE(deviceProperties.isSubdevice, devicePropertiesBefore.isSubdevice);
    EXPECT_NE(deviceProperties.subdeviceId, devicePropertiesBefore.subdeviceId);
    EXPECT_NE(deviceProperties.coreClockRate, devicePropertiesBefore.coreClockRate);
    EXPECT_NE(deviceProperties.unifiedMemorySupported, devicePropertiesBefore.unifiedMemorySupported);
    EXPECT_NE(deviceProperties.eccMemorySupported, devicePropertiesBefore.eccMemorySupported);
    EXPECT_NE(deviceProperties.onDemandPageFaultsSupported, devicePropertiesBefore.onDemandPageFaultsSupported);
    EXPECT_NE(deviceProperties.maxCommandQueues, devicePropertiesBefore.maxCommandQueues);
    EXPECT_NE(deviceProperties.numAsyncComputeEngines, devicePropertiesBefore.numAsyncComputeEngines);
    EXPECT_NE(deviceProperties.numAsyncCopyEngines, devicePropertiesBefore.numAsyncCopyEngines);
    EXPECT_NE(deviceProperties.maxCommandQueuePriority, devicePropertiesBefore.maxCommandQueuePriority);
    EXPECT_NE(deviceProperties.numThreadsPerEU, devicePropertiesBefore.numThreadsPerEU);
    EXPECT_NE(deviceProperties.physicalEUSimdWidth, devicePropertiesBefore.physicalEUSimdWidth);
    EXPECT_NE(deviceProperties.numEUsPerSubslice, devicePropertiesBefore.numEUsPerSubslice);
    EXPECT_NE(deviceProperties.numSubslicesPerSlice, devicePropertiesBefore.numSubslicesPerSlice);
    EXPECT_NE(deviceProperties.numSlices, devicePropertiesBefore.numSlices);
    EXPECT_NE(deviceProperties.timerResolution, devicePropertiesBefore.timerResolution);
    EXPECT_NE(0, memcmp(&deviceProperties.name, &devicePropertiesBefore.name, sizeof(devicePropertiesBefore.name)));
}

struct MockMemoryManagerMultiDevice : public MemoryManagerMock {
    MockMemoryManagerMultiDevice(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
};

struct MultipleDevicesTest : public ::testing::Test {
    void SetUp() override {
        DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
        }

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), executionEnvironment, i)));
        }

        memoryManager = new ::testing::NiceMock<MockMemoryManagerMultiDevice>(*devices[0].get()->getExecutionEnvironment());
        devices[0].get()->getExecutionEnvironment()->memoryManager.reset(memoryManager);

        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    MockMemoryManagerMultiDevice *memoryManager = nullptr;

    const uint32_t numRootDevices = 2u;
    const uint32_t numSubDevices = 2u;
};

TEST_F(MultipleDevicesTest, givenTheSameDeviceThenCanAccessPeerReturnsTrue) {
    L0::Device *device0 = driverHandle->devices[0];

    ze_bool_t canAccess = false;
    ze_result_t res = device0->canAccessPeer(device0->toHandle(), &canAccess);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(canAccess);
}

TEST_F(MultipleDevicesTest, givenTwoDevicesFromSameFamilyThenCanAccessPeerReturnsTrue) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    GFXCORE_FAMILY device0Family = device0->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    GFXCORE_FAMILY device1Family = device1->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    EXPECT_EQ(device0Family, device1Family);

    ze_bool_t canAccess = false;
    ze_result_t res = device0->canAccessPeer(device1->toHandle(), &canAccess);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(canAccess);
}

TEST_F(MultipleDevicesTest, givenTwoSubDevicesFromTheSameRootDeviceThenCanAccessPeerReturnsTrue) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    uint32_t subDeviceCount = 0;
    ze_result_t res = device0->getSubDevices(&subDeviceCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(numSubDevices, subDeviceCount);

    std::vector<ze_device_handle_t> subDevices0(subDeviceCount);
    res = device0->getSubDevices(&subDeviceCount, subDevices0.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    subDeviceCount = 0;
    res = device1->getSubDevices(&subDeviceCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(numSubDevices, subDeviceCount);

    std::vector<ze_device_handle_t> subDevices1(subDeviceCount);
    res = device1->getSubDevices(&subDeviceCount, subDevices1.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_bool_t canAccess = false;
    L0::Device *subDevice0_0 = Device::fromHandle(subDevices0[0]);
    subDevice0_0->canAccessPeer(subDevices0[1], &canAccess);
    EXPECT_TRUE(canAccess);

    canAccess = false;
    L0::Device *subDevice1_0 = Device::fromHandle(subDevices1[0]);
    subDevice1_0->canAccessPeer(subDevices1[1], &canAccess);
    EXPECT_TRUE(canAccess);
}

struct MultipleDevicesDifferentLocalMemorySupportTest : public MultipleDevicesTest {
    void SetUp() override {
        MultipleDevicesTest::SetUp();
        memoryManager->localMemorySupported[0] = 1;

        deviceWithLocalMemory = driverHandle->devices[0];
        deviceWithoutLocalMemory = driverHandle->devices[1];
    }

    L0::Device *deviceWithLocalMemory = nullptr;
    L0::Device *deviceWithoutLocalMemory = nullptr;
};

TEST_F(MultipleDevicesDifferentLocalMemorySupportTest, givenTwoDevicesWithDifferentLocalMemorySupportThenCanAccessPeerReturnsTrue) {
    ze_bool_t canAccess = false;
    ze_result_t res = deviceWithLocalMemory->canAccessPeer(deviceWithoutLocalMemory->toHandle(), &canAccess);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(canAccess);
}

struct MultipleDevicesDifferentFamilyAndLocalMemorySupportTest : public MultipleDevicesTest {
    void SetUp() override {
        if ((NEO::HwInfoConfig::get(IGFX_SKYLAKE) == nullptr) ||
            (NEO::HwInfoConfig::get(IGFX_KABYLAKE) == nullptr)) {
            GTEST_SKIP();
        }

        MultipleDevicesTest::SetUp();

        memoryManager->localMemorySupported[0] = 1;
        memoryManager->localMemorySupported[1] = 1;

        deviceSKL = driverHandle->devices[0];
        deviceKBL = driverHandle->devices[1];

        deviceSKL->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.eProductFamily = IGFX_SKYLAKE;
        deviceKBL->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.eProductFamily = IGFX_KABYLAKE;
    }

    L0::Device *deviceSKL = nullptr;
    L0::Device *deviceKBL = nullptr;
};

TEST_F(MultipleDevicesDifferentFamilyAndLocalMemorySupportTest, givenTwoDevicesFromDifferentFamiliesThenCanAccessPeerReturnsFalse) {
    PRODUCT_FAMILY deviceSKLFamily = deviceSKL->getNEODevice()->getHardwareInfo().platform.eProductFamily;
    PRODUCT_FAMILY deviceKBLFamily = deviceKBL->getNEODevice()->getHardwareInfo().platform.eProductFamily;
    EXPECT_NE(deviceSKLFamily, deviceKBLFamily);

    ze_bool_t canAccess = true;
    ze_result_t res = deviceSKL->canAccessPeer(deviceKBL->toHandle(), &canAccess);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_FALSE(canAccess);
}

TEST_F(DeviceTest, givenHwInfoAndCopyOnlyFlagWhenCopyOnlyDebugFlagIsDefaultThenUseBliterIsFalse) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    Mock<L0::DeviceImp> l0Device(neoMockDevice, neoDevice->getExecutionEnvironment());
    ze_command_list_desc_t desc = {};
    desc.flags = ZE_COMMAND_LIST_FLAG_COPY_ONLY;
    auto flag = ZE_COMMAND_LIST_FLAG_COPY_ONLY;
    bool useBliter = true;
    ze_result_t res = l0Device.isCreatedCommandListCopyOnly(&desc, &useBliter, flag);
    EXPECT_FALSE(useBliter);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(DeviceTest, givenHwInfoAndCopyOnlyFlagWhenCopyOnlyDebugFlagIsEnabledThenUseBliterIsTrue) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCopyOnlyCommandListsAndCommandQueues.set(true);
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    Mock<L0::DeviceImp> l0Device(neoMockDevice, neoDevice->getExecutionEnvironment());
    ze_command_list_desc_t desc = {};
    desc.flags = ZE_COMMAND_LIST_FLAG_COPY_ONLY;
    auto flag = ZE_COMMAND_LIST_FLAG_COPY_ONLY;
    bool useBliter = false;
    ze_result_t res = l0Device.isCreatedCommandListCopyOnly(&desc, &useBliter, flag);
    EXPECT_TRUE(useBliter);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(DeviceTest, givenHwInfoAndCopyOnlyFlagWhenCopyOnlyDebugFlagIsDisabledThenUseBliterIsFalse) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCopyOnlyCommandListsAndCommandQueues.set(false);
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    Mock<L0::DeviceImp> l0Device(neoMockDevice, neoDevice->getExecutionEnvironment());
    ze_command_list_desc_t desc = {};
    desc.flags = ZE_COMMAND_LIST_FLAG_COPY_ONLY;
    auto flag = ZE_COMMAND_LIST_FLAG_COPY_ONLY;
    bool useBliter = false;
    ze_result_t res = l0Device.isCreatedCommandListCopyOnly(&desc, &useBliter, flag);
    EXPECT_FALSE(useBliter);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

} // namespace ult
} // namespace L0