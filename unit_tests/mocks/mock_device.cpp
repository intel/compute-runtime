/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_device.h"
#include "runtime/device/driver_info.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/mocks/mock_ostime.h"
#include "unit_tests/tests_configuration.h"

using namespace OCLRT;

MockDevice::MockDevice(const HardwareInfo &hwInfo)
    : MockDevice(hwInfo, new ExecutionEnvironment, 0u) {
    CommandStreamReceiver *commandStreamReceiver = createCommandStream(&hwInfo, *this->executionEnvironment);
    executionEnvironment->commandStreamReceivers.resize(getDeviceIndex() + 1);
    executionEnvironment->commandStreamReceivers[getDeviceIndex()].reset(commandStreamReceiver);
    this->executionEnvironment->memoryManager = std::move(this->mockMemoryManager);
    this->commandStreamReceiver = commandStreamReceiver;
}
MockDevice::MockDevice(const HardwareInfo &hwInfo, ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex)
    : Device(hwInfo, executionEnvironment, deviceIndex) {
    bool aubUsage = (testMode == TestMode::AubTests) || (testMode == TestMode::AubTestsWithTbx);
    this->mockMemoryManager.reset(new OsAgnosticMemoryManager(false, this->getHardwareCapabilities().localMemorySupported, aubUsage, *executionEnvironment));
    this->osTime = MockOSTime::create();
    mockWaTable = *hwInfo.pWaTable;
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
    executionEnvironment->commandStreamReceivers[getDeviceIndex()].reset(newCsr);
    executionEnvironment->commandStreamReceivers[getDeviceIndex()]->initializeTagAllocation();
    executionEnvironment->commandStreamReceivers[getDeviceIndex()]->setPreemptionCsrAllocation(preemptionAllocation);
    this->commandStreamReceiver = newCsr;
    UNRECOVERABLE_IF(getDeviceIndex() != 0u);
    this->tagAddress = executionEnvironment->commandStreamReceivers[getDeviceIndex()]->getTagAddress();
}

MockAlignedMallocManagerDevice::MockAlignedMallocManagerDevice(const HardwareInfo &hwInfo, ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex) : MockDevice(hwInfo, executionEnvironment, deviceIndex) {
    this->mockMemoryManager.reset(new MockAllocSysMemAgnosticMemoryManager(*executionEnvironment));
}
FailDevice::FailDevice(const HardwareInfo &hwInfo, ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex)
    : MockDevice(hwInfo, executionEnvironment, deviceIndex) {
    this->mockMemoryManager.reset(new FailMemoryManager(*executionEnvironment));
}
FailDeviceAfterOne::FailDeviceAfterOne(const HardwareInfo &hwInfo, ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex)
    : MockDevice(hwInfo, executionEnvironment, deviceIndex) {
    this->mockMemoryManager.reset(new FailMemoryManager(1));
}

void MockDevice::setHWCapsLocalMemorySupported(bool localMemorySupported) {
    this->hardwareCapabilities.localMemorySupported = localMemorySupported;
}
