/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "unit_tests/mocks/mock_device.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/os_time.h"
#include "runtime/device/driver_info.h"
#include "unit_tests/mocks/mock_ostime.h"
#include "unit_tests/mocks/mock_memory_manager.h"

using namespace OCLRT;

MockDevice::MockDevice(const HardwareInfo &hwInfo)
    : Device(hwInfo) {
    memoryManager = new OsAgnosticMemoryManager;
    this->osTime = MockOSTime::create();
    mockWaTable = *hwInfo.pWaTable;
}

void MockDevice::setMemoryManager(MemoryManager *memoryManager) {
    delete this->memoryManager;
    this->memoryManager = memoryManager;
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
    memoryManager->setCommandStreamReceiver(commandStreamReceiver);
    commandStreamReceiver->setMemoryManager(memoryManager);
    setMemoryManager(memoryManager);
    memoryManager->setDevice(this);
}

void MockDevice::resetCommandStreamReceiver(CommandStreamReceiver *newCsr) {
    if (commandStreamReceiver) {
        delete commandStreamReceiver;
    }
    commandStreamReceiver = newCsr;
    commandStreamReceiver->setMemoryManager(memoryManager);
    commandStreamReceiver->setTagAllocation(tagAllocation);
    commandStreamReceiver->setPreemptionCsrAllocation(preemptionAllocation);
    memoryManager->csr = commandStreamReceiver;
}

OCLRT::FailMemoryManager::FailMemoryManager() : MockMemoryManager() {
    agnostic = nullptr;
    fail = 0;
}

OCLRT::FailMemoryManager::FailMemoryManager(int32_t fail) : MockMemoryManager() {
    allocations.reserve(fail);
    agnostic = new OsAgnosticMemoryManager(false);
    this->fail = fail;
}

MockAlignedMallocManagerDevice::MockAlignedMallocManagerDevice(const HardwareInfo &hwInfo) : MockDevice(hwInfo) {
    //delete regular OsAgnosticMemoryManager
    delete memoryManager;
    //and create specific
    memoryManager = new MockAllocSysMemAgnosticMemoryManager();
}
