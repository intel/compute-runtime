/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/device.h"
#include "hw_cmds.h"
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
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/os_interface/os_time.h"
#include "runtime/source_level_debugger/source_level_debugger.h"
#include <cstring>
#include <map>

namespace OCLRT {

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

CommandStreamReceiver *createCommandStream(const HardwareInfo *pHwInfo, ExecutionEnvironment &executionEnvironment);

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

Device::Device(const HardwareInfo &hwInfo, ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex)
    : hwInfo(hwInfo), executionEnvironment(executionEnvironment), deviceIndex(deviceIndex) {
    memset(&deviceInfo, 0, sizeof(deviceInfo));
    deviceExtensions.reserve(1000);
    name.reserve(100);
    preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfo);

    if (!getSourceLevelDebugger()) {
        this->executionEnvironment->initSourceLevelDebugger(hwInfo);
    }
    this->executionEnvironment->incRefInternal();
    auto &hwHelper = HwHelper::get(hwInfo.pPlatform->eRenderCoreFamily);
    hwHelper.setupHardwareCapabilities(&this->hardwareCapabilities, hwInfo);
}

Device::~Device() {
    DEBUG_BREAK_IF(nullptr == executionEnvironment->memoryManager.get());
    if (performanceCounters) {
        performanceCounters->shutdown();
    }

    for (auto &engine : engines) {
        if (engine.commandStreamReceiver) {
            engine.commandStreamReceiver->flushBatchedSubmissions();
        }
    }

    if (deviceInfo.sourceLevelDebuggerActive && executionEnvironment->sourceLevelDebugger) {
        executionEnvironment->sourceLevelDebugger->notifyDeviceDestruction();
    }

    if (executionEnvironment->memoryManager) {
        if (preemptionAllocation) {
            executionEnvironment->memoryManager->freeGraphicsMemory(preemptionAllocation);
            preemptionAllocation = nullptr;
        }
        executionEnvironment->memoryManager->waitForDeletions();

        alignedFree(this->slmWindowStartAddress);
    }
    executionEnvironment->decRefInternal();
}

bool Device::createDeviceImpl(const HardwareInfo *pHwInfo, Device &outDevice) {
    auto executionEnvironment = outDevice.executionEnvironment;
    executionEnvironment->initGmm(pHwInfo);

    if (!createEngines(pHwInfo, outDevice)) {
        return false;
    }

    executionEnvironment->memoryManager->setDefaultEngineIndex(outDevice.defaultEngineIndex);

    auto osInterface = executionEnvironment->osInterface.get();

    if (!outDevice.osTime) {
        outDevice.osTime = OSTime::create(osInterface);
    }
    outDevice.driverInfo.reset(DriverInfo::create(osInterface));

    outDevice.initializeCaps();

    if (outDevice.osTime->getOSInterface()) {
        if (pHwInfo->capabilityTable.instrumentationEnabled) {
            outDevice.performanceCounters = createPerformanceCountersFunc(outDevice.osTime.get());
            outDevice.performanceCounters->initialize(pHwInfo);
        }
    }

    uint32_t deviceHandle = 0;
    if (osInterface) {
        deviceHandle = osInterface->getDeviceHandle();
    }

    if (outDevice.deviceInfo.sourceLevelDebuggerActive) {
        outDevice.executionEnvironment->sourceLevelDebugger->notifyNewDevice(deviceHandle);
    }

    outDevice.executionEnvironment->memoryManager->setForce32BitAllocations(outDevice.getDeviceInfo().force32BitAddressess);

    if (outDevice.preemptionMode == PreemptionMode::MidThread || outDevice.isSourceLevelDebuggerActive()) {
        AllocationProperties properties(true, pHwInfo->capabilityTable.requiredPreemptionSurfaceSize, GraphicsAllocation::AllocationType::UNDECIDED);
        properties.flags.uncacheable = outDevice.getWaTable()->waCSRUncachable;
        properties.alignment = 256 * MemoryConstants::kiloByte;
        outDevice.preemptionAllocation = outDevice.executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(properties);
        if (!outDevice.preemptionAllocation) {
            return false;
        }
    }

    for (auto engine : outDevice.engines) {
        auto csr = engine.commandStreamReceiver;
        csr->setPreemptionCsrAllocation(outDevice.preemptionAllocation);
        if (DebugManager.flags.EnableExperimentalCommandBuffer.get() > 0) {
            csr->setExperimentalCmdBuffer(std::make_unique<ExperimentalCommandBuffer>(csr, outDevice.getDeviceInfo().profilingTimerResolution));
        }
    }

    return true;
}

bool Device::createEngines(const HardwareInfo *pHwInfo, Device &outDevice) {
    auto executionEnvironment = outDevice.executionEnvironment;
    auto defaultEngineType = getChosenEngineType(*pHwInfo);

    for (uint32_t deviceCsrIndex = 0; deviceCsrIndex < gpgpuEngineInstances.size(); deviceCsrIndex++) {
        if (!executionEnvironment->initializeCommandStreamReceiver(pHwInfo, outDevice.getDeviceIndex(), deviceCsrIndex)) {
            return false;
        }
        executionEnvironment->initializeMemoryManager(outDevice.getEnabled64kbPages(), outDevice.getEnableLocalMemory(),
                                                      outDevice.getDeviceIndex(), deviceCsrIndex);

        auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(gpgpuEngineInstances[deviceCsrIndex], outDevice.preemptionMode);
        auto commandStreamReceiver = executionEnvironment->commandStreamReceivers[outDevice.getDeviceIndex()][deviceCsrIndex].get();
        commandStreamReceiver->setOsContext(*osContext);
        if (!commandStreamReceiver->initializeTagAllocation()) {
            return false;
        }

        if (gpgpuEngineInstances[deviceCsrIndex].type == defaultEngineType && gpgpuEngineInstances[deviceCsrIndex].id == 0) {
            outDevice.defaultEngineIndex = deviceCsrIndex;
        }

        outDevice.engines[deviceCsrIndex] = {commandStreamReceiver, osContext};
    }
    return true;
}

const HardwareInfo *Device::getDeviceInitHwInfo(const HardwareInfo *pHwInfoIn) {
    return pHwInfoIn ? pHwInfoIn : platformDevices[0];
}

const HardwareInfo &Device::getHardwareInfo() const { return hwInfo; }

const WorkaroundTable *Device::getWaTable() const { return hwInfo.pWaTable; }

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
    return hardwarePrefix[hwInfo.pPlatform->eProductFamily];
}

const std::string Device::getFamilyNameWithType() const {
    std::string platformName = familyName[hwInfo.pPlatform->eRenderCoreFamily];
    platformName.append(getPlatformType(hwInfo));
    return platformName;
}

double Device::getProfilingTimerResolution() {
    return osTime->getDynamicDeviceTimerResolution(hwInfo);
}

unsigned int Device::getSupportedClVersion() const {
    return hwInfo.capabilityTable.clVersionSupport;
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
    bool simulation = hwInfo.capabilityTable.isSimulation(hwInfo.pPlatform->usDeviceID);
    if (engines[0].commandStreamReceiver->getType() != CommandStreamReceiverType::CSR_HW) {
        simulation = true;
    }
    if (hwInfo.pSkuTable->ftrSimulationMode) {
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
    return this->getHardwareInfo().pPlatform->eRenderCoreFamily;
}

bool Device::isSourceLevelDebuggerActive() const {
    return deviceInfo.sourceLevelDebuggerActive;
}

void Device::initMaxPowerSavingMode() {
    for (auto &engine : engines) {
        if (engine.commandStreamReceiver) {
            engine.commandStreamReceiver->peekKmdNotifyHelper()->initMaxPowerSavingMode();
        }
    }
}
} // namespace OCLRT
