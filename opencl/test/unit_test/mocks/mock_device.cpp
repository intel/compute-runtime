/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_device.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/unit_test/tests_configuration.h"

#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_ostime.h"

using namespace NEO;

bool MockDevice::createSingleDevice = true;

decltype(&createCommandStream) MockSubDevice::createCommandStreamReceiverFunc = createCommandStream;
decltype(&createCommandStream) MockDevice::createCommandStreamReceiverFunc = createCommandStream;

MockDevice::MockDevice()
    : MockDevice(new MockExecutionEnvironment(), 0u) {
    CommandStreamReceiver *commandStreamReceiver = createCommandStream(*this->executionEnvironment, this->getRootDeviceIndex());
    commandStreamReceivers.resize(1);
    commandStreamReceivers[0].reset(commandStreamReceiver);
    this->executionEnvironment->memoryManager = std::move(this->mockMemoryManager);
    this->engines.resize(1);
    this->engines[0] = {commandStreamReceiver, nullptr};
    initializeCaps();
}

const char *MockDevice::getProductAbbrev() const {
    return hardwarePrefix[getHardwareInfo().platform.eProductFamily];
}

MockDevice::MockDevice(ExecutionEnvironment *executionEnvironment, uint32_t rootDeviceIndex)
    : RootDevice(executionEnvironment, rootDeviceIndex) {
    auto &hwInfo = getHardwareInfo();
    bool enableLocalMemory = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getEnableLocalMemory(hwInfo);
    bool aubUsage = (testMode == TestMode::AubTests) || (testMode == TestMode::AubTestsWithTbx);
    this->mockMemoryManager.reset(new MemoryManagerCreate<OsAgnosticMemoryManager>(false, enableLocalMemory, aubUsage, *executionEnvironment));
    this->osTime = MockOSTime::create();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(&hwInfo);
    executionEnvironment->initializeMemoryManager();
    initializeCaps();
    preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfo);
}

bool MockDevice::createDeviceImpl() {
    if (MockDevice::createSingleDevice) {
        return Device::createDeviceImpl();
    }
    return RootDevice::createDeviceImpl();
}

void MockDevice::setOSTime(OSTime *osTime) {
    this->osTime.reset(osTime);
};

void MockDevice::injectMemoryManager(MemoryManager *memoryManager) {
    executionEnvironment->memoryManager.reset(memoryManager);
}

void MockDevice::resetCommandStreamReceiver(CommandStreamReceiver *newCsr) {
    resetCommandStreamReceiver(newCsr, defaultEngineIndex);
}

void MockDevice::resetCommandStreamReceiver(CommandStreamReceiver *newCsr, uint32_t engineIndex) {

    auto osContext = this->engines[engineIndex].osContext;
    auto memoryManager = executionEnvironment->memoryManager.get();
    auto registeredEngine = *memoryManager->getRegisteredEngineForCsr(engines[engineIndex].commandStreamReceiver);

    registeredEngine.commandStreamReceiver = newCsr;
    engines[engineIndex].commandStreamReceiver = newCsr;
    memoryManager->getRegisteredEngines().emplace_back(registeredEngine);
    osContext->incRefInternal();
    newCsr->setupContext(*osContext);
    commandStreamReceivers[engineIndex].reset(newCsr);
    commandStreamReceivers[engineIndex]->initializeTagAllocation();
    commandStreamReceivers[engineIndex]->createGlobalFenceAllocation();

    if (preemptionMode == PreemptionMode::MidThread || isDebuggerActive()) {
        commandStreamReceivers[engineIndex]->createPreemptionAllocation();
    }
}

MockAlignedMallocManagerDevice::MockAlignedMallocManagerDevice(ExecutionEnvironment *executionEnvironment, uint32_t internalDeviceIndex) : MockDevice(executionEnvironment, internalDeviceIndex) {
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
