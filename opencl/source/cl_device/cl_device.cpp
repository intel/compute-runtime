/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/cl_device/cl_device.h"

#include "shared/source/compiler_interface/oclc_extensions.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_interface.h"

#include "opencl/source/gtpin/gtpin_gfx_core_helper.h"
#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/source/platform/platform.h"

namespace NEO {

ClDevice::ClDevice(Device &device, ClDevice &rootClDevice, Platform *platform) : device(device), rootClDevice(rootClDevice), platformId(platform) {
    device.incRefInternal();
    device.setSpecializedDevice(this);
    deviceExtensions.reserve(1000);
    name.reserve(100);
    auto osInterface = getRootDeviceEnvironment().osInterface.get();
    driverInfo.reset(DriverInfo::create(&device.getHardwareInfo(), osInterface));
    initGTPinHelper();
    initializeCaps();
    initializeMaxPoolCount();

    OpenClCFeaturesContainer emptyOpenClCFeatures;
    compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(deviceInfo.deviceExtensions, emptyOpenClCFeatures);
    compilerExtensionsWithFeatures = convertEnabledExtensionsToCompilerInternalOptions(deviceInfo.deviceExtensions, deviceInfo.openclCFeatures);

    for (auto &subDevice : device.getSubDevices()) {
        if (!subDevice) {
            continue;
        }

        auto pClSubDevice = std::make_unique<ClDevice>(*subDevice, rootClDevice, platform);
        pClSubDevice->incRefInternal();
        pClSubDevice->decRefApi();
        pClSubDevice->internalParentDevice = this;

        if (device.getExecutionEnvironment()->getDeviceHierarchyMode() == DeviceHierarchyMode::composite) {
            auto &deviceInfo = pClSubDevice->deviceInfo;
            deviceInfo.parentDevice = this;
            deviceInfo.partitionType[0] = CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN;
            deviceInfo.partitionType[1] = CL_DEVICE_AFFINITY_DOMAIN_NUMA;
            deviceInfo.partitionType[2] = 0;
        }

        subDevices.push_back(std::move(pClSubDevice));
    }
}

ClDevice::ClDevice(Device &device, Platform *platformId) : ClDevice(device, *this, platformId) {
}

ClDevice::~ClDevice() {
    for (auto &subDevice : subDevices) {
        subDevice.reset();
    }
    device.decRefInternal();
}

void ClDevice::incRefInternal() {
    if (internalParentDevice == nullptr) {
        BaseObject<_cl_device_id>::incRefInternal();
        return;
    }
    auto pParentDevice = static_cast<ClDevice *>(internalParentDevice);
    pParentDevice->incRefInternal();
}

unique_ptr_if_unused<ClDevice> ClDevice::decRefInternal() {
    if (internalParentDevice == nullptr) {
        return BaseObject<_cl_device_id>::decRefInternal();
    }
    auto pParentDevice = static_cast<ClDevice *>(internalParentDevice);
    return pParentDevice->decRefInternal();
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

ClDevice *ClDevice::getSubDevice(uint32_t deviceId) const {
    UNRECOVERABLE_IF(deviceId >= subDevices.size());
    return subDevices[deviceId].get();
}

ClDevice *ClDevice::getNearestGenericSubDevice(uint32_t deviceId) {

    if (subDevices.empty() || !getDevice().hasRootCsr()) {
        return this;
    }
    UNRECOVERABLE_IF(deviceId >= subDevices.size());
    return subDevices[deviceId].get();
}

bool ClDevice::getDeviceAndHostTimer(uint64_t *deviceTimestamp, uint64_t *hostTimestamp) const { return device.getDeviceAndHostTimer(deviceTimestamp, hostTimestamp); }
bool ClDevice::getHostTimer(uint64_t *hostTimestamp) const { return device.getHostTimer(hostTimestamp); }
const HardwareInfo &ClDevice::getHardwareInfo() const { return device.getHardwareInfo(); }
EngineControl &ClDevice::getEngine(aub_stream::EngineType engineType, EngineUsage engineUsage) {
    return device.getEngine(engineType, engineUsage);
}
EngineControl &ClDevice::getDefaultEngine() { return device.getDefaultEngine(); }
EngineControl &ClDevice::getInternalEngine() { return device.getInternalEngine(); }
SelectorCopyEngine &ClDevice::getSelectorCopyEngine() { return device.getSelectorCopyEngine(); }
MemoryManager *ClDevice::getMemoryManager() const { return device.getMemoryManager(); }
GmmHelper *ClDevice::getGmmHelper() const { return device.getGmmHelper(); }
GmmClientContext *ClDevice::getGmmClientContext() const { return device.getGmmClientContext(); }
double ClDevice::getPlatformHostTimerResolution() const { return device.getPlatformHostTimerResolution(); }
GFXCORE_FAMILY ClDevice::getRenderCoreFamily() const { return device.getRenderCoreFamily(); }
PerformanceCounters *ClDevice::getPerformanceCounters() { return device.getPerformanceCounters(); }
PreemptionMode ClDevice::getPreemptionMode() const { return device.getPreemptionMode(); }
Debugger *ClDevice::getDebugger() { return device.getDebugger(); }
ExecutionEnvironment *ClDevice::getExecutionEnvironment() const { return device.getExecutionEnvironment(); }
const RootDeviceEnvironment &ClDevice::getRootDeviceEnvironment() const { return device.getRootDeviceEnvironment(); }
bool ClDevice::isFullRangeSvm() const { return device.isFullRangeSvm(); }
bool ClDevice::areSharedSystemAllocationsAllowed() const { return device.areSharedSystemAllocationsAllowed(); }
uint32_t ClDevice::getRootDeviceIndex() const { return device.getRootDeviceIndex(); }
uint32_t ClDevice::getNumGenericSubDevices() const { return device.getNumGenericSubDevices(); }
uint32_t ClDevice::getNumSubDevices() const { return static_cast<uint32_t>(subDevices.size()); }

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

void ClDevice::initGTPinHelper() {
    gtpinGfxCoreHelper = GTPinGfxCoreHelper::create(this->getRootDeviceEnvironment().getHardwareInfo()->platform.eRenderCoreFamily);
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
    auto &clGfxCoreHelper = this->getRootDeviceEnvironment().getHelper<ClGfxCoreHelper>();

    cl_command_queue_capabilities_intel disabledProperties = 0u;
    if (EngineHelper::isCopyOnlyEngineType(type)) {
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_KERNEL_INTEL);
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_FILL_BUFFER_INTEL);           // clEnqueueFillBuffer
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_INTEL);        // clEnqueueCopyImage
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_FILL_IMAGE_INTEL);            // clEnqueueFillImage
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_IMAGE_INTEL); // clEnqueueCopyBufferToImage
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_BUFFER_INTEL); // clEnqueueCopyImageToBuffer
    }
    disabledProperties |= clGfxCoreHelper.getAdditionalDisabledQueueFamilyCapabilities(type);

    if (disabledProperties != 0) {
        return getQueueFamilyCapabilitiesAll() & ~disabledProperties;
    }
    return CL_QUEUE_DEFAULT_CAPABILITIES_INTEL;
}

void ClDevice::getQueueFamilyName(char *outputName, EngineGroupType type) {
    std::string name{};

    const auto &clGfxCoreHelper = this->getRootDeviceEnvironment().getHelper<ClGfxCoreHelper>();
    const bool hasHwSpecificName = clGfxCoreHelper.getQueueFamilyName(name, type);

    if (!hasHwSpecificName) {
        switch (type) {
        case EngineGroupType::renderCompute:
            name = "rcs";
            break;
        case EngineGroupType::compute:
            name = "ccs";
            break;
        case EngineGroupType::copy:
            name = "bcs";
            break;
        default:
            name = "";
            break;
        }
    }

    UNRECOVERABLE_IF(name.size() >= CL_QUEUE_FAMILY_MAX_NAME_SIZE_INTEL);
    strncpy_s(outputName, CL_QUEUE_FAMILY_MAX_NAME_SIZE_INTEL, name.c_str(), name.size());
}
Platform *ClDevice::getPlatform() const {
    return castToObject<Platform>(platformId);
}
bool ClDevice::isPciBusInfoValid() const {
    return deviceInfo.pciBusInfo.pci_domain != PhysicalDevicePciBusInfo::invalidValue && deviceInfo.pciBusInfo.pci_bus != PhysicalDevicePciBusInfo::invalidValue &&
           deviceInfo.pciBusInfo.pci_device != PhysicalDevicePciBusInfo::invalidValue && deviceInfo.pciBusInfo.pci_function != PhysicalDevicePciBusInfo::invalidValue;
}
const GfxCoreHelper &ClDevice::getGfxCoreHelper() const {
    return device.getGfxCoreHelper();
}

const ProductHelper &ClDevice::getProductHelper() const {
    return device.getProductHelper();
}

const CompilerProductHelper &ClDevice::getCompilerProductHelper() const {
    return device.getCompilerProductHelper();
}

const GTPinGfxCoreHelper &ClDevice::getGTPinGfxCoreHelper() const {
    return *gtpinGfxCoreHelper;
}

cl_version ClDevice::getExtensionVersion(std::string name) {
    return getOclCExtensionVersion(name, CL_MAKE_VERSION(1u, 0, 0));
}

} // namespace NEO
