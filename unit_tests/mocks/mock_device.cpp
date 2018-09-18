/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_device.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/driver_info.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/os_time.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/mocks/mock_ostime.h"

using namespace OCLRT;

MockDevice::MockDevice(const HardwareInfo &hwInfo)
    : MockDevice(hwInfo, new ExecutionEnvironment, 0u) {
    CommandStreamReceiver *commandStreamReceiver = createCommandStream(&hwInfo, *this->executionEnvironment);
    executionEnvironment->commandStreamReceivers.resize(getDeviceIndex() + 1);
    executionEnvironment->commandStreamReceivers[getDeviceIndex()].reset(commandStreamReceiver);
    commandStreamReceiver->setMemoryManager(this->mockMemoryManager.get());
    this->executionEnvironment->memoryManager = std::move(this->mockMemoryManager);
    this->commandStreamReceiver = commandStreamReceiver;
}
OCLRT::MockDevice::MockDevice(const HardwareInfo &hwInfo, ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex)
    : Device(hwInfo, executionEnvironment, deviceIndex) {
    this->mockMemoryManager.reset(new OsAgnosticMemoryManager(false, this->getHardwareCapabilities().localMemorySupported));
    this->osTime = MockOSTime::create();
    mockWaTable = *hwInfo.pWaTable;
}

void MockDevice::setMemoryManager(MemoryManager *memoryManager) {
    executionEnvironment->memoryManager.reset(memoryManager);
    for (auto &commandStreamReceiver : executionEnvironment->commandStreamReceivers) {
        if (commandStreamReceiver) {
            commandStreamReceiver->setMemoryManager(memoryManager);
        }
    }
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

void MockDevice::injectMemoryManager(MockMemoryManager *memoryManager) {
    memoryManager->setCommandStreamReceiver(executionEnvironment->commandStreamReceivers[getDeviceIndex()].get());
    executionEnvironment->commandStreamReceivers[getDeviceIndex()]->setMemoryManager(memoryManager);
    setMemoryManager(memoryManager);
}

void MockDevice::resetCommandStreamReceiver(CommandStreamReceiver *newCsr) {
    executionEnvironment->commandStreamReceivers[getDeviceIndex()].reset(newCsr);
    executionEnvironment->commandStreamReceivers[getDeviceIndex()]->setMemoryManager(executionEnvironment->memoryManager.get());
    executionEnvironment->commandStreamReceivers[getDeviceIndex()]->initializeTagAllocation();
    executionEnvironment->commandStreamReceivers[getDeviceIndex()]->setPreemptionCsrAllocation(preemptionAllocation);
    executionEnvironment->memoryManager->csr = executionEnvironment->commandStreamReceivers[getDeviceIndex()].get();
    this->commandStreamReceiver = newCsr;
    this->tagAddress = executionEnvironment->commandStreamReceivers[getDeviceIndex()]->getTagAddress();
}

OCLRT::FailMemoryManager::FailMemoryManager() : MockMemoryManager() {
    agnostic = nullptr;
    fail = 0;
}

OCLRT::FailMemoryManager::FailMemoryManager(int32_t fail) : MockMemoryManager() {
    allocations.reserve(fail);
    agnostic = new OsAgnosticMemoryManager(false, false);
    this->fail = fail;
}

MockAlignedMallocManagerDevice::MockAlignedMallocManagerDevice(const HardwareInfo &hwInfo, ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex) : MockDevice(hwInfo, executionEnvironment, deviceIndex) {
    this->mockMemoryManager.reset(new MockAllocSysMemAgnosticMemoryManager());
}
