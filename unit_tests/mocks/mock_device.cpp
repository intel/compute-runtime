/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_device.h"

#include "runtime/device/driver_info.h"
#include "runtime/os_interface/os_context.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/mocks/mock_ostime.h"
#include "unit_tests/tests_configuration.h"

using namespace NEO;

MockDevice::MockDevice()
    : MockDevice(new MockExecutionEnvironment(), 0u) {
    CommandStreamReceiver *commandStreamReceiver = createCommandStream(*this->executionEnvironment);
    executionEnvironment->commandStreamReceivers.resize(getDeviceIndex() + 1);
    executionEnvironment->commandStreamReceivers[getDeviceIndex()].resize(defaultEngineIndex + 1);
    executionEnvironment->commandStreamReceivers[getDeviceIndex()][defaultEngineIndex].reset(commandStreamReceiver);
    this->executionEnvironment->memoryManager = std::move(this->mockMemoryManager);
    this->engines.resize(defaultEngineIndex + 1);
    this->engines[defaultEngineIndex] = {commandStreamReceiver, nullptr};
    initializeCaps();
}

MockDevice::MockDevice(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex)
    : RootDevice(executionEnvironment, deviceIndex) {
    auto &hwInfo = getHardwareInfo();
    bool enableLocalMemory = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getEnableLocalMemory(hwInfo);
    bool aubUsage = (testMode == TestMode::AubTests) || (testMode == TestMode::AubTestsWithTbx);
    this->mockMemoryManager.reset(new MemoryManagerCreate<OsAgnosticMemoryManager>(false, enableLocalMemory, aubUsage, *executionEnvironment));
    this->osTime = MockOSTime::create();
    executionEnvironment->setHwInfo(&hwInfo);
    executionEnvironment->initializeMemoryManager();
    initializeCaps();
}

void MockDevice::setOSTime(OSTime *osTime) {
    this->osTime.reset(osTime);
};

void MockDevice::setDriverInfo(DriverInfo *driverInfo) {
    this->driverInfo.reset(driverInfo);
};

bool MockDevice::hasDriverInfo() {
    return driverInfo.get() != nullptr;
};

void MockDevice::injectMemoryManager(MemoryManager *memoryManager) {
    executionEnvironment->memoryManager.reset(memoryManager);
}

void MockDevice::resetCommandStreamReceiver(CommandStreamReceiver *newCsr) {
    resetCommandStreamReceiver(newCsr, defaultEngineIndex);
}

void MockDevice::resetCommandStreamReceiver(CommandStreamReceiver *newCsr, uint32_t engineIndex) {
    UNRECOVERABLE_IF(getDeviceIndex() != 0u);

    auto osContext = this->engines[engineIndex].osContext;
    auto memoryManager = executionEnvironment->memoryManager.get();
    auto registeredEngine = *memoryManager->getRegisteredEngineForCsr(engines[engineIndex].commandStreamReceiver);

    registeredEngine.commandStreamReceiver = newCsr;
    engines[engineIndex].commandStreamReceiver = newCsr;
    memoryManager->getRegisteredEngines().emplace_back(registeredEngine);
    osContext->incRefInternal();
    newCsr->setupContext(*osContext);
    executionEnvironment->commandStreamReceivers[getDeviceIndex()][engineIndex].reset(newCsr);
    executionEnvironment->commandStreamReceivers[getDeviceIndex()][engineIndex]->initializeTagAllocation();

    if (preemptionMode == PreemptionMode::MidThread || isSourceLevelDebuggerActive()) {
        executionEnvironment->commandStreamReceivers[getDeviceIndex()][engineIndex]->createPreemptionAllocation();
    }
}

MockAlignedMallocManagerDevice::MockAlignedMallocManagerDevice(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex) : MockDevice(executionEnvironment, deviceIndex) {
    this->mockMemoryManager.reset(new MockAllocSysMemAgnosticMemoryManager(*executionEnvironment));
}
FailDevice::FailDevice(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex)
    : MockDevice(executionEnvironment, deviceIndex) {
    this->mockMemoryManager.reset(new FailMemoryManager(*executionEnvironment));
}
FailDeviceAfterOne::FailDeviceAfterOne(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex)
    : MockDevice(executionEnvironment, deviceIndex) {
    this->mockMemoryManager.reset(new FailMemoryManager(1, *executionEnvironment));
}
