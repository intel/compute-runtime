/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include "runtime/os_interface/linux/drm_null_device.h"

#include "drm/i915_drm.h"
#include "hw_cmds.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>

namespace NEO {

const DeviceDescriptor deviceDescriptorTable[] = {
#define DEVICE(devId, gt, gtType) {devId, &gt::hwInfo, &gt::setupHardwareInfo, gtType},
#include "devices.inl"
#undef DEVICE
    {0, nullptr, nullptr, GTTYPE_UNDEFINED}};

static std::array<Drm *, 1> drms = {{nullptr}};

Drm::~Drm() {
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
    char name[5] = {};
    version.name = name;
    version.name_len = 5;

    int ret = ::ioctl(fd, DRM_IOCTL_VERSION, &version);
    if (ret) {
        return false;
    }

    name[4] = '\0';
    return strcmp(name, "i915") == 0;
}

int Drm::getDeviceFd(const int devType) {
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
        snprintf(fullPath, PATH_MAX, "%s%u", pathPrefix, i + startNum);
        int fd = ::open(fullPath, O_RDWR);
        if (fd >= 0) {
            if (isi915Version(fd)) {
                return fd;
            }
            ::close(fd);
        }
    }

    return -1;
}

int Drm::openDevice() {
    int fd = getDeviceFd(0);
    if (fd < 0) {
        fd = getDeviceFd(1);
    }

    return fd;
}

Drm *Drm::create(int32_t deviceOrdinal) {
    // right now we support only one device
    if (deviceOrdinal != 0)
        return nullptr;

    if (drms[deviceOrdinal % drms.size()] != nullptr)
        return drms[deviceOrdinal % drms.size()];

    auto fd = openDevice();

    if (fd == -1) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot open device!\n");
        return nullptr;
    }

    std::unique_ptr<Drm> drmObject;
    if (DebugManager.flags.EnableNullHardware.get() == true) {
        drmObject.reset(new DrmNullDevice(fd));
    } else {
        drmObject.reset(new Drm(fd));
    }

    // Get HW version (I915_drm.h)
    int ret = drmObject->getDeviceID(drmObject->deviceId);
    if (ret != 0) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query device ID parameter!\n");
        return nullptr;
    }

    // Get HW Revision (I915_drm.h)
    ret = drmObject->getDeviceRevID(drmObject->revisionId);
    if (ret != 0) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query device Rev ID parameter!\n");
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
        device->setupHardwareInfo(const_cast<HardwareInfo *>(platformDevices[0]), true);
        drmObject->setGtType(eGtType);
    } else {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr,
                         "FATAL: Unknown device: deviceId: %04x, revisionId: %04x\n", drmObject->deviceId, drmObject->revisionId);
        return nullptr;
    }

    // Detect device parameters
    int hasExecSoftPin = 0;
    ret = drmObject->getExecSoftPin(hasExecSoftPin);
    if (ret != 0) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query Soft Pin parameter!\n");
        return nullptr;
    }

    if (!hasExecSoftPin) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s",
                         "FATAL: Device doesn't support Soft-Pin but this is required.\n");
        return nullptr;
    }

    // Activate the Turbo Boost Frequency feature
    ret = drmObject->enableTurboBoost();
    if (ret != 0) {
        // turbo patch not present, we are not on custom Kernel, switch to simplified Mocs selection
        // do this only for GEN9+
        if (device->pHwInfo->platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {
            drmObject->setSimplifiedMocsTableUsage(true);
        }
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Failed to request OCL Turbo Boost\n");
    }

    drmObject->queryEngineInfo();
    if (HwHelper::get(device->pHwInfo->platform.eRenderCoreFamily).getEnableLocalMemory(*device->pHwInfo)) {
        drmObject->queryMemoryInfo();
        drmObject->setMemoryRegions();
    }

    drms[deviceOrdinal % drms.size()] = drmObject.release();
    return drms[deviceOrdinal % drms.size()];
}
} // namespace NEO
