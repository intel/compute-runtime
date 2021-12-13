/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_ostime.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/unit_test/tests_configuration.h"

using namespace NEO;

bool MockDevice::createSingleDevice = true;

decltype(&createCommandStream) MockSubDevice::createCommandStreamReceiverFunc = createCommandStream;
decltype(&createCommandStream) MockDevice::createCommandStreamReceiverFunc = createCommandStream;

MockDevice::MockDevice()
    : MockDevice(new MockExecutionEnvironment(), 0u) {
    UltDeviceFactory::initializeMemoryManager(*executionEnvironment);
    CommandStreamReceiver *commandStreamReceiver = createCommandStream(*executionEnvironment, this->getRootDeviceIndex(), this->getDeviceBitfield());
    commandStreamReceivers.resize(1);
    commandStreamReceivers[0].reset(commandStreamReceiver);

    EngineDescriptor engineDescriptor = {EngineTypeUsage{aub_stream::ENGINE_CCS, EngineUsage::Regular}, this->getDeviceBitfield(), PreemptionMode::Disabled, true, false};

    OsContext *osContext = getMemoryManager()->createAndRegisterOsContext(commandStreamReceiver, engineDescriptor);
    commandStreamReceiver->setupContext(*osContext);
    this->allEngines.resize(1);
    this->allEngines[0] = {commandStreamReceiver, osContext};
    initializeCaps();
}

const char *MockDevice::getProductAbbrev() const {
    return hardwarePrefix[getHardwareInfo().platform.eProductFamily];
}

MockDevice::MockDevice(ExecutionEnvironment *executionEnvironment, uint32_t rootDeviceIndex)
    : RootDevice(executionEnvironment, rootDeviceIndex) {
    UltDeviceFactory::initializeMemoryManager(*executionEnvironment);

    if (!getOSTime()) {
        getRootDeviceEnvironmentRef().osTime = MockOSTime::create();
    }
    auto &hwInfo = getHardwareInfo();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(&hwInfo);
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
    getRootDeviceEnvironmentRef().osTime.reset(osTime);
}

void MockDevice::injectMemoryManager(MemoryManager *memoryManager) {
    executionEnvironment->memoryManager.reset(memoryManager);
}

void MockDevice::resetCommandStreamReceiver(CommandStreamReceiver *newCsr) {
    resetCommandStreamReceiver(newCsr, defaultEngineIndex);
}

void MockDevice::resetCommandStreamReceiver(CommandStreamReceiver *newCsr, uint32_t engineIndex) {

    auto osContext = this->allEngines[engineIndex].osContext;
    auto memoryManager = executionEnvironment->memoryManager.get();
    auto registeredEngine = *memoryManager->getRegisteredEngineForCsr(allEngines[engineIndex].commandStreamReceiver);

    registeredEngine.commandStreamReceiver = newCsr;
    allEngines[engineIndex].commandStreamReceiver = newCsr;
    memoryManager->getRegisteredEngines().emplace_back(registeredEngine);
    osContext->incRefInternal();
    newCsr->setupContext(*osContext);
    osContext->ensureContextInitialized();
    commandStreamReceivers[engineIndex].reset(newCsr);
    commandStreamReceivers[engineIndex]->initializeTagAllocation();
    commandStreamReceivers[engineIndex]->createGlobalFenceAllocation();

    if (preemptionMode == PreemptionMode::MidThread || isDebuggerActive()) {
        commandStreamReceivers[engineIndex]->createPreemptionAllocation();
    }
}

bool MockSubDevice::createEngine(uint32_t deviceCsrIndex, EngineTypeUsage engineTypeUsage) {
    if (failOnCreateEngine) {
        return false;
    }
    return SubDevice::createEngine(deviceCsrIndex, engineTypeUsage);
}

MockAlignedMallocManagerDevice::MockAlignedMallocManagerDevice(ExecutionEnvironment *executionEnvironment, uint32_t internalDeviceIndex) : MockDevice(executionEnvironment, internalDeviceIndex) {
    executionEnvironment->memoryManager.reset(new MockAllocSysMemAgnosticMemoryManager(*executionEnvironment));
}
FailDevice::FailDevice(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex)
    : MockDevice(executionEnvironment, deviceIndex) {
    executionEnvironment->memoryManager.reset(new FailMemoryManager(*executionEnvironment));
}
FailDeviceAfterOne::FailDeviceAfterOne(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex)
    : MockDevice(executionEnvironment, deviceIndex) {
    executionEnvironment->memoryManager.reset(new FailMemoryManager(1, *executionEnvironment));
}
