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

bool MockGmmStatus::initialized = false;

MockDevice::MockDevice(const HardwareInfo &hwInfo, bool isRootDevice)
    : Device(hwInfo, isRootDevice) {
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
    this->memoryManager->freeGraphicsMemory(tagAllocation);
    tagAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t));
    auto pTagMemory = reinterpret_cast<uint32_t *>(tagAllocation->getUnderlyingBuffer());
    *pTagMemory = initialHardwareTag;
    tagAddress = pTagMemory;
    commandStreamReceiver->setMemoryManager(memoryManager);
    commandStreamReceiver->setTagAllocation(tagAllocation);
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

OCLRT::FailMemoryManager::FailMemoryManager() : MemoryManager(false) {
    agnostic = nullptr;
    fail = 0;
}

OCLRT::FailMemoryManager::FailMemoryManager(int32_t fail) : MemoryManager(false) {
    allocations.reserve(fail);
    agnostic = new OsAgnosticMemoryManager(false);
    this->fail = fail;
}
