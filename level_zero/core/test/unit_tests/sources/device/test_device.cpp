/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/mock_hw_info_config_hw.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_info.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cache/cache_reservation.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/driver/host_pointer_manager.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"

#include "gtest/gtest.h"

#include <memory>

namespace NEO {
extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];
} // namespace NEO

namespace L0 {
namespace ult {

TEST(L0DeviceTest, GivenCreatedDeviceHandleWhenCallingdeviceReinitThenNewDeviceHandleIsNotCreated) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue);
    ASSERT_NE(nullptr, device);
    static_cast<DeviceImp *>(device)->releaseResources();

    auto newNeoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    EXPECT_EQ(device, Device::deviceReinit(device->getDriverHandle(), device, newNeoDevice, &returnValue));
    delete device;
}

TEST(L0DeviceTest, GivenDualStorageSharedMemorySupportedWhenCreatingDeviceThenPageFaultCmdListImmediateWithInitializedCmdQIsCreated) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(1);

    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);
    auto deviceImp = static_cast<DeviceImp *>(device.get());
    ASSERT_NE(nullptr, deviceImp->pageFaultCommandList);

    ASSERT_NE(nullptr, deviceImp->pageFaultCommandList->cmdQImmediate);
    EXPECT_NE(nullptr, static_cast<CommandQueueImp *>(deviceImp->pageFaultCommandList->cmdQImmediate)->getCsr());
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS, static_cast<CommandQueueImp *>(deviceImp->pageFaultCommandList->cmdQImmediate)->getSynchronousMode());
}

TEST(L0DeviceTest, GivenDualStorageSharedMemoryAndImplicitScalingThenPageFaultCmdListImmediateWithInitializedCmdQIsCreatedAgainstSubDeviceZero) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(1);
    DebugManager.flags.EnableImplicitScaling.set(1u);
    DebugManager.flags.CreateMultipleSubDevices.set(2u);

    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);
    auto deviceImp = static_cast<DeviceImp *>(device.get());
    ASSERT_NE(nullptr, deviceImp->pageFaultCommandList);

    ASSERT_NE(nullptr, deviceImp->pageFaultCommandList->cmdQImmediate);
    EXPECT_NE(nullptr, static_cast<CommandQueueImp *>(deviceImp->pageFaultCommandList->cmdQImmediate)->getCsr());
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS, static_cast<CommandQueueImp *>(deviceImp->pageFaultCommandList->cmdQImmediate)->getSynchronousMode());
    EXPECT_EQ(deviceImp->pageFaultCommandList->device, deviceImp->subDevices[0]);
}

TEST(L0DeviceTest, givenMultipleMaskedSubDevicesWhenCreatingL0DeviceThenDontAddDisabledNeoDevies) {
    constexpr uint32_t numSubDevices = 3;
    constexpr uint32_t numMaskedSubDevices = 2;

    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
    DebugManager.flags.ZE_AFFINITY_MASK.set("0.0,0.2");

    auto executionEnvironment = std::make_unique<NEO::ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->parseAffinityMask();
    auto deviceFactory = std::make_unique<NEO::UltDeviceFactory>(1, numSubDevices, *executionEnvironment.release());
    auto rootDevice = deviceFactory->rootDevices[0];
    EXPECT_NE(nullptr, rootDevice);
    EXPECT_EQ(numMaskedSubDevices, rootDevice->getNumSubDevices());

    auto driverHandle = std::make_unique<DriverHandleImp>();

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), rootDevice, false, &returnValue));
    ASSERT_NE(nullptr, device);

    auto deviceImp = static_cast<DeviceImp *>(device.get());
    ASSERT_EQ(numMaskedSubDevices, deviceImp->numSubDevices);

    EXPECT_EQ(0b1u, deviceImp->subDevices[0]->getNEODevice()->getDeviceBitfield().to_ulong());
    EXPECT_EQ(0b100u, deviceImp->subDevices[1]->getNEODevice()->getDeviceBitfield().to_ulong());
}

TEST(L0DeviceTest, givenMidThreadPreemptionWhenCreatingDeviceThenSipKernelIsInitialized) {
    NEO::MockCompilerEnableGuard mock(true);
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    VariableBackup<bool> mockSipCalled(&NEO::MockSipData::called, false);
    VariableBackup<NEO::SipKernelType> mockSipCalledType(&NEO::MockSipData::calledType, NEO::SipKernelType::COUNT);
    VariableBackup<bool> backupSipInitType(&MockSipData::useMockSip, true);

    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.capabilityTable.defaultPreemptionMode = NEO::PreemptionMode::MidThread;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    EXPECT_EQ(NEO::SipKernelType::COUNT, NEO::MockSipData::calledType);
    EXPECT_FALSE(NEO::MockSipData::called);

    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);

    EXPECT_EQ(NEO::SipKernelType::Csr, NEO::MockSipData::calledType);
    EXPECT_TRUE(NEO::MockSipData::called);
}

TEST(L0DeviceTest, givenDebuggerEnabledButIGCNotReturnsSSAHThenSSAHIsNotCopied) {
    NEO::MockCompilerEnableGuard mock(true);
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    auto mockBuiltIns = new MockBuiltins();
    mockBuiltIns->stateSaveAreaHeader.clear();

    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    auto neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->enableProgramDebugging = true;

    driverHandle->initialize(std::move(devices));
    auto sipType = SipKernel::getSipKernelType(*neoDevice);
    auto &stateSaveAreaHeader = neoDevice->getBuiltIns()->getSipKernel(sipType, *neoDevice).getStateSaveAreaHeader();
    EXPECT_EQ(static_cast<size_t>(0), stateSaveAreaHeader.size());
}

TEST(L0DeviceTest, givenDisabledPreemptionWhenCreatingDeviceThenSipKernelIsNotInitialized) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    VariableBackup<bool> mockSipCalled(&NEO::MockSipData::called, false);
    VariableBackup<NEO::SipKernelType> mockSipCalledType(&NEO::MockSipData::calledType, NEO::SipKernelType::COUNT);
    VariableBackup<bool> backupSipInitType(&MockSipData::useMockSip, true);

    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.capabilityTable.defaultPreemptionMode = NEO::PreemptionMode::Disabled;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    EXPECT_EQ(NEO::SipKernelType::COUNT, NEO::MockSipData::calledType);
    EXPECT_FALSE(NEO::MockSipData::called);

    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);

    EXPECT_EQ(NEO::SipKernelType::COUNT, NEO::MockSipData::calledType);
    EXPECT_FALSE(NEO::MockSipData::called);
}

TEST(L0DeviceTest, givenDeviceWithoutFCLCompilerLibraryThenInvalidDependencyReturned) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;

    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    auto oldFclDllName = Os::frontEndDllName;
    Os::frontEndDllName = "_invalidFCL";

    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);
    EXPECT_EQ(returnValue, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);

    Os::frontEndDllName = oldFclDllName;
}

TEST(L0DeviceTest, givenDeviceWithoutIGCCompilerLibraryThenInvalidDependencyReturned) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;

    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    auto oldIgcDllName = Os::igcDllName;
    Os::igcDllName = "_invalidIGC";

    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);
    EXPECT_EQ(returnValue, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);

    Os::igcDllName = oldIgcDllName;
}

TEST(L0DeviceTest, givenDeviceWithoutAnyCompilerLibraryThenInvalidDependencyReturned) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;

    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    auto oldFclDllName = Os::frontEndDllName;
    auto oldIgcDllName = Os::igcDllName;
    Os::frontEndDllName = "_invalidFCL";
    Os::igcDllName = "_invalidIGC";

    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);
    EXPECT_EQ(returnValue, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);

    Os::igcDllName = oldIgcDllName;
    Os::frontEndDllName = oldFclDllName;
}

TEST(L0DeviceTest, givenFilledTopologyWhenGettingApiSliceThenCorrectSliceIdIsReturned) {
    NEO::MockCompilerEnableGuard mock(true);
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, nullptr));
    ASSERT_NE(nullptr, device);

    auto deviceImp = static_cast<DeviceImp *>(device.get());

    NEO::TopologyMap map;
    TopologyMapping mapping;

    mapping.sliceIndices.resize(hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i;
    }

    mapping.subsliceIndices.resize(hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.subsliceIndices[i] = i;
    }

    map[0] = mapping;

    uint32_t sliceId = hwInfo.gtSystemInfo.SliceCount - 1;
    uint32_t subsliceId = 0;
    auto ret = deviceImp->toApiSliceId(map, sliceId, subsliceId, 0);

    EXPECT_EQ(hwInfo.gtSystemInfo.SliceCount - 1, sliceId);
    EXPECT_TRUE(ret);

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i + 1;
    }
    map[0] = mapping;

    sliceId = 1;
    ret = deviceImp->toApiSliceId(map, sliceId, subsliceId, 0);

    EXPECT_EQ(0u, sliceId);
    EXPECT_TRUE(ret);
}

TEST(L0DeviceTest, givenFilledTopologyForZeroSubDeviceWhenGettingApiSliceForHigherSubDevicesThenFalseIsReturned) {
    NEO::MockCompilerEnableGuard mock(true);
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, nullptr));
    ASSERT_NE(nullptr, device);

    auto deviceImp = static_cast<DeviceImp *>(device.get());

    NEO::TopologyMap map;
    TopologyMapping mapping;

    mapping.sliceIndices.resize(hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i + 1;
    }

    mapping.subsliceIndices.resize(hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.subsliceIndices[i] = i;
    }
    map[0] = mapping;

    uint32_t sliceId = 1;
    uint32_t subsliceId = 0;
    const uint32_t deviceIndex = 2;
    auto ret = deviceImp->toApiSliceId(map, sliceId, subsliceId, deviceIndex);
    EXPECT_FALSE(ret);
}

TEST(L0DeviceTest, givenInvalidPhysicalSliceIdWhenGettingApiSliceIdThenFalseIsReturned) {
    NEO::MockCompilerEnableGuard mock(true);
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, nullptr));
    ASSERT_NE(nullptr, device);

    auto deviceImp = static_cast<DeviceImp *>(device.get());

    NEO::TopologyMap map;
    TopologyMapping mapping;

    mapping.sliceIndices.resize(hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i;
    }

    mapping.subsliceIndices.resize(hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.subsliceIndices[i] = i;
    }
    map[0] = mapping;

    uint32_t sliceId = hwInfo.gtSystemInfo.SliceCount + 1;
    uint32_t subsliceId = 0;
    auto ret = deviceImp->toApiSliceId(map, sliceId, subsliceId, 0);

    EXPECT_FALSE(ret);
}

TEST(L0DeviceTest, givenInvalidApiSliceIdWhenGettingPhysicalSliceIdThenFalseIsReturned) {
    NEO::MockCompilerEnableGuard mock(true);
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, nullptr));
    ASSERT_NE(nullptr, device);

    auto deviceImp = static_cast<DeviceImp *>(device.get());

    NEO::TopologyMap map;
    TopologyMapping mapping;

    mapping.sliceIndices.resize(hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i;
    }

    mapping.subsliceIndices.resize(hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.subsliceIndices[i] = i;
    }
    map[0] = mapping;

    uint32_t sliceId = hwInfo.gtSystemInfo.SliceCount + 1;
    uint32_t subsliceId = 1;
    uint32_t deviceIndex = 0;
    auto ret = deviceImp->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

    EXPECT_FALSE(ret);
}

TEST(L0DeviceTest, givenEmptyTopologyWhenGettingApiSliceIdThenFalseIsReturned) {
    NEO::MockCompilerEnableGuard mock(true);
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, nullptr));
    ASSERT_NE(nullptr, device);

    auto deviceImp = static_cast<DeviceImp *>(device.get());

    NEO::TopologyMap map;
    uint32_t sliceId = hwInfo.gtSystemInfo.SliceCount - 1;
    uint32_t subsliceId = 0;
    auto ret = deviceImp->toApiSliceId(map, sliceId, subsliceId, 0);

    EXPECT_FALSE(ret);
}

TEST(L0DeviceTest, givenDeviceWithoutSubDevicesWhenGettingPhysicalSliceIdThenCorrectValuesAreReturned) {
    NEO::MockCompilerEnableGuard mock(true);
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, nullptr));
    ASSERT_NE(nullptr, device);

    auto deviceImp = static_cast<DeviceImp *>(device.get());

    NEO::TopologyMap map;
    TopologyMapping mapping;

    mapping.sliceIndices.resize(hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i + 1;
    }

    mapping.subsliceIndices.resize(hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.subsliceIndices[i] = i;
    }
    map[0] = mapping;

    uint32_t sliceId = 0;
    uint32_t subsliceId = 0;
    uint32_t deviceIndex = 10;
    auto ret = deviceImp->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

    EXPECT_TRUE(ret);
    EXPECT_EQ(1u, sliceId);
    EXPECT_EQ(0u, subsliceId);
    EXPECT_EQ(0u, deviceIndex);
}

TEST(L0DeviceTest, givenTopologyNotAvaialbleWhenGettingPhysicalSliceIdThenFalseIsReturned) {
    NEO::MockCompilerEnableGuard mock(true);
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, nullptr));
    ASSERT_NE(nullptr, device);

    auto deviceImp = static_cast<DeviceImp *>(device.get());

    NEO::TopologyMap map;
    TopologyMapping mapping;

    uint32_t sliceId = 0;
    uint32_t subsliceId = 0;
    uint32_t deviceIndex = 10;
    auto ret = deviceImp->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

    EXPECT_FALSE(ret);
}

TEST(L0DeviceTest, givenSingleSliceTopologyWhenConvertingToApiIdsThenSubsliceIdsAreRemapped) {
    NEO::MockCompilerEnableGuard mock(true);
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;

    hwInfo.gtSystemInfo.SliceCount = 1;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, nullptr));
    ASSERT_NE(nullptr, device);

    auto deviceImp = static_cast<DeviceImp *>(device.get());

    NEO::TopologyMap map;
    TopologyMapping mapping;

    mapping.sliceIndices.resize(hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i;
    }

    mapping.subsliceIndices.resize(hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount);

    // disable 5 physical subslices, shift subslice ids by 5
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.subsliceIndices[i] = i + 5;
    }

    map[0] = mapping;

    uint32_t sliceId = 0;
    uint32_t subsliceId = 5;
    auto ret = deviceImp->toApiSliceId(map, sliceId, subsliceId, 0);

    EXPECT_EQ(0u, sliceId);
    EXPECT_EQ(0u, subsliceId);
    EXPECT_TRUE(ret);

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i + 1;
    }
    map[0] = mapping;

    sliceId = 1;
    subsliceId = 5;
    ret = deviceImp->toApiSliceId(map, sliceId, subsliceId, 0);

    EXPECT_EQ(0u, sliceId);
    EXPECT_EQ(0u, subsliceId);
    EXPECT_TRUE(ret);
}

TEST(L0DeviceTest, givenSingleSliceTopologyWhenConvertingToPhysicalIdsThenSubsliceIdsAreRemapped) {
    NEO::MockCompilerEnableGuard mock(true);
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;

    hwInfo.gtSystemInfo.SliceCount = 1;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, nullptr));
    ASSERT_NE(nullptr, device);

    auto deviceImp = static_cast<DeviceImp *>(device.get());

    NEO::TopologyMap map;
    TopologyMapping mapping;

    mapping.sliceIndices.resize(hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i;
    }

    mapping.subsliceIndices.resize(hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount);

    // disable 5 physical subslices, shift subslice ids by 5
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.subsliceIndices[i] = i + 5;
    }

    map[0] = mapping;

    uint32_t sliceId = 0;
    uint32_t subsliceId = 0;
    uint32_t deviceIndex = 0;
    auto ret = deviceImp->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

    EXPECT_EQ(0u, sliceId);
    EXPECT_EQ(5u, subsliceId);
    EXPECT_EQ(0u, deviceIndex);
    EXPECT_TRUE(ret);

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i + 1;
    }
    map[0] = mapping;

    sliceId = 0;
    subsliceId = 0;
    ret = deviceImp->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

    EXPECT_EQ(1u, sliceId);
    EXPECT_EQ(5u, subsliceId);
    EXPECT_EQ(0u, deviceIndex);
    EXPECT_TRUE(ret);
}

struct DeviceTest : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), rootDeviceIndex);
        execEnv = neoDevice->getExecutionEnvironment();
        execEnv->incRefInternal();
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }

    void TearDown() override {
        driverHandle.reset(nullptr);
        execEnv->decRefInternal();
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::ExecutionEnvironment *execEnv;
    NEO::Device *neoDevice = nullptr;
    L0::Device *device = nullptr;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
};

TEST_F(DeviceTest, givenEmptySVmAllocStorageWhenAllocateManagedMemoryFromHostPtrThenBufferHostAllocationIsCreated) {
    int data;
    auto allocation = device->allocateManagedMemoryFromHostPtr(&data, sizeof(data), nullptr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(NEO::AllocationType::BUFFER_HOST_MEMORY, allocation->getAllocationType());
    EXPECT_EQ(rootDeviceIndex, allocation->getRootDeviceIndex());
    neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
}

TEST_F(DeviceTest, givenEmptySVmAllocStorageWhenAllocateMemoryFromHostPtrThenValidExternalHostPtrAllocationIsCreated) {
    DebugManager.flags.EnableHostPtrTracking.set(0);
    constexpr auto dataSize = 1024u;
    auto data = std::make_unique<int[]>(dataSize);

    constexpr auto allocationSize = sizeof(int) * dataSize;

    auto allocation = device->allocateMemoryFromHostPtr(data.get(), allocationSize, false);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(NEO::AllocationType::EXTERNAL_HOST_PTR, allocation->getAllocationType());
    EXPECT_EQ(rootDeviceIndex, allocation->getRootDeviceIndex());

    auto alignedPtr = alignDown(data.get(), MemoryConstants::pageSize);
    auto offsetInPage = ptrDiff(data.get(), alignedPtr);

    EXPECT_EQ(allocation->getAllocationOffset(), offsetInPage);
    EXPECT_EQ(allocation->getUnderlyingBufferSize(), allocationSize);
    EXPECT_EQ(allocation->isFlushL3Required(), true);

    neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
}

TEST_F(DeviceTest, givenNonEmptyAllocationsListWhenRequestingAllocationSmallerOrEqualInSizeThenAllocationFromListIsReturned) {
    auto deviceImp = static_cast<DeviceImp *>(device);
    constexpr auto dataSize = 1024u;
    auto data = std::make_unique<int[]>(dataSize);

    constexpr auto allocationSize = sizeof(int) * dataSize;

    auto allocation = device->getDriverHandle()->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getNEODevice()->getRootDeviceIndex(),
                                                                                                           allocationSize,
                                                                                                           NEO::AllocationType::FILL_PATTERN,
                                                                                                           neoDevice->getDeviceBitfield()});
    device->storeReusableAllocation(*allocation);
    EXPECT_FALSE(deviceImp->allocationsForReuse->peekIsEmpty());
    auto obtaindedAllocation = device->obtainReusableAllocation(dataSize, NEO::AllocationType::FILL_PATTERN);
    EXPECT_TRUE(deviceImp->allocationsForReuse->peekIsEmpty());
    EXPECT_NE(nullptr, obtaindedAllocation);
    EXPECT_EQ(allocation, obtaindedAllocation);
    neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
}

TEST_F(DeviceTest, givenNonEmptyAllocationsListWhenRequestingAllocationBiggerInSizeThenNullptrIsReturned) {
    auto deviceImp = static_cast<DeviceImp *>(device);
    constexpr auto dataSize = 1024u;
    auto data = std::make_unique<int[]>(dataSize);

    constexpr auto allocationSize = sizeof(int) * dataSize;

    auto allocation = device->getDriverHandle()->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getNEODevice()->getRootDeviceIndex(),
                                                                                                           allocationSize,
                                                                                                           NEO::AllocationType::FILL_PATTERN,
                                                                                                           neoDevice->getDeviceBitfield()});
    device->storeReusableAllocation(*allocation);
    EXPECT_FALSE(deviceImp->allocationsForReuse->peekIsEmpty());
    auto obtaindedAllocation = device->obtainReusableAllocation(4 * dataSize + 1u, NEO::AllocationType::FILL_PATTERN);
    EXPECT_EQ(nullptr, obtaindedAllocation);
    EXPECT_FALSE(deviceImp->allocationsForReuse->peekIsEmpty());
}

TEST_F(DeviceTest, givenNonEmptyAllocationsListAndUnproperAllocationTypeWhenRequestingAllocationThenNullptrIsReturned) {
    auto deviceImp = static_cast<DeviceImp *>(device);
    constexpr auto dataSize = 1024u;
    auto data = std::make_unique<int[]>(dataSize);

    constexpr auto allocationSize = sizeof(int) * dataSize;

    auto allocation = device->getDriverHandle()->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getNEODevice()->getRootDeviceIndex(),
                                                                                                           allocationSize,
                                                                                                           NEO::AllocationType::BUFFER,
                                                                                                           neoDevice->getDeviceBitfield()});
    device->storeReusableAllocation(*allocation);
    EXPECT_FALSE(deviceImp->allocationsForReuse->peekIsEmpty());
    auto obtaindedAllocation = device->obtainReusableAllocation(4 * dataSize + 1u, NEO::AllocationType::FILL_PATTERN);
    EXPECT_EQ(nullptr, obtaindedAllocation);
    EXPECT_FALSE(deviceImp->allocationsForReuse->peekIsEmpty());
}

struct DeviceHostPointerTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }

        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, rootDeviceIndex);
        std::vector<std::unique_ptr<NEO::Device>> devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));

        static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->isMockHostMemoryManager = true;
        static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInAllocationWithHostPointer = true;

        device = driverHandle->devices[0];
    }
    void TearDown() override {
    }

    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
};

TEST_F(DeviceHostPointerTest, givenHostPointerNotAcceptedByKernelThenNewAllocationIsCreatedAndHostPointerCopied) {
    size_t size = 55;
    uint64_t *buffer = new uint64_t[size];
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = i + 10;
    }

    auto allocation = device->allocateMemoryFromHostPtr(buffer, size, true);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(NEO::AllocationType::INTERNAL_HOST_MEMORY, allocation->getAllocationType());
    EXPECT_EQ(rootDeviceIndex, allocation->getRootDeviceIndex());
    EXPECT_NE(allocation->getUnderlyingBuffer(), reinterpret_cast<void *>(buffer));
    EXPECT_EQ(allocation->getUnderlyingBufferSize(), size);
    EXPECT_EQ(0, memcmp(buffer, allocation->getUnderlyingBuffer(), size));

    neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
    delete[] buffer;
}

TEST_F(DeviceHostPointerTest, givenHostPointerNotAcceptedByKernelAndHostPointerCopyIsNotAllowedThenAllocationIsNull) {
    size_t size = 55;
    uint64_t *buffer = new uint64_t[size];
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = i + 10;
    }

    auto allocation = device->allocateMemoryFromHostPtr(buffer, size, false);
    EXPECT_EQ(nullptr, allocation);
    delete[] buffer;
}

TEST_F(DeviceTest, givenKernelExtendedPropertiesStructureWhenKernelPropertiesCalledThenSuccessIsReturnedAndPropertiesAreSet) {
    ze_device_module_properties_t kernelProperties = {};

    ze_float_atomic_ext_properties_t kernelExtendedProperties = {};
    kernelExtendedProperties.stype = ZE_STRUCTURE_TYPE_FLOAT_ATOMIC_EXT_PROPERTIES;
    uint32_t maxValue = static_cast<ze_device_fp_flags_t>(std::numeric_limits<uint32_t>::max());
    kernelExtendedProperties.fp16Flags = maxValue;
    kernelExtendedProperties.fp32Flags = maxValue;
    kernelExtendedProperties.fp64Flags = maxValue;

    kernelProperties.pNext = &kernelExtendedProperties;
    ze_result_t res = device->getKernelProperties(&kernelProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_NE(maxValue, kernelExtendedProperties.fp16Flags);
    EXPECT_NE(maxValue, kernelExtendedProperties.fp32Flags);
    EXPECT_NE(maxValue, kernelExtendedProperties.fp64Flags);
}

TEST_F(DeviceTest, givenKernelExtendedPropertiesStructureWhenKernelPropertiesCalledWithIncorrectsStypeThenSuccessIsReturnedButPropertiesAreNotSet) {
    ze_device_module_properties_t kernelProperties = {};

    ze_float_atomic_ext_properties_t kernelExtendedProperties = {};
    kernelExtendedProperties.stype = ZE_STRUCTURE_TYPE_FORCE_UINT32;
    uint32_t maxValue = static_cast<ze_device_fp_flags_t>(std::numeric_limits<uint32_t>::max());
    kernelExtendedProperties.fp16Flags = maxValue;
    kernelExtendedProperties.fp32Flags = maxValue;
    kernelExtendedProperties.fp64Flags = maxValue;

    kernelProperties.pNext = &kernelExtendedProperties;
    ze_result_t res = device->getKernelProperties(&kernelProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(maxValue, kernelExtendedProperties.fp16Flags);
    EXPECT_EQ(maxValue, kernelExtendedProperties.fp32Flags);
    EXPECT_EQ(maxValue, kernelExtendedProperties.fp64Flags);
}

HWTEST_F(DeviceTest, whenPassingSchedulingHintExpStructToGetPropertiesThenPropertiesWithCorrectFlagIsReturned) {

    ze_device_module_properties_t kernelProperties = {};
    kernelProperties.stype = ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES;

    ze_scheduling_hint_exp_properties_t schedulingHintProperties = {};
    schedulingHintProperties.stype = ZE_STRUCTURE_TYPE_SCHEDULING_HINT_EXP_PROPERTIES;
    schedulingHintProperties.schedulingHintFlags = ZE_SCHEDULING_HINT_EXP_FLAG_FORCE_UINT32;

    kernelProperties.pNext = &schedulingHintProperties;

    ze_result_t res = device->getKernelProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_NE(ZE_SCHEDULING_HINT_EXP_FLAG_FORCE_UINT32, schedulingHintProperties.schedulingHintFlags);
    auto supportedThreadArbitrationPolicies = NEO::PreambleHelper<FamilyType>::getSupportedThreadArbitrationPolicies();
    for (auto &p : supportedThreadArbitrationPolicies) {
        switch (p) {
        case ThreadArbitrationPolicy::AgeBased:
            EXPECT_NE(0u, (schedulingHintProperties.schedulingHintFlags &
                           ZE_SCHEDULING_HINT_EXP_FLAG_OLDEST_FIRST));
            break;
        case ThreadArbitrationPolicy::RoundRobin:
            EXPECT_NE(0u, (schedulingHintProperties.schedulingHintFlags &
                           ZE_SCHEDULING_HINT_EXP_FLAG_ROUND_ROBIN));
            break;
        case ThreadArbitrationPolicy::RoundRobinAfterDependency:
            EXPECT_NE(0u, (schedulingHintProperties.schedulingHintFlags &
                           ZE_SCHEDULING_HINT_EXP_FLAG_STALL_BASED_ROUND_ROBIN));
            break;
        default:
            FAIL();
        }
    }
}

HWTEST2_F(DeviceTest, givenAllThreadArbitrationPoliciesWhenPassingSchedulingHintExpStructToGetPropertiesThenPropertiesWithAllFlagsAreReturned, MatchAny) {
    const uint32_t rootDeviceIndex = 0u;
    auto hwInfo = *NEO::defaultHwInfo;
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo,
                                                                                              rootDeviceIndex);

    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    MockHwInfoConfigHw<productFamily> hwInfoConfig;
    hwInfoConfig.threadArbPolicies = {ThreadArbitrationPolicy::AgeBased,
                                      ThreadArbitrationPolicy::RoundRobin,
                                      ThreadArbitrationPolicy::RoundRobinAfterDependency};
    VariableBackup<HwInfoConfig *> hwInfoConfigFactoryBackup{&NEO::hwInfoConfigFactory[static_cast<size_t>(hwInfo.platform.eProductFamily)]};
    hwInfoConfigFactoryBackup = &hwInfoConfig;

    ze_device_module_properties_t kernelProperties = {};
    kernelProperties.stype = ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES;

    ze_scheduling_hint_exp_properties_t schedulingHintProperties = {};
    schedulingHintProperties.stype = ZE_STRUCTURE_TYPE_SCHEDULING_HINT_EXP_PROPERTIES;
    schedulingHintProperties.schedulingHintFlags = ZE_SCHEDULING_HINT_EXP_FLAG_FORCE_UINT32;

    kernelProperties.pNext = &schedulingHintProperties;

    ze_result_t res = deviceImp.getKernelProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_scheduling_hint_exp_flags_t expected = (ZE_SCHEDULING_HINT_EXP_FLAG_OLDEST_FIRST |
                                               ZE_SCHEDULING_HINT_EXP_FLAG_ROUND_ROBIN |
                                               ZE_SCHEDULING_HINT_EXP_FLAG_STALL_BASED_ROUND_ROBIN);
    EXPECT_EQ(expected, schedulingHintProperties.schedulingHintFlags);
}

HWTEST2_F(DeviceTest, givenIncorrectThreadArbitrationPolicyWhenPassingSchedulingHintExpStructToGetPropertiesThenNoneIsReturned, MatchAny) {
    const uint32_t rootDeviceIndex = 0u;
    auto hwInfo = *NEO::defaultHwInfo;
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo,
                                                                                              rootDeviceIndex);

    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    MockHwInfoConfigHw<productFamily> hwInfoConfig;
    hwInfoConfig.threadArbPolicies = {ThreadArbitrationPolicy::NotPresent};
    VariableBackup<HwInfoConfig *> hwInfoConfigFactoryBackup{&NEO::hwInfoConfigFactory[static_cast<size_t>(hwInfo.platform.eProductFamily)]};
    hwInfoConfigFactoryBackup = &hwInfoConfig;

    ze_device_module_properties_t kernelProperties = {};
    kernelProperties.stype = ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES;

    ze_scheduling_hint_exp_properties_t schedulingHintProperties = {};
    schedulingHintProperties.stype = ZE_STRUCTURE_TYPE_SCHEDULING_HINT_EXP_PROPERTIES;
    schedulingHintProperties.schedulingHintFlags = ZE_SCHEDULING_HINT_EXP_FLAG_FORCE_UINT32;

    kernelProperties.pNext = &schedulingHintProperties;

    ze_result_t res = deviceImp.getKernelProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(0u, schedulingHintProperties.schedulingHintFlags);
}

TEST_F(DeviceTest, givenKernelPropertiesStructureWhenKernelPropertiesCalledThenAllPropertiesAreAssigned) {
    const auto &hardwareInfo = this->neoDevice->getHardwareInfo();

    ze_device_module_properties_t kernelProperties = {};
    ze_device_module_properties_t kernelPropertiesBefore = {};
    memset(&kernelProperties, std::numeric_limits<int>::max(), sizeof(ze_device_module_properties_t));
    kernelProperties.pNext = nullptr;
    kernelPropertiesBefore = kernelProperties;
    device->getKernelProperties(&kernelProperties);

    EXPECT_NE(kernelPropertiesBefore.spirvVersionSupported, kernelProperties.spirvVersionSupported);
    uint8_t *nativeKernelSupportedIdPointerBefore = kernelPropertiesBefore.nativeKernelSupported.id;
    uint8_t *nativeKernelSupportedIdPointerNow = kernelProperties.nativeKernelSupported.id;
    EXPECT_NE(nativeKernelSupportedIdPointerBefore, nativeKernelSupportedIdPointerNow);

    EXPECT_TRUE(kernelPropertiesBefore.flags & ZE_DEVICE_MODULE_FLAG_FP16);
    if (hardwareInfo.capabilityTable.ftrSupportsInteger64BitAtomics) {
        EXPECT_TRUE(kernelPropertiesBefore.flags & ZE_DEVICE_MODULE_FLAG_INT64_ATOMICS);
    }

    EXPECT_NE(kernelPropertiesBefore.maxArgumentsSize, kernelProperties.maxArgumentsSize);
    EXPECT_NE(kernelPropertiesBefore.printfBufferSize, kernelProperties.printfBufferSize);
}

TEST_F(DeviceTest, givenDeviceCachePropertiesThenAllPropertiesAreAssigned) {
    ze_device_cache_properties_t deviceCacheProperties = {};
    ze_device_cache_properties_t deviceCachePropertiesBefore = {};

    deviceCacheProperties.cacheSize = std::numeric_limits<size_t>::max();

    deviceCachePropertiesBefore = deviceCacheProperties;

    uint32_t count = 0;
    ze_result_t res = device->getCacheProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, 1u);

    res = device->getCacheProperties(&count, &deviceCacheProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_NE(deviceCacheProperties.cacheSize, deviceCachePropertiesBefore.cacheSize);
}

TEST_F(DeviceTest, givenDevicePropertiesStructureWhenDevicePropertiesCalledThenAllPropertiesAreAssigned) {
    ze_device_properties_t deviceProperties, devicePropertiesBefore;
    deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    deviceProperties.type = ZE_DEVICE_TYPE_FPGA;
    memset(&deviceProperties.vendorId, std::numeric_limits<int>::max(), sizeof(deviceProperties.vendorId));
    memset(&deviceProperties.deviceId, std::numeric_limits<int>::max(), sizeof(deviceProperties.deviceId));
    memset(&deviceProperties.uuid, std::numeric_limits<int>::max(), sizeof(deviceProperties.uuid));
    memset(&deviceProperties.subdeviceId, std::numeric_limits<int>::max(), sizeof(deviceProperties.subdeviceId));
    memset(&deviceProperties.coreClockRate, std::numeric_limits<int>::max(), sizeof(deviceProperties.coreClockRate));
    memset(&deviceProperties.maxCommandQueuePriority, std::numeric_limits<int>::max(), sizeof(deviceProperties.maxCommandQueuePriority));
    memset(&deviceProperties.maxHardwareContexts, std::numeric_limits<int>::max(), sizeof(deviceProperties.maxHardwareContexts));
    memset(&deviceProperties.numThreadsPerEU, std::numeric_limits<int>::max(), sizeof(deviceProperties.numThreadsPerEU));
    memset(&deviceProperties.physicalEUSimdWidth, std::numeric_limits<int>::max(), sizeof(deviceProperties.physicalEUSimdWidth));
    memset(&deviceProperties.numEUsPerSubslice, std::numeric_limits<int>::max(), sizeof(deviceProperties.numEUsPerSubslice));
    memset(&deviceProperties.numSubslicesPerSlice, std::numeric_limits<int>::max(), sizeof(deviceProperties.numSubslicesPerSlice));
    memset(&deviceProperties.numSlices, std::numeric_limits<int>::max(), sizeof(deviceProperties.numSlices));
    memset(&deviceProperties.timerResolution, std::numeric_limits<int>::max(), sizeof(deviceProperties.timerResolution));
    memset(&deviceProperties.timestampValidBits, std::numeric_limits<uint32_t>::max(), sizeof(deviceProperties.timestampValidBits));
    memset(&deviceProperties.kernelTimestampValidBits, std::numeric_limits<uint32_t>::max(), sizeof(deviceProperties.kernelTimestampValidBits));
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
    EXPECT_NE(deviceProperties.maxHardwareContexts, devicePropertiesBefore.maxHardwareContexts);
    EXPECT_NE(deviceProperties.numThreadsPerEU, devicePropertiesBefore.numThreadsPerEU);
    EXPECT_NE(deviceProperties.physicalEUSimdWidth, devicePropertiesBefore.physicalEUSimdWidth);
    EXPECT_NE(deviceProperties.numEUsPerSubslice, devicePropertiesBefore.numEUsPerSubslice);
    EXPECT_NE(deviceProperties.numSubslicesPerSlice, devicePropertiesBefore.numSubslicesPerSlice);
    EXPECT_NE(deviceProperties.numSlices, devicePropertiesBefore.numSlices);
    EXPECT_NE(deviceProperties.timerResolution, devicePropertiesBefore.timerResolution);
    EXPECT_NE(deviceProperties.timestampValidBits, devicePropertiesBefore.timestampValidBits);
    EXPECT_NE(deviceProperties.kernelTimestampValidBits, devicePropertiesBefore.kernelTimestampValidBits);
    EXPECT_NE(0, memcmp(&deviceProperties.name, &devicePropertiesBefore.name, sizeof(devicePropertiesBefore.name)));
    EXPECT_NE(deviceProperties.maxMemAllocSize, devicePropertiesBefore.maxMemAllocSize);
}

TEST_F(DeviceTest, givenDevicePropertiesStructureWhenDriverInfoIsEmptyThenDeviceNameTheSameAsInDeviceInfo) {
    auto deviceImp = static_cast<DeviceImp *>(device);
    ze_device_properties_t deviceProperties{};
    auto name = device->getNEODevice()->getDeviceInfo().name;
    deviceImp->driverInfo.reset();
    deviceImp->getProperties(&deviceProperties);
    EXPECT_STREQ(deviceProperties.name, name.c_str());
}

TEST_F(DeviceTest, givenDevicePropertiesStructureWhenDriverInfoIsNotEmptyThenDeviceNameTheSameAsInDriverInfo) {
    auto deviceImp = static_cast<DeviceImp *>(device);
    ze_device_properties_t deviceProperties{};
    auto driverInfo = std::make_unique<DriverInfoMock>();
    std::string customDevName = "Custom device name";
    auto name = device->getNEODevice()->getDeviceInfo().name;
    driverInfo->setDeviceName(customDevName);
    deviceImp->driverInfo.reset(driverInfo.release());
    deviceImp->getProperties(&deviceProperties);
    EXPECT_STREQ(deviceProperties.name, customDevName.c_str());
    EXPECT_STRNE(deviceProperties.name, name.c_str());
}

TEST_F(DeviceTest, givenDevicePropertiesStructureWhenDebugVariableOverrideDeviceNameIsSpecifiedThenDeviceNameIsTakenFromDebugVariable) {
    DebugManagerStateRestore restore;
    const std::string testDeviceName = "test device name";
    DebugManager.flags.OverrideDeviceName.set(testDeviceName);

    auto deviceImp = static_cast<DeviceImp *>(device);
    ze_device_properties_t deviceProperties{};
    auto name = device->getNEODevice()->getDeviceInfo().name;
    deviceImp->driverInfo.reset();
    deviceImp->getProperties(&deviceProperties);
    EXPECT_STRNE(deviceProperties.name, name.c_str());
    EXPECT_STREQ(deviceProperties.name, testDeviceName.c_str());
}

TEST_F(DeviceTest, WhenGettingDevicePropertiesThenSubslicesPerSliceIsBasedOnSubslicesSupported) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    deviceProperties.type = ZE_DEVICE_TYPE_GPU;

    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.MaxSubSlicesSupported = 48;
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.MaxSlicesSupported = 3;
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.SubSliceCount = 8;
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.SliceCount = 1;
    device->getProperties(&deviceProperties);

    EXPECT_EQ(8u, deviceProperties.numSubslicesPerSlice);
}

TEST_F(DeviceTest, GivenDebugApiUsedSetWhenGettingDevicePropertiesThenSubslicesPerSliceIsBasedOnMaxSubslicesSupported) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DebugApiUsed.set(1);
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    deviceProperties.type = ZE_DEVICE_TYPE_GPU;

    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.MaxSubSlicesSupported = 48;
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.MaxSlicesSupported = 3;
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.SubSliceCount = 8;
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.SliceCount = 1;
    device->getProperties(&deviceProperties);

    EXPECT_EQ(16u, deviceProperties.numSubslicesPerSlice);
}

TEST_F(DeviceTest, givenCallToDevicePropertiesThenMaximumMemoryToBeAllocatedIsCorrectlyReturned) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    deviceProperties.maxMemAllocSize = 0;
    device->getProperties(&deviceProperties);
    EXPECT_EQ(deviceProperties.maxMemAllocSize, this->neoDevice->getDeviceInfo().maxMemAllocSize);

    auto &hwHelper = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    auto expectedSize = this->neoDevice->getDeviceInfo().globalMemSize;
    if (!this->neoDevice->areSharedSystemAllocationsAllowed()) {
        expectedSize = std::min(expectedSize, hwHelper.getMaxMemAllocSize());
    }
    EXPECT_EQ(deviceProperties.maxMemAllocSize, expectedSize);
}

TEST_F(DeviceTest, whenCheckingIfStatelessCompressionIsSupportedThenReturnFalse) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.allowStatelessCompression(*defaultHwInfo));
}

TEST_F(DeviceTest, givenNodeOrdinalFlagWhenCallAdjustCommandQueueDescThenDescOrdinalProperlySet) {
    DebugManagerStateRestore restore;
    auto nodeOrdinal = EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, *defaultHwInfo);
    DebugManager.flags.NodeOrdinal.set(nodeOrdinal);

    auto deviceImp = static_cast<Mock<L0::DeviceImp> *>(device);
    ze_command_queue_desc_t desc = {};
    EXPECT_EQ(desc.ordinal, 0u);

    auto &engineGroups = deviceImp->getActiveDevice()->getRegularEngineGroups();
    engineGroups.clear();
    NEO::EngineGroupT engineGroupCompute{};
    engineGroupCompute.engineGroupType = NEO::EngineGroupType::Compute;
    NEO::EngineGroupT engineGroupRender{};
    engineGroupRender.engineGroupType = NEO::EngineGroupType::RenderCompute;
    engineGroups.push_back(engineGroupCompute);
    engineGroups.push_back(engineGroupRender);

    uint32_t expectedOrdinal = 1u;
    deviceImp->adjustCommandQueueDesc(desc);
    EXPECT_EQ(desc.ordinal, expectedOrdinal);
}

HWTEST_F(DeviceTest, givenNodeOrdinalFlagWhenCallAdjustCommandQueueDescThenDescOrdinalAndDescIndexProperlySet) {
    DebugManagerStateRestore restore;
    struct MockHwHelper : NEO::HwHelperHw<FamilyType> {
        EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const override {
            return EngineGroupType::Compute;
        }
    };
    auto hwInfo = *defaultHwInfo.get();
    MockHwHelper hwHelper{};
    VariableBackup<HwHelper *> hwHelperFactoryBackup{&NEO::hwHelperFactory[static_cast<size_t>(hwInfo.platform.eRenderCoreFamily)]};
    hwHelperFactoryBackup = &hwHelper;

    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    auto nodeOrdinal = EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_CCS2, hwInfo);
    DebugManager.flags.NodeOrdinal.set(nodeOrdinal);

    auto deviceImp = static_cast<Mock<L0::DeviceImp> *>(device);
    ze_command_queue_desc_t desc = {};
    EXPECT_EQ(desc.ordinal, 0u);

    auto &engineGroups = deviceImp->getActiveDevice()->getRegularEngineGroups();
    engineGroups.clear();
    NEO::EngineGroupT engineGroupCompute{};
    engineGroupCompute.engineGroupType = NEO::EngineGroupType::Compute;
    engineGroupCompute.engines.resize(3);
    auto osContext1 = std::make_unique<MockOsContext>(0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext1->engineType = aub_stream::EngineType::ENGINE_CCS;
    engineGroupCompute.engines[0].osContext = osContext1.get();
    auto osContext2 = std::make_unique<MockOsContext>(0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext2->engineType = aub_stream::EngineType::ENGINE_CCS1;
    engineGroupCompute.engines[1].osContext = osContext2.get();
    auto osContext3 = std::make_unique<MockOsContext>(0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext3->engineType = aub_stream::EngineType::ENGINE_CCS2;
    engineGroupCompute.engines[2].osContext = osContext3.get();
    NEO::EngineGroupT engineGroupRender{};
    engineGroupRender.engineGroupType = NEO::EngineGroupType::RenderCompute;
    engineGroups.push_back(engineGroupRender);
    engineGroups.push_back(engineGroupCompute);

    uint32_t expectedOrdinal = 1u;
    uint32_t expectedIndex = 2u;
    deviceImp->adjustCommandQueueDesc(desc);
    EXPECT_EQ(desc.ordinal, expectedOrdinal);
    EXPECT_EQ(desc.index, expectedIndex);
}

TEST_F(DeviceTest, givenNodeOrdinalFlagNotSetWhenCallAdjustCommandQueueDescThenDescOrdinalIsNotModified) {
    DebugManagerStateRestore restore;
    int nodeOrdinal = -1;
    DebugManager.flags.NodeOrdinal.set(nodeOrdinal);

    auto deviceImp = static_cast<Mock<L0::DeviceImp> *>(device);
    ze_command_queue_desc_t desc = {};
    EXPECT_EQ(desc.ordinal, 0u);

    deviceImp->adjustCommandQueueDesc(desc);
    EXPECT_EQ(desc.ordinal, 0u);
}

struct DeviceHwInfoTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1U);
    }

    void TearDown() override {
    }

    void setDriverAndDevice() {
        NEO::MockCompilerEnableGuard mock(true);
        std::vector<std::unique_ptr<NEO::Device>> devices;
        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0);
        EXPECT_NE(neoDevice, nullptr);

        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        ze_result_t res = driverHandle->initialize(std::move(devices));
        EXPECT_EQ(res, ZE_RESULT_SUCCESS);
        device = driverHandle->devices[0];
    }

    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
};

TEST_F(DeviceHwInfoTest, givenDeviceWithNoPageFaultSupportThenFlagIsNotSet) {
    NEO::HardwareInfo hardwareInfo = *NEO::defaultHwInfo;
    hardwareInfo.capabilityTable.supportsOnDemandPageFaults = false;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hardwareInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    setDriverAndDevice();

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    device->getProperties(&deviceProps);
    EXPECT_FALSE(deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING);
}

TEST_F(DeviceHwInfoTest, givenDeviceWithPageFaultSupportThenFlagIsSet) {
    NEO::HardwareInfo hardwareInfo = *NEO::defaultHwInfo;
    hardwareInfo.capabilityTable.supportsOnDemandPageFaults = true;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hardwareInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    setDriverAndDevice();

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    device->getProperties(&deviceProps);
    EXPECT_TRUE(deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING);
}

TEST_F(DeviceTest, whenGetDevicePropertiesCalledThenCorrectDevicePropertyEccFlagSet) {
    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};

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

TEST_F(DeviceTest, givenCallToDevicePropertiesThenTimestampValidBitsAreCorrectlyAssigned) {
    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    device->getProperties(&deviceProps);
    EXPECT_EQ(device->getHwInfo().capabilityTable.timestampValidBits, deviceProps.timestampValidBits);
    EXPECT_EQ(device->getHwInfo().capabilityTable.kernelTimestampValidBits, deviceProps.kernelTimestampValidBits);
}

TEST_F(DeviceTest, givenNullDriverInfowhenPciPropertiesIsCalledThenUninitializedErrorIsReturned) {
    auto deviceImp = static_cast<L0::DeviceImp *>(device);
    ze_pci_ext_properties_t pciProperties = {};

    deviceImp->driverInfo.reset(nullptr);
    ze_result_t res = device->getPciProperties(&pciProperties);

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, res);
}

TEST_F(DeviceTest, givenValidPciExtPropertiesWhenPciPropertiesIsCalledThenSuccessIsReturned) {

    auto deviceImp = static_cast<L0::DeviceImp *>(device);
    const NEO::PhysicalDevicePciBusInfo pciBusInfo(0, 1, 2, 3);
    NEO::DriverInfoMock *driverInfo = new DriverInfoMock();
    ze_pci_ext_properties_t pciProperties = {};

    driverInfo->setPciBusInfo(pciBusInfo);
    deviceImp->driverInfo.reset(driverInfo);
    ze_result_t res = device->getPciProperties(&pciProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(pciBusInfo.pciDomain, pciProperties.address.domain);
    EXPECT_EQ(pciBusInfo.pciBus, pciProperties.address.bus);
    EXPECT_EQ(pciBusInfo.pciDevice, pciProperties.address.device);
    EXPECT_EQ(pciBusInfo.pciFunction, pciProperties.address.function);
}

TEST_F(DeviceTest, givenInvalidPciBusInfoWhenPciPropertiesIsCalledThenUninitializedErrorIsReturned) {
    constexpr uint32_t invalid = NEO::PhysicalDevicePciBusInfo::invalidValue;
    auto deviceImp = static_cast<L0::DeviceImp *>(device);
    ze_pci_ext_properties_t pciProperties = {};
    std::vector<NEO::PhysicalDevicePciBusInfo> pciBusInfos;

    pciBusInfos.push_back(NEO::PhysicalDevicePciBusInfo(0, 1, 2, invalid));
    pciBusInfos.push_back(NEO::PhysicalDevicePciBusInfo(0, 1, invalid, 3));
    pciBusInfos.push_back(NEO::PhysicalDevicePciBusInfo(0, invalid, 2, 3));
    pciBusInfos.push_back(NEO::PhysicalDevicePciBusInfo(invalid, 1, 2, 3));

    for (auto pciBusInfo : pciBusInfos) {
        NEO::DriverInfoMock *driverInfo = new DriverInfoMock();
        driverInfo->setPciBusInfo(pciBusInfo);
        deviceImp->driverInfo.reset(driverInfo);

        ze_result_t res = device->getPciProperties(&pciProperties);
        EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, res);
    }
}
TEST_F(DeviceTest, whenGetExternalMemoryPropertiesIsCalledThenSuccessIsReturnedAndNoPropertiesAreReturned) {
    ze_device_external_memory_properties_t externalMemoryProperties;

    ze_result_t result = device->getExternalMemoryProperties(&externalMemoryProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(externalMemoryProperties.imageExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD);
    EXPECT_FALSE(externalMemoryProperties.imageExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
    EXPECT_FALSE(externalMemoryProperties.imageImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD);
    EXPECT_FALSE(externalMemoryProperties.imageImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
    EXPECT_FALSE(externalMemoryProperties.memoryAllocationExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD);
    EXPECT_TRUE(externalMemoryProperties.memoryAllocationExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
    EXPECT_FALSE(externalMemoryProperties.memoryAllocationImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD);
    EXPECT_TRUE(externalMemoryProperties.memoryAllocationImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
}

TEST_F(DeviceTest, whenGetGlobalTimestampIsCalledThenSuccessIsReturnedAndValuesSetCorrectly) {
    uint64_t hostTs = 0u;
    uint64_t deviceTs = 0u;

    ze_result_t result = device->getGlobalTimestamps(&hostTs, &deviceTs);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_NE(0u, hostTs);
    EXPECT_NE(0u, deviceTs);
}

class FalseCpuGpuDeviceTime : public NEO::DeviceTime {
  public:
    bool getCpuGpuTime(TimeStampData *pGpuCpuTime, OSTime *osTime) override {
        return false;
    }
    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const override {
        return NEO::OSTime::getDeviceTimerResolution(hwInfo);
    }
    uint64_t getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const override {
        return static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution(hwInfo));
    }
};

class FalseCpuGpuTime : public NEO::OSTime {
  public:
    FalseCpuGpuTime() {
        this->deviceTime = std::make_unique<FalseCpuGpuDeviceTime>();
    }

    bool getCpuTime(uint64_t *timeStamp) override {
        return true;
    };
    double getHostTimerResolution() const override {
        return 0;
    }
    uint64_t getCpuRawTimestamp() override {
        return 0;
    }
    static std::unique_ptr<OSTime> create() {
        return std::unique_ptr<OSTime>(new FalseCpuGpuTime());
    }
};

struct GlobalTimestampTest : public ::testing::Test {
    void SetUp() override {
        DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), rootDeviceIndex);
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
};

TEST_F(GlobalTimestampTest, whenGetGlobalTimestampCalledAndGetCpuGpuTimeIsFalseReturnError) {
    uint64_t hostTs = 0u;
    uint64_t deviceTs = 0u;

    neoDevice->setOSTime(new FalseCpuGpuTime());
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];

    ze_result_t result = device->getGlobalTimestamps(&hostTs, &deviceTs);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
}

TEST_F(GlobalTimestampTest, whenGetProfilingTimerClockandProfilingTimerResolutionThenVerifyRelation) {
    neoDevice->setOSTime(new FalseCpuGpuTime());
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));

    uint64_t timerClock = neoDevice->getProfilingTimerClock();
    EXPECT_NE(timerClock, 0u);
    double timerResolution = neoDevice->getProfilingTimerResolution();
    EXPECT_NE(timerResolution, 0.0);
    EXPECT_EQ(timerClock, static_cast<uint64_t>(1000000000.0 / timerResolution));
}

TEST_F(GlobalTimestampTest, whenQueryingForTimerResolutionWithLegacyDevicePropertiesStructThenDefaultTimerResolutionInNanoSecondsIsReturned) {
    neoDevice->setOSTime(new FalseCpuGpuTime());
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    std::unique_ptr<L0::DriverHandleImp> driverHandle = std::make_unique<L0::DriverHandleImp>();
    driverHandle->initialize(std::move(devices));

    double timerResolution = neoDevice->getProfilingTimerResolution();
    EXPECT_NE(timerResolution, 0.0);

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_result_t res = driverHandle->devices[0]->getProperties(&deviceProps);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(deviceProps.timerResolution, static_cast<uint64_t>(timerResolution));
}

TEST_F(GlobalTimestampTest, whenQueryingForTimerResolutionWithDeviceProperties_1_2_StructThenDefaultTimerResolutionInCyclesPerSecondsIsReturned) {
    neoDevice->setOSTime(new FalseCpuGpuTime());
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    std::unique_ptr<L0::DriverHandleImp> driverHandle = std::make_unique<L0::DriverHandleImp>();
    driverHandle->initialize(std::move(devices));

    uint64_t timerClock = neoDevice->getProfilingTimerClock();
    EXPECT_NE(timerClock, 0u);

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2};
    ze_result_t res = driverHandle->devices[0]->getProperties(&deviceProps);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(deviceProps.timerResolution, timerClock);
}

TEST_F(GlobalTimestampTest, whenQueryingForTimerResolutionWithUseCyclesPerSecondTimerSetThenTimerResolutionInCyclesPerSecondsIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseCyclesPerSecondTimer.set(1u);

    neoDevice->setOSTime(new FalseCpuGpuTime());
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    std::unique_ptr<L0::DriverHandleImp> driverHandle = std::make_unique<L0::DriverHandleImp>();
    driverHandle->initialize(std::move(devices));

    uint64_t timerClock = neoDevice->getProfilingTimerClock();
    EXPECT_NE(timerClock, 0u);

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_result_t res = driverHandle->devices[0]->getProperties(&deviceProps);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(deviceProps.timerResolution, timerClock);
}

class FalseCpuDeviceTime : public NEO::DeviceTime {
  public:
    bool getCpuGpuTime(TimeStampData *pGpuCpuTime, NEO::OSTime *) override {
        return true;
    }
    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const override {
        return NEO::OSTime::getDeviceTimerResolution(hwInfo);
    }
    uint64_t getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const override {
        return static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution(hwInfo));
    }
};

class FalseCpuTime : public NEO::OSTime {
  public:
    FalseCpuTime() {
        this->deviceTime = std::make_unique<FalseCpuDeviceTime>();
    }
    bool getCpuTime(uint64_t *timeStamp) override {
        return false;
    };
    double getHostTimerResolution() const override {
        return 0;
    }
    uint64_t getCpuRawTimestamp() override {
        return 0;
    }
    static std::unique_ptr<OSTime> create() {
        return std::unique_ptr<OSTime>(new FalseCpuTime());
    }
};

TEST_F(GlobalTimestampTest, whenGetGlobalTimestampCalledAndGetCpuTimeIsFalseReturnError) {
    uint64_t hostTs = 0u;
    uint64_t deviceTs = 0u;

    neoDevice->setOSTime(new FalseCpuTime());
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];

    ze_result_t result = device->getGlobalTimestamps(&hostTs, &deviceTs);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
}

using DeviceGetMemoryTests = DeviceTest;

TEST_F(DeviceGetMemoryTests, whenCallingGetMemoryPropertiesWithCountZeroThenOneIsReturned) {
    uint32_t count = 0;
    ze_result_t res = device->getMemoryProperties(&count, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);
}

TEST_F(DeviceGetMemoryTests, whenCallingGetMemoryPropertiesWithNullPtrThenInvalidArgumentIsReturned) {
    uint32_t count = 0;
    ze_result_t res = device->getMemoryProperties(&count, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    count++;
    res = device->getMemoryProperties(&count, nullptr);
    EXPECT_EQ(res, ZE_RESULT_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(1u, count);
}

TEST_F(DeviceGetMemoryTests, whenCallingGetMemoryPropertiesWithNonNullPtrThenPropertiesAreReturned) {
    uint32_t count = 0;
    ze_result_t res = device->getMemoryProperties(&count, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    ze_device_memory_properties_t memProperties = {};
    res = device->getMemoryProperties(&count, &memProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);
    auto hwInfo = *NEO::defaultHwInfo;
    auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
    EXPECT_EQ(memProperties.maxClockRate, hwInfoConfig.getDeviceMemoryMaxClkRate(hwInfo, nullptr, 0));
    EXPECT_EQ(memProperties.maxBusWidth, this->neoDevice->getDeviceInfo().addressBits);
    EXPECT_EQ(memProperties.totalSize, this->neoDevice->getDeviceInfo().globalMemSize);
    EXPECT_EQ(0u, memProperties.flags);
}

HWTEST2_F(DeviceGetMemoryTests, whenCallingGetMemoryPropertiesForMemoryExtPropertiesThenPropertiesAreReturned, MatchAny) {
    uint32_t count = 0;
    ze_result_t res = device->getMemoryProperties(&count, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    auto hwInfo = *NEO::defaultHwInfo;
    MockHwInfoConfigHw<productFamily> hwInfoConfig;
    VariableBackup<HwInfoConfig *> hwInfoConfigFactoryBackup{&NEO::hwInfoConfigFactory[static_cast<size_t>(hwInfo.platform.eProductFamily)]};
    hwInfoConfigFactoryBackup = &hwInfoConfig;

    ze_device_memory_properties_t memProperties = {};
    ze_device_memory_ext_properties_t memExtProperties = {};
    memExtProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_EXT_PROPERTIES;
    memProperties.pNext = &memExtProperties;
    res = device->getMemoryProperties(&count, &memProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);
    const std::array<ze_device_memory_ext_type_t, 5> sysInfoMemType = {
        ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR4,
        ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR5,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
        ZE_DEVICE_MEMORY_EXT_TYPE_GDDR6,
    };

    auto bandwidthPerNanoSecond = hwInfoConfig.getDeviceMemoryMaxBandWidthInBytesPerSecond(hwInfo, nullptr, 0) / 1000000000;

    EXPECT_EQ(memExtProperties.type, sysInfoMemType[hwInfo.gtSystemInfo.MemoryType]);
    EXPECT_EQ(memExtProperties.physicalSize, hwInfoConfig.getDeviceMemoryPhysicalSizeInBytes(nullptr, 0));
    EXPECT_EQ(memExtProperties.readBandwidth, bandwidthPerNanoSecond);
    EXPECT_EQ(memExtProperties.writeBandwidth, memExtProperties.readBandwidth);
    EXPECT_EQ(memExtProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
}

TEST_F(DeviceGetMemoryTests, whenCallingGetMemoryPropertiesWhenPnextIsNonNullAndStypeIsUnSupportedThenNoErrorIsReturned) {
    uint32_t count = 0;
    ze_result_t res = device->getMemoryProperties(&count, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    ze_device_memory_properties_t memProperties = {};
    ze_device_memory_ext_properties_t memExtProperties = {};
    memExtProperties.stype = ZE_STRUCTURE_TYPE_FORCE_UINT32;
    memProperties.pNext = &memExtProperties;
    res = device->getMemoryProperties(&count, &memProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
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
    ze_device_module_properties_t kernelProperties = {};
    memset(&kernelProperties, std::numeric_limits<int>::max(), sizeof(ze_device_module_properties_t));
    kernelProperties.pNext = nullptr;

    device->getKernelProperties(&kernelProperties);
    EXPECT_FALSE(kernelProperties.flags & ZE_DEVICE_MODULE_FLAG_FP64);
    EXPECT_EQ(0u, kernelProperties.fp64flags);

    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.OverrideDefaultFP64Settings.set(1);

    device->getKernelProperties(&kernelProperties);
    EXPECT_TRUE(kernelProperties.flags & ZE_DEVICE_MODULE_FLAG_FP64);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_ROUNDED_DIVIDE_SQRT);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_ROUND_TO_NEAREST);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_ROUND_TO_ZERO);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_ROUND_TO_INF);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_INF_NAN);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_DENORM);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_FMA);
}

struct DeviceHasFp64Test : public ::testing::Test {
    void SetUp() override {
        DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        HardwareInfo fp64DeviceInfo = *defaultHwInfo;
        fp64DeviceInfo.capabilityTable.ftrSupportsFP64 = true;
        fp64DeviceInfo.capabilityTable.ftrSupports64BitMath = true;
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&fp64DeviceInfo, rootDeviceIndex);
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

TEST_F(DeviceHasFp64Test, givenDeviceWithFp64ThenReportCorrectFp64Flags) {
    ze_device_module_properties_t kernelProperties = {};
    memset(&kernelProperties, std::numeric_limits<int>::max(), sizeof(ze_device_module_properties_t));
    kernelProperties.pNext = nullptr;

    device->getKernelProperties(&kernelProperties);

    device->getKernelProperties(&kernelProperties);
    EXPECT_TRUE(kernelProperties.flags & ZE_DEVICE_MODULE_FLAG_FP64);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_ROUNDED_DIVIDE_SQRT);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_ROUND_TO_NEAREST);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_ROUND_TO_ZERO);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_ROUND_TO_INF);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_INF_NAN);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_DENORM);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_FMA);

    EXPECT_TRUE(kernelProperties.fp32flags & ZE_DEVICE_FP_FLAG_ROUNDED_DIVIDE_SQRT);
    EXPECT_TRUE(kernelProperties.fp32flags & ZE_DEVICE_FP_FLAG_ROUND_TO_NEAREST);
    EXPECT_TRUE(kernelProperties.fp32flags & ZE_DEVICE_FP_FLAG_ROUND_TO_ZERO);
    EXPECT_TRUE(kernelProperties.fp32flags & ZE_DEVICE_FP_FLAG_ROUND_TO_INF);
    EXPECT_TRUE(kernelProperties.fp32flags & ZE_DEVICE_FP_FLAG_INF_NAN);
    EXPECT_TRUE(kernelProperties.fp32flags & ZE_DEVICE_FP_FLAG_DENORM);
    EXPECT_TRUE(kernelProperties.fp32flags & ZE_DEVICE_FP_FLAG_FMA);
}

struct DeviceHasFp64ButNoBitMathTest : public ::testing::Test {
    void SetUp() override {
        DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        HardwareInfo fp64DeviceInfo = *defaultHwInfo;
        fp64DeviceInfo.capabilityTable.ftrSupportsFP64 = true;
        fp64DeviceInfo.capabilityTable.ftrSupports64BitMath = false;
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&fp64DeviceInfo, rootDeviceIndex);
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

TEST_F(DeviceHasFp64ButNoBitMathTest, givenDeviceWithFp64ButNoBitMathThenReportCorrectFp64Flags) {
    ze_device_module_properties_t kernelProperties = {};
    memset(&kernelProperties, std::numeric_limits<int>::max(), sizeof(ze_device_module_properties_t));
    kernelProperties.pNext = nullptr;

    device->getKernelProperties(&kernelProperties);

    device->getKernelProperties(&kernelProperties);
    EXPECT_TRUE(kernelProperties.flags & ZE_DEVICE_MODULE_FLAG_FP64);
    EXPECT_FALSE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_ROUNDED_DIVIDE_SQRT);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_ROUND_TO_NEAREST);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_ROUND_TO_ZERO);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_ROUND_TO_INF);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_INF_NAN);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_DENORM);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_FMA);

    EXPECT_FALSE(kernelProperties.fp32flags & ZE_DEVICE_FP_FLAG_ROUNDED_DIVIDE_SQRT);
    EXPECT_TRUE(kernelProperties.fp32flags & ZE_DEVICE_FP_FLAG_ROUND_TO_NEAREST);
    EXPECT_TRUE(kernelProperties.fp32flags & ZE_DEVICE_FP_FLAG_ROUND_TO_ZERO);
    EXPECT_TRUE(kernelProperties.fp32flags & ZE_DEVICE_FP_FLAG_ROUND_TO_INF);
    EXPECT_TRUE(kernelProperties.fp32flags & ZE_DEVICE_FP_FLAG_INF_NAN);
    EXPECT_TRUE(kernelProperties.fp32flags & ZE_DEVICE_FP_FLAG_DENORM);
    EXPECT_TRUE(kernelProperties.fp32flags & ZE_DEVICE_FP_FLAG_FMA);
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
    ze_device_module_properties_t kernelProperties = {};
    memset(&kernelProperties, std::numeric_limits<int>::max(), sizeof(ze_device_module_properties_t));
    kernelProperties.pNext = nullptr;

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
    ze_device_module_properties_t kernelProperties = {};
    memset(&kernelProperties, std::numeric_limits<int>::max(), sizeof(ze_device_module_properties_t));
    kernelProperties.pNext = nullptr;

    device->getKernelProperties(&kernelProperties);
    EXPECT_TRUE(kernelProperties.flags & ZE_DEVICE_MODULE_FLAG_FP16);
    EXPECT_TRUE(kernelProperties.flags & ZE_DEVICE_MODULE_FLAG_INT64_ATOMICS);
}

struct MockMemoryManagerMultiDevice : public MemoryManagerMock {
    MockMemoryManagerMultiDevice(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
};

template <int32_t enablePartitionWalker>
struct MultipleDevicesFixture : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        DebugManager.flags.EnableWalkerPartition.set(enablePartitionWalker);
        DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }

        memoryManager = new MockMemoryManagerMultiDevice(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
        }
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));

        context = std::make_unique<ContextShareableMock>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        for (auto i = 0u; i < numRootDevices; i++) {
            auto device = driverHandle->devices[i];
            context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
            auto neoDevice = device->getNEODevice();
            context->rootDeviceIndices.push_back(neoDevice->getRootDeviceIndex());
            context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
        }
        context->rootDeviceIndices.remove_duplicates();
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    MockMemoryManagerMultiDevice *memoryManager = nullptr;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    std::unique_ptr<ContextShareableMock> context;

    const uint32_t numRootDevices = 2u;
    const uint32_t numSubDevices = 2u;
};

using MultipleDevicesTest = MultipleDevicesFixture<-1>;
using MultipleDevicesDisabledImplicitScalingTest = MultipleDevicesFixture<0>;
using MultipleDevicesEnabledImplicitScalingTest = MultipleDevicesFixture<1>;

TEST_F(MultipleDevicesDisabledImplicitScalingTest, whenCallingGetMemoryPropertiesWithSubDevicesThenCorrectSizeReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    uint32_t count = 1;

    ze_device_memory_properties_t memProperties = {};
    ze_result_t res = device0->getMemoryProperties(&count, &memProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);
    EXPECT_EQ(memProperties.totalSize, device0->getNEODevice()->getDeviceInfo().globalMemSize / numSubDevices);
}

TEST_F(MultipleDevicesEnabledImplicitScalingTest, whenCallingGetMemoryPropertiesWithSubDevicesThenCorrectSizeReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    uint32_t count = 1;

    ze_device_memory_properties_t memProperties = {};
    ze_result_t res = device0->getMemoryProperties(&count, &memProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);
    EXPECT_EQ(memProperties.totalSize, device0->getNEODevice()->getDeviceInfo().globalMemSize);
}

TEST_F(MultipleDevicesDisabledImplicitScalingTest, GivenImplicitScalingDisabledWhenGettingDevicePropertiesGetSubslicesPerSliceThenCorrectValuesReturned) {
    L0::Device *device = driverHandle->devices[0];
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    deviceProperties.type = ZE_DEVICE_TYPE_GPU;

    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.MaxSubSlicesSupported = 48;
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.MaxSlicesSupported = 3;
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.SubSliceCount = 8;
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.SliceCount = 2;

    auto &gtSysInfo = device->getNEODevice()->getHardwareInfo().gtSystemInfo;

    device->getProperties(&deviceProperties);
    EXPECT_EQ(((gtSysInfo.SubSliceCount / gtSysInfo.SliceCount)), deviceProperties.numSubslicesPerSlice);
    EXPECT_EQ(gtSysInfo.SliceCount, deviceProperties.numSlices);
}

TEST_F(MultipleDevicesEnabledImplicitScalingTest, GivenImplicitScalingEnabledWhenGettingDevicePropertiesGetSubslicesPerSliceThenCorrectValuesReturned) {
    L0::Device *device = driverHandle->devices[0];
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    deviceProperties.type = ZE_DEVICE_TYPE_GPU;

    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.MaxSubSlicesSupported = 48;
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.MaxSlicesSupported = 3;
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.SubSliceCount = 8;
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.SliceCount = 2;

    auto &gtSysInfo = device->getNEODevice()->getHardwareInfo().gtSystemInfo;

    device->getProperties(&deviceProperties);
    EXPECT_EQ((gtSysInfo.SubSliceCount / gtSysInfo.SliceCount), deviceProperties.numSubslicesPerSlice);
    EXPECT_EQ((gtSysInfo.SliceCount * numSubDevices), deviceProperties.numSlices);
}

HWTEST2_F(MultipleDevicesEnabledImplicitScalingTest, GivenImplicitScalingEnabledDeviceWhenCallingGetMemoryPropertiesForMemoryExtPropertiesThenPropertiesAreReturned, MatchAny) {
    uint32_t count = 0;
    L0::Device *device = driverHandle->devices[0];
    auto hwInfo = *NEO::defaultHwInfo;
    ze_result_t res = device->getMemoryProperties(&count, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    MockHwInfoConfigHw<productFamily> hwInfoConfig;
    VariableBackup<HwInfoConfig *> hwInfoConfigFactoryBackup{&NEO::hwInfoConfigFactory[static_cast<size_t>(hwInfo.platform.eProductFamily)]};
    hwInfoConfigFactoryBackup = &hwInfoConfig;

    ze_device_memory_properties_t memProperties = {};
    ze_device_memory_ext_properties_t memExtProperties = {};
    memExtProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_EXT_PROPERTIES;
    memProperties.pNext = &memExtProperties;
    res = device->getMemoryProperties(&count, &memProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);
    const std::array<ze_device_memory_ext_type_t, 5> sysInfoMemType = {
        ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR4,
        ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR5,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
        ZE_DEVICE_MEMORY_EXT_TYPE_GDDR6,
    };

    auto bandwidthPerNanoSecond = hwInfoConfig.getDeviceMemoryMaxBandWidthInBytesPerSecond(hwInfo, nullptr, 0) / 1000000000;

    EXPECT_EQ(memExtProperties.type, sysInfoMemType[hwInfo.gtSystemInfo.MemoryType]);
    EXPECT_EQ(memExtProperties.physicalSize, hwInfoConfig.getDeviceMemoryPhysicalSizeInBytes(nullptr, 0) * numSubDevices);
    EXPECT_EQ(memExtProperties.readBandwidth, bandwidthPerNanoSecond * numSubDevices);
    EXPECT_EQ(memExtProperties.writeBandwidth, memExtProperties.readBandwidth);
    EXPECT_EQ(memExtProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
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

TEST_F(MultipleDevicesTest, givenNonZeroNumbersOfSubdevicesWhenGetSubDevicesIsCalledWithNullPointerThenInvalidArgumentIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];

    uint32_t count = 1;
    auto result = device0->getSubDevices(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
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

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};

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

TEST_F(MultipleDevicesDisabledImplicitScalingTest, givenTwoRootDevicesFromSameFamilyThenCanAccessPeerSuccessfullyCompletes) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    GFXCORE_FAMILY device0Family = device0->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    GFXCORE_FAMILY device1Family = device1->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    EXPECT_EQ(device0Family, device1Family);

    ze_bool_t canAccess = true;
    ze_result_t res = device0->canAccessPeer(device1->toHandle(), &canAccess);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

HWTEST_F(MultipleDevicesDisabledImplicitScalingTest, givenTwoRootDevicesFromSameFamilyAndDeviceLostSynchronizeThenCanAccessPeerReturnsDeviceLost) {
    constexpr size_t devicesCount{2};
    ASSERT_LE(devicesCount, driverHandle->devices.size());

    L0::Device *devices[devicesCount] = {driverHandle->devices[0], driverHandle->devices[1]};
    std::vector<NEO::Device *> allNeoDevices{};

    for (const auto device : devices) {
        const auto neoDevice = device->getNEODevice();
        const auto neoSubDevices = neoDevice->getSubDevices();

        allNeoDevices.push_back(neoDevice);
        allNeoDevices.insert(allNeoDevices.end(), neoSubDevices.begin(), neoSubDevices.end());
    }

    for (const auto neoDevice : allNeoDevices) {
        auto &deviceRegularEngines = neoDevice->getRegularEngineGroups();
        ASSERT_EQ(1u, deviceRegularEngines.size());
        ASSERT_EQ(1u, deviceRegularEngines[0].engines.size());

        auto &deviceEngine = deviceRegularEngines[0].engines[0];
        auto hwCsr = static_cast<CommandStreamReceiverHw<FamilyType> *>(deviceEngine.commandStreamReceiver);
        auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(hwCsr);

        ultCsr->callBaseWaitForCompletionWithTimeout = false;
        ultCsr->returnWaitForCompletionWithTimeout = WaitStatus::GpuHang;
    }

    GFXCORE_FAMILY device0Family = devices[0]->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    GFXCORE_FAMILY device1Family = devices[1]->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    EXPECT_EQ(device0Family, device1Family);

    ze_bool_t canAccess = true;
    ze_result_t res = devices[0]->canAccessPeer(devices[1]->toHandle(), &canAccess);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, res);
}

using DeviceTests = Test<DeviceFixture>;

TEST_F(DeviceTests, WhenGettingMemoryAccessPropertiesThenSuccessIsReturned) {
    ze_device_memory_access_properties_t properties;
    auto result = device->getMemoryAccessProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &hwInfo = device->getHwInfo();
    auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto expectedHostAllocCapabilities = static_cast<ze_memory_access_cap_flags_t>(hwInfoConfig.getHostMemCapabilities(&hwInfo));
    EXPECT_EQ(expectedHostAllocCapabilities, properties.hostAllocCapabilities);

    auto expectedDeviceAllocCapabilities = static_cast<ze_memory_access_cap_flags_t>(hwInfoConfig.getDeviceMemCapabilities());
    EXPECT_EQ(expectedDeviceAllocCapabilities, properties.deviceAllocCapabilities);

    auto expectedSharedSingleDeviceAllocCapabilities = static_cast<ze_memory_access_cap_flags_t>(hwInfoConfig.getSingleDeviceSharedMemCapabilities());
    EXPECT_EQ(expectedSharedSingleDeviceAllocCapabilities, properties.sharedSingleDeviceAllocCapabilities);

    auto expectedSharedSystemAllocCapabilities = static_cast<ze_memory_access_cap_flags_t>(hwInfoConfig.getSharedSystemMemCapabilities(&hwInfo));
    EXPECT_EQ(expectedSharedSystemAllocCapabilities, properties.sharedSystemAllocCapabilities);
}

template <bool p2pAccessDevice0, bool p2pAtomicAccessDevice0, bool p2pAccessDevice1, bool p2pAtomicAccessDevice1>
struct MultipleDevicesP2PFixture : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);

        NEO::HardwareInfo hardwareInfo = *NEO::defaultHwInfo;

        hardwareInfo.capabilityTable.p2pAccessSupported = p2pAccessDevice0;
        hardwareInfo.capabilityTable.p2pAtomicAccessSupported = p2pAtomicAccessDevice0;
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hardwareInfo);
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        hardwareInfo.capabilityTable.p2pAccessSupported = p2pAccessDevice1;
        hardwareInfo.capabilityTable.p2pAtomicAccessSupported = p2pAtomicAccessDevice1;
        executionEnvironment->rootDeviceEnvironments[1]->setHwInfo(&hardwareInfo);
        executionEnvironment->rootDeviceEnvironments[1]->initGmm();

        memoryManager = new MockMemoryManagerMultiDevice(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
        }
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));

        context = std::make_unique<ContextImp>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        for (auto i = 0u; i < numRootDevices; i++) {
            auto device = driverHandle->devices[i];
            context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
            auto neoDevice = device->getNEODevice();
            context->rootDeviceIndices.push_back(neoDevice->getRootDeviceIndex());
            context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
        }
        context->rootDeviceIndices.remove_duplicates();
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    MockMemoryManagerMultiDevice *memoryManager = nullptr;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    std::unique_ptr<ContextImp> context;

    const uint32_t numRootDevices = 2u;
    const uint32_t numSubDevices = 2u;
};

using MemoryAccessPropertieP2PAccess0Atomic0 = MultipleDevicesP2PFixture<0, 0, 0, 0>;
TEST_F(MemoryAccessPropertieP2PAccess0Atomic0, WhenCallingGetMemoryAccessPropertiesWithDevicesHavingNoAccessSupportThenNoSupportIsReturned) {
    L0::Device *device = driverHandle->devices[0];
    ze_device_memory_access_properties_t properties;
    auto result = device->getMemoryAccessProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_memory_access_cap_flags_t expectedSharedCrossDeviceAllocCapabilities = {};
    EXPECT_EQ(expectedSharedCrossDeviceAllocCapabilities, properties.sharedCrossDeviceAllocCapabilities);
}

using MemoryAccessPropertieP2PAccess1Atomic0 = MultipleDevicesP2PFixture<1, 0, 0, 0>;
TEST_F(MemoryAccessPropertieP2PAccess1Atomic0, WhenCallingGetMemoryAccessPropertiesWithDevicesHavingP2PAccessSupportThenSupportIsReturned) {
    L0::Device *device = driverHandle->devices[0];
    ze_device_memory_access_properties_t properties;
    auto result = device->getMemoryAccessProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_memory_access_cap_flags_t expectedSharedCrossDeviceAllocCapabilities =
        ZE_MEMORY_ACCESS_CAP_FLAG_RW | ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT;
    EXPECT_EQ(expectedSharedCrossDeviceAllocCapabilities, properties.sharedCrossDeviceAllocCapabilities);
}

using MemoryAccessPropertieP2PAccess1Atomic1 = MultipleDevicesP2PFixture<1, 1, 0, 0>;
TEST_F(MemoryAccessPropertieP2PAccess1Atomic1, WhenCallingGetMemoryAccessPropertiesWithDevicesHavingP2PAndAtomicAccessSupportThenSupportIsReturned) {
    L0::Device *device = driverHandle->devices[0];
    ze_device_memory_access_properties_t properties;
    auto result = device->getMemoryAccessProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_memory_access_cap_flags_t expectedSharedCrossDeviceAllocCapabilities =
        ZE_MEMORY_ACCESS_CAP_FLAG_RW | ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT |
        ZE_MEMORY_ACCESS_CAP_FLAG_ATOMIC | ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT_ATOMIC;
    EXPECT_EQ(expectedSharedCrossDeviceAllocCapabilities, properties.sharedCrossDeviceAllocCapabilities);
}

using MultipleDevicesP2PDevice0Access0Atomic0Device1Access0Atomic0Test = MultipleDevicesP2PFixture<0, 0, 0, 0>;
TEST_F(MultipleDevicesP2PDevice0Access0Atomic0Device1Access0Atomic0Test, WhenCallingGetP2PPropertiesWithBothDevicesHavingNoAccessSupportThenNoSupportIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_device_p2p_properties_t p2pProperties = {};
    device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

using MultipleDevicesP2PDevice0Access0Atomic0Device1Access1Atomic0Test = MultipleDevicesP2PFixture<0, 0, 1, 0>;
TEST_F(MultipleDevicesP2PDevice0Access0Atomic0Device1Access1Atomic0Test, WhenCallingGetP2PPropertiesWithOnlyOneDeviceHavingAccessSupportThenNoSupportIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_device_p2p_properties_t p2pProperties = {};
    device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

using MultipleDevicesP2PDevice0Access1Atomic0Device1Access0Atomic0Test = MultipleDevicesP2PFixture<1, 0, 0, 0>;
TEST_F(MultipleDevicesP2PDevice0Access1Atomic0Device1Access0Atomic0Test, WhenCallingGetP2PPropertiesWithOnlyFirstDeviceHavingAccessSupportThenNoSupportIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_device_p2p_properties_t p2pProperties = {};
    device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

using MultipleDevicesP2PDevice0Access1Atomic0Device1Access1Atomic0Test = MultipleDevicesP2PFixture<1, 0, 1, 0>;
TEST_F(MultipleDevicesP2PDevice0Access1Atomic0Device1Access1Atomic0Test, WhenCallingGetP2PPropertiesWithBothDevicesHavingAccessSupportThenSupportIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_device_p2p_properties_t p2pProperties = {};
    device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

using MultipleDevicesP2PDevice0Access1Atomic0Device1Access1Atomic1Test = MultipleDevicesP2PFixture<1, 0, 1, 1>;
TEST_F(MultipleDevicesP2PDevice0Access1Atomic0Device1Access1Atomic1Test, WhenCallingGetP2PPropertiesWithBothDevicesHavingAccessSupportAndOnlyOneWithAtomicThenSupportIsReturnedOnlyForAccess) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_device_p2p_properties_t p2pProperties = {};
    device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

using MultipleDevicesP2PDevice0Access1Atomic1Device1Access1Atomic0Test = MultipleDevicesP2PFixture<1, 1, 1, 0>;
TEST_F(MultipleDevicesP2PDevice0Access1Atomic1Device1Access1Atomic0Test, WhenCallingGetP2PPropertiesWithBothDevicesHavingAccessSupportAndOnlyFirstWithAtomicThenSupportIsReturnedOnlyForAccess) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_device_p2p_properties_t p2pProperties = {};
    device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

using MultipleDevicesP2PDevice0Access1Atomic1Device1Access1Atomic1Test = MultipleDevicesP2PFixture<1, 1, 1, 1>;
TEST_F(MultipleDevicesP2PDevice0Access1Atomic1Device1Access1Atomic1Test, WhenCallingGetP2PPropertiesWithBothDevicesHavingAccessAndAtomicSupportThenSupportIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_device_p2p_properties_t p2pProperties = {};
    device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

TEST_F(MultipleDevicesDisabledImplicitScalingTest, givenTwoRootDevicesFromSameFamilyThenCanAccessPeerReturnsTrue) {
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

TEST_F(MultipleDevicesDisabledImplicitScalingTest, givenCanAccessPeerCalledTwiceThenCanAccessPeerReturnsSameValueEachTime) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    GFXCORE_FAMILY device0Family = device0->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    GFXCORE_FAMILY device1Family = device1->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    EXPECT_EQ(device0Family, device1Family);

    ze_bool_t canAccess = false;
    ze_result_t res = device0->canAccessPeer(device1->toHandle(), &canAccess);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(canAccess);

    res = device0->canAccessPeer(device1->toHandle(), &canAccess);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(canAccess);
}

TEST_F(MultipleDevicesTest, givenDeviceFailsAppendMemoryCopyThenCanAccessPeerReturnsFalse) {
    struct MockDeviceFail : public Mock<DeviceImp> {
        MockDeviceFail(L0::Device *device) : Mock(device->getNEODevice(), static_cast<NEO::ExecutionEnvironment *>(device->getExecEnvironment())) {
            this->driverHandle = device->getDriverHandle();
            this->commandList.appendMemoryCopyResult = ZE_RESULT_ERROR_UNKNOWN;
        }

        ze_result_t createCommandQueue(const ze_command_queue_desc_t *desc,
                                       ze_command_queue_handle_t *commandQueue) override {
            *commandQueue = &this->commandQueue;
            return ZE_RESULT_SUCCESS;
        }

        ze_result_t createCommandList(const ze_command_list_desc_t *desc,
                                      ze_command_list_handle_t *commandList) override {
            *commandList = &this->commandList;
            return ZE_RESULT_SUCCESS;
        }

        MockCommandList commandList;
        Mock<CommandQueue> commandQueue;
    };

    MockDeviceFail *device0 = new MockDeviceFail(driverHandle->devices[0]);
    L0::Device *device1 = driverHandle->devices[1];

    ze_bool_t canAccess = false;
    ze_result_t res = device0->canAccessPeer(device1->toHandle(), &canAccess);
    EXPECT_GT(device0->commandList.appendMemoryCopyCalled, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_FALSE(canAccess);
    delete device0;
}

TEST_F(MultipleDevicesTest, givenDeviceFailsExecuteCommandListThenCanAccessPeerReturnsFalse) {
    struct MockDeviceFail : public Mock<DeviceImp> {
        struct MockCommandQueueImp : public Mock<CommandQueue> {
            ze_result_t destroy() override {
                return ZE_RESULT_SUCCESS;
            }

            ze_result_t executeCommandLists(uint32_t numCommandLists,
                                            ze_command_list_handle_t *phCommandLists,
                                            ze_fence_handle_t hFence, bool performMigration) override { return ZE_RESULT_ERROR_UNKNOWN; }
        };

        MockDeviceFail(L0::Device *device) : Mock(device->getNEODevice(), static_cast<NEO::ExecutionEnvironment *>(device->getExecEnvironment())) {
            this->driverHandle = device->getDriverHandle();
        }

        ze_result_t createCommandQueue(const ze_command_queue_desc_t *desc,
                                       ze_command_queue_handle_t *commandQueue) override {
            *commandQueue = &this->commandQueue;
            return ZE_RESULT_SUCCESS;
        }

        ze_result_t createCommandList(const ze_command_list_desc_t *desc,
                                      ze_command_list_handle_t *commandList) override {
            *commandList = &this->commandList;
            return ZE_RESULT_SUCCESS;
        }

        MockCommandList commandList;
        MockCommandQueueImp commandQueue;
    };

    MockDeviceFail *device0 = new MockDeviceFail(driverHandle->devices[0]);
    L0::Device *device1 = driverHandle->devices[1];

    ze_bool_t canAccess = false;
    ze_result_t res = device0->canAccessPeer(device1->toHandle(), &canAccess);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_FALSE(canAccess);
    delete device0;
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
    L0::Device *subDevice00 = Device::fromHandle(subDevices0[0]);
    subDevice00->canAccessPeer(subDevices0[1], &canAccess);
    EXPECT_TRUE(canAccess);

    canAccess = false;
    L0::Device *subDevice10 = Device::fromHandle(subDevices1[0]);
    subDevice10->canAccessPeer(subDevices1[1], &canAccess);
    EXPECT_TRUE(canAccess);
}

TEST_F(MultipleDevicesDisabledImplicitScalingTest, givenTopologyForTwoSubdevicesWhenGettingApiSliceIdWithRootDeviceThenCorrectMappingIsUsedAndApiSliceIdsForSubdeviceReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    auto deviceImp0 = static_cast<DeviceImp *>(device0);
    auto hwInfo = device0->getHwInfo();

    ze_device_properties_t deviceProperties = {};
    deviceImp0->getProperties(&deviceProperties);

    NEO::TopologyMap map;
    TopologyMapping mapping;

    EXPECT_EQ(hwInfo.gtSystemInfo.SliceCount, deviceProperties.numSlices);

    mapping.sliceIndices.resize(hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i;
    }

    mapping.subsliceIndices.resize(hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.subsliceIndices[i] = i;
    }
    map[0] = mapping;

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i + 10;
    }
    map[1] = mapping;

    uint32_t sliceId = 0;
    uint32_t subsliceId = 0;
    auto ret = deviceImp0->toApiSliceId(map, sliceId, subsliceId, 0);

    EXPECT_TRUE(ret);
    EXPECT_EQ(0u, sliceId);

    sliceId = 10;
    ret = deviceImp0->toApiSliceId(map, sliceId, subsliceId, 1);

    EXPECT_TRUE(ret);
    EXPECT_EQ(hwInfo.gtSystemInfo.SliceCount + 0u, sliceId);
}

TEST_F(MultipleDevicesDisabledImplicitScalingTest, givenTopologyForSingleSubdeviceWhenGettingApiSliceIdWithRootDeviceThenCorrectApiSliceIdsForFirstSubDeviceIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    auto deviceImp0 = static_cast<DeviceImp *>(device0);
    auto hwInfo = device0->getHwInfo();

    ze_device_properties_t deviceProperties = {};
    deviceImp0->getProperties(&deviceProperties);

    NEO::TopologyMap map;
    TopologyMapping mapping;

    EXPECT_EQ(hwInfo.gtSystemInfo.SliceCount, deviceProperties.numSlices);

    mapping.sliceIndices.resize(hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i;
    }

    mapping.subsliceIndices.resize(hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.subsliceIndices[i] = i;
    }
    map[0] = mapping;

    uint32_t sliceId = 0;
    uint32_t subsliceId = 0;
    auto ret = deviceImp0->toApiSliceId(map, sliceId, subsliceId, 0);

    EXPECT_TRUE(ret);
    EXPECT_EQ(0u, sliceId);

    sliceId = 0;
    ret = deviceImp0->toApiSliceId(map, sliceId, subsliceId, 1);

    EXPECT_FALSE(ret);
}

TEST_F(MultipleDevicesTest, givenTopologyForTwoSubdevicesWhenGettingApiSliceIdWithSubDeviceThenCorrectSliceIdsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    auto hwInfo = device0->getHwInfo();

    uint32_t subDeviceCount = numSubDevices;
    std::vector<ze_device_handle_t> subDevices0(subDeviceCount);
    auto res = device0->getSubDevices(&subDeviceCount, subDevices0.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    L0::Device *subDevice0 = Device::fromHandle(subDevices0[0]);
    L0::Device *subDevice1 = Device::fromHandle(subDevices0[1]);

    L0::DeviceImp *subDeviceImp0 = static_cast<DeviceImp *>(subDevice0);
    L0::DeviceImp *subDeviceImp1 = static_cast<DeviceImp *>(subDevice1);

    NEO::TopologyMap map;
    TopologyMapping mapping;

    mapping.sliceIndices.resize(hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i;
    }

    mapping.subsliceIndices.resize(hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.subsliceIndices[i] = i;
    }

    map[0] = mapping;

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i + 10;
    }
    map[1] = mapping;

    uint32_t sliceId = 0;
    uint32_t subsliceId = 0;
    auto ret = subDeviceImp0->toApiSliceId(map, sliceId, subsliceId, 0);

    EXPECT_TRUE(ret);
    EXPECT_EQ(0u, sliceId);

    sliceId = 10;
    ret = subDeviceImp1->toApiSliceId(map, sliceId, subsliceId, 1);

    EXPECT_TRUE(ret);
    EXPECT_EQ(0u, sliceId);
}

TEST_F(MultipleDevicesTest, givenTopologyForTwoSubdevicesWhenGettingPhysicalSliceIdWithRootDeviceThenCorrectSliceIdAndDeviceIndexIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    auto deviceImp0 = static_cast<DeviceImp *>(device0);
    auto hwInfo = device0->getHwInfo();

    NEO::TopologyMap map;
    TopologyMapping mapping;

    mapping.sliceIndices.resize(hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i;
    }

    mapping.subsliceIndices.resize(hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.subsliceIndices[i] = i;
    }

    map[0] = mapping;

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i + 10;
    }
    map[1] = mapping;

    uint32_t sliceId = 0;
    uint32_t subsliceId = 0;
    uint32_t deviceIndex = 100;
    auto ret = deviceImp0->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

    EXPECT_TRUE(ret);
    EXPECT_EQ(0u, sliceId);
    EXPECT_EQ(0u, subsliceId);
    EXPECT_EQ(0u, deviceIndex);

    sliceId = hwInfo.gtSystemInfo.SliceCount;
    deviceIndex = 200;
    ret = deviceImp0->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

    EXPECT_TRUE(ret);
    EXPECT_EQ(10u, sliceId);
    EXPECT_EQ(0u, subsliceId);
    EXPECT_EQ(1u, deviceIndex);
}

TEST_F(MultipleDevicesTest, givenTopologyForTwoSubdevicesWhenGettingPhysicalSliceIdWithSubDeviceThenCorrectSliceIdAndDeviceIndexIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    auto hwInfo = device0->getHwInfo();

    uint32_t subDeviceCount = numSubDevices;
    std::vector<ze_device_handle_t> subDevices0(subDeviceCount);
    auto res = device0->getSubDevices(&subDeviceCount, subDevices0.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    L0::Device *subDevice0 = Device::fromHandle(subDevices0[0]);
    L0::Device *subDevice1 = Device::fromHandle(subDevices0[1]);

    L0::DeviceImp *subDeviceImp0 = static_cast<DeviceImp *>(subDevice0);
    L0::DeviceImp *subDeviceImp1 = static_cast<DeviceImp *>(subDevice1);

    NEO::TopologyMap map;
    TopologyMapping mapping;

    mapping.sliceIndices.resize(hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i + 5;
    }

    mapping.subsliceIndices.resize(hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.subsliceIndices[i] = i;
    }

    map[0] = mapping;

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i + 10;
    }
    map[1] = mapping;

    uint32_t sliceId = 0;
    uint32_t subsliceId = 1;
    uint32_t deviceIndex = 0;
    auto ret = subDeviceImp0->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

    EXPECT_TRUE(ret);
    EXPECT_EQ(5u, sliceId);
    EXPECT_EQ(1u, subsliceId);
    EXPECT_EQ(0u, deviceIndex);

    sliceId = 0;
    subsliceId = 1;
    deviceIndex = 100;
    ret = subDeviceImp1->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

    EXPECT_TRUE(ret);
    EXPECT_EQ(10u, sliceId);
    EXPECT_EQ(1u, subsliceId);
    EXPECT_EQ(1u, deviceIndex);
}

TEST_F(MultipleDevicesTest, givenInvalidApiSliceIdWhenGettingPhysicalSliceIdThenFalseIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    auto hwInfo = device0->getHwInfo();

    uint32_t subDeviceCount = numSubDevices;
    std::vector<ze_device_handle_t> subDevices0(subDeviceCount);
    auto res = device0->getSubDevices(&subDeviceCount, subDevices0.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    L0::Device *subDevice1 = Device::fromHandle(subDevices0[1]);
    L0::DeviceImp *subDeviceImp1 = static_cast<DeviceImp *>(subDevice1);

    NEO::TopologyMap map;
    TopologyMapping mapping;

    mapping.sliceIndices.resize(hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i + 5;
    }
    map[0] = mapping;

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i + 10;
    }
    map[1] = mapping;

    uint32_t sliceId = hwInfo.gtSystemInfo.SliceCount;
    uint32_t subsliceId = 0;
    uint32_t deviceIndex = 1;
    auto ret = subDeviceImp1->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

    EXPECT_FALSE(ret);
}

TEST_F(MultipleDevicesTest, givenTopologyMapForSubdeviceZeroWhenGettingPhysicalSliceIdForSubdeviceOneThenFalseIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    auto hwInfo = device0->getHwInfo();

    uint32_t subDeviceCount = numSubDevices;
    std::vector<ze_device_handle_t> subDevices0(subDeviceCount);
    auto res = device0->getSubDevices(&subDeviceCount, subDevices0.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    L0::Device *subDevice1 = Device::fromHandle(subDevices0[1]);
    L0::DeviceImp *subDeviceImp1 = static_cast<DeviceImp *>(subDevice1);

    NEO::TopologyMap map;
    TopologyMapping mapping;

    mapping.sliceIndices.resize(hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i + 5;
    }

    mapping.subsliceIndices.resize(hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.subsliceIndices[i] = i;
    }
    map[0] = mapping;

    uint32_t sliceId = 0;
    uint32_t subsliceId = 3;
    uint32_t deviceIndex = 1;
    auto ret = subDeviceImp1->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

    EXPECT_FALSE(ret);
}

using MultipleDevicesWithCustomHwInfoTest = Test<MultipleDevicesWithCustomHwInfo>;

TEST_F(MultipleDevicesWithCustomHwInfoTest, givenTopologyWhenMappingToAndFromApiAndPhysicalSliceIdThenIdsAreMatching) {
    L0::Device *device0 = driverHandle->devices[0];
    auto hwInfo = device0->getHwInfo();

    uint32_t subDeviceCount = numSubDevices;
    std::vector<ze_device_handle_t> subDevices0(subDeviceCount);
    auto res = device0->getSubDevices(&subDeviceCount, subDevices0.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    L0::Device *subDevice1 = Device::fromHandle(subDevices0[1]);
    L0::DeviceImp *subDeviceImp1 = static_cast<DeviceImp *>(subDevice1);
    L0::DeviceImp *device = static_cast<DeviceImp *>(device0);

    NEO::TopologyMap map;
    TopologyMapping mapping;

    mapping.sliceIndices.resize(hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i;
    }

    mapping.subsliceIndices.resize(hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount);
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.subsliceIndices[i] = i + 10;
    }
    map[0] = mapping;

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.sliceIndices[i] = i + 1;
    }
    map[1] = mapping;

    uint32_t deviceIndex = 0;

    ze_device_properties_t deviceProperties = {};
    device->getProperties(&deviceProperties);

    for (uint32_t i = 0; i < deviceProperties.numSlices; i++) {
        uint32_t sliceId = i;
        uint32_t subsliceId = deviceProperties.numSubslicesPerSlice / 2;
        auto ret = device->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

        EXPECT_TRUE(ret);
        if (i < sliceCount) {
            EXPECT_EQ(i, sliceId);
            EXPECT_EQ(0u, deviceIndex);
        } else {
            EXPECT_EQ(i + 1 - (deviceProperties.numSlices / subDeviceCount), sliceId);
            EXPECT_EQ(1u, deviceIndex);
        }

        ret = device->toApiSliceId(map, sliceId, subsliceId, deviceIndex);

        EXPECT_TRUE(ret);
        EXPECT_EQ(i, sliceId);
        EXPECT_EQ(deviceProperties.numSubslicesPerSlice / 2, subsliceId);
    }

    subDeviceImp1->getProperties(&deviceProperties);

    for (uint32_t i = 0; i < deviceProperties.numSlices; i++) {
        uint32_t sliceId = i;
        uint32_t subsliceId = deviceProperties.numSubslicesPerSlice - 1;
        auto ret = subDeviceImp1->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

        EXPECT_TRUE(ret);

        EXPECT_EQ(i + 1, sliceId);
        EXPECT_EQ(1u, deviceIndex);
        ret = subDeviceImp1->toApiSliceId(map, sliceId, subsliceId, deviceIndex);

        EXPECT_TRUE(ret);
        EXPECT_EQ(i, sliceId);
        EXPECT_EQ(deviceProperties.numSubslicesPerSlice - 1, subsliceId);
    }
}

TEST_F(MultipleDevicesWithCustomHwInfoTest, givenSingleSliceTopologyWhenMappingToAndFromApiAndPhysicalSubSliceIdThenIdsAreMatching) {
    L0::Device *device0 = driverHandle->devices[0];

    uint32_t subDeviceCount = numSubDevices;
    std::vector<ze_device_handle_t> subDevices0(subDeviceCount);
    auto res = device0->getSubDevices(&subDeviceCount, subDevices0.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    L0::Device *subDevice1 = Device::fromHandle(subDevices0[1]);
    L0::DeviceImp *subDeviceImp1 = static_cast<DeviceImp *>(subDevice1);
    L0::DeviceImp *device = static_cast<DeviceImp *>(device0);

    NEO::TopologyMap map;
    TopologyMapping mapping;

    mapping.sliceIndices.resize(1);
    mapping.sliceIndices[0] = 1;

    mapping.subsliceIndices.resize(hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount);

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.subsliceIndices[i] = i + 10;
    }
    map[0] = mapping;

    mapping.sliceIndices.resize(1);
    mapping.sliceIndices[0] = 0;

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount / hwInfo.gtSystemInfo.SliceCount; i++) {
        mapping.subsliceIndices[i] = i + 20;
    }
    map[1] = mapping;

    uint32_t deviceIndex = 0;

    for (uint32_t i = 0; i < 2; i++) {
        uint32_t sliceId = i;
        uint32_t subsliceId = 2;
        auto ret = device->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

        EXPECT_TRUE(ret);
        if (i < 1) {
            EXPECT_EQ(1u, sliceId);
            EXPECT_EQ(12u, subsliceId);
            EXPECT_EQ(0u, deviceIndex);
        } else {
            EXPECT_EQ(0u, sliceId);
            EXPECT_EQ(22u, subsliceId);
            EXPECT_EQ(1u, deviceIndex);
        }

        ret = device->toApiSliceId(map, sliceId, subsliceId, deviceIndex);

        EXPECT_TRUE(ret);
        EXPECT_EQ(i, sliceId);
        EXPECT_EQ(2u, subsliceId);
    }

    // subdevice 1
    {
        uint32_t sliceId = 0;
        uint32_t subsliceId = 1;
        auto ret = subDeviceImp1->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

        EXPECT_TRUE(ret);

        EXPECT_EQ(0u, sliceId);
        EXPECT_EQ(21u, subsliceId);
        EXPECT_EQ(1u, deviceIndex);
        ret = subDeviceImp1->toApiSliceId(map, sliceId, subsliceId, deviceIndex);

        EXPECT_TRUE(ret);
        EXPECT_EQ(0u, sliceId);
        EXPECT_EQ(1u, subsliceId);
    }
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

TEST_F(DeviceTest, givenNoActiveSourceLevelDebuggerWhenGetIsCalledThenNullptrIsReturned) {
    EXPECT_EQ(nullptr, device->getSourceLevelDebugger());
}

TEST_F(DeviceTest, givenNoL0DebuggerWhenGettingL0DebuggerThenNullptrReturned) {
    EXPECT_EQ(nullptr, device->getL0Debugger());
}

TEST_F(DeviceTest, givenValidDeviceWhenCallingReleaseResourcesThenResourcesReleased) {
    auto deviceImp = static_cast<DeviceImp *>(device);
    EXPECT_FALSE(deviceImp->resourcesReleased);
    EXPECT_FALSE(nullptr == deviceImp->getNEODevice());
    deviceImp->releaseResources();
    EXPECT_TRUE(deviceImp->resourcesReleased);
    EXPECT_TRUE(nullptr == deviceImp->getNEODevice());
    EXPECT_TRUE(nullptr == deviceImp->pageFaultCommandList);
    EXPECT_TRUE(nullptr == deviceImp->getDebugSurface());
    deviceImp->releaseResources();
    EXPECT_TRUE(deviceImp->resourcesReleased);
}

HWTEST_F(DeviceTest, givenCooperativeDispatchSupportedWhenQueryingPropertiesFlagsThenCooperativeKernelsAreSupported) {
    struct MockHwHelper : NEO::HwHelperHw<FamilyType> {
        bool isCooperativeDispatchSupported(const EngineGroupType engineGroupType, const HardwareInfo &hwInfo) const override {
            return isCooperativeDispatchSupportedValue;
        }
        bool isCooperativeDispatchSupportedValue = true;
    };

    const uint32_t rootDeviceIndex = 0u;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo,
                                                                                              rootDeviceIndex);
    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    MockHwHelper hwHelper{};
    VariableBackup<HwHelper *> hwHelperFactoryBackup{&NEO::hwHelperFactory[static_cast<size_t>(hwInfo.platform.eRenderCoreFamily)]};
    hwHelperFactoryBackup = &hwHelper;

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    NEO::EngineGroupType engineGroupTypes[] = {NEO::EngineGroupType::RenderCompute, NEO::EngineGroupType::Compute};
    for (auto isCooperativeDispatchSupported : ::testing::Bool()) {
        hwHelper.isCooperativeDispatchSupportedValue = isCooperativeDispatchSupported;

        std::vector<ze_command_queue_group_properties_t> properties(count);
        res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        for (auto engineGroupType : engineGroupTypes) {
            auto groupOrdinal = static_cast<size_t>(engineGroupType);
            if (groupOrdinal >= count) {
                continue;
            }
            auto actualValue = NEO::isValueSet(properties[groupOrdinal].flags, ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(actualValue);
        }
    }
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

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};

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

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    device->getProperties(&deviceProps);
    EXPECT_EQ(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED);
}

TEST(zeDevice, givenValidImagePropertiesStructWhenGettingImagePropertiesThenSuccessIsReturned) {
    Mock<Device> device;
    ze_result_t result;
    ze_device_image_properties_t imageProperties;

    result = zeDeviceGetImageProperties(device.toHandle(), &imageProperties);
    EXPECT_EQ(1u, device.getDeviceImagePropertiesCalled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeDevice, givenImagesSupportedWhenGettingImagePropertiesThenValidValuesAreReturned) {
    ze_result_t errorValue;
    NEO::MockCompilerEnableGuard mock(true);
    DriverHandleImp driverHandle{};
    NEO::MockDevice *neoDevice = (NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(&driverHandle, neoDevice, false, &errorValue));
    DeviceInfo &deviceInfo = neoDevice->deviceInfo;

    deviceInfo.imageSupport = true;
    deviceInfo.image2DMaxWidth = 1;
    deviceInfo.image2DMaxHeight = 2;
    deviceInfo.image3DMaxDepth = 3;
    deviceInfo.imageMaxBufferSize = 4;
    deviceInfo.imageMaxArraySize = 5;
    deviceInfo.maxSamplers = 6;
    deviceInfo.maxReadImageArgs = 7;
    deviceInfo.maxWriteImageArgs = 8;

    ze_device_image_properties_t properties{};
    ze_result_t result = zeDeviceGetImageProperties(device->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(deviceInfo.image2DMaxWidth, static_cast<uint64_t>(properties.maxImageDims1D));
    EXPECT_EQ(deviceInfo.image2DMaxHeight, static_cast<uint64_t>(properties.maxImageDims2D));
    EXPECT_EQ(deviceInfo.image3DMaxDepth, static_cast<uint64_t>(properties.maxImageDims3D));
    EXPECT_EQ(deviceInfo.imageMaxBufferSize, properties.maxImageBufferSize);
    EXPECT_EQ(deviceInfo.imageMaxArraySize, static_cast<uint64_t>(properties.maxImageArraySlices));
    EXPECT_EQ(deviceInfo.maxSamplers, properties.maxSamplers);
    EXPECT_EQ(deviceInfo.maxReadImageArgs, properties.maxReadImageArgs);
    EXPECT_EQ(deviceInfo.maxWriteImageArgs, properties.maxWriteImageArgs);
}

TEST(zeDevice, givenNoImagesSupportedWhenGettingImagePropertiesThenZeroValuesAreReturned) {
    ze_result_t errorValue;
    NEO::MockCompilerEnableGuard mock(true);
    DriverHandleImp driverHandle{};
    NEO::MockDevice *neoDevice = (NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(&driverHandle, neoDevice, false, &errorValue));
    DeviceInfo &deviceInfo = neoDevice->deviceInfo;

    neoDevice->deviceInfo.imageSupport = false;
    deviceInfo.image2DMaxWidth = 1;
    deviceInfo.image2DMaxHeight = 2;
    deviceInfo.image3DMaxDepth = 3;
    deviceInfo.imageMaxBufferSize = 4;
    deviceInfo.imageMaxArraySize = 5;
    deviceInfo.maxSamplers = 6;
    deviceInfo.maxReadImageArgs = 7;
    deviceInfo.maxWriteImageArgs = 8;

    ze_device_image_properties_t properties{};
    ze_result_t result = zeDeviceGetImageProperties(device->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, properties.maxImageDims1D);
    EXPECT_EQ(0u, properties.maxImageDims2D);
    EXPECT_EQ(0u, properties.maxImageDims3D);
    EXPECT_EQ(0u, properties.maxImageBufferSize);
    EXPECT_EQ(0u, properties.maxImageArraySlices);
    EXPECT_EQ(0u, properties.maxSamplers);
    EXPECT_EQ(0u, properties.maxReadImageArgs);
    EXPECT_EQ(0u, properties.maxWriteImageArgs);
}

class MockCacheReservation : public CacheReservation {
  public:
    ~MockCacheReservation() override = default;
    MockCacheReservation(L0::Device &device, bool initialize) : isInitialized(initialize){};

    bool reserveCache(size_t cacheLevel, size_t cacheReservationSize) override {
        receivedCacheLevel = cacheLevel;
        return isInitialized;
    }
    bool setCacheAdvice(void *ptr, size_t regionSize, ze_cache_ext_region_t cacheRegion) override {
        receivedCacheRegion = cacheRegion;
        return isInitialized;
    }
    size_t getMaxCacheReservationSize() override {
        return maxCacheReservationSize;
    }

    static size_t maxCacheReservationSize;

    bool isInitialized = false;
    size_t receivedCacheLevel = 3;
    ze_cache_ext_region_t receivedCacheRegion = ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_REGION_DEFAULT;
};

size_t MockCacheReservation::maxCacheReservationSize = 1024;

using zeDeviceCacheReservationTest = DeviceTest;

TEST_F(zeDeviceCacheReservationTest, givenDeviceCacheExtendedDescriptorWhenGetCachePropertiesCalledWithIncorrectStructureTypeThenReturnErrorUnsupportedEnumeration) {
    ze_cache_reservation_ext_desc_t cacheReservationExtDesc = {};

    ze_device_cache_properties_t deviceCacheProperties = {};
    deviceCacheProperties.pNext = &cacheReservationExtDesc;

    uint32_t count = 1;
    ze_result_t res = device->getCacheProperties(&count, &deviceCacheProperties);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, res);
}

TEST_F(zeDeviceCacheReservationTest, givenGreaterThanOneCountOfDeviceCachePropertiesWhenGetCachePropertiesIsCalledThenSetCountToOne) {
    static_cast<DeviceImp *>(device)->cacheReservation.reset(new MockCacheReservation(*device, true));
    ze_device_cache_properties_t deviceCacheProperties = {};

    uint32_t count = 10;
    ze_result_t res = device->getCacheProperties(&count, &deviceCacheProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, 1u);
}

TEST_F(zeDeviceCacheReservationTest, givenDeviceCacheExtendedDescriptorWhenGetCachePropertiesCalledOnDeviceWithNoSupportForCacheReservationThenReturnZeroMaxCacheReservationSize) {
    VariableBackup<size_t> maxCacheReservationSizeBackup{&MockCacheReservation::maxCacheReservationSize, 0};
    static_cast<DeviceImp *>(device)->cacheReservation.reset(new MockCacheReservation(*device, true));

    ze_cache_reservation_ext_desc_t cacheReservationExtDesc = {};
    cacheReservationExtDesc.stype = ZE_STRUCTURE_TYPE_CACHE_RESERVATION_EXT_DESC;

    ze_device_cache_properties_t deviceCacheProperties = {};
    deviceCacheProperties.pNext = &cacheReservationExtDesc;

    uint32_t count = 1;
    ze_result_t res = device->getCacheProperties(&count, &deviceCacheProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(0u, cacheReservationExtDesc.maxCacheReservationSize);
}

TEST_F(zeDeviceCacheReservationTest, givenDeviceCacheExtendedDescriptorWhenGetCachePropertiesCalledOnDeviceWithSupportForCacheReservationThenReturnNonZeroMaxCacheReservationSize) {
    static_cast<DeviceImp *>(device)->cacheReservation.reset(new MockCacheReservation(*device, true));

    ze_cache_reservation_ext_desc_t cacheReservationExtDesc = {};
    cacheReservationExtDesc.stype = ZE_STRUCTURE_TYPE_CACHE_RESERVATION_EXT_DESC;

    ze_device_cache_properties_t deviceCacheProperties = {};
    deviceCacheProperties.pNext = &cacheReservationExtDesc;

    uint32_t count = 1;
    ze_result_t res = device->getCacheProperties(&count, &deviceCacheProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_NE(0u, cacheReservationExtDesc.maxCacheReservationSize);
}

TEST_F(zeDeviceCacheReservationTest, WhenCallingZeDeviceReserveCacheExtOnDeviceWithNoSupportForCacheReservationThenReturnErrorUnsupportedFeature) {
    VariableBackup<size_t> maxCacheReservationSizeBackup{&MockCacheReservation::maxCacheReservationSize, 0};
    static_cast<DeviceImp *>(device)->cacheReservation.reset(new MockCacheReservation(*device, true));

    size_t cacheLevel = 3;
    size_t cacheReservationSize = 1024;

    auto result = zeDeviceReserveCacheExt(device->toHandle(), cacheLevel, cacheReservationSize);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST_F(zeDeviceCacheReservationTest, WhenCallingZeDeviceReserveCacheExtWithCacheLevel0ThenDriverShouldDefaultToCacheLevel3) {
    auto mockCacheReservation = new MockCacheReservation(*device, true);
    static_cast<DeviceImp *>(device)->cacheReservation.reset(mockCacheReservation);

    size_t cacheLevel = 0;
    size_t cacheReservationSize = 1024;

    auto result = zeDeviceReserveCacheExt(device->toHandle(), cacheLevel, cacheReservationSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(3u, mockCacheReservation->receivedCacheLevel);
}

TEST_F(zeDeviceCacheReservationTest, WhenCallingZeDeviceReserveCacheExtFailsToReserveCacheOnDeviceThenReturnErrorUninitialized) {
    size_t cacheLevel = 3;
    size_t cacheReservationSize = 1024;

    for (auto initialize : {false, true}) {
        auto mockCacheReservation = new MockCacheReservation(*device, initialize);
        static_cast<DeviceImp *>(device)->cacheReservation.reset(mockCacheReservation);

        auto result = zeDeviceReserveCacheExt(device->toHandle(), cacheLevel, cacheReservationSize);

        if (initialize) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        } else {
            EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
        }

        EXPECT_EQ(3u, mockCacheReservation->receivedCacheLevel);
    }
}

TEST_F(zeDeviceCacheReservationTest, WhenCallingZeDeviceSetCacheAdviceExtWithDefaultCacheRegionThenDriverShouldDefaultToNonReservedRegion) {
    auto mockCacheReservation = new MockCacheReservation(*device, true);
    static_cast<DeviceImp *>(device)->cacheReservation.reset(mockCacheReservation);

    void *ptr = reinterpret_cast<void *>(0x123456789);
    size_t regionSize = 512;
    ze_cache_ext_region_t cacheRegion = ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_REGION_DEFAULT;

    auto result = zeDeviceSetCacheAdviceExt(device->toHandle(), ptr, regionSize, cacheRegion);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_NON_RESERVED_REGION, mockCacheReservation->receivedCacheRegion);
}

TEST_F(zeDeviceCacheReservationTest, WhenCallingZeDeviceSetCacheAdviceExtOnDeviceWithNoSupportForCacheReservationThenReturnErrorUnsupportedFeature) {
    VariableBackup<size_t> maxCacheReservationSizeBackup{&MockCacheReservation::maxCacheReservationSize, 0};
    static_cast<DeviceImp *>(device)->cacheReservation.reset(new MockCacheReservation(*device, true));

    void *ptr = reinterpret_cast<void *>(0x123456789);
    size_t regionSize = 512;
    ze_cache_ext_region_t cacheRegion = ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_REGION_DEFAULT;

    auto result = zeDeviceSetCacheAdviceExt(device->toHandle(), ptr, regionSize, cacheRegion);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST_F(zeDeviceCacheReservationTest, WhenCallingZeDeviceSetCacheAdviceExtFailsToSetCacheRegionThenReturnErrorUnitialized) {
    void *ptr = reinterpret_cast<void *>(0x123456789);
    size_t regionSize = 512;
    ze_cache_ext_region_t cacheRegion = ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_RESERVE_REGION;

    for (auto initialize : {false, true}) {
        auto mockCacheReservation = new MockCacheReservation(*device, initialize);
        static_cast<DeviceImp *>(device)->cacheReservation.reset(mockCacheReservation);

        auto result = zeDeviceSetCacheAdviceExt(device->toHandle(), ptr, regionSize, cacheRegion);

        if (initialize) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        } else {
            EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
        }

        EXPECT_EQ(ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_RESERVE_REGION, mockCacheReservation->receivedCacheRegion);
    }
}

using zeDeviceSystemBarrierTest = DeviceTest;

TEST_F(zeDeviceSystemBarrierTest, whenCallingSystemBarrierThenReturnErrorUnsupportedFeature) {

    auto result = static_cast<DeviceImp *>(device)->systemBarrier();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

template <bool osLocalMemory, bool apiSupport, int32_t enablePartitionWalker, int32_t enableImplicitScaling>
struct MultiSubDeviceFixture : public DeviceFixture {
    void SetUp() {
        DebugManager.flags.CreateMultipleSubDevices.set(2);
        DebugManager.flags.EnableWalkerPartition.set(enablePartitionWalker);
        DebugManager.flags.EnableImplicitScaling.set(enableImplicitScaling);
        osLocalMemoryBackup = std::make_unique<VariableBackup<bool>>(&NEO::OSInterface::osEnableLocalMemory, osLocalMemory);
        apiSupportBackup = std::make_unique<VariableBackup<bool>>(&NEO::ImplicitScaling::apiSupport, apiSupport);

        DeviceFixture::SetUp();

        deviceImp = reinterpret_cast<L0::DeviceImp *>(device);
        subDevice = neoDevice->getSubDevice(0);
    }

    L0::DeviceImp *deviceImp = nullptr;
    NEO::Device *subDevice = nullptr;
    DebugManagerStateRestore restorer;
    std::unique_ptr<VariableBackup<bool>> osLocalMemoryBackup;
    std::unique_ptr<VariableBackup<bool>> apiSupportBackup;
};

using MultiSubDeviceTest = Test<MultiSubDeviceFixture<true, true, -1, -1>>;
TEST_F(MultiSubDeviceTest, GivenApiSupportAndLocalMemoryEnabledWhenDeviceContainsSubDevicesThenItIsImplicitScalingCapable) {
    if (NEO::HwHelper::get(neoDevice->getHardwareInfo().platform.eRenderCoreFamily).platformSupportsImplicitScaling(neoDevice->getHardwareInfo())) {
        EXPECT_TRUE(device->isImplicitScalingCapable());
        EXPECT_EQ(neoDevice, deviceImp->getActiveDevice());
    } else {
        EXPECT_FALSE(device->isImplicitScalingCapable());
        EXPECT_EQ(subDevice, deviceImp->getActiveDevice());
    }
}

using MultiSubDeviceTestNoApi = Test<MultiSubDeviceFixture<true, false, -1, -1>>;
TEST_F(MultiSubDeviceTestNoApi, GivenNoApiSupportAndLocalMemoryEnabledWhenDeviceContainsSubDevicesThenItIsNotImplicitScalingCapable) {
    EXPECT_FALSE(device->isImplicitScalingCapable());
    EXPECT_EQ(subDevice, deviceImp->getActiveDevice());
}

using MultiSubDeviceTestNoLocalMemory = Test<MultiSubDeviceFixture<false, true, -1, -1>>;
TEST_F(MultiSubDeviceTestNoLocalMemory, GivenApiSupportAndLocalMemoryDisabledWhenDeviceContainsSubDevicesThenItIsNotImplicitScalingCapable) {
    EXPECT_FALSE(device->isImplicitScalingCapable());
    EXPECT_EQ(subDevice, deviceImp->getActiveDevice());
}

using MultiSubDeviceTestNoApiForceOn = Test<MultiSubDeviceFixture<true, false, 1, -1>>;
TEST_F(MultiSubDeviceTestNoApiForceOn, GivenNoApiSupportAndLocalMemoryEnabledWhenForcedImplicitScalingThenItIsImplicitScalingCapable) {
    EXPECT_TRUE(device->isImplicitScalingCapable());
    EXPECT_EQ(neoDevice, deviceImp->getActiveDevice());
}

using MultiSubDeviceEnabledImplicitScalingTest = Test<MultiSubDeviceFixture<true, true, -1, 1>>;
TEST_F(MultiSubDeviceEnabledImplicitScalingTest, GivenApiSupportAndLocalMemoryEnabledWhenDeviceContainsSubDevicesAndSupportsImplicitScalingThenItIsImplicitScalingCapable) {
    EXPECT_TRUE(device->isImplicitScalingCapable());
    EXPECT_EQ(neoDevice, deviceImp->getActiveDevice());
}

TEST_F(MultiSubDeviceEnabledImplicitScalingTest, GivenEnabledImplicitScalingWhenDeviceReturnsLowPriorityCsrThenItIsDefaultCsr) {
    auto &defaultEngine = deviceImp->getActiveDevice()->getDefaultEngine();

    NEO::CommandStreamReceiver *csr = nullptr;
    auto ret = deviceImp->getCsrForLowPriority(&csr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(defaultEngine.commandStreamReceiver, csr);
}

using DeviceSimpleTests = Test<DeviceFixture>;

static_assert(ZE_MEMORY_ACCESS_CAP_FLAG_RW == UNIFIED_SHARED_MEMORY_ACCESS, "Flags value difference");
static_assert(ZE_MEMORY_ACCESS_CAP_FLAG_ATOMIC == UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS, "Flags value difference");
static_assert(ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT == UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS, "Flags value difference");
static_assert(ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT_ATOMIC == UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS, "Flags value difference");

TEST_F(DeviceSimpleTests, returnsGPUType) {
    ze_device_properties_t properties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    device->getProperties(&properties);
    EXPECT_EQ(ZE_DEVICE_TYPE_GPU, properties.type);
}

TEST_F(DeviceSimpleTests, givenNoSubDevicesThenNonZeroNumSlicesAreReturned) {
    uint32_t subDeviceCount = 0;
    device->getSubDevices(&subDeviceCount, nullptr);
    EXPECT_EQ(0u, device->getNEODevice()->getNumGenericSubDevices());
    EXPECT_EQ(device->getNEODevice()->getNumSubDevices(), subDeviceCount);

    ze_device_properties_t properties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    device->getProperties(&properties);
    EXPECT_NE(0u, properties.numSlices);
}

TEST_F(DeviceSimpleTests, givenDeviceThenValidUuidIsReturned) {
    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    device->getProperties(&deviceProps);
    uint32_t rootDeviceIndex = neoDevice->getRootDeviceIndex();

    EXPECT_EQ(memcmp(&deviceProps.vendorId, deviceProps.uuid.id, sizeof(uint32_t)), 0);
    EXPECT_EQ(memcmp(&deviceProps.deviceId, deviceProps.uuid.id + sizeof(uint32_t), sizeof(uint32_t)), 0);
    EXPECT_EQ(memcmp(&rootDeviceIndex, deviceProps.uuid.id + (2 * sizeof(uint32_t)), sizeof(uint32_t)), 0);
}

TEST_F(DeviceSimpleTests, WhenGettingKernelPropertiesThenSuccessIsReturned) {
    ze_device_module_properties_t kernelProps = {};

    auto result = device->getKernelProperties(&kernelProps);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(DeviceSimpleTests, WhenGettingMemoryPropertiesThenSuccessIsReturned) {
    ze_device_memory_properties_t properties = {};
    uint32_t count = 1;
    auto result = device->getMemoryProperties(&count, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(DeviceSimpleTests, givenDeviceWhenAskingForSubGroupSizesThenReturnCorrectValues) {
    ze_device_compute_properties_t properties;
    auto result = device->getComputeProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto maxSubGroupsFromDeviceInfo = device->getDeviceInfo().maxSubGroups;

    EXPECT_NE(0u, maxSubGroupsFromDeviceInfo.size());
    EXPECT_EQ(maxSubGroupsFromDeviceInfo.size(), properties.numSubGroupSizes);

    for (uint32_t i = 0; i < properties.numSubGroupSizes; i++) {
        EXPECT_EQ(maxSubGroupsFromDeviceInfo[i], properties.subGroupSizes[i]);
    }
}

using IsAtMostProductDG2 = IsAtMostProduct<IGFX_DG2>;

HWTEST2_F(DeviceSimpleTests, WhenCreatingImageThenSuccessIsReturned, IsAtMostProductDG2) {
    ze_image_handle_t image = {};
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    auto result = device->createImage(&desc, &image);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, image);

    Image::fromHandle(image)->destroy();
}

TEST_F(DeviceSimpleTests, WhenGettingMaxHwThreadsThenCorrectValueIsReturned) {
    auto hwInfo = neoDevice->getHardwareInfo();

    uint32_t threadsPerEU = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount) +
                            hwInfo.capabilityTable.extraQuantityThreadsPerEU;
    uint32_t value = device->getMaxNumHwThreads();

    uint32_t expected = hwInfo.gtSystemInfo.EUCount * threadsPerEU;
    EXPECT_EQ(expected, value);
}

TEST_F(DeviceSimpleTests, WhenCreatingCommandListThenSuccessIsReturned) {
    ze_command_list_handle_t commandList = {};
    ze_command_list_desc_t desc = {};

    auto result = device->createCommandList(&desc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, commandList);

    CommandList::fromHandle(commandList)->destroy();
}

TEST_F(DeviceSimpleTests, WhenCreatingCommandQueueThenSuccessIsReturned) {
    ze_command_queue_handle_t commandQueue = {};
    ze_command_queue_desc_t desc = {};

    auto result = device->createCommandQueue(&desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, commandQueue);

    auto queue = L0::CommandQueue::fromHandle(commandQueue);
    queue->destroy();
}

TEST_F(DeviceSimpleTests, givenValidDeviceThenValidCoreDeviceIsRetrievedWithGetSpecializedDevice) {
    auto specializedDevice = neoDevice->getSpecializedDevice<Device>();
    EXPECT_EQ(device, specializedDevice);
}

} // namespace ult
} // namespace L0
