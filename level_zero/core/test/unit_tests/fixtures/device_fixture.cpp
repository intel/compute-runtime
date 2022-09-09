/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/mocks/mock_cpu_page_fault_manager.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace L0 {
namespace ult {

void DeviceFixture::setUp() {
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(NEO::defaultHwInfo.get(), 0u);
    setupWithExecutionEnvironment(*executionEnvironment);
}
void DeviceFixture::setupWithExecutionEnvironment(NEO::ExecutionEnvironment &executionEnvironment) {
    execEnv = &executionEnvironment;
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
    executionEnvironment.incRefInternal();
}

void DeviceFixture::tearDown() {
    context->destroy();
    driverHandle.reset(nullptr);
    execEnv->decRefInternal();
}

void PageFaultDeviceFixture::setUp() {
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

void PageFaultDeviceFixture::tearDown() {
    device->getDriverHandle()->setMemoryManager(memoryManager);
    context->destroy();
}

void MultiDeviceFixture::setUp() {
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

void MultiDeviceFixture::tearDown() {
    context->destroy();
}

void MultipleDevicesWithCustomHwInfo::setUp() {

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

    ASSERT_FALSE(numSubDevices == 0 || numSubDevices > 4);

    hwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = numSubDevices > 0;
    hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = numSubDevices;
    hwInfo.gtSystemInfo.MultiTileArchInfo.Tile0 = numSubDevices >= 1;
    hwInfo.gtSystemInfo.MultiTileArchInfo.Tile1 = numSubDevices >= 2;
    hwInfo.gtSystemInfo.MultiTileArchInfo.Tile2 = numSubDevices >= 3;
    hwInfo.gtSystemInfo.MultiTileArchInfo.Tile3 = numSubDevices == 4;
    hwInfo.gtSystemInfo.MultiTileArchInfo.TileMask = static_cast<uint8_t>(maxNBitValue(numSubDevices));

    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(&hwInfo);
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
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

void SingleRootMultiSubDeviceFixture::setUp() {
    MultiDeviceFixture::numRootDevices = 1u;
    MultiDeviceFixture::setUp();

    device = driverHandle->devices[0];
    neoDevice = device->getNEODevice();
}
void SingleRootMultiSubDeviceFixtureWithImplicitScalingImpl::setUp() {
    DebugManager.flags.EnableImplicitScaling.set(implicitScaling);
    DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.featureTable.flags.ftrCCSNode = true;

    if (expectedCopyEngineCount != 0) {
        hwInfo.capabilityTable.blitterOperationsSupported = true;
        hwInfo.featureTable.ftrBcsInfo = maxNBitValue(expectedCopyEngineCount);
    } else {
        hwInfo.capabilityTable.blitterOperationsSupported = false;
    }

    if (implicitScaling) {
        expectedComputeEngineCount = 1u;
    } else {
        expectedComputeEngineCount = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
    }

    MockDevice *mockDevice = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(mockDevice));

    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    ze_result_t res = driverHandle->initialize(std::move(devices));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    device = driverHandle->devices[0];
    neoDevice = device->getNEODevice();
    deviceImp = static_cast<L0::DeviceImp *>(device);

    NEO::Device *activeDevice = deviceImp->getActiveDevice();
    auto &engineGroups = activeDevice->getRegularEngineGroups();
    numEngineGroups = static_cast<uint32_t>(engineGroups.size());

    if (activeDevice->getSubDevices().size() > 0) {
        NEO::Device *activeSubDevice = activeDevice->getSubDevice(0u);
        (void)activeSubDevice;
        auto &subDeviceEngineGroups = activeSubDevice->getRegularEngineGroups();
        (void)subDeviceEngineGroups;

        for (uint32_t i = 0; i < subDeviceEngineGroups.size(); i++) {
            if (subDeviceEngineGroups[i].engineGroupType == NEO::EngineGroupType::Copy ||
                subDeviceEngineGroups[i].engineGroupType == NEO::EngineGroupType::LinkedCopy) {
                subDeviceNumEngineGroups += 1;
            }
        }
    }
}

void SingleRootMultiSubDeviceFixtureWithImplicitScalingImpl::tearDown() {
    context->destroy();
}

void GetMemHandlePtrTestFixture::setUp() {

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
    context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
    auto neoDevice = device->getNEODevice();
    context->rootDeviceIndices.push_back(neoDevice->getRootDeviceIndex());
    context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
}

void GetMemHandlePtrTestFixture::tearDown() {
    driverHandle->setMemoryManager(prevMemoryManager);
    delete currMemoryManager;
}

PageFaultDeviceFixture::PageFaultDeviceFixture() = default;
PageFaultDeviceFixture::~PageFaultDeviceFixture() = default;
} // namespace ult
} // namespace L0
