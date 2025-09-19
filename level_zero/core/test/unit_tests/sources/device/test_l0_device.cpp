/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/os_interface/linux/hw_device_id.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/execution_environment_helper.h"
#include "shared/test/common/helpers/mock_product_helper_hw.h"
#include "shared/test/common/helpers/raii_gfx_core_helper.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_compiler_product_helper.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_info.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/destructor_counted.h"

#include "level_zero/core/source/cache/cache_reservation.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/driver/extension_function_address.h"
#include "level_zero/core/source/driver/host_pointer_manager.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/fabric/fabric.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist.h"
#include "level_zero/core/source/rtas/rtas.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include "level_zero/driver_experimental/zex_common.h"

#include "common/StateSaveAreaHeader.h"
#include "gtest/gtest.h"

#include <memory>

namespace NEO {
extern GfxCoreHelperCreateFunctionType gfxCoreHelperFactory[IGFX_MAX_CORE];
} // namespace NEO

namespace L0 {
namespace ult {

TEST(L0DeviceTest, givenNonExistingFclWhenCreatingDeviceThenCompilerInterfaceIsCreated) {

    VariableBackup<const char *> frontEndDllName(&Os::frontEndDllName);
    Os::frontEndDllName = "_fake_fcl1_so";

    ze_result_t returnValue = ZE_RESULT_SUCCESS;

    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    ASSERT_NE(nullptr, neoDevice);

    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    auto compilerInterface = device->getNEODevice()->getCompilerInterface();
    ASSERT_NE(nullptr, compilerInterface);
}

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
    NEO::debugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(1);

    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);
    auto deviceImp = static_cast<DeviceImp *>(device.get());
    ASSERT_NE(nullptr, deviceImp->pageFaultCommandList);

    ASSERT_NE(nullptr, CommandList::whiteboxCast(deviceImp->pageFaultCommandList)->cmdQImmediate);
    EXPECT_NE(nullptr, static_cast<CommandQueueImp *>(CommandList::whiteboxCast(deviceImp->pageFaultCommandList)->cmdQImmediate)->getCsr());
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS, static_cast<CommandQueueImp *>(CommandList::whiteboxCast(deviceImp->pageFaultCommandList)->cmdQImmediate)->getCommandQueueMode());
}

TEST(L0DeviceTest, GivenDualStorageSharedMemoryAndImplicitScalingThenPageFaultCmdListImmediateWithInitializedCmdQIsCreatedAgainstSubDeviceZero) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(1);
    debugManager.flags.EnableImplicitScaling.set(1u);
    debugManager.flags.CreateMultipleSubDevices.set(2u);

    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);
    auto deviceImp = static_cast<DeviceImp *>(device.get());
    ASSERT_NE(nullptr, deviceImp->pageFaultCommandList);

    ASSERT_NE(nullptr, CommandList::whiteboxCast(deviceImp->pageFaultCommandList)->cmdQImmediate);
    EXPECT_NE(nullptr, static_cast<CommandQueueImp *>(CommandList::whiteboxCast(deviceImp->pageFaultCommandList)->cmdQImmediate)->getCsr());
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS, static_cast<CommandQueueImp *>(CommandList::whiteboxCast(deviceImp->pageFaultCommandList)->cmdQImmediate)->getCommandQueueMode());
    EXPECT_EQ(CommandList::whiteboxCast(deviceImp->pageFaultCommandList)->device, deviceImp->subDevices[0]);
}

TEST(L0DeviceTest, givenMultipleMaskedSubDevicesWhenCreatingL0DeviceThenDontAddDisabledNeoDevies) {
    constexpr uint32_t numSubDevices = 3;
    constexpr uint32_t numMaskedSubDevices = 2;

    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
    debugManager.flags.ZE_AFFINITY_MASK.set("0.0,0.2");

    auto executionEnvironment = std::make_unique<NEO::ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
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

using DeviceCopyEngineTests = ::testing::Test;

HWTEST2_F(DeviceCopyEngineTests, givenRootOrSubDeviceWhenAskingForCopyOrdinalThenReturnCorrectId, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    debugManager.flags.EnableImplicitScaling.set(1);

    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo = 0b111;

    auto executionEnvironment = std::make_unique<NEO::ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->parseAffinityMask();
    auto deviceFactory = std::make_unique<NEO::UltDeviceFactory>(1, 2, *executionEnvironment.release());
    auto rootDevice = deviceFactory->rootDevices[0];
    EXPECT_NE(nullptr, rootDevice);
    EXPECT_EQ(2u, rootDevice->getNumSubDevices());

    auto driverHandle = std::make_unique<DriverHandleImp>();

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), rootDevice, false, &returnValue));
    ASSERT_NE(nullptr, device);

    auto deviceImp = static_cast<DeviceImp *>(device.get());
    ASSERT_EQ(2u, deviceImp->numSubDevices);

    EXPECT_EQ(static_cast<uint32_t>(device->getNEODevice()->getRegularEngineGroups().size()), deviceImp->getCopyEngineOrdinal());

    auto &subDeviceEngines = deviceImp->subDevices[0]->getNEODevice()->getRegularEngineGroups();

    uint32_t subDeviceCopyEngineId = std::numeric_limits<uint32_t>::max();
    for (uint32_t i = 0; i < subDeviceEngines.size(); i++) {
        if (subDeviceEngines[i].engineGroupType == EngineGroupType::copy) {
            subDeviceCopyEngineId = i;
            break;
        }
    }
    EXPECT_NE(std::numeric_limits<uint32_t>::max(), subDeviceCopyEngineId);

    EXPECT_EQ(subDeviceCopyEngineId, static_cast<DeviceImp *>(deviceImp->subDevices[0])->getCopyEngineOrdinal());
    EXPECT_EQ(subDeviceCopyEngineId, static_cast<DeviceImp *>(deviceImp->subDevices[1])->getCopyEngineOrdinal());
}

TEST(L0DeviceTest, givenMidThreadPreemptionWhenCreatingDeviceThenSipKernelIsInitialized) {

    VariableBackup<bool> mockSipCalled(&NEO::MockSipData::called, false);
    VariableBackup<NEO::SipKernelType> mockSipCalledType(&NEO::MockSipData::calledType, NEO::SipKernelType::count);
    VariableBackup<bool> backupSipInitType(&MockSipData::useMockSip, true);

    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.capabilityTable.defaultPreemptionMode = NEO::PreemptionMode::MidThread;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    EXPECT_EQ(NEO::SipKernelType::csr, NEO::MockSipData::calledType);
    EXPECT_TRUE(NEO::MockSipData::called);
}

TEST(L0DeviceTest, givenDebuggerEnabledButIGCNotReturnsSSAHThenSSAHIsNotCopied) {

    auto executionEnvironment = new NEO::ExecutionEnvironment();
    auto mockBuiltIns = new MockBuiltins();
    mockBuiltIns->stateSaveAreaHeader.clear();

    executionEnvironment->prepareRootDeviceEnvironments(1);
    MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    auto neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->enableProgramDebugging = NEO::DebuggingMode::online;

    driverHandle->initialize(std::move(devices));
    auto sipType = SipKernel::getSipKernelType(*neoDevice);
    auto &stateSaveAreaHeader = neoDevice->getBuiltIns()->getSipKernel(sipType, *neoDevice).getStateSaveAreaHeader();
    EXPECT_EQ(static_cast<size_t>(0), stateSaveAreaHeader.size());
}

TEST(L0DeviceTest, givenDisabledPreemptionWhenCreatingDeviceThenSipKernelIsNotInitialized) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    VariableBackup<bool> mockSipCalled(&NEO::MockSipData::called, false);
    VariableBackup<NEO::SipKernelType> mockSipCalledType(&NEO::MockSipData::calledType, NEO::SipKernelType::count);
    VariableBackup<bool> backupSipInitType(&MockSipData::useMockSip, true);

    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.capabilityTable.defaultPreemptionMode = NEO::PreemptionMode::Disabled;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    EXPECT_EQ(NEO::SipKernelType::count, NEO::MockSipData::calledType);
    EXPECT_FALSE(NEO::MockSipData::called);

    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);

    EXPECT_EQ(NEO::SipKernelType::count, NEO::MockSipData::calledType);
    EXPECT_FALSE(NEO::MockSipData::called);
}

TEST(L0DeviceTest, givenDeviceWithoutIGCCompilerLibraryThenInvalidDependencyIsNotReturned) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;

    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto igcNameGuard = NEO::pushIgcDllName("_invalidIGC");
    auto mockDevice = reinterpret_cast<NEO::MockDevice *>(neoDevice.get());
    mockDevice->setPreemptionMode(NEO::PreemptionMode::Initial);
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
}

TEST(L0DeviceTest, givenDeviceWithoutAnyCompilerLibraryThenInvalidDependencyIsNotReturned) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;

    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    auto oldFclDllName = Os::frontEndDllName;
    Os::frontEndDllName = "_invalidFCL";
    auto igcNameGuard = NEO::pushIgcDllName("_invalidIGC");
    auto mockDevice = reinterpret_cast<NEO::MockDevice *>(neoDevice.get());
    mockDevice->setPreemptionMode(NEO::PreemptionMode::Initial);

    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    Os::frontEndDllName = oldFclDllName;
    ASSERT_NE(nullptr, device);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
}

TEST(L0DeviceTest, givenFilledTopologyWhenGettingApiSliceThenCorrectSliceIdIsReturned) {

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

TEST(L0DeviceTest, whenCallingGetDeviceMemoryNameThenCorrectTypeIsReturned) {
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, nullptr));
    ASSERT_NE(nullptr, device);

    uint32_t count = 1;
    ze_device_memory_properties_t memProperties = {};

    auto &sysInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo;

    for (uint32_t i = 0; i < 10; i++) {
        sysInfo.MemoryType = i;

        EXPECT_EQ(ZE_RESULT_SUCCESS, device->getMemoryProperties(&count, &memProperties));

        if (i == 2 || i == 3 || i == 5 || i == 8 || i == 9) {
            EXPECT_STREQ("HBM", memProperties.name);
        } else {
            EXPECT_STREQ("DDR", memProperties.name);
        }
    }
}

TEST(L0DeviceTest, givenFilledTopologyForZeroSubDeviceWhenGettingApiSliceForHigherSubDevicesThenFalseIsReturned) {

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
        debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), rootDeviceIndex);
        execEnv = neoDevice->getExecutionEnvironment();
        execEnv->incRefInternal();
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
        if (neoDevice->getPreemptionMode() == NEO::PreemptionMode::MidThread) {
            for (auto &engine : neoDevice->getAllEngines()) {
                NEO::CommandStreamReceiver *csr = engine.commandStreamReceiver;
                if (!csr->getPreemptionAllocation()) {
                    csr->createPreemptionAllocation();
                }
            }
        }
    }

    void TearDown() override {
        driverHandle.reset(nullptr);
        execEnv->decRefInternal();
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::ExecutionEnvironment *execEnv;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
};

TEST_F(DeviceTest, givenEmptySVmAllocStorageWhenAllocateManagedMemoryFromHostPtrThenBufferHostAllocationIsCreated) {
    int data;
    auto allocation = device->allocateManagedMemoryFromHostPtr(&data, sizeof(data), nullptr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(NEO::AllocationType::bufferHostMemory, allocation->getAllocationType());
    EXPECT_EQ(rootDeviceIndex, allocation->getRootDeviceIndex());
    neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
}

TEST_F(DeviceTest, givenEmptySVmAllocStorageWhenAllocateMemoryFromHostPtrThenValidExternalHostPtrAllocationIsCreated) {
    debugManager.flags.EnableHostPtrTracking.set(0);
    constexpr auto dataSize = 1024u;
    auto data = std::make_unique<int[]>(dataSize);

    constexpr auto allocationSize = sizeof(int) * dataSize;

    auto allocation = device->allocateMemoryFromHostPtr(data.get(), allocationSize, false);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(NEO::AllocationType::externalHostPtr, allocation->getAllocationType());
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
                                                                                                           NEO::AllocationType::fillPattern,
                                                                                                           neoDevice->getDeviceBitfield()});
    device->storeReusableAllocation(*allocation);
    EXPECT_FALSE(deviceImp->allocationsForReuse->peekIsEmpty());
    auto obtaindedAllocation = device->obtainReusableAllocation(dataSize, NEO::AllocationType::fillPattern);
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
                                                                                                           NEO::AllocationType::fillPattern,
                                                                                                           neoDevice->getDeviceBitfield()});
    device->storeReusableAllocation(*allocation);
    EXPECT_FALSE(deviceImp->allocationsForReuse->peekIsEmpty());
    auto obtaindedAllocation = device->obtainReusableAllocation(4 * dataSize + 1u, NEO::AllocationType::fillPattern);
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
                                                                                                           NEO::AllocationType::buffer,
                                                                                                           neoDevice->getDeviceBitfield()});
    device->storeReusableAllocation(*allocation);
    EXPECT_FALSE(deviceImp->allocationsForReuse->peekIsEmpty());
    auto obtaindedAllocation = device->obtainReusableAllocation(4 * dataSize + 1u, NEO::AllocationType::fillPattern);
    EXPECT_EQ(nullptr, obtaindedAllocation);
    EXPECT_FALSE(deviceImp->allocationsForReuse->peekIsEmpty());
}

TEST_F(DeviceTest, givenNoOsInterfaceWhenUseCacheReservationApiThenUnsupportedFeatureErrorIsReturned) {
    EXPECT_EQ(nullptr, neoDevice->getRootDeviceEnvironment().osInterface.get());
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, device->reserveCache(0, 0));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, device->setCacheAdvice(nullptr, 0, ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_REGION_DEFAULT));
}

struct DeviceHostPointerTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
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
    EXPECT_EQ(NEO::AllocationType::internalHostMemory, allocation->getAllocationType());
    EXPECT_EQ(rootDeviceIndex, allocation->getRootDeviceIndex());
    EXPECT_NE(allocation->getUnderlyingBuffer(), reinterpret_cast<void *>(buffer));
    EXPECT_EQ(alignUp(size, MemoryConstants::pageSize), allocation->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(buffer, allocation->getUnderlyingBuffer(), size));
    EXPECT_FALSE(allocation->isFlushL3Required());

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

TEST_F(DeviceTest, whenCreatingDeviceThenCreateInOrderCounterAllocatorOnDemandAndHandleDestruction) {
    uint32_t destructorId = 0u;

    class MockDeviceTagAllocator : public DestructorCounted<MockTagAllocator<NEO::DeviceAllocNodeType<true>>, 0> {
      public:
        MockDeviceTagAllocator(uint32_t rootDeviceIndex, MemoryManager *memoryManager, uint32_t &destructorId) : DestructorCounted(destructorId, rootDeviceIndex, memoryManager, 10) {}
    };

    class MockHostTagAllocator : public DestructorCounted<MockTagAllocator<NEO::DeviceAllocNodeType<true>>, 1> {
      public:
        MockHostTagAllocator(uint32_t rootDeviceIndex, MemoryManager *memoryManager, uint32_t &destructorId) : DestructorCounted(destructorId, rootDeviceIndex, memoryManager, 10) {}
    };

    class MockTsAllocator : public DestructorCounted<MockTagAllocator<NEO::DeviceAllocNodeType<true>>, 2> {
      public:
        MockTsAllocator(uint32_t rootDeviceIndex, MemoryManager *memoryManager, uint32_t &destructorId) : DestructorCounted(destructorId, rootDeviceIndex, memoryManager, 10) {}
    };

    class MyMockDevice : public DestructorCounted<NEO::MockDevice, 3> {
      public:
        MyMockDevice(NEO::ExecutionEnvironment *executionEnvironment, uint32_t rootDeviceIndex, uint32_t &destructorId) : DestructorCounted(destructorId, executionEnvironment, rootDeviceIndex) {}
    };

    const uint32_t rootDeviceIndex = 0u;
    auto hwInfo = *NEO::defaultHwInfo;

    auto executionEnvironment = MyMockDevice::prepareExecutionEnvironment(&hwInfo, rootDeviceIndex);
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(&hwInfo);
    auto *neoMockDevice = new MyMockDevice(executionEnvironment, rootDeviceIndex, destructorId);
    neoMockDevice->createDeviceImpl();

    {
        auto deviceAllocator = new MockDeviceTagAllocator(0, neoMockDevice->getMemoryManager(), destructorId);
        auto hostAllocator = new MockHostTagAllocator(0, neoMockDevice->getMemoryManager(), destructorId);
        auto tsAllocator = new MockTsAllocator(0, neoMockDevice->getMemoryManager(), destructorId);

        MockDeviceImp deviceImp(neoMockDevice);
        deviceImp.deviceInOrderCounterAllocator.reset(deviceAllocator);
        deviceImp.hostInOrderCounterAllocator.reset(hostAllocator);
        deviceImp.inOrderTimestampAllocator.reset(tsAllocator);

        EXPECT_EQ(deviceAllocator, deviceImp.getDeviceInOrderCounterAllocator());
        EXPECT_EQ(hostAllocator, deviceImp.getHostInOrderCounterAllocator());
        EXPECT_EQ(tsAllocator, deviceImp.getInOrderTimestampAllocator());
    }
}

HWTEST_F(DeviceTest, givenTsAllocatorWhenGettingNewTagThenDoInitialize) {
    using TimestampPacketType = typename FamilyType::TimestampPacketType;
    using TimestampPacketsT = NEO::TimestampPackets<TimestampPacketType, 1>;

    auto executionEnvironment = NEO::MockDevice::prepareExecutionEnvironment(NEO::defaultHwInfo.get(), 0);
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
    auto *neoMockDevice = new NEO::MockDevice(executionEnvironment, rootDeviceIndex);
    neoMockDevice->createDeviceImpl();

    MockDeviceImp deviceImp(neoMockDevice);

    auto allocator = deviceImp.getInOrderTimestampAllocator();

    TimestampPacketType data[4] = {123, 456, 789, 987};

    auto tag = static_cast<NEO::TagNode<TimestampPacketsT> *>(allocator->getTag());
    tag->tagForCpuAccess->assignDataToAllTimestamps(0, data);

    EXPECT_EQ(data[0], tag->tagForCpuAccess->getContextStartValue(0));
    EXPECT_EQ(data[1], tag->tagForCpuAccess->getGlobalStartValue(0));
    EXPECT_EQ(data[2], tag->tagForCpuAccess->getContextEndValue(0));
    EXPECT_EQ(data[3], tag->tagForCpuAccess->getGlobalEndValue(0));

    tag->returnTag();

    auto tag2 = static_cast<NEO::TagNode<TimestampPacketsT> *>(allocator->getTag());
    EXPECT_EQ(tag, tag2);

    EXPECT_EQ(static_cast<uint32_t>(Event::STATE_INITIAL), tag->tagForCpuAccess->getContextStartValue(0));
    EXPECT_EQ(static_cast<uint32_t>(Event::STATE_INITIAL), tag->tagForCpuAccess->getGlobalStartValue(0));
    EXPECT_EQ(static_cast<uint32_t>(Event::STATE_INITIAL), tag->tagForCpuAccess->getContextEndValue(0));
    EXPECT_EQ(static_cast<uint32_t>(Event::STATE_INITIAL), tag->tagForCpuAccess->getGlobalEndValue(0));
}

TEST_F(DeviceTest, givenMoreThanOneExtendedPropertiesStructuresWhenKernelPropertiesCalledThenSuccessIsReturnedAndPropertiesAreSet) {
    ze_scheduling_hint_exp_properties_t schedulingHintProperties = {};
    schedulingHintProperties.stype = ZE_STRUCTURE_TYPE_SCHEDULING_HINT_EXP_PROPERTIES;
    schedulingHintProperties.schedulingHintFlags = ZE_SCHEDULING_HINT_EXP_FLAG_FORCE_UINT32;
    schedulingHintProperties.pNext = nullptr;

    ze_float_atomic_ext_properties_t floatAtomicExtendedProperties = {};
    floatAtomicExtendedProperties.stype = ZE_STRUCTURE_TYPE_FLOAT_ATOMIC_EXT_PROPERTIES;
    floatAtomicExtendedProperties.pNext = &schedulingHintProperties;

    const ze_device_fp_flags_t maxValue{std::numeric_limits<uint32_t>::max()};
    floatAtomicExtendedProperties.fp16Flags = maxValue;
    floatAtomicExtendedProperties.fp32Flags = maxValue;
    floatAtomicExtendedProperties.fp64Flags = maxValue;

    ze_device_module_properties_t kernelProperties = {};
    kernelProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_MODULE_PROPERTIES;
    kernelProperties.pNext = &floatAtomicExtendedProperties;

    ze_result_t res = device->getKernelProperties(&kernelProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    EXPECT_NE(maxValue, floatAtomicExtendedProperties.fp16Flags);
    EXPECT_NE(maxValue, floatAtomicExtendedProperties.fp32Flags);
    EXPECT_NE(maxValue, floatAtomicExtendedProperties.fp64Flags);

    EXPECT_NE(ZE_SCHEDULING_HINT_EXP_FLAG_FORCE_UINT32, schedulingHintProperties.schedulingHintFlags);
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

    MockDeviceImp deviceImp(neoMockDevice);

    NEO::RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(*neoMockDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    raii.mockProductHelper->threadArbPolicies = {ThreadArbitrationPolicy::AgeBased,
                                                 ThreadArbitrationPolicy::RoundRobin,
                                                 ThreadArbitrationPolicy::RoundRobinAfterDependency};

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

    MockDeviceImp deviceImp(neoMockDevice);

    NEO::RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(*neoMockDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    raii.mockProductHelper->threadArbPolicies = {ThreadArbitrationPolicy::NotPresent};

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

HWTEST_F(DeviceTest, whenPassingRaytracingExpStructToGetPropertiesThenPropertiesWithCorrectFlagIsReturned) {
    ze_device_module_properties_t kernelProperties = {};
    kernelProperties.stype = ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES;

    ze_device_raytracing_ext_properties_t rayTracingProperties = {};
    rayTracingProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_RAYTRACING_EXT_PROPERTIES;
    rayTracingProperties.flags = ZE_DEVICE_RAYTRACING_EXT_FLAG_FORCE_UINT32;
    rayTracingProperties.maxBVHLevels = 37u;

    kernelProperties.pNext = &rayTracingProperties;

    ze_result_t res = device->getKernelProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_NE(ZE_DEVICE_RAYTRACING_EXT_FLAG_FORCE_UINT32, rayTracingProperties.flags);
    EXPECT_NE(37u, rayTracingProperties.maxBVHLevels);

    unsigned int expectedMaxBVHLevels = 0;
    auto releaseHelper = this->neoDevice->getReleaseHelper();

    if (releaseHelper && releaseHelper->isRayTracingSupported()) {
        expectedMaxBVHLevels = NEO::RayTracingHelper::maxBvhLevels;
    }

    EXPECT_EQ(expectedMaxBVHLevels, rayTracingProperties.maxBVHLevels);
}

HWTEST_F(DeviceTest, givenSetMaxBVHLevelsWhenPassingRaytracingExpStructToGetPropertiesThenPropertiesWithCorrectFlagIsReturned) {

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.SetMaxBVHLevels.set(7);

    ze_device_module_properties_t kernelProperties = {};
    kernelProperties.stype = ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES;

    ze_device_raytracing_ext_properties_t rayTracingProperties = {};
    rayTracingProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_RAYTRACING_EXT_PROPERTIES;
    rayTracingProperties.flags = ZE_DEVICE_RAYTRACING_EXT_FLAG_FORCE_UINT32;
    rayTracingProperties.maxBVHLevels = 37u;

    kernelProperties.pNext = &rayTracingProperties;

    ze_result_t res = device->getKernelProperties(&kernelProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(ZE_DEVICE_RAYTRACING_EXT_FLAG_FORCE_UINT32, rayTracingProperties.flags);

    auto releaseHelper = this->neoDevice->getReleaseHelper();
    if (releaseHelper && releaseHelper->isRayTracingSupported()) {
        EXPECT_EQ(7u, rayTracingProperties.maxBVHLevels);
    } else {
        EXPECT_EQ(0u, rayTracingProperties.maxBVHLevels);
    }
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

using MultiSubDeviceCachePropertiesTest = Test<SingleRootMultiSubDeviceFixture>;
TEST_F(MultiSubDeviceCachePropertiesTest, givenDeviceWithSubDevicesWhenQueriedForCacheSizeThenValueIsMultiplied) {
    ze_device_cache_properties_t deviceCacheProperties = {};

    auto rootDeviceIndex = device->getNEODevice()->getRootDeviceIndex();
    auto &hwInfo = *device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->getMutableHardwareInfo();
    auto singleRootDeviceCacheSize = hwInfo.gtSystemInfo.L3CacheSizeInKb * MemoryConstants::kiloByte;

    uint32_t count = 0;
    ze_result_t res = device->getCacheProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, 1u);
    res = device->getCacheProperties(&count, &deviceCacheProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(deviceCacheProperties.cacheSize, singleRootDeviceCacheSize * numSubDevices);

    uint32_t subDeviceCount = numSubDevices;
    std::vector<ze_device_handle_t> subDevices0(subDeviceCount);
    res = device->getSubDevices(&subDeviceCount, subDevices0.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    L0::Device *subDevice = Device::fromHandle(subDevices0[0]);
    count = 0;
    res = subDevice->getCacheProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, 1u);
    res = subDevice->getCacheProperties(&count, &deviceCacheProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(deviceCacheProperties.cacheSize, singleRootDeviceCacheSize);
}

TEST_F(DeviceTest, givenDevicePropertiesStructureWhenDevicePropertiesCalledThenAllPropertiesAreAssigned) {

    auto &hwInfo = *neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo();

    hwInfo.gtSystemInfo.SliceCount = 3;
    hwInfo.gtSystemInfo.SubSliceCount = 5;
    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 8;

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

    EXPECT_EQ(8u, deviceProperties.numEUsPerSubslice);
    EXPECT_EQ(2u, deviceProperties.numSubslicesPerSlice);
    EXPECT_EQ(3u, deviceProperties.numSlices);
}

TEST_F(DeviceTest, givenNullPointerWhenCallingGetVectorWidthPropertiesExtThenInvalidArgumentIsReturned) {
    ze_device_vector_width_properties_ext_t *vectorWidthProperties = nullptr;
    auto deviceImp = static_cast<DeviceImp *>(device);
    uint32_t pCount = 1;
    ze_result_t result = deviceImp->getVectorWidthPropertiesExt(&pCount, vectorWidthProperties);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(DeviceTest, givenValidPointerWhenCallingGetVectorWidthPropertiesExtThenPropertiesAreSetCorrectly) {
    ze_device_vector_width_properties_ext_t vectorWidthProperties = {};
    vectorWidthProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_VECTOR_WIDTH_PROPERTIES_EXT;
    vectorWidthProperties.pNext = nullptr;
    uint32_t pCount = 0;

    auto deviceImp = static_cast<DeviceImp *>(device);
    ze_result_t result = deviceImp->getVectorWidthPropertiesExt(&pCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, pCount);

    pCount = 2; // Check that the pCount is updated to the correct limit.
    result = deviceImp->getVectorWidthPropertiesExt(&pCount, &vectorWidthProperties);
    EXPECT_EQ(1u, pCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(16u, vectorWidthProperties.preferred_vector_width_char);
    EXPECT_EQ(8u, vectorWidthProperties.preferred_vector_width_short);
    EXPECT_EQ(4u, vectorWidthProperties.preferred_vector_width_int);
    EXPECT_EQ(1u, vectorWidthProperties.preferred_vector_width_long);
    EXPECT_EQ(1u, vectorWidthProperties.preferred_vector_width_float);
    EXPECT_EQ(8u, vectorWidthProperties.preferred_vector_width_half);
    EXPECT_EQ(16u, vectorWidthProperties.native_vector_width_char);
    EXPECT_EQ(8u, vectorWidthProperties.native_vector_width_short);
    EXPECT_EQ(4u, vectorWidthProperties.native_vector_width_int);
    EXPECT_EQ(1u, vectorWidthProperties.native_vector_width_long);
    EXPECT_EQ(1u, vectorWidthProperties.native_vector_width_float);
    EXPECT_EQ(8u, vectorWidthProperties.native_vector_width_half);

    // Check that using the pCount updated to the correct limit, that this still works.
    result = deviceImp->getVectorWidthPropertiesExt(&pCount, &vectorWidthProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(16u, vectorWidthProperties.preferred_vector_width_char);
    EXPECT_EQ(8u, vectorWidthProperties.preferred_vector_width_short);
    EXPECT_EQ(4u, vectorWidthProperties.preferred_vector_width_int);
    EXPECT_EQ(1u, vectorWidthProperties.preferred_vector_width_long);
    EXPECT_EQ(1u, vectorWidthProperties.preferred_vector_width_float);
    EXPECT_EQ(8u, vectorWidthProperties.preferred_vector_width_half);
    EXPECT_EQ(16u, vectorWidthProperties.native_vector_width_char);
    EXPECT_EQ(8u, vectorWidthProperties.native_vector_width_short);
    EXPECT_EQ(4u, vectorWidthProperties.native_vector_width_int);
    EXPECT_EQ(1u, vectorWidthProperties.native_vector_width_long);
    EXPECT_EQ(1u, vectorWidthProperties.native_vector_width_float);
    EXPECT_EQ(8u, vectorWidthProperties.native_vector_width_half);
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
    debugManager.flags.OverrideDeviceName.set(testDeviceName);

    auto deviceImp = static_cast<DeviceImp *>(device);
    ze_device_properties_t deviceProperties{};
    auto name = device->getNEODevice()->getDeviceInfo().name;
    deviceImp->driverInfo.reset();
    deviceImp->getProperties(&deviceProperties);
    EXPECT_STRNE(deviceProperties.name, name.c_str());
    EXPECT_STREQ(deviceProperties.name, testDeviceName.c_str());
}

TEST_F(DeviceTest, WhenRequestingZeEuCountThenExpectedEUsAreReturned) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_eu_count_ext_t zeEuCountDesc = {ZE_STRUCTURE_TYPE_EU_COUNT_EXT};
    deviceProperties.pNext = &zeEuCountDesc;

    uint32_t maxEuPerSubSlice = 8;
    uint32_t subSliceCount = 18;
    uint32_t expectedEUs = maxEuPerSubSlice * subSliceCount;

    auto hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.EUCount = expectedEUs;

    device->getProperties(&deviceProperties);

    EXPECT_EQ(expectedEUs, zeEuCountDesc.numTotalEUs);
}

TEST_F(DeviceTest, WhenRequestingZeEuCountWithoutStypeCorrectThenNoEusAreReturned) {
    ze_device_properties_t deviceProperties = {};
    ze_eu_count_ext_t zeEuCountDesc = {ZE_STRUCTURE_TYPE_EU_COUNT_EXT};
    zeEuCountDesc.numTotalEUs = std::numeric_limits<uint32_t>::max();
    deviceProperties.pNext = &zeEuCountDesc;

    uint32_t maxEuPerSubSlice = 48;
    uint32_t subSliceCount = 8;
    uint32_t expectedEUs = maxEuPerSubSlice * subSliceCount;

    auto hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.EUCount = expectedEUs;

    device->getProperties(&deviceProperties);

    EXPECT_NE(expectedEUs, zeEuCountDesc.numTotalEUs);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), zeEuCountDesc.numTotalEUs);
}

TEST_F(DeviceTest, WhenRequestingZeEuCountWithIncorrectStypeThenPNextIsIgnored) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_eu_count_ext_t zeEuCountDesc = {ZE_STRUCTURE_TYPE_SCHEDULING_HINT_EXP_PROPERTIES};
    deviceProperties.pNext = &zeEuCountDesc;
    device->getProperties(&deviceProperties);
    EXPECT_EQ(0u, zeEuCountDesc.numTotalEUs);
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
    debugManager.flags.DebugApiUsed.set(1);
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    deviceProperties.type = ZE_DEVICE_TYPE_GPU;

    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.MaxSubSlicesSupported = 48;
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.MaxSlicesSupported = 3;
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.SubSliceCount = 8;
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->gtSystemInfo.SliceCount = 1;
    device->getProperties(&deviceProperties);

    EXPECT_EQ(16u, deviceProperties.numSubslicesPerSlice);
}

TEST_F(DeviceTest, givenDeviceIpVersionWhenGetDevicePropertiesThenCorrectResultIsReturned) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_device_ip_version_ext_t zeDeviceIpVersion = {ZE_STRUCTURE_TYPE_DEVICE_IP_VERSION_EXT};
    zeDeviceIpVersion.ipVersion = std::numeric_limits<uint32_t>::max();
    deviceProperties.pNext = &zeDeviceIpVersion;

    auto &compilerProductHelper = device->getCompilerProductHelper();

    auto expectedIpVersion = compilerProductHelper.getHwIpVersion(device->getHwInfo());

    device->getProperties(&deviceProperties);
    EXPECT_NE(std::numeric_limits<uint32_t>::max(), zeDeviceIpVersion.ipVersion);
    EXPECT_EQ(expectedIpVersion, zeDeviceIpVersion.ipVersion);
}

TEST_F(DeviceTest, givenDeviceIpVersionOverrideWhenGetDevicePropertiesThenReturnedOverriddenIpVersion) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_device_ip_version_ext_t zeDeviceIpVersion = {ZE_STRUCTURE_TYPE_DEVICE_IP_VERSION_EXT};
    zeDeviceIpVersion.ipVersion = std::numeric_limits<uint32_t>::max();
    deviceProperties.pNext = &zeDeviceIpVersion;

    auto &compilerProductHelper = device->getCompilerProductHelper();
    auto originalIpVersion = compilerProductHelper.getHwIpVersion(device->getHwInfo());
    auto overriddenHwInfo = device->getHwInfo();
    overriddenHwInfo.ipVersionOverrideExposedToTheApplication.value = originalIpVersion + 1;

    NEO::DeviceVector neoDevices;
    neoDevices.push_back(std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&overriddenHwInfo, rootDeviceIndex)));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(neoDevices));
    auto l0DeviceWithOverride = driverHandle->devices[0];

    l0DeviceWithOverride->getProperties(&deviceProperties);
    EXPECT_EQ(overriddenHwInfo.ipVersionOverrideExposedToTheApplication.value, zeDeviceIpVersion.ipVersion);
}

TEST_F(DeviceTest, givenCallToDevicePropertiesThenMaximumMemoryToBeAllocatedIsCorrectlyReturned) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    deviceProperties.maxMemAllocSize = 0;
    device->getProperties(&deviceProperties);
    EXPECT_EQ(deviceProperties.maxMemAllocSize, this->neoDevice->getDeviceInfo().maxMemAllocSize);

    auto expectedSize = this->neoDevice->getDeviceInfo().globalMemSize;

    auto &rootDeviceEnvironment = this->neoDevice->getRootDeviceEnvironment();
    const auto &compilerProductHelper = rootDeviceEnvironment.getHelper<NEO::CompilerProductHelper>();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<NEO::GfxCoreHelper>();

    if (compilerProductHelper.isForceToStatelessRequired()) {
        EXPECT_EQ(deviceProperties.maxMemAllocSize, expectedSize);
    } else {
        EXPECT_EQ(deviceProperties.maxMemAllocSize,
                  std::min(ApiSpecificConfig::getReducedMaxAllocSize(expectedSize), gfxCoreHelper.getMaxMemAllocSize()));
    }
}

TEST_F(DeviceTest, givenNodeOrdinalFlagWhenCallAdjustCommandQueueDescThenDescOrdinalProperlySet) {
    DebugManagerStateRestore restore;
    auto nodeOrdinal = EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, neoDevice->getRootDeviceEnvironment());
    debugManager.flags.NodeOrdinal.set(nodeOrdinal);

    auto deviceImp = static_cast<MockDeviceImp *>(device);
    ze_command_queue_desc_t desc = {};
    EXPECT_EQ(desc.ordinal, 0u);

    auto &engineGroups = deviceImp->getActiveDevice()->getRegularEngineGroups();
    engineGroups.clear();
    NEO::EngineGroupT engineGroupCompute{};
    engineGroupCompute.engineGroupType = NEO::EngineGroupType::compute;
    NEO::EngineGroupT engineGroupRender{};
    engineGroupRender.engineGroupType = NEO::EngineGroupType::renderCompute;
    engineGroups.push_back(engineGroupCompute);
    engineGroups.push_back(engineGroupRender);

    uint32_t expectedOrdinal = 1u;
    deviceImp->adjustCommandQueueDesc(desc.ordinal, desc.index);
    EXPECT_EQ(desc.ordinal, expectedOrdinal);
}

using InvalidExtensionTest = DeviceTest;
TEST_F(InvalidExtensionTest, givenInvalidExtensionPropertiesDuringDeviceGetPropertiesThenPropertiesIgnoredWithSuccess) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_device_properties_t invalidExtendedProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    deviceProperties.pNext = &invalidExtendedProperties;
    ze_result_t result = device->getProperties(&deviceProperties);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(DeviceTest, givenNodeOrdinalFlagWhenCallAdjustCommandQueueDescThenDescOrdinalAndDescIndexProperlySet) {
    DebugManagerStateRestore restore;
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {
        EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const override {
            return EngineGroupType::compute;
        }
    };

    auto rootDeviceIndex = device->getNEODevice()->getRootDeviceIndex();
    auto &hwInfo = *device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->getMutableHardwareInfo();
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;

    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]);

    auto nodeOrdinal = EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_CCS2, neoDevice->getRootDeviceEnvironment());
    debugManager.flags.NodeOrdinal.set(nodeOrdinal);

    auto deviceImp = static_cast<MockDeviceImp *>(device);
    ze_command_queue_desc_t desc = {};
    EXPECT_EQ(desc.ordinal, 0u);

    auto &engineGroups = deviceImp->getActiveDevice()->getRegularEngineGroups();
    engineGroups.clear();
    NEO::EngineGroupT engineGroupCompute{};
    engineGroupCompute.engineGroupType = NEO::EngineGroupType::compute;
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
    engineGroupRender.engineGroupType = NEO::EngineGroupType::renderCompute;
    engineGroups.push_back(engineGroupRender);
    engineGroups.push_back(engineGroupCompute);

    uint32_t expectedOrdinal = 1u;
    uint32_t expectedIndex = 2u;
    deviceImp->adjustCommandQueueDesc(desc.ordinal, desc.index);
    EXPECT_EQ(desc.ordinal, expectedOrdinal);
    EXPECT_EQ(desc.index, expectedIndex);
}

TEST_F(DeviceTest, givenNodeOrdinalFlagNotSetWhenCallAdjustCommandQueueDescThenDescOrdinalIsNotModified) {
    DebugManagerStateRestore restore;
    int nodeOrdinal = -1;
    debugManager.flags.NodeOrdinal.set(nodeOrdinal);

    auto deviceImp = static_cast<MockDeviceImp *>(device);
    ze_command_queue_desc_t desc = {};
    EXPECT_EQ(desc.ordinal, 0u);

    deviceImp->adjustCommandQueueDesc(desc.ordinal, desc.index);
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
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hardwareInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    setDriverAndDevice();

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    device->getProperties(&deviceProps);
    EXPECT_FALSE(deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING);
}

TEST_F(DeviceHwInfoTest, givenDeviceWithPageFaultSupportThenFlagIsSet) {
    NEO::HardwareInfo hardwareInfo = *NEO::defaultHwInfo;
    hardwareInfo.capabilityTable.supportsOnDemandPageFaults = true;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hardwareInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    setDriverAndDevice();

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    device->getProperties(&deviceProps);
    EXPECT_TRUE(deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING);
}

TEST_F(DeviceTest, whenGetDevicePropertiesCalledThenCorrectDevicePropertyEccFlagSet) {
    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    device->getProperties(&deviceProps);
    uint32_t expected = (this->neoDevice->getDeviceInfo().errorCorrectionSupport) ? static_cast<uint32_t>(ZE_DEVICE_PROPERTY_FLAG_ECC) : 0u;
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

TEST_F(DeviceTest, WhenGettingKernelPropertiesThenDp4aFlagIsSet) {
    ze_device_module_properties_t kernelProps = {};

    device->getKernelProperties(&kernelProps);
    EXPECT_NE(0u, kernelProps.flags & ZE_DEVICE_MODULE_FLAG_DP4A);
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

TEST_F(DeviceTest, whenGetGlobalTimestampIsCalledWithOsInterfaceThenSuccessIsReturnedAndValuesSetCorrectly) {
    uint64_t hostTs = 0u;
    uint64_t deviceTs = 0u;

    debugManager.flags.EnableGlobalTimestampViaSubmission.set(0);

    ze_result_t result = device->getGlobalTimestamps(&hostTs, &deviceTs);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_NE(0u, hostTs);
    EXPECT_NE(0u, deviceTs);
}

TEST_F(DeviceTest, whenGetGlobalTimestampIsCalledWithSubmissionThenSuccessIsReturned) {

    debugManager.flags.EnableGlobalTimestampViaSubmission.set(1);

    uint64_t hostTs = 0u;
    uint64_t deviceTs = 0u;

    // First time to hit the if case of initialization of internal structures.
    ze_result_t result = device->getGlobalTimestamps(&hostTs, &deviceTs);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(0u, hostTs);

    // Second time to hit the false case for initialization of internal structures as they are already initialized.
    result = device->getGlobalTimestamps(&hostTs, &deviceTs);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(0u, hostTs);
}

TEST_F(DeviceTest, givenAppendWriteGlobalTimestampFailsWhenGetGlobalTimestampsUsingSubmissionThenErrorIsReturned) {
    struct MockCommandListAppendWriteGlobalTimestampFail : public MockCommandList {
        ze_result_t appendWriteGlobalTimestamp(uint64_t *dstptr, ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    };

    debugManager.flags.EnableGlobalTimestampViaSubmission.set(1);

    uint64_t hostTs = 0u;
    uint64_t deviceTs = 0u;

    // First time to hit the if case of initialization of internal structures.
    ze_result_t result = device->getGlobalTimestamps(&hostTs, &deviceTs);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(0u, hostTs);

    auto mockCommandList = std::make_unique<MockCommandListAppendWriteGlobalTimestampFail>();
    ze_command_list_handle_t mockCommandListHandle = mockCommandList.get();

    auto deviceImp = static_cast<DeviceImp *>(device);
    auto actualGlobalTimestampCommandList = deviceImp->globalTimestampCommandList;
    // Swap the command list with the mock command list.
    deviceImp->globalTimestampCommandList = mockCommandListHandle;

    L0::CommandList::fromHandle(deviceImp->globalTimestampCommandList)->appendWriteGlobalTimestamp(nullptr, nullptr, 0, nullptr);

    result = device->getGlobalTimestamps(&hostTs, &deviceTs);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);

    // Swap back the command list.
    deviceImp->globalTimestampCommandList = actualGlobalTimestampCommandList;
}

TEST_F(DeviceTest, givenCreateHostUnifiedMemoryAllocationFailsWhenGetGlobalTimestampsUsingSubmissionThenErrorIsReturned) {
    struct MockSvmAllocsManager : public NEO::SVMAllocsManager {
        MockSvmAllocsManager(MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager) {}

        void *createHostUnifiedMemoryAllocation(size_t size, const NEO::SVMAllocsManager::UnifiedMemoryProperties &unifiedMemoryProperties) override {
            return nullptr;
        }
    };

    class MockDriverHandleImp : public Mock<L0::DriverHandleImp> {
      public:
        void setSVMAllocsManager(MockSvmAllocsManager *mockSvmAllocsManager) {
            this->svmAllocsManager = mockSvmAllocsManager;
        }
    };

    debugManager.flags.EnableGlobalTimestampViaSubmission.set(1);

    auto mockSvmAllocsManager = std::make_unique<MockSvmAllocsManager>(nullptr);
    auto mockSvmAllocsManagerHandle = mockSvmAllocsManager.get();

    auto mockDriverHandleImp = std::make_unique<MockDriverHandleImp>();
    mockDriverHandleImp->setSVMAllocsManager(mockSvmAllocsManagerHandle);

    // Swap the driver handle with the mock driver handle but keep the original driver handle to be swapped back.
    auto deviceImp = static_cast<DeviceImp *>(device);
    auto originalDriverHandle = deviceImp->getDriverHandle();
    deviceImp->setDriverHandle(mockDriverHandleImp.get());

    uint64_t hostTs = 0u;
    uint64_t deviceTs = 0u;

    ze_result_t result = device->getGlobalTimestamps(&hostTs, &deviceTs);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);

    mockDriverHandleImp->setSVMAllocsManager(nullptr);
    // Swap back the driver handle.
    deviceImp->setDriverHandle(originalDriverHandle);
}

struct GlobalTimestampTest : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), rootDeviceIndex);
        if (neoDevice->getPreemptionMode() == NEO::PreemptionMode::MidThread) {
            for (auto &engine : neoDevice->getAllEngines()) {
                NEO::CommandStreamReceiver *csr = engine.commandStreamReceiver;
                if (!csr->getPreemptionAllocation()) {
                    csr->createPreemptionAllocation();
                }
            }
        }
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
};

TEST_F(GlobalTimestampTest, whenGetGlobalTimestampCalledAndGetGpuCpuTimeIsDeviceLostReturnError) {
    uint64_t hostTs = 0u;
    uint64_t deviceTs = 0u;

    neoDevice->setOSTime(new FalseGpuCpuTime());
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];

    ze_result_t result = device->getGlobalTimestamps(&hostTs, &deviceTs);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
}

TEST_F(GlobalTimestampTest, whenGetGlobalTimestampCalledAndGetGpuCpuTimeIsUnsupportedFeatureThenSubmissionMethodIsUsed) {

    neoDevice->setOSTime(new FalseUnSupportedFeatureGpuCpuTime());
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];

    uint64_t hostTs = 0u;
    uint64_t deviceTs = 0u;

    ze_result_t result = device->getGlobalTimestamps(&hostTs, &deviceTs);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(GlobalTimestampTest, whenGetProfilingTimerClockandProfilingTimerResolutionThenVerifyRelation) {
    neoDevice->setOSTime(new FalseGpuCpuTime());
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
    neoDevice->setOSTime(new FalseGpuCpuTime());
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
    neoDevice->setOSTime(new FalseGpuCpuTime());
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
    debugManager.flags.UseCyclesPerSecondTimer.set(1u);

    neoDevice->setOSTime(new FalseGpuCpuTime());
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
    TimeQueryStatus getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, NEO::OSTime *) override {
        pGpuCpuTime->cpuTimeinNS = mockCpuTimeInNs;
        pGpuCpuTime->gpuTimeStamp = mockGpuTimeInNs;
        return TimeQueryStatus::success;
    }
    double getDynamicDeviceTimerResolution() const override {
        return NEO::OSTime::getDeviceTimerResolution();
    }
    uint64_t getDynamicDeviceTimerClock() const override {
        return static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution());
    }
    uint64_t mockCpuTimeInNs = 0u;
    uint64_t mockGpuTimeInNs = 100u;
};

class FailCpuTime : public NEO::OSTime {
  public:
    FailCpuTime() {
        this->deviceTime = std::make_unique<FalseCpuDeviceTime>();
    }
    bool getCpuTime(uint64_t *timeStamp) override {
        return false;
    };
    bool getCpuTimeHost(uint64_t *timeStamp) override {
        return true;
    };
    double getHostTimerResolution() const override {
        return 0;
    }
    uint64_t getCpuRawTimestamp() override {
        return 0;
    }
    static std::unique_ptr<OSTime> create() {
        return std::unique_ptr<OSTime>(new FailCpuTime());
    }

    FalseCpuDeviceTime *getFalseCpuDeviceTime() {
        return static_cast<FalseCpuDeviceTime *>(this->deviceTime.get());
    }
};

TEST_F(GlobalTimestampTest, whenGetGlobalTimestampsUsingSubmissionAndGetCpuTimeHostFailsThenDeviceLostErrorIsReturned) {

    debugManager.flags.EnableGlobalTimestampViaSubmission.set(1);

    neoDevice->setOSTime(new FailCpuTime());
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];

    uint64_t hostTs = 0u;
    uint64_t deviceTs = 0u;

    ze_result_t result = device->getGlobalTimestamps(&hostTs, &deviceTs);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
}

TEST_F(GlobalTimestampTest, whenGetGlobalTimestampCalledAndGetCpuTimeIsFalseReturnArbitraryValues) {
    uint64_t hostTs = 0u;
    uint64_t deviceTs = 0u;

    neoDevice->setOSTime(new FailCpuTime());
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];

    ze_result_t result = device->getGlobalTimestamps(&hostTs, &deviceTs);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, hostTs);
    EXPECT_NE(0u, deviceTs);
}

TEST_F(DeviceTest, givenPrintGlobalTimestampIsSetWhenGetGlobalTimestampIsCalledThenOutputStringIsAsExpected) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintGlobalTimestampInNs.set(true);
    uint64_t hostTs = 0u;
    uint64_t deviceTs = 0u;

    auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironmentRef();
    auto falseCpuTime = std::make_unique<FailCpuTime>();
    auto cpuDeviceTime = falseCpuTime->getFalseCpuDeviceTime();
    // Using 36 bits for gpu timestamp
    cpuDeviceTime->mockGpuTimeInNs = 0xFFFFFFFFF;
    rootDeviceEnvironment.osTime = std::move(falseCpuTime);

    auto &capabilityTable = rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable;
    capabilityTable.timestampValidBits = 36;
    capabilityTable.kernelTimestampValidBits = 32;

    StreamCapture capture;
    capture.captureStdout();
    ze_result_t result = device->getGlobalTimestamps(&hostTs, &deviceTs);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::string output = capture.getCapturedStdout();
    // Considering kernelTimestampValidBits(32)
    auto gpuTimeStamp = cpuDeviceTime->mockGpuTimeInNs & 0xFFFFFFFF;
    const std::string expectedString("Host timestamp in ns : 0 | Device timestamp in ns : " +
                                     std::to_string(static_cast<uint64_t>(neoDevice->getProfilingTimerResolution()) *
                                                    gpuTimeStamp) +
                                     "\n");
    EXPECT_STREQ(output.c_str(), expectedString.c_str());
}

TEST_F(DeviceTest, givenPrintGlobalTimestampIsSetAnd64bitTimestampWhenGetGlobalTimestampIsCalledThenOutputStringIsAsExpected) {

    auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironmentRef();
    auto falseCpuTime = std::make_unique<FailCpuTime>();
    auto cpuDeviceTime = falseCpuTime->getFalseCpuDeviceTime();
    // Using 36 bits for gpu timestamp
    cpuDeviceTime->mockGpuTimeInNs = 0xFFFFFFFFF;
    rootDeviceEnvironment.osTime = std::move(falseCpuTime);

    auto &capabilityTable = rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable;
    capabilityTable.timestampValidBits = 64;
    capabilityTable.kernelTimestampValidBits = 64;
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintGlobalTimestampInNs.set(true);

    uint64_t hostTs = 0u;
    uint64_t deviceTs = 0u;

    StreamCapture capture;
    capture.captureStdout();
    ze_result_t result = device->getGlobalTimestamps(&hostTs, &deviceTs);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::string output = capture.getCapturedStdout();
    const std::string expectedString("Host timestamp in ns : 0 | Device timestamp in ns : " +
                                     std::to_string(static_cast<uint64_t>(neoDevice->getProfilingTimerResolution()) *
                                                    cpuDeviceTime->mockGpuTimeInNs) +
                                     "\n");
    EXPECT_STREQ(output.c_str(), expectedString.c_str());
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
    auto &productHelper = device->getProductHelper();
    EXPECT_EQ(memProperties.maxClockRate, productHelper.getDeviceMemoryMaxClkRate(hwInfo, nullptr, 0));
    EXPECT_EQ(memProperties.maxBusWidth, this->neoDevice->getDeviceInfo().addressBits);
    EXPECT_EQ(memProperties.totalSize, this->neoDevice->getDeviceInfo().globalMemSize);
    EXPECT_EQ(0u, memProperties.flags);
}

HWTEST2_F(DeviceGetMemoryTests, whenCallingGetMemoryPropertiesForMemoryExtPropertiesThenPropertiesAreReturned, MatchAny) {
    uint32_t count = 0;
    ze_result_t res = device->getMemoryProperties(&count, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    NEO::RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(*this->neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    auto &productHelper = device->getProductHelper();
    ze_device_memory_properties_t memProperties = {};
    ze_device_memory_ext_properties_t memExtProperties = {};
    memExtProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_EXT_PROPERTIES;
    memProperties.pNext = &memExtProperties;
    res = device->getMemoryProperties(&count, &memProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);
    const std::array<ze_device_memory_ext_type_t, 10> sysInfoMemType = {
        ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR4,
        ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR5,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
        ZE_DEVICE_MEMORY_EXT_TYPE_GDDR6,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
        ZE_DEVICE_MEMORY_EXT_TYPE_DDR5,
        ZE_DEVICE_MEMORY_EXT_TYPE_GDDR7,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
    };

    auto &hwInfo = device->getHwInfo();
    auto bandwidthPerNanoSecond = productHelper.getDeviceMemoryMaxBandWidthInBytesPerSecond(hwInfo, nullptr, 0) / 1000000000;

    UNRECOVERABLE_IF(hwInfo.gtSystemInfo.MemoryType >= sizeof(sysInfoMemType));
    EXPECT_EQ(memExtProperties.type, sysInfoMemType[hwInfo.gtSystemInfo.MemoryType]);
    EXPECT_EQ(memExtProperties.physicalSize, productHelper.getDeviceMemoryPhysicalSizeInBytes(nullptr, 0));
    EXPECT_EQ(memExtProperties.readBandwidth, bandwidthPerNanoSecond);
    EXPECT_EQ(memExtProperties.writeBandwidth, memExtProperties.readBandwidth);
    EXPECT_EQ(memExtProperties.bandwidthUnit, ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC);
}

HWTEST2_F(DeviceGetMemoryTests, whenCallingGetMemoryPropertiesWith2LevelsOfPnextForMemoryExtPropertiesThenPropertiesAreReturned, MatchAny) {
    uint32_t count = 0;
    ze_result_t res = device->getMemoryProperties(&count, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    NEO::RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(*device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    auto &productHelper = device->getProductHelper();
    ze_device_memory_properties_t memProperties = {};
    ze_base_properties_t baseProperties{};
    ze_device_memory_ext_properties_t memExtProperties = {};
    memExtProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_EXT_PROPERTIES;
    // Setting up the 1st level pNext
    memProperties.pNext = &baseProperties;
    // Setting up the 2nd level pNext with device memory properties
    baseProperties.pNext = &memExtProperties;

    res = device->getMemoryProperties(&count, &memProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);
    const std::array<ze_device_memory_ext_type_t, 10> sysInfoMemType = {
        ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR4,
        ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR5,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
        ZE_DEVICE_MEMORY_EXT_TYPE_GDDR6,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
        ZE_DEVICE_MEMORY_EXT_TYPE_DDR5,
        ZE_DEVICE_MEMORY_EXT_TYPE_GDDR7,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
    };

    auto &hwInfo = device->getHwInfo();
    auto bandwidthPerNanoSecond = productHelper.getDeviceMemoryMaxBandWidthInBytesPerSecond(hwInfo, nullptr, 0) / 1000000000;

    UNRECOVERABLE_IF(hwInfo.gtSystemInfo.MemoryType >= sizeof(sysInfoMemType));
    EXPECT_EQ(memExtProperties.type, sysInfoMemType[hwInfo.gtSystemInfo.MemoryType]);
    EXPECT_EQ(memExtProperties.physicalSize, productHelper.getDeviceMemoryPhysicalSizeInBytes(nullptr, 0));
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
        debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
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
    debugManager.flags.OverrideDefaultFP64Settings.set(1);

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

struct DeviceHasNoFp64HasFp64EmulationTest : public ::testing::Test {
    void SetUp() override {
        HardwareInfo fp64EmulationDevice = *defaultHwInfo;
        fp64EmulationDevice.capabilityTable.ftrSupportsFP64 = false;
        fp64EmulationDevice.capabilityTable.ftrSupportsFP64Emulation = true;
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&fp64EmulationDevice, rootDeviceIndex);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::Device *neoDevice = nullptr;
    L0::Device *device = nullptr;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 1u;
};

TEST_F(DeviceHasNoFp64HasFp64EmulationTest, givenDefaultFp64EmulationSettingsAndDeviceSupportingFp64EmulationAndWithoutNativeFp64ThenReportCorrectFp64Flags) {
    ze_device_module_properties_t kernelProperties = {};
    memset(&kernelProperties, std::numeric_limits<int>::max(), sizeof(ze_device_module_properties_t));
    kernelProperties.pNext = nullptr;

    device->getKernelProperties(&kernelProperties);
    EXPECT_FALSE(kernelProperties.flags & ZE_DEVICE_MODULE_FLAG_FP64);
    EXPECT_FALSE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_SOFT_FLOAT);
    EXPECT_EQ(0u, kernelProperties.fp64flags);
}

TEST_F(DeviceHasNoFp64HasFp64EmulationTest, givenFp64EmulationEnabledAndDeviceSupportingFp64EmulationAndWithoutNativeFp64ThenReportCorrectFp64Flags) {
    neoDevice->getExecutionEnvironment()->setFP64EmulationEnabled();
    ze_device_module_properties_t kernelProperties = {};
    memset(&kernelProperties, std::numeric_limits<int>::max(), sizeof(ze_device_module_properties_t));
    kernelProperties.pNext = nullptr;

    device->getKernelProperties(&kernelProperties);
    EXPECT_TRUE(kernelProperties.flags & ZE_DEVICE_MODULE_FLAG_FP64);
    EXPECT_TRUE(kernelProperties.fp64flags & ZE_DEVICE_FP_FLAG_SOFT_FLOAT);

    ze_device_fp_flags_t defaultFpFlags = static_cast<ze_device_fp_flags_t>(ZE_DEVICE_FP_FLAG_ROUND_TO_NEAREST |
                                                                            ZE_DEVICE_FP_FLAG_ROUND_TO_ZERO |
                                                                            ZE_DEVICE_FP_FLAG_ROUND_TO_INF |
                                                                            ZE_DEVICE_FP_FLAG_INF_NAN |
                                                                            ZE_DEVICE_FP_FLAG_DENORM |
                                                                            ZE_DEVICE_FP_FLAG_FMA);
    EXPECT_EQ(defaultFpFlags, kernelProperties.fp64flags & defaultFpFlags);
}

struct DeviceHasFp64Test : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
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
        debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
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

    bool hasPageFaultsEnabled(const NEO::Device &neoDevice) override {
        if (debugManager.flags.EnableRecoverablePageFaults.get() != -1) {
            return debugManager.flags.EnableRecoverablePageFaults.get();
        }

        return false;
    }

    bool isKmdMigrationAvailable(uint32_t rootDeviceIndex) override {
        if (debugManager.flags.UseKmdMigration.get() != -1) {
            return debugManager.flags.UseKmdMigration.get();
        }

        return false;
    }
};

template <int32_t enablePartitionWalker>
struct MultipleDevicesFixture : public ::testing::Test {
    void SetUp() override {

        debugManager.flags.EnableWalkerPartition.set(enablePartitionWalker);
        debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        VariableBackup<bool> mockDeviceFlagBackup(&NEO::MockDevice::createSingleDevice, false);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
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
            context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
            context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
            if (neoDevice->getPreemptionMode() == NEO::PreemptionMode::MidThread) {
                auto &csr = neoDevice->getInternalEngine().commandStreamReceiver;
                if (!csr->getPreemptionAllocation()) {
                    csr->createPreemptionAllocation();
                }
            }
        }
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

TEST_F(MultipleDevicesEnabledImplicitScalingTest, WhenRequestingZeEuCountThenExpectedEUsAreReturned) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_eu_count_ext_t zeEuCountDesc = {ZE_STRUCTURE_TYPE_EU_COUNT_EXT};
    deviceProperties.pNext = &zeEuCountDesc;

    uint32_t maxEuPerSubSlice = 48;
    uint32_t subSliceCount = 8;
    uint32_t expectedEUs = maxEuPerSubSlice * subSliceCount;

    L0::Device *device = driverHandle->devices[0];
    auto hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.EUCount = expectedEUs;

    device->getProperties(&deviceProperties);

    EXPECT_EQ(expectedEUs * numSubDevices, zeEuCountDesc.numTotalEUs);
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

    NEO::RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(*device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]);

    ze_device_memory_properties_t memProperties = {};
    ze_device_memory_ext_properties_t memExtProperties = {};
    memExtProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_EXT_PROPERTIES;
    memProperties.pNext = &memExtProperties;
    res = device->getMemoryProperties(&count, &memProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);
    const std::array<ze_device_memory_ext_type_t, 10> sysInfoMemType = {
        ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR4,
        ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR5,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
        ZE_DEVICE_MEMORY_EXT_TYPE_GDDR6,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
        ZE_DEVICE_MEMORY_EXT_TYPE_DDR5,
        ZE_DEVICE_MEMORY_EXT_TYPE_GDDR7,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
        ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
    };

    auto bandwidthPerNanoSecond = raii.mockProductHelper->getDeviceMemoryMaxBandWidthInBytesPerSecond(device->getHwInfo(), nullptr, 0) / 1000000000;

    UNRECOVERABLE_IF(hwInfo.gtSystemInfo.MemoryType >= sizeof(sysInfoMemType));
    EXPECT_EQ(memExtProperties.type, sysInfoMemType[hwInfo.gtSystemInfo.MemoryType]);
    EXPECT_EQ(memExtProperties.physicalSize, raii.mockProductHelper->getDeviceMemoryPhysicalSizeInBytes(nullptr, 0) * numSubDevices);
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

TEST_F(MultipleDevicesTest, whenCallingsetAtomicAccessAttributeForSystemAccessSharedCrossDeviceThenSuccessIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    L0::Device *device0 = driverHandle->devices[0];
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(true);
    debugManager.flags.EnableRecoverablePageFaults.set(true);
    debugManager.flags.EnableConcurrentSharedCrossP2PDeviceAccess.set(true);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_SYSTEM_ATOMICS;
    result = context->setAtomicAccessAttribute(device0->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MultipleDevicesDisabledImplicitScalingTest, givenTwoRootDevicesFromSameFamilyThenQueryPeerAccessSuccessfullyCompletes) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    GFXCORE_FAMILY device0Family = device0->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    GFXCORE_FAMILY device1Family = device1->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    EXPECT_EQ(device0Family, device1Family);

    bool canAccess = true;
    bool res = MockDeviceImp::queryPeerAccess(*device0->getNEODevice(), *device1->getNEODevice(), canAccess);
    EXPECT_TRUE(res);
}

HWTEST_F(MultipleDevicesDisabledImplicitScalingTest, givenTwoRootDevicesFromSameFamilyAndDeviceLostSynchronizeThenQueryPeerAccessReturnsFalse) {
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
        auto deviceInternalEngine = neoDevice->getInternalEngine();

        auto hwCsr = static_cast<CommandStreamReceiverHw<FamilyType> *>(deviceInternalEngine.commandStreamReceiver);
        auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(hwCsr);

        ultCsr->callBaseWaitForCompletionWithTimeout = false;
        ultCsr->returnWaitForCompletionWithTimeout = WaitStatus::gpuHang;
    }

    GFXCORE_FAMILY device0Family = devices[0]->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    GFXCORE_FAMILY device1Family = devices[1]->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    EXPECT_EQ(device0Family, device1Family);

    bool canAccess = true;
    bool res = MockDeviceImp::queryPeerAccess(*devices[0]->getNEODevice(), *devices[1]->getNEODevice(), canAccess);
    EXPECT_FALSE(res);
}

using DeviceGetStatusTest = Test<DeviceFixture>;
TEST_F(DeviceGetStatusTest, givenCallToDeviceGetStatusThenCorrectErrorCodeIsReturnedWhenResourcesHaveBeenReleased) {
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    ze_result_t res = device->getStatus();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    driverHandle->getSvmAllocsManager()->cleanupUSMAllocCaches();
    deviceImp->releaseResources();
    res = device->getStatus();
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, res);
}

struct DeviceGetStatusTestWithMockCsr : public DeviceGetStatusTest {

    void SetUp() override {
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<MockCommandStreamReceiver>();

        DeviceGetStatusTest::SetUp();
    }

    void TearDown() override {
        DeviceGetStatusTest::TearDown();
    }
};

TEST_F(DeviceGetStatusTestWithMockCsr, givenCallToDeviceGetStatusThenCorrectErrorCodeIsReturnedWhenGpuHangs) {
    ze_result_t res = device->getStatus();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto mockCSR = static_cast<MockCommandStreamReceiver *>(&neoDevice->getGpgpuCommandStreamReceiver());
    mockCSR->isGpuHangDetectedReturnValue = true;

    res = device->getStatus();

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, res);
}

using DeviceTests = Test<DeviceFixture>;

TEST_F(DeviceTests, WhenGettingMemoryAccessPropertiesThenSuccessIsReturned) {
    ze_device_memory_access_properties_t properties;
    auto result = device->getMemoryAccessProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &hwInfo = device->getHwInfo();
    auto &productHelper = device->getProductHelper();

    auto expectedHostAllocCapabilities = static_cast<ze_memory_access_cap_flags_t>(productHelper.getHostMemCapabilities(&hwInfo));
    EXPECT_EQ(expectedHostAllocCapabilities, properties.hostAllocCapabilities);

    auto expectedDeviceAllocCapabilities = static_cast<ze_memory_access_cap_flags_t>(productHelper.getDeviceMemCapabilities());
    EXPECT_EQ(expectedDeviceAllocCapabilities, properties.deviceAllocCapabilities);

    bool isKmdMigrationSupported{false};
    auto expectedSharedSingleDeviceAllocCapabilities = static_cast<ze_memory_access_cap_flags_t>(productHelper.getSingleDeviceSharedMemCapabilities(isKmdMigrationSupported));
    EXPECT_EQ(expectedSharedSingleDeviceAllocCapabilities, properties.sharedSingleDeviceAllocCapabilities);

    auto expectedSharedSystemAllocCapabilities = static_cast<ze_memory_access_cap_flags_t>(productHelper.getSharedSystemMemCapabilities(&hwInfo));
    EXPECT_EQ(expectedSharedSystemAllocCapabilities, properties.sharedSystemAllocCapabilities);
}

TEST(MultipleSubDevicesP2PTest, whenGettingP2PPropertiesBetweenSubDevicesThenPeerAccessIsAvailable) {

    DebugManagerStateRestore restorer;
    ze_device_p2p_properties_t p2pProperties = {};
    static constexpr uint32_t numSubDevices = 2;
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);

    auto executionEnvironment = std::make_unique<NEO::ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    auto hardwareInfo = *NEO::defaultHwInfo;

    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hardwareInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto deviceFactory = std::make_unique<NEO::UltDeviceFactory>(1, numSubDevices, *executionEnvironment.release());
    auto rootDevice = deviceFactory->rootDevices[0];
    EXPECT_NE(nullptr, rootDevice);
    EXPECT_EQ(numSubDevices, rootDevice->getNumSubDevices());

    auto driverHandle = std::make_unique<DriverHandleImp>();

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), rootDevice, false, &returnValue));
    EXPECT_NE(nullptr, device);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto deviceImp = static_cast<DeviceImp *>(device.get());
    EXPECT_EQ(numSubDevices, deviceImp->numSubDevices);

    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceImp->subDevices[0]->getP2PProperties(deviceImp->subDevices[1], &p2pProperties));

    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

template <bool p2pAccess, bool p2pAtomicAccess>
struct MultipleDevicesP2PFixture : public ::testing::Test {
    void SetUp() override {

        VariableBackup<bool> mockDeviceFlagBackup(&NEO::MockDevice::createSingleDevice, false);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);

        NEO::HardwareInfo hardwareInfo = *NEO::defaultHwInfo;

        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hardwareInfo);
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        executionEnvironment->rootDeviceEnvironments[1]->setHwInfoAndInitHelpers(&hardwareInfo);
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
            context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
            context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
        }
        driverHandle->devices[0]->getNEODevice()->crossAccessEnabledDevices[1] = p2pAccess;
        driverHandle->devices[1]->getNEODevice()->crossAccessEnabledDevices[0] = p2pAccess;
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    MockMemoryManagerMultiDevice *memoryManager = nullptr;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    std::unique_ptr<ContextImp> context;

    const uint32_t numRootDevices = 2u;
    const uint32_t numSubDevices = 2u;
};

using MemoryAccessPropertiesSharedCrossDeviceCapsTest = Test<DeviceFixture>;
TEST_F(MemoryAccessPropertiesSharedCrossDeviceCapsTest,
       WhenCallingGetMemoryAccessPropertiesWithDevicesHavingKmdMigrationsSupportAndCrossP2PSharedAccessKeyNotSetThenNoSupportIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableRecoverablePageFaults.set(1);
    debugManager.flags.UseKmdMigration.set(1);
    L0::Device *device = driverHandle->devices[0];
    ze_device_memory_access_properties_t properties;
    auto result = device->getMemoryAccessProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_memory_access_cap_flags_t expectedSharedCrossDeviceAllocCapabilities = {};
    EXPECT_EQ(expectedSharedCrossDeviceAllocCapabilities, properties.sharedCrossDeviceAllocCapabilities);
}

TEST_F(MemoryAccessPropertiesSharedCrossDeviceCapsTest,
       WhenCallingGetMemoryAccessPropertiesWithDevicesHavingNoKmdMigrationsSupportAndEnableCrossP2PSharedAccessKeyThenNoSupportIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableRecoverablePageFaults.set(1);
    debugManager.flags.UseKmdMigration.set(0);
    debugManager.flags.EnableConcurrentSharedCrossP2PDeviceAccess.set(1);

    L0::Device *device = driverHandle->devices[0];
    ze_device_memory_access_properties_t properties;
    auto result = device->getMemoryAccessProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_memory_access_cap_flags_t expectedSharedCrossDeviceAllocCapabilities = {};
    EXPECT_EQ(expectedSharedCrossDeviceAllocCapabilities, properties.sharedCrossDeviceAllocCapabilities);
}
TEST_F(MemoryAccessPropertiesSharedCrossDeviceCapsTest,
       WhenCallingGetMemoryAccessPropertiesWithDevicesHavingNoRecoverablePageFaultSupportAndEnableCrossP2PSharedAccessKeyThenNoSupportIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableRecoverablePageFaults.set(0);
    debugManager.flags.UseKmdMigration.set(1);
    debugManager.flags.EnableConcurrentSharedCrossP2PDeviceAccess.set(1);

    L0::Device *device = driverHandle->devices[0];
    ze_device_memory_access_properties_t properties;
    auto result = device->getMemoryAccessProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_memory_access_cap_flags_t expectedSharedCrossDeviceAllocCapabilities = {};
    EXPECT_EQ(expectedSharedCrossDeviceAllocCapabilities, properties.sharedCrossDeviceAllocCapabilities);
}

TEST_F(MemoryAccessPropertiesSharedCrossDeviceCapsTest,
       WhenCallingGetMemoryAccessPropertiesWithDevicesHavingKmdMigrationsAndRecoverablePageFaultSupportAndEnableCrossP2PSharedAccessKeyThenSupportIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableRecoverablePageFaults.set(1);
    debugManager.flags.UseKmdMigration.set(1);
    debugManager.flags.EnableConcurrentSharedCrossP2PDeviceAccess.set(1);

    L0::Device *device = driverHandle->devices[0];
    ze_device_memory_access_properties_t properties;
    auto result = device->getMemoryAccessProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_memory_access_cap_flags_t expectedSharedCrossDeviceAllocCapabilities =
        ZE_MEMORY_ACCESS_CAP_FLAG_RW | ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT |
        ZE_MEMORY_ACCESS_CAP_FLAG_ATOMIC | ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT_ATOMIC;
    EXPECT_EQ(expectedSharedCrossDeviceAllocCapabilities, properties.sharedCrossDeviceAllocCapabilities);
}

struct MemoryAccessPropertiesMultiDevicesSingleRootDeviceTest : public Test<MultiDeviceFixtureHierarchy> {
    void SetUp() override {
        this->deviceHierarchyMode = NEO::DeviceHierarchyMode::flat;
        this->numRootDevices = 1;
        MultiDeviceFixtureHierarchy::setUp();
    }
};
TEST_F(MemoryAccessPropertiesMultiDevicesSingleRootDeviceTest,
       givenSingleRootAndMultiDevicesWhenCallingGetMemoryAccessPropertiesThenSharedCrossDeviceAccessSupportIsReturned) {
    L0::Device *device = driverHandle->devices[0];
    ze_device_memory_access_properties_t properties;
    auto result = device->getMemoryAccessProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_memory_access_cap_flags_t expectedSharedCrossDeviceAllocCapabilities =
        ZE_MEMORY_ACCESS_CAP_FLAG_RW | ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT |
        ZE_MEMORY_ACCESS_CAP_FLAG_ATOMIC | ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT_ATOMIC;
    EXPECT_EQ(expectedSharedCrossDeviceAllocCapabilities, properties.sharedCrossDeviceAllocCapabilities);
}

struct MemoryAccessPropertiesMultiDevicesMultiRootDeviceTest : public Test<MultiDeviceFixtureHierarchy> {
    void SetUp() override {
        this->deviceHierarchyMode = NEO::DeviceHierarchyMode::flat;
        this->numRootDevices = 2;
        MultiDeviceFixtureHierarchy::setUp();
    }
};
TEST_F(MemoryAccessPropertiesMultiDevicesMultiRootDeviceTest,
       givenMultiRootAndMultiSubDevicesWhenCallingGetMemoryAccessPropertiesThenSharedCrossDeviceAccessSupportIsNotExposed) {
    L0::Device *device = driverHandle->devices[0];
    ze_device_memory_access_properties_t properties;
    auto result = device->getMemoryAccessProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_memory_access_cap_flags_t expectedSharedCrossDeviceAllocCapabilities = 0;
    EXPECT_EQ(expectedSharedCrossDeviceAllocCapabilities, properties.sharedCrossDeviceAllocCapabilities);
}

struct MemoryAccessPropertiesSingleExposedDeviceTest : public Test<MultiDeviceFixtureHierarchy> {
    void SetUp() override {
        this->deviceHierarchyMode = NEO::DeviceHierarchyMode::composite;
        this->numRootDevices = 1;
        MultiDeviceFixtureHierarchy::setUp();
    }
};

TEST_F(MemoryAccessPropertiesSingleExposedDeviceTest,
       givenSingleExposedDevicesWhenCallingGetMemoryAccessPropertiesForRootDeviceThenSharedCrossDeviceAccessSupportIsNotExposed) {
    L0::Device *device = driverHandle->devices[0];
    ze_device_memory_access_properties_t properties;
    auto result = device->getMemoryAccessProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_memory_access_cap_flags_t expectedSharedCrossDeviceAllocCapabilities = 0;
    EXPECT_EQ(expectedSharedCrossDeviceAllocCapabilities, properties.sharedCrossDeviceAllocCapabilities);
}

TEST_F(MemoryAccessPropertiesSingleExposedDeviceTest,
       givenSingleExposedDevicesWhenCallingGetMemoryAccessPropertiesForSubDeviceThenSharedCrossDeviceAccessSupportIsExposed) {
    L0::Device *rootDevice = driverHandle->devices[0];
    auto device = static_cast<L0::DeviceImp *>(rootDevice)->subDevices[0];
    ze_device_memory_access_properties_t properties;
    auto result = device->getMemoryAccessProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_memory_access_cap_flags_t expectedSharedCrossDeviceAllocCapabilities =
        ZE_MEMORY_ACCESS_CAP_FLAG_RW | ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT |
        ZE_MEMORY_ACCESS_CAP_FLAG_ATOMIC | ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT_ATOMIC;
    EXPECT_EQ(expectedSharedCrossDeviceAllocCapabilities, properties.sharedCrossDeviceAllocCapabilities);
}

using MultipleDevicesP2PDevice0Access0Atomic0Device1Access0Atomic0Test = MultipleDevicesP2PFixture<0, 0>;
TEST_F(MultipleDevicesP2PDevice0Access0Atomic0Device1Access0Atomic0Test, WhenCallingGetP2PPropertiesWithBothDevicesHavingNoAccessSupportThenNoSupportIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_device_p2p_properties_t p2pProperties = {};
    device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

TEST_F(MultipleDevicesP2PDevice0Access0Atomic0Device1Access0Atomic0Test, WhenCallingGetP2PPropertiesWithBandwidthPropertiesExtensionThenPropertiesSet) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_device_p2p_properties_t p2pProperties = {};
    ze_device_p2p_bandwidth_exp_properties_t p2pBandwidthProps = {};

    p2pProperties.pNext = &p2pBandwidthProps;
    p2pBandwidthProps.stype = ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES;
    p2pBandwidthProps.pNext = nullptr;

    device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_EQ(0u, p2pBandwidthProps.logicalBandwidth);
    EXPECT_EQ(0u, p2pBandwidthProps.physicalBandwidth);
    EXPECT_EQ(ZE_BANDWIDTH_UNIT_UNKNOWN, p2pBandwidthProps.bandwidthUnit);

    EXPECT_EQ(0u, p2pBandwidthProps.logicalLatency);
    EXPECT_EQ(0u, p2pBandwidthProps.physicalLatency);
    EXPECT_EQ(ZE_LATENCY_UNIT_UNKNOWN, p2pBandwidthProps.latencyUnit);
}

TEST_F(MultipleDevicesP2PDevice0Access0Atomic0Device1Access0Atomic0Test, WhenCallingGetP2PPropertiesWithPNextStypeNotSetThenGetP2PPropertiesReturnsSuccessfully) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_device_p2p_properties_t p2pProperties = {};
    ze_device_p2p_bandwidth_exp_properties_t p2pBandwidthProps = {};

    p2pProperties.pNext = &p2pBandwidthProps;

    auto res = device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

using MultipleDevicesP2PDevice0Access1Atomic0Device1Access1Atomic0Test = MultipleDevicesP2PFixture<1, 0>;
TEST_F(MultipleDevicesP2PDevice0Access1Atomic0Device1Access1Atomic0Test, WhenCallingGetP2PPropertiesWithBothDevicesHavingAccessSupportThenSupportIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_device_p2p_properties_t p2pProperties = {};
    device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

using MultipleDevicesP2PDevice0Access1Atomic1Device1Access1Atomic1Test = MultipleDevicesP2PFixture<1, 1>;
TEST_F(MultipleDevicesP2PDevice0Access1Atomic1Device1Access1Atomic1Test, WhenCallingGetP2PPropertiesWithBothDevicesHavingAccessAndAtomicSupportThenSupportIsReturnedOnlyForAccess) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_device_p2p_properties_t p2pProperties = {};
    device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

template <bool p2pAccess, bool p2pAtomicAccess>
struct MultipleDevicesP2PWithXeLinkFixture : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);

        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        EXPECT_EQ(numRootDevices, executionEnvironment->rootDeviceEnvironments.size());

        auto hwInfo0 = *NEO::defaultHwInfo;
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo0);
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        auto hwInfo1 = *NEO::defaultHwInfo;
        executionEnvironment->rootDeviceEnvironments[1]->setHwInfoAndInitHelpers(&hwInfo1);
        executionEnvironment->rootDeviceEnvironments[1]->initGmm();

        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[0]));
        devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[1]));

        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

        driverHandle->initializeVertexes();

        uint32_t subDeviceCount = 2u;
        EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->devices[0]->getSubDevices(&subDeviceCount, device0SubDevices));
        EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->devices[1]->getSubDevices(&subDeviceCount, device1SubDevices));
        EXPECT_NE(nullptr, device0SubDevices[0]);
        EXPECT_NE(nullptr, device0SubDevices[1]);
        EXPECT_NE(nullptr, device1SubDevices[0]);
        EXPECT_NE(nullptr, device1SubDevices[1]);

        static_cast<L0::Device *>(device0SubDevices[0])->getNEODevice()->crossAccessEnabledDevices[1] = p2pAccess;
        static_cast<L0::Device *>(device0SubDevices[1])->getNEODevice()->crossAccessEnabledDevices[1] = p2pAccess;
        static_cast<L0::Device *>(device1SubDevices[0])->getNEODevice()->crossAccessEnabledDevices[0] = p2pAccess;
        static_cast<L0::Device *>(device1SubDevices[1])->getNEODevice()->crossAccessEnabledDevices[0] = p2pAccess;

        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[0])->getFabricVertex(&vertex0SubVertices[0]));
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[1])->getFabricVertex(&vertex0SubVertices[1]));
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device1SubDevices[0])->getFabricVertex(&vertex1SubVertices[0]));
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device1SubDevices[1])->getFabricVertex(&vertex1SubVertices[1]));
        EXPECT_NE(nullptr, vertex0SubVertices[0]);
        EXPECT_NE(nullptr, vertex0SubVertices[1]);
        EXPECT_NE(nullptr, vertex1SubVertices[0]);
        EXPECT_NE(nullptr, vertex1SubVertices[1]);

        const char *mdfiModel = "MDFI";
        const char *xeLinkModel = "XeLink";
        const char *xeLinkMdfiModel = "XeLink-MDFI";
        const char *mdfiXeLinkModel = "MDFI-XeLink";
        const char *mdfiXeLinkMdfiModel = "MDFI-XeLink-MDFI";
        const uint32_t xeLinkBandwidth = 16;

        // MDFI in device 0
        auto edgeMdfi0 = new FabricEdge;
        edgeMdfi0->vertexA = FabricVertex::fromHandle(vertex0SubVertices[0]);
        edgeMdfi0->vertexB = FabricVertex::fromHandle(vertex0SubVertices[1]);
        memcpy_s(edgeMdfi0->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, mdfiModel, strlen(mdfiModel));
        edgeMdfi0->properties.bandwidth = 0u;
        edgeMdfi0->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_UNKNOWN;
        edgeMdfi0->properties.latency = 0u;
        edgeMdfi0->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
        driverHandle->fabricEdges.push_back(edgeMdfi0);

        // MDFI in device 1
        auto edgeMdfi1 = new FabricEdge;
        edgeMdfi1->vertexA = FabricVertex::fromHandle(vertex1SubVertices[0]);
        edgeMdfi1->vertexB = FabricVertex::fromHandle(vertex1SubVertices[1]);
        memcpy_s(edgeMdfi1->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, mdfiModel, strlen(mdfiModel));
        edgeMdfi1->properties.bandwidth = 0u;
        edgeMdfi1->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_UNKNOWN;
        edgeMdfi1->properties.latency = 0u;
        edgeMdfi1->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
        driverHandle->fabricEdges.push_back(edgeMdfi1);

        // XeLink between 0.1 & 1.0
        auto edgeXeLink = new FabricEdge;
        edgeXeLink->vertexA = FabricVertex::fromHandle(vertex0SubVertices[1]);
        edgeXeLink->vertexB = FabricVertex::fromHandle(vertex1SubVertices[0]);
        memcpy_s(edgeXeLink->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, xeLinkModel, strlen(xeLinkModel));
        edgeXeLink->properties.bandwidth = p2pAtomicAccess ? xeLinkBandwidth : 0;
        edgeXeLink->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
        edgeXeLink->properties.latency = 1u;
        edgeXeLink->properties.latencyUnit = ZE_LATENCY_UNIT_HOP;
        driverHandle->fabricEdges.push_back(edgeXeLink);

        // MDFI-XeLink between 0.0 & 1.0
        auto edgeMdfiXeLink = new FabricEdge;
        edgeMdfiXeLink->vertexA = FabricVertex::fromHandle(vertex0SubVertices[0]);
        edgeMdfiXeLink->vertexB = FabricVertex::fromHandle(vertex1SubVertices[0]);
        memcpy_s(edgeMdfiXeLink->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, mdfiXeLinkModel, strlen(mdfiXeLinkModel));
        edgeMdfiXeLink->properties.bandwidth = p2pAtomicAccess ? xeLinkBandwidth : 0;
        edgeMdfiXeLink->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
        edgeMdfiXeLink->properties.latency = std::numeric_limits<uint32_t>::max();
        edgeMdfiXeLink->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
        driverHandle->fabricIndirectEdges.push_back(edgeMdfiXeLink);

        // MDFI-XeLink-MDFI between 0.0 & 1.1
        auto edgeMdfiXeLinkMdfi = new FabricEdge;
        edgeMdfiXeLinkMdfi->vertexA = FabricVertex::fromHandle(vertex0SubVertices[0]);
        edgeMdfiXeLinkMdfi->vertexB = FabricVertex::fromHandle(vertex1SubVertices[1]);
        memcpy_s(edgeMdfiXeLinkMdfi->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, mdfiXeLinkMdfiModel, strlen(mdfiXeLinkMdfiModel));
        edgeMdfiXeLinkMdfi->properties.bandwidth = p2pAtomicAccess ? xeLinkBandwidth : 0;
        edgeMdfiXeLinkMdfi->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
        edgeMdfiXeLinkMdfi->properties.latency = std::numeric_limits<uint32_t>::max();
        edgeMdfiXeLinkMdfi->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
        driverHandle->fabricIndirectEdges.push_back(edgeMdfiXeLinkMdfi);

        // XeLink-MDFI between 0.1 & 1.1
        auto edgeXeLinkMdfi = new FabricEdge;
        edgeXeLinkMdfi->vertexA = FabricVertex::fromHandle(vertex0SubVertices[1]);
        edgeXeLinkMdfi->vertexB = FabricVertex::fromHandle(vertex1SubVertices[1]);
        memcpy_s(edgeXeLinkMdfi->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, xeLinkMdfiModel, strlen(xeLinkMdfiModel));
        edgeXeLinkMdfi->properties.bandwidth = p2pAtomicAccess ? xeLinkBandwidth : 0;
        edgeXeLinkMdfi->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
        edgeXeLinkMdfi->properties.latency = std::numeric_limits<uint32_t>::max();
        edgeXeLinkMdfi->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
        driverHandle->fabricIndirectEdges.push_back(edgeXeLinkMdfi);
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    static constexpr uint32_t numRootDevices = 2u;
    static constexpr uint32_t numSubDevices = 2u;
    ze_device_handle_t device0SubDevices[numSubDevices];
    ze_device_handle_t device1SubDevices[numSubDevices];
    ze_fabric_vertex_handle_t vertex0SubVertices[numSubDevices];
    ze_fabric_vertex_handle_t vertex1SubVertices[numSubDevices];
};

using MultipleDevicesP2PWithXeLinkDevice0Access0Atomic0Device1Access0Atomic0Test = MultipleDevicesP2PWithXeLinkFixture<false, false>;
TEST_F(MultipleDevicesP2PWithXeLinkDevice0Access0Atomic0Device1Access0Atomic0Test, WhenCallingGetP2PPropertiesWithNoDeviceHavingAccessSupportThenNoDeviceHasP2PAccessSupport) {
    auto subDevice00 = DeviceImp::fromHandle(device0SubDevices[0]);
    auto subDevice01 = DeviceImp::fromHandle(device0SubDevices[1]);
    auto subDevice10 = DeviceImp::fromHandle(device1SubDevices[0]);
    auto subDevice11 = DeviceImp::fromHandle(device1SubDevices[1]);

    ze_device_p2p_properties_t p2pProperties;

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice00->getP2PProperties(subDevice10, &p2pProperties));
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice00->getP2PProperties(subDevice11, &p2pProperties));
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice01->getP2PProperties(subDevice10, &p2pProperties));
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice01->getP2PProperties(subDevice11, &p2pProperties));
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

using MultipleDevicesP2PWithXeLinkDevice0Access1Atomic0Device1Access1Atomic0Test = MultipleDevicesP2PWithXeLinkFixture<true, false>;
TEST_F(MultipleDevicesP2PWithXeLinkDevice0Access1Atomic0Device1Access1Atomic0Test, WhenCallingGetP2PPropertiesWithBothDeviceHavingAccessSupportThenBothDevicesHaveP2PAccessSupport) {
    auto subDevice00 = DeviceImp::fromHandle(device0SubDevices[0]);
    auto subDevice01 = DeviceImp::fromHandle(device0SubDevices[1]);
    auto subDevice10 = DeviceImp::fromHandle(device1SubDevices[0]);
    auto subDevice11 = DeviceImp::fromHandle(device1SubDevices[1]);

    ze_device_p2p_properties_t p2pProperties;

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice00->getP2PProperties(subDevice10, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice00->getP2PProperties(subDevice11, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice01->getP2PProperties(subDevice10, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice01->getP2PProperties(subDevice11, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_FALSE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

using MultipleDevicesP2PWithXeLinkDevice0Access1Atomic1Device1Access1Atomic1Test = MultipleDevicesP2PWithXeLinkFixture<true, true>;
TEST_F(MultipleDevicesP2PWithXeLinkDevice0Access1Atomic1Device1Access1Atomic1Test, WhenCallingGetP2PPropertiesWithBothDevicesHavingAccessAndAtomicSupportThenCorrectSupportIsReturned) {
    auto subDevice00 = DeviceImp::fromHandle(device0SubDevices[0]);
    auto subDevice01 = DeviceImp::fromHandle(device0SubDevices[1]);
    auto subDevice10 = DeviceImp::fromHandle(device1SubDevices[0]);
    auto subDevice11 = DeviceImp::fromHandle(device1SubDevices[1]);

    ze_device_p2p_properties_t p2pProperties;

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice00->getP2PProperties(subDevice01, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice00->getP2PProperties(subDevice10, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice00->getP2PProperties(subDevice11, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice01->getP2PProperties(subDevice10, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice01->getP2PProperties(subDevice11, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);

    p2pProperties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, subDevice10->getP2PProperties(subDevice11, &p2pProperties));
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS);
    EXPECT_TRUE(p2pProperties.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS);
}

TEST_F(MultipleDevicesDisabledImplicitScalingTest, givenTwoRootDevicesFromSameFamilyThenQueryPeerAccessReturnsTrue) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    GFXCORE_FAMILY device0Family = device0->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    GFXCORE_FAMILY device1Family = device1->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    EXPECT_EQ(device0Family, device1Family);

    bool canAccess = false;
    bool res = MockDeviceImp::queryPeerAccess(*device0->getNEODevice(), *device1->getNEODevice(), canAccess);
    EXPECT_TRUE(res);
    EXPECT_TRUE(canAccess);
}

TEST_F(MultipleDevicesDisabledImplicitScalingTest, givenQueryPeerAccessCalledTwiceThenQueryPeerAccessReturnsSameValueEachTime) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    GFXCORE_FAMILY device0Family = device0->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    GFXCORE_FAMILY device1Family = device1->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    EXPECT_EQ(device0Family, device1Family);

    bool canAccess = false;
    bool res = MockDeviceImp::queryPeerAccess(*device0->getNEODevice(), *device1->getNEODevice(), canAccess);
    EXPECT_TRUE(res);
    EXPECT_TRUE(canAccess);

    res = MockDeviceImp::queryPeerAccess(*device0->getNEODevice(), *device1->getNEODevice(), canAccess);
    EXPECT_TRUE(res);
    EXPECT_TRUE(canAccess);
}

TEST_F(MultipleDevicesTest, givenDeviceFailsAppendMemoryCopyThenQueryPeerAccessReturnsFalse) {
    struct MockDeviceFail : public MockDeviceImp {
        MockDeviceFail(L0::Device *device) : MockDeviceImp(device->getNEODevice()) {
            this->driverHandle = device->getDriverHandle();
            this->commandList.appendMemoryCopyResult = ZE_RESULT_ERROR_UNKNOWN;
            this->neoDevice->setSpecializedDevice<L0::Device>(this);
        }

        ze_result_t queryFabricStats(DeviceImp *pPeerDevice, uint32_t &latency, uint32_t &bandwidth) override {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
        ze_result_t createInternalCommandQueue(const ze_command_queue_desc_t *desc,
                                               ze_command_queue_handle_t *commandQueue) override {
            *commandQueue = &this->commandQueue;
            return ZE_RESULT_SUCCESS;
        }

        ze_result_t createInternalCommandList(const ze_command_list_desc_t *desc,
                                              ze_command_list_handle_t *commandList) override {
            *commandList = &this->commandList;
            return ZE_RESULT_SUCCESS;
        }

        MockCommandList commandList;
        Mock<CommandQueue> commandQueue;
    };

    MockDeviceFail *device0 = new MockDeviceFail(driverHandle->devices[0]);
    L0::Device *device1 = driverHandle->devices[1];

    bool canAccess = false;
    bool res = MockDeviceImp::queryPeerAccess(*device0->getNEODevice(), *device1->getNEODevice(), canAccess);
    EXPECT_GT(device0->commandList.appendMemoryCopyCalled, 0u);
    EXPECT_TRUE(res);
    EXPECT_FALSE(canAccess);
    delete device0;
}

TEST_F(MultipleDevicesTest, givenCanAccessPeerSucceedsThenReturnsSuccessAndCorrectValue) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_bool_t canAccess = false;
    ze_result_t res = device0->canAccessPeer(device1->toHandle(), &canAccess);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(canAccess);
}

TEST_F(MultipleDevicesTest, givenCanAccessPeerFailsThenReturnsDeviceLost) {
    struct MockDeviceFail : public MockDeviceImp {
        struct MockCommandQueueImp : public Mock<CommandQueue> {
            ze_result_t synchronize(uint64_t timeout) override {
                return ZE_RESULT_ERROR_DEVICE_LOST;
            }
        };

        MockDeviceFail(L0::Device *device) : MockDeviceImp(device->getNEODevice()) {
            this->driverHandle = device->getDriverHandle();
            this->neoDevice->setSpecializedDevice<L0::Device>(this);
        }

        ze_result_t queryFabricStats(DeviceImp *pPeerDevice, uint32_t &latency, uint32_t &bandwidth) override {
            bandwidth = 0;
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
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

        ze_result_t createInternalCommandQueue(const ze_command_queue_desc_t *desc,
                                               ze_command_queue_handle_t *commandQueue) override {
            *commandQueue = &this->commandQueue;
            return ZE_RESULT_SUCCESS;
        }

        ze_result_t createInternalCommandList(const ze_command_list_desc_t *desc,
                                              ze_command_list_handle_t *commandList) override {
            *commandList = &this->commandList;
            return ZE_RESULT_SUCCESS;
        }

        MockCommandList commandList;
        MockCommandQueueImp commandQueue;
    };

    MockDeviceFail *device0 = new MockDeviceFail(driverHandle->devices[0]);
    L0::Device *device1 = driverHandle->devices[1];

    ze_bool_t canAccess = true;
    ze_result_t res = device0->canAccessPeer(device1->toHandle(), &canAccess);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, res);
    EXPECT_FALSE(canAccess);
    delete device0;
}

HWTEST_F(MultipleDevicesTest, givenCsrModeDifferentThanHardwareWhenQueryPeerAccessThenReturnsFalse) {
    struct MockDeviceFail : public MockDeviceImp {
        MockDeviceFail(L0::Device *device) : MockDeviceImp(device->getNEODevice()) {
            this->driverHandle = device->getDriverHandle();
            this->neoDevice->template setSpecializedDevice<L0::Device>(this);
        }

        ze_result_t queryFabricStats(DeviceImp *pPeerDevice, uint32_t &latency, uint32_t &bandwidth) override {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
        ze_result_t createInternalCommandQueue(const ze_command_queue_desc_t *desc,
                                               ze_command_queue_handle_t *commandQueue) override {
            *commandQueue = &this->commandQueue;
            return ZE_RESULT_SUCCESS;
        }

        ze_result_t createInternalCommandList(const ze_command_list_desc_t *desc,
                                              ze_command_list_handle_t *commandList) override {
            *commandList = &this->commandList;
            return ZE_RESULT_SUCCESS;
        }

        MockCommandList commandList;
        Mock<CommandQueue> commandQueue;
    };

    MockDeviceFail *device0 = new MockDeviceFail(driverHandle->devices[0]);
    L0::Device *device1 = driverHandle->devices[1];

    auto deviceInternalEngine = device0->getNEODevice()->getInternalEngine();
    auto hwCsr = static_cast<CommandStreamReceiverHw<FamilyType> *>(deviceInternalEngine.commandStreamReceiver);
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(hwCsr);
    ultCsr->commandStreamReceiverType = CommandStreamReceiverType::tbx;

    bool canAccess = false;
    bool res = MockDeviceImp::queryPeerAccess(*device0->getNEODevice(), *device1->getNEODevice(), canAccess);
    EXPECT_FALSE(res);
    EXPECT_FALSE(canAccess);
    delete device0;
}

TEST_F(MultipleDevicesTest, givenDeviceFailsExecuteCommandListThenQueryPeerAccessReturnsFalse) {
    struct MockDeviceFail : public MockDeviceImp {
        struct MockCommandQueueImp : public Mock<CommandQueue> {
            ze_result_t destroy() override {
                return ZE_RESULT_SUCCESS;
            }

            ze_result_t executeCommandLists(uint32_t numCommandLists,
                                            ze_command_list_handle_t *phCommandLists,
                                            ze_fence_handle_t hFence, bool performMigration,
                                            NEO::LinearStream *parentImmediateCommandlistLinearStream,
                                            std::unique_lock<std::mutex> *outerLockForIndirect)
                override { return ZE_RESULT_ERROR_UNKNOWN; }
        };

        MockDeviceFail(L0::Device *device) : MockDeviceImp(device->getNEODevice()) {
            this->driverHandle = device->getDriverHandle();
            this->neoDevice->setSpecializedDevice<L0::Device>(this);
        }

        ze_result_t queryFabricStats(DeviceImp *pPeerDevice, uint32_t &latency, uint32_t &bandwidth) override {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
        ze_result_t createInternalCommandQueue(const ze_command_queue_desc_t *desc,
                                               ze_command_queue_handle_t *commandQueue) override {
            *commandQueue = &this->commandQueue;
            return ZE_RESULT_SUCCESS;
        }

        ze_result_t createInternalCommandList(const ze_command_list_desc_t *desc,
                                              ze_command_list_handle_t *commandList) override {
            *commandList = &this->commandList;
            return ZE_RESULT_SUCCESS;
        }

        MockCommandList commandList;
        MockCommandQueueImp commandQueue;
    };

    MockDeviceFail *device0 = new MockDeviceFail(driverHandle->devices[0]);
    L0::Device *device1 = driverHandle->devices[1];

    bool canAccess = false;
    bool res = MockDeviceImp::queryPeerAccess(*device0->getNEODevice(), *device1->getNEODevice(), canAccess);
    EXPECT_TRUE(res);
    EXPECT_FALSE(canAccess);
    delete device0;
}

TEST_F(MultipleDevicesTest, givenQueryFabricStatsReturningBandwidthZeroAndDeviceFailsThenQueryPeerAccessReturnsFalse) {
    struct MockDeviceFail : public MockDeviceImp {
        struct MockCommandQueueImp : public Mock<CommandQueue> {
            ze_result_t destroy() override {
                return ZE_RESULT_SUCCESS;
            }

            ze_result_t executeCommandLists(uint32_t numCommandLists,
                                            ze_command_list_handle_t *phCommandLists,
                                            ze_fence_handle_t hFence, bool performMigration,
                                            NEO::LinearStream *parentImmediateCommandlistLinearStream,
                                            std::unique_lock<std::mutex> *outerLockForIndirect)
                override { return ZE_RESULT_ERROR_UNKNOWN; }
        };

        MockDeviceFail(L0::Device *device) : MockDeviceImp(device->getNEODevice()) {
            this->driverHandle = device->getDriverHandle();
            this->neoDevice->setSpecializedDevice<L0::Device>(this);
        }

        ze_result_t queryFabricStats(DeviceImp *pPeerDevice, uint32_t &latency, uint32_t &bandwidth) override {
            bandwidth = 0;
            return ZE_RESULT_SUCCESS;
        }

        ze_result_t createCommandQueue(const ze_command_queue_desc_t *desc,
                                       ze_command_queue_handle_t *commandQueue) override {
            *commandQueue = &this->commandQueue;
            return ZE_RESULT_SUCCESS;
        }

        ze_result_t createInternalCommandList(const ze_command_list_desc_t *desc,
                                              ze_command_list_handle_t *commandList) override {
            *commandList = &this->commandList;
            return ZE_RESULT_SUCCESS;
        }
        ze_result_t createInternalCommandQueue(const ze_command_queue_desc_t *desc,
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

    bool canAccess = false;
    bool res = MockDeviceImp::queryPeerAccess(*device0->getNEODevice(), *device1->getNEODevice(), canAccess);
    EXPECT_TRUE(res);
    EXPECT_FALSE(canAccess);
    delete device0;
}

TEST_F(MultipleDevicesTest, givenQueryFabricStatsReturningBandwidthNonZeroAndDeviceDoesFailThenQueryPeerAccessReturnsTrue) {
    struct MockDeviceFail : public MockDeviceImp {
        struct MockCommandQueueImp : public Mock<CommandQueue> {
            ze_result_t destroy() override {
                return ZE_RESULT_SUCCESS;
            }

            ze_result_t executeCommandLists(uint32_t numCommandLists,
                                            ze_command_list_handle_t *phCommandLists,
                                            ze_fence_handle_t hFence, bool performMigration,
                                            NEO::LinearStream *parentImmediateCommandlistLinearStream,
                                            std::unique_lock<std::mutex> *outerLockForIndirect) override {
                return ZE_RESULT_SUCCESS;
            }
        };

        MockDeviceFail(L0::Device *device) : MockDeviceImp(device->getNEODevice()) {
            this->driverHandle = device->getDriverHandle();
            this->neoDevice->setSpecializedDevice<L0::Device>(this);
        }

        ze_result_t queryFabricStats(DeviceImp *pPeerDevice, uint32_t &latency, uint32_t &bandwidth) override {
            bandwidth = 100;
            return ZE_RESULT_SUCCESS;
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

        ze_result_t createInternalCommandQueue(const ze_command_queue_desc_t *desc,
                                               ze_command_queue_handle_t *commandQueue) override {
            *commandQueue = &this->commandQueue;
            return ZE_RESULT_SUCCESS;
        }

        ze_result_t createInternalCommandList(const ze_command_list_desc_t *desc,
                                              ze_command_list_handle_t *commandList) override {
            *commandList = &this->commandList;
            return ZE_RESULT_SUCCESS;
        }

        MockCommandList commandList;
        MockCommandQueueImp commandQueue;
    };

    MockDeviceFail *device0 = new MockDeviceFail(driverHandle->devices[0]);
    L0::Device *device1 = driverHandle->devices[1];

    bool canAccess = false;
    bool res = MockDeviceImp::queryPeerAccess(*device0->getNEODevice(), *device1->getNEODevice(), canAccess);
    EXPECT_TRUE(res);
    EXPECT_TRUE(canAccess);
    delete device0;
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

    ASSERT_NE(0u, hwInfo.gtSystemInfo.SliceCount);

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

struct MultipleDevicesAffinityTest : MultipleDevicesFixture<-1> {
    void SetUp() override {
        debugManager.flags.ZE_AFFINITY_MASK.set("0.1,1.0");
        MultipleDevicesFixture<-1>::SetUp();
    }

    void TearDown() override {
        MultipleDevicesFixture<-1>::TearDown();
    }

    DebugManagerStateRestore restorer;
};
TEST_F(MultipleDevicesAffinityTest, givenAffinityMaskRootDeviceCorrespondingToTileWhenGettingPhysicalSliceIdThenCorrectSliceIdAndDeviceIndexIsReturned) {
    ASSERT_EQ(2u, driverHandle->devices.size());

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];
    auto hwInfo = device0->getHwInfo();

    uint32_t subDeviceCount = numSubDevices;
    std::vector<ze_device_handle_t> subDevices0(subDeviceCount);
    auto res = device0->getSubDevices(&subDeviceCount, subDevices0.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(0u, subDeviceCount);

    L0::DeviceImp *deviceImp0 = static_cast<DeviceImp *>(device0);
    L0::DeviceImp *deviceImp1 = static_cast<DeviceImp *>(device1);

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
    auto ret = deviceImp0->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

    EXPECT_TRUE(ret);
    EXPECT_EQ(10u, sliceId);
    EXPECT_EQ(1u, subsliceId);
    EXPECT_EQ(1u, deviceIndex);

    sliceId = 0;
    subsliceId = 1;
    deviceIndex = 100;
    ret = deviceImp1->toPhysicalSliceId(map, sliceId, subsliceId, deviceIndex);

    EXPECT_TRUE(ret);
    EXPECT_EQ(5u, sliceId);
    EXPECT_EQ(1u, subsliceId);
    EXPECT_EQ(0u, deviceIndex);
}

TEST_F(MultipleDevicesAffinityTest, givenAffinityMaskRootDeviceCorrespondingToTileWhenGettingApiSliceIdThenCorrectSliceIdsReturned) {
    ASSERT_EQ(2u, driverHandle->devices.size());

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];
    auto hwInfo = device0->getHwInfo();

    uint32_t subDeviceCount = numSubDevices;
    std::vector<ze_device_handle_t> subDevices0(subDeviceCount);
    auto res = device0->getSubDevices(&subDeviceCount, subDevices0.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(0u, subDeviceCount);

    L0::DeviceImp *deviceImp0 = static_cast<DeviceImp *>(device0);
    L0::DeviceImp *deviceImp1 = static_cast<DeviceImp *>(device1);

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

    uint32_t sliceId = 10;
    uint32_t subsliceId = 0;
    uint32_t tileIndex = 1;
    auto ret = deviceImp0->toApiSliceId(map, sliceId, subsliceId, tileIndex);

    EXPECT_TRUE(ret);
    EXPECT_EQ(0u, sliceId);

    sliceId = 0;
    tileIndex = 0;
    ret = deviceImp1->toApiSliceId(map, sliceId, subsliceId, tileIndex);

    EXPECT_TRUE(ret);
    EXPECT_EQ(0u, sliceId);
}

TEST_F(MultipleDevicesAffinityTest, givenAffinityMaskRootDeviceCorrespondingToTileWhenMappingToAndFromApiAndPhysicalSliceIdThenIdsAreMatching) {
    ASSERT_EQ(2u, driverHandle->devices.size());

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];
    auto hwInfo = device0->getHwInfo();

    uint32_t subDeviceCount = numSubDevices;
    std::vector<ze_device_handle_t> subDevices0(subDeviceCount);
    auto res = device0->getSubDevices(&subDeviceCount, subDevices0.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(0u, subDeviceCount);

    L0::DeviceImp *deviceImp0 = static_cast<DeviceImp *>(device0);
    L0::DeviceImp *deviceImp1 = static_cast<DeviceImp *>(device1);

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

    uint32_t tileIndex = 1;

    ze_device_properties_t deviceProperties = {};
    deviceImp0->getProperties(&deviceProperties);

    for (uint32_t i = 0; i < deviceProperties.numSlices; i++) {
        uint32_t sliceId = i;
        uint32_t subsliceId = deviceProperties.numSubslicesPerSlice / 2;
        auto ret = deviceImp0->toPhysicalSliceId(map, sliceId, subsliceId, tileIndex);

        EXPECT_TRUE(ret);
        EXPECT_EQ(i + 1, sliceId);
        EXPECT_EQ(1u, tileIndex);
        if (mapping.sliceIndices.size() == 1) {
            EXPECT_EQ(deviceProperties.numSubslicesPerSlice / 2 + 10u, subsliceId);
        } else {
            EXPECT_EQ(deviceProperties.numSubslicesPerSlice / 2, subsliceId);
        }

        ret = deviceImp0->toApiSliceId(map, sliceId, subsliceId, tileIndex);

        EXPECT_TRUE(ret);
        EXPECT_EQ(i, sliceId);
        EXPECT_EQ(deviceProperties.numSubslicesPerSlice / 2, subsliceId);
    }

    deviceImp1->getProperties(&deviceProperties);

    tileIndex = 0;
    for (uint32_t i = 0; i < deviceProperties.numSlices; i++) {
        uint32_t sliceId = i;
        uint32_t subsliceId = deviceProperties.numSubslicesPerSlice - 1;
        auto ret = deviceImp1->toPhysicalSliceId(map, sliceId, subsliceId, tileIndex);

        EXPECT_TRUE(ret);

        EXPECT_EQ(i, sliceId);
        EXPECT_EQ(0u, tileIndex);
        if (mapping.sliceIndices.size() == 1) {
            EXPECT_EQ(deviceProperties.numSubslicesPerSlice - 1 + 10u, subsliceId);
        } else {
            EXPECT_EQ(deviceProperties.numSubslicesPerSlice - 1, subsliceId);
        }

        ret = deviceImp1->toApiSliceId(map, sliceId, subsliceId, tileIndex);

        EXPECT_TRUE(ret);
        EXPECT_EQ(i, sliceId);
        EXPECT_EQ(deviceProperties.numSubslicesPerSlice - 1, subsliceId);
    }
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

TEST_F(DeviceTest, givenNoL0DebuggerWhenGettingL0DebuggerThenNullptrReturned) {
    EXPECT_EQ(nullptr, device->getL0Debugger());
}

TEST_F(DeviceTest, givenValidDeviceWhenCallingReleaseResourcesThenResourcesReleased) {
    auto deviceImp = static_cast<DeviceImp *>(device);
    EXPECT_FALSE(deviceImp->resourcesReleased);
    EXPECT_FALSE(nullptr == deviceImp->getNEODevice());
    driverHandle->getSvmAllocsManager()->cleanupUSMAllocCaches();
    deviceImp->releaseResources();
    EXPECT_TRUE(deviceImp->resourcesReleased);
    EXPECT_TRUE(nullptr == deviceImp->getNEODevice());
    EXPECT_TRUE(nullptr == deviceImp->globalTimestampCommandList);
    EXPECT_TRUE(nullptr == deviceImp->globalTimestampAllocation);
    EXPECT_TRUE(nullptr == deviceImp->pageFaultCommandList);
    deviceImp->releaseResources();
    EXPECT_TRUE(deviceImp->resourcesReleased);
}

TEST_F(DeviceTest, givenValidDeviceWhenCallingReleaseResourcesThenDirectSubmissionIsStopped) {
    auto deviceImp = static_cast<DeviceImp *>(device);
    EXPECT_FALSE(neoDevice->stopDirectSubmissionCalled);
    neoDevice->incRefInternal();
    driverHandle->getSvmAllocsManager()->cleanupUSMAllocCaches();
    deviceImp->releaseResources();
    EXPECT_TRUE(neoDevice->stopDirectSubmissionCalled);
    neoDevice->decRefInternal();
}

HWTEST_F(DeviceTest, givenCooperativeDispatchSupportedWhenQueryingPropertiesFlagsThenCooperativeKernelsAreSupported) {
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {
        bool isCooperativeDispatchSupported(const EngineGroupType engineGroupType, const RootDeviceEnvironment &rootDeviceEnvironment) const override {
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
    MockDeviceImp deviceImp(neoMockDevice);

    MockExecutionEnvironment mockExecutionEnvironment{&hwInfo};
    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*neoMockDevice->executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    NEO::EngineGroupType engineGroupTypes[] = {NEO::EngineGroupType::renderCompute, NEO::EngineGroupType::compute};
    for (auto isCooperativeDispatchSupported : ::testing::Bool()) {
        raii.mockGfxCoreHelper->isCooperativeDispatchSupportedValue = isCooperativeDispatchSupported;

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

HWTEST_F(DeviceTest, givenContextGroupSupportedWhenGettingLowPriorityCsrThenCorrectCsrAndContextIsReturned) {
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {

        const EngineInstancesContainer getGpgpuEngineInstances(const RootDeviceEnvironment &rootDeviceEnvironment) const override {
            auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
            EngineInstancesContainer engines;

            uint32_t numCcs = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

            if (hwInfo.featureTable.flags.ftrCCSNode) {
                for (uint32_t i = 0; i < numCcs; i++) {
                    auto engineType = static_cast<aub_stream::EngineType>(i + aub_stream::ENGINE_CCS);
                    engines.push_back({engineType, EngineUsage::regular});
                }

                engines.push_back({aub_stream::ENGINE_CCS, EngineUsage::lowPriority});
            }
            if (hwInfo.featureTable.flags.ftrRcsNode) {
                engines.push_back({aub_stream::ENGINE_RCS, EngineUsage::regular});
            }

            return engines;
        }
        EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const override {
            if (engineType == aub_stream::ENGINE_RCS) {
                return EngineGroupType::renderCompute;
            }
            if (engineType >= aub_stream::ENGINE_CCS && engineType < (aub_stream::ENGINE_CCS + hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled)) {
                return EngineGroupType::compute;
            }
            UNRECOVERABLE_IF(true);
        }
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.ContextGroupSize.set(8);

    const uint32_t rootDeviceIndex = 0u;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    MockExecutionEnvironment mockExecutionEnvironment{&hwInfo};
    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*mockExecutionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);

    {
        MockExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment{&hwInfo};
        auto *neoMockDevice = NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(&hwInfo, executionEnvironment, rootDeviceIndex);
        MockDeviceImp deviceImp(neoMockDevice);

        NEO::CommandStreamReceiver *lowPriorityCsr = nullptr;
        auto result = deviceImp.getCsrForLowPriority(&lowPriorityCsr, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, lowPriorityCsr);

        auto &secondaryEngines = neoMockDevice->secondaryEngines[aub_stream::EngineType::ENGINE_CCS];

        ASSERT_EQ(8u, secondaryEngines.engines.size());
        for (int i = 0; i < 8; i++) {
            EXPECT_NE(secondaryEngines.engines[i].osContext, &lowPriorityCsr->getOsContext());
            EXPECT_NE(secondaryEngines.engines[i].commandStreamReceiver, lowPriorityCsr);
        }

        EXPECT_FALSE(lowPriorityCsr->getOsContext().isPartOfContextGroup());
        EXPECT_EQ(nullptr, lowPriorityCsr->getOsContext().getPrimaryContext());
    }
}

HWTEST_F(DeviceTest, givenContextGroupSupportedWhenGettingHighPriorityCsrThenCorrectCsrAndContextIsReturned) {
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {

        const EngineInstancesContainer getGpgpuEngineInstances(const RootDeviceEnvironment &rootDeviceEnvironment) const override {
            auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
            EngineInstancesContainer engines;

            uint32_t numCcs = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

            if (hwInfo.featureTable.flags.ftrCCSNode) {
                for (uint32_t i = 0; i < numCcs; i++) {
                    auto engineType = static_cast<aub_stream::EngineType>(i + aub_stream::ENGINE_CCS);
                    engines.push_back({engineType, EngineUsage::regular});
                }

                engines.push_back({aub_stream::ENGINE_CCS, EngineUsage::lowPriority});
            }
            if (hwInfo.featureTable.flags.ftrRcsNode) {
                engines.push_back({aub_stream::ENGINE_RCS, EngineUsage::regular});
            }

            for (uint32_t i = 1; i < hwInfo.featureTable.ftrBcsInfo.size(); i++) {
                auto engineType = EngineHelpers::getBcsEngineAtIdx(i);

                if (hwInfo.featureTable.ftrBcsInfo.test(i)) {

                    engines.push_back({engineType, EngineUsage::regular});
                }
            }

            return engines;
        }
        EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const override {
            if (engineType == aub_stream::ENGINE_RCS) {
                return EngineGroupType::renderCompute;
            }
            if (engineType >= aub_stream::ENGINE_CCS && engineType < (aub_stream::ENGINE_CCS + hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled)) {
                return EngineGroupType::compute;
            }

            if (engineType == aub_stream::ENGINE_BCS1) {
                return EngineGroupType::copy;
            }
            UNRECOVERABLE_IF(true);
        }
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.ContextGroupSize.set(8);

    const uint32_t rootDeviceIndex = 0u;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    hwInfo.featureTable.ftrBcsInfo = 0b10;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    MockExecutionEnvironment mockExecutionEnvironment{&hwInfo};
    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*mockExecutionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);

    {
        MockExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment{&hwInfo};
        auto *neoMockDevice = NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(&hwInfo, executionEnvironment, rootDeviceIndex);
        MockDeviceImp deviceImp(neoMockDevice);

        NEO::CommandStreamReceiver *highPriorityCsr = nullptr;
        NEO::CommandStreamReceiver *highPriorityCsr2 = nullptr;

        auto &engineGroups = neoMockDevice->getRegularEngineGroups();
        uint32_t count = static_cast<uint32_t>(engineGroups.size());
        auto ordinal = 0u;
        auto ordinalCopy = 0u;

        for (uint32_t i = 0; i < count; i++) {
            if (engineGroups[i].engineGroupType == NEO::EngineGroupType::compute) {
                ordinal = i;
            }
            if (engineGroups[i].engineGroupType == NEO::EngineGroupType::copy) {
                ordinalCopy = i;
            }
        }

        ASSERT_TRUE(engineGroups[ordinal].engineGroupType == NEO::EngineGroupType::compute);
        ASSERT_TRUE(engineGroups[ordinalCopy].engineGroupType == NEO::EngineGroupType::copy);

        uint32_t index = 1;
        auto result = deviceImp.getCsrForOrdinalAndIndex(&highPriorityCsr, ordinal, index, ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH, deviceImp.queuePriorityHigh, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, highPriorityCsr);

        auto &secondaryEngines = neoMockDevice->secondaryEngines[EngineHelpers::mapCcsIndexToEngineType(index)];

        ASSERT_EQ(8u / hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled, secondaryEngines.engines.size());

        auto highPriorityIndex = secondaryEngines.regularEnginesTotal;
        ASSERT_LT(highPriorityIndex, static_cast<uint32_t>(secondaryEngines.engines.size()));

        EXPECT_EQ(secondaryEngines.engines[highPriorityIndex].osContext, &highPriorityCsr->getOsContext());
        EXPECT_EQ(secondaryEngines.engines[highPriorityIndex].commandStreamReceiver, highPriorityCsr);

        EXPECT_TRUE(highPriorityCsr->getOsContext().isPartOfContextGroup());
        EXPECT_NE(nullptr, highPriorityCsr->getOsContext().getPrimaryContext());

        result = deviceImp.getCsrForOrdinalAndIndex(&highPriorityCsr2, ordinal, index, ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH, deviceImp.queuePriorityHigh, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, highPriorityCsr2);
        EXPECT_NE(highPriorityCsr, highPriorityCsr2);

        EXPECT_EQ(secondaryEngines.engines[highPriorityIndex + 1].commandStreamReceiver, highPriorityCsr2);
        EXPECT_TRUE(highPriorityCsr2->getOsContext().isPartOfContextGroup());

        index = 100;
        result = deviceImp.getCsrForOrdinalAndIndex(&highPriorityCsr, ordinal, index, ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH, deviceImp.queuePriorityHigh, false);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

        index = 0;
        ordinal = 100;
        result = deviceImp.getCsrForOrdinalAndIndex(&highPriorityCsr, ordinal, index, ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH, deviceImp.queuePriorityHigh, false);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

        // When no HP copy engine, then hp csr from group is returned
        NEO::CommandStreamReceiver *bcsEngine = nullptr, *bcsEngine2 = nullptr;
        EXPECT_EQ(nullptr, neoMockDevice->getHpCopyEngine());

        result = deviceImp.getCsrForOrdinalAndIndex(&bcsEngine, ordinalCopy, index, ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH, deviceImp.queuePriorityHigh, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, bcsEngine);
        EXPECT_TRUE(bcsEngine->getOsContext().isHighPriority());

        result = deviceImp.getCsrForOrdinalAndIndex(&bcsEngine2, ordinalCopy, index, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_EQ(bcsEngine2, bcsEngine->getPrimaryCsr());
        EXPECT_TRUE(bcsEngine2->getOsContext().isRegular());
    }
}

HWTEST2_F(DeviceTest, givenHpCopyEngineWhenGettingHighPriorityCsrThenCorrectCsrAndContextIsReturned, IsAtLeastXeHpcCore) {
    struct MockGfxCoreHelper : public NEO::GfxCoreHelperHw<FamilyType> {
      public:
        const EngineInstancesContainer getGpgpuEngineInstances(const RootDeviceEnvironment &rootDeviceEnvironment) const override {
            auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
            auto defaultEngine = getChosenEngineType(hwInfo);

            EngineInstancesContainer engines;

            if (hwInfo.featureTable.flags.ftrRcsNode) {
                engines.push_back({aub_stream::ENGINE_RCS, EngineUsage::regular});
            } else {
                engines.push_back({defaultEngine, EngineUsage::regular});
            }

            engines.push_back({defaultEngine, EngineUsage::lowPriority});
            engines.push_back({defaultEngine, EngineUsage::internal});

            if (hwInfo.capabilityTable.blitterOperationsSupported && hwInfo.featureTable.ftrBcsInfo.test(0)) {
                engines.push_back({aub_stream::ENGINE_BCS, EngineUsage::regular});
                engines.push_back({aub_stream::ENGINE_BCS, EngineUsage::internal}); // internal usage
            }

            uint32_t hpIndex = 0;
            for (uint32_t i = static_cast<uint32_t>(hwInfo.featureTable.ftrBcsInfo.size() - 1); i > 0; i--) {

                if (hwInfo.featureTable.ftrBcsInfo.test(i)) {
                    hpIndex = i;
                    break;
                }
            }

            for (uint32_t i = 1; i < hwInfo.featureTable.ftrBcsInfo.size(); i++) {
                auto engineType = EngineHelpers::getBcsEngineAtIdx(i);

                if (hpIndex == i) {
                    engines.push_back({engineType, EngineUsage::highPriority});
                    continue;
                }

                if (hwInfo.featureTable.ftrBcsInfo.test(i)) {

                    engines.push_back({engineType, EngineUsage::regular});
                }
            }

            return engines;
        }

        aub_stream::EngineType getDefaultHpCopyEngine(const HardwareInfo &hwInfo) const override {
            uint32_t hpIndex = 0;

            auto bscCount = static_cast<uint32_t>(hwInfo.featureTable.ftrBcsInfo.size());
            for (uint32_t i = bscCount - 1; i > 0; i--) {
                if (hwInfo.featureTable.ftrBcsInfo.test(i)) {
                    hpIndex = i;
                    break;
                }
            }

            if (hpIndex == 0) {
                return aub_stream::EngineType::NUM_ENGINES;
            }
            return EngineHelpers::getBcsEngineAtIdx(hpIndex);
        }
    };

    const uint32_t rootDeviceIndex = 0u;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    hwInfo.featureTable.ftrBcsInfo = 0b111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    MockExecutionEnvironment mockExecutionEnvironment{&hwInfo};
    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*mockExecutionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);

    {
        MockExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment{&hwInfo};
        auto *neoMockDevice = NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(&hwInfo, executionEnvironment, rootDeviceIndex);
        MockDeviceImp deviceImp(neoMockDevice);

        NEO::CommandStreamReceiver *highPriorityCsr = nullptr;

        auto &engineGroups = neoMockDevice->getRegularEngineGroups();
        uint32_t count = static_cast<uint32_t>(engineGroups.size());
        auto ordinal = 0u;

        for (uint32_t i = 0; i < count; i++) {
            if (engineGroups[i].engineGroupType == NEO::EngineGroupType::copy || engineGroups[i].engineGroupType == NEO::EngineGroupType::linkedCopy) {
                ordinal = i;

                uint32_t index = 0;
                auto result = deviceImp.getCsrForOrdinalAndIndex(&highPriorityCsr, ordinal, index, ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH, 0, false);
                EXPECT_EQ(ZE_RESULT_SUCCESS, result);
                ASSERT_NE(nullptr, highPriorityCsr);

                EXPECT_TRUE(highPriorityCsr->getOsContext().getIsPrimaryEngine());
                EXPECT_TRUE(highPriorityCsr->getOsContext().isHighPriority());

                EXPECT_EQ(aub_stream::ENGINE_BCS2, highPriorityCsr->getOsContext().getEngineType());
            }
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
    MockDevice device;
    ze_result_t result = ZE_RESULT_SUCCESS;
    ze_device_image_properties_t imageProperties;

    result = zeDeviceGetImageProperties(device.toHandle(), &imageProperties);
    EXPECT_EQ(1u, device.getDeviceImagePropertiesCalled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeDevice, givenPitchedAllocPropertiesStructWhenGettingImagePropertiesThenCorrectPropertiesAreReturned) {
    ze_result_t errorValue;
    DriverHandleImp driverHandle{};
    NEO::MockDevice *neoDevice = (NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(&driverHandle, neoDevice, false, &errorValue));
    DeviceInfo &deviceInfo = neoDevice->deviceInfo;

    neoDevice->deviceInfo.imageSupport = true;
    deviceInfo.image2DMaxWidth = 32;
    deviceInfo.image2DMaxHeight = 16;
    deviceInfo.image3DMaxDepth = 1;
    deviceInfo.imageMaxBufferSize = 1024;
    deviceInfo.imageMaxArraySize = 1;
    deviceInfo.maxSamplers = 6;
    deviceInfo.maxReadImageArgs = 7;
    deviceInfo.maxWriteImageArgs = 8;
    ze_result_t result = ZE_RESULT_SUCCESS;
    ze_device_pitched_alloc_exp_properties_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_PITCHED_ALLOC_DEVICE_EXP_PROPERTIES;

    ze_device_image_properties_t imageProperties;
    imageProperties.pNext = &extendedProperties;

    result = zeDeviceGetImageProperties(device->toHandle(), &imageProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(32u, extendedProperties.maxImageLinearWidth);
    EXPECT_EQ(16u, extendedProperties.maxImageLinearHeight);
}

TEST(zeDevice, givenImagesSupportedWhenGettingImagePropertiesThenValidValuesAreReturned) {
    ze_result_t errorValue;

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

TEST(zeDevice, givenNoImagesSupportedWhenCreatingImageErrorReturns) {
    ze_result_t errorValue;

    DriverHandleImp driverHandle{};
    NEO::MockDevice *neoDevice = (NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(&driverHandle, neoDevice, false, &errorValue));

    ze_image_handle_t image = {};
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    neoDevice->deviceInfo.imageSupport = false;

    auto result = device->createImage(&desc, &image);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    EXPECT_EQ(nullptr, image);
}

using zeDeviceSystemBarrierTest = DeviceTest;

TEST_F(zeDeviceSystemBarrierTest, whenCallingSystemBarrierThenReturnErrorUnsupportedFeature) {

    auto result = static_cast<DeviceImp *>(device)->systemBarrier();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

using MultiSubDeviceTest = Test<MultiSubDeviceFixture<true, -1, -1>>;
TEST_F(MultiSubDeviceTest, GivenApiSupportAndLocalMemoryEnabledWhenDeviceContainsSubDevicesThenItIsImplicitScalingCapable) {
    auto &gfxCoreHelper = neoDevice->getGfxCoreHelper();
    if (gfxCoreHelper.platformSupportsImplicitScaling(neoDevice->getRootDeviceEnvironment())) {
        EXPECT_TRUE(device->isImplicitScalingCapable());
        EXPECT_EQ(neoDevice, deviceImp->getActiveDevice());
    } else {
        EXPECT_FALSE(device->isImplicitScalingCapable());
        EXPECT_EQ(subDevice, deviceImp->getActiveDevice());
    }
}

using MultiSubDeviceTestNoApi = Test<MultiSubDeviceFixture<false, -1, -1>>;
TEST_F(MultiSubDeviceTestNoApi, GivenNoApiSupportAndLocalMemoryEnabledWhenDeviceContainsSubDevicesThenItIsNotImplicitScalingCapable) {
    EXPECT_FALSE(device->isImplicitScalingCapable());
    EXPECT_EQ(subDevice, deviceImp->getActiveDevice());
}

using MultiSubDeviceTestNoApiForceOn = Test<MultiSubDeviceFixture<false, 1, -1>>;
TEST_F(MultiSubDeviceTestNoApiForceOn, GivenNoApiSupportAndLocalMemoryEnabledWhenForcedImplicitScalingThenItIsImplicitScalingCapable) {
    EXPECT_TRUE(device->isImplicitScalingCapable());
    EXPECT_EQ(neoDevice, deviceImp->getActiveDevice());
}

using MultiSubDeviceEnabledImplicitScalingTest = Test<MultiSubDeviceFixture<true, -1, 1>>;
TEST_F(MultiSubDeviceEnabledImplicitScalingTest, GivenApiSupportAndLocalMemoryEnabledWhenDeviceContainsSubDevicesAndSupportsImplicitScalingThenItIsImplicitScalingCapable) {
    EXPECT_TRUE(device->isImplicitScalingCapable());
    EXPECT_EQ(neoDevice, deviceImp->getActiveDevice());
}

TEST_F(MultiSubDeviceEnabledImplicitScalingTest, GivenEnabledImplicitScalingWhenDeviceReturnsLowPriorityCsrThenItIsDefaultCsr) {
    auto &defaultEngine = deviceImp->getActiveDevice()->getDefaultEngine();

    NEO::CommandStreamReceiver *csr = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, deviceImp->getCsrForLowPriority(&csr, false));

    auto ret = deviceImp->getCsrForOrdinalAndIndex(&csr, 0, 0, ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW, 0, false);

    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(defaultEngine.commandStreamReceiver, csr);
}

HWTEST2_F(MultiSubDeviceWithContextGroupAndImplicitScalingTest, GivenRootDeviceWhenGettingLowPriorityCsrForComputeEngineThenDefaultCsrReturned, IsAtLeastXeCore) {
    auto &defaultEngine = deviceImp->getActiveDevice()->getDefaultEngine();

    NEO::CommandStreamReceiver *csr = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, deviceImp->getCsrForLowPriority(&csr, false));

    auto ret = deviceImp->getCsrForOrdinalAndIndex(&csr, 0, 0, ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW, 0, false);

    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(defaultEngine.commandStreamReceiver, csr);
}

HWTEST2_F(MultiSubDeviceWithContextGroupAndImplicitScalingTest, GivenRootDeviceWhenGettingLowPriorityCsrForCopyEngineThenRegularBcsIsReturned, IsAtMostXe3Core) {
    NEO::CommandStreamReceiver *csr = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, deviceImp->getCsrForLowPriority(&csr, true));

    auto ordinal = deviceImp->getCopyEngineOrdinal();
    auto ret = deviceImp->getCsrForOrdinalAndIndex(&csr, ordinal, 0, ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW, 0, false);

    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_NE(nullptr, csr);
    EXPECT_TRUE(csr->getOsContext().isRegular());
    EXPECT_EQ(1u, csr->getOsContext().getDeviceBitfield().count());
}

HWTEST2_F(MultiSubDeviceWithContextGroupAndImplicitScalingTest, GivenRootDeviceWhenGettingHighPriorityCsrForCopyEngineThenSubDeviceHpBcsIsReturned, IsAtLeastXeCore) {

    NEO::CommandStreamReceiver *csr = nullptr;
    auto ordinal = deviceImp->getCopyEngineOrdinal();

    auto ret = deviceImp->getCsrForOrdinalAndIndex(&csr, ordinal, 0, ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH, 0, false);

    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_NE(nullptr, csr);
    EXPECT_TRUE(csr->getOsContext().isHighPriority());
    EXPECT_EQ(1u, csr->getOsContext().getDeviceBitfield().count());
}

using DeviceSimpleTests = Test<DeviceFixture>;

static_assert(ZE_MEMORY_ACCESS_CAP_FLAG_RW == UnifiedSharedMemoryFlags::access, "Flags value difference");
static_assert(ZE_MEMORY_ACCESS_CAP_FLAG_ATOMIC == UnifiedSharedMemoryFlags::atomicAccess, "Flags value difference");
static_assert(ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT == UnifiedSharedMemoryFlags::concurrentAccess, "Flags value difference");
static_assert(ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT_ATOMIC == UnifiedSharedMemoryFlags::concurrentAtomicAccess, "Flags value difference");

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

    EXPECT_EQ(memcmp(&deviceProps.vendorId, deviceProps.uuid.id, sizeof(uint16_t)), 0);
    EXPECT_EQ(memcmp(&deviceProps.deviceId, deviceProps.uuid.id + sizeof(uint16_t), sizeof(uint16_t)), 0);
    EXPECT_EQ(memcmp(&rootDeviceIndex, deviceProps.uuid.id + (3 * sizeof(uint16_t)), sizeof(uint32_t)), 0);
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

    uint32_t threadsPerEU = hwInfo.gtSystemInfo.NumThreadsPerEu +
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

using P2pBandwidthPropertiesTest = MultipleDevicesTest;
TEST_F(P2pBandwidthPropertiesTest, GivenDirectFabricConnectionBetweenDevicesWhenQueryingBandwidthPropertiesThenCorrectPropertiesAreSet) {

    const uint32_t testLatency = 1;
    const uint32_t testBandwidth = 20;

    driverHandle->initializeVertexes();

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    FabricEdge testEdge{};
    testEdge.vertexA = static_cast<DeviceImp *>(device0)->fabricVertex;
    testEdge.vertexB = static_cast<DeviceImp *>(device1)->fabricVertex;
    const char *linkModel = "XeLink";
    memcpy_s(testEdge.properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, linkModel, strlen(linkModel));
    testEdge.properties.bandwidth = testBandwidth;
    testEdge.properties.latency = testLatency;
    testEdge.properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
    testEdge.properties.latencyUnit = ZE_LATENCY_UNIT_HOP;
    driverHandle->fabricEdges.push_back(&testEdge);

    ze_device_p2p_properties_t p2pProperties = {};
    ze_device_p2p_bandwidth_exp_properties_t p2pBandwidthProps = {};

    p2pProperties.pNext = &p2pBandwidthProps;
    p2pBandwidthProps.stype = ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES;
    p2pBandwidthProps.pNext = nullptr;

    device0->getP2PProperties(device1, &p2pProperties);

    EXPECT_EQ(testBandwidth, p2pBandwidthProps.logicalBandwidth);
    EXPECT_EQ(testBandwidth, p2pBandwidthProps.physicalBandwidth);
    EXPECT_EQ(ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC, p2pBandwidthProps.bandwidthUnit);

    EXPECT_EQ(testLatency, p2pBandwidthProps.logicalLatency);
    EXPECT_EQ(testLatency, p2pBandwidthProps.physicalLatency);
    EXPECT_EQ(ZE_LATENCY_UNIT_HOP, p2pBandwidthProps.latencyUnit);

    driverHandle->fabricEdges.pop_back();
}

TEST_F(P2pBandwidthPropertiesTest, GivenNoXeLinkFabricConnectionBetweenDevicesWhenQueryingBandwidthPropertiesThenBandwidthIsZero) {

    const uint32_t testLatency = 1;
    const uint32_t testBandwidth = 20;

    driverHandle->initializeVertexes();

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    FabricEdge testEdge{};
    testEdge.vertexA = static_cast<DeviceImp *>(device0)->fabricVertex;
    testEdge.vertexB = static_cast<DeviceImp *>(device1)->fabricVertex;
    const char *linkModel = "Dummy";
    memcpy_s(testEdge.properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, linkModel, strlen(linkModel));
    testEdge.properties.bandwidth = testBandwidth;
    testEdge.properties.latency = testLatency;
    testEdge.properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
    testEdge.properties.latencyUnit = ZE_LATENCY_UNIT_HOP;
    driverHandle->fabricEdges.push_back(&testEdge);

    ze_device_p2p_properties_t p2pProperties = {};
    ze_device_p2p_bandwidth_exp_properties_t p2pBandwidthProps = {};

    p2pProperties.pNext = &p2pBandwidthProps;
    p2pBandwidthProps.stype = ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES;
    p2pBandwidthProps.pNext = nullptr;

    // Calling with "Dummy" link
    device0->getP2PProperties(device1, &p2pProperties);
    EXPECT_EQ(0u, p2pBandwidthProps.logicalBandwidth);
    driverHandle->fabricEdges.pop_back();
}

TEST_F(P2pBandwidthPropertiesTest, GivenXeLinkAndMdfiFabricConnectionBetweenSubDevicesWhenQueryingBandwidthPropertiesThenCorrectPropertiesAreSet) {
    constexpr uint32_t xeLinkBandwidth = 3;

    driverHandle->initializeVertexes();

    uint32_t subDeviceCount = 2;
    ze_device_handle_t device0SubDevices[2];
    ze_device_handle_t device1SubDevices[2];
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->devices[0]->getSubDevices(&subDeviceCount, device0SubDevices));
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->devices[1]->getSubDevices(&subDeviceCount, device1SubDevices));
    EXPECT_NE(nullptr, device0SubDevices[0]);
    EXPECT_NE(nullptr, device0SubDevices[1]);
    EXPECT_NE(nullptr, device1SubDevices[0]);
    EXPECT_NE(nullptr, device1SubDevices[1]);

    ze_fabric_vertex_handle_t vertex00 = nullptr;
    ze_fabric_vertex_handle_t vertex01 = nullptr;
    ze_fabric_vertex_handle_t vertex10 = nullptr;
    ze_fabric_vertex_handle_t vertex11 = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[0])->getFabricVertex(&vertex00));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[1])->getFabricVertex(&vertex01));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device1SubDevices[0])->getFabricVertex(&vertex10));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device1SubDevices[1])->getFabricVertex(&vertex11));
    EXPECT_NE(nullptr, vertex00);
    EXPECT_NE(nullptr, vertex01);
    EXPECT_NE(nullptr, vertex10);
    EXPECT_NE(nullptr, vertex11);

    const char *mdfiModel = "MDFI";
    const char *xeLinkModel = "XeLink";
    const char *xeLinkMdfiModel = "XeLink-MDFI";
    const char *mdfiXeLinkModel = "MDFI-XeLink";
    const char *mdfiXeLinkMdfiModel = "MDFI-XeLink-MDFI";

    // MDFI between 00 & 01
    auto testEdgeMdfi0 = new FabricEdge;
    testEdgeMdfi0->vertexA = FabricVertex::fromHandle(vertex00);
    testEdgeMdfi0->vertexB = FabricVertex::fromHandle(vertex01);
    memcpy_s(testEdgeMdfi0->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, mdfiModel, strlen(mdfiModel));
    testEdgeMdfi0->properties.bandwidth = 0u;
    testEdgeMdfi0->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_UNKNOWN;
    testEdgeMdfi0->properties.latency = 0u;
    testEdgeMdfi0->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
    driverHandle->fabricEdges.push_back(testEdgeMdfi0);

    // MDFI between 10 & 11
    auto testEdgeMdfi1 = new FabricEdge;
    testEdgeMdfi1->vertexA = FabricVertex::fromHandle(vertex10);
    testEdgeMdfi1->vertexB = FabricVertex::fromHandle(vertex11);
    memcpy_s(testEdgeMdfi1->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, mdfiModel, strlen(mdfiModel));
    testEdgeMdfi1->properties.bandwidth = 0u;
    testEdgeMdfi1->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_UNKNOWN;
    testEdgeMdfi1->properties.latency = 0u;
    testEdgeMdfi1->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
    driverHandle->fabricEdges.push_back(testEdgeMdfi1);

    // XeLink between 01 & 10
    auto testEdgeXeLink = new FabricEdge;
    testEdgeXeLink->vertexA = FabricVertex::fromHandle(vertex01);
    testEdgeXeLink->vertexB = FabricVertex::fromHandle(vertex10);
    memcpy_s(testEdgeXeLink->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, xeLinkModel, strlen(xeLinkModel));
    testEdgeXeLink->properties.bandwidth = xeLinkBandwidth;
    testEdgeXeLink->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
    testEdgeXeLink->properties.latency = 1u;
    testEdgeXeLink->properties.latencyUnit = ZE_LATENCY_UNIT_HOP;
    driverHandle->fabricEdges.push_back(testEdgeXeLink);

    // MDFI-XeLink between 00 & 10
    auto testEdgeMdfiXeLink = new FabricEdge;
    testEdgeMdfiXeLink->vertexA = FabricVertex::fromHandle(vertex00);
    testEdgeMdfiXeLink->vertexB = FabricVertex::fromHandle(vertex10);
    memcpy_s(testEdgeMdfiXeLink->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, mdfiXeLinkModel, strlen(mdfiXeLinkModel));
    testEdgeMdfiXeLink->properties.bandwidth = xeLinkBandwidth;
    testEdgeMdfiXeLink->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
    testEdgeMdfiXeLink->properties.latency = std::numeric_limits<uint32_t>::max();
    testEdgeMdfiXeLink->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
    driverHandle->fabricIndirectEdges.push_back(testEdgeMdfiXeLink);

    // MDFI-XeLink-MDFI between 00 & 11
    auto testEdgeMdfiXeLinkMdfi = new FabricEdge;
    testEdgeMdfiXeLinkMdfi->vertexA = FabricVertex::fromHandle(vertex00);
    testEdgeMdfiXeLinkMdfi->vertexB = FabricVertex::fromHandle(vertex11);
    memcpy_s(testEdgeMdfiXeLinkMdfi->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, mdfiXeLinkMdfiModel, strlen(mdfiXeLinkMdfiModel));
    testEdgeMdfiXeLinkMdfi->properties.bandwidth = xeLinkBandwidth;
    testEdgeMdfiXeLinkMdfi->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
    testEdgeMdfiXeLinkMdfi->properties.latency = std::numeric_limits<uint32_t>::max();
    testEdgeMdfiXeLinkMdfi->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
    driverHandle->fabricIndirectEdges.push_back(testEdgeMdfiXeLinkMdfi);

    // XeLink-MDFI between 01 & 11
    auto testEdgeXeLinkMdfi = new FabricEdge;
    testEdgeXeLinkMdfi->vertexA = FabricVertex::fromHandle(vertex01);
    testEdgeXeLinkMdfi->vertexB = FabricVertex::fromHandle(vertex11);
    memcpy_s(testEdgeXeLinkMdfi->properties.model, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE, xeLinkMdfiModel, strlen(xeLinkMdfiModel));
    testEdgeXeLinkMdfi->properties.bandwidth = xeLinkBandwidth;
    testEdgeXeLinkMdfi->properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
    testEdgeXeLinkMdfi->properties.latency = std::numeric_limits<uint32_t>::max();
    testEdgeXeLinkMdfi->properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
    driverHandle->fabricIndirectEdges.push_back(testEdgeXeLinkMdfi);

    ze_device_p2p_properties_t p2pProperties = {};
    ze_device_p2p_bandwidth_exp_properties_t p2pBandwidthProps = {};

    p2pProperties.pNext = &p2pBandwidthProps;
    p2pBandwidthProps.stype = ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES;
    p2pBandwidthProps.pNext = nullptr;

    // Check MDFI
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[0])->getP2PProperties(device0SubDevices[1], &p2pProperties));
    EXPECT_EQ(0u, p2pBandwidthProps.logicalBandwidth);
    EXPECT_EQ(0u, p2pBandwidthProps.physicalBandwidth);
    EXPECT_EQ(ZE_BANDWIDTH_UNIT_UNKNOWN, p2pBandwidthProps.bandwidthUnit);
    EXPECT_EQ(0u, p2pBandwidthProps.logicalLatency);
    EXPECT_EQ(0u, p2pBandwidthProps.physicalLatency);
    EXPECT_EQ(ZE_LATENCY_UNIT_UNKNOWN, p2pBandwidthProps.latencyUnit);

    // Check XeLink
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[1])->getP2PProperties(device1SubDevices[0], &p2pProperties));
    EXPECT_EQ(xeLinkBandwidth, p2pBandwidthProps.logicalBandwidth);
    EXPECT_EQ(xeLinkBandwidth, p2pBandwidthProps.physicalBandwidth);
    EXPECT_EQ(ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC, p2pBandwidthProps.bandwidthUnit);
    EXPECT_EQ(1u, p2pBandwidthProps.logicalLatency);
    EXPECT_EQ(1u, p2pBandwidthProps.physicalLatency);
    EXPECT_EQ(ZE_LATENCY_UNIT_HOP, p2pBandwidthProps.latencyUnit);

    // Check MDFI-XeLink
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[0])->getP2PProperties(device1SubDevices[0], &p2pProperties));
    EXPECT_EQ(xeLinkBandwidth, p2pBandwidthProps.logicalBandwidth);
    EXPECT_EQ(xeLinkBandwidth, p2pBandwidthProps.physicalBandwidth);
    EXPECT_EQ(ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC, p2pBandwidthProps.bandwidthUnit);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), p2pBandwidthProps.logicalLatency);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), p2pBandwidthProps.physicalLatency);
    EXPECT_EQ(ZE_LATENCY_UNIT_UNKNOWN, p2pBandwidthProps.latencyUnit);

    // Check MDFI-XeLink-MDFI
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[0])->getP2PProperties(device1SubDevices[1], &p2pProperties));
    EXPECT_EQ(xeLinkBandwidth, p2pBandwidthProps.logicalBandwidth);
    EXPECT_EQ(xeLinkBandwidth, p2pBandwidthProps.physicalBandwidth);
    EXPECT_EQ(ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC, p2pBandwidthProps.bandwidthUnit);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), p2pBandwidthProps.logicalLatency);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), p2pBandwidthProps.physicalLatency);
    EXPECT_EQ(ZE_LATENCY_UNIT_UNKNOWN, p2pBandwidthProps.latencyUnit);

    // Check XeLink-MDFI
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::Device::fromHandle(device0SubDevices[1])->getP2PProperties(device1SubDevices[1], &p2pProperties));
    EXPECT_EQ(xeLinkBandwidth, p2pBandwidthProps.logicalBandwidth);
    EXPECT_EQ(xeLinkBandwidth, p2pBandwidthProps.physicalBandwidth);
    EXPECT_EQ(ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC, p2pBandwidthProps.bandwidthUnit);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), p2pBandwidthProps.logicalLatency);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), p2pBandwidthProps.physicalLatency);
    EXPECT_EQ(ZE_LATENCY_UNIT_UNKNOWN, p2pBandwidthProps.latencyUnit);
}

TEST_F(P2pBandwidthPropertiesTest, GivenNoDirectFabricConnectionBetweenDevicesWhenQueryingBandwidthPropertiesThenBandwidthIsZero) {

    driverHandle->initializeVertexes();

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_device_p2p_properties_t p2pProperties = {};
    ze_device_p2p_bandwidth_exp_properties_t p2pBandwidthProps = {};

    p2pProperties.pNext = &p2pBandwidthProps;
    p2pBandwidthProps.stype = ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES;
    p2pBandwidthProps.pNext = nullptr;

    // By default Xelink connections are not available.
    // So getting the p2p properties without it.
    device0->getP2PProperties(device1, &p2pProperties);
    EXPECT_EQ(0u, p2pBandwidthProps.logicalBandwidth);
}

TEST_F(P2pBandwidthPropertiesTest, GivenFabricVerticesAreNotInitializedWhenQueryingBandwidthPropertiesThenFabricVerticesAreInitialized) {

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_device_p2p_properties_t p2pProperties = {};
    ze_device_p2p_bandwidth_exp_properties_t p2pBandwidthProps = {};

    p2pProperties.pNext = &p2pBandwidthProps;
    p2pBandwidthProps.stype = ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES;
    p2pBandwidthProps.pNext = nullptr;

    // Calling without initialization
    device0->getP2PProperties(device1, &p2pProperties);
    EXPECT_NE(driverHandle->fabricVertices.size(), 0u);
}

TEST_F(P2pBandwidthPropertiesTest, GivenFabricVerticesAreNotAvailableForDevicesWhenQueryingBandwidthPropertiesThenBandwidthIsZero) {

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    driverHandle->initializeVertexes();

    // Check for device 0
    auto backupFabricVertex = static_cast<DeviceImp *>(device0)->fabricVertex;
    static_cast<DeviceImp *>(device0)->fabricVertex = nullptr;

    ze_device_p2p_properties_t p2pProperties = {};
    ze_device_p2p_bandwidth_exp_properties_t p2pBandwidthProps = {};

    p2pProperties.pNext = &p2pBandwidthProps;
    p2pBandwidthProps.stype = ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES;
    p2pBandwidthProps.pNext = nullptr;

    device0->getP2PProperties(device1, &p2pProperties);
    EXPECT_EQ(p2pBandwidthProps.logicalBandwidth, 0u);
    static_cast<DeviceImp *>(device0)->fabricVertex = backupFabricVertex;

    // Check for device 1
    backupFabricVertex = static_cast<DeviceImp *>(device1)->fabricVertex;
    static_cast<DeviceImp *>(device1)->fabricVertex = nullptr;

    device0->getP2PProperties(device1, &p2pProperties);
    EXPECT_EQ(p2pBandwidthProps.logicalBandwidth, 0u);
    static_cast<DeviceImp *>(device1)->fabricVertex = backupFabricVertex;
}

TEST(DeviceReturnFlatHierarchyTest, GivenFlatHierarchyIsSetWithMaskThenFlagsOfDevicePropertiesIsCorrect) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0,1.1,2");
    MultiDeviceFixtureFlatHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_device_handle_t> hDevices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    // mask is "0,1.1,2", but with L0::L0DeviceHierarchyMode::L0_DEVICE_HIERARCHY_FLAT 1.1
    // is not valid, so expected count is 2.
    EXPECT_EQ(count, 2u);

    hDevices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

    for (auto &hDevice : hDevices) {
        ze_device_properties_t deviceProperties{};
        deviceProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
        EXPECT_EQ(Device::fromHandle(hDevice)->getProperties(&deviceProperties), ZE_RESULT_SUCCESS);
        EXPECT_NE(ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);

        uint32_t subDeviceCount = 0;
        EXPECT_EQ(Device::fromHandle(hDevice)->getSubDevices(&subDeviceCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(subDeviceCount, 0u);
    }

    multiDeviceFixture.tearDown();
}

TEST(DeviceReturnCombinedHierarchyTest, GivenCombinedHierarchyIsSetWithMaskThenFlagsOfDevicePropertiesIsCorrect) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0,1.1,2");
    MultiDeviceFixtureCombinedHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_device_handle_t> hDevices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    // mask is "0,1.1,2", but with L0::L0DeviceHierarchyMode::L0_DEVICE_HIERARCHY_COMBINED 1.1
    // is not valid, so expected count is 2.
    EXPECT_EQ(count, 2u);

    hDevices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

    for (auto &hDevice : hDevices) {
        ze_device_properties_t deviceProperties{};
        deviceProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
        EXPECT_EQ(Device::fromHandle(hDevice)->getProperties(&deviceProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);

        uint32_t subDeviceCount = 0;
        EXPECT_EQ(Device::fromHandle(hDevice)->getSubDevices(&subDeviceCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(subDeviceCount, 0u);
    }

    multiDeviceFixture.tearDown();
}

TEST(DeviceReturnCombinedHierarchyTest, GivenCombinedHierarchyIsSetWithMaskThenGetRootDeviceDoesNotReturnNullptr) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0,1.1,2");
    MultiDeviceFixtureCombinedHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_device_handle_t> hDevices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    // mask is "0,1.1,2", but with L0::L0DeviceHierarchyMode::L0_DEVICE_HIERARCHY_COMBINED 1.1
    // is not valid, so expected count is 2.
    EXPECT_EQ(count, 2u);

    hDevices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

    for (auto &hDevice : hDevices) {
        ze_device_handle_t rootDevice = nullptr;
        EXPECT_EQ(Device::fromHandle(hDevice)->getRootDevice(&rootDevice), ZE_RESULT_SUCCESS);
        EXPECT_NE(rootDevice, nullptr);
    }

    multiDeviceFixture.tearDown();
}

TEST(DeviceReturnCombinedHierarchyTest, GivenCombinedHierarchyIsSetWithMaskWithoutSubDevicesThenGetRootDeviceReturnsNullptr) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0,1,2,3");
    MultiDeviceFixtureCombinedHierarchy multiDeviceFixture{};
    multiDeviceFixture.numSubDevices = 0;
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_device_handle_t> hDevices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(count, 4u);

    hDevices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

    for (auto &hDevice : hDevices) {
        ze_device_handle_t rootDevice = nullptr;
        EXPECT_EQ(Device::fromHandle(hDevice)->getRootDevice(&rootDevice), ZE_RESULT_SUCCESS);
        EXPECT_EQ(rootDevice, nullptr);
    }

    multiDeviceFixture.tearDown();
}

TEST(DeviceReturnCombinedHierarchyTest, GivenCombinedHierarchyIsSetWithoutDevicesArrayThenGetDeviceFails) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0,1,2");
    MultiDeviceFixtureCombinedHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(count, 3u);

    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);

    multiDeviceFixture.tearDown();
}

TEST(DeviceReturnCombinedHierarchyTest, GivenCombinedHierarchyIsSetWithInvalidAffinityThenGetDeviceFails) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ZE_AFFINITY_MASK.set("90");
    MultiDeviceFixtureCombinedHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST(DeviceReturnCombinedHierarchyTest, GivenCombinedHierarchyIsSetThenGetRootDeviceReturnsNullptr) {

    MultiDeviceFixtureCombinedHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_device_handle_t> hDevices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(count, multiDeviceFixture.numRootDevices * multiDeviceFixture.numSubDevices);

    hDevices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

    for (auto &hDevice : hDevices) {
        ze_device_handle_t rootDevice = nullptr;
        EXPECT_EQ(Device::fromHandle(hDevice)->getRootDevice(&rootDevice), ZE_RESULT_SUCCESS);
        EXPECT_NE(rootDevice, nullptr);
    }

    multiDeviceFixture.tearDown();
}

TEST(DeviceReturnCombinedHierarchyTest, GivenCombinedHierarchyIsSetAndDefaultAffinityThenFlagsOfDevicePropertiesIsCorrect) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ZE_AFFINITY_MASK.set("");
    MultiDeviceFixtureCombinedHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_device_handle_t> hDevices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(count, multiDeviceFixture.numRootDevices * multiDeviceFixture.numSubDevices);

    hDevices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

    for (auto &hDevice : hDevices) {
        ze_device_properties_t deviceProperties{};
        deviceProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
        EXPECT_EQ(Device::fromHandle(hDevice)->getProperties(&deviceProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);

        uint32_t subDeviceCount = 0;
        EXPECT_EQ(Device::fromHandle(hDevice)->getSubDevices(&subDeviceCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(subDeviceCount, 0u);
    }

    multiDeviceFixture.tearDown();
}

TEST(DeviceReturnCombinedHierarchyTest, GivenCombinedHierarchyIsSetAndEmptyAffinityMaskThenFlagsOfDevicePropertiesIsCorrect) {

    MultiDeviceFixtureCombinedHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_device_handle_t> hDevices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(count, multiDeviceFixture.numRootDevices * multiDeviceFixture.numSubDevices);

    hDevices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

    for (auto &hDevice : hDevices) {
        ze_device_properties_t deviceProperties{};
        deviceProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
        EXPECT_EQ(Device::fromHandle(hDevice)->getProperties(&deviceProperties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);

        uint32_t subDeviceCount = 0;
        EXPECT_EQ(Device::fromHandle(hDevice)->getSubDevices(&subDeviceCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(subDeviceCount, 0u);
    }

    multiDeviceFixture.tearDown();
}

TEST(DeviceReturnCombinedHierarchyTest, GivenCombinedHierarchyIsSetWithMaskThenActiveDeviceIsCorrect) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ZE_AFFINITY_MASK.set("1");
    MultiDeviceFixtureCombinedHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    auto device = multiDeviceFixture.driverHandle->devices[0];
    auto deviceImp = static_cast<L0::DeviceImp *>(device);

    auto activeDevice = deviceImp->getActiveDevice();
    EXPECT_EQ(activeDevice, device->getNEODevice()->getSubDevice(1u));

    multiDeviceFixture.tearDown();
}

TEST(DeviceReturnFlatHierarchyTest, GivenFlatHierarchyIsSetThenFlagsOfDevicePropertiesIsCorrect) {

    MultiDeviceFixtureFlatHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_device_handle_t> hDevices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(count, multiDeviceFixture.numRootDevices * multiDeviceFixture.numSubDevices);

    hDevices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

    for (auto &hDevice : hDevices) {
        ze_device_properties_t deviceProperties{};
        deviceProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
        EXPECT_EQ(Device::fromHandle(hDevice)->getProperties(&deviceProperties), ZE_RESULT_SUCCESS);
        EXPECT_NE(ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);

        uint32_t subDeviceCount = 0;
        EXPECT_EQ(Device::fromHandle(hDevice)->getSubDevices(&subDeviceCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(subDeviceCount, 0u);
    }

    multiDeviceFixture.tearDown();
}

TEST(DeviceReturnFlatHierarchyTest, GivenFlatHierarchyIsSetWithMaskThenGetRootDeviceReturnsNullptr) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0,1.1,2");
    MultiDeviceFixtureFlatHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_device_handle_t> hDevices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    // mask is "0,1.1,2", but with L0::L0DeviceHierarchyMode::L0_DEVICE_HIERARCHY_FLAT 1.1
    // is not valid, so expected count is 2.
    EXPECT_EQ(count, 2u);

    hDevices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

    for (auto &hDevice : hDevices) {
        ze_device_handle_t rootDevice = nullptr;
        EXPECT_EQ(Device::fromHandle(hDevice)->getRootDevice(&rootDevice), ZE_RESULT_SUCCESS);
        EXPECT_EQ(rootDevice, nullptr);
    }

    multiDeviceFixture.tearDown();
}

TEST(DeviceReturnFlatHierarchyTest, GivenFlatHierarchyIsSetThenGetRootDeviceReturnsNullptr) {

    MultiDeviceFixtureFlatHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_device_handle_t> hDevices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(count, multiDeviceFixture.numRootDevices * multiDeviceFixture.numSubDevices);

    hDevices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

    for (auto &hDevice : hDevices) {
        ze_device_handle_t rootDevice = nullptr;
        EXPECT_EQ(Device::fromHandle(hDevice)->getRootDevice(&rootDevice), ZE_RESULT_SUCCESS);
        EXPECT_EQ(rootDevice, nullptr);
    }

    multiDeviceFixture.tearDown();
}

TEST(DeviceReturnCompositeHierarchyTest, GivenCompositeHierarchyIsSetWithMaskThenGetRootDeviceIsNotNullForSubDevices) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0,1,2");
    MultiDeviceFixtureCompositeHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_device_handle_t> hDevices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(count, 3u);

    hDevices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

    for (auto &hDevice : hDevices) {

        uint32_t subDeviceCount = 0;
        EXPECT_EQ(Device::fromHandle(hDevice)->getSubDevices(&subDeviceCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(subDeviceCount, multiDeviceFixture.numSubDevices);
        std::vector<ze_device_handle_t> hSubDevices(multiDeviceFixture.numSubDevices);
        EXPECT_EQ(Device::fromHandle(hDevice)->getSubDevices(&subDeviceCount, hSubDevices.data()), ZE_RESULT_SUCCESS);

        for (auto &hSubDevice : hSubDevices) {
            ze_device_handle_t rootDevice = nullptr;
            EXPECT_EQ(Device::fromHandle(hSubDevice)->getRootDevice(&rootDevice), ZE_RESULT_SUCCESS);
            EXPECT_NE(rootDevice, nullptr);
            ze_device_properties_t deviceProperties{};
            deviceProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
            EXPECT_EQ(Device::fromHandle(hSubDevice)->getProperties(&deviceProperties), ZE_RESULT_SUCCESS);
            EXPECT_EQ(ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);
        }
    }

    multiDeviceFixture.tearDown();
}

TEST(DeviceReturnCompositeHierarchyTest, GivenCompositeHierarchyIsSetThenGetRootDeviceIsNotNullForSubDevices) {

    MultiDeviceFixtureCompositeHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_device_handle_t> hDevices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(count, multiDeviceFixture.numRootDevices);

    hDevices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

    for (auto &hDevice : hDevices) {

        uint32_t subDeviceCount = 0;
        EXPECT_EQ(Device::fromHandle(hDevice)->getSubDevices(&subDeviceCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(subDeviceCount, multiDeviceFixture.numSubDevices);
        std::vector<ze_device_handle_t> hSubDevices(multiDeviceFixture.numSubDevices);
        EXPECT_EQ(Device::fromHandle(hDevice)->getSubDevices(&subDeviceCount, hSubDevices.data()), ZE_RESULT_SUCCESS);

        for (auto &hSubDevice : hSubDevices) {
            ze_device_handle_t rootDevice = nullptr;
            EXPECT_EQ(Device::fromHandle(hSubDevice)->getRootDevice(&rootDevice), ZE_RESULT_SUCCESS);
            EXPECT_NE(rootDevice, nullptr);
            ze_device_properties_t deviceProperties{};
            deviceProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
            EXPECT_EQ(Device::fromHandle(hSubDevice)->getProperties(&deviceProperties), ZE_RESULT_SUCCESS);
            EXPECT_EQ(ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);
        }
    }

    multiDeviceFixture.tearDown();
}

template <NEO::DeviceHierarchyMode hierarchyMode>
struct MultiDeviceTest : public Test<MultiDeviceFixture> {
    void SetUp() override {
        this->deviceHierarchyMode = hierarchyMode;
        NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0,2,3");
        Test<MultiDeviceFixture>::SetUp();
        L0::globalDriverHandles->push_back(driverHandle->toHandle());
        L0::globalDriverHandles->push_back(nullptr);
    }
    void TearDown() override {
        L0::globalDriverHandles->clear();
        Test<MultiDeviceFixture>::TearDown();
    }
    void whenGettingDeviceIdentifiersThenMonoticallyIncreasingIdentifiersAreReturned() {
        uint32_t count = 0;
        std::vector<ze_device_handle_t> hDevices;
        EXPECT_EQ(driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

        EXPECT_EQ(3u, count);

        hDevices.resize(count);
        EXPECT_EQ(driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

        uint32_t expectedIdentifier = 0u;
        for (auto &hDevice : hDevices) {

            EXPECT_EQ(expectedIdentifier, Device::fromHandle(hDevice)->getIdentifier());
            EXPECT_EQ(expectedIdentifier, zerTranslateDeviceHandleToIdentifier(hDevice));

            EXPECT_EQ(hDevice, zerTranslateIdentifierToDeviceHandle(expectedIdentifier));

            expectedIdentifier++;
        }
        EXPECT_EQ(nullptr, zerTranslateIdentifierToDeviceHandle(expectedIdentifier));
    }

    DebugManagerStateRestore restorer;
};

using MultiDeviceCompositeTest = MultiDeviceTest<NEO::DeviceHierarchyMode::composite>;

TEST_F(MultiDeviceCompositeTest, whenGettingDeviceIdentifiersThenMonoticallyIncreasingIdentifiersAreReturned) {
    whenGettingDeviceIdentifiersThenMonoticallyIncreasingIdentifiersAreReturned();
}

using MultiDeviceCombinedTest = MultiDeviceTest<NEO::DeviceHierarchyMode::combined>;

TEST_F(MultiDeviceCombinedTest, whenGettingDeviceIdentifiersThenMonoticallyIncreasingIdentifiersAreReturned) {
    whenGettingDeviceIdentifiersThenMonoticallyIncreasingIdentifiersAreReturned();
}
using MultiDeviceFlatTest = MultiDeviceTest<NEO::DeviceHierarchyMode::flat>;

TEST_F(MultiDeviceFlatTest, whenGettingDeviceIdentifiersThenMonoticallyIncreasingIdentifiersAreReturned) {
    whenGettingDeviceIdentifiersThenMonoticallyIncreasingIdentifiersAreReturned();
}

TEST(SingleDeviceModeTest, GivenFlatHierarchyIsSetWhenGettingDevicesThenOnlySingleRootDeviceIsExposed) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ExposeSingleDevice.set(1);
    NEO::debugManager.flags.EnableImplicitScaling.set(1);

    MultiDeviceFixtureFlatHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_device_handle_t> hDevices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(count, multiDeviceFixture.numRootDevices);

    hDevices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

    for (auto &hDevice : hDevices) {

        uint32_t subDeviceCount = 0;
        EXPECT_EQ(Device::fromHandle(hDevice)->getSubDevices(&subDeviceCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(subDeviceCount, 0u);

        EXPECT_EQ(Device::fromHandle(hDevice)->getNEODevice(), static_cast<DeviceImp *>(Device::fromHandle(hDevice))->getActiveDevice());
    }

    multiDeviceFixture.tearDown();
}

TEST(SingleDeviceModeTest, GivenCombinedHierarchyIsSetWhenGettingDevicesThenOnlySingleRootDeviceIsExposed) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ExposeSingleDevice.set(1);
    NEO::debugManager.flags.EnableImplicitScaling.set(1);

    MultiDeviceFixtureCombinedHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_device_handle_t> hDevices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(count, multiDeviceFixture.numRootDevices);

    hDevices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

    for (auto &hDevice : hDevices) {

        uint32_t subDeviceCount = 0;
        EXPECT_EQ(Device::fromHandle(hDevice)->getSubDevices(&subDeviceCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(subDeviceCount, 0u);

        EXPECT_EQ(Device::fromHandle(hDevice)->getNEODevice(), static_cast<DeviceImp *>(Device::fromHandle(hDevice))->getActiveDevice());
    }

    multiDeviceFixture.tearDown();
}

TEST(SingleDeviceModeTest, GivenCompositeHierarchyIsSetWhenGettingDevicesThenOnlySingleRootDeviceIsExposed) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ExposeSingleDevice.set(1);
    NEO::debugManager.flags.EnableImplicitScaling.set(1);

    MultiDeviceFixtureCompositeHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_device_handle_t> hDevices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(count, multiDeviceFixture.numRootDevices);

    hDevices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

    for (auto &hDevice : hDevices) {

        uint32_t subDeviceCount = 0;
        EXPECT_EQ(Device::fromHandle(hDevice)->getSubDevices(&subDeviceCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(subDeviceCount, 0u);

        EXPECT_EQ(Device::fromHandle(hDevice)->getNEODevice(), static_cast<DeviceImp *>(Device::fromHandle(hDevice))->getActiveDevice());
    }

    multiDeviceFixture.tearDown();
}

TEST(SingleDeviceModeTest, GivenCompositeHierarchyWithAffinityMaskWhenGettingDevicesThenRootDeviceContainAllSubdevices) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ExposeSingleDevice.set(1);
    NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0.0,1.1,2.0,3.0");
    NEO::debugManager.flags.EnableImplicitScaling.set(1);

    MultiDeviceFixtureCompositeHierarchy multiDeviceFixture{};
    multiDeviceFixture.setUp();

    uint32_t count = 0;
    std::vector<ze_device_handle_t> hDevices;
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(count, multiDeviceFixture.numRootDevices);

    hDevices.resize(count);
    EXPECT_EQ(multiDeviceFixture.driverHandle->getDevice(&count, hDevices.data()), ZE_RESULT_SUCCESS);

    for (auto &hDevice : hDevices) {

        uint32_t subDeviceCount = 0;
        EXPECT_EQ(Device::fromHandle(hDevice)->getSubDevices(&subDeviceCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(subDeviceCount, 0u);

        EXPECT_EQ(2u, Device::fromHandle(hDevice)->getNEODevice()->getNumSubDevices());

        EXPECT_EQ(Device::fromHandle(hDevice)->getNEODevice(), static_cast<DeviceImp *>(Device::fromHandle(hDevice))->getActiveDevice());
    }

    multiDeviceFixture.tearDown();
}
using SingleDeviceModeTests = ::testing::Test;
HWTEST_F(SingleDeviceModeTests, givenContextGroupSupportedWhenGettingCsrsThenSecondaryContextsCreatedInRootDeviceForCCSAndInSubdevicesForBCS) {
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {

        const EngineInstancesContainer getGpgpuEngineInstances(const RootDeviceEnvironment &rootDeviceEnvironment) const override {
            auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
            EngineInstancesContainer engines;

            uint32_t numCcs = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

            if (hwInfo.featureTable.flags.ftrCCSNode) {
                for (uint32_t i = 0; i < numCcs; i++) {
                    auto engineType = static_cast<aub_stream::EngineType>(i + aub_stream::ENGINE_CCS);
                    engines.push_back({engineType, EngineUsage::regular});
                }

                engines.push_back({aub_stream::ENGINE_CCS, EngineUsage::lowPriority});
            }

            for (uint32_t i = 1; i < hwInfo.featureTable.ftrBcsInfo.size(); i++) {
                auto engineType = EngineHelpers::getBcsEngineAtIdx(i);

                if (hwInfo.featureTable.ftrBcsInfo.test(i)) {

                    engines.push_back({engineType, EngineUsage::regular});
                }
            }

            return engines;
        }
        EngineGroupType getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const override {
            if (engineType == aub_stream::ENGINE_RCS) {
                return EngineGroupType::renderCompute;
            }
            if (engineType >= aub_stream::ENGINE_CCS && engineType < (aub_stream::ENGINE_CCS + hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled)) {
                return EngineGroupType::compute;
            }

            if (engineType == aub_stream::ENGINE_BCS1) {
                return EngineGroupType::copy;
            }
            UNRECOVERABLE_IF(true);
        }
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.ContextGroupSize.set(8);
    debugManager.flags.CreateMultipleSubDevices.set(2);
    debugManager.flags.EnableImplicitScaling.set(1);

    const uint32_t rootDeviceIndex = 0u;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    hwInfo.featureTable.ftrBcsInfo = 0b10;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    MockExecutionEnvironment mockExecutionEnvironment{&hwInfo};
    for (auto rootDeviceIndex = 0u; rootDeviceIndex < mockExecutionEnvironment.rootDeviceEnvironments.size(); rootDeviceIndex++) {
        mockExecutionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->setExposeSingleDeviceMode(true);
    }
    mockExecutionEnvironment.incRefInternal();
    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*mockExecutionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);

    {
        auto *neoMockDevice = NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(&hwInfo, &mockExecutionEnvironment, rootDeviceIndex);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoMockDevice));
        auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));

        auto device = driverHandle->devices[0];

        uint32_t count = 0;
        device->getCommandQueueGroupProperties(&count, nullptr);
        ze_command_queue_group_properties_t queueGroupProperties[10] = {};
        count = std::min(count, 10u);
        device->getCommandQueueGroupProperties(&count, queueGroupProperties);

        auto ordinal = std::numeric_limits<uint32_t>::max();
        auto ordinalCopy = std::numeric_limits<uint32_t>::max();

        for (uint32_t i = 0; i < count; i++) {
            if ((ordinal == std::numeric_limits<uint32_t>::max()) &&
                (queueGroupProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE)) {
                ordinal = i;
            }
            if ((ordinalCopy == std::numeric_limits<uint32_t>::max()) &&
                !(queueGroupProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) &&
                (queueGroupProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY)) {
                ordinalCopy = i;
            }
        }
        ASSERT_NE(std::numeric_limits<uint32_t>::max(), ordinal);
        ASSERT_NE(std::numeric_limits<uint32_t>::max(), ordinalCopy);

        uint32_t index = 0;
        CommandStreamReceiver *csr = nullptr;
        auto result = device->getCsrForOrdinalAndIndex(&csr, ordinal, index, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, csr);

        auto &secondaryEngines = neoMockDevice->secondaryEngines[EngineHelpers::mapCcsIndexToEngineType(index)];

        ASSERT_EQ(8u, secondaryEngines.engines.size());

        auto highPriorityIndex = secondaryEngines.regularEnginesTotal;
        ASSERT_LT(highPriorityIndex, static_cast<uint32_t>(secondaryEngines.engines.size()));

        EXPECT_TRUE(secondaryEngines.engines[highPriorityIndex].osContext->isHighPriority());

        EXPECT_TRUE(csr->getOsContext().isPartOfContextGroup());
        EXPECT_NE(nullptr, secondaryEngines.engines[highPriorityIndex].osContext->getPrimaryContext());

        index = 100;
        result = device->getCsrForOrdinalAndIndex(&csr, ordinal, index, ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH, 0, false);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

        index = 0;
        ordinal = 100;
        result = device->getCsrForOrdinalAndIndex(&csr, ordinal, index, ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH, 0, false);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

        NEO::CommandStreamReceiver *bcsEngine = nullptr, *bcsEngine2 = nullptr;
        EXPECT_EQ(nullptr, neoMockDevice->getHpCopyEngine());

        result = device->getCsrForOrdinalAndIndex(&bcsEngine, ordinalCopy, index, ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH, 0, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, bcsEngine);
        EXPECT_TRUE(bcsEngine->getOsContext().isHighPriority());
        EXPECT_EQ(1u, bcsEngine->getOsContext().getDeviceBitfield().count());

        result = device->getCsrForOrdinalAndIndex(&bcsEngine2, ordinalCopy, index, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_EQ(bcsEngine2, bcsEngine->getPrimaryCsr());
        EXPECT_TRUE(bcsEngine2->getOsContext().getIsPrimaryEngine());
        EXPECT_EQ(1u, bcsEngine2->getOsContext().getDeviceBitfield().count());
        EXPECT_TRUE(bcsEngine2->getOsContext().getDeviceBitfield().test(0));

        bcsEngine2 = nullptr;
        result = device->getCsrForOrdinalAndIndex(&bcsEngine2, ordinalCopy + 1, index, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(bcsEngine2, bcsEngine->getPrimaryCsr());
        EXPECT_TRUE(bcsEngine2->getOsContext().getIsPrimaryEngine());
        EXPECT_TRUE(bcsEngine2->getOsContext().getDeviceBitfield().test(1));
    }
}

TEST_F(DeviceTest, GivenValidDeviceWhenQueryingKernelTimestampsProptertiesThenCorrectPropertiesIsReturned) {
    ze_device_properties_t devProps;
    ze_event_query_kernel_timestamps_ext_properties_t tsProps;

    devProps.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    devProps.pNext = &tsProps;

    tsProps.stype = ZE_STRUCTURE_TYPE_EVENT_QUERY_KERNEL_TIMESTAMPS_EXT_PROPERTIES;
    tsProps.pNext = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetProperties(device, &devProps));
    EXPECT_NE(0u, tsProps.flags & ZE_EVENT_QUERY_KERNEL_TIMESTAMPS_EXT_FLAG_KERNEL);
    EXPECT_NE(0u, tsProps.flags & ZE_EVENT_QUERY_KERNEL_TIMESTAMPS_EXT_FLAG_SYNCHRONIZED);
}

TEST_F(DeviceTest, givenDeviceWhenQueryingCmdListMemWaitOnMemDataSizeThenReturnValueFromHelper) {
    ze_device_properties_t devProps;
    ze_intel_device_command_list_wait_on_memory_data_size_exp_desc_t sizeProps = {ZE_INTEL_STRUCTURE_TYPE_DEVICE_COMMAND_LIST_WAIT_ON_MEMORY_DATA_SIZE_EXP_DESC};

    devProps.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    devProps.pNext = &sizeProps;

    auto &l0GfxCoreHelper = this->neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetProperties(device, &devProps));
    EXPECT_EQ(l0GfxCoreHelper.getCmdListWaitOnMemoryDataSize(), sizeProps.cmdListWaitOnMemoryDataSizeInBytes);
}

TEST_F(DeviceTest, givenDeviceWhenQueryingMediaPropertiesThenReturnZero) {
    ze_device_properties_t devProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_intel_device_media_exp_properties_t mediaProps = {ZE_STRUCTURE_TYPE_INTEL_DEVICE_MEDIA_EXP_PROPERTIES};
    mediaProps.numDecoderCores = 123;
    mediaProps.numEncoderCores = 456;

    devProps.pNext = &mediaProps;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetProperties(device, &devProps));
    EXPECT_EQ(0u, mediaProps.numDecoderCores);
    EXPECT_EQ(0u, mediaProps.numEncoderCores);
}

struct RTASDeviceTest : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
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

    struct MockOsLibrary : public OsLibrary {
      public:
        MockOsLibrary(const std::string &name, std::string *errorValue) {
        }
        MockOsLibrary() {}
        ~MockOsLibrary() override = default;
        void *getProcAddress(const std::string &procName) override {
            if (failGetProcAddress) {
                return nullptr;
            }
            return reinterpret_cast<void *>(0x1234);
        }
        bool isLoaded() override {
            return libraryLoaded;
        }
        std::string getFullPath() override {
            return std::string();
        }
        static OsLibrary *load(const NEO::OsLibraryCreateProperties &properties) {
            if (failLibraryLoad) {
                return nullptr;
            }
            auto ptr = new (std::nothrow) MockOsLibrary();
            return ptr;
        }

        static bool libraryLoaded;
        static bool failLibraryLoad;
        static bool failGetProcAddress;
    };

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::ExecutionEnvironment *execEnv;
    NEO::Device *neoDevice = nullptr;
    L0::Device *device = nullptr;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
};

bool RTASDeviceTest::MockOsLibrary::libraryLoaded = false;
bool RTASDeviceTest::MockOsLibrary::failLibraryLoad = false;
bool RTASDeviceTest::MockOsLibrary::failGetProcAddress = false;

HWTEST_F(RTASDeviceTest, GivenValidRTASLibraryWhenQueryingRTASProptertiesThenCorrectPropertiesIsReturned) {
    MockOsLibrary::libraryLoaded = false;
    MockOsLibrary::failLibraryLoad = false;
    MockOsLibrary::failGetProcAddress = false;

    ze_device_properties_t devProps = {};
    ze_rtas_device_exp_properties_t rtasProperties = {};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};
    driverHandle->rtasLibraryHandle.reset();

    devProps.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    devProps.pNext = &rtasProperties;

    rtasProperties.stype = ZE_STRUCTURE_TYPE_RTAS_DEVICE_EXP_PROPERTIES;
    rtasProperties.pNext = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetProperties(device, &devProps));
    EXPECT_EQ(128u, rtasProperties.rtasBufferAlignment);

    auto releaseHelper = this->neoDevice->getReleaseHelper();

    if (releaseHelper && releaseHelper->isRayTracingSupported()) {
        EXPECT_NE(ZE_RTAS_FORMAT_EXP_INVALID, rtasProperties.rtasFormat);
    }
}

HWTEST_F(RTASDeviceTest, GivenRTASLibraryPreLoadedWhenQueryingRTASProptertiesThenCorrectPropertiesIsReturned) {
    MockOsLibrary::libraryLoaded = false;
    MockOsLibrary::failLibraryLoad = false;
    MockOsLibrary::failGetProcAddress = false;

    ze_device_properties_t devProps = {};
    ze_rtas_device_exp_properties_t rtasProperties = {};
    driverHandle->rtasLibraryHandle = std::make_unique<MockOsLibrary>();

    devProps.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    devProps.pNext = &rtasProperties;

    rtasProperties.stype = ZE_STRUCTURE_TYPE_RTAS_DEVICE_EXP_PROPERTIES;
    rtasProperties.pNext = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetProperties(device, &devProps));
    EXPECT_EQ(128u, rtasProperties.rtasBufferAlignment);

    auto releaseHelper = this->neoDevice->getReleaseHelper();

    if (releaseHelper && releaseHelper->isRayTracingSupported()) {
        EXPECT_NE(ZE_RTAS_FORMAT_EXP_INVALID, rtasProperties.rtasFormat);
    }
}

HWTEST_F(RTASDeviceTest, GivenInvalidRTASLibraryWhenQueryingRTASProptertiesThenCorrectPropertiesIsReturned) {
    auto releaseHelper = this->neoDevice->getReleaseHelper();

    if (!releaseHelper || !releaseHelper->isRayTracingSupported()) {
        GTEST_SKIP();
    }
    MockOsLibrary::libraryLoaded = false;
    MockOsLibrary::failLibraryLoad = true;
    MockOsLibrary::failGetProcAddress = true;

    ze_device_properties_t devProps = {};
    ze_rtas_device_exp_properties_t rtasProperties = {};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};
    driverHandle->rtasLibraryHandle.reset();

    devProps.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    devProps.pNext = &rtasProperties;

    rtasProperties.stype = ZE_STRUCTURE_TYPE_RTAS_DEVICE_EXP_PROPERTIES;
    rtasProperties.pNext = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetProperties(device, &devProps));
    EXPECT_EQ(128u, rtasProperties.rtasBufferAlignment);
    EXPECT_EQ(ZE_RTAS_FORMAT_EXP_INVALID, rtasProperties.rtasFormat);
}

HWTEST_F(RTASDeviceTest, GivenMissingSymbolsInRTASLibraryWhenQueryingRTASProptertiesThenCorrectPropertiesIsReturned) {
    auto releaseHelper = this->neoDevice->getReleaseHelper();

    if (!releaseHelper || !releaseHelper->isRayTracingSupported()) {
        GTEST_SKIP();
    }
    MockOsLibrary::libraryLoaded = false;
    MockOsLibrary::failLibraryLoad = false;
    MockOsLibrary::failGetProcAddress = true;

    ze_device_properties_t devProps = {};
    ze_rtas_device_exp_properties_t rtasProperties = {};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};
    driverHandle->rtasLibraryHandle.reset();

    devProps.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    devProps.pNext = &rtasProperties;

    rtasProperties.stype = ZE_STRUCTURE_TYPE_RTAS_DEVICE_EXP_PROPERTIES;
    rtasProperties.pNext = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetProperties(device, &devProps));
    EXPECT_EQ(128u, rtasProperties.rtasBufferAlignment);
    EXPECT_EQ(ZE_RTAS_FORMAT_EXP_INVALID, rtasProperties.rtasFormat);
}

HWTEST_F(RTASDeviceTest, GivenValidRTASLibraryWhenQueryingRTASProptertiesExtThenCorrectPropertiesIsReturned) {
    MockOsLibrary::libraryLoaded = false;
    MockOsLibrary::failLibraryLoad = false;
    MockOsLibrary::failGetProcAddress = false;

    ze_device_properties_t devProps = {};
    ze_rtas_device_ext_properties_t rtasProperties = {};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};
    driverHandle->rtasLibraryHandle.reset();

    devProps.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    devProps.pNext = &rtasProperties;

    rtasProperties.stype = ZE_STRUCTURE_TYPE_RTAS_DEVICE_EXT_PROPERTIES;
    rtasProperties.pNext = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetProperties(device, &devProps));
    EXPECT_EQ(128u, rtasProperties.rtasBufferAlignment);

    auto releaseHelper = this->neoDevice->getReleaseHelper();

    if (releaseHelper && releaseHelper->isRayTracingSupported()) {
        EXPECT_NE(ZE_RTAS_FORMAT_EXT_INVALID, rtasProperties.rtasFormat);
    }
}

HWTEST_F(RTASDeviceTest, GivenRTASLibraryPreLoadedWhenQueryingRTASProptertiesExtThenCorrectPropertiesIsReturned) {
    MockOsLibrary::libraryLoaded = false;
    MockOsLibrary::failLibraryLoad = false;
    MockOsLibrary::failGetProcAddress = false;

    ze_device_properties_t devProps = {};
    ze_rtas_device_ext_properties_t rtasProperties = {};
    driverHandle->rtasLibraryHandle = std::make_unique<MockOsLibrary>();

    devProps.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    devProps.pNext = &rtasProperties;

    rtasProperties.stype = ZE_STRUCTURE_TYPE_RTAS_DEVICE_EXT_PROPERTIES;
    rtasProperties.pNext = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetProperties(device, &devProps));
    EXPECT_EQ(128u, rtasProperties.rtasBufferAlignment);

    auto releaseHelper = this->neoDevice->getReleaseHelper();

    if (releaseHelper && releaseHelper->isRayTracingSupported()) {
        EXPECT_NE(ZE_RTAS_FORMAT_EXT_INVALID, rtasProperties.rtasFormat);
    }
}

HWTEST_F(RTASDeviceTest, GivenInvalidRTASLibraryWhenQueryingRTASPropertiesExtThenCorrectPropertiesIsReturned) {
    auto releaseHelper = this->neoDevice->getReleaseHelper();

    if (!releaseHelper || !releaseHelper->isRayTracingSupported()) {
        GTEST_SKIP();
    }
    MockOsLibrary::libraryLoaded = false;
    MockOsLibrary::failLibraryLoad = true;
    MockOsLibrary::failGetProcAddress = true;

    ze_device_properties_t devProps = {};
    ze_rtas_device_ext_properties_t rtasProperties = {};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};
    driverHandle->rtasLibraryHandle.reset();

    devProps.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    devProps.pNext = &rtasProperties;

    rtasProperties.stype = ZE_STRUCTURE_TYPE_RTAS_DEVICE_EXT_PROPERTIES;
    rtasProperties.pNext = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetProperties(device, &devProps));
    EXPECT_EQ(128u, rtasProperties.rtasBufferAlignment);
    EXPECT_EQ(ZE_RTAS_FORMAT_EXT_INVALID, rtasProperties.rtasFormat);
}

HWTEST_F(RTASDeviceTest, GivenMissingSymbolsInRTASLibraryWhenQueryingRTASProptertiesExtThenCorrectPropertiesIsReturned) {
    auto releaseHelper = this->neoDevice->getReleaseHelper();

    if (!releaseHelper || !releaseHelper->isRayTracingSupported()) {
        GTEST_SKIP();
    }
    MockOsLibrary::libraryLoaded = false;
    MockOsLibrary::failLibraryLoad = false;
    MockOsLibrary::failGetProcAddress = true;

    ze_device_properties_t devProps = {};
    ze_rtas_device_ext_properties_t rtasProperties = {};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};
    driverHandle->rtasLibraryHandle.reset();

    devProps.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    devProps.pNext = &rtasProperties;

    rtasProperties.stype = ZE_STRUCTURE_TYPE_RTAS_DEVICE_EXT_PROPERTIES;
    rtasProperties.pNext = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetProperties(device, &devProps));
    EXPECT_EQ(128u, rtasProperties.rtasBufferAlignment);
    EXPECT_EQ(ZE_RTAS_FORMAT_EXT_INVALID, rtasProperties.rtasFormat);
}

TEST(ExtensionLookupTest, givenLookupMapWhenAskingForZexCommandListAppendWaitOnMemory64ThenReturnCorrectValue) {
    EXPECT_NE(nullptr, ExtensionFunctionAddressHelper::getExtensionFunctionAddress("zexCommandListAppendWaitOnMemory64"));
}

TEST(ExtensionLookupTest, givenLookupMapWhenAskingForBindlessImageExtensionFunctionsThenValidPointersReturned) {
    EXPECT_NE(nullptr, ExtensionFunctionAddressHelper::getExtensionFunctionAddress("zeMemGetPitchFor2dImage"));
    EXPECT_NE(nullptr, ExtensionFunctionAddressHelper::getExtensionFunctionAddress("zeImageGetDeviceOffsetExp"));
}

TEST(ExtensionLookupTest, givenLookupMapWhenAskingForZeIntelGetDriverVersionStringThenReturnCorrectValue) {
    EXPECT_NE(nullptr, ExtensionFunctionAddressHelper::getExtensionFunctionAddress("zeIntelGetDriverVersionString"));
}

template <bool blockLoad, bool blockStore>
class Mock2DTransposeProductHelper : public MockProductHelperHw<IGFX_UNKNOWN> {
  public:
    bool supports2DBlockLoad() const override {
        return blockLoad;
    }
    bool supports2DBlockStore() const override {
        return blockStore;
    }
};

template <bool blockLoad, bool blockStore>
class Mock2DTransposeDevice : public MockDeviceImp {
  public:
    using mockProductHelperType = Mock2DTransposeProductHelper<blockLoad, blockStore>;

    using MockDeviceImp::MockDeviceImp;

    const ProductHelper &getProductHelper() override {
        return *mockProductHelper;
    }

  private:
    std::unique_ptr<mockProductHelperType> mockProductHelper = std::make_unique<mockProductHelperType>();
};

TEST(ExtensionLookupTest, given2DBlockLoadFalseAnd2DBlockStoreFalseThenFlagsIndicateSupportsNeither) {
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(defaultHwInfo.get(), 0);
    Mock2DTransposeDevice<false, false> deviceImp(neoMockDevice);

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_intel_device_block_array_exp_properties_t blockArrayProps = {ZE_INTEL_DEVICE_BLOCK_ARRAY_EXP_PROPERTIES};
    deviceProps.pNext = &blockArrayProps;

    auto success = L0::Device::fromHandle(static_cast<ze_device_handle_t>(&deviceImp))->getProperties(&deviceProps);
    ASSERT_EQ(success, ZE_RESULT_SUCCESS);

    ASSERT_FALSE(static_cast<bool>(blockArrayProps.flags & ZE_INTEL_DEVICE_EXP_FLAG_2D_BLOCK_LOAD));
    ASSERT_FALSE(static_cast<bool>(blockArrayProps.flags & ZE_INTEL_DEVICE_EXP_FLAG_2D_BLOCK_STORE));
}

TEST(ExtensionLookupTest, given2DBlockLoadTrueAnd2DBlockStoreFalseThenFlagsIndicateSupportLoad) {
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(defaultHwInfo.get(), 0);
    Mock2DTransposeDevice<true, false> deviceImp(neoMockDevice);

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_intel_device_block_array_exp_properties_t blockArrayProps = {ZE_INTEL_DEVICE_BLOCK_ARRAY_EXP_PROPERTIES};
    deviceProps.pNext = &blockArrayProps;

    auto success = L0::Device::fromHandle(static_cast<ze_device_handle_t>(&deviceImp))->getProperties(&deviceProps);
    ASSERT_EQ(success, ZE_RESULT_SUCCESS);

    ASSERT_TRUE(static_cast<bool>(blockArrayProps.flags & ZE_INTEL_DEVICE_EXP_FLAG_2D_BLOCK_LOAD));
    ASSERT_FALSE(static_cast<bool>(blockArrayProps.flags & ZE_INTEL_DEVICE_EXP_FLAG_2D_BLOCK_STORE));
}

TEST(ExtensionLookupTest, given2DBlockLoadFalseAnd2DBlockStoreTrueThenFlagsIndicateSupportStore) {
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(defaultHwInfo.get(), 0);
    Mock2DTransposeDevice<false, true> deviceImp(neoMockDevice);

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_intel_device_block_array_exp_properties_t blockArrayProps = {ZE_INTEL_DEVICE_BLOCK_ARRAY_EXP_PROPERTIES};
    deviceProps.pNext = &blockArrayProps;

    auto success = L0::Device::fromHandle(static_cast<ze_device_handle_t>(&deviceImp))->getProperties(&deviceProps);
    ASSERT_EQ(success, ZE_RESULT_SUCCESS);

    ASSERT_FALSE(static_cast<bool>(blockArrayProps.flags & ZE_INTEL_DEVICE_EXP_FLAG_2D_BLOCK_LOAD));
    ASSERT_TRUE(static_cast<bool>(blockArrayProps.flags & ZE_INTEL_DEVICE_EXP_FLAG_2D_BLOCK_STORE));
}

TEST(ExtensionLookupTest, given2DBlockLoadTrueAnd2DBlockStoreTrueThenFlagsIndicateSupportBoth) {
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(defaultHwInfo.get(), 0);
    Mock2DTransposeDevice<true, true> deviceImp(neoMockDevice);

    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_intel_device_block_array_exp_properties_t blockArrayProps = {ZE_INTEL_DEVICE_BLOCK_ARRAY_EXP_PROPERTIES};
    deviceProps.pNext = &blockArrayProps;

    auto success = L0::Device::fromHandle(static_cast<ze_device_handle_t>(&deviceImp))->getProperties(&deviceProps);
    ASSERT_EQ(success, ZE_RESULT_SUCCESS);

    ASSERT_TRUE(static_cast<bool>(blockArrayProps.flags & ZE_INTEL_DEVICE_EXP_FLAG_2D_BLOCK_LOAD));
    ASSERT_TRUE(static_cast<bool>(blockArrayProps.flags & ZE_INTEL_DEVICE_EXP_FLAG_2D_BLOCK_STORE));
}

TEST_F(DeviceSimpleTests, whenWorkgroupSizeCheckedThenSizeLimitIs1kOrLess) {
    ze_device_compute_properties_t properties{};
    auto result = device->getComputeProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_LE(properties.maxTotalGroupSize, CommonConstants::maxWorkgroupSize);
}

HWTEST_F(DeviceSimpleTests, givenGpuHangWhenSynchronizingDeviceThenErrorIsPropagated) {
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.resourcesInitialized = true;
    csr.waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::gpuHang;

    auto result = zeDeviceSynchronize(device);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
}

HWTEST_F(DeviceSimpleTests, givenNoGpuHangWhenSynchronizingDeviceThenCallWaitForTaskCountWithKmdNotifyFallbackOnEachCsr) {
    auto &engines = neoDevice->getAllEngines();

    TaskCountType taskCountToWait = 1u;
    FlushStamp flushStampToWait = 4u;
    for (auto &engine : engines) {
        auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(engine.commandStreamReceiver);
        csr->latestSentTaskCount = 0u;
        csr->latestFlushedTaskCount = 0u;
        csr->taskCount = taskCountToWait++;
        csr->flushStamp->setStamp(flushStampToWait++);
        csr->captureWaitForTaskCountWithKmdNotifyInputParams = true;
        csr->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::ready;
        csr->resourcesInitialized = true;
    }

    auto &secondaryCsrs = neoDevice->getSecondaryCsrs();
    for (auto &secondaryCsr : secondaryCsrs) {
        auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(secondaryCsr.get());
        csr->latestSentTaskCount = 0u;
        csr->latestFlushedTaskCount = 0u;
        csr->taskCount = taskCountToWait++;
        csr->flushStamp->setStamp(flushStampToWait++);
        csr->captureWaitForTaskCountWithKmdNotifyInputParams = true;
        csr->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::ready;
        csr->resourcesInitialized = true;
    }

    auto result = zeDeviceSynchronize(device);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    for (auto &engine : engines) {
        auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(engine.commandStreamReceiver);
        EXPECT_EQ(1u, csr->waitForTaskCountWithKmdNotifyInputParams.size());
        EXPECT_EQ(csr->taskCount, csr->waitForTaskCountWithKmdNotifyInputParams[0].taskCountToWait);
        EXPECT_EQ(csr->flushStamp->peekStamp(), csr->waitForTaskCountWithKmdNotifyInputParams[0].flushStampToWait);
        EXPECT_FALSE(csr->waitForTaskCountWithKmdNotifyInputParams[0].useQuickKmdSleep);
        EXPECT_EQ(NEO::QueueThrottle::MEDIUM, csr->waitForTaskCountWithKmdNotifyInputParams[0].throttle);
    }

    for (auto &secondaryCsr : secondaryCsrs) {
        auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(secondaryCsr.get());
        EXPECT_EQ(1u, csr->waitForTaskCountWithKmdNotifyInputParams.size());
        EXPECT_EQ(csr->taskCount, csr->waitForTaskCountWithKmdNotifyInputParams[0].taskCountToWait);
        EXPECT_EQ(csr->flushStamp->peekStamp(), csr->waitForTaskCountWithKmdNotifyInputParams[0].flushStampToWait);
        EXPECT_FALSE(csr->waitForTaskCountWithKmdNotifyInputParams[0].useQuickKmdSleep);
        EXPECT_EQ(NEO::QueueThrottle::MEDIUM, csr->waitForTaskCountWithKmdNotifyInputParams[0].throttle);
    }
}

HWTEST_F(DeviceSimpleTests, whenSynchronizingDeviceThenIgnoreUninitializedCsrs) {
    auto &engines = neoDevice->getAllEngines();

    TaskCountType taskCountToWait = 1u;
    FlushStamp flushStampToWait = 4u;
    for (auto &engine : engines) {
        auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(engine.commandStreamReceiver);
        csr->latestSentTaskCount = 0u;
        csr->latestFlushedTaskCount = 0u;
        csr->taskCount = taskCountToWait++;
        csr->flushStamp->setStamp(flushStampToWait++);
        csr->captureWaitForTaskCountWithKmdNotifyInputParams = true;
        csr->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::gpuHang;
        csr->resourcesInitialized = false;
    }

    auto &secondaryCsrs = neoDevice->getSecondaryCsrs();
    for (auto &secondaryCsr : secondaryCsrs) {
        auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(secondaryCsr.get());
        csr->latestSentTaskCount = 0u;
        csr->latestFlushedTaskCount = 0u;
        csr->taskCount = taskCountToWait++;
        csr->flushStamp->setStamp(flushStampToWait++);
        csr->captureWaitForTaskCountWithKmdNotifyInputParams = true;
        csr->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::gpuHang;
        csr->resourcesInitialized = false;
    }

    auto result = zeDeviceSynchronize(device);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    for (auto &engine : engines) {
        auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(engine.commandStreamReceiver);
        EXPECT_EQ(0u, csr->waitForTaskCountWithKmdNotifyInputParams.size());
    }

    for (auto &secondaryCsr : secondaryCsrs) {
        auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(secondaryCsr.get());
        EXPECT_EQ(0u, csr->waitForTaskCountWithKmdNotifyInputParams.size());
    }
}

HWTEST_F(DeviceSimpleTests, givenGpuHangOnSecondaryCsrWhenSynchronizingDeviceThenErrorIsPropagated) {
    if (neoDevice->getSecondaryCsrs().empty()) {
        GTEST_SKIP();
    }
    auto &engines = neoDevice->getAllEngines();

    for (auto &engine : engines) {
        auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(engine.commandStreamReceiver);
        csr->resourcesInitialized = true;
        csr->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::ready;
    }

    auto &secondaryCsrs = neoDevice->getSecondaryCsrs();
    for (auto &secondaryCsr : secondaryCsrs) {
        auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(secondaryCsr.get());
        csr->resourcesInitialized = true;
        csr->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::gpuHang;
    }

    auto result = zeDeviceSynchronize(device);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
}

HWTEST2_F(DeviceSimpleTests, givenDeviceWhenQueryingPriorityLevelsThenHighAndLowLevelsAreReturned, IsAtMostXe3Core) {

    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {
        uint32_t getQueuePriorityLevels() const override {
            return numPriorityLevels;
        }

        uint32_t numPriorityLevels = 1;
    };

    auto rootDeviceIndex = device->getNEODevice()->getRootDeviceIndex();
    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]);

    for (int numLevels = 1; numLevels < 10; numLevels++) {
        raii.mockGfxCoreHelper->numPriorityLevels = numLevels;
        int highestPriorityLevel = 0;
        int lowestPriorityLevel = 0;

        ze_result_t returnValue = ZE_RESULT_SUCCESS;
        auto l0Device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice, false, &returnValue));
        ASSERT_NE(nullptr, l0Device);

        EXPECT_EQ(ZE_RESULT_SUCCESS, l0Device->getPriorityLevels(&lowestPriorityLevel, &highestPriorityLevel));

        EXPECT_EQ(numLevels, lowestPriorityLevel - highestPriorityLevel + 1);
        if (numLevels == 1) {
            EXPECT_EQ(0, highestPriorityLevel);
            EXPECT_EQ(0, lowestPriorityLevel);
        } else if (numLevels == 2) {
            EXPECT_EQ(0, highestPriorityLevel);
            EXPECT_EQ(1, lowestPriorityLevel);
        } else if (numLevels == 3) {
            EXPECT_EQ(-1, highestPriorityLevel);
            EXPECT_EQ(1, lowestPriorityLevel);
        } else if (numLevels == 4) {
            EXPECT_EQ(-1, highestPriorityLevel);
            EXPECT_EQ(2, lowestPriorityLevel);
        }
    }
}

HWTEST2_F(DeviceSimpleTests, givenDeviceWhenQueryingPriorityLevelsThen2LevelsAreReturned, IsAtMostXe3Core) {

    int32_t highestPriorityLevel = 0;
    int32_t lowestPriorityLevel = 0;

    auto result = zeDeviceGetPriorityLevels(device, &lowestPriorityLevel, &highestPriorityLevel);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0, highestPriorityLevel);
    EXPECT_EQ(1, lowestPriorityLevel);
}

struct L0DeviceGetCmdlistCreateFunFixture {
    using CmdListCreateFunPtrT = L0::DeviceImp::CmdListCreateFunPtrT;
    void setUp() {
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
        auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0);
        mockDeviceImp = std::make_unique<MockDeviceImp>(neoMockDevice);
        ASSERT_NE(nullptr, mockDeviceImp);
        deviceImp = static_cast<L0::DeviceImp *>(mockDeviceImp.get());
        desc.pNext = nullptr;
    }
    void tearDown() {}

    ze_command_list_desc_t desc{};
    L0::DeviceImp *deviceImp = nullptr;
    ze_mutable_command_list_exp_desc_t mutableExpDesc{};

    const CmdListCreateFunPtrT mutableCreateAddress = &L0::MCL::MutableCommandList::create;

  protected:
    std::unique_ptr<MockDeviceImp> mockDeviceImp;
};

using L0DeviceGetCmdlistCreateFunTest = Test<L0DeviceGetCmdlistCreateFunFixture>;

TEST_F(L0DeviceGetCmdlistCreateFunTest, GivenNoExtReturnBaseCommandListCreateFun) {
    EXPECT_EQ(nullptr, deviceImp->getCmdListCreateFunc(nullptr));
}

TEST_F(L0DeviceGetCmdlistCreateFunTest, GivenOtherExtReturnBaseCommandListCreateFun) {
    ze_command_list_desc_t otherDesc{ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};

    EXPECT_EQ(nullptr, deviceImp->getCmdListCreateFunc(reinterpret_cast<const ze_base_desc_t *>(&otherDesc)));
}

TEST_F(L0DeviceGetCmdlistCreateFunTest, GivenMclExpDescriptorReturnMclCreateFun) {
    ze_mutable_command_list_exp_desc_t mutableExpDesc{ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_LIST_EXP_DESC};
    EXPECT_EQ(mutableCreateAddress, deviceImp->getCmdListCreateFunc(reinterpret_cast<const ze_base_desc_t *>(&mutableExpDesc)));
}

TEST_F(L0DeviceGetCmdlistCreateFunTest, GivenQueryDeviceMclPropertiesWhenReturnMclPropertiesThenProvidePerDeviceCapability) {
    ze_mutable_command_list_exp_properties_t mclDeviceProperties{ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_LIST_EXP_PROPERTIES};

    ze_device_properties_t deviceProperties{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    deviceProperties.pNext = &mclDeviceProperties;

    deviceImp->getProperties(&deviceProperties);

    uint32_t deviceMclCapability = deviceImp->getL0GfxCoreHelper().getCmdListUpdateCapabilities(deviceImp->getNEODevice()->getRootDeviceEnvironment());
    EXPECT_EQ(deviceMclCapability, mclDeviceProperties.mutableCommandFlags);
}

TEST_F(L0DeviceGetCmdlistCreateFunTest, GivenQueryDeviceRecordReplayGraphWhenReturnReplayGraphPropertiesThenProvidePerDeviceCapability) {
    ze_record_replay_graph_exp_properties_t recordReplayGraphProperties{ZE_STRUCTURE_TYPE_RECORD_REPLAY_GRAPH_EXP_PROPERTIES};

    ze_device_properties_t deviceProperties{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    deviceProperties.pNext = &recordReplayGraphProperties;

    deviceImp->getProperties(&deviceProperties);

    uint32_t deviceRecordReplayGraphCapability = deviceImp->getL0GfxCoreHelper().getRecordReplayGraphCapabilities(deviceImp->getNEODevice()->getRootDeviceEnvironment());
    EXPECT_EQ(deviceRecordReplayGraphCapability, recordReplayGraphProperties.graphFlags);
}

} // namespace ult
} // namespace L0
