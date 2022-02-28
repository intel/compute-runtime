/*
 * Copyright (C) 2018-2022 Intel Corporation
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
#include "shared/source/os_interface/linux/cache_info_impl.h"
#include "shared/source/os_interface/linux/clos_helper.h"
#include "shared/source/os_interface/linux/drm_engine_mapper.h"
#include "shared/source/os_interface/linux/drm_gem_close_worker.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/linux/hw_device_id.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/ioctl_strings.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/linux/system_info.h"
#include "shared/source/os_interface/os_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/directory.h"

#include <cstdio>
#include <cstring>
#include <linux/limits.h>
#include <map>

namespace NEO {

namespace IoctlToStringHelper {
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

} // namespace IoctlToStringHelper

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
            printf("IOCTL %s called\n", IoctlToStringHelper::getIoctlString(request).c_str());
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
            if (ret == 0) {
                printf("IOCTL %s returns %d\n",
                       IoctlToStringHelper::getIoctlString(request).c_str(), ret);
            } else {
                printf("IOCTL %s returns %d, errno %d(%s)\n",
                       IoctlToStringHelper::getIoctlString(request).c_str(), ret, returnedErrno, strerror(returnedErrno));
            }
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
               IoctlToStringHelper::getIoctlParamString(param).c_str(),
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

bool Drm::isGpuHangDetected(OsContext &osContext) {
    const auto osContextLinux = static_cast<OsContextLinux *>(&osContext);
    const auto &drmContextIds = osContextLinux->getDrmContextIds();

    for (const auto drmContextId : drmContextIds) {
        drm_i915_reset_stats reset_stats{};
        reset_stats.ctx_id = drmContextId;

        const auto retVal{ioctl(DRM_IOCTL_I915_GET_RESET_STATS, &reset_stats)};
        UNRECOVERABLE_IF(retVal != 0);

        if (reset_stats.batch_active > 0 || reset_stats.batch_pending > 0) {
            PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "ERROR: GPU HANG detected!\n");
            return true;
        }
    }

    return false;
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

uint32_t Drm::createDrmContext(uint32_t drmVmId, bool isDirectSubmissionRequested, bool isCooperativeContextRequested) {
    drm_i915_gem_context_create_ext gcc = {};

    if (DebugManager.flags.DirectSubmissionDrmContext.get() != -1) {
        isDirectSubmissionRequested = DebugManager.flags.DirectSubmissionDrmContext.get();
    }
    if (isDirectSubmissionRequested) {
        gcc.flags |= ioctlHelper->getDirectSubmissionFlag();
    }

    drm_i915_gem_context_create_ext_setparam extSetparam = {};

    if (drmVmId > 0) {
        extSetparam.base.name = I915_CONTEXT_CREATE_EXT_SETPARAM;
        extSetparam.param.param = I915_CONTEXT_PARAM_VM;
        extSetparam.param.value = drmVmId;
        gcc.extensions = reinterpret_cast<uint64_t>(&extSetparam);
        gcc.flags |= I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS;
    }

    if (DebugManager.flags.CreateContextWithAccessCounters.get() != -1) {
        return ioctlHelper->createContextWithAccessCounters(this, gcc);
    }

    if (DebugManager.flags.ForceRunAloneContext.get() != -1) {
        isCooperativeContextRequested = DebugManager.flags.ForceRunAloneContext.get();
    }
    if (isCooperativeContextRequested) {
        return ioctlHelper->createCooperativeContext(this, gcc);
    }
    auto ioctlResult = ioctl(DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT, &gcc);

    UNRECOVERABLE_IF(ioctlResult != 0);
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

    const auto productFamily = hwInfo->platform.eProductFamily;
    setupIoctlHelper(productFamily);

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
    rootDeviceEnvironment.setHwInfo(hwInfo);

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
    std::string str = "";
    return Drm::discoverDevices(executionEnvironment, str);
}

std::vector<std::unique_ptr<HwDeviceId>> Drm::discoverDevice(ExecutionEnvironment &executionEnvironment, std::string &osPciPath) {
    return Drm::discoverDevices(executionEnvironment, osPciPath);
}

std::vector<std::unique_ptr<HwDeviceId>> Drm::discoverDevices(ExecutionEnvironment &executionEnvironment, std::string &osPciPath) {
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

            if (!osPciPath.empty()) {
                if (osPciPath.compare(pciPath) != 0) {
                    // if osPciPath is non-empty, then interest is only in discovering device having same bdf as ocPciPath. Skip all other devices.
                    continue;
                }
            }

            if (DebugManager.flags.FilterBdfPath.get() != "unk") {
                if (devicePathView.find(DebugManager.flags.FilterBdfPath.get().c_str()) == std::string::npos) {
                    continue;
                }
            }
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

std::vector<uint8_t> Drm::query(uint32_t queryId, uint32_t queryItemFlags) {
    drm_i915_query query{};
    drm_i915_query_item queryItem{};
    queryItem.query_id = queryId;
    queryItem.length = 0; // query length first
    queryItem.flags = queryItemFlags;
    query.items_ptr = reinterpret_cast<__u64>(&queryItem);
    query.num_items = 1;

    auto ret = this->ioctl(DRM_IOCTL_I915_QUERY, &query);
    if (ret != 0 || queryItem.length <= 0) {
        return {};
    }

    auto data = std::vector<uint8_t>(queryItem.length, 0);
    queryItem.data_ptr = castToUint64(data.data());

    ret = this->ioctl(DRM_IOCTL_I915_QUERY, &query);
    if (ret != 0 || queryItem.length <= 0) {
        return {};
    }
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
               IoctlToStringHelper::getIoctlString(ioctlData.first).c_str(),
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

        // single slice available
        if (sliceCount == 1) {
            mapping.subsliceIndices = std::move(subSliceIndices);
        }
    }

    if (sliceIndices.size()) {
        maxSliceCount = sliceIndices[sliceIndices.size() - 1] + 1;
        mapping.sliceIndices = std::move(sliceIndices);
    }

    if (sliceCount != 1) {
        mapping.subsliceIndices.clear();
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
    uint16_t domain = -1;
    uint8_t bus = -1, device = -1, function = -1;

    if (NEO::parseBdfString(hwDeviceId->getPciPath(), domain, bus, device, function) != pciBusInfoTokensNum) {
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

bool Drm::useVMBindImmediate() const {
    bool useBindImmediate = isDirectSubmissionActive() || hasPageFaultSupport();

    if (DebugManager.flags.EnableImmediateVmBindExt.get() != -1) {
        useBindImmediate = DebugManager.flags.EnableImmediateVmBindExt.get();
    }

    return useBindImmediate;
}

void Drm::setupSystemInfo(HardwareInfo *hwInfo, SystemInfo *sysInfo) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * sysInfo->getNumThreadsPerEu();
    gtSysInfo->L3CacheSizeInKb = sysInfo->getL3CacheSizeInKb();
    gtSysInfo->L3BankCount = sysInfo->getL3BankCount();
    gtSysInfo->MemoryType = sysInfo->getMemoryType();
    gtSysInfo->MaxFillRate = sysInfo->getMaxFillRate();
    gtSysInfo->TotalVsThreads = sysInfo->getTotalVsThreads();
    gtSysInfo->TotalHsThreads = sysInfo->getTotalHsThreads();
    gtSysInfo->TotalDsThreads = sysInfo->getTotalDsThreads();
    gtSysInfo->TotalGsThreads = sysInfo->getTotalGsThreads();
    gtSysInfo->TotalPsThreadsWindowerRange = sysInfo->getTotalPsThreads();
    gtSysInfo->MaxEuPerSubSlice = sysInfo->getMaxEuPerDualSubSlice();
    gtSysInfo->MaxSlicesSupported = sysInfo->getMaxSlicesSupported();
    gtSysInfo->MaxSubSlicesSupported = sysInfo->getMaxDualSubSlicesSupported();
    gtSysInfo->MaxDualSubSlicesSupported = sysInfo->getMaxDualSubSlicesSupported();
}

void Drm::setupCacheInfo(const HardwareInfo &hwInfo) {
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    if (DebugManager.flags.ClosEnabled.get() == 0 || hwHelper.getNumCacheRegions() == 0) {
        this->cacheInfo.reset(new CacheInfoImpl(*this, 0, 0, 0));
        return;
    }

    const GT_SYSTEM_INFO *gtSysInfo = &hwInfo.gtSystemInfo;

    constexpr uint16_t maxNumWays = 32;
    constexpr uint16_t globalReservationLimit = 16;
    constexpr uint16_t clientReservationLimit = 8;
    constexpr uint16_t maxReservationNumWays = std::min(globalReservationLimit, clientReservationLimit);
    const size_t totalCacheSize = gtSysInfo->L3CacheSizeInKb * MemoryConstants::kiloByte;
    const size_t maxReservationCacheSize = (totalCacheSize * maxReservationNumWays) / maxNumWays;
    const uint32_t maxReservationNumCacheRegions = hwHelper.getNumCacheRegions() - 1;

    this->cacheInfo.reset(new CacheInfoImpl(*this, maxReservationCacheSize, maxReservationNumCacheRegions, maxReservationNumWays));
}

void Drm::getPrelimVersion(std::string &prelimVersion) {
    std::string sysFsPciPath = getSysFsPciPath();
    std::string prelimVersionPath = sysFsPciPath + "/prelim_uapi_version";

    std::ifstream ifs(prelimVersionPath.c_str(), std::ifstream::in);

    if (ifs.fail()) {
        prelimVersion = "";
    } else {
        ifs >> prelimVersion;
    }
    ifs.close();
}

int Drm::waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags) {
    return ioctlHelper->waitUserFence(this, ctxId, address, value, static_cast<uint32_t>(dataWidth), timeout, flags);
}

bool Drm::querySystemInfo() {
    auto request = ioctlHelper->getHwConfigIoctlVal();
    auto deviceBlobQuery = this->query(request, 0);
    if (deviceBlobQuery.empty()) {
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stdout, "%s", "INFO: System Info query failed!\n");
        return false;
    }
    this->systemInfo.reset(new SystemInfo(deviceBlobQuery));
    return true;
}

std::vector<uint8_t> Drm::getMemoryRegions() {
    return this->query(ioctlHelper->getMemRegionsIoctlVal(), 0);
}

bool Drm::queryMemoryInfo() {
    auto dataQuery = getMemoryRegions();
    if (!dataQuery.empty()) {
        auto memRegions = ioctlHelper->translateToMemoryRegions(dataQuery);
        this->memoryInfo.reset(new MemoryInfo(memRegions));
        return true;
    }
    return false;
}

bool Drm::queryEngineInfo(bool isSysmanEnabled) {
    auto enginesQuery = this->query(ioctlHelper->getEngineInfoIoctlVal(), 0);
    if (enginesQuery.empty()) {
        return false;
    }
    auto engines = ioctlHelper->translateToEngineCaps(enginesQuery);
    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();

    auto memInfo = memoryInfo.get();

    if (!memInfo) {
        this->engineInfo.reset(new EngineInfo(this, hwInfo, engines));
        return true;
    }

    auto &memoryRegions = memInfo->getDrmRegionInfos();

    auto tileCount = 0u;
    std::vector<DistanceInfo> distanceInfos;
    for (const auto &region : memoryRegions) {
        if (I915_MEMORY_CLASS_DEVICE == region.region.memoryClass) {
            tileCount++;
            DistanceInfo distanceInfo{};
            distanceInfo.region = region.region;

            for (const auto &engine : engines) {
                switch (engine.engine.engineClass) {
                case I915_ENGINE_CLASS_RENDER:
                case I915_ENGINE_CLASS_COPY:
                    distanceInfo.engine = engine.engine;
                    distanceInfos.push_back(distanceInfo);
                    break;
                case I915_ENGINE_CLASS_VIDEO:
                case I915_ENGINE_CLASS_VIDEO_ENHANCE:
                    if (isSysmanEnabled == true) {
                        distanceInfo.engine = engine.engine;
                        distanceInfos.push_back(distanceInfo);
                    }
                    break;
                default:
                    if (engine.engine.engineClass == ioctlHelper->getComputeEngineClass()) {
                        distanceInfo.engine = engine.engine;
                        distanceInfos.push_back(distanceInfo);
                    }
                    break;
                }
            }
        }
    }

    if (tileCount == 0u) {
        this->engineInfo.reset(new EngineInfo(this, hwInfo, engines));
        return true;
    }

    std::vector<drm_i915_query_item> queryItems{distanceInfos.size()};
    auto ret = ioctlHelper->queryDistances(this, queryItems, distanceInfos);
    if (ret != 0) {
        return false;
    }

    const bool queryUnsupported = std::all_of(queryItems.begin(), queryItems.end(),
                                              [](const drm_i915_query_item &item) { return item.length == -EINVAL; });
    if (queryUnsupported) {
        DEBUG_BREAK_IF(tileCount != 1);
        this->engineInfo.reset(new EngineInfo(this, hwInfo, engines));
        return true;
    }

    memInfo->assignRegionsFromDistances(distanceInfos);

    auto &multiTileArchInfo = const_cast<GT_MULTI_TILE_ARCH_INFO &>(hwInfo->gtSystemInfo.MultiTileArchInfo);
    multiTileArchInfo.IsValid = true;
    multiTileArchInfo.TileCount = tileCount;
    multiTileArchInfo.TileMask = static_cast<uint8_t>(maxNBitValue(tileCount));

    this->engineInfo.reset(new EngineInfo(this, hwInfo, tileCount, distanceInfos, queryItems, engines));
    return true;
}

bool Drm::completionFenceSupport() {
    std::call_once(checkCompletionFenceOnce, [this]() {
        const bool vmBindAvailable = isVmBindAvailable();
        bool support = ioctlHelper->completionFenceExtensionSupported(*getRootDeviceEnvironment().getHardwareInfo(), vmBindAvailable);
        int32_t overrideCompletionFence = DebugManager.flags.EnableDrmCompletionFence.get();
        if (overrideCompletionFence != -1) {
            support = !!overrideCompletionFence;
        }

        completionFenceSupported = support;
    });
    return completionFenceSupported;
}

void Drm::setupIoctlHelper(const PRODUCT_FAMILY productFamily) {
    std::string prelimVersion = "";
    getPrelimVersion(prelimVersion);
    this->ioctlHelper.reset(IoctlHelper::get(productFamily, prelimVersion));
}

bool Drm::queryTopology(const HardwareInfo &hwInfo, QueryTopologyData &topologyData) {
    topologyData.sliceCount = 0;
    topologyData.subSliceCount = 0;
    topologyData.euCount = 0;

    int sliceCount = 0;
    int subSliceCount = 0;
    int euCount = 0;

    const auto queryComputeSlicesIoctl = ioctlHelper->getComputeSlicesIoctlVal();
    if (DebugManager.flags.UseNewQueryTopoIoctl.get() && this->engineInfo && hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount > 0 && queryComputeSlicesIoctl != 0) {
        bool success = true;

        for (uint32_t i = 0; i < hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount; i++) {
            auto classInstance = this->engineInfo->getEngineInstance(i, hwInfo.capabilityTable.defaultEngineType);
            UNRECOVERABLE_IF(!classInstance);

            uint32_t flags = classInstance->engineClass;
            flags |= (classInstance->engineInstance << 8);

            auto dataQuery = this->query(queryComputeSlicesIoctl, flags);
            if (dataQuery.empty()) {
                success = false;
                break;
            }
            auto data = reinterpret_cast<drm_i915_query_topology_info *>(dataQuery.data());

            QueryTopologyData tileTopologyData = {};
            TopologyMapping mapping;
            if (!translateTopologyInfo(data, tileTopologyData, mapping)) {
                success = false;
                break;
            }

            // pick smallest config
            sliceCount = (sliceCount == 0) ? tileTopologyData.sliceCount : std::min(sliceCount, tileTopologyData.sliceCount);
            subSliceCount = (subSliceCount == 0) ? tileTopologyData.subSliceCount : std::min(subSliceCount, tileTopologyData.subSliceCount);
            euCount = (euCount == 0) ? tileTopologyData.euCount : std::min(euCount, tileTopologyData.euCount);

            topologyData.maxSliceCount = std::max(topologyData.maxSliceCount, tileTopologyData.maxSliceCount);
            topologyData.maxSubSliceCount = std::max(topologyData.maxSubSliceCount, tileTopologyData.maxSubSliceCount);
            topologyData.maxEuCount = std::max(topologyData.maxEuCount, static_cast<int>(data->max_eus_per_subslice));

            this->topologyMap[i] = mapping;
        }

        if (success) {
            topologyData.sliceCount = sliceCount;
            topologyData.subSliceCount = subSliceCount;
            topologyData.euCount = euCount;
            return true;
        }
    }

    // fallback to DRM_I915_QUERY_TOPOLOGY_INFO

    auto dataQuery = this->query(DRM_I915_QUERY_TOPOLOGY_INFO, 0);
    if (dataQuery.empty()) {
        return false;
    }
    auto data = reinterpret_cast<drm_i915_query_topology_info *>(dataQuery.data());

    TopologyMapping mapping;
    auto retVal = translateTopologyInfo(data, topologyData, mapping);
    topologyData.maxEuCount = data->max_eus_per_subslice;

    this->topologyMap.clear();
    this->topologyMap[0] = mapping;

    return retVal;
}

void Drm::queryPageFaultSupport() {

    if (const auto paramId = ioctlHelper->getHasPageFaultParamId(); paramId) {
        int support = 0;
        const auto ret = getParamIoctl(*paramId, &support);
        pageFaultSupported = (0 == ret) && (support > 0);
    }
}

unsigned int Drm::bindDrmContext(uint32_t drmContextId, uint32_t deviceIndex, aub_stream::EngineType engineType, bool engineInstancedDevice) {
    auto engineInfo = this->engineInfo.get();
    if (!engineInfo) {
        return DrmEngineMapper::engineNodeMap(engineType);
    }
    auto engine = engineInfo->getEngineInstance(deviceIndex, engineType);
    if (!engine) {
        return DrmEngineMapper::engineNodeMap(engineType);
    }

    bool useVirtualEnginesForCcs = !engineInstancedDevice;
    if (DebugManager.flags.UseDrmVirtualEnginesForCcs.get() != -1) {
        useVirtualEnginesForCcs = !!DebugManager.flags.UseDrmVirtualEnginesForCcs.get();
    }

    auto numberOfCCS = rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
    constexpr uint32_t maxEngines = 9u;

    bool useVirtualEnginesForBcs = EngineHelpers::isBcsVirtualEngineEnabled();
    auto numberOfBCS = rootDeviceEnvironment.getHardwareInfo()->featureTable.ftrBcsInfo.count();

    if (DebugManager.flags.LimitEngineCountForVirtualBcs.get() != -1) {
        numberOfBCS = DebugManager.flags.LimitEngineCountForVirtualBcs.get();
    }

    if (DebugManager.flags.LimitEngineCountForVirtualCcs.get() != -1) {
        numberOfCCS = DebugManager.flags.LimitEngineCountForVirtualCcs.get();
    }

    uint32_t numEnginesInContext = 1;

    I915_DEFINE_CONTEXT_PARAM_ENGINES(contextEngines, 1 + maxEngines){};
    I915_DEFINE_CONTEXT_ENGINES_LOAD_BALANCE(balancer, maxEngines){};

    contextEngines.engines[0] = {engine->engineClass, engine->engineInstance};

    bool setupVirtualEngines = false;
    unsigned int engineCount = static_cast<unsigned int>(numberOfCCS);
    if (useVirtualEnginesForCcs && engine->engineClass == ioctlHelper->getComputeEngineClass() && numberOfCCS > 1u) {
        numEnginesInContext = numberOfCCS + 1;
        balancer.num_siblings = numberOfCCS;
        setupVirtualEngines = true;
    }

    bool includeMainCopyEngineInGroup = false;
    if (useVirtualEnginesForBcs && engine->engineClass == I915_ENGINE_CLASS_COPY && numberOfBCS > 1u) {
        numEnginesInContext = static_cast<uint32_t>(numberOfBCS) + 1;
        balancer.num_siblings = numberOfBCS;
        setupVirtualEngines = true;
        engineCount = static_cast<unsigned int>(rootDeviceEnvironment.getHardwareInfo()->featureTable.ftrBcsInfo.size());
        if (EngineHelpers::getBcsIndex(engineType) == 0u) {
            includeMainCopyEngineInGroup = true;
        } else {
            engineCount--;
            balancer.num_siblings = numberOfBCS - 1;
            numEnginesInContext = static_cast<uint32_t>(numberOfBCS);
        }
    }

    if (setupVirtualEngines) {
        balancer.base.name = I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE;
        contextEngines.extensions = castToUint64(&balancer);
        contextEngines.engines[0].engine_class = I915_ENGINE_CLASS_INVALID;
        contextEngines.engines[0].engine_instance = I915_ENGINE_CLASS_INVALID_NONE;

        for (auto engineIndex = 0u; engineIndex < engineCount; engineIndex++) {
            if (useVirtualEnginesForBcs && engine->engineClass == I915_ENGINE_CLASS_COPY) {
                auto mappedBcsEngineType = static_cast<aub_stream::EngineType>(EngineHelpers::mapBcsIndexToEngineType(engineIndex, includeMainCopyEngineInGroup));
                bool isBcsEnabled = rootDeviceEnvironment.getHardwareInfo()->featureTable.ftrBcsInfo.test(EngineHelpers::getBcsIndex(mappedBcsEngineType));

                if (!isBcsEnabled) {
                    continue;
                }

                engine = engineInfo->getEngineInstance(deviceIndex, mappedBcsEngineType);
            }
            UNRECOVERABLE_IF(!engine);

            if (useVirtualEnginesForCcs && engine->engineClass == ioctlHelper->getComputeEngineClass()) {
                engine = engineInfo->getEngineInstance(deviceIndex, static_cast<aub_stream::EngineType>(EngineHelpers::mapCcsIndexToEngineType(engineIndex)));
            }
            UNRECOVERABLE_IF(!engine);
            balancer.engines[engineIndex] = {engine->engineClass, engine->engineInstance};
            contextEngines.engines[1 + engineIndex] = {engine->engineClass, engine->engineInstance};
        }
    }

    drm_i915_gem_context_param param{};
    param.ctx_id = drmContextId;
    param.size = static_cast<uint32_t>(ptrDiff(contextEngines.engines + numEnginesInContext, &contextEngines));
    param.param = I915_CONTEXT_PARAM_ENGINES;
    param.value = castToUint64(&contextEngines);

    auto retVal = ioctl(DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, &param);
    UNRECOVERABLE_IF(retVal != 0);

    return I915_EXEC_DEFAULT;
}

void Drm::waitForBind(uint32_t vmHandleId) {
    if (pagingFence[vmHandleId] >= fenceVal[vmHandleId]) {
        return;
    }
    auto lock = this->lockBindFenceMutex();
    waitUserFence(0u, castToUint64(&this->pagingFence[vmHandleId]), this->fenceVal[vmHandleId], ValueWidth::U64, -1, ioctlHelper->getWaitUserFenceSoftFlag());
}

bool Drm::isVmBindAvailable() {
    std::call_once(checkBindOnce, [this]() {
        int ret = ioctlHelper->isVmBindAvailable(this);

        auto hwInfo = this->getRootDeviceEnvironment().getHardwareInfo();
        auto hwInfoConfig = HwInfoConfig::get(hwInfo->platform.eProductFamily);
        ret &= static_cast<int>(hwInfoConfig->isNewResidencyModelSupported());

        bindAvailable = ret;

        Drm::overrideBindSupport(bindAvailable);
    });

    return bindAvailable;
}

int changeBufferObjectBinding(Drm *drm, OsContext *osContext, uint32_t vmHandleId, BufferObject *bo, bool bind) {
    auto vmId = drm->getVirtualMemoryAddressSpace(vmHandleId);
    auto ioctlHelper = drm->getIoctlHelper();

    uint64_t flags = 0u;

    if (drm->isPerContextVMRequired()) {
        auto osContextLinux = static_cast<const OsContextLinux *>(osContext);
        UNRECOVERABLE_IF(osContextLinux->getDrmVmIds().size() <= vmHandleId);
        vmId = osContextLinux->getDrmVmIds()[vmHandleId];
    }

    std::unique_ptr<uint8_t[]> extensions;
    if (bind) {
        bool allowUUIDsForDebug = !osContext->isInternalEngine() && !EngineHelpers::isBcs(osContext->getEngineType());
        if (bo->getBindExtHandles().size() > 0 && allowUUIDsForDebug) {
            extensions = ioctlHelper->prepareVmBindExt(bo->getBindExtHandles());
        }
        bool bindCapture = bo->isMarkedForCapture();
        bool bindImmediate = bo->isImmediateBindingRequired();
        bool bindMakeResident = false;
        if (drm->useVMBindImmediate()) {
            bindMakeResident = bo->isExplicitResidencyRequired();
            bindImmediate = true;
        }
        flags |= ioctlHelper->getFlagsForVmBind(bindCapture, bindImmediate, bindMakeResident);
    }

    auto bindAddresses = bo->getColourAddresses();
    auto bindIterations = bindAddresses.size();
    if (bindIterations == 0) {
        bindIterations = 1;
    }

    int ret = 0;
    for (size_t i = 0; i < bindIterations; i++) {

        VmBindParams vmBind{};
        vmBind.vmId = static_cast<uint32_t>(vmId);
        vmBind.flags = flags;
        vmBind.handle = bo->peekHandle();
        vmBind.length = bo->peekSize();
        vmBind.offset = 0;
        vmBind.start = bo->peekAddress();

        if (bo->getColourWithBind()) {
            vmBind.length = bo->getColourChunk();
            vmBind.offset = bo->getColourChunk() * i;
            vmBind.start = bindAddresses[i];
        }

        auto hwInfo = drm->getRootDeviceEnvironment().getHardwareInfo();
        auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);

        bool closEnabled = (hwHelper.getNumCacheRegions() > 0);

        if (DebugManager.flags.ClosEnabled.get() != -1) {
            closEnabled = !!DebugManager.flags.ClosEnabled.get();
        }

        auto vmBindExtSetPat = ioctlHelper->createVmBindExtSetPat();

        if (closEnabled) {
            uint64_t patIndex = ClosHelper::getPatIndex(bo->peekCacheRegion(), bo->peekCachePolicy());
            if (DebugManager.flags.OverridePatIndex.get() != -1) {
                patIndex = static_cast<uint64_t>(DebugManager.flags.OverridePatIndex.get());
            }
            ioctlHelper->fillVmBindExtSetPat(vmBindExtSetPat, patIndex, castToUint64(extensions.get()));
            vmBind.extensions = castToUint64(vmBindExtSetPat.get());
        } else {
            vmBind.extensions = castToUint64(extensions.get());
        }

        if (bind) {
            std::unique_lock<std::mutex> lock;

            auto vmBindExtSyncFence = ioctlHelper->createVmBindExtSyncFence();

            if (drm->useVMBindImmediate()) {
                lock = drm->lockBindFenceMutex();

                if (!drm->hasPageFaultSupport() || bo->isExplicitResidencyRequired()) {
                    auto nextExtension = vmBind.extensions;
                    auto address = castToUint64(drm->getFenceAddr(vmHandleId));
                    auto value = drm->getNextFenceVal(vmHandleId);

                    ioctlHelper->fillVmBindExtSyncFence(vmBindExtSyncFence, address, value, nextExtension);
                    vmBind.extensions = castToUint64(vmBindExtSyncFence.get());
                }
            }

            ret = ioctlHelper->vmBind(drm, vmBind);

            if (ret) {
                break;
            }

            drm->setNewResourceBoundToVM(vmHandleId);
        } else {
            vmBind.handle = 0u;
            ret = ioctlHelper->vmUnbind(drm, vmBind);

            if (ret) {
                break;
            }
        }
    }

    return ret;
}

int Drm::bindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo) {
    auto ret = changeBufferObjectBinding(this, osContext, vmHandleId, bo, true);
    if (ret != 0) {
        static_cast<DrmMemoryOperationsHandlerBind *>(this->rootDeviceEnvironment.memoryOperationsInterface.get())->evictUnusedAllocations(false, false);
        ret = changeBufferObjectBinding(this, osContext, vmHandleId, bo, true);
    }
    return ret;
}

int Drm::unbindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo) {
    return changeBufferObjectBinding(this, osContext, vmHandleId, bo, false);
}

int Drm::createDrmVirtualMemory(uint32_t &drmVmId) {
    drm_i915_gem_vm_control ctl = {};

    std::optional<MemoryClassInstance> regionInstanceClass;

    uint32_t memoryBank = 1 << drmVmId;

    auto hwInfo = this->getRootDeviceEnvironment().getHardwareInfo();
    auto memInfo = this->getMemoryInfo();
    if (DebugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.get() != 0) {
        if (memInfo && HwHelper::get(hwInfo->platform.eRenderCoreFamily).getEnableLocalMemory(*hwInfo)) {
            regionInstanceClass = memInfo->getMemoryRegionClassAndInstance(memoryBank, *this->rootDeviceEnvironment.getHardwareInfo());
        }
    }

    auto vmControlExtRegion = ioctlHelper->createVmControlExtRegion(regionInstanceClass);

    if (vmControlExtRegion) {
        ctl.extensions = castToUint64(vmControlExtRegion.get());
    }

    bool disableScratch = DebugManager.flags.DisableScratchPages.get();
    bool enablePageFault = hasPageFaultSupport() && isVmBindAvailable();

    ctl.flags = ioctlHelper->getFlagsForVmCreate(disableScratch, enablePageFault);

    auto ret = SysCalls::ioctl(getFileDescriptor(), DRM_IOCTL_I915_GEM_VM_CREATE, &ctl);

    if (ret == 0) {
        drmVmId = ctl.vm_id;
        if (ctl.vm_id == 0) {
            // 0 is reserved for invalid/unassigned ppgtt
            return -1;
        }
    } else {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr,
                         "INFO: Cannot create Virtual Memory at memory bank 0x%x info present %d  return code %d\n",
                         memoryBank, memoryInfo != nullptr, ret);
    }
    return ret;
}

PhyicalDevicePciSpeedInfo Drm::getPciSpeedInfo() const {

    PhyicalDevicePciSpeedInfo pciSpeedInfo = {};

    std::string pathPrefix{};
    bool isIntegratedDevice = rootDeviceEnvironment.getHardwareInfo()->capabilityTable.isIntegratedDevice;
    // If integrated device, read properties from the specific device path.
    // If discrete device, read properties from the root path of the pci device.
    if (isIntegratedDevice) {
        auto devicePath = NEO::getPciLinkPath(getFileDescriptor());
        if (!devicePath.has_value()) {
            return pciSpeedInfo;
        }
        pathPrefix = "/sys/class/drm/" + devicePath.value() + "/device/";
    } else {
        auto rootPath = NEO::getPciRootPath(getFileDescriptor());
        if (!rootPath.has_value()) {
            return pciSpeedInfo;
        }
        pathPrefix += "/sys/devices" + rootPath.value();
    }

    std::array<char, 32> readString = {'\0'};

    errno = 0;

    auto readFile = [](const std::string fileName, const std::string_view pathPrefix, std::array<char, 32> &readString) {
        std::ostringstream linkWidthStream{};
        linkWidthStream << pathPrefix << fileName;

        int fd = NEO::SysCalls::open(linkWidthStream.str().c_str(), O_RDONLY);
        ssize_t bytesRead = NEO::SysCalls::pread(fd, readString.data(), readString.size() - 1, 0);
        NEO::SysCalls::close(fd);
        if (bytesRead <= 0) {
            return false;
        }
        std::replace(readString.begin(), readString.end(), '\n', '\0');
        return true;
    };

    // read max link width
    if (readFile("/max_link_width", pathPrefix, readString) != true) {
        return pciSpeedInfo;
    }

    char *endPtr = nullptr;
    uint32_t linkWidth = static_cast<uint32_t>(std::strtoul(readString.data(), &endPtr, 10));
    if ((endPtr == readString.data()) || (errno != 0)) {
        return pciSpeedInfo;
    }
    pciSpeedInfo.width = linkWidth;

    // read max link speed
    if (readFile("/max_link_speed", pathPrefix, readString) != true) {
        return pciSpeedInfo;
    }

    endPtr = nullptr;
    const auto maxSpeed = strtod(readString.data(), &endPtr);
    if ((endPtr == readString.data()) || (errno != 0)) {
        return pciSpeedInfo;
    }

    double gen3EncodingLossFactor = 128.0 / 130.0;
    std::map<double, std::pair<int32_t, double>> maxSpeedToGenAndEncodingLossMapping{
        //{max link speed,  {pci generation,    encoding loss factor}}
        {2.5, {1, 0.2}},
        {5.0, {2, 0.2}},
        {8.0, {3, gen3EncodingLossFactor}},
        {16.0, {4, gen3EncodingLossFactor}},
        {32.0, {5, gen3EncodingLossFactor}}};

    if (maxSpeedToGenAndEncodingLossMapping.find(maxSpeed) == maxSpeedToGenAndEncodingLossMapping.end()) {
        return pciSpeedInfo;
    }
    pciSpeedInfo.genVersion = maxSpeedToGenAndEncodingLossMapping[maxSpeed].first;

    constexpr double gigaBitsPerSecondToBytesPerSecondMultiplier = 125000000;
    const auto maxSpeedWithEncodingLoss = maxSpeed * gigaBitsPerSecondToBytesPerSecondMultiplier * maxSpeedToGenAndEncodingLossMapping[maxSpeed].second;
    pciSpeedInfo.maxBandwidth = maxSpeedWithEncodingLoss * pciSpeedInfo.width;

    return pciSpeedInfo;
}
} // namespace NEO
