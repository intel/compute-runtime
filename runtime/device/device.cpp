/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/device.h"

#include "runtime/built_ins/built_ins.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/command_stream/experimental_command_buffer.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/device/device_vector.h"
#include "runtime/device/driver_info.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/os_interface/os_time.h"
#include "runtime/source_level_debugger/source_level_debugger.h"

#include "hw_cmds.h"

#include <cstring>
#include <map>

namespace NEO {

decltype(&PerformanceCounters::create) Device::createPerformanceCountersFunc = PerformanceCounters::create;

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

CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment);

// Global table of hardware prefixes
const char *hardwarePrefix[IGFX_MAX_PRODUCT] = {
    nullptr,
};
// Global table of family names
const char *familyName[IGFX_MAX_CORE] = {
    nullptr,
};
// Global table of family names
bool familyEnabled[IGFX_MAX_CORE] = {
    false,
};

Device::Device(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex)
    : executionEnvironment(executionEnvironment), deviceIndex(deviceIndex) {
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

    executionEnvironment->memoryManager->waitForDeletions();
    alignedFree(this->slmWindowStartAddress);

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
        if (!createEngine(getDeviceIndex(), deviceCsrIndex, gpgpuEngines[deviceCsrIndex])) {
            return false;
        }
    }
    return true;
}

bool Device::createEngine(uint32_t deviceIndex, uint32_t deviceCsrIndex, aub_stream::EngineType engineType) {
    auto &hwInfo = getHardwareInfo();
    auto defaultEngineType = getChosenEngineType(hwInfo);

    if (!executionEnvironment->initializeCommandStreamReceiver(deviceIndex, deviceCsrIndex)) {
        return false;
    }

    auto commandStreamReceiver = executionEnvironment->commandStreamReceivers[deviceIndex][deviceCsrIndex].get();

    DeviceBitfield deviceBitfield;
    deviceBitfield.set(deviceIndex);
    bool lowPriority = (deviceCsrIndex == HwHelper::lowPriorityGpgpuEngineIndex);
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(commandStreamReceiver, engineType,
                                                                                     deviceBitfield, preemptionMode, lowPriority);
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

    engines.push_back({commandStreamReceiver, osContext});

    return true;
}

const HardwareInfo &Device::getHardwareInfo() const { return *executionEnvironment->getHardwareInfo(); }

const WorkaroundTable *Device::getWaTable() const { return &getHardwareInfo().workaroundTable; }

const DeviceInfo &Device::getDeviceInfo() const {
    return deviceInfo;
}

DeviceInfo *Device::getMutableDeviceInfo() {
    return &deviceInfo;
}

void *Device::getSLMWindowStartAddress() {
    prepareSLMWindow();
    return this->slmWindowStartAddress;
}

void Device::prepareSLMWindow() {
    if (this->slmWindowStartAddress == nullptr) {
        this->slmWindowStartAddress = executionEnvironment->memoryManager->allocateSystemMemory(MemoryConstants::slmWindowSize, MemoryConstants::slmWindowAlignment);
    }
}

const char *Device::getProductAbbrev() const {
    return hardwarePrefix[executionEnvironment->getHardwareInfo()->platform.eProductFamily];
}

const std::string Device::getFamilyNameWithType() const {
    auto &hwInfo = getHardwareInfo();
    std::string platformName = familyName[hwInfo.platform.eRenderCoreFamily];
    platformName.append(hwInfo.capabilityTable.platformType);
    return platformName;
}

double Device::getProfilingTimerResolution() {
    return osTime->getDynamicDeviceTimerResolution(getHardwareInfo());
}

unsigned int Device::getSupportedClVersion() const {
    return getHardwareInfo().capabilityTable.clVersionSupport;
}

/* We hide the retain and release function of BaseObject. */
void Device::retain() {
    DEBUG_BREAK_IF(!isValid());
}

unique_ptr_if_unused<Device> Device::release() {
    DEBUG_BREAK_IF(!isValid());
    return unique_ptr_if_unused<Device>(this, false);
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
