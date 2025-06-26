/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/mocks/mock_cpu_page_fault_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

void DeviceFixture::setUp() {
    hardwareInfo = defaultHwInfo.get();
    setUpImpl(hardwareInfo);
}

void DeviceFixture::setUpImpl(NEO::HardwareInfo *hwInfo) {
    hardwareInfo = hwInfo;
    auto executionEnvironment = NEO::MockDevice::prepareExecutionEnvironment(hardwareInfo, 0u);
    setupWithExecutionEnvironment(*executionEnvironment);
}

void DeviceFixture::setupWithExecutionEnvironment(NEO::ExecutionEnvironment &executionEnvironment) {
    execEnv = &executionEnvironment;
    neoDevice = NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(hardwareInfo == nullptr ? defaultHwInfo.get() : hardwareInfo, &executionEnvironment, rootDeviceIndex);
    mockBuiltIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
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
    if (neoDevice->getPreemptionMode() == NEO::PreemptionMode::MidThread) {
        for (auto &engine : neoDevice->getAllEngines()) {
            NEO::CommandStreamReceiver *csr = engine.commandStreamReceiver;
            if (!csr->getPreemptionAllocation()) {
                csr->createPreemptionAllocation();
            }
        }
    }
}

void DeviceFixture::tearDown() {
    context->destroy();
    driverHandle.reset(nullptr);
    execEnv->decRefInternal();
}

template <typename HelperType>
HelperType &DeviceFixture::getHelper() const {
    auto &helper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<HelperType>();
    return helper;
}

template L0GfxCoreHelper &DeviceFixture::getHelper() const;
template NEO::ProductHelper &DeviceFixture::getHelper() const;
template NEO::CompilerProductHelper &DeviceFixture::getHelper() const;

void PageFaultDeviceFixture::setUp() {
    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
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
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
    auto executionEnvironment = new NEO::ExecutionEnvironment;
    executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
    for (size_t i = 0; i < numRootDevices; ++i) {
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<MockMemoryOperations>();
    }
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

void MultiDeviceFixtureHierarchy::setUp() {
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
    auto executionEnvironment = new NEO::ExecutionEnvironment;
    executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
    for (size_t i = 0; i < numRootDevices; ++i) {
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<MockMemoryOperations>();
    }

    executionEnvironment->setDeviceHierarchyMode(deviceHierarchyMode);

    auto devices = NEO::DeviceFactory::createDevices(*executionEnvironment);
    if (devices.size()) {
        ze_result_t res = driverHandle->initialize(std::move(devices));
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        ze_context_handle_t hContext;
        ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        context = static_cast<ContextImp *>(Context::fromHandle(hContext));
    } else {
        delete executionEnvironment;
        return;
    }
}

void MultipleDevicesWithCustomHwInfo::setUp() {

    VariableBackup<bool> mockDeviceFlagBackup(&NEO::MockDevice::createSingleDevice, false);

    std::vector<std::unique_ptr<NEO::Device>> devices;
    NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);

    hwInfo = *NEO::defaultHwInfo.get();

    hwInfo.gtSystemInfo.SliceCount = sliceCount;
    for (uint32_t slice = 0; slice < sliceCount; slice++) {
        hwInfo.gtSystemInfo.SliceInfo[slice].Enabled = true;
    }
    for (uint32_t slice = sliceCount; slice < GT_MAX_SLICE; slice++) {
        hwInfo.gtSystemInfo.SliceInfo[slice].Enabled = false;
    }
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
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(&hwInfo);
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
    debugManager.flags.EnableImplicitScaling.set(implicitScaling);
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);

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

    NEO::MockDevice *mockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0);

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
            if (subDeviceEngineGroups[i].engineGroupType == NEO::EngineGroupType::copy ||
                subDeviceEngineGroups[i].engineGroupType == NEO::EngineGroupType::linkedCopy) {
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
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
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
    context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
    context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
}

void GetMemHandlePtrTestFixture::tearDown() {
    driverHandle->setMemoryManager(prevMemoryManager);
    delete currMemoryManager;
}

PageFaultDeviceFixture::PageFaultDeviceFixture() = default;
PageFaultDeviceFixture::~PageFaultDeviceFixture() = default;

void ExtensionFixture::setUp() {
    DeviceFixture::setUp();

    uint32_t count = 0;
    ze_result_t res = DeviceFixture::driverHandle->getExtensionProperties(&count, nullptr);
    EXPECT_NE(0u, count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    extensionProperties.resize(count);
    res = DeviceFixture::driverHandle->getExtensionProperties(&count, extensionProperties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

void ExtensionFixture::tearDown() {
    DeviceFixture::tearDown();
}

void ExtensionFixture::verifyExtensionDefinition(const char *extName, unsigned int extVersion) {
    auto it = std::find_if(extensionProperties.begin(), extensionProperties.end(), [&extName](const auto &extension) { return (strcmp(extension.name, extName) == 0); });
    EXPECT_NE(it, extensionProperties.end());
    EXPECT_EQ((*it).version, extVersion);
}

} // namespace ult
} // namespace L0
