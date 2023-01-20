/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"

#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/test/common/mocks/linux/mock_drm_command_stream_receiver.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"

#include "hw_cmds_default.h"

namespace NEO {

extern std::vector<void *> mmapVector;

void DrmMemoryManagerBasic::SetUp() {
    for (auto i = 0u; i < numRootDevices; i++) {
        executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment.rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[i]);
        executionEnvironment.rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
        executionEnvironment.rootDeviceEnvironments[i]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, i);
        executionEnvironment.rootDeviceEnvironments[i]->initGmm();
    }
}

void DrmMemoryManagerFixture::setUp() {
    MemoryManagementFixture::setUp();

    executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), numRootDevices - 1);
    setUp(new DrmMockCustom(*executionEnvironment->rootDeviceEnvironments[0]), false);
} // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

void DrmMemoryManagerFixture::setUp(DrmMockCustom *mock, bool localMemoryEnabled) {
    ASSERT_NE(nullptr, executionEnvironment);
    executionEnvironment->incRefInternal();
    DebugManager.flags.DeferOsContextInitialization.set(0);
    DebugManager.flags.SetAmountOfReusableAllocations.set(0);

    environmentWrapper.setCsrType<TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>>();
    allocationData.rootDeviceIndex = rootDeviceIndex;
    this->mock = mock;
    for (auto i = 0u; i < numRootDevices; i++) {
        auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[i].get();
        rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMockCustom(*rootDeviceEnvironment)));
        rootDeviceEnvironment->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*rootDeviceEnvironment->osInterface->getDriverModel()->as<Drm>(), i);
        rootDeviceEnvironment->builtins.reset(new MockBuiltins);
        rootDeviceEnvironment->initGmm();
    }

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

    int enginesCount = static_cast<int>(device->getMemoryManager()->getRegisteredEnginesCount());

    mock->ioctl_expected.contextDestroy = enginesCount;
    mock->ioctl_expected.gemClose = enginesCount;
    mock->ioctl_expected.gemWait = enginesCount;

    auto csr = static_cast<TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *>(device->getDefaultEngine().commandStreamReceiver);
    if (csr->globalFenceAllocation) {
        mock->ioctl_expected.gemClose += enginesCount;
        mock->ioctl_expected.gemWait += enginesCount;
    }
    if (csr->getPreemptionAllocation()) {
        mock->ioctl_expected.gemClose += enginesCount;
        mock->ioctl_expected.gemWait += enginesCount;
    }
    if (csr->getKernelArgsBufferAllocation()) {
        mock->ioctl_expected.gemClose += enginesCount;
        mock->ioctl_expected.gemWait += enginesCount;
    }
    mock->ioctl_expected.gemWait += additionalDestroyDeviceIoctls.gemWait.load();
    mock->ioctl_expected.gemClose += additionalDestroyDeviceIoctls.gemClose.load();
    delete device;
    if (dontTestIoctlInTearDown) {
        mock->reset();
    }
    mock->testIoctls();
    executionEnvironment->decRefInternal();
    MemoryManagementFixture::tearDown();
    mmapVector.clear();
}

void DrmMemoryManagerWithLocalMemoryFixture::setUp() {
    backup = std::make_unique<VariableBackup<UltHwConfig>>(&ultHwConfig);
    ultHwConfig.csrBaseCallCreatePreemption = false;

    MemoryManagementFixture::setUp();
    executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), numRootDevices - 1);
    DrmMemoryManagerFixture::setUp(new DrmMockCustom(*executionEnvironment->rootDeviceEnvironments[0]), true);
} // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
void DrmMemoryManagerWithLocalMemoryFixture::tearDown() {
    DrmMemoryManagerFixture::tearDown();
}

void DrmMemoryManagerFixtureWithoutQuietIoctlExpectation::setUp() {
    setUp(false);
}
void DrmMemoryManagerFixtureWithoutQuietIoctlExpectation::setUp(bool enableLocalMem) {
    DebugManager.flags.DeferOsContextInitialization.set(0);

    executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
    uint32_t i = 0;
    for (auto &rootDeviceEnvironment : executionEnvironment->rootDeviceEnvironments) {
        rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMockCustom(*rootDeviceEnvironment)));
        rootDeviceEnvironment->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*rootDeviceEnvironment->osInterface->getDriverModel()->as<Drm>(), i);
        rootDeviceEnvironment->initGmm();
        i++;
    }
    mock = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>());
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * GB;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * GB;
    mock->memoryInfo.reset(new MockedMemoryInfo(regionInfo, *mock));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u);
    memoryManager.reset(new TestedDrmMemoryManager(enableLocalMem, false, false, *executionEnvironment));

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
