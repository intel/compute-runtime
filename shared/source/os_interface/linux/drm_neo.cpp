/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "drm_neo.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/linux/drm_gem_close_worker.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/hw_device_id.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/linux/system_info.h"
#include "shared/source/os_interface/os_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/directory.h"

#include "drm_query_flags.h"

#include <cstdio>
#include <cstring>
#include <linux/limits.h>

namespace NEO {

namespace IoctlHelper {
std::string getIoctlParamString(int param) {
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
    case I915_PARAM_CS_TIMESTAMP_FREQUENCY:
        return "I915_PARAM_CS_TIMESTAMP_FREQUENCY";
    default:
        return getIoctlParamStringRemaining(param);
    }
}

std::string getIoctlString(unsigned long request) {
    switch (request) {
    case DRM_IOCTL_I915_GEM_EXECBUFFER2:
        return "DRM_IOCTL_I915_GEM_EXECBUFFER2";
    case DRM_IOCTL_I915_GEM_WAIT:
        return "DRM_IOCTL_I915_GEM_WAIT";
    case DRM_IOCTL_GEM_CLOSE:
        return "DRM_IOCTL_GEM_CLOSE";
    case DRM_IOCTL_I915_GEM_USERPTR:
        return "DRM_IOCTL_I915_GEM_USERPTR";
    case DRM_IOCTL_I915_INIT:
        return "DRM_IOCTL_I915_INIT";
    case DRM_IOCTL_I915_FLUSH:
        return "DRM_IOCTL_I915_FLUSH";
    case DRM_IOCTL_I915_FLIP:
        return "DRM_IOCTL_I915_FLIP";
    case DRM_IOCTL_I915_BATCHBUFFER:
        return "DRM_IOCTL_I915_BATCHBUFFER";
    case DRM_IOCTL_I915_IRQ_EMIT:
        return "DRM_IOCTL_I915_IRQ_EMIT";
    case DRM_IOCTL_I915_IRQ_WAIT:
        return "DRM_IOCTL_I915_IRQ_WAIT";
    case DRM_IOCTL_I915_GETPARAM:
        return "DRM_IOCTL_I915_GETPARAM";
    case DRM_IOCTL_I915_SETPARAM:
        return "DRM_IOCTL_I915_SETPARAM";
    case DRM_IOCTL_I915_ALLOC:
        return "DRM_IOCTL_I915_ALLOC";
    case DRM_IOCTL_I915_FREE:
        return "DRM_IOCTL_I915_FREE";
    case DRM_IOCTL_I915_INIT_HEAP:
        return "DRM_IOCTL_I915_INIT_HEAP";
    case DRM_IOCTL_I915_CMDBUFFER:
        return "DRM_IOCTL_I915_CMDBUFFER";
    case DRM_IOCTL_I915_DESTROY_HEAP:
        return "DRM_IOCTL_I915_DESTROY_HEAP";
    case DRM_IOCTL_I915_SET_VBLANK_PIPE:
        return "DRM_IOCTL_I915_SET_VBLANK_PIPE";
    case DRM_IOCTL_I915_GET_VBLANK_PIPE:
        return "DRM_IOCTL_I915_GET_VBLANK_PIPE";
    case DRM_IOCTL_I915_VBLANK_SWAP:
        return "DRM_IOCTL_I915_VBLANK_SWAP";
    case DRM_IOCTL_I915_HWS_ADDR:
        return "DRM_IOCTL_I915_HWS_ADDR";
    case DRM_IOCTL_I915_GEM_INIT:
        return "DRM_IOCTL_I915_GEM_INIT";
    case DRM_IOCTL_I915_GEM_EXECBUFFER:
        return "DRM_IOCTL_I915_GEM_EXECBUFFER";
    case DRM_IOCTL_I915_GEM_EXECBUFFER2_WR:
        return "DRM_IOCTL_I915_GEM_EXECBUFFER2_WR";
    case DRM_IOCTL_I915_GEM_PIN:
        return "DRM_IOCTL_I915_GEM_PIN";
    case DRM_IOCTL_I915_GEM_UNPIN:
        return "DRM_IOCTL_I915_GEM_UNPIN";
    case DRM_IOCTL_I915_GEM_BUSY:
        return "DRM_IOCTL_I915_GEM_BUSY";
    case DRM_IOCTL_I915_GEM_SET_CACHING:
        return "DRM_IOCTL_I915_GEM_SET_CACHING";
    case DRM_IOCTL_I915_GEM_GET_CACHING:
        return "DRM_IOCTL_I915_GEM_GET_CACHING";
    case DRM_IOCTL_I915_GEM_THROTTLE:
        return "DRM_IOCTL_I915_GEM_THROTTLE";
    case DRM_IOCTL_I915_GEM_ENTERVT:
        return "DRM_IOCTL_I915_GEM_ENTERVT";
    case DRM_IOCTL_I915_GEM_LEAVEVT:
        return "DRM_IOCTL_I915_GEM_LEAVEVT";
    case DRM_IOCTL_I915_GEM_CREATE:
        return "DRM_IOCTL_I915_GEM_CREATE";
    case DRM_IOCTL_I915_GEM_PREAD:
        return "DRM_IOCTL_I915_GEM_PREAD";
    case DRM_IOCTL_I915_GEM_PWRITE:
        return "DRM_IOCTL_I915_GEM_PWRITE";
    case DRM_IOCTL_I915_GEM_SET_DOMAIN:
        return "DRM_IOCTL_I915_GEM_SET_DOMAIN";
    case DRM_IOCTL_I915_GEM_SW_FINISH:
        return "DRM_IOCTL_I915_GEM_SW_FINISH";
    case DRM_IOCTL_I915_GEM_SET_TILING:
        return "DRM_IOCTL_I915_GEM_SET_TILING";
    case DRM_IOCTL_I915_GEM_GET_TILING:
        return "DRM_IOCTL_I915_GEM_GET_TILING";
    case DRM_IOCTL_I915_GEM_GET_APERTURE:
        return "DRM_IOCTL_I915_GEM_GET_APERTURE";
    case DRM_IOCTL_I915_GET_PIPE_FROM_CRTC_ID:
        return "DRM_IOCTL_I915_GET_PIPE_FROM_CRTC_ID";
    case DRM_IOCTL_I915_GEM_MADVISE:
        return "DRM_IOCTL_I915_GEM_MADVISE";
    case DRM_IOCTL_I915_OVERLAY_PUT_IMAGE:
        return "DRM_IOCTL_I915_OVERLAY_PUT_IMAGE";
    case DRM_IOCTL_I915_OVERLAY_ATTRS:
        return "DRM_IOCTL_I915_OVERLAY_ATTRS";
    case DRM_IOCTL_I915_SET_SPRITE_COLORKEY:
        return "DRM_IOCTL_I915_SET_SPRITE_COLORKEY";
    case DRM_IOCTL_I915_GET_SPRITE_COLORKEY:
        return "DRM_IOCTL_I915_GET_SPRITE_COLORKEY";
    case DRM_IOCTL_I915_GEM_CONTEXT_CREATE:
        return "DRM_IOCTL_I915_GEM_CONTEXT_CREATE";
    case DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT:
        return "DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT";
    case DRM_IOCTL_I915_GEM_CONTEXT_DESTROY:
        return "DRM_IOCTL_I915_GEM_CONTEXT_DESTROY";
    case DRM_IOCTL_I915_REG_READ:
        return "DRM_IOCTL_I915_REG_READ";
    case DRM_IOCTL_I915_GET_RESET_STATS:
        return "DRM_IOCTL_I915_GET_RESET_STATS";
    case DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM:
        return "DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM";
    case DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM:
        return "DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM";
    case DRM_IOCTL_I915_PERF_OPEN:
        return "DRM_IOCTL_I915_PERF_OPEN";
    case DRM_IOCTL_I915_PERF_ADD_CONFIG:
        return "DRM_IOCTL_I915_PERF_ADD_CONFIG";
    case DRM_IOCTL_I915_PERF_REMOVE_CONFIG:
        return "DRM_IOCTL_I915_PERF_REMOVE_CONFIG";
    case DRM_IOCTL_I915_QUERY:
        return "DRM_IOCTL_I915_QUERY";
    case DRM_IOCTL_I915_GEM_MMAP:
        return "DRM_IOCTL_I915_GEM_MMAP";
    case DRM_IOCTL_PRIME_FD_TO_HANDLE:
        return "DRM_IOCTL_PRIME_FD_TO_HANDLE";
    case DRM_IOCTL_PRIME_HANDLE_TO_FD:
        return "DRM_IOCTL_PRIME_HANDLE_TO_FD";
    default:
        return getIoctlStringRemaining(request);
    }
}

} // namespace IoctlHelper

Drm::Drm(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceIdIn, RootDeviceEnvironment &rootDeviceEnvironment)
    : DriverModel(DriverModelType::DRM),
      hwDeviceId(std::move(hwDeviceIdIn)), rootDeviceEnvironment(rootDeviceEnvironment) {
    pagingFence.fill(0u);
    fenceVal.fill(0u);
}

int Drm::ioctl(unsigned long request, void *arg) {
    int ret;
    int returnedErrno;
    SYSTEM_ENTER();
    do {
        auto measureTime = DebugManager.flags.PrintIoctlTimes.get();
        std::chrono::steady_clock::time_point start;
        std::chrono::steady_clock::time_point end;

        auto printIoctl = DebugManager.flags.PrintIoctlEntries.get();

        if (printIoctl) {
            printf("IOCTL %s called\n", IoctlHelper::getIoctlString(request).c_str());
        }

        if (measureTime) {
            start = std::chrono::steady_clock::now();
        }
        ret = SysCalls::ioctl(getFileDescriptor(), request, arg);

        returnedErrno = errno;

        if (measureTime) {
            end = std::chrono::steady_clock::now();
            long long elapsedTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

            IoctlStatisticsEntry ioctlData{};
            auto ioctlDataIt = this->ioctlStatistics.find(request);
            if (ioctlDataIt != this->ioctlStatistics.end()) {
                ioctlData = ioctlDataIt->second;
            }

            ioctlData.totalTime += elapsedTime;
            ioctlData.count++;
            ioctlData.minTime = std::min(ioctlData.minTime, elapsedTime);
            ioctlData.maxTime = std::max(ioctlData.maxTime, elapsedTime);

            this->ioctlStatistics[request] = ioctlData;
        }

        if (printIoctl) {
            printf("IOCTL %s returns %d, errno %d(%s)\n", IoctlHelper::getIoctlString(request).c_str(), ret, returnedErrno, strerror(returnedErrno));
        }

    } while (ret == -1 && (returnedErrno == EINTR || returnedErrno == EAGAIN || returnedErrno == EBUSY));
    SYSTEM_LEAVE(request);
    return ret;
}

int Drm::getParamIoctl(int param, int *dstValue) {
    drm_i915_getparam_t getParam = {};
    getParam.param = param;
    getParam.value = dstValue;

    int retVal = ioctl(DRM_IOCTL_I915_GETPARAM, &getParam);
    if (DebugManager.flags.PrintIoctlEntries.get()) {
        printf("DRM_IOCTL_I915_GETPARAM: param: %s, output value: %d, retCode:% d\n",
               IoctlHelper::getIoctlParamString(param).c_str(),
               *getParam.value,
               retVal);
    }
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

void Drm::setUnrecoverableContext(uint32_t drmContextId) {
    drm_i915_gem_context_param contextParam = {};
    contextParam.ctx_id = drmContextId;
    contextParam.param = I915_CONTEXT_PARAM_RECOVERABLE;
    contextParam.value = 0;
    contextParam.size = 0;

    ioctl(DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, &contextParam);
}

uint32_t Drm::createDrmContext(uint32_t drmVmId, bool isSpecialContextRequested) {
    drm_i915_gem_context_create_ext gcc = {};

    this->appendDrmContextFlags(gcc, isSpecialContextRequested);

    auto retVal = this->createDrmContextExt(gcc, drmVmId, isSpecialContextRequested);
    UNRECOVERABLE_IF(retVal != 0);

    if (drmVmId > 0) {
        drm_i915_gem_context_param param{};
        param.ctx_id = gcc.ctx_id;
        param.value = drmVmId;
        param.param = I915_CONTEXT_PARAM_VM;
        retVal = ioctl(DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, &param);
        UNRECOVERABLE_IF(retVal != 0);
    }

    return gcc.ctx_id;
}

void Drm::destroyDrmContext(uint32_t drmContextId) {
    drm_i915_gem_context_destroy destroy = {};
    destroy.ctx_id = drmContextId;
    auto retVal = ioctl(DRM_IOCTL_I915_GEM_CONTEXT_DESTROY, &destroy);
    UNRECOVERABLE_IF(retVal != 0);
}

void Drm::destroyDrmVirtualMemory(uint32_t drmVmId) {
    drm_i915_gem_vm_control ctl = {};
    ctl.vm_id = drmVmId;
    auto ret = SysCalls::ioctl(getFileDescriptor(), DRM_IOCTL_I915_GEM_VM_DESTROY, &ctl);
    UNRECOVERABLE_IF(ret != 0);
}

int Drm::queryVmId(uint32_t drmContextId, uint32_t &vmId) {
    drm_i915_gem_context_param param{};
    param.ctx_id = drmContextId;
    param.value = 0;
    param.param = I915_CONTEXT_PARAM_VM;
    auto retVal = this->ioctl(DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM, &param);

    vmId = static_cast<uint32_t>(param.value);

    return retVal;
}

std::unique_lock<std::mutex> Drm::lockBindFenceMutex() {
    return std::unique_lock<std::mutex>(this->bindFenceMutex);
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

    Drm::QueryTopologyData topologyData = {};

    bool status = queryTopology(*hwInfo, topologyData);

    if (!status) {
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Topology query failed!\n");

        ret = getEuTotal(topologyData.euCount);
        if (ret != 0) {
            PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query EU total parameter!\n");
            return ret;
        }

        ret = getSubsliceTotal(topologyData.subSliceCount);
        if (ret != 0) {
            PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query subslice total parameter!\n");
            return ret;
        }
    }

    hwInfo->gtSystemInfo.SliceCount = static_cast<uint32_t>(topologyData.sliceCount);
    hwInfo->gtSystemInfo.SubSliceCount = static_cast<uint32_t>(topologyData.subSliceCount);
    hwInfo->gtSystemInfo.DualSubSliceCount = static_cast<uint32_t>(topologyData.subSliceCount);
    hwInfo->gtSystemInfo.EUCount = static_cast<uint32_t>(topologyData.euCount);

    status = querySystemInfo();
    if (status) {
        setupSystemInfo(hwInfo, systemInfo.get());
    }
    device->setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);

    if (systemInfo) {
        systemInfo->checkSysInfoMismatch(hwInfo);
    }

    setupCacheInfo(*hwInfo);

    return 0;
}

void appendHwDeviceId(std::vector<std::unique_ptr<HwDeviceId>> &hwDeviceIds, int fileDescriptor, const char *pciPath) {
    if (fileDescriptor >= 0) {
        if (Drm::isi915Version(fileDescriptor)) {
            hwDeviceIds.push_back(std::make_unique<HwDeviceIdDrm>(fileDescriptor, pciPath));
        } else {
            SysCalls::close(fileDescriptor);
        }
    }
}

std::vector<std::unique_ptr<HwDeviceId>> Drm::discoverDevices(ExecutionEnvironment &executionEnvironment) {
    std::vector<std::unique_ptr<HwDeviceId>> hwDeviceIds;
    executionEnvironment.osEnvironment = std::make_unique<OsEnvironment>();
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

            auto pciPath = NEO::getPciPath(fileDescriptor);

            appendHwDeviceId(hwDeviceIds, fileDescriptor, pciPath.value_or("0000:00:02.0").c_str());
            if (!hwDeviceIds.empty() && hwDeviceIds.size() == numRootDevices) {
                break;
            }
        }
        return hwDeviceIds;
    }

    do {
        const char *renderDeviceSuffix = "-render";
        for (std::vector<std::string>::iterator file = files.begin(); file != files.end(); ++file) {
            std::string_view devicePathView(file->c_str(), file->size());
            devicePathView = devicePathView.substr(strlen(Os::pciDevicesDirectory));

            auto rdsPos = devicePathView.rfind(renderDeviceSuffix);
            if (rdsPos == std::string::npos) {
                continue;
            }
            if (rdsPos < devicePathView.size() - strlen(renderDeviceSuffix)) {
                continue;
            }
            // at least 'pci-0000:00:00.0' -> 16
            if (rdsPos < 16 || devicePathView[rdsPos - 13] != '-') {
                continue;
            }
            std::string pciPath(devicePathView.substr(rdsPos - 12, 12));

            if (DebugManager.flags.ForceDeviceId.get() != "unk") {
                if (devicePathView.find(DebugManager.flags.ForceDeviceId.get().c_str()) == std::string::npos) {
                    continue;
                }
            }
            int fileDescriptor = SysCalls::open(file->c_str(), O_RDWR);
            appendHwDeviceId(hwDeviceIds, fileDescriptor, pciPath.c_str());
            if (!hwDeviceIds.empty() && hwDeviceIds.size() == numRootDevices) {
                break;
            }
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

std::unique_ptr<uint8_t[]> Drm::query(uint32_t queryId, uint32_t queryItemFlags, int32_t &length) {
    drm_i915_query query{};
    drm_i915_query_item queryItem{};
    queryItem.query_id = queryId;
    queryItem.length = 0; // query length first
    queryItem.flags = queryItemFlags;
    query.items_ptr = reinterpret_cast<__u64>(&queryItem);
    query.num_items = 1;
    length = 0;

    auto ret = this->ioctl(DRM_IOCTL_I915_QUERY, &query);
    if (ret != 0 || queryItem.length <= 0) {
        return nullptr;
    }

    auto data = std::make_unique<uint8_t[]>(queryItem.length);
    memset(data.get(), 0, queryItem.length);
    queryItem.data_ptr = castToUint64(data.get());

    ret = this->ioctl(DRM_IOCTL_I915_QUERY, &query);
    if (ret != 0 || queryItem.length <= 0) {
        return nullptr;
    }

    length = queryItem.length;
    return data;
}

void Drm::printIoctlStatistics() {
    if (!DebugManager.flags.PrintIoctlTimes.get()) {
        return;
    }

    printf("\n--- Ioctls statistics ---\n");
    printf("%41s %15s %10s %20s %20s %20s", "Request", "Total time(ns)", "Count", "Avg time per ioctl", "Min", "Max\n");
    for (const auto &ioctlData : this->ioctlStatistics) {
        printf("%41s %15llu %10lu %20f %20lld %20lld\n",
               IoctlHelper::getIoctlString(ioctlData.first).c_str(),
               ioctlData.second.totalTime,
               static_cast<unsigned long>(ioctlData.second.count),
               ioctlData.second.totalTime / static_cast<double>(ioctlData.second.count),
               ioctlData.second.minTime,
               ioctlData.second.maxTime);
    }
    printf("\n");
}

bool Drm::createVirtualMemoryAddressSpace(uint32_t vmCount) {
    for (auto i = 0u; i < vmCount; i++) {
        uint32_t id = i;
        if (0 != createDrmVirtualMemory(id)) {
            return false;
        }
        virtualMemoryIds.push_back(id);
    }
    return true;
}

void Drm::destroyVirtualMemoryAddressSpace() {
    for (auto id : virtualMemoryIds) {
        destroyDrmVirtualMemory(id);
    }
    virtualMemoryIds.clear();
}

uint32_t Drm::getVirtualMemoryAddressSpace(uint32_t vmId) {
    if (vmId < virtualMemoryIds.size()) {
        return virtualMemoryIds[vmId];
    }
    return 0;
}

void Drm::setNewResourceBoundToVM(uint32_t vmHandleId) {
    const auto &engines = this->rootDeviceEnvironment.executionEnvironment.memoryManager->getRegisteredEngines();
    for (const auto &engine : engines) {
        if (engine.osContext->getDeviceBitfield().test(vmHandleId)) {
            auto osContextLinux = static_cast<OsContextLinux *>(engine.osContext);

            if (&osContextLinux->getDrm() == this) {
                osContextLinux->setNewResourceBound(true);
            }
        }
    }
}

bool Drm::translateTopologyInfo(const drm_i915_query_topology_info *queryTopologyInfo, QueryTopologyData &data, TopologyMapping &mapping) {
    int sliceCount = 0;
    int subSliceCount = 0;
    int euCount = 0;
    int maxSliceCount = 0;
    int maxSubSliceCountPerSlice = 0;
    std::vector<int> sliceIndices;
    sliceIndices.reserve(maxSliceCount);

    for (int x = 0; x < queryTopologyInfo->max_slices; x++) {
        bool isSliceEnable = (queryTopologyInfo->data[x / 8] >> (x % 8)) & 1;
        if (!isSliceEnable) {
            continue;
        }
        sliceIndices.push_back(x);
        sliceCount++;

        std::vector<int> subSliceIndices;
        subSliceIndices.reserve(queryTopologyInfo->max_subslices);

        for (int y = 0; y < queryTopologyInfo->max_subslices; y++) {
            size_t yOffset = (queryTopologyInfo->subslice_offset + x * queryTopologyInfo->subslice_stride + y / 8);
            bool isSubSliceEnabled = (queryTopologyInfo->data[yOffset] >> (y % 8)) & 1;
            if (!isSubSliceEnabled) {
                continue;
            }
            subSliceCount++;
            subSliceIndices.push_back(y);

            for (int z = 0; z < queryTopologyInfo->max_eus_per_subslice; z++) {
                size_t zOffset = (queryTopologyInfo->eu_offset + (x * queryTopologyInfo->max_subslices + y) * queryTopologyInfo->eu_stride + z / 8);
                bool isEUEnabled = (queryTopologyInfo->data[zOffset] >> (z % 8)) & 1;
                if (!isEUEnabled) {
                    continue;
                }
                euCount++;
            }
        }

        if (subSliceIndices.size()) {
            maxSubSliceCountPerSlice = std::max(maxSubSliceCountPerSlice, subSliceIndices[subSliceIndices.size() - 1] + 1);
        }
    }

    if (sliceIndices.size()) {
        maxSliceCount = sliceIndices[sliceIndices.size() - 1] + 1;
        mapping.sliceIndices = std::move(sliceIndices);
    }

    data.sliceCount = sliceCount;
    data.subSliceCount = subSliceCount;
    data.euCount = euCount;
    data.maxSliceCount = maxSliceCount;
    data.maxSubSliceCount = maxSubSliceCountPerSlice;

    return (data.sliceCount && data.subSliceCount && data.euCount);
}

PhysicalDevicePciBusInfo Drm::getPciBusInfo() const {
    PhysicalDevicePciBusInfo pciBusInfo(PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue);

    if (adapterBDF.Data != std::numeric_limits<uint32_t>::max()) {
        pciBusInfo.pciDomain = this->pciDomain;
        pciBusInfo.pciBus = adapterBDF.Bus;
        pciBusInfo.pciDevice = adapterBDF.Device;
        pciBusInfo.pciFunction = adapterBDF.Function;
    }
    return pciBusInfo;
}

Drm::~Drm() {
    destroyVirtualMemoryAddressSpace();
    this->printIoctlStatistics();
}

int Drm::queryAdapterBDF() {
    constexpr int pciBusInfoTokensNum = 4;
    uint32_t domain, bus, device, function;

    if (std::sscanf(hwDeviceId->getPciPath(), "%04x:%02x:%02x.%01x",
                    &domain, &bus, &device, &function) != pciBusInfoTokensNum) {
        adapterBDF.Data = std::numeric_limits<uint32_t>::max();
        return 1;
    }
    setPciDomain(domain);
    adapterBDF.Bus = bus;
    adapterBDF.Function = function;
    adapterBDF.Device = device;
    return 0;
}

void Drm::setGmmInputArgs(void *args) {
    auto gmmInArgs = reinterpret_cast<GMM_INIT_IN_ARGS *>(args);
    auto adapterBDF = this->getAdapterBDF();
#if defined(__linux__)
    gmmInArgs->FileDescriptor = adapterBDF.Data;
#endif
    gmmInArgs->ClientType = GMM_CLIENT::GMM_OCL_VISTA;
}

const std::vector<int> &Drm::getSliceMappings(uint32_t deviceIndex) {
    return topologyMap[deviceIndex].sliceIndices;
}

const TopologyMap &Drm::getTopologyMap() {
    return topologyMap;
}

int Drm::waitHandle(uint32_t waitHandle, int64_t timeout) {
    UNRECOVERABLE_IF(isVmBindAvailable());

    drm_i915_gem_wait wait = {};
    wait.bo_handle = waitHandle;
    wait.timeout_ns = timeout;

    int ret = ioctl(DRM_IOCTL_I915_GEM_WAIT, &wait);
    if (ret != 0) {
        int err = errno;
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(I915_GEM_WAIT) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
    }

    return ret;
}

int Drm::getTimestampFrequency(int &frequency) {
    frequency = 0;
    return getParamIoctl(I915_PARAM_CS_TIMESTAMP_FREQUENCY, &frequency);
}

bool Drm::queryEngineInfo() {
    return Drm::queryEngineInfo(false);
}

bool Drm::sysmanQueryEngineInfo() {
    return Drm::queryEngineInfo(true);
}

int getMaxGpuFrequencyOfDevice(Drm &drm, std::string &sysFsPciPath, int &maxGpuFrequency) {
    maxGpuFrequency = 0;
    std::string clockSysFsPath = sysFsPciPath + "/gt_max_freq_mhz";

    std::ifstream ifs(clockSysFsPath.c_str(), std::ifstream::in);
    if (ifs.fail()) {
        return -1;
    }

    ifs >> maxGpuFrequency;
    ifs.close();
    return 0;
}

int getMaxGpuFrequencyOfSubDevice(Drm &drm, std::string &sysFsPciPath, int subDeviceId, int &maxGpuFrequency) {
    maxGpuFrequency = 0;
    std::string clockSysFsPath = sysFsPciPath + "/gt/gt" + std::to_string(subDeviceId) + "/rps_max_freq_mhz";

    std::ifstream ifs(clockSysFsPath.c_str(), std::ifstream::in);
    if (ifs.fail()) {
        return -1;
    }

    ifs >> maxGpuFrequency;
    ifs.close();
    return 0;
}

int Drm::getMaxGpuFrequency(HardwareInfo &hwInfo, int &maxGpuFrequency) {
    int ret = 0;
    std::string sysFsPciPath = getSysFsPciPath();
    auto tileCount = hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount;

    if (hwInfo.gtSystemInfo.MultiTileArchInfo.IsValid && tileCount > 0) {
        for (auto tileId = 0; tileId < tileCount; tileId++) {
            int maxGpuFreqOfSubDevice = 0;
            ret |= getMaxGpuFrequencyOfSubDevice(*this, sysFsPciPath, tileId, maxGpuFreqOfSubDevice);
            maxGpuFrequency = std::max(maxGpuFrequency, maxGpuFreqOfSubDevice);
        }
        if (ret == 0) {
            return 0;
        }
    }
    return getMaxGpuFrequencyOfDevice(*this, sysFsPciPath, maxGpuFrequency);
}

} // namespace NEO
