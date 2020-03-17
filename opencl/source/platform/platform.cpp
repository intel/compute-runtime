/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "platform.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/compiler_interface/compiler_interface.h"
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
#include "opencl/source/device/cl_device.h"
#include "opencl/source/event/async_events_handler.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/built_ins_helper.h"
#include "opencl/source/helpers/get_info_status_mapper.h"
#include "opencl/source/platform/extensions.h"
#include "opencl/source/sharings/sharing_factory.h"

#include "CL/cl_ext.h"
#include "gmm_client_context.h"

#include <algorithm>
#include <map>
namespace NEO {

std::vector<std::unique_ptr<Platform>> platformsImpl;

Platform::Platform(ExecutionEnvironment &executionEnvironmentIn) : executionEnvironment(executionEnvironmentIn) {
    clDevices.reserve(4);
    setAsyncEventsHandler(std::unique_ptr<AsyncEventsHandler>(new AsyncEventsHandler()));
    executionEnvironment.incRefInternal();
}

Platform::~Platform() {
    asyncEventsHandler->closeThread();
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
    size_t paramSize = 0;
    uint64_t pVal = 0;

    switch (paramName) {
    case CL_PLATFORM_HOST_TIMER_RESOLUTION:
        pVal = static_cast<uint64_t>(this->clDevices[0]->getPlatformHostTimerResolution());
        paramSize = sizeof(uint64_t);
        retVal = changeGetInfoStatusToCLResultType(::getInfo(paramValue, paramValueSize, &pVal, paramSize));
        break;
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
        retVal = changeGetInfoStatusToCLResultType(::getInfo(paramValue, paramValueSize, param->c_str(), paramSize));
    }

    if (paramValueSizeRet) {
        *paramValueSizeRet = paramSize;
    }

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

    if (DebugManager.flags.LoopAtPlatformInitialize.get()) {
        while (DebugManager.flags.LoopAtPlatformInitialize.get())
            this->initializationLoopHelper();
    }

    state = StateIniting;

    DEBUG_BREAK_IF(this->platformInfo);
    this->platformInfo.reset(new PlatformInfo);

    for (auto &inputDevice : devices) {
        ClDevice *pClDevice = nullptr;
        auto pDevice = inputDevice.release();
        UNRECOVERABLE_IF(!pDevice);
        pClDevice = new ClDevice{*pDevice, this};
        this->clDevices.push_back(pClDevice);

        this->platformInfo->extensions = pClDevice->getDeviceInfo().deviceExtensions;

        switch (pClDevice->getEnabledClVersion()) {
        case 21:
            this->platformInfo->version = "OpenCL 2.1 ";
            break;
        case 20:
            this->platformInfo->version = "OpenCL 2.0 ";
            break;
        default:
            this->platformInfo->version = "OpenCL 1.2 ";
            break;
        }
    }

    for (auto &clDevice : clDevices) {
        auto hwInfo = clDevice->getHardwareInfo();
        if (clDevice->getPreemptionMode() == PreemptionMode::MidThread || clDevice->isDebuggerActive()) {
            auto sipType = SipKernel::getSipKernelType(hwInfo.platform.eRenderCoreFamily, clDevice->isDebuggerActive());
            initSipKernel(sipType, clDevice->getDevice());
        }
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

AsyncEventsHandler *Platform::getAsyncEventsHandler() {
    return asyncEventsHandler.get();
}

std::unique_ptr<AsyncEventsHandler> Platform::setAsyncEventsHandler(std::unique_ptr<AsyncEventsHandler> handler) {
    asyncEventsHandler.swap(handler);
    return handler;
}

GmmHelper *Platform::peekGmmHelper() const {
    return executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();
}

GmmClientContext *Platform::peekGmmClientContext() const {
    return peekGmmHelper()->getClientContext();
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
