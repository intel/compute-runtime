/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "drm_neo.h"
#include "runtime/os_interface/os_inc.h"
#include "runtime/utilities/directory.h"
#include "drm/i915_drm.h"

#include <cstdio>
#include <cstring>
#include <fstream>

namespace OCLRT {

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

int Drm::getDeviceID(int &devId) {
    int ret = 0;
#if defined(I915_PARAM_CHIPSET_ID)
    drm_i915_getparam_t gp;

    gp.param = I915_PARAM_CHIPSET_ID;
    gp.value = &devId;

    ret = ioctl(DRM_IOCTL_I915_GETPARAM, &gp);
#endif
    return ret;
}

int Drm::getDeviceRevID(int &revId) {
    int ret = 0;
#if defined(I915_PARAM_REVISION)
    drm_i915_getparam_t gp;

    gp.param = I915_PARAM_REVISION;
    gp.value = &revId;

    ret = ioctl(DRM_IOCTL_I915_GETPARAM, &gp);
#endif
    return ret;
}

int Drm::getExecSoftPin(int &execSoftPin) {
    int ret = 0;

    drm_i915_getparam_t gp;

    gp.param = I915_PARAM_HAS_EXEC_SOFTPIN;
    gp.value = &execSoftPin;
    ret = ioctl(DRM_IOCTL_I915_GETPARAM, &gp);
    return ret;
}

int Drm::enableTurboBoost() {
    int ret = 0;
    struct drm_i915_gem_context_param contextParam;

    memset(&contextParam, 0, sizeof(contextParam));
    contextParam.param = I915_CONTEXT_PRIVATE_PARAM_BOOST;
    contextParam.value = 1;
    ret = ioctl(DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, &contextParam);
    return ret;
}

int Drm::getEnabledPooledEu(int &enabled) {
    int ret = 0;
    drm_i915_getparam_t gp;
#if defined(I915_PARAM_HAS_POOLED_EU)
    gp.value = &enabled;
    gp.param = I915_PARAM_HAS_POOLED_EU;
    ret = ioctl(DRM_IOCTL_I915_GETPARAM, &gp);
#endif
    return ret;
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

void Drm::obtainCoherencyDisablePatchActive() {
    drm_i915_getparam_t GPUParams;
    int value = 0;

    GPUParams.param = I915_PRIVATE_PARAM_HAS_EXEC_FORCE_NON_COHERENT;
    GPUParams.value = &value;

    auto retVal = ioctl(DRM_IOCTL_I915_GETPARAM, &GPUParams);

    if (retVal == 0) {
        coherencyDisablePatchActive = value != 0 ? 1 : 0;
    }
}

std::string Drm::getSysFsPciPath(int deviceID) {
    std::string nullPath;
    std::string sysFsPciDirectory = Os::sysFsPciPath;
    std::vector<std::string> files = Directory::getFiles(sysFsPciDirectory);

    for (std::vector<std::string>::iterator file = files.begin(); file != files.end(); ++file) {
        PCIConfig config;
        memset(&config, 0, sizeof(PCIConfig));
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
    drm_i915_getparam_t gp;
    int value = 0;
    gp.value = &value;

    gp.param = I915_PARAM_HAS_ALIASING_PPGTT;
    int ret = ioctl(DRM_IOCTL_I915_GETPARAM, &gp);
    if (ret == 0 && *gp.value == 3)
        return true;
    return false;
}

bool Drm::hasPreemption() {
#if defined(I915_PARAM_HAS_PREEMPTION)
    drm_i915_getparam_t gp;
    int value = 0;
    gp.value = &value;
    gp.param = I915_PARAM_HAS_PREEMPTION;
    int ret = ioctl(DRM_IOCTL_I915_GETPARAM, &gp);
    if (ret == 0 && *gp.value == 1) {
        return contextCreate() && setLowPriority();
    }
#endif
    return false;
}

bool Drm::setLowPriority() {
#if defined(I915_PARAM_HAS_PREEMPTION)
    struct drm_i915_gem_context_param gcp = {0};
    gcp.ctx_id = lowPriorityContextId;
    gcp.param = I915_CONTEXT_PARAM_PRIORITY;
    gcp.value = -1023;

    int ret = ioctl(DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, &gcp);
    if (ret == 0) {
        return true;
    }
#endif
    return false;
}

bool Drm::contextCreate() {
#if defined(I915_PARAM_HAS_PREEMPTION)
    struct drm_i915_gem_context_create gcc;
    memset(&gcc, 0, sizeof(gcc));
    int ret = ioctl(DRM_IOCTL_I915_GEM_CONTEXT_CREATE, &gcc);
    if (ret == 0) {
        lowPriorityContextId = gcc.ctx_id;
        return true;
    }
#endif
    return false;
}

void Drm::contextDestroy() {
#if defined(I915_PARAM_HAS_PREEMPTION)
    struct drm_i915_gem_context_destroy destroy;
    destroy.ctx_id = lowPriorityContextId;
    ioctl(DRM_IOCTL_I915_GEM_CONTEXT_DESTROY, &destroy);
#endif
}

int Drm::getEuTotal(int &euTotal) {
#if defined(I915_PARAM_EU_TOTAL) || defined(I915_PARAM_EU_COUNT)
    drm_i915_getparam_t gp;

    memset(&gp, 0, sizeof(gp));
    gp.value = &euTotal;
    gp.param =
#if defined(I915_PARAM_EU_TOTAL)
        I915_PARAM_EU_TOTAL;
#elif defined(I915_PARAM_EU_COUNT)
        I915_PARAM_EU_COUNT;
#endif

    int ret = ioctl(DRM_IOCTL_I915_GETPARAM, &gp);
    return ret;
#else
    (void)euTotal;
    return 0;
#endif
}

int Drm::getSubsliceTotal(int &subsliceTotal) {
#if defined(I915_PARAM_SUBSLICE_TOTAL)
    drm_i915_getparam_t gp;

    memset(&gp, 0, sizeof(gp));
    gp.value = &subsliceTotal;
    gp.param = I915_PARAM_SUBSLICE_TOTAL;

    int ret = ioctl(DRM_IOCTL_I915_GETPARAM, &gp);
    return ret;
#else
    (void)subsliceTotal;
    return 0;
#endif
}

int Drm::getMinEuInPool(int &minEUinPool) {
#if defined(I915_PARAM_MIN_EU_IN_POOL)
    drm_i915_getparam_t gp;
    memset(&gp, 0, sizeof(gp));
    gp.value = &minEUinPool;
    gp.param = I915_PARAM_MIN_EU_IN_POOL;

    int ret = ioctl(DRM_IOCTL_I915_GETPARAM, &gp);
    return ret;
#else
    (void)minEUinPool;
    return 0;
#endif
}

} // namespace OCLRT
