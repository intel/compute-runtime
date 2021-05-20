/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/cl_device/cl_device.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/platform/extensions.h"
#include "opencl/source/platform/platform.h"

namespace NEO {

ClDevice::ClDevice(Device &device, Platform *platform) : device(device), platformId(platform) {
    device.incRefInternal();
    device.setSpecializedDevice(this);
    deviceExtensions.reserve(1000);
    name.reserve(100);
    auto osInterface = getRootDeviceEnvironment().osInterface.get();
    driverInfo.reset(DriverInfo::create(&device.getHardwareInfo(), osInterface));
    initializeCaps();
    OpenClCFeaturesContainer emptyOpenClCFeatures;
    compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(deviceInfo.deviceExtensions, emptyOpenClCFeatures);
    compilerExtensionsWithFeatures = convertEnabledExtensionsToCompilerInternalOptions(deviceInfo.deviceExtensions, deviceInfo.openclCFeatures);

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
    if (getSharedDeviceInfo().debuggerActive && getSourceLevelDebugger()) {
        auto osInterface = device.getRootDeviceEnvironment().osInterface.get();
        getSourceLevelDebugger()->notifyNewDevice(osInterface ? osInterface->getDeviceHandle() : 0);
    }
}

ClDevice::~ClDevice() {

    if (getSharedDeviceInfo().debuggerActive && getSourceLevelDebugger()) {
        getSourceLevelDebugger()->notifyDeviceDestruction();
    }

    for (auto &subDevice : subDevices) {
        subDevice.reset();
    }
    device.decRefInternal();
}

void ClDevice::incRefInternal() {
    if (deviceInfo.parentDevice == nullptr) {
        BaseObject<_cl_device_id>::incRefInternal();
        return;
    }
    auto pParentDevice = static_cast<ClDevice *>(deviceInfo.parentDevice);
    pParentDevice->incRefInternal();
}

unique_ptr_if_unused<ClDevice> ClDevice::decRefInternal() {
    if (deviceInfo.parentDevice == nullptr) {
        return BaseObject<_cl_device_id>::decRefInternal();
    }
    auto pParentDevice = static_cast<ClDevice *>(deviceInfo.parentDevice);
    return pParentDevice->decRefInternal();
}

bool ClDevice::isOcl21Conformant() const {
    auto &hwInfo = device.getHardwareInfo();
    return (hwInfo.capabilityTable.supportsOcl21Features && hwInfo.capabilityTable.supportsDeviceEnqueue &&
            hwInfo.capabilityTable.supportsPipes && hwInfo.capabilityTable.supportsIndependentForwardProgress);
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
EngineControl &ClDevice::getEngine(aub_stream::EngineType engineType, EngineUsage engineUsage) { return device.getEngine(engineType, engineUsage); }
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

void ClDeviceVector::toDeviceIDs(std::vector<cl_device_id> &devIDs) const {
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
const std::string &ClDevice::peekCompilerExtensionsWithFeatures() const {
    return compilerExtensionsWithFeatures;
}
DeviceBitfield ClDevice::getDeviceBitfield() const {
    return device.getDeviceBitfield();
}

bool ClDevice::isDeviceEnqueueSupported() const {
    if (DebugManager.flags.ForceDeviceEnqueueSupport.get() != -1) {
        return DebugManager.flags.ForceDeviceEnqueueSupport.get();
    }
    return device.getHardwareInfo().capabilityTable.supportsDeviceEnqueue;
}

bool ClDevice::arePipesSupported() const {
    if (DebugManager.flags.ForcePipeSupport.get() != -1) {
        return DebugManager.flags.ForcePipeSupport.get();
    }
    return device.getHardwareInfo().capabilityTable.supportsPipes;
}

cl_command_queue_capabilities_intel ClDevice::getQueueFamilyCapabilitiesAll() {
    return CL_QUEUE_CAPABILITY_CREATE_SINGLE_QUEUE_EVENTS_INTEL |
           CL_QUEUE_CAPABILITY_CREATE_CROSS_QUEUE_EVENTS_INTEL |
           CL_QUEUE_CAPABILITY_SINGLE_QUEUE_EVENT_WAIT_LIST_INTEL |
           CL_QUEUE_CAPABILITY_CROSS_QUEUE_EVENT_WAIT_LIST_INTEL |
           CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL |
           CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_RECT_INTEL |
           CL_QUEUE_CAPABILITY_MAP_BUFFER_INTEL |
           CL_QUEUE_CAPABILITY_FILL_BUFFER_INTEL |
           CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_INTEL |
           CL_QUEUE_CAPABILITY_MAP_IMAGE_INTEL |
           CL_QUEUE_CAPABILITY_FILL_IMAGE_INTEL |
           CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_IMAGE_INTEL |
           CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_BUFFER_INTEL |
           CL_QUEUE_CAPABILITY_MARKER_INTEL |
           CL_QUEUE_CAPABILITY_BARRIER_INTEL |
           CL_QUEUE_CAPABILITY_KERNEL_INTEL;
}

cl_command_queue_capabilities_intel ClDevice::getQueueFamilyCapabilities(EngineGroupType type) {
    auto &hwHelper = NEO::HwHelper::get(getHardwareInfo().platform.eRenderCoreFamily);
    auto &clHwHelper = NEO::ClHwHelper::get(getHardwareInfo().platform.eRenderCoreFamily);

    cl_command_queue_capabilities_intel disabledProperties = 0u;
    if (hwHelper.isCopyOnlyEngineType(type)) {
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_KERNEL_INTEL);
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_FILL_BUFFER_INTEL);           // clEnqueueFillBuffer
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_INTEL);        // clEnqueueCopyImage
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_FILL_IMAGE_INTEL);            // clEnqueueFillImage
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_IMAGE_INTEL); // clEnqueueCopyBufferToImage
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_BUFFER_INTEL); // clEnqueueCopyImageToBuffer
    }
    disabledProperties |= clHwHelper.getAdditionalDisabledQueueFamilyCapabilities(type);

    if (disabledProperties != 0) {
        return getQueueFamilyCapabilitiesAll() & ~disabledProperties;
    }
    return CL_QUEUE_DEFAULT_CAPABILITIES_INTEL;
}

void ClDevice::getQueueFamilyName(char *outputName, size_t maxOutputNameLength, EngineGroupType type) {
    std::string name{};

    const auto &clHwHelper = ClHwHelper::get(getHardwareInfo().platform.eRenderCoreFamily);
    const bool hasHwSpecificName = clHwHelper.getQueueFamilyName(name, type);

    if (!hasHwSpecificName) {
        switch (type) {
        case EngineGroupType::RenderCompute:
            name = "rcs";
            break;
        case EngineGroupType::Compute:
            name = "ccs";
            break;
        case EngineGroupType::Copy:
            name = "bcs";
            break;
        default:
            name = "";
            break;
        }
    }

    UNRECOVERABLE_IF(name.length() > maxOutputNameLength + 1);
    strncpy_s(outputName, maxOutputNameLength, name.c_str(), name.size());
}
Platform *ClDevice::getPlatform() const {
    return castToObject<Platform>(platformId);
}
bool ClDevice::isPciBusInfoValid() const {
    return deviceInfo.pciBusInfo.pci_domain != PhysicalDevicePciBusInfo::InvalidValue && deviceInfo.pciBusInfo.pci_bus != PhysicalDevicePciBusInfo::InvalidValue &&
           deviceInfo.pciBusInfo.pci_device != PhysicalDevicePciBusInfo::InvalidValue && deviceInfo.pciBusInfo.pci_function != PhysicalDevicePciBusInfo::InvalidValue;
}

} // namespace NEO
