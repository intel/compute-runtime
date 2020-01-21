/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/device.h"

#include "core/command_stream/preemption.h"
#include "core/execution_environment/root_device_environment.h"
#include "core/helpers/hw_helper.h"
#include "core/os_interface/os_context.h"
#include "core/os_interface/os_interface.h"
#include "core/os_interface/os_time.h"
#include "core/program/sync_buffer_handler.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/experimental_command_buffer.h"
#include "runtime/device/device_vector.h"
#include "runtime/device/driver_info.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/platform/platform.h"
#include "runtime/source_level_debugger/source_level_debugger.h"

namespace NEO {

decltype(&PerformanceCounters::create) Device::createPerformanceCountersFunc = PerformanceCounters::create;
extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);

ClDevice::ClDevice(Device &device) : device(device), platformId(platform()) {
    device.incRefInternal();
    initializeCaps();

    auto numAvailableDevices = device.getNumAvailableDevices();
    if (numAvailableDevices > 1) {
        for (uint32_t i = 0; i < numAvailableDevices; i++) {
            subDevices.push_back(std::make_unique<ClDevice>(*device.getDeviceById(i)));
            platform()->clDeviceMap.emplace(device.getDeviceById(i), subDevices[i].get());
        }
    }
}

ClDevice::~ClDevice() {
    syncBufferHandler.reset();
    for (auto &subDevice : subDevices) {
        subDevice.reset();
    }
    device.decRefInternal();
}

void ClDevice::allocateSyncBufferHandler() {
    TakeOwnershipWrapper<ClDevice> lock(*this);
    if (syncBufferHandler.get() == nullptr) {
        syncBufferHandler = std::make_unique<SyncBufferHandler>(this->getDevice());
        UNRECOVERABLE_IF(syncBufferHandler.get() == nullptr);
    }
}

unsigned int ClDevice::getEnabledClVersion() const { return device.getEnabledClVersion(); }
unsigned int ClDevice::getSupportedClVersion() const { return device.getSupportedClVersion(); }

void ClDevice::retainApi() {
    if (device.isReleasable()) {
        platform()->getClDevice(device.getRootDeviceIndex())->incRefInternal();
        this->incRefApi();
    }
};
unique_ptr_if_unused<ClDevice> ClDevice::releaseApi() {
    if (!device.isReleasable()) {
        return unique_ptr_if_unused<ClDevice>(this, false);
    }
    platform()->getClDevice(device.getRootDeviceIndex())->decRefInternal();
    return this->decRefApi();
}

ClDevice *ClDevice::getDeviceById(uint32_t deviceId) {
    UNRECOVERABLE_IF(deviceId >= getNumAvailableDevices());
    if (subDevices.empty()) {
        return this;
    }
    return subDevices[deviceId].get();
}

bool ClDevice::getDeviceAndHostTimer(uint64_t *deviceTimestamp, uint64_t *hostTimestamp) const { return device.getDeviceAndHostTimer(deviceTimestamp, hostTimestamp); }
bool ClDevice::getHostTimer(uint64_t *hostTimestamp) const { return device.getHostTimer(hostTimestamp); }
const HardwareInfo &ClDevice::getHardwareInfo() const { return device.getHardwareInfo(); }
const DeviceInfo &ClDevice::getDeviceInfo() const { return device.getDeviceInfo(); }
EngineControl &ClDevice::getEngine(aub_stream::EngineType engineType, bool lowPriority) { return device.getEngine(engineType, lowPriority); }
EngineControl &ClDevice::getDefaultEngine() { return device.getDefaultEngine(); }
MemoryManager *ClDevice::getMemoryManager() const { return device.getMemoryManager(); }
GmmHelper *ClDevice::getGmmHelper() const { return device.getGmmHelper(); }
double ClDevice::getProfilingTimerResolution() { return device.getProfilingTimerResolution(); }
double ClDevice::getPlatformHostTimerResolution() const { return device.getPlatformHostTimerResolution(); }
bool ClDevice::isSimulation() const { return device.isSimulation(); }
GFXCORE_FAMILY ClDevice::getRenderCoreFamily() const { return device.getRenderCoreFamily(); }
PerformanceCounters *ClDevice::getPerformanceCounters() { return device.getPerformanceCounters(); }
PreemptionMode ClDevice::getPreemptionMode() const { return device.getPreemptionMode(); }
bool ClDevice::isSourceLevelDebuggerActive() const { return device.isSourceLevelDebuggerActive(); }
SourceLevelDebugger *ClDevice::getSourceLevelDebugger() { return device.getSourceLevelDebugger(); }
ExecutionEnvironment *ClDevice::getExecutionEnvironment() const { return device.getExecutionEnvironment(); }
const RootDeviceEnvironment &ClDevice::getRootDeviceEnvironment() const { return device.getRootDeviceEnvironment(); }
const HardwareCapabilities &ClDevice::getHardwareCapabilities() const { return device.getHardwareCapabilities(); }
bool ClDevice::isFullRangeSvm() const { return device.isFullRangeSvm(); }
bool ClDevice::areSharedSystemAllocationsAllowed() const { return device.areSharedSystemAllocationsAllowed(); }
uint32_t ClDevice::getRootDeviceIndex() const { return device.getRootDeviceIndex(); }
uint32_t ClDevice::getNumAvailableDevices() const { return device.getNumAvailableDevices(); }

ClDeviceVector::ClDeviceVector(const cl_device_id *devices,
                               cl_uint numDevices) {
    for (cl_uint i = 0; i < numDevices; i++) {
        auto pClDevice = castToObject<ClDevice>(devices[i]);
        this->push_back(pClDevice);
    }
}

void ClDeviceVector::toDeviceIDs(std::vector<cl_device_id> &devIDs) {
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

    auto osInterface = getRootDeviceEnvironment().osInterface.get();

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
