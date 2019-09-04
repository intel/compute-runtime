/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "drm_neo.h"

#include "core/memory_manager/memory_constants.h"
#include "runtime/os_interface/os_inc_base.h"
#include "runtime/utilities/directory.h"

#include "drm/i915_drm.h"

#include <cstdio>
#include <cstring>
#include <fstream>

namespace NEO {

const char *Drm::sysFsDefaultGpuPath = "/drm/card0";
const char *Drm::maxGpuFrequencyFile = "/gt_max_freq_mhz";
const char *Drm::configFileName = "/config";

int Drm::ioctl(unsigned long request, void *arg) {
    int ret;
    SYSTEM_ENTER();
    do {
        ret = ::ioctl(fd, request, arg);
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));
    SYSTEM_LEAVE(request);
    return ret;
}

int Drm::getParamIoctl(int param, int *dstValue) {
    drm_i915_getparam_t getParam = {};
    getParam.param = param;
    getParam.value = dstValue;

    return ioctl(DRM_IOCTL_I915_GETPARAM, &getParam);
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

int Drm::getMaxGpuFrequency(int &maxGpuFrequency) {
    maxGpuFrequency = 0;
    int deviceID = 0;
    int ret = getDeviceID(deviceID);
    if (ret != 0) {
        return ret;
    }
    std::string clockSysFsPath = getSysFsPciPath(deviceID);

    if (clockSysFsPath.size() == 0) {
        return 0;
    }

    clockSysFsPath += sysFsDefaultGpuPath;
    clockSysFsPath += maxGpuFrequencyFile;

    std::ifstream ifs(clockSysFsPath.c_str(), std::ifstream::in);
    if (ifs.fail()) {
        return 0;
    }

    ifs >> maxGpuFrequency;
    ifs.close();
    return 0;
}

std::string Drm::getSysFsPciPath(int deviceID) {
    std::string nullPath;
    std::string sysFsPciDirectory = Os::sysFsPciPath;
    std::vector<std::string> files = Directory::getFiles(sysFsPciDirectory);

    for (std::vector<std::string>::iterator file = files.begin(); file != files.end(); ++file) {
        PCIConfig config = {};
        std::string configPath = *file + configFileName;
        std::string sysfsPath = *file;
        std::ifstream configFile(configPath, std::ifstream::binary);
        if (configFile.is_open()) {
            configFile.read(reinterpret_cast<char *>(&config), sizeof(config));

            if (!configFile.good() || (config.DeviceID != deviceID)) {
                configFile.close();
                continue;
            }
            return sysfsPath;
        }
    }
    return nullPath;
}

bool Drm::is48BitAddressRangeSupported() {
    drm_i915_gem_context_param contextParam = {0};
    contextParam.param = I915_CONTEXT_PARAM_GTT_SIZE;

    auto ret = ioctl(DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM, &contextParam);
    if (ret == 0) {
        return contextParam.value > MemoryConstants::max64BitAppAddress;
    }
    return true;
}

void Drm::checkPreemptionSupport() {
    int value = 0;
    auto ret = getParamIoctl(I915_PARAM_HAS_SCHEDULER, &value);
    preemptionSupported = ((0 == ret) && (value & I915_SCHEDULER_CAP_PREEMPTION));
}

void Drm::setLowPriorityContextParam(uint32_t drmContextId) {
    drm_i915_gem_context_param gcp = {};
    gcp.ctx_id = drmContextId;
    gcp.param = I915_CONTEXT_PARAM_PRIORITY;
    gcp.value = -1023;

    auto retVal = ioctl(DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, &gcp);
    UNRECOVERABLE_IF(retVal != 0);
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

bool Drm::getSimplifiedMocsTableUsage() const {
    return useSimplifiedMocsTable;
}

void Drm::setSimplifiedMocsTableUsage(bool value) {
    useSimplifiedMocsTable = value;
}

} // namespace NEO
