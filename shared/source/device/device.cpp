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
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"

namespace NEO {

decltype(&PerformanceCounters::create) Device::createPerformanceCountersFunc = PerformanceCounters::create;
extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);

Device::Device(ExecutionEnvironment *executionEnvironment)
    : executionEnvironment(executionEnvironment) {
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
        this->executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->initDebugger();
    }
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    hwHelper.setupHardwareCapabilities(&this->hardwareCapabilities, hwInfo);
    executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->initGmm();

    if (!createEngines()) {
        return false;
    }

    getDefaultEngine().osContext->setDefaultContext(true);

    for (auto &engine : engines) {
        auto commandStreamReceiver = engine.commandStreamReceiver;
        auto osContext = engine.osContext;
        if (!commandStreamReceiver->initDirectSubmission(*this, *osContext)) {
            return false;
        }
    }

    executionEnvironment->memoryManager->setDefaultEngineIndex(defaultEngineIndex);

    auto osInterface = getRootDeviceEnvironment().osInterface.get();

    if (!osTime) {
        osTime = OSTime::create(osInterface);
    }

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
    auto gpgpuEngines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);

    this->engineGroups.resize(static_cast<uint32_t>(EngineGroupType::MaxEngineGroups));
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

bool Device::createEngine(uint32_t deviceCsrIndex, EngineTypeUsage engineTypeUsage) {
    auto &hwInfo = getHardwareInfo();
    auto defaultEngineType = getChosenEngineType(hwInfo);

    std::unique_ptr<CommandStreamReceiver> commandStreamReceiver = createCommandStreamReceiver();
    if (!commandStreamReceiver) {
        return false;
    }

    auto engineType = engineTypeUsage.first;

    bool internalUsage = (engineTypeUsage.second == EngineUsage::Internal);
    if (internalUsage) {
        commandStreamReceiver->initializeDefaultsForInternalEngine();
    }

    if (commandStreamReceiver->needsPageTableManager(engineType)) {
        commandStreamReceiver->createPageTableManager();
    }

    bool lowPriority = (engineTypeUsage.second == EngineUsage::LowPriority);
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(commandStreamReceiver.get(), engineType,
                                                                                     getDeviceBitfield(), preemptionMode,
                                                                                     lowPriority, internalUsage, false);
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

    EngineControl engine{commandStreamReceiver.get(), osContext};
    engines.push_back(engine);
    if (!lowPriority && !internalUsage) {
        const auto &hardwareInfo = this->getHardwareInfo();
        auto &hwHelper = NEO::HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
        hwHelper.addEngineToEngineGroup(engineGroups, engine, hwInfo);
    }

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

bool Device::isSimulation() const {
    auto &hwInfo = getHardwareInfo();

    bool simulation = hwInfo.capabilityTable.isSimulation(hwInfo.platform.usDeviceID);
    for (const auto &engine : engines) {
        if (engine.commandStreamReceiver->getType() != CommandStreamReceiverType::CSR_HW) {
            simulation = true;
        }
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

EngineControl &Device::getEngine(aub_stream::EngineType engineType, bool lowPriority, bool internalUsage) {
    for (auto &engine : engines) {
        if (engine.osContext->getEngineType() == engineType &&
            engine.osContext->isLowPriority() == lowPriority &&
            engine.osContext->isInternalEngine() == internalUsage) {
            return engine;
        }
    }
    if (DebugManager.flags.OverrideInvalidEngineWithDefault.get()) {
        return engines[0];
    }
    UNRECOVERABLE_IF(true);
}

EngineControl &Device::getEngine(uint32_t index) {
    UNRECOVERABLE_IF(index >= engines.size());
    return engines[index];
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

GmmClientContext *Device::getGmmClientContext() const {
    return getGmmHelper()->getClientContext();
}

uint64_t Device::getGlobalMemorySize() const {

    auto globalMemorySize = getMemoryManager()->isLocalMemorySupported(this->getRootDeviceIndex())
                                ? getMemoryManager()->getLocalMemorySize(this->getRootDeviceIndex())
                                : getMemoryManager()->getSystemSharedMemory(this->getRootDeviceIndex());
    globalMemorySize = std::min(globalMemorySize, getMemoryManager()->getMaxApplicationAddress() + 1);
    globalMemorySize = static_cast<uint64_t>(static_cast<double>(globalMemorySize) * 0.8);
    return globalMemorySize;
}

NEO::SourceLevelDebugger *Device::getSourceLevelDebugger() {
    auto debugger = getDebugger();
    if (debugger) {
        return debugger->isLegacy() ? static_cast<NEO::SourceLevelDebugger *>(debugger) : nullptr;
    }
    return nullptr;
}

const std::vector<EngineControl> &Device::getEngines() const {
    return this->engines;
}

} // namespace NEO
