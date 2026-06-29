/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/platform/platform.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device_info.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/source/pin/pin.h"

#include "level_zero/api/opencl/source/helpers/get_info_status_mapper.h"
#include "level_zero/api/opencl/source/sharings/sharing_factory.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"

namespace NEO {
namespace LEO {

std::vector<std::unique_ptr<Platform>> *platformsImpl = nullptr;

Platform::Platform(ze_driver_handle_t driverHandle) : platformInfo(new PlatformInfo), driverHandle(driverHandle) {
    const auto &deviceHandles = L0::DriverHandle::fromHandle(driverHandle)->devicesToExpose;
    clDevices.reserve(deviceHandles.size());
    for (const auto &deviceHandle : deviceHandles) {
        const bool isSubdevice = L0::Device::fromHandle(deviceHandle)->getNEODevice()->isSubDevice();
        clDevices.emplace_back(new ClDevice(this, deviceHandle, isSubdevice));
        this->rootDeviceIndices.pushUnique(clDevices.back()->getRootDeviceIndex());
    }

    for (const auto &rootDeviceIndex : this->rootDeviceIndices) {
        DeviceBitfield deviceBitfield{};
        for (const auto &clDevice : this->clDevices) {
            if (clDevice->getRootDeviceIndex() == rootDeviceIndex) {
                deviceBitfield |= clDevice->getDevice().getDeviceBitfield();
            }
        }
        this->deviceBitfields.emplace(rootDeviceIndex, deviceBitfield);
    }

    this->platformInfo->extensions = this->clDevices[0]->getDeviceInfo().deviceExtensions;

    auto preferredPlatformName = this->clDevices[0]->getL0Object()->getHwInfo().capabilityTable.preferredPlatformName;
    if (preferredPlatformName != nullptr) {
        this->platformInfo->name = preferredPlatformName;
    }

    if (debugManager.flags.OverridePlatformName.get() != "unk") {
        this->platformInfo->name.assign(debugManager.flags.OverridePlatformName.get().c_str());
    }

    this->platformInfo->version = "OpenCL 3.0 ";
    this->platformInfo->numericVersion = CL_MAKE_VERSION(3, 0, 0);

    sharingFactory.fillGlobalDispatchTable();
}

cl_int Platform::getInfo(cl_platform_info paramName,
                         size_t paramValueSize,
                         void *paramValue,
                         size_t *paramValueSizeRet) {
    auto retVal = CL_INVALID_VALUE;
    const std::string *param = nullptr;
    size_t paramSize = GetInfo::invalidSourceSize;
    auto getInfoStatus = GetInfoStatus::invalidValue;

    switch (paramName) {
    case CL_PLATFORM_HOST_TIMER_RESOLUTION: {
        auto pVal = static_cast<uint64_t>(this->clDevices[0]->getPlatformHostTimerResolution());
        paramSize = sizeof(uint64_t);
        getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, &pVal, paramSize);
        break;
    }
    case CL_PLATFORM_NUMERIC_VERSION: {
        auto pVal = platformInfo->numericVersion;
        paramSize = sizeof(pVal);
        getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, &pVal, paramSize);
        break;
    }
    case CL_PLATFORM_EXTENSIONS_WITH_VERSION: {
        std::call_once(initializeExtensionsWithVersionOnce, [this]() {
            this->clDevices[0]->getDeviceInfo(CL_DEVICE_EXTENSIONS_WITH_VERSION, 0, nullptr, nullptr);
            this->platformInfo->extensionsWithVersion = this->clDevices[0]->getDeviceInfo().extensionsWithVersion;
        });

        auto pVal = platformInfo->extensionsWithVersion.data();
        paramSize = platformInfo->extensionsWithVersion.size() * sizeof(cl_name_version);
        getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, pVal, paramSize);
        break;
    }
    case CL_PLATFORM_PROFILE:
        param = &platformInfo->profile;
        break;
    case CL_PLATFORM_VERSION:
        param = &platformInfo->version;
        break;
    case CL_PLATFORM_NAME:
        param = &platformInfo->name;
        break;
    case CL_PLATFORM_VENDOR:
        param = &platformInfo->vendor;
        break;
    case CL_PLATFORM_EXTENSIONS:
        param = &platformInfo->extensions;
        break;
    case CL_PLATFORM_ICD_SUFFIX_KHR:
        param = &platformInfo->icdSuffixKhr;
        break;
    case CL_PLATFORM_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR: {
        paramSize = sizeof(this->clDevices[0]->getDeviceInfo().externalMemorySharing);
        getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, &this->clDevices[0]->getDeviceInfo().externalMemorySharing, paramSize);
        break;
    }
    case CL_PLATFORM_UNLOADABLE_KHR: {
        cl_bool unloadable = CL_TRUE;
        paramSize = sizeof(unloadable);
        getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, &unloadable, paramSize);
        break;
    }
    case CL_L0_DRIVER_HANDLE: {
        paramSize = sizeof(ze_driver_handle_t);
        getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, &this->driverHandle, paramSize);
        break;
    }
    default:
        break;
    }

    // Case for string parameters
    if (param) {
        paramSize = param->length() + 1;
        getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, param->c_str(), paramSize);
    }

    retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, paramSize, getInfoStatus);

    return retVal;
}

void Platform::tryNotifyGtpinInit() {
    std::call_once(oclInitGtpinOnce, []() {
        EnvironmentVariableReader envReader;
        if (envReader.getSetting("ZET_ENABLE_PROGRAM_INSTRUMENTATION", false)) {
            const std::string gtpinFuncName{"OpenGTPinOCL"};
            PinContext::init(gtpinFuncName);
        }
    });
}

} // namespace LEO
} // namespace NEO
