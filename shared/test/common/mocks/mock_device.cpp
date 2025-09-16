/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/fixtures/mock_aub_center_fixture.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_ostime.h"
#include "shared/test/common/mocks/ult_device_factory.h"

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

    EngineDescriptor engineDescriptor = {EngineTypeUsage{aub_stream::ENGINE_CCS, EngineUsage::regular}, this->getDeviceBitfield(), PreemptionMode::Disabled, true};

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
    auto &hwInfo = getHardwareInfo();
    if (!getOSTime()) {
        getRootDeviceEnvironmentRef().osTime = MockOSTime::create();
        getRootDeviceEnvironmentRef().osTime->setDeviceTimerResolution();
    }
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(&hwInfo);
    UnitTestSetter::setRcsExposure(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    UnitTestSetter::setCcsExposure(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();

    if (!executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface) {
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = std::make_unique<MockMemoryOperations>();
    }

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

    if (osContext->isPartOfContextGroup()) {
        auto &secondaryEnginesForType = secondaryEngines[osContext->getEngineType()];
        for (size_t i = 0; i < secondaryEnginesForType.engines.size(); i++) {
            if (secondaryEnginesForType.engines[i].commandStreamReceiver == commandStreamReceivers[engineIndex].get()) {
                secondaryEnginesForType.engines[i].commandStreamReceiver = newCsr;
                // only primary csr is replaced
                EXPECT_EQ(0u, i);
            }
            if (i > 0) {
                secondaryEnginesForType.engines[i].commandStreamReceiver->setPrimaryCsr(newCsr);
            }
        }
    }

    const_cast<EngineControlContainer &>(memoryManager->getRegisteredEngines(rootDeviceIndex)).emplace_back(registeredEngine);
    osContext->incRefInternal();
    newCsr->setupContext(*osContext);
    osContext->ensureContextInitialized(false);
    commandStreamReceivers[engineIndex].reset(newCsr);
    commandStreamReceivers[engineIndex]->initializeTagAllocation();
    commandStreamReceivers[engineIndex]->createGlobalFenceAllocation();

    if (preemptionMode == PreemptionMode::MidThread) {
        commandStreamReceivers[engineIndex]->createPreemptionAllocation();
    }
}

ExecutionEnvironment *MockDevice::prepareExecutionEnvironment(const HardwareInfo *pHwInfo, uint32_t rootDeviceIndex) {
    ExecutionEnvironment *executionEnvironment = new ExecutionEnvironment();
    auto numRootDevices = debugManager.flags.CreateMultipleRootDevices.get() ? debugManager.flags.CreateMultipleRootDevices.get() : rootDeviceIndex + 1;
    executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
    pHwInfo = pHwInfo ? pHwInfo : defaultHwInfo.get();
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(pHwInfo);

        UnitTestSetter::setRcsExposure(*executionEnvironment->rootDeviceEnvironments[i]);
        UnitTestSetter::setCcsExposure(*executionEnvironment->rootDeviceEnvironments[i]);
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    executionEnvironment->setDeviceHierarchyMode(executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>());
    executionEnvironment->calculateMaxOsContextCount();
    return executionEnvironment;
}

bool MockDevice::verifyAdapterLuid() {
    if (callBaseVerifyAdapterLuid)
        return Device::verifyAdapterLuid();
    return verifyAdapterLuidReturnValue;
}

void MockDevice::finalizeRayTracing() {
    for (unsigned int i = 0; i < rtDispatchGlobalsInfos.size(); i++) {
        auto rtDispatchGlobalsInfo = rtDispatchGlobalsInfos[i];
        if (rtDispatchGlobalsForceAllocation == true && rtDispatchGlobalsInfo != nullptr) {
            for (unsigned int j = 0; j < rtDispatchGlobalsInfo->rtStacks.size(); j++) {
                delete rtDispatchGlobalsInfo->rtStacks[j];
                rtDispatchGlobalsInfo->rtStacks[j] = nullptr;
            }
            delete rtDispatchGlobalsInfo->rtDispatchGlobalsArray;
            rtDispatchGlobalsInfo->rtDispatchGlobalsArray = nullptr;
            delete rtDispatchGlobalsInfos[i];
            rtDispatchGlobalsInfos[i] = nullptr;
        }
    }

    Device::finalizeRayTracing();
}

ExecutionEnvironment *MockDevice::prepareExecutionEnvironment(const HardwareInfo *pHwInfo) {
    auto executionEnvironment = new ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    auto hwInfo = pHwInfo ? pHwInfo : defaultHwInfo.get();

    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(hwInfo);
    UnitTestSetter::setRcsExposure(*executionEnvironment->rootDeviceEnvironments[0]);
    UnitTestSetter::setCcsExposure(*executionEnvironment->rootDeviceEnvironments[0]);

    executionEnvironment->setDeviceHierarchyMode(executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>());

    MockAubCenterFixture::setMockAubCenter(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->initializeMemoryManager();
    return executionEnvironment;
}

ReleaseHelper *MockDevice::getReleaseHelper() const {
    if (mockReleaseHelper) {
        return mockReleaseHelper;
    }
    return Device::getReleaseHelper();
}

AILConfiguration *MockDevice::getAilConfigurationHelper() const {
    if (mockAilConfigurationHelper) {
        return mockAilConfigurationHelper;
    }
    return Device::getAilConfigurationHelper();
}

EngineControl *MockDevice::getSecondaryEngineCsr(EngineTypeUsage engineTypeUsage, std::optional<int> priorityLevel, bool allocateInterrupt) {
    if (disableSecondaryEngines) {
        return nullptr;
    }
    return RootDevice::getSecondaryEngineCsr(engineTypeUsage, priorityLevel, allocateInterrupt);
}

std::unique_ptr<CommandStreamReceiver> MockDevice::createCommandStreamReceiver() const {
    return std::unique_ptr<CommandStreamReceiver>(createCommandStreamReceiverFunc(*executionEnvironment, getRootDeviceIndex(), getDeviceBitfield()));
}

std::unique_ptr<CommandStreamReceiver> MockSubDevice::createCommandStreamReceiver() const {
    return std::unique_ptr<CommandStreamReceiver>(createCommandStreamReceiverFunc(*executionEnvironment, getRootDeviceIndex(), getDeviceBitfield()));
}

bool MockSubDevice::createEngine(EngineTypeUsage engineTypeUsage) {
    if (failOnCreateEngine) {
        return false;
    }
    return SubDevice::createEngine(engineTypeUsage);
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
