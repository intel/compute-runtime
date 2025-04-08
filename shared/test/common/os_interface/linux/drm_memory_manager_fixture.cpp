/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"

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
