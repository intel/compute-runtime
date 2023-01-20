/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"

#include "shared/source/command_stream/submission_status.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_engine_mapper.h"
#include "shared/source/os_interface/linux/drm_gem_close_worker.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/hw_device_id.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/linux/system_info.h"
#include "shared/source/os_interface/os_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/api_intercept.h"
#include "shared/source/utilities/directory.h"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <map>
#include <sstream>

namespace NEO {

Drm::Drm(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceIdIn, RootDeviceEnvironment &rootDeviceEnvironment)
    : DriverModel(DriverModelType::DRM),
      hwDeviceId(std::move(hwDeviceIdIn)), rootDeviceEnvironment(rootDeviceEnvironment) {
    pagingFence.fill(0u);
    fenceVal.fill(0u);
}

SubmissionStatus Drm::getSubmissionStatusFromReturnCode(int32_t retCode) {
    switch (retCode) {
    case 0:
        return SubmissionStatus::SUCCESS;
    case EWOULDBLOCK:
    case ENOMEM:
    case ENOSPC:
        return SubmissionStatus::OUT_OF_HOST_MEMORY;
    case ENXIO:
        return SubmissionStatus::OUT_OF_MEMORY;
    default:
        return SubmissionStatus::FAILED;
    }
}

void Drm::queryAndSetVmBindPatIndexProgrammingSupport() {

    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    this->vmBindPatIndexProgrammingSupported = productHelper.isVmBindPatIndexProgrammingSupported();
}

int Drm::ioctl(DrmIoctl request, void *arg) {
    auto requestValue = getIoctlRequestValue(request, ioctlHelper.get());
    int ret;
    int returnedErrno = 0;
    SYSTEM_ENTER();
    do {
        auto measureTime = DebugManager.flags.PrintIoctlTimes.get();
        std::chrono::steady_clock::time_point start;
        std::chrono::steady_clock::time_point end;

        auto printIoctl = DebugManager.flags.PrintIoctlEntries.get();

        if (printIoctl) {
            printf("IOCTL %s called\n", getIoctlString(request, ioctlHelper.get()).c_str());
        }

        if (measureTime) {
            start = std::chrono::steady_clock::now();
        }
        ret = SysCalls::ioctl(getFileDescriptor(), requestValue, arg);

        if (ret != 0) {
            returnedErrno = getErrno();
        }

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
                       getIoctlString(request, ioctlHelper.get()).c_str(), ret);
            } else {
                printf("IOCTL %s returns %d, errno %d(%s)\n",
                       getIoctlString(request, ioctlHelper.get()).c_str(), ret, returnedErrno, strerror(returnedErrno));
            }
        }

    } while (ret == -1 && checkIfIoctlReinvokeRequired(returnedErrno, request, ioctlHelper.get()));
    SYSTEM_LEAVE(request);
    return ret;
}

int Drm::getParamIoctl(DrmParam param, int *dstValue) {
    GetParam getParam{};
    getParam.param = getDrmParamValue(param, ioctlHelper.get());
    getParam.value = dstValue;

    int retVal = ioctlHelper ? ioctlHelper->ioctl(DrmIoctl::Getparam, &getParam) : ioctl(DrmIoctl::Getparam, &getParam);
    if (DebugManager.flags.PrintIoctlEntries.get()) {
        printf("DRM_IOCTL_I915_GETPARAM: param: %s, output value: %d, retCode:% d\n",
               getDrmParamString(param, ioctlHelper.get()).c_str(),
               *getParam.value,
               retVal);
    }
    return retVal;
}

int Drm::getExecSoftPin(int &execSoftPin) {
    return getParamIoctl(DrmParam::ParamHasExecSoftpin, &execSoftPin);
}

bool Drm::queryI915DeviceIdAndRevision() {
    HardwareInfo *hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    int deviceId = hwInfo->platform.usDeviceID;
    int revisionId = hwInfo->platform.usRevId;
    auto ret = getParamIoctl(DrmParam::ParamChipsetId, &deviceId);
    if (ret != 0) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query device ID parameter!\n");
        return false;
    }
    ret = getParamIoctl(DrmParam::ParamRevision, &revisionId);
    if (ret != 0) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query device Rev ID parameter!\n");
        return false;
    }
    hwInfo->platform.usDeviceID = deviceId;
    hwInfo->platform.usRevId = revisionId;
    return true;
}

int Drm::enableTurboBoost() {
    GemContextParam contextParam = {};

    contextParam.param = contextPrivateParamBoost;
    contextParam.value = 1;
    return ioctlHelper->ioctl(DrmIoctl::GemContextSetparam, &contextParam);
}

int Drm::getEnabledPooledEu(int &enabled) {
    return getParamIoctl(DrmParam::ParamHasPooledEu, &enabled);
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

bool Drm::readSysFsAsString(const std::string &relativeFilePath, std::string &readString) {

    auto devicePath = getSysFsPciPath();
    if (devicePath.empty()) {
        return false;
    }

    const std::string fileName = devicePath + relativeFilePath;
    int fd = SysCalls::open(fileName.c_str(), O_RDONLY);
    if (fd < 0) {
        return false;
    }

    ssize_t bytesRead = SysCalls::pread(fd, readString.data(), readString.size() - 1, 0);
    NEO::SysCalls::close(fd);
    if (bytesRead <= 0) {
        return false;
    }

    std::replace(readString.begin(), readString.end(), '\n', '\0');
    return true;
}

int Drm::queryGttSize(uint64_t &gttSizeOutput) {
    GemContextParam contextParam = {0};
    contextParam.param = ioctlHelper->getDrmParamValue(DrmParam::ContextParamGttSize);

    int ret = ioctlHelper->ioctl(DrmIoctl::GemContextGetparam, &contextParam);
    if (ret == 0) {
        gttSizeOutput = contextParam.value;
    }

    return ret;
}

bool Drm::isGpuHangDetected(OsContext &osContext) {
    const auto osContextLinux = static_cast<OsContextLinux *>(&osContext);
    const auto &drmContextIds = osContextLinux->getDrmContextIds();

    for (const auto drmContextId : drmContextIds) {
        ResetStats resetStats{};
        resetStats.contextId = drmContextId;

        const auto retVal{ioctlHelper->ioctl(DrmIoctl::GetResetStats, &resetStats)};
        UNRECOVERABLE_IF(retVal != 0);

        if (resetStats.batchActive > 0 || resetStats.batchPending > 0) {
            PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "ERROR: GPU HANG detected!\n");
            osContextLinux->setHangDetected();
            return true;
        }
    }

    return false;
}

void Drm::checkPreemptionSupport() {
    int value = 0;
    auto ret = getParamIoctl(DrmParam::ParamHasScheduler, &value);
    auto schedulerCapPreemption = ioctlHelper->getDrmParamValue(DrmParam::SchedulerCapPreemption);
    preemptionSupported = ((0 == ret) && (value & schedulerCapPreemption));
}

void Drm::checkQueueSliceSupport() {
    sliceCountChangeSupported = getQueueSliceCount(&sseu) == 0 ? true : false;
}

void Drm::setLowPriorityContextParam(uint32_t drmContextId) {
    GemContextParam gcp = {};
    gcp.contextId = drmContextId;
    gcp.param = ioctlHelper->getDrmParamValue(DrmParam::ContextParamPriority);
    gcp.value = -1023;

    auto retVal = ioctlHelper->ioctl(DrmIoctl::GemContextSetparam, &gcp);
    UNRECOVERABLE_IF(retVal != 0);
}

int Drm::getQueueSliceCount(GemContextParamSseu *sseu) {
    GemContextParam contextParam = {};
    contextParam.param = ioctlHelper->getDrmParamValue(DrmParam::ContextParamSseu);
    sseu->engine.engineClass = ioctlHelper->getDrmParamValue(DrmParam::EngineClassRender);
    sseu->engine.engineInstance = ioctlHelper->getDrmParamValue(DrmParam::ExecDefault);
    contextParam.value = reinterpret_cast<uint64_t>(sseu);
    contextParam.size = sizeof(struct GemContextParamSseu);

    return ioctlHelper->ioctl(DrmIoctl::GemContextGetparam, &contextParam);
}

uint64_t Drm::getSliceMask(uint64_t sliceCount) {
    return maxNBitValue(sliceCount);
}
bool Drm::setQueueSliceCount(uint64_t sliceCount) {
    if (sliceCountChangeSupported) {
        GemContextParam contextParam = {};
        sseu.sliceMask = getSliceMask(sliceCount);

        contextParam.param = ioctlHelper->getDrmParamValue(DrmParam::ContextParamSseu);
        contextParam.contextId = 0;
        contextParam.value = reinterpret_cast<uint64_t>(&sseu);
        contextParam.size = sizeof(struct GemContextParamSseu);
        int retVal = ioctlHelper->ioctl(DrmIoctl::GemContextSetparam, &contextParam);
        if (retVal == 0) {
            return true;
        }
    }
    return false;
}

void Drm::checkNonPersistentContextsSupport() {
    GemContextParam contextParam = {};
    contextParam.param = ioctlHelper->getDrmParamValue(DrmParam::ContextParamPersistence);

    auto retVal = ioctlHelper->ioctl(DrmIoctl::GemContextGetparam, &contextParam);
    if (retVal == 0 && contextParam.value == 1) {
        nonPersistentContextsSupported = true;
    } else {
        nonPersistentContextsSupported = false;
    }
}

void Drm::setNonPersistentContext(uint32_t drmContextId) {
    GemContextParam contextParam = {};
    contextParam.contextId = drmContextId;
    contextParam.param = ioctlHelper->getDrmParamValue(DrmParam::ContextParamPersistence);

    ioctlHelper->ioctl(DrmIoctl::GemContextSetparam, &contextParam);
}

void Drm::setUnrecoverableContext(uint32_t drmContextId) {
    GemContextParam contextParam = {};
    contextParam.contextId = drmContextId;
    contextParam.param = ioctlHelper->getDrmParamValue(DrmParam::ContextParamRecoverable);
    contextParam.value = 0;
    contextParam.size = 0;

    ioctlHelper->ioctl(DrmIoctl::GemContextSetparam, &contextParam);
}

uint32_t Drm::createDrmContext(uint32_t drmVmId, bool isDirectSubmissionRequested, bool isCooperativeContextRequested) {
    GemContextCreateExt gcc{};

    if (DebugManager.flags.DirectSubmissionDrmContext.get() != -1) {
        isDirectSubmissionRequested = DebugManager.flags.DirectSubmissionDrmContext.get();
    }
    if (isDirectSubmissionRequested) {
        gcc.flags |= ioctlHelper->getDirectSubmissionFlag();
    }

    GemContextCreateExtSetParam extSetparam = {};

    if (drmVmId > 0) {
        extSetparam.base.name = ioctlHelper->getDrmParamValue(DrmParam::ContextCreateExtSetparam);
        extSetparam.param.param = ioctlHelper->getDrmParamValue(DrmParam::ContextParamVm);
        extSetparam.param.value = drmVmId;
        gcc.extensions = reinterpret_cast<uint64_t>(&extSetparam);
        gcc.flags |= ioctlHelper->getDrmParamValue(DrmParam::ContextCreateFlagsUseExtensions);
    }

    if (DebugManager.flags.CreateContextWithAccessCounters.get() != -1) {
        return ioctlHelper->createContextWithAccessCounters(gcc);
    }

    if (DebugManager.flags.ForceRunAloneContext.get() != -1) {
        isCooperativeContextRequested = DebugManager.flags.ForceRunAloneContext.get();
    }
    if (isCooperativeContextRequested) {
        return ioctlHelper->createCooperativeContext(gcc);
    }
    auto ioctlResult = ioctlHelper->ioctl(DrmIoctl::GemContextCreateExt, &gcc);

    UNRECOVERABLE_IF(ioctlResult != 0);
    return gcc.contextId;
}

void Drm::destroyDrmContext(uint32_t drmContextId) {
    GemContextDestroy destroy{};
    destroy.contextId = drmContextId;
    auto retVal = ioctlHelper->ioctl(DrmIoctl::GemContextDestroy, &destroy);
    UNRECOVERABLE_IF((retVal != 0) && (errno != ENODEV));
}

void Drm::destroyDrmVirtualMemory(uint32_t drmVmId) {
    GemVmControl ctl = {};
    ctl.vmId = drmVmId;
    auto ret = ioctlHelper->ioctl(DrmIoctl::GemVmDestroy, &ctl);
    UNRECOVERABLE_IF((ret != 0) && (errno != ENODEV));
}

int Drm::queryVmId(uint32_t drmContextId, uint32_t &vmId) {
    GemContextParam param{};
    param.contextId = drmContextId;
    param.value = 0;
    param.param = ioctlHelper->getDrmParamValue(DrmParam::ContextParamVm);
    auto retVal = ioctlHelper->ioctl(DrmIoctl::GemContextGetparam, &param);

    vmId = static_cast<uint32_t>(param.value);

    return retVal;
}

std::unique_lock<std::mutex> Drm::lockBindFenceMutex() {
    return std::unique_lock<std::mutex>(this->bindFenceMutex);
}

int Drm::getEuTotal(int &euTotal) {
    return getParamIoctl(DrmParam::ParamEuTotal, &euTotal);
}

int Drm::getSubsliceTotal(int &subsliceTotal) {
    return getParamIoctl(DrmParam::ParamSubsliceTotal, &subsliceTotal);
}

int Drm::getMinEuInPool(int &minEUinPool) {
    return getParamIoctl(DrmParam::ParamMinEuInPool, &minEUinPool);
}

int Drm::getErrno() {
    return errno;
}

int Drm::setupHardwareInfo(const DeviceDescriptor *device, bool setupFeatureTableAndWorkaroundTable) {
    HardwareInfo *hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    auto deviceId = hwInfo->platform.usDeviceID;
    auto revisionId = hwInfo->platform.usRevId;

    rootDeviceEnvironment.setHwInfoAndInitHelpers(device->pHwInfo);

    hwInfo->platform.usDeviceID = deviceId;
    hwInfo->platform.usRevId = revisionId;

    const auto productFamily = hwInfo->platform.eProductFamily;
    setupIoctlHelper(productFamily);

    Drm::QueryTopologyData topologyData = {};

    bool status = queryTopology(*hwInfo, topologyData);

    if (!status) {
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Topology query failed!\n");

        auto ret = getEuTotal(topologyData.euCount);
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
    if (topologyData.maxSubSliceCount > 0) {
        hwInfo->gtSystemInfo.MaxSubSlicesSupported = static_cast<uint32_t>(topologyData.maxSubSliceCount);
        hwInfo->gtSystemInfo.MaxDualSubSlicesSupported = static_cast<uint32_t>(topologyData.maxSubSliceCount);
    }

    status = querySystemInfo();
    device->setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);

    if (status) {
        systemInfo->checkSysInfoMismatch(hwInfo);
        setupSystemInfo(hwInfo, systemInfo.get());

        uint32_t bankCount = (hwInfo->gtSystemInfo.L3BankCount > 0) ? hwInfo->gtSystemInfo.L3BankCount : hwInfo->gtSystemInfo.MaxDualSubSlicesSupported;

        hwInfo->gtSystemInfo.L3CacheSizeInKb = systemInfo->getL3BankSizeInKb() * bankCount;
    }

    setupCacheInfo(*hwInfo);

    return 0;
}

void appendHwDeviceId(std::vector<std::unique_ptr<HwDeviceId>> &hwDeviceIds, int fileDescriptor, const char *pciPath) {
    if (fileDescriptor >= 0) {
        if (Drm::isDrmSupported(fileDescriptor)) {
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

std::string Drm::getDrmVersion(int fileDescriptor) {
    DrmVersion version = {};
    char name[5] = {};
    version.name = name;
    version.nameLen = 5;

    auto requestValue = getIoctlRequestValue(DrmIoctl::Version, nullptr);

    int ret = SysCalls::ioctl(fileDescriptor, requestValue, &version);
    if (ret) {
        return {};
    }

    name[4] = '\0';
    return std::string(name);
}

std::vector<uint8_t> Drm::query(uint32_t queryId, uint32_t queryItemFlags) {
    Query query{};
    QueryItem queryItem{};
    queryItem.queryId = queryId;
    queryItem.length = 0; // query length first
    queryItem.flags = queryItemFlags;
    query.itemsPtr = reinterpret_cast<uint64_t>(&queryItem);
    query.numItems = 1;

    auto ret = ioctlHelper->ioctl(DrmIoctl::Query, &query);
    if (ret != 0 || queryItem.length <= 0) {
        return {};
    }

    auto data = std::vector<uint8_t>(queryItem.length, 0);
    queryItem.dataPtr = castToUint64(data.data());

    ret = ioctlHelper->ioctl(DrmIoctl::Query, &query);
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
               getIoctlString(ioctlData.first, ioctlHelper.get()).c_str(),
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

uint32_t Drm::getVirtualMemoryAddressSpace(uint32_t vmId) const {
    if (vmId < virtualMemoryIds.size()) {
        return virtualMemoryIds[vmId];
    }
    return 0;
}

void Drm::setNewResourceBoundToVM(BufferObject *bo, uint32_t vmHandleId) {
    if (!this->rootDeviceEnvironment.getProductHelper().isTlbFlushRequired() && isAligned(bo->peekAddress(), MemoryConstants::pageSize2Mb)) {
        return;
    }
    const auto &engines = this->rootDeviceEnvironment.executionEnvironment.memoryManager->getRegisteredEngines();
    for (const auto &engine : engines) {
        if (engine.osContext->getDeviceBitfield().test(vmHandleId)) {
            auto osContextLinux = static_cast<OsContextLinux *>(engine.osContext);

            if (&osContextLinux->getDrm() == this) {
                osContextLinux->setNewResourceBound();
            }
        }
    }
}

bool Drm::translateTopologyInfo(const QueryTopologyInfo *queryTopologyInfo, QueryTopologyData &data, TopologyMapping &mapping) {
    int sliceCount = 0;
    int subSliceCount = 0;
    int euCount = 0;
    int maxSliceCount = 0;
    int maxSubSliceCountPerSlice = 0;
    std::vector<int> sliceIndices;
    sliceIndices.reserve(maxSliceCount);

    for (int x = 0; x < queryTopologyInfo->maxSlices; x++) {
        bool isSliceEnable = (queryTopologyInfo->data[x / 8] >> (x % 8)) & 1;
        if (!isSliceEnable) {
            continue;
        }
        sliceIndices.push_back(x);
        sliceCount++;

        std::vector<int> subSliceIndices;
        subSliceIndices.reserve(queryTopologyInfo->maxSubslices);

        for (int y = 0; y < queryTopologyInfo->maxSubslices; y++) {
            size_t yOffset = (queryTopologyInfo->subsliceOffset + x * queryTopologyInfo->subsliceStride + y / 8);
            bool isSubSliceEnabled = (queryTopologyInfo->data[yOffset] >> (y % 8)) & 1;
            if (!isSubSliceEnabled) {
                continue;
            }
            subSliceCount++;
            subSliceIndices.push_back(y);

            for (int z = 0; z < queryTopologyInfo->maxEusPerSubslice; z++) {
                size_t zOffset = (queryTopologyInfo->euOffset + (x * queryTopologyInfo->maxSubslices + y) * queryTopologyInfo->euStride + z / 8);
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
    PhysicalDevicePciBusInfo pciBusInfo(PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue);

    if (adapterBDF.Data != std::numeric_limits<uint32_t>::max()) {
        pciBusInfo.pciDomain = this->pciDomain;
        pciBusInfo.pciBus = adapterBDF.Bus;
        pciBusInfo.pciDevice = adapterBDF.Device;
        pciBusInfo.pciFunction = adapterBDF.Function;
    }
    return pciBusInfo;
}

void Drm::cleanup() {
    destroyVirtualMemoryAddressSpace();
}

Drm::~Drm() {
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

int Drm::waitHandle(uint32_t waitHandle, int64_t timeout) {
    UNRECOVERABLE_IF(isVmBindAvailable());

    GemWait wait{};
    wait.boHandle = waitHandle;
    wait.timeoutNs = timeout;

    int ret = ioctlHelper->ioctl(DrmIoctl::GemWait, &wait);
    if (ret != 0) {
        int err = errno;
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(I915_GEM_WAIT) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
    }

    return ret;
}

int Drm::getTimestampFrequency(int &frequency) {
    frequency = 0;
    return getParamIoctl(DrmParam::ParamCsTimestampFrequency, &frequency);
}

bool Drm::queryEngineInfo() {
    return Drm::queryEngineInfo(false);
}

bool Drm::sysmanQueryEngineInfo() {
    return Drm::queryEngineInfo(true);
}

bool Drm::isDebugAttachAvailable() {
    return ioctlHelper->isDebugAttachAvailable();
}

int getMaxGpuFrequencyOfDevice(Drm &drm, std::string &sysFsPciPath, int &maxGpuFrequency) {
    maxGpuFrequency = 0;
    std::string clockSysFsPath = sysFsPciPath + drm.getIoctlHelper()->getFileForMaxGpuFrequency();

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
    std::string clockSysFsPath = sysFsPciPath + drm.getIoctlHelper()->getFileForMaxGpuFrequencyOfSubDevice(subDeviceId);

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

bool Drm::getDeviceMemoryMaxClockRateInMhz(uint32_t tileId, uint32_t &clkRate) {
    const std::string relativefilePath = ioctlHelper->getFileForMaxMemoryFrequencyOfSubDevice(tileId);
    std::string readString(64, '\0');
    errno = 0;
    if (readSysFsAsString(relativefilePath, readString) == false) {
        return false;
    }

    char *endPtr = nullptr;
    uint32_t retClkRate = static_cast<uint32_t>(std::strtoul(readString.data(), &endPtr, 10));
    if ((endPtr == readString.data()) || (errno != 0)) {
        return false;
    }
    clkRate = retClkRate;
    return true;
}

bool Drm::getDeviceMemoryPhysicalSizeInBytes(uint32_t tileId, uint64_t &physicalSize) {
    const std::string relativefilePath = "/gt/gt" + std::to_string(tileId) + "/addr_range";
    std::string readString(64, '\0');
    errno = 0;
    if (readSysFsAsString(relativefilePath, readString) == false) {
        return false;
    }

    char *endPtr = nullptr;
    uint64_t retSize = static_cast<uint64_t>(std::strtoull(readString.data(), &endPtr, 16));
    if ((endPtr == readString.data()) || (errno != 0)) {
        return false;
    }
    physicalSize = retSize;
    return true;
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
    gtSysInfo->MemoryType = sysInfo->getMemoryType();
    gtSysInfo->TotalVsThreads = sysInfo->getTotalVsThreads();
    gtSysInfo->TotalHsThreads = sysInfo->getTotalHsThreads();
    gtSysInfo->TotalDsThreads = sysInfo->getTotalDsThreads();
    gtSysInfo->TotalGsThreads = sysInfo->getTotalGsThreads();
    gtSysInfo->TotalPsThreadsWindowerRange = sysInfo->getTotalPsThreads();
    gtSysInfo->MaxEuPerSubSlice = sysInfo->getMaxEuPerDualSubSlice();
    gtSysInfo->MaxSlicesSupported = sysInfo->getMaxSlicesSupported();
    if (sysInfo->getMaxDualSubSlicesSupported() > 0) {
        gtSysInfo->MaxSubSlicesSupported = sysInfo->getMaxDualSubSlicesSupported();
        gtSysInfo->MaxDualSubSlicesSupported = sysInfo->getMaxDualSubSlicesSupported();
    }
}

void Drm::setupCacheInfo(const HardwareInfo &hwInfo) {
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();

    if (DebugManager.flags.ClosEnabled.get() == 0 || gfxCoreHelper.getNumCacheRegions() == 0) {
        this->cacheInfo.reset(new CacheInfo(*this, 0, 0, 0));
        return;
    }

    const GT_SYSTEM_INFO *gtSysInfo = &hwInfo.gtSystemInfo;

    constexpr uint16_t maxNumWays = 32;
    constexpr uint16_t globalReservationLimit = 16;
    constexpr uint16_t clientReservationLimit = 8;
    constexpr uint16_t maxReservationNumWays = std::min(globalReservationLimit, clientReservationLimit);
    const size_t totalCacheSize = gtSysInfo->L3CacheSizeInKb * MemoryConstants::kiloByte;
    const size_t maxReservationCacheSize = (totalCacheSize * maxReservationNumWays) / maxNumWays;
    const uint32_t maxReservationNumCacheRegions = gfxCoreHelper.getNumCacheRegions() - 1;

    this->cacheInfo.reset(new CacheInfo(*this, maxReservationCacheSize, maxReservationNumCacheRegions, maxReservationNumWays));
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
    return ioctlHelper->waitUserFence(ctxId, address, value, static_cast<uint32_t>(dataWidth), timeout, flags);
}

bool Drm::querySystemInfo() {
    auto request = ioctlHelper->getDrmParamValue(DrmParam::QueryHwconfigTable);
    auto deviceBlobQuery = this->query(request, 0);
    if (deviceBlobQuery.empty()) {
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stdout, "%s", "INFO: System Info query failed!\n");
        return false;
    }
    this->systemInfo.reset(new SystemInfo(deviceBlobQuery));
    return true;
}

std::vector<uint8_t> Drm::getMemoryRegions() {
    auto request = ioctlHelper->getDrmParamValue(DrmParam::QueryMemoryRegions);
    return this->query(request, 0);
}

bool Drm::queryMemoryInfo() {
    auto dataQuery = getMemoryRegions();
    if (!dataQuery.empty()) {
        auto memRegions = ioctlHelper->translateToMemoryRegions(dataQuery);
        this->memoryInfo.reset(new MemoryInfo(memRegions, *this));
        return true;
    }
    return false;
}

bool Drm::queryEngineInfo(bool isSysmanEnabled) {
    auto request = ioctlHelper->getDrmParamValue(DrmParam::QueryEngineInfo);
    auto enginesQuery = this->query(request, 0);
    if (enginesQuery.empty()) {
        return false;
    }
    auto engines = ioctlHelper->translateToEngineCaps(enginesQuery);
    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();

    auto memInfo = memoryInfo.get();

    if (!memInfo) {
        this->engineInfo.reset(new EngineInfo(this, engines));
        return true;
    }

    auto &memoryRegions = memInfo->getDrmRegionInfos();

    auto tileCount = 0u;
    std::vector<DistanceInfo> distanceInfos;
    for (const auto &region : memoryRegions) {
        if (ioctlHelper->getDrmParamValue(DrmParam::MemoryClassDevice) == region.region.memoryClass) {
            tileCount++;
            DistanceInfo distanceInfo{};
            distanceInfo.region = region.region;

            for (const auto &engine : engines) {
                if (engine.engine.engineClass == ioctlHelper->getDrmParamValue(DrmParam::EngineClassCompute) ||
                    engine.engine.engineClass == ioctlHelper->getDrmParamValue(DrmParam::EngineClassRender) ||
                    engine.engine.engineClass == ioctlHelper->getDrmParamValue(DrmParam::EngineClassCopy)) {
                    distanceInfo.engine = engine.engine;
                    distanceInfos.push_back(distanceInfo);
                } else if (isSysmanEnabled) {

                    if (engine.engine.engineClass == ioctlHelper->getDrmParamValue(DrmParam::EngineClassVideo) ||
                        engine.engine.engineClass == ioctlHelper->getDrmParamValue(DrmParam::EngineClassVideoEnhance)) {
                        distanceInfo.engine = engine.engine;
                        distanceInfos.push_back(distanceInfo);
                    }
                }
            }
        }
    }

    if (tileCount == 0u) {
        this->engineInfo.reset(new EngineInfo(this, engines));
        return true;
    }

    std::vector<QueryItem> queryItems{distanceInfos.size()};
    auto ret = ioctlHelper->queryDistances(queryItems, distanceInfos);
    if (ret != 0) {
        return false;
    }

    const bool queryUnsupported = std::all_of(queryItems.begin(), queryItems.end(),
                                              [](const QueryItem &item) { return item.length == -EINVAL; });
    if (queryUnsupported) {
        DEBUG_BREAK_IF(tileCount != 1);
        this->engineInfo.reset(new EngineInfo(this, engines));
        return true;
    }

    memInfo->assignRegionsFromDistances(distanceInfos);

    auto &multiTileArchInfo = hwInfo->gtSystemInfo.MultiTileArchInfo;
    multiTileArchInfo.IsValid = true;
    multiTileArchInfo.TileCount = tileCount;
    multiTileArchInfo.TileMask = static_cast<uint8_t>(maxNBitValue(tileCount));

    this->engineInfo.reset(new EngineInfo(this, tileCount, distanceInfos, queryItems, engines));
    return true;
}

bool Drm::completionFenceSupport() {
    std::call_once(checkCompletionFenceOnce, [this]() {
        const bool vmBindAvailable = isVmBindAvailable();
        bool support = ioctlHelper->completionFenceExtensionSupported(vmBindAvailable);
        int32_t overrideCompletionFence = DebugManager.flags.EnableDrmCompletionFence.get();
        if (overrideCompletionFence != -1) {
            support = !!overrideCompletionFence;
        }

        completionFenceSupported = support;
        if (DebugManager.flags.PrintCompletionFenceUsage.get()) {
            std::cout << "Completion fence supported: " << completionFenceSupported << std::endl;
        }
    });
    return completionFenceSupported;
}

void Drm::setupIoctlHelper(const PRODUCT_FAMILY productFamily) {
    if (!this->ioctlHelper) {
        std::string prelimVersion = "";
        getPrelimVersion(prelimVersion);
        this->ioctlHelper = IoctlHelper::getI915Helper(productFamily, prelimVersion, *this);
        this->ioctlHelper->initialize();
    }
}

bool Drm::queryTopology(const HardwareInfo &hwInfo, QueryTopologyData &topologyData) {
    topologyData.sliceCount = 0;
    topologyData.subSliceCount = 0;
    topologyData.euCount = 0;

    int sliceCount = 0;
    int subSliceCount = 0;
    int euCount = 0;

    auto request = ioctlHelper->getDrmParamValue(DrmParam::QueryComputeSlices);
    if (DebugManager.flags.UseNewQueryTopoIoctl.get() && this->engineInfo && hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount > 0 && request != 0) {
        bool success = true;

        for (uint32_t i = 0; i < hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount; i++) {
            auto classInstance = this->engineInfo->getEngineInstance(i, hwInfo.capabilityTable.defaultEngineType);
            UNRECOVERABLE_IF(!classInstance);

            uint32_t flags = classInstance->engineClass;
            flags |= (classInstance->engineInstance << 8);

            auto dataQuery = this->query(request, flags);
            if (dataQuery.empty()) {
                success = false;
                break;
            }
            auto data = reinterpret_cast<QueryTopologyInfo *>(dataQuery.data());

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
            topologyData.maxEuCount = std::max(topologyData.maxEuCount, static_cast<int>(data->maxEusPerSubslice));

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

    request = ioctlHelper->getDrmParamValue(DrmParam::QueryTopologyInfo);
    auto dataQuery = this->query(request, 0);
    if (dataQuery.empty()) {
        return false;
    }
    auto data = reinterpret_cast<QueryTopologyInfo *>(dataQuery.data());

    TopologyMapping mapping;
    auto retVal = translateTopologyInfo(data, topologyData, mapping);
    topologyData.maxEuCount = data->maxEusPerSubslice;

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

bool Drm::hasPageFaultSupport() const {
    if (DebugManager.flags.EnableRecoverablePageFaults.get() != -1) {
        return !!DebugManager.flags.EnableRecoverablePageFaults.get();
    }

    return false;
}

unsigned int Drm::bindDrmContext(uint32_t drmContextId, uint32_t deviceIndex, aub_stream::EngineType engineType, bool engineInstancedDevice) {
    auto engineInfo = this->engineInfo.get();

    auto retVal = static_cast<unsigned int>(ioctlHelper->getDrmParamValue(DrmEngineMapper::engineNodeMap(engineType)));

    if (!engineInfo) {
        return retVal;
    }
    auto engine = engineInfo->getEngineInstance(deviceIndex, engineType);
    if (!engine) {
        return retVal;
    }

    bool useVirtualEnginesForCcs = !engineInstancedDevice;
    if (DebugManager.flags.UseDrmVirtualEnginesForCcs.get() != -1) {
        useVirtualEnginesForCcs = !!DebugManager.flags.UseDrmVirtualEnginesForCcs.get();
    }

    auto numberOfCCS = rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
    constexpr uint32_t maxEngines = 9u;

    bool useVirtualEnginesForBcs = EngineHelpers::isBcsVirtualEngineEnabled(engineType);
    auto numberOfBCS = rootDeviceEnvironment.getHardwareInfo()->featureTable.ftrBcsInfo.count();

    if (DebugManager.flags.LimitEngineCountForVirtualBcs.get() != -1) {
        numberOfBCS = DebugManager.flags.LimitEngineCountForVirtualBcs.get();
    }

    if (DebugManager.flags.LimitEngineCountForVirtualCcs.get() != -1) {
        numberOfCCS = DebugManager.flags.LimitEngineCountForVirtualCcs.get();
    }

    uint32_t numEnginesInContext = 1;

    ContextParamEngines<1 + maxEngines> contextEngines{};
    ContextEnginesLoadBalance<maxEngines> balancer{};

    contextEngines.engines[0] = {engine->engineClass, engine->engineInstance};

    bool setupVirtualEngines = false;
    unsigned int engineCount = static_cast<unsigned int>(numberOfCCS);
    if (useVirtualEnginesForCcs && engine->engineClass == ioctlHelper->getDrmParamValue(DrmParam::EngineClassCompute) && numberOfCCS > 1u) {
        numEnginesInContext = numberOfCCS + 1;
        balancer.numSiblings = numberOfCCS;
        setupVirtualEngines = true;
    }

    bool includeMainCopyEngineInGroup = false;
    if (useVirtualEnginesForBcs && engine->engineClass == ioctlHelper->getDrmParamValue(DrmParam::EngineClassCopy) && numberOfBCS > 1u) {
        numEnginesInContext = static_cast<uint32_t>(numberOfBCS) + 1;
        balancer.numSiblings = numberOfBCS;
        setupVirtualEngines = true;
        engineCount = static_cast<unsigned int>(rootDeviceEnvironment.getHardwareInfo()->featureTable.ftrBcsInfo.size());
        if (EngineHelpers::getBcsIndex(engineType) == 0u) {
            includeMainCopyEngineInGroup = true;
        } else {
            engineCount--;
            balancer.numSiblings = numberOfBCS - 1;
            numEnginesInContext = static_cast<uint32_t>(numberOfBCS);
        }
    }

    if (setupVirtualEngines) {
        balancer.base.name = ioctlHelper->getDrmParamValue(DrmParam::ContextEnginesExtLoadBalance);
        contextEngines.extensions = castToUint64(&balancer);
        contextEngines.engines[0].engineClass = ioctlHelper->getDrmParamValue(DrmParam::EngineClassInvalid);
        contextEngines.engines[0].engineInstance = ioctlHelper->getDrmParamValue(DrmParam::EngineClassInvalidNone);

        for (auto engineIndex = 0u; engineIndex < engineCount; engineIndex++) {
            if (useVirtualEnginesForBcs && engine->engineClass == ioctlHelper->getDrmParamValue(DrmParam::EngineClassCopy)) {
                auto mappedBcsEngineType = static_cast<aub_stream::EngineType>(EngineHelpers::mapBcsIndexToEngineType(engineIndex, includeMainCopyEngineInGroup));
                bool isBcsEnabled = rootDeviceEnvironment.getHardwareInfo()->featureTable.ftrBcsInfo.test(EngineHelpers::getBcsIndex(mappedBcsEngineType));

                if (!isBcsEnabled) {
                    continue;
                }

                engine = engineInfo->getEngineInstance(deviceIndex, mappedBcsEngineType);
            }
            UNRECOVERABLE_IF(!engine);

            if (useVirtualEnginesForCcs && engine->engineClass == ioctlHelper->getDrmParamValue(DrmParam::EngineClassCompute)) {
                engine = engineInfo->getEngineInstance(deviceIndex, static_cast<aub_stream::EngineType>(EngineHelpers::mapCcsIndexToEngineType(engineIndex)));
            }
            UNRECOVERABLE_IF(!engine);
            balancer.engines[engineIndex] = {engine->engineClass, engine->engineInstance};
            contextEngines.engines[1 + engineIndex] = {engine->engineClass, engine->engineInstance};
        }
    }

    GemContextParam param{};
    param.contextId = drmContextId;
    param.size = static_cast<uint32_t>(ptrDiff(contextEngines.engines + numEnginesInContext, &contextEngines));
    param.param = ioctlHelper->getDrmParamValue(DrmParam::ContextParamEngines);
    param.value = castToUint64(&contextEngines);

    auto ioctlValue = ioctlHelper->ioctl(DrmIoctl::GemContextSetparam, &param);
    UNRECOVERABLE_IF(ioctlValue != 0);

    retVal = static_cast<unsigned int>(ioctlHelper->getDrmParamValue(DrmParam::ExecDefault));
    return retVal;
}

void Drm::waitForBind(uint32_t vmHandleId) {
    if (pagingFence[vmHandleId] >= fenceVal[vmHandleId]) {
        return;
    }
    auto lock = this->lockBindFenceMutex();
    auto fenceAddress = castToUint64(&this->pagingFence[vmHandleId]);
    auto fenceValue = this->fenceVal[vmHandleId];
    lock.unlock();

    waitUserFence(0u, fenceAddress, fenceValue, ValueWidth::U64, -1, ioctlHelper->getWaitUserFenceSoftFlag());
}

bool Drm::isSetPairAvailable() {
    if (DebugManager.flags.EnableSetPair.get() == 1) {
        std::call_once(checkSetPairOnce, [this]() {
            int ret = ioctlHelper->isSetPairAvailable();
            setPairAvailable = ret;
        });
    }

    return setPairAvailable;
}

bool Drm::isVmBindAvailable() {
    std::call_once(checkBindOnce, [this]() {
        int ret = ioctlHelper->isVmBindAvailable();

        const auto &productHelper = this->getRootDeviceEnvironment().getHelper<ProductHelper>();

        ret &= static_cast<int>(productHelper.isNewResidencyModelSupported());

        bindAvailable = ret;

        Drm::overrideBindSupport(bindAvailable);

        queryAndSetVmBindPatIndexProgrammingSupport();
    });

    return bindAvailable;
}

uint64_t Drm::getPatIndex(Gmm *gmm, AllocationType allocationType, CacheRegion cacheRegion, CachePolicy cachePolicy, bool closEnabled) const {
    if (DebugManager.flags.OverridePatIndex.get() != -1) {
        return static_cast<uint64_t>(DebugManager.flags.OverridePatIndex.get());
    }

    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();

    if (!this->vmBindPatIndexProgrammingSupported) {
        return CommonConstants::unsupportedPatIndex;
    }

    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto &productHelper = rootDeviceEnvironment.getProductHelper();

    GMM_RESOURCE_INFO *resourceInfo = nullptr;
    GMM_RESOURCE_USAGE_TYPE usageType = CacheSettingsHelper::getGmmUsageType(allocationType, false, *hwInfo);
    bool cachable = !CacheSettingsHelper::isUncachedType(usageType);
    bool compressed = false;

    if (gmm) {
        resourceInfo = gmm->gmmResourceInfo->peekGmmResourceInfo();
        usageType = gmm->resourceParams.Usage;
        compressed = gmm->isCompressionEnabled;
        cachable = gmm->gmmResourceInfo->getResourceFlags()->Info.Cacheable;
    }

    uint64_t patIndex = rootDeviceEnvironment.getGmmClientContext()->cachePolicyGetPATIndex(resourceInfo, usageType, compressed, cachable);
    patIndex = productHelper.overridePatIndex(allocationType, patIndex);

    UNRECOVERABLE_IF(patIndex == static_cast<uint64_t>(GMM_PAT_ERROR));

    if (DebugManager.flags.ClosEnabled.get() != -1) {
        closEnabled = !!DebugManager.flags.ClosEnabled.get();
    }

    if (closEnabled) {
        patIndex = gfxCoreHelper.getPatIndex(cacheRegion, cachePolicy);
    }

    return patIndex;
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

    auto &bindAddresses = bo->getColourAddresses();
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

        VmBindExtSetPatT vmBindExtSetPat{};

        if (drm->isVmBindPatIndexProgrammingSupported()) {
            UNRECOVERABLE_IF(bo->peekPatIndex() == CommonConstants::unsupportedPatIndex);
            ioctlHelper->fillVmBindExtSetPat(vmBindExtSetPat, bo->peekPatIndex(), castToUint64(extensions.get()));
            vmBind.extensions = castToUint64(vmBindExtSetPat);
        } else {
            vmBind.extensions = castToUint64(extensions.get());
        }

        if (bind) {
            std::unique_lock<std::mutex> lock;

            VmBindExtUserFenceT vmBindExtUserFence{};

            if (drm->useVMBindImmediate()) {
                lock = drm->lockBindFenceMutex();

                if (!drm->hasPageFaultSupport() || bo->isExplicitResidencyRequired()) {
                    auto nextExtension = vmBind.extensions;
                    auto address = castToUint64(drm->getFenceAddr(vmHandleId));
                    auto value = drm->getNextFenceVal(vmHandleId);

                    ioctlHelper->fillVmBindExtUserFence(vmBindExtUserFence, address, value, nextExtension);
                    vmBind.extensions = castToUint64(vmBindExtUserFence);
                }
            }

            ret = ioctlHelper->vmBind(vmBind);

            if (ret) {
                break;
            }

            drm->setNewResourceBoundToVM(bo, vmHandleId);
        } else {
            vmBind.handle = 0u;
            ret = ioctlHelper->vmUnbind(vmBind);

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
    GemVmControl ctl{};

    std::optional<MemoryClassInstance> regionInstanceClass;

    uint32_t memoryBank = 1 << drmVmId;

    auto hwInfo = this->getRootDeviceEnvironment().getHardwareInfo();
    auto memInfo = this->getMemoryInfo();
    if (DebugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.get() != 0) {
        if (memInfo && rootDeviceEnvironment.getHelper<GfxCoreHelper>().getEnableLocalMemory(*hwInfo)) {
            regionInstanceClass = memInfo->getMemoryRegionClassAndInstance(memoryBank, *this->rootDeviceEnvironment.getHardwareInfo());
        }
    }

    auto vmControlExtRegion = ioctlHelper->createVmControlExtRegion(regionInstanceClass);

    if (vmControlExtRegion) {
        ctl.extensions = castToUint64(vmControlExtRegion.get());
    }

    bool disableScratch = DebugManager.flags.DisableScratchPages.get();
    bool useVmBind = isVmBindAvailable();
    bool enablePageFault = hasPageFaultSupport() && useVmBind;

    ctl.flags = ioctlHelper->getFlagsForVmCreate(disableScratch, enablePageFault, useVmBind);

    auto ret = ioctlHelper->ioctl(DrmIoctl::GemVmCreate, &ctl);

    if (ret == 0) {
        drmVmId = ctl.vmId;
        if (ctl.vmId == 0) {
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

PhysicalDevicePciSpeedInfo Drm::getPciSpeedInfo() const {

    PhysicalDevicePciSpeedInfo pciSpeedInfo = {};

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
        if (fd < 0) {
            return false;
        }
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
    pciSpeedInfo.maxBandwidth = static_cast<int64_t>(maxSpeedWithEncodingLoss * pciSpeedInfo.width);

    return pciSpeedInfo;
}

void Drm::waitOnUserFences(const OsContextLinux &osContext, uint64_t address, uint64_t value, uint32_t numActiveTiles, uint32_t postSyncOffset) {
    auto &drmContextIds = osContext.getDrmContextIds();
    UNRECOVERABLE_IF(numActiveTiles > drmContextIds.size());
    auto completionFenceCpuAddress = address;
    static constexpr int64_t defaultTimeout = -1;
    const auto selectedTimeout = osContext.isHangDetected() ? 1 : defaultTimeout;

    for (auto drmIterator = 0u; drmIterator < numActiveTiles; drmIterator++) {
        if (*reinterpret_cast<uint32_t *>(completionFenceCpuAddress) < value) {
            static constexpr uint16_t flags = 0;
            int retVal = waitUserFence(drmContextIds[drmIterator], completionFenceCpuAddress, value, Drm::ValueWidth::U64, selectedTimeout, flags);
            if (DebugManager.flags.PrintCompletionFenceUsage.get()) {
                std::cout << "Completion fence waited."
                          << " Status: " << retVal
                          << ", CPU address: " << std::hex << completionFenceCpuAddress << std::dec
                          << ", current value: " << *reinterpret_cast<uint32_t *>(completionFenceCpuAddress)
                          << ", wait value: " << value << std::endl;
            }
        } else if (DebugManager.flags.PrintCompletionFenceUsage.get()) {
            std::cout << "Completion fence already completed."
                      << " CPU address: " << std::hex << completionFenceCpuAddress << std::dec
                      << ", current value: " << *reinterpret_cast<uint32_t *>(completionFenceCpuAddress)
                      << ", wait value: " << value << std::endl;
        }
        completionFenceCpuAddress = ptrOffset(completionFenceCpuAddress, postSyncOffset);
    }
}
} // namespace NEO
