/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "platform.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/oclc_extensions.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/root_device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"

#include "opencl/source/api/api.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/get_info_status_mapper.h"
#include "opencl/source/sharings/sharing_factory.h"

#include "CL/cl_ext.h"

#include <algorithm>
#include <map>
namespace NEO {

std::vector<std::unique_ptr<Platform>> *platformsImpl = nullptr;

Platform::Platform(ExecutionEnvironment &executionEnvironmentIn) : executionEnvironment(executionEnvironmentIn) {
    clDevices.reserve(4);
    executionEnvironment.incRefInternal();
}

Platform::~Platform() {
    for (auto clDevice : this->clDevices) {
        clDevice->decRefInternal();
    }

    gtpinNotifyPlatformShutdown();
    executionEnvironment.decRefInternal();
}

cl_int Platform::getInfo(cl_platform_info paramName,
                         size_t paramValueSize,
                         void *paramValue,
                         size_t *paramValueSizeRet) {
    auto retVal = CL_INVALID_VALUE;
    const std::string *param = nullptr;
    size_t paramSize = GetInfo::invalidSourceSize;
    auto getInfoStatus = GetInfoStatus::INVALID_VALUE;

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

bool Platform::initialize(std::vector<std::unique_ptr<Device>> devices) {

    TakeOwnershipWrapper<Platform> platformOwnership(*this);
    if (devices.empty()) {
        return false;
    }
    if (state == StateInited) {
        return true;
    }

    state = StateIniting;

    for (auto &inputDevice : devices) {
        ClDevice *pClDevice = nullptr;
        auto pDevice = inputDevice.release();
        UNRECOVERABLE_IF(!pDevice);
        pClDevice = new ClDevice{*pDevice, this};
        this->clDevices.push_back(pClDevice);

        if (pClDevice->getPreemptionMode() == PreemptionMode::MidThread || pClDevice->isDebuggerActive()) {
            bool ret = SipKernel::initSipKernel(SipKernel::getSipKernelType(*pDevice), *pDevice);
            UNRECOVERABLE_IF(!ret);
        }
    }

    DEBUG_BREAK_IF(this->platformInfo);
    this->platformInfo.reset(new PlatformInfo);

    this->platformInfo->extensions = this->clDevices[0]->getDeviceInfo().deviceExtensions;

    switch (this->clDevices[0]->getEnabledClVersion()) {
    case 30:
        this->platformInfo->version = "OpenCL 3.0 ";
        this->platformInfo->numericVersion = CL_MAKE_VERSION(3, 0, 0);
        break;
    case 21:
        this->platformInfo->version = "OpenCL 2.1 ";
        this->platformInfo->numericVersion = CL_MAKE_VERSION(2, 1, 0);
        break;
    default:
        this->platformInfo->version = "OpenCL 1.2 ";
        this->platformInfo->numericVersion = CL_MAKE_VERSION(1, 2, 0);
        break;
    }

    this->fillGlobalDispatchTable();
    DEBUG_BREAK_IF(DebugManager.flags.CreateMultipleSubDevices.get() > 1 && !this->clDevices[0]->getDefaultEngine().commandStreamReceiver->peekTimestampPacketWriteEnabled());
    state = StateInited;
    return true;
}

void Platform::fillGlobalDispatchTable() {
    sharingFactory.fillGlobalDispatchTable();
}

bool Platform::isInitialized() {
    TakeOwnershipWrapper<Platform> platformOwnership(*this);
    bool ret = (this->state == StateInited);
    return ret;
}

ClDevice *Platform::getClDevice(size_t deviceOrdinal) {
    TakeOwnershipWrapper<Platform> platformOwnership(*this);

    if (this->state != StateInited || deviceOrdinal >= clDevices.size()) {
        return nullptr;
    }

    auto pClDevice = clDevices[deviceOrdinal];
    DEBUG_BREAK_IF(pClDevice == nullptr);

    return pClDevice;
}

size_t Platform::getNumDevices() const {
    TakeOwnershipWrapper<const Platform> platformOwnership(*this);

    if (this->state != StateInited) {
        return 0;
    }

    return clDevices.size();
}

ClDevice **Platform::getClDevices() {
    TakeOwnershipWrapper<Platform> platformOwnership(*this);

    if (this->state != StateInited) {
        return nullptr;
    }

    return clDevices.data();
}

const PlatformInfo &Platform::getPlatformInfo() const {
    DEBUG_BREAK_IF(!platformInfo);
    return *platformInfo;
}

std::unique_ptr<Platform> (*Platform::createFunc)(ExecutionEnvironment &) = [](ExecutionEnvironment &executionEnvironment) -> std::unique_ptr<Platform> {
    return std::make_unique<Platform>(executionEnvironment);
};

std::vector<DeviceVector> Platform::groupDevices(DeviceVector devices) {
    std::map<PRODUCT_FAMILY, size_t> platformsMap;
    std::vector<DeviceVector> outDevices;
    for (auto &device : devices) {
        auto productFamily = device->getHardwareInfo().platform.eProductFamily;
        auto result = platformsMap.find(productFamily);
        if (result == platformsMap.end()) {
            platformsMap.insert({productFamily, platformsMap.size()});
            outDevices.push_back(DeviceVector{});
        }
        auto platformId = platformsMap[productFamily];
        outDevices[platformId].push_back(std::move(device));
    }
    std::sort(outDevices.begin(), outDevices.end(), [](DeviceVector &lhs, DeviceVector &rhs) -> bool {
        return lhs[0]->getHardwareInfo().platform.eProductFamily > rhs[0]->getHardwareInfo().platform.eProductFamily;
    });
    return outDevices;
}

} // namespace NEO
