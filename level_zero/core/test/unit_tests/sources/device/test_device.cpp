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

#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"

#include <memory>

namespace L0 {
namespace ult {

TEST(L0DeviceTest, GivenDualStorageSharedMemorySupportedWhenCreatingDeviceThenPageFaultCmdListImmediateWithInitializedCmdQIsCreated) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(1);

    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrLocalMemory = true;
    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), 1, false));
    ASSERT_NE(nullptr, device);
    auto deviceImp = static_cast<DeviceImp *>(device.get());
    ASSERT_NE(nullptr, deviceImp->pageFaultCommandList);

    ASSERT_NE(nullptr, deviceImp->pageFaultCommandList->cmdQImmediate);
    EXPECT_NE(nullptr, static_cast<CommandQueueImp *>(deviceImp->pageFaultCommandList->cmdQImmediate)->getCsr());
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS, static_cast<CommandQueueImp *>(deviceImp->pageFaultCommandList->cmdQImmediate)->getSynchronousMode());
}

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
    const auto &hardwareInfo = this->neoDevice->getHardwareInfo();

    ze_device_module_properties_t kernelProperties, kernelPropertiesBefore;
    memset(&kernelProperties, std::numeric_limits<int>::max(), sizeof(ze_device_module_properties_t));
    kernelPropertiesBefore = kernelProperties;
    device->getKernelProperties(&kernelProperties);

    EXPECT_NE(kernelPropertiesBefore.spirvVersionSupported, kernelProperties.spirvVersionSupported);
    EXPECT_NE(kernelPropertiesBefore.nativeKernelSupported.id, kernelProperties.nativeKernelSupported.id);

    EXPECT_TRUE(kernelPropertiesBefore.flags & ZE_DEVICE_MODULE_FLAG_FP16);
    if (hardwareInfo.capabilityTable.ftrSupportsInteger64BitAtomics) {
        EXPECT_TRUE(kernelPropertiesBefore.flags & ZE_DEVICE_MODULE_FLAG_INT64_ATOMICS);
    }

    EXPECT_NE(kernelPropertiesBefore.maxArgumentsSize, kernelProperties.maxArgumentsSize);
    EXPECT_NE(kernelPropertiesBefore.printfBufferSize, kernelProperties.printfBufferSize);
}

TEST_F(DeviceTest, givenDevicePropertiesStructureWhenDevicePropertiesCalledThenAllPropertiesAreAssigned) {
    ze_device_properties_t deviceProperties, devicePropertiesBefore;

    deviceProperties.type = ZE_DEVICE_TYPE_FPGA;
    memset(&deviceProperties.vendorId, std::numeric_limits<int>::max(), sizeof(deviceProperties.vendorId));
    memset(&deviceProperties.deviceId, std::numeric_limits<int>::max(), sizeof(deviceProperties.deviceId));
    memset(&deviceProperties.uuid, std::numeric_limits<int>::max(), sizeof(deviceProperties.uuid));
    memset(&deviceProperties.subdeviceId, std::numeric_limits<int>::max(), sizeof(deviceProperties.subdeviceId));
    memset(&deviceProperties.coreClockRate, std::numeric_limits<int>::max(), sizeof(deviceProperties.coreClockRate));
    memset(&deviceProperties.maxCommandQueuePriority, std::numeric_limits<int>::max(), sizeof(deviceProperties.maxCommandQueuePriority));
    memset(&deviceProperties.numThreadsPerEU, std::numeric_limits<int>::max(), sizeof(deviceProperties.numThreadsPerEU));
    memset(&deviceProperties.physicalEUSimdWidth, std::numeric_limits<int>::max(), sizeof(deviceProperties.physicalEUSimdWidth));
    memset(&deviceProperties.numEUsPerSubslice, std::numeric_limits<int>::max(), sizeof(deviceProperties.numEUsPerSubslice));
    memset(&deviceProperties.numSubslicesPerSlice, std::numeric_limits<int>::max(), sizeof(deviceProperties.numSubslicesPerSlice));
    memset(&deviceProperties.numSlices, std::numeric_limits<int>::max(), sizeof(deviceProperties.numSlices));
    memset(&deviceProperties.timerResolution, std::numeric_limits<int>::max(), sizeof(deviceProperties.timerResolution));
    memset(&deviceProperties.name, std::numeric_limits<int>::max(), sizeof(deviceProperties.name));
    deviceProperties.maxMemAllocSize = 0;

    devicePropertiesBefore = deviceProperties;
    device->getProperties(&deviceProperties);

    EXPECT_NE(deviceProperties.type, devicePropertiesBefore.type);
    EXPECT_NE(deviceProperties.vendorId, devicePropertiesBefore.vendorId);
    EXPECT_NE(deviceProperties.deviceId, devicePropertiesBefore.deviceId);
    EXPECT_NE(0, memcmp(&deviceProperties.uuid, &devicePropertiesBefore.uuid, sizeof(devicePropertiesBefore.uuid)));
    EXPECT_NE(deviceProperties.subdeviceId, devicePropertiesBefore.subdeviceId);
    EXPECT_NE(deviceProperties.coreClockRate, devicePropertiesBefore.coreClockRate);
    EXPECT_NE(deviceProperties.maxCommandQueuePriority, devicePropertiesBefore.maxCommandQueuePriority);
    EXPECT_NE(deviceProperties.numThreadsPerEU, devicePropertiesBefore.numThreadsPerEU);
    EXPECT_NE(deviceProperties.physicalEUSimdWidth, devicePropertiesBefore.physicalEUSimdWidth);
    EXPECT_NE(deviceProperties.numEUsPerSubslice, devicePropertiesBefore.numEUsPerSubslice);
    EXPECT_NE(deviceProperties.numSubslicesPerSlice, devicePropertiesBefore.numSubslicesPerSlice);
    EXPECT_NE(deviceProperties.numSlices, devicePropertiesBefore.numSlices);
    EXPECT_NE(deviceProperties.timerResolution, devicePropertiesBefore.timerResolution);
    EXPECT_NE(0, memcmp(&deviceProperties.name, &devicePropertiesBefore.name, sizeof(devicePropertiesBefore.name)));
    EXPECT_NE(deviceProperties.maxMemAllocSize, devicePropertiesBefore.maxMemAllocSize);
}

TEST_F(DeviceTest, givenCallToDevicePropertiesThenMaximumMemoryToBeAllocatedIsCorrectlyReturned) {
    ze_device_properties_t deviceProperties;
    deviceProperties.maxMemAllocSize = 0;
    device->getProperties(&deviceProperties);
    EXPECT_EQ(deviceProperties.maxMemAllocSize, this->neoDevice->getDeviceInfo().maxMemAllocSize);
}

TEST_F(DeviceTest, whenGetDevicePropertiesCalledThenCorrectDevicePropertyEccFlagSet) {
    ze_device_properties_t deviceProps;

    device->getProperties(&deviceProps);
    auto expected = (this->neoDevice->getDeviceInfo().errorCorrectionSupport) ? ZE_DEVICE_PROPERTY_FLAG_ECC : static_cast<ze_device_property_flag_t>(0u);
    EXPECT_EQ(expected, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ECC);
}

TEST_F(DeviceTest, givenCommandQueuePropertiesCallThenCallSucceeds) {
    uint32_t count = 0;
    ze_result_t res = device->getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 1u);

    std::vector<ze_command_queue_group_properties_t> queueProperties(count);
    res = device->getCommandQueueGroupProperties(&count, queueProperties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

struct DeviceHasNoDoubleFp64Test : public ::testing::Test {
    void SetUp() override {
        DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        HardwareInfo nonFp64Device = *defaultHwInfo;
        nonFp64Device.capabilityTable.ftrSupportsFP64 = false;
        nonFp64Device.capabilityTable.ftrSupports64BitMath = false;
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&nonFp64Device, rootDeviceIndex);
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

TEST_F(DeviceHasNoDoubleFp64Test, givenDeviceThatDoesntHaveFp64WhenDbgFlagEnablesFp64ThenReportFp64Flags) {
    ze_device_module_properties_t kernelProperties;
    memset(&kernelProperties, std::numeric_limits<int>::max(), sizeof(ze_device_module_properties_t));

    device->getKernelProperties(&kernelProperties);
    EXPECT_FALSE(kernelProperties.flags & ZE_DEVICE_MODULE_FLAG_FP64);
    EXPECT_EQ(0u, kernelProperties.fp64flags);

    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.OverrideDefaultFP64Settings.set(1);

    device->getKernelProperties(&kernelProperties);
    EXPECT_TRUE(kernelProperties.flags & ZE_DEVICE_MODULE_FLAG_FP64);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_ROUND_TO_NEAREST);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_ROUND_TO_ZERO);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_ROUND_TO_INF);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_INF_NAN);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_DENORM);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_FMA);
}

struct DeviceHasNo64BitAtomicTest : public ::testing::Test {
    void SetUp() override {
        HardwareInfo nonFp64Device = *defaultHwInfo;
        nonFp64Device.capabilityTable.ftrSupportsInteger64BitAtomics = false;
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&nonFp64Device, rootDeviceIndex);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[rootDeviceIndex];
    }

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::Device *neoDevice = nullptr;
    L0::Device *device = nullptr;
    const uint32_t rootDeviceIndex = 0u;
};

TEST_F(DeviceHasNo64BitAtomicTest, givenDeviceWithNoSupportForInteger64BitAtomicsThenFlagsAreSetCorrectly) {
    ze_device_module_properties_t kernelProperties;
    memset(&kernelProperties, std::numeric_limits<int>::max(), sizeof(ze_device_module_properties_t));

    device->getKernelProperties(&kernelProperties);
    EXPECT_TRUE(kernelProperties.flags & ZE_DEVICE_MODULE_FLAG_FP16);
    EXPECT_FALSE(kernelProperties.flags & ZE_DEVICE_MODULE_FLAG_INT64_ATOMICS);
}

struct DeviceHas64BitAtomicTest : public ::testing::Test {
    void SetUp() override {
        HardwareInfo nonFp64Device = *defaultHwInfo;
        nonFp64Device.capabilityTable.ftrSupportsInteger64BitAtomics = true;
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&nonFp64Device, rootDeviceIndex);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[rootDeviceIndex];
    }

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::Device *neoDevice = nullptr;
    L0::Device *device = nullptr;
    const uint32_t rootDeviceIndex = 0u;
};

TEST_F(DeviceHas64BitAtomicTest, givenDeviceWithSupportForInteger64BitAtomicsThenFlagsAreSetCorrectly) {
    ze_device_module_properties_t kernelProperties;
    memset(&kernelProperties, std::numeric_limits<int>::max(), sizeof(ze_device_module_properties_t));

    device->getKernelProperties(&kernelProperties);
    EXPECT_TRUE(kernelProperties.flags & ZE_DEVICE_MODULE_FLAG_FP16);
    EXPECT_TRUE(kernelProperties.flags & ZE_DEVICE_MODULE_FLAG_INT64_ATOMICS);
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

TEST_F(MultipleDevicesTest, whenDeviceContainsSubDevicesThenItIsMultiDeviceCapable) {
    L0::Device *device0 = driverHandle->devices[0];
    EXPECT_TRUE(device0->isMultiDeviceCapable());

    L0::Device *device1 = driverHandle->devices[1];
    EXPECT_TRUE(device1->isMultiDeviceCapable());
}

TEST_F(MultipleDevicesTest, whenRetrievingNumberOfSubdevicesThenCorrectNumberIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];

    uint32_t count = 0;
    auto result = device0->getSubDevices(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numSubDevices, count);

    std::vector<ze_device_handle_t> subDevices(count);
    count++;
    result = device0->getSubDevices(&count, subDevices.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numSubDevices, count);
    for (auto subDevice : subDevices) {
        EXPECT_NE(nullptr, subDevice);
        EXPECT_TRUE(static_cast<DeviceImp *>(subDevice)->isSubdevice);
    }
}

TEST_F(MultipleDevicesTest, whenRetriecingSubDevicePropertiesThenCorrectFlagIsSet) {
    L0::Device *device0 = driverHandle->devices[0];

    uint32_t count = 0;
    auto result = device0->getSubDevices(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numSubDevices, count);

    std::vector<ze_device_handle_t> subDevices(count);
    count++;
    result = device0->getSubDevices(&count, subDevices.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numSubDevices, count);

    ze_device_properties_t deviceProps;

    L0::Device *subdevice0 = static_cast<L0::Device *>(subDevices[0]);
    subdevice0->getProperties(&deviceProps);
    EXPECT_EQ(ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);
}

TEST_F(MultipleDevicesTest, givenTheSameDeviceThenCanAccessPeerReturnsTrue) {
    L0::Device *device0 = driverHandle->devices[0];

    ze_bool_t canAccess = false;
    ze_result_t res = device0->canAccessPeer(device0->toHandle(), &canAccess);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(canAccess);
}

TEST_F(MultipleDevicesTest, givenTwoRootDevicesFromSameFamilyThenCanAccessPeerReturnsFalse) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    GFXCORE_FAMILY device0Family = device0->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    GFXCORE_FAMILY device1Family = device1->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    EXPECT_EQ(device0Family, device1Family);

    ze_bool_t canAccess = true;
    ze_result_t res = device0->canAccessPeer(device1->toHandle(), &canAccess);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_FALSE(canAccess);
}

TEST_F(MultipleDevicesTest, givenTwoRootDevicesFromSameFamilyThenCanAccessPeerReturnsTrueIfEnableCrossDeviceAccessIsSetToOne) {
    DebugManager.flags.EnableCrossDeviceAccess.set(1);
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

TEST_F(MultipleDevicesTest, givenTwoSubDevicesFromTheSameRootDeviceThenCanAccessPeerReturnsFalseIfEnableCrossDeviceAccessIsSetToZero) {
    DebugManager.flags.EnableCrossDeviceAccess.set(0);
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

    ze_bool_t canAccess = true;
    L0::Device *subDevice0_0 = Device::fromHandle(subDevices0[0]);
    subDevice0_0->canAccessPeer(subDevices0[1], &canAccess);
    EXPECT_FALSE(canAccess);

    canAccess = true;
    L0::Device *subDevice1_0 = Device::fromHandle(subDevices1[0]);
    subDevice1_0->canAccessPeer(subDevices1[1], &canAccess);
    EXPECT_FALSE(canAccess);
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

TEST_F(MultipleDevicesDifferentLocalMemorySupportTest, givenTwoDevicesWithDifferentLocalMemorySupportThenCanAccessPeerReturnsFalse) {
    ze_bool_t canAccess = true;
    ze_result_t res = deviceWithLocalMemory->canAccessPeer(deviceWithoutLocalMemory->toHandle(), &canAccess);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_FALSE(canAccess);
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

struct MultipleDevicesSameFamilyAndLocalMemorySupportTest : public MultipleDevicesTest {
    void SetUp() override {
        MultipleDevicesTest::SetUp();

        memoryManager->localMemorySupported[0] = 1;
        memoryManager->localMemorySupported[1] = 1;

        device0 = driverHandle->devices[0];
        device1 = driverHandle->devices[1];
    }

    L0::Device *device0 = nullptr;
    L0::Device *device1 = nullptr;
};

TEST_F(MultipleDevicesSameFamilyAndLocalMemorySupportTest, givenTwoDevicesFromSameFamilyThenCanAccessPeerReturnsFalse) {
    PRODUCT_FAMILY device0Family = device0->getNEODevice()->getHardwareInfo().platform.eProductFamily;
    PRODUCT_FAMILY device1Family = device1->getNEODevice()->getHardwareInfo().platform.eProductFamily;
    EXPECT_EQ(device0Family, device1Family);

    ze_bool_t canAccess = true;
    ze_result_t res = device0->canAccessPeer(device1->toHandle(), &canAccess);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_FALSE(canAccess);
}

TEST_F(DeviceTest, givenNoActiveSourceLevelDebuggerWhenGetIsCalledThenNullptrIsReturned) {
    EXPECT_EQ(nullptr, device->getSourceLevelDebugger());
}

TEST_F(DeviceTest, givenNoL0DebuggerWhenGettingL0DebuggerThenNullptrReturned) {
    EXPECT_EQ(nullptr, device->getL0Debugger());
}

TEST(DevicePropertyFlagIsIntegratedTest, givenIntegratedDeviceThenCorrectDevicePropertyFlagSet) {
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.isIntegratedDevice = true;

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    auto device = driverHandle->devices[0];

    ze_device_properties_t deviceProps;

    device->getProperties(&deviceProps);
    EXPECT_EQ(ZE_DEVICE_PROPERTY_FLAG_INTEGRATED, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED);
}

TEST(DevicePropertyFlagDiscreteDeviceTest, givenDiscreteDeviceThenCorrectDevicePropertyFlagSet) {
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.isIntegratedDevice = false;

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    auto device = driverHandle->devices[0];

    ze_device_properties_t deviceProps;

    device->getProperties(&deviceProps);
    EXPECT_EQ(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED);
}

} // namespace ult
} // namespace L0