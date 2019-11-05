/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/device.h"

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/experimental_command_buffer.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/device/device_vector.h"
#include "runtime/device/driver_info.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/os_interface/os_time.h"
#include "runtime/source_level_debugger/source_level_debugger.h"

namespace NEO {

decltype(&PerformanceCounters::create) Device::createPerformanceCountersFunc = PerformanceCounters::create;
extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);

DeviceVector::DeviceVector(const cl_device_id *devices,
                           cl_uint numDevices) {
    for (cl_uint i = 0; i < numDevices; i++) {
        this->push_back(castToObject<Device>(devices[i]));
    }
}

void DeviceVector::toDeviceIDs(std::vector<cl_device_id> &devIDs) {
    int i = 0;
    devIDs.resize(this->size());

    for (auto &it : *this) {
        devIDs[i] = it;
        i++;
    }
}

Device::Device(ExecutionEnvironment *executionEnvironment)
    : executionEnvironment(executionEnvironment) {
    memset(&deviceInfo, 0, sizeof(deviceInfo));
    deviceExtensions.reserve(1000);
    name.reserve(100);
    auto &hwInfo = getHardwareInfo();
    preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfo);

    if (!getSourceLevelDebugger()) {
        this->executionEnvironment->initSourceLevelDebugger();
    }
    this->executionEnvironment->incRefInternal();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    hwHelper.setupHardwareCapabilities(&this->hardwareCapabilities, hwInfo);
}

Device::~Device() {
    DEBUG_BREAK_IF(nullptr == executionEnvironment->memoryManager.get());
    if (performanceCounters) {
        performanceCounters->shutdown();
    }

    for (auto &engine : engines) {
        engine.commandStreamReceiver->flushBatchedSubmissions();
    }

    if (deviceInfo.sourceLevelDebuggerActive && executionEnvironment->sourceLevelDebugger) {
        executionEnvironment->sourceLevelDebugger->notifyDeviceDestruction();
    }
    commandStreamReceivers.clear();
    executionEnvironment->memoryManager->waitForDeletions();
    executionEnvironment->decRefInternal();
}

bool Device::createDeviceImpl() {
    executionEnvironment->initGmm();

    if (!createEngines()) {
        return false;
    }
    executionEnvironment->memoryManager->setDefaultEngineIndex(defaultEngineIndex);

    auto osInterface = executionEnvironment->osInterface.get();

    if (!osTime) {
        osTime = OSTime::create(osInterface);
    }
    driverInfo.reset(DriverInfo::create(osInterface));

    initializeCaps();

    auto &hwInfo = getHardwareInfo();
    if (osTime->getOSInterface()) {
        if (hwInfo.capabilityTable.instrumentationEnabled) {
            performanceCounters = createPerformanceCountersFunc(this);
        }
    }

    uint32_t deviceHandle = 0;
    if (osInterface) {
        deviceHandle = osInterface->getDeviceHandle();
    }

    if (deviceInfo.sourceLevelDebuggerActive) {
        executionEnvironment->sourceLevelDebugger->notifyNewDevice(deviceHandle);
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
    if (HwHelper::get(hwInfo.platform.eRenderCoreFamily).isPageTableManagerSupported(hwInfo)) {
        commandStreamReceiver->createPageTableManager();
    }

    bool lowPriority = (deviceCsrIndex == HwHelper::lowPriorityGpgpuEngineIndex);
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(commandStreamReceiver.get(), engineType,
                                                                                     getDeviceBitfieldForOsContext(), preemptionMode, lowPriority);
    commandStreamReceiver->setupContext(*osContext);

    if (!commandStreamReceiver->initializeTagAllocation()) {
        return false;
    }
    if (engineType == defaultEngineType && !lowPriority) {
        defaultEngineIndex = deviceCsrIndex;
    }

    if ((preemptionMode == PreemptionMode::MidThread || isSourceLevelDebuggerActive()) && !commandStreamReceiver->createPreemptionAllocation()) {
        return false;
    }

    engines.push_back({commandStreamReceiver.get(), osContext});
    commandStreamReceivers.push_back(std::move(commandStreamReceiver));

    return true;
}

const HardwareInfo &Device::getHardwareInfo() const { return *executionEnvironment->getHardwareInfo(); }

const DeviceInfo &Device::getDeviceInfo() const {
    return deviceInfo;
}

const char *Device::getProductAbbrev() const {
    return hardwarePrefix[getHardwareInfo().platform.eProductFamily];
}

double Device::getProfilingTimerResolution() {
    return osTime->getDynamicDeviceTimerResolution(getHardwareInfo());
}

unsigned int Device::getSupportedClVersion() const {
    return getHardwareInfo().capabilityTable.clVersionSupport;
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

bool Device::isSourceLevelDebuggerActive() const {
    return deviceInfo.sourceLevelDebuggerActive;
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

} // namespace NEO
