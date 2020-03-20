/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/cl_device/cl_device.h"

#include "shared/source/device/device.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/program/sync_buffer_handler.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"

#include "opencl/source/platform/extensions.h"
#include "opencl/source/platform/platform.h"

namespace NEO {

ClDevice::ClDevice(Device &device, Platform *platform) : device(device), platformId(platform) {
    device.incRefInternal();
    device.setSpecializedDevice(this);
    deviceExtensions.reserve(1000);
    name.reserve(100);
    auto osInterface = getRootDeviceEnvironment().osInterface.get();
    driverInfo.reset(DriverInfo::create(osInterface));
    initializeCaps();
    compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(deviceInfo.deviceExtensions);

    auto numAvailableDevices = device.getNumAvailableDevices();
    if (numAvailableDevices > 1) {
        for (uint32_t i = 0; i < numAvailableDevices; i++) {
            auto &coreSubDevice = static_cast<SubDevice &>(*device.getDeviceById(i));
            auto pClSubDevice = std::make_unique<ClDevice>(coreSubDevice, platform);
            pClSubDevice->incRefInternal();
            pClSubDevice->decRefApi();

            auto &deviceInfo = pClSubDevice->deviceInfo;
            deviceInfo.parentDevice = this;
            deviceInfo.partitionMaxSubDevices = 0;
            deviceInfo.partitionProperties[0] = 0;
            deviceInfo.partitionAffinityDomain = 0;
            deviceInfo.partitionType[0] = CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN;
            deviceInfo.partitionType[1] = CL_DEVICE_AFFINITY_DOMAIN_NUMA;
            deviceInfo.partitionType[2] = 0;

            subDevices.push_back(std::move(pClSubDevice));
        }
    }
    if (getSharedDeviceInfo().debuggerActive) {
        auto osInterface = device.getRootDeviceEnvironment().osInterface.get();
        getSourceLevelDebugger()->notifyNewDevice(osInterface ? osInterface->getDeviceHandle() : 0);
    }
}

ClDevice::~ClDevice() {

    if (getSharedDeviceInfo().debuggerActive) {
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

unsigned int ClDevice::getSupportedClVersion() const {
    return device.getHardwareInfo().capabilityTable.clVersionSupport;
}

void ClDevice::retainApi() {
    auto parentDeviceId = deviceInfo.parentDevice;
    if (parentDeviceId) {
        auto pParentClDevice = static_cast<ClDevice *>(parentDeviceId);
        pParentClDevice->incRefInternal();
        this->incRefApi();
    }
};
unique_ptr_if_unused<ClDevice> ClDevice::releaseApi() {
    auto parentDeviceId = deviceInfo.parentDevice;
    if (!parentDeviceId) {
        return unique_ptr_if_unused<ClDevice>(this, false);
    }
    auto pParentClDevice = static_cast<ClDevice *>(parentDeviceId);
    pParentClDevice->decRefInternal();
    return this->decRefApi();
}

const DeviceInfo &ClDevice::getSharedDeviceInfo() const {
    return device.getDeviceInfo();
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
Debugger *ClDevice::getDebugger() { return device.getDebugger(); }
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
