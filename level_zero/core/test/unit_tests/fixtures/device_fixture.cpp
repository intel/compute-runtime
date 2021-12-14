/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/unit_test/page_fault_manager/mock_cpu_page_fault_manager.h"

#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace L0 {
namespace ult {

void DeviceFixture::SetUp() { // NOLINT(readability-identifier-naming)
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(NEO::defaultHwInfo.get(), 0u);
    setupWithExecutionEnvironment(*executionEnvironment);
}
void DeviceFixture::setupWithExecutionEnvironment(NEO::ExecutionEnvironment &executionEnvironment) {
    neoDevice = NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), &executionEnvironment, 0u);
    mockBuiltIns = new MockBuiltins();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    context = static_cast<ContextImp *>(Context::fromHandle(hContext));
}

void DeviceFixture::TearDown() { // NOLINT(readability-identifier-naming)
    context->destroy();
}

void PageFaultDeviceFixture::SetUp() { // NOLINT(readability-identifier-naming)
    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    context = static_cast<ContextImp *>(Context::fromHandle(hContext));
    mockPageFaultManager = new MockPageFaultManager;
    mockMemoryManager = std::make_unique<MockMemoryManager>();
    memoryManager = device->getDriverHandle()->getMemoryManager();
    mockMemoryManager->pageFaultManager.reset(mockPageFaultManager);
    device->getDriverHandle()->setMemoryManager(mockMemoryManager.get());
}

void PageFaultDeviceFixture::TearDown() { // NOLINT(readability-identifier-naming)
    device->getDriverHandle()->setMemoryManager(memoryManager);
    context->destroy();
}

void MultiDeviceFixture::SetUp() { // NOLINT(readability-identifier-naming)
    DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
    auto executionEnvironment = new NEO::ExecutionEnvironment;
    auto devices = NEO::DeviceFactory::createDevices(*executionEnvironment);
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    ze_result_t res = driverHandle->initialize(std::move(devices));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    context = static_cast<ContextImp *>(Context::fromHandle(hContext));
}

void MultiDeviceFixture::TearDown() { // NOLINT(readability-identifier-naming)
    context->destroy();
}

void ContextFixture::SetUp() {
    DeviceFixture::SetUp();
}

void ContextFixture::TearDown() {
    DeviceFixture::TearDown();
}

void MultipleDevicesWithCustomHwInfo::SetUp() {
    NEO::MockCompilerEnableGuard mock(true);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

    std::vector<std::unique_ptr<NEO::Device>> devices;
    NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);

    hwInfo = *NEO::defaultHwInfo.get();

    hwInfo.gtSystemInfo.SliceCount = sliceCount;
    hwInfo.gtSystemInfo.SubSliceCount = subsliceCount;
    hwInfo.gtSystemInfo.EUCount = subsliceCount * numEuPerSubslice;
    hwInfo.gtSystemInfo.ThreadCount = subsliceCount * numEuPerSubslice * numThreadsPerEu;

    hwInfo.gtSystemInfo.MaxEuPerSubSlice = numEuPerSubslice;
    hwInfo.gtSystemInfo.NumThreadsPerEu = numThreadsPerEu;

    hwInfo.gtSystemInfo.MaxSlicesSupported = sliceCount;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = sliceCount * subsliceCount;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = sliceCount * subsliceCount;

    hwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = numSubDevices;
    hwInfo.gtSystemInfo.MultiTileArchInfo.Tile0 = 1;
    hwInfo.gtSystemInfo.MultiTileArchInfo.Tile1 = 1;

    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(&hwInfo);
    }

    memoryManager = new NEO::OsAgnosticMemoryManager(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);

    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
    }
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
}

void SingleRootMultiSubDeviceFixture::SetUp() {
    MultiDeviceFixture::numRootDevices = 1u;
    MultiDeviceFixture::SetUp();

    device = driverHandle->devices[0];
    neoDevice = device->getNEODevice();
}

void GetMemHandlePtrTestFixture::SetUp() {
    NEO::MockCompilerEnableGuard mock(true);
    neoDevice =
        NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<DriverHandleGetMemHandlePtrMock>();
    driverHandle->initialize(std::move(devices));
    prevMemoryManager = driverHandle->getMemoryManager();
    currMemoryManager = new MemoryManagerMemHandleMock();
    driverHandle->setMemoryManager(currMemoryManager);
    device = driverHandle->devices[0];

    context = std::make_unique<L0::ContextImp>(driverHandle.get());
    EXPECT_NE(context, nullptr);
    context->getDevices().insert(std::make_pair(device->toHandle(), device));
    auto neoDevice = device->getNEODevice();
    context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
    context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
}

void GetMemHandlePtrTestFixture::TearDown() {
    driverHandle->setMemoryManager(prevMemoryManager);
    delete currMemoryManager;
}

} // namespace ult
} // namespace L0
