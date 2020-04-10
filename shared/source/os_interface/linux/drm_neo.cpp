/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "drm_neo.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/hw_device_id.h"
#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/directory.h"

#include <cstdio>
#include <cstring>
#include <linux/limits.h>

namespace NEO {

namespace IoctlHelper {
constexpr const char *getIoctlParamString(int param) {
    switch (param) {
    case I915_PARAM_CHIPSET_ID:
        return "I915_PARAM_CHIPSET_ID";
    case I915_PARAM_REVISION:
        return "I915_PARAM_REVISION";
    case I915_PARAM_HAS_EXEC_SOFTPIN:
        return "I915_PARAM_HAS_EXEC_SOFTPIN";
    case I915_PARAM_HAS_POOLED_EU:
        return "I915_PARAM_HAS_POOLED_EU";
    case I915_PARAM_HAS_SCHEDULER:
        return "I915_PARAM_HAS_SCHEDULER";
    case I915_PARAM_EU_TOTAL:
        return "I915_PARAM_EU_TOTAL";
    case I915_PARAM_SUBSLICE_TOTAL:
        return "I915_PARAM_SUBSLICE_TOTAL";
    case I915_PARAM_MIN_EU_IN_POOL:
        return "I915_PARAM_MIN_EU_IN_POOL";
    default:
        break;
    }

    return "UNKNOWN";
}

} // namespace IoctlHelper

int Drm::ioctl(unsigned long request, void *arg) {
    int ret;
    SYSTEM_ENTER();
    do {
        ret = SysCalls::ioctl(getFileDescriptor(), request, arg);
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));
    SYSTEM_LEAVE(request);
    return ret;
}

int Drm::getParamIoctl(int param, int *dstValue) {
    drm_i915_getparam_t getParam = {};
    getParam.param = param;
    getParam.value = dstValue;

    int retVal = ioctl(DRM_IOCTL_I915_GETPARAM, &getParam);

    printDebugString(DebugManager.flags.PrintDebugMessages.get(), stdout,
                     "\nDRM_IOCTL_I915_GETPARAM: param: %s, output value: %d, retCode: %d\n",
                     IoctlHelper::getIoctlParamString(param), *getParam.value, retVal);

    return retVal;
}

int Drm::getDeviceID(int &devId) {
    return getParamIoctl(I915_PARAM_CHIPSET_ID, &devId);
}

int Drm::getDeviceRevID(int &revId) {
    return getParamIoctl(I915_PARAM_REVISION, &revId);
}

int Drm::getExecSoftPin(int &execSoftPin) {
    return getParamIoctl(I915_PARAM_HAS_EXEC_SOFTPIN, &execSoftPin);
}

int Drm::enableTurboBoost() {
    drm_i915_gem_context_param contextParam = {};

    contextParam.param = I915_CONTEXT_PRIVATE_PARAM_BOOST;
    contextParam.value = 1;
    return ioctl(DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, &contextParam);
}

int Drm::getEnabledPooledEu(int &enabled) {
    return getParamIoctl(I915_PARAM_HAS_POOLED_EU, &enabled);
}

std::string Drm::getSysFsPciPath() {
    std::string path = std::string(Os::sysFsPciPathPrefix) + hwDeviceId->getPciPath() + "/drm";
    std::string expectedFilePrefix = path + "/card";
    auto files = Directory::getFiles(path.c_str());
    for (auto &file : files) {
        if (file.find(expectedFilePrefix.c_str()) != std::string::npos) {
            return file;
        }
    }
    return {};
}

int Drm::queryGttSize(uint64_t &gttSizeOutput) {
    drm_i915_gem_context_param contextParam = {0};
    contextParam.param = I915_CONTEXT_PARAM_GTT_SIZE;

    int ret = ioctl(DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM, &contextParam);
    if (ret == 0) {
        gttSizeOutput = contextParam.value;
    }

    return ret;
}

void Drm::checkPreemptionSupport() {
    int value = 0;
    auto ret = getParamIoctl(I915_PARAM_HAS_SCHEDULER, &value);
    preemptionSupported = ((0 == ret) && (value & I915_SCHEDULER_CAP_PREEMPTION));
}

void Drm::checkQueueSliceSupport() {
    sliceCountChangeSupported = getQueueSliceCount(&sseu) == 0 ? true : false;
}

void Drm::setLowPriorityContextParam(uint32_t drmContextId) {
    drm_i915_gem_context_param gcp = {};
    gcp.ctx_id = drmContextId;
    gcp.param = I915_CONTEXT_PARAM_PRIORITY;
    gcp.value = -1023;

    auto retVal = ioctl(DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, &gcp);
    UNRECOVERABLE_IF(retVal != 0);
}

int Drm::getQueueSliceCount(drm_i915_gem_context_param_sseu *sseu) {
    drm_i915_gem_context_param contextParam = {};
    contextParam.param = I915_CONTEXT_PARAM_SSEU;
    sseu->engine.engine_class = I915_ENGINE_CLASS_RENDER;
    sseu->engine.engine_instance = I915_EXEC_DEFAULT;
    contextParam.value = reinterpret_cast<uint64_t>(sseu);
    contextParam.size = sizeof(struct drm_i915_gem_context_param_sseu);

    return ioctl(DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM, &contextParam);
}

uint64_t Drm::getSliceMask(uint64_t sliceCount) {
    return maxNBitValue(sliceCount);
}
bool Drm::setQueueSliceCount(uint64_t sliceCount) {
    if (sliceCountChangeSupported) {
        drm_i915_gem_context_param contextParam = {};
        sseu.slice_mask = getSliceMask(sliceCount);

        contextParam.param = I915_CONTEXT_PARAM_SSEU;
        contextParam.ctx_id = 0;
        contextParam.value = reinterpret_cast<uint64_t>(&sseu);
        contextParam.size = sizeof(struct drm_i915_gem_context_param_sseu);
        int retVal = ioctl(DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, &contextParam);
        if (retVal == 0) {
            return true;
        }
    }
    return false;
}

void Drm::checkNonPersistentContextsSupport() {
    drm_i915_gem_context_param contextParam = {};
    contextParam.param = I915_CONTEXT_PARAM_PERSISTENCE;

    auto retVal = ioctl(DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM, &contextParam);
    if (retVal == 0 && contextParam.value == 1) {
        nonPersistentContextsSupported = true;
    } else {
        nonPersistentContextsSupported = false;
    }
}

void Drm::setNonPersistentContext(uint32_t drmContextId) {
    drm_i915_gem_context_param contextParam = {};
    contextParam.ctx_id = drmContextId;
    contextParam.param = I915_CONTEXT_PARAM_PERSISTENCE;

    ioctl(DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, &contextParam);
}

uint32_t Drm::createDrmContext() {
    drm_i915_gem_context_create gcc = {};
    auto retVal = ioctl(DRM_IOCTL_I915_GEM_CONTEXT_CREATE, &gcc);
    UNRECOVERABLE_IF(retVal != 0);

    return gcc.ctx_id;
}

void Drm::destroyDrmContext(uint32_t drmContextId) {
    drm_i915_gem_context_destroy destroy = {};
    destroy.ctx_id = drmContextId;
    auto retVal = ioctl(DRM_IOCTL_I915_GEM_CONTEXT_DESTROY, &destroy);
    UNRECOVERABLE_IF(retVal != 0);
}

int Drm::getEuTotal(int &euTotal) {
    return getParamIoctl(I915_PARAM_EU_TOTAL, &euTotal);
}

int Drm::getSubsliceTotal(int &subsliceTotal) {
    return getParamIoctl(I915_PARAM_SUBSLICE_TOTAL, &subsliceTotal);
}

int Drm::getMinEuInPool(int &minEUinPool) {
    return getParamIoctl(I915_PARAM_MIN_EU_IN_POOL, &minEUinPool);
}

int Drm::getErrno() {
    return errno;
}

int Drm::setupHardwareInfo(DeviceDescriptor *device, bool setupFeatureTableAndWorkaroundTable) {
    HardwareInfo *hwInfo = const_cast<HardwareInfo *>(device->pHwInfo);
    int ret;
    int euTotal;
    int subsliceTotal;

    ret = getEuTotal(euTotal);
    if (ret != 0) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query EU total parameter!\n");
        return ret;
    }

    ret = getSubsliceTotal(subsliceTotal);
    if (ret != 0) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query subslice total parameter!\n");
        return ret;
    }

    hwInfo->gtSystemInfo.EUCount = static_cast<uint32_t>(euTotal);
    hwInfo->gtSystemInfo.SubSliceCount = static_cast<uint32_t>(subsliceTotal);
    device->setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    return 0;
}

void appendHwDeviceId(std::vector<std::unique_ptr<HwDeviceId>> &hwDeviceIds, int fileDescriptor, const char *pciPath) {
    if (fileDescriptor >= 0) {
        if (Drm::isi915Version(fileDescriptor)) {
            hwDeviceIds.push_back(std::make_unique<HwDeviceId>(fileDescriptor, pciPath));
        } else {
            SysCalls::close(fileDescriptor);
        }
    }
}

std::vector<std::unique_ptr<HwDeviceId>> OSInterface::discoverDevices(ExecutionEnvironment &executionEnvironment) {
    std::vector<std::unique_ptr<HwDeviceId>> hwDeviceIds;
    executionEnvironment.osEnvironment = std::make_unique<OsEnvironment>();
    std::string devicePrefix = std::string(Os::pciDevicesDirectory) + "/pci-0000:";
    const char *renderDeviceSuffix = "-render";
    size_t numRootDevices = 0u;
    if (DebugManager.flags.CreateMultipleRootDevices.get()) {
        numRootDevices = DebugManager.flags.CreateMultipleRootDevices.get();
    }

    std::vector<std::string> files = Directory::getFiles(Os::pciDevicesDirectory);

    if (files.size() == 0) {
        const char *pathPrefix = "/dev/dri/renderD";
        const unsigned int maxDrmDevices = 64;
        unsigned int startNum = 128;

        for (unsigned int i = 0; i < maxDrmDevices; i++) {
            std::string path = std::string(pathPrefix) + std::to_string(i + startNum);
            int fileDescriptor = SysCalls::open(path.c_str(), O_RDWR);
            appendHwDeviceId(hwDeviceIds, fileDescriptor, "00:02.0");
        }
        return hwDeviceIds;
    }

    do {
        for (std::vector<std::string>::iterator file = files.begin(); file != files.end(); ++file) {
            if (file->find(renderDeviceSuffix) == std::string::npos) {
                continue;
            }
            std::string pciPath = file->substr(devicePrefix.size(), file->size() - devicePrefix.size() - strlen(renderDeviceSuffix));

            if (DebugManager.flags.ForceDeviceId.get() != "unk") {
                if (file->find(DebugManager.flags.ForceDeviceId.get().c_str()) == std::string::npos) {
                    continue;
                }
            }
            int fileDescriptor = SysCalls::open(file->c_str(), O_RDWR);
            appendHwDeviceId(hwDeviceIds, fileDescriptor, pciPath.c_str());
        }
        if (hwDeviceIds.empty()) {
            return hwDeviceIds;
        }
    } while (hwDeviceIds.size() < numRootDevices);
    return hwDeviceIds;
}

bool Drm::isi915Version(int fileDescriptor) {
    drm_version_t version = {};
    char name[5] = {};
    version.name = name;
    version.name_len = 5;

    int ret = SysCalls::ioctl(fileDescriptor, DRM_IOCTL_VERSION, &version);
    if (ret) {
        return false;
    }

    name[4] = '\0';
    return strcmp(name, "i915") == 0;
}

Drm::~Drm() = default;

} // namespace NEO
