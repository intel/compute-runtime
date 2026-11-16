/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"

#include "shared/source/compiler_interface/oclc_extensions.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/driver_info.h"

#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/helpers/leo_get_info_status_mapper.h"
#include "level_zero/api/opencl/source/l0_dispatch/leo_l0_dispatch.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/core/source/device/device.h"

namespace NEO {
namespace LEO {

Device &ClDevice::getDevice() const noexcept {
    return *getL0Object()->getNEODevice();
}

L0::Device *ClDevice::getL0Object() const {
    return L0::Device::fromHandle(this->deviceHandle);
}

NEO::DriverInfo *ClDevice::getDriverInfo() {
    return getL0Object()->driverInfo.get();
}

Platform *ClDevice::getPlatform() const {
    return castToObject<Platform>(this->platform);
}

cl_int ClDevice::getPeerInfo(const ClDevice &peer,
                             cl_device_peer_info_intel paramName,
                             size_t paramValueSize,
                             void *paramValue,
                             size_t *paramValueSizeRet) const {
    ze_device_p2p_property_flags_t requestedFlag = 0u;
    switch (paramName) {
    case CL_DEVICE_PEER_ACCESS_SUPPORTED_INTEL:
        requestedFlag = ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS;
        break;
    case CL_DEVICE_PEER_ATOMICS_SUPPORTED_INTEL:
        requestedFlag = ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS;
        break;
    default:
        return CL_INVALID_VALUE;
    }

    // Value-initialised on purpose: the driver reads pNext unconditionally and
    // leaves flags unwritten for a pair without peer access.
    ze_device_p2p_properties_t p2pProperties{ZE_STRUCTURE_TYPE_DEVICE_P2P_PROPERTIES};
    auto result = zeDeviceGetP2PProperties(getL0Handle(), peer.getL0Handle(), &p2pProperties);
    if (result != ZE_RESULT_SUCCESS) {
        return L0ToClResultMapper(result);
    }

    cl_bool value = ((p2pProperties.flags & requestedFlag) != 0u) ? CL_TRUE : CL_FALSE;
    auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, &value, sizeof(value));
    GetInfo::setParamValueReturnSize(paramValueSizeRet, sizeof(value), getInfoStatus);
    return changeGetInfoStatusToCLResultType(getInfoStatus);
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

    cl_command_queue_capabilities_intel disabledProperties = 0u;
    if (EngineHelper::isCopyOnlyEngineType(type)) {
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_KERNEL_INTEL);
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_FILL_BUFFER_INTEL);           // clEnqueueFillBuffer
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_INTEL);        // clEnqueueCopyImage
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_FILL_IMAGE_INTEL);            // clEnqueueFillImage
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_IMAGE_INTEL); // clEnqueueCopyBufferToImage
        disabledProperties |= static_cast<cl_command_queue_capabilities_intel>(CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_BUFFER_INTEL); // clEnqueueCopyImageToBuffer
    }

    if (disabledProperties != 0) {
        return getQueueFamilyCapabilitiesAll() & ~disabledProperties;
    }
    return CL_QUEUE_DEFAULT_CAPABILITIES_INTEL;
}

void ClDevice::getQueueFamilyName(char *outputName, EngineGroupType type) {
    std::string name{};

    switch (type) {
    case EngineGroupType::compute:
        name = "ccs";
        break;
    case EngineGroupType::copy:
        name = "bcs";
        break;
    case EngineGroupType::renderCompute:
        name = "cccs";
        break;
    case EngineGroupType::linkedCopy:
        name = "linked bcs";
        break;
    default:
        name = "";
        break;
    }

    UNRECOVERABLE_IF(name.size() >= CL_QUEUE_FAMILY_MAX_NAME_SIZE_INTEL);
    strncpy_s(outputName, CL_QUEUE_FAMILY_MAX_NAME_SIZE_INTEL, name.c_str(), name.size());
}

bool ClDevice::isPciBusInfoValid() const {
    return deviceInfo.pciBusInfo.pci_domain != PhysicalDevicePciBusInfo::invalidValue && deviceInfo.pciBusInfo.pci_bus != PhysicalDevicePciBusInfo::invalidValue &&
           deviceInfo.pciBusInfo.pci_device != PhysicalDevicePciBusInfo::invalidValue && deviceInfo.pciBusInfo.pci_function != PhysicalDevicePciBusInfo::invalidValue;
}

cl_version ClDevice::getExtensionVersion(std::string name) {
    return getOclCExtensionVersion(std::move(name), CL_MAKE_VERSION(1u, 0, 0));
}

} // namespace LEO
} // namespace NEO
