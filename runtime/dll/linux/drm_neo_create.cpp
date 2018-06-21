/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "hw_cmds.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include "runtime/os_interface/linux/drm_null_device.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "drm/i915_drm.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include <stdio.h>
#include <limits.h>
#include <array>

namespace OCLRT {

const DeviceDescriptor deviceDescriptorTable[] = {
#define DEVICE(devId, gt, gtType) {devId, &gt::hwInfo, &gt::setupGtSystemInfo, gtType},
#include "devices.m"
#undef DEVICE
    {0, nullptr, nullptr, GTTYPE_UNDEFINED}};

static std::array<Drm *, 1> drms = {{nullptr}};

Drm::~Drm() {
    if (lowPriorityContextId)
        contextDestroy();
    close(fd);
    fd = -1;
}

Drm *Drm::get(int32_t deviceOrdinal) {
    if (static_cast<uint32_t>(deviceOrdinal) >= drms.size())
        return nullptr;
    return drms[deviceOrdinal % drms.size()];
}

void Drm::closeDevice(int32_t deviceOrdinal) {
    if (static_cast<uint32_t>(deviceOrdinal) >= drms.size())
        return;
    if (drms[deviceOrdinal % drms.size()] != nullptr) {
        delete drms[deviceOrdinal % drms.size()];
        drms[deviceOrdinal % drms.size()] = nullptr;
    }
}

bool Drm::isi915Version(int fd) {
    drm_version_t version = {};
    int ret;

    version.name = reinterpret_cast<char *>(calloc(1, 5));
    version.name_len = 5;

    ret = ::ioctl(fd, DRM_IOCTL_VERSION, &version);

    if (ret) {
        free(version.name);
        return false;
    }

    version.name[4] = '\0';

    if (!strcmp(version.name, "i915")) {
        free(version.name);
        return true;
    }

    free(version.name);

    return false;
}

int Drm::getDeviceFd(const int devType) {
    int fd = -1;
    char fullPath[PATH_MAX];
    const char *pathPrefix;
    unsigned int startNum;
    const unsigned int maxDrmDevices = 64;

    switch (devType) {
    case 0:
        startNum = 128;
        pathPrefix = "/dev/dri/renderD";
        break;
    default:
        startNum = 0;
        pathPrefix = "/dev/dri/card";
        break;
    }

    for (unsigned int i = 0; i < maxDrmDevices; i++) {
        snprintf(fullPath, PATH_MAX, "%s%d", pathPrefix, i + startNum);
        if ((fd = ::open(fullPath, O_RDWR)) >= 0) {
            if (isi915Version(fd)) {
                break;
            }
            ::close(fd);
            fd = -1;
        }
    }

    return fd;
}

int Drm::openDevice() {
    int fd = -1;
    fd = getDeviceFd(0);
    if (fd < 0) {
        fd = getDeviceFd(1);
    }

    return fd;
}

Drm *Drm::create(int32_t deviceOrdinal) {
    //right now we support only one device
    if (deviceOrdinal != 0)
        return nullptr;

    if (drms[deviceOrdinal % drms.size()] != nullptr)
        return drms[deviceOrdinal % drms.size()];

    auto fd = openDevice();

    if (fd == -1) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot open device!\n");
        return nullptr;
    }

    Drm *drmObject = nullptr;
    if (DebugManager.flags.EnableNullHardware.get() == true) {
        drmObject = new DrmNullDevice(fd);
    } else {
        drmObject = new Drm(fd);
    }

    // Get HW version (I915_drm.h)
    int ret = drmObject->getDeviceID(drmObject->deviceId);
    if (ret != 0) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query device ID parameter!\n");
        delete drmObject;
        return nullptr;
    }

    // Get HW Revision (I915_drm.h)
    ret = drmObject->getDeviceRevID(drmObject->revisionId);
    if (ret != 0) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query device Rev ID parameter!\n");
        delete drmObject;
        return nullptr;
    }

    const DeviceDescriptor *device = nullptr;
    GTTYPE eGtType = GTTYPE_UNDEFINED;
    for (auto &d : deviceDescriptorTable) {
        if (drmObject->deviceId == d.deviceId) {
            device = &d;
            eGtType = d.eGtType;
            break;
        }
    }
    if (device) {
        platformDevices[0] = device->pHwInfo;
        device->setupGtSystemInfo(const_cast<GT_SYSTEM_INFO *>(platformDevices[0]->pSysInfo));
        drmObject->setGtType(eGtType);
    } else {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Unknown device: deviceId: %04x, revisionId: %04x\n",
                         drmObject->deviceId, drmObject->revisionId);
        delete drmObject;
        return nullptr;
    }

    // Detect device parameters
    int hasExecSoftPin = 0;
    ret = drmObject->getExecSoftPin(hasExecSoftPin);
    if (ret != 0) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query Soft Pin parameter!\n");
        delete drmObject;
        return nullptr;
    }

    if (!hasExecSoftPin) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Device doesn't support Soft-Pin but this is required.\n");
        delete drmObject;
        return nullptr;
    }

    // Activate the Turbo Boost Frequency feature
    ret = drmObject->enableTurboBoost();
    if (ret != 0) {
        //turbo patch not present, we are not on custom Kernel, switch to simplified Mocs selection
        //do this only for GEN9+
        if (device->pHwInfo->pPlatform->eRenderCoreFamily >= IGFX_GEN9_CORE) {
            GmmHelper::useSimplifiedMocsTable = true;
        }
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Failed to request OCL Turbo Boost\n");
    }

    drms[deviceOrdinal % drms.size()] = drmObject;
    return drms[deviceOrdinal % drms.size()];
}

} // namespace OCLRT
