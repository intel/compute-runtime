/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/linux/mock_drm_command_stream_receiver.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "hw_cmds_default.h"

namespace NEO {

void DrmMemoryManagerBasic::SetUp() {
    for (auto i = 0u; i < numRootDevices; i++) {
        executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment.rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        UnitTestSetter::setCcsExposure(*executionEnvironment.rootDeviceEnvironments[i]);
        UnitTestSetter::setRcsExposure(*executionEnvironment.rootDeviceEnvironments[i]);

        auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[i]);
        executionEnvironment.rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
        executionEnvironment.rootDeviceEnvironments[i]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, i, false);
        executionEnvironment.rootDeviceEnvironments[i]->initGmm();
    }
    executionEnvironment.calculateMaxOsContextCount();
}

void DrmMemoryManagerFixture::setUp() {
    MemoryManagementFixture::setUp();

    executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), numRootDevices - 1);
    setUp(DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[0]).release(), false);
} // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

void DrmMemoryManagerFixture::setUp(DrmMockCustom *mock, bool localMemoryEnabled) {
    ASSERT_NE(nullptr, executionEnvironment);
    executionEnvironment->incRefInternal();
    debugManager.flags.DeferOsContextInitialization.set(0);
    debugManager.flags.SetAmountOfReusableAllocations.set(0);

    environmentWrapper.setCsrType<TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>>();
    allocationData.rootDeviceIndex = rootDeviceIndex;
    this->mock = mock;
    for (auto i = 0u; i < numRootDevices; i++) {
        auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[i].get();
        rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
        UnitTestSetter::setCcsExposure(*executionEnvironment->rootDeviceEnvironments[i]);
        UnitTestSetter::setRcsExposure(*executionEnvironment->rootDeviceEnvironments[i]);

        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->setDriverModel(DrmMockCustom::create(*rootDeviceEnvironment));
        rootDeviceEnvironment->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*rootDeviceEnvironment->osInterface->getDriverModel()->as<Drm>(), i, false);
        MockRootDeviceEnvironment::resetBuiltins(rootDeviceEnvironment, new MockBuiltins);
        rootDeviceEnvironment->initGmm();
    }

    executionEnvironment->calculateMaxOsContextCount();
    rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get();
    rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));

    memoryManager = new (std::nothrow) TestedDrmMemoryManager(localMemoryEnabled, false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    // assert we have memory manager
    ASSERT_NE(nullptr, memoryManager);
    if (memoryManager->getgemCloseWorker()) {
        memoryManager->getgemCloseWorker()->close(true);
    }
    device = MockDevice::create<MockDevice>(executionEnvironment, rootDeviceIndex);
    mock->reset();
}

void DrmMemoryManagerFixture::tearDown() {
    mock->testIoctls();
    mock->reset();

    int enginesCount = 0;

    for (auto &engineContainer : memoryManager->getRegisteredEngines()) {
        enginesCount += engineContainer.size();
    }

    mock->ioctlExpected.contextDestroy = enginesCount;
    mock->ioctlExpected.gemClose = enginesCount;
    mock->ioctlExpected.gemWait = enginesCount;

    auto csr = static_cast<TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *>(device->getDefaultEngine().commandStreamReceiver);
    if (csr->globalFenceAllocation) {
        mock->ioctlExpected.gemClose += enginesCount;
        mock->ioctlExpected.gemWait += enginesCount;
    }
    if (csr->getPreemptionAllocation()) {
        mock->ioctlExpected.gemClose += enginesCount;
        mock->ioctlExpected.gemWait += enginesCount;
    }

    auto &compilerProductHelper = device->getCompilerProductHelper();
    auto isHeapless = compilerProductHelper.isHeaplessModeEnabled();
    auto isHeaplessStateInit = compilerProductHelper.isHeaplessStateInitEnabled(isHeapless);
    if (isHeaplessStateInit) {
        mock->ioctlExpected.gemClose += 1;
        mock->ioctlExpected.gemWait += 1;
    }

    mock->ioctlExpected.gemWait += additionalDestroyDeviceIoctls.gemWait.load();
    mock->ioctlExpected.gemClose += additionalDestroyDeviceIoctls.gemClose.load();
    delete device;
    if (dontTestIoctlInTearDown) {
        mock->reset();
    }
    mock->testIoctls();
    executionEnvironment->decRefInternal();
    MemoryManagementFixture::tearDown();
    SysCalls::mmapVector.clear();
}

void DrmMemoryManagerWithLocalMemoryFixture::setUp() {
    backup = std::make_unique<VariableBackup<UltHwConfig>>(&ultHwConfig);
    ultHwConfig.csrBaseCallCreatePreemption = true;

    MemoryManagementFixture::setUp();

    executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), numRootDevices - 1);

    executionEnvironment->calculateMaxOsContextCount();
    auto drmMock = DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[1]).release();
    drmMock->memoryInfo.reset(new MockMemoryInfo{*drmMock});
    DrmMemoryManagerFixture::setUp(drmMock, true);
    drmMock->reset();
} // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
void DrmMemoryManagerWithLocalMemoryFixture::tearDown() {
    DrmMemoryManagerFixture::tearDown();
}

DrmMemoryManagerFixtureWithoutQuietIoctlExpectation::DrmMemoryManagerFixtureWithoutQuietIoctlExpectation() {}

DrmMemoryManagerFixtureWithoutQuietIoctlExpectation::DrmMemoryManagerFixtureWithoutQuietIoctlExpectation(uint32_t numRootDevices, uint32_t rootIndex) : rootDeviceIndex(0), numRootDevices(numRootDevices) {}

void DrmMemoryManagerFixtureWithoutQuietIoctlExpectation::setUp() {
    setUp(false);
}
void DrmMemoryManagerFixtureWithoutQuietIoctlExpectation::setUp(bool enableLocalMem) {
    debugManager.flags.DeferOsContextInitialization.set(0);

    executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
    uint32_t i = 0;
    for (auto &rootDeviceEnvironment : executionEnvironment->rootDeviceEnvironments) {
        rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->setDriverModel(DrmMockCustom::create(*rootDeviceEnvironment));
        rootDeviceEnvironment->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*rootDeviceEnvironment->osInterface->getDriverModel()->as<Drm>(), i, false);
        rootDeviceEnvironment->initGmm();
        i++;
    }
    mock = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>());
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    mock->memoryInfo.reset(new MockedMemoryInfo(regionInfo, *mock));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);
    memoryManager = new TestedDrmMemoryManager(enableLocalMem, false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);

    ASSERT_NE(nullptr, memoryManager);
    if (memoryManager->getgemCloseWorker()) {
        memoryManager->getgemCloseWorker()->close(true);
    }
    device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, rootDeviceIndex));
}
void DrmMemoryManagerFixtureWithoutQuietIoctlExpectation::tearDown() {
}

void DrmMemoryManagerFixtureWithLocalMemoryAndWithoutQuietIoctlExpectation::setUp() {
    DrmMemoryManagerFixtureWithoutQuietIoctlExpectation::setUp(true);
}
void DrmMemoryManagerFixtureWithLocalMemoryAndWithoutQuietIoctlExpectation::tearDown() {
    DrmMemoryManagerFixtureWithoutQuietIoctlExpectation::tearDown();
}

} // namespace NEO
