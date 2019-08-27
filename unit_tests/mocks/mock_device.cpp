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
}

MockDevice::MockDevice(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex)
    : RootDevice(executionEnvironment, deviceIndex) {
    auto &hwInfo = getHardwareInfo();
    bool enableLocalMemory = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getEnableLocalMemory(hwInfo);
    bool aubUsage = (testMode == TestMode::AubTests) || (testMode == TestMode::AubTestsWithTbx);
    this->mockMemoryManager.reset(new MemoryManagerCreate<OsAgnosticMemoryManager>(false, enableLocalMemory, aubUsage, *executionEnvironment));
    this->osTime = MockOSTime::create();
    mockWaTable = hwInfo.workaroundTable;
    executionEnvironment->setHwInfo(&hwInfo);
    executionEnvironment->initializeMemoryManager();
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
    executionEnvironment->commandStreamReceivers[getDeviceIndex()][engineIndex].reset(newCsr);
    executionEnvironment->commandStreamReceivers[getDeviceIndex()][engineIndex]->initializeTagAllocation();

    if (preemptionMode == PreemptionMode::MidThread || isSourceLevelDebuggerActive()) {
        executionEnvironment->commandStreamReceivers[getDeviceIndex()][engineIndex]->createPreemptionAllocation();
    }
    this->engines[engineIndex].commandStreamReceiver = newCsr;

    auto osContext = this->engines[engineIndex].osContext;
    executionEnvironment->memoryManager->getRegisteredEngines()[osContext->getContextId()].commandStreamReceiver = newCsr;
    this->engines[engineIndex].commandStreamReceiver->setupContext(*osContext);
    UNRECOVERABLE_IF(getDeviceIndex() != 0u);
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
