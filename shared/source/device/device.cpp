/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/experimental_command_buffer.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"
#include "opencl/source/device/driver_info.h"
#include "opencl/source/source_level_debugger/source_level_debugger.h"

namespace NEO {

decltype(&PerformanceCounters::create) Device::createPerformanceCountersFunc = PerformanceCounters::create;
extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);

Device::Device(ExecutionEnvironment *executionEnvironment)
    : executionEnvironment(executionEnvironment) {
    deviceExtensions.reserve(1000);
    name.reserve(100);
    this->executionEnvironment->incRefInternal();
}

Device::~Device() {
    DEBUG_BREAK_IF(nullptr == executionEnvironment->memoryManager.get());
    if (performanceCounters) {
        performanceCounters->shutdown();
    }

    for (auto &engine : engines) {
        engine.commandStreamReceiver->flushBatchedSubmissions();
    }

    commandStreamReceivers.clear();
    executionEnvironment->memoryManager->waitForDeletions();
    executionEnvironment->decRefInternal();
}

bool Device::createDeviceImpl() {
    auto &hwInfo = getHardwareInfo();
    preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfo);

    if (!getDebugger()) {
        this->executionEnvironment->initDebugger();
    }
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    hwHelper.setupHardwareCapabilities(&this->hardwareCapabilities, hwInfo);

    executionEnvironment->initGmm();

    if (!createEngines()) {
        return false;
    }
    executionEnvironment->memoryManager->setDefaultEngineIndex(defaultEngineIndex);

    auto osInterface = getRootDeviceEnvironment().osInterface.get();

    if (!osTime) {
        osTime = OSTime::create(osInterface);
    }
    driverInfo.reset(DriverInfo::create(osInterface));

    initializeCaps();

    if (osTime->getOSInterface()) {
        if (hwInfo.capabilityTable.instrumentationEnabled) {
            performanceCounters = createPerformanceCountersFunc(this);
        }
    }

    executionEnvironment->memoryManager->setForce32BitAllocations(getDeviceInfo().force32BitAddressess);

    if (DebugManager.flags.EnableExperimentalCommandBuffer.get() > 0) {
        for (auto &engine : engines) {
            auto csr = engine.commandStreamReceiver;
            csr->setExperimentalCmdBuffer(std::make_unique<ExperimentalCommandBuffer>(csr, getDeviceInfo().profilingTimerResolution));
        }
    }

    return true;
}

bool Device::createEngines() {
    auto &hwInfo = getHardwareInfo();
    auto &gpgpuEngines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances();

    for (uint32_t deviceCsrIndex = 0; deviceCsrIndex < gpgpuEngines.size(); deviceCsrIndex++) {
        if (!createEngine(deviceCsrIndex, gpgpuEngines[deviceCsrIndex])) {
            return false;
        }
    }
    return true;
}

std::unique_ptr<CommandStreamReceiver> Device::createCommandStreamReceiver() const {
    return std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, getRootDeviceIndex()));
}

bool Device::createEngine(uint32_t deviceCsrIndex, aub_stream::EngineType engineType) {
    auto &hwInfo = getHardwareInfo();
    auto defaultEngineType = getChosenEngineType(hwInfo);

    std::unique_ptr<CommandStreamReceiver> commandStreamReceiver = createCommandStreamReceiver();
    if (!commandStreamReceiver) {
        return false;
    }

    bool internalUsage = (deviceCsrIndex == HwHelper::internalUsageEngineIndex);
    if (internalUsage) {
        engineType = defaultEngineType;
    }

    if (commandStreamReceiver->needsPageTableManager(engineType)) {
        commandStreamReceiver->createPageTableManager();
    }

    bool lowPriority = (deviceCsrIndex == HwHelper::lowPriorityGpgpuEngineIndex);
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(commandStreamReceiver.get(), engineType,
                                                                                     getDeviceBitfield(), preemptionMode, lowPriority);
    commandStreamReceiver->setupContext(*osContext);

    if (!commandStreamReceiver->initializeTagAllocation()) {
        return false;
    }

    if (!commandStreamReceiver->createGlobalFenceAllocation()) {
        return false;
    }

    if (engineType == defaultEngineType && !lowPriority && !internalUsage) {
        defaultEngineIndex = deviceCsrIndex;
    }

    if ((preemptionMode == PreemptionMode::MidThread || isDebuggerActive()) && !commandStreamReceiver->createPreemptionAllocation()) {
        return false;
    }

    if (!commandStreamReceiver->initDirectSubmission(*this, *osContext)) {
        return false;
    }

    engines.push_back({commandStreamReceiver.get(), osContext});
    commandStreamReceivers.push_back(std::move(commandStreamReceiver));

    return true;
}

const HardwareInfo &Device::getHardwareInfo() const { return *getRootDeviceEnvironment().getHardwareInfo(); }

const DeviceInfo &Device::getDeviceInfo() const {
    return deviceInfo;
}

double Device::getProfilingTimerResolution() {
    return osTime->getDynamicDeviceTimerResolution(getHardwareInfo());
}

unsigned int Device::getSupportedClVersion() const {
    return getHardwareInfo().capabilityTable.clVersionSupport;
}

void Device::appendOSExtensions(const std::string &newExtensions) {
    deviceExtensions += newExtensions;
    deviceInfo.deviceExtensions = deviceExtensions.c_str();
}

bool Device::isSimulation() const {
    auto &hwInfo = getHardwareInfo();

    bool simulation = hwInfo.capabilityTable.isSimulation(hwInfo.platform.usDeviceID);
    if (engines[0].commandStreamReceiver->getType() != CommandStreamReceiverType::CSR_HW) {
        simulation = true;
    }
    if (hwInfo.featureTable.ftrSimulationMode) {
        simulation = true;
    }
    return simulation;
}

double Device::getPlatformHostTimerResolution() const {
    if (osTime.get())
        return osTime->getHostTimerResolution();
    return 0.0;
}

GFXCORE_FAMILY Device::getRenderCoreFamily() const {
    return this->getHardwareInfo().platform.eRenderCoreFamily;
}

bool Device::isDebuggerActive() const {
    return deviceInfo.debuggerActive;
}

EngineControl &Device::getEngine(aub_stream::EngineType engineType, bool lowPriority) {
    for (auto &engine : engines) {
        if (engine.osContext->getEngineType() == engineType &&
            engine.osContext->isLowPriority() == lowPriority) {
            return engine;
        }
    }
    if (DebugManager.flags.OverrideInvalidEngineWithDefault.get()) {
        return engines[0];
    }
    UNRECOVERABLE_IF(true);
}

bool Device::getDeviceAndHostTimer(uint64_t *deviceTimestamp, uint64_t *hostTimestamp) const {
    TimeStampData queueTimeStamp;
    bool retVal = getOSTime()->getCpuGpuTime(&queueTimeStamp);
    if (retVal) {
        uint64_t resolution = (uint64_t)getOSTime()->getDynamicDeviceTimerResolution(getHardwareInfo());
        *deviceTimestamp = queueTimeStamp.GPUTimeStamp * resolution;
    }

    retVal = getOSTime()->getCpuTime(hostTimestamp);
    return retVal;
}

bool Device::getHostTimer(uint64_t *hostTimestamp) const {
    return getOSTime()->getCpuTime(hostTimestamp);
}

} // namespace NEO
