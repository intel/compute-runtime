/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/device/cl_device.h"

#include "shared/source/device/device.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/program/sync_buffer_handler.h"

#include "opencl/source/platform/extensions.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/source_level_debugger/source_level_debugger.h"

namespace NEO {

ClDevice::ClDevice(Device &device, Platform *platform) : device(device), platformId(platform) {
    device.incRefInternal();
    device.setSpecializedDevice(this);
    initializeCaps();
    compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(getDeviceInfo().deviceExtensions);

    auto numAvailableDevices = device.getNumAvailableDevices();
    if (numAvailableDevices > 1) {
        for (uint32_t i = 0; i < numAvailableDevices; i++) {
            auto &coreSubDevice = static_cast<SubDevice &>(*device.getDeviceById(i));
            auto &deviceInfo = coreSubDevice.getMutableDeviceInfo();
            deviceInfo.parentDevice = device.getSpecializedDevice<ClDevice>();
            deviceInfo.partitionMaxSubDevices = 0;
            deviceInfo.partitionProperties[0] = 0;
            deviceInfo.partitionAffinityDomain = 0;
            deviceInfo.partitionType[0] = CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN;
            deviceInfo.partitionType[1] = CL_DEVICE_AFFINITY_DOMAIN_NUMA;
            deviceInfo.partitionType[2] = 0;

            subDevices.push_back(std::make_unique<ClDevice>(coreSubDevice, platform));
        }
    }
    if (device.getDeviceInfo().debuggerActive) {
        auto osInterface = device.getRootDeviceEnvironment().osInterface.get();
        getSourceLevelDebugger()->notifyNewDevice(osInterface ? osInterface->getDeviceHandle() : 0);
    }
}

ClDevice::~ClDevice() {

    if (device.getDeviceInfo().debuggerActive && getSourceLevelDebugger()) {
        getSourceLevelDebugger()->notifyDeviceDestruction();
    }

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
        auto pPlatform = castToObject<Platform>(platformId);
        pPlatform->getClDevice(device.getRootDeviceIndex())->incRefInternal();
        this->incRefApi();
    }
};
unique_ptr_if_unused<ClDevice> ClDevice::releaseApi() {
    if (!device.isReleasable()) {
        return unique_ptr_if_unused<ClDevice>(this, false);
    }
    auto pPlatform = castToObject<Platform>(platformId);
    pPlatform->getClDevice(device.getRootDeviceIndex())->decRefInternal();
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
EngineControl &ClDevice::getInternalEngine() { return device.getInternalEngine(); }
std::atomic<uint32_t> &ClDevice::getSelectorCopyEngine() { return device.getSelectorCopyEngine(); }
MemoryManager *ClDevice::getMemoryManager() const { return device.getMemoryManager(); }
GmmHelper *ClDevice::getGmmHelper() const { return device.getGmmHelper(); }
GmmClientContext *ClDevice::getGmmClientContext() const { return device.getGmmClientContext(); }
double ClDevice::getProfilingTimerResolution() { return device.getProfilingTimerResolution(); }
double ClDevice::getPlatformHostTimerResolution() const { return device.getPlatformHostTimerResolution(); }
bool ClDevice::isSimulation() const { return device.isSimulation(); }
GFXCORE_FAMILY ClDevice::getRenderCoreFamily() const { return device.getRenderCoreFamily(); }
PerformanceCounters *ClDevice::getPerformanceCounters() { return device.getPerformanceCounters(); }
PreemptionMode ClDevice::getPreemptionMode() const { return device.getPreemptionMode(); }
bool ClDevice::isDebuggerActive() const { return device.isDebuggerActive(); }
SourceLevelDebugger *ClDevice::getSourceLevelDebugger() { return reinterpret_cast<SourceLevelDebugger *>(device.getDebugger()); }
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
const std::string &ClDevice::peekCompilerExtensions() const {
    return compilerExtensions;
}
} // namespace NEO
