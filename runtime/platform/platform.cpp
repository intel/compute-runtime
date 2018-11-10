/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "platform.h"
#include "runtime/api/api.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "CL/cl_ext.h"
#include "runtime/device/device.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gtpin/gtpin_notify.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/string.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/event/async_events_handler.h"
#include "runtime/sharings/sharing_factory.h"
#include "runtime/platform/extensions.h"
#include "CL/cl_ext.h"

namespace OCLRT {

std::unique_ptr<Platform> platformImpl;

bool getDevices(HardwareInfo **hwInfo, size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment);

Platform *platform() { return platformImpl.get(); }

Platform *constructPlatform() {
    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    if (!platformImpl) {
        platformImpl.reset(new Platform());
    }
    return platformImpl.get();
}

Platform::Platform() {
    devices.reserve(4);
    setAsyncEventsHandler(std::unique_ptr<AsyncEventsHandler>(new AsyncEventsHandler()));
    executionEnvironment = new ExecutionEnvironment;
    executionEnvironment->incRefInternal();
}

Platform::~Platform() {
    asyncEventsHandler->closeThread();
    for (auto dev : this->devices) {
        if (dev) {
            dev->decRefInternal();
        }
    }

    gtpinNotifyPlatformShutdown();
    executionEnvironment->decRefInternal();
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
        pVal = static_cast<uint64_t>(this->devices[0]->getPlatformHostTimerResolution());
        paramSize = sizeof(uint64_t);
        retVal = ::getInfo(paramValue, paramValueSize, &pVal, paramSize);
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
        retVal = ::getInfo(paramValue, paramValueSize, param->c_str(), paramSize);
    }

    if (paramValueSizeRet) {
        *paramValueSizeRet = paramSize;
    }

    return retVal;
}

const std::string &Platform::peekCompilerExtensions() const {
    return compilerExtensions;
}

bool Platform::initialize() {
    HardwareInfo *hwInfo = nullptr;
    size_t numDevicesReturned = 0;

    TakeOwnershipWrapper<Platform> platformOwnership(*this);

    if (state == StateInited) {
        return true;
    }

    if (DebugManager.flags.LoopAtPlatformInitialize.get()) {
        while (DebugManager.flags.LoopAtPlatformInitialize.get())
            this->initializationLoopHelper();
    }

    state = OCLRT::getDevices(&hwInfo, numDevicesReturned, *executionEnvironment) ? StateIniting : StateNone;

    if (state == StateNone) {
        return false;
    }

    DEBUG_BREAK_IF(this->platformInfo);
    this->platformInfo.reset(new PlatformInfo);

    this->devices.resize(numDevicesReturned);
    for (uint32_t deviceOrdinal = 0; deviceOrdinal < numDevicesReturned; ++deviceOrdinal) {
        auto pDevice = Device::create<OCLRT::Device>(&hwInfo[deviceOrdinal], executionEnvironment, deviceOrdinal);
        DEBUG_BREAK_IF(!pDevice);
        if (pDevice) {
            this->devices[deviceOrdinal] = pDevice;

            this->platformInfo->extensions = pDevice->getDeviceInfo().deviceExtensions;

            switch (pDevice->getEnabledClVersion()) {
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

            compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(pDevice->getDeviceInfo().deviceExtensions);
        } else {
            return false;
        }
    }

    CommandStreamReceiverType csrType = this->devices[0]->getCommandStreamReceiver().getType();
    if (csrType != CommandStreamReceiverType::CSR_HW) {
        executionEnvironment->initAubCenter(&hwInfo[0], this->devices[0]->getEnableLocalMemory());
    }

    this->fillGlobalDispatchTable();

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

Device *Platform::getDevice(size_t deviceOrdinal) {
    TakeOwnershipWrapper<Platform> platformOwnership(*this);

    if (this->state != StateInited || deviceOrdinal >= devices.size()) {
        return nullptr;
    }

    auto pDevice = devices[deviceOrdinal];
    DEBUG_BREAK_IF(pDevice == nullptr);

    return pDevice;
}

size_t Platform::getNumDevices() const {
    TakeOwnershipWrapper<const Platform> platformOwnership(*this);

    if (this->state != StateInited) {
        return 0;
    }

    return devices.size();
}

Device **Platform::getDevices() {
    TakeOwnershipWrapper<Platform> platformOwnership(*this);

    if (this->state != StateInited) {
        return nullptr;
    }

    return devices.data();
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

} // namespace OCLRT
