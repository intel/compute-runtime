/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "shared/test/common/mocks/ult_device_factory.h"

#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace L0 {
namespace ult {

void DeviceFixture::SetUp() { // NOLINT(readability-identifier-naming)
    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];

    ze_context_handle_t hContext;
    ze_context_desc_t desc;
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
    ze_context_desc_t desc;
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
    ze_context_desc_t desc;
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

    hwInfo.gtSystemInfo.MaxSlicesSupported = 2 * sliceCount;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = 2 * subsliceCount;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = 2 * subsliceCount;

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

} // namespace ult
} // namespace L0
