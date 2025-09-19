/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/device_caps_reader.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/gpu_page_fault_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_engine_mapper.h"
#include "shared/source/os_interface/linux/drm_gem_close_worker.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/hw_device_id.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/linux/system_info.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/source/os_interface/os_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/utilities/api_intercept.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/source/utilities/directory.h"
#include "shared/source/utilities/io_functions.h"

#include "xe_drm.h"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <map>
#include <sstream>

namespace NEO {

Drm::Drm(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceIdIn, RootDeviceEnvironment &rootDeviceEnvironment)
    : DriverModel(DriverModelType::drm),
      hwDeviceId(std::move(hwDeviceIdIn)), rootDeviceEnvironment(rootDeviceEnvironment) {
    pagingFence.fill(0u);
    fenceVal.fill(0u);
    minimalChunkingSize = MemoryConstants::pageSize2M;
}

SubmissionStatus Drm::getSubmissionStatusFromReturnCode(int32_t retCode) {
    switch (retCode) {
    case 0:
        return SubmissionStatus::success;
    case EWOULDBLOCK:
    case ENOMEM:
    case ENOSPC:
        return SubmissionStatus::outOfHostMemory;
    case ENXIO:
        return SubmissionStatus::outOfMemory;
    default:
        return SubmissionStatus::failed;
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
        auto measureTime = debugManager.flags.PrintKmdTimes.get();
        std::chrono::steady_clock::time_point start;
        std::chrono::steady_clock::time_point end;

        auto printIoctl = debugManager.flags.PrintIoctlEntries.get();

        if (printIoctl) {
            PRINT_DEBUG_STRING(true, stdout, "IOCTL %s called\n", ioctlHelper->getIoctlString(request).c_str());
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

            static std::mutex mtx;
            std::lock_guard lock(mtx);

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
                PRINT_DEBUG_STRING(true, stdout, "IOCTL %s returns %d\n",
                                   ioctlHelper->getIoctlString(request).c_str(), ret);
            } else {
                PRINT_DEBUG_STRING(true, stdout, "IOCTL %s returns %d, errno %d(%s)\n",
                                   ioctlHelper->getIoctlString(request).c_str(), ret, returnedErrno, strerror(returnedErrno));
            }
        }

    } while (ret == -1 && checkIfIoctlReinvokeRequired(returnedErrno, request, ioctlHelper.get()));
    SYSTEM_LEAVE(request);
    return ret;
}

int Drm::getParamIoctl(DrmParam param, int *dstValue) {
    GetParam getParam{};
    getParam.param = ioctlHelper->getDrmParamValue(param);
    getParam.value = dstValue;

    int retVal = ioctlHelper->ioctl(DrmIoctl::getparam, &getParam);
    if (debugManager.flags.PrintIoctlEntries.get()) {
        printf("DRM_IOCTL_I915_GETPARAM: param: %s, output value: %d, retCode:% d\n",
               ioctlHelper->getDrmParamString(param).c_str(),
               *getParam.value,
               retVal);
    }
    return retVal;
}

int Drm::enableTurboBoost() {
    GemContextParam contextParam = {};

    contextParam.param = contextPrivateParamBoost;
    contextParam.value = 1;
    return ioctlHelper->ioctl(DrmIoctl::gemContextSetparam, &contextParam);
}

int Drm::getEnabledPooledEu(int &enabled) {
    return getParamIoctl(DrmParam::paramHasPooledEu, &enabled);
}

std::string Drm::getSysFsPciPathBaseName() {
    auto fullPath = getSysFsPciPath();
    size_t pos = fullPath.rfind("/");
    if (std::string::npos == pos) {
        return fullPath;
    }
    return fullPath.substr(pos + 1, std::string::npos);
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

int Drm::queryGttSize(uint64_t &gttSizeOutput, bool alignUpToFullRange) {
    GemContextParam contextParam = {0};
    contextParam.param = ioctlHelper->getDrmParamValue(DrmParam::contextParamGttSize);

    int ret = ioctlHelper->ioctl(DrmIoctl::gemContextGetparam, &contextParam);
    if (ret == 0) {
        if (alignUpToFullRange) {
            gttSizeOutput = Drm::alignUpGttSize(contextParam.value);
        } else {
            gttSizeOutput = contextParam.value;
        }
    }

    return ret;
}

bool Drm::isGpuHangDetected(OsContext &osContext) {
    bool ret = checkResetStatus(osContext);
    auto threshold = getGpuFaultCheckThreshold();

    if (checkGpuPageFaultRequired()) {
        if (gpuFaultCheckCounter >= threshold) {
            auto memoryManager = static_cast<DrmMemoryManager *>(this->rootDeviceEnvironment.executionEnvironment.memoryManager.get());
            memoryManager->checkUnexpectedGpuPageFault();
            gpuFaultCheckCounter = 0;
            return false;
        }
        gpuFaultCheckCounter++;
    }
    return ret;
}

bool Drm::checkResetStatus(OsContext &osContext) {
    const auto osContextLinux = static_cast<OsContextLinux *>(&osContext);
    const auto &drmContextIds = osContextLinux->getDrmContextIds();

    for (const auto drmContextId : drmContextIds) {
        ResetStats resetStats{};
        resetStats.contextId = drmContextId;
        ResetStatsFault fault{};
        uint32_t status = 0;
        const auto retVal{ioctlHelper->getResetStats(resetStats, &status, &fault)};
        UNRECOVERABLE_IF(retVal != 0);
        auto debuggingEnabled = rootDeviceEnvironment.executionEnvironment.isDebuggingEnabled();
        if (checkToDisableScratchPage() && ioctlHelper->validPageFault(fault.flags)) {
            bool banned = ((status & ioctlHelper->getStatusForResetStats(true)) != 0);
            if (!banned && debuggingEnabled) {
                return false;
            }
            IoFunctions::fprintf(stderr, "Segmentation fault from GPU at 0x%llx, ctx_id: %u (%s) type: %d (%s), level: %d (%s), access: %d (%s), banned: %d, aborting.\n",
                                 fault.addr,
                                 resetStats.contextId,
                                 EngineHelpers::engineTypeToString(osContext.getEngineType()).c_str(),
                                 fault.type, GpuPageFaultHelpers::faultTypeToString(static_cast<FaultType>(fault.type)).c_str(),
                                 fault.level, GpuPageFaultHelpers::faultLevelToString(static_cast<FaultLevel>(fault.level)).c_str(),
                                 fault.access, GpuPageFaultHelpers::faultAccessToString(static_cast<FaultAccess>(fault.access)).c_str(),
                                 banned);
            IoFunctions::fprintf(stdout, "Segmentation fault from GPU at 0x%llx, ctx_id: %u (%s) type: %d (%s), level: %d (%s), access: %d (%s), banned: %d, aborting.\n",
                                 fault.addr,
                                 resetStats.contextId,
                                 EngineHelpers::engineTypeToString(osContext.getEngineType()).c_str(),
                                 fault.type, GpuPageFaultHelpers::faultTypeToString(static_cast<FaultType>(fault.type)).c_str(),
                                 fault.level, GpuPageFaultHelpers::faultLevelToString(static_cast<FaultLevel>(fault.level)).c_str(),
                                 fault.access, GpuPageFaultHelpers::faultAccessToString(static_cast<FaultAccess>(fault.access)).c_str(),
                                 banned);
            UNRECOVERABLE_IF(true);
        }
        if (resetStats.batchActive > 0 || resetStats.batchPending > 0) {
            PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "ERROR: GPU HANG detected!\n");
            osContextLinux->setHangDetected();
            return true;
        }
    }
    return false;
}

void Drm::checkPreemptionSupport() {
    preemptionSupported = ioctlHelper->isPreemptionSupported();
}

void Drm::checkQueueSliceSupport() {
    sliceCountChangeSupported = getQueueSliceCount(&sseu) == 0 ? true : false;
}

void Drm::setLowPriorityContextParam(uint32_t drmContextId) {
    GemContextParam gcp = {};
    gcp.contextId = drmContextId;
    gcp.param = ioctlHelper->getDrmParamValue(DrmParam::contextParamPriority);
    gcp.value = -1023;

    auto retVal = ioctlHelper->ioctl(DrmIoctl::gemContextSetparam, &gcp);
    UNRECOVERABLE_IF(retVal != 0);
}

int Drm::getQueueSliceCount(GemContextParamSseu *sseu) {
    GemContextParam contextParam = {};
    contextParam.param = ioctlHelper->getDrmParamValue(DrmParam::contextParamSseu);
    sseu->engine.engineClass = ioctlHelper->getDrmParamValue(DrmParam::engineClassRender);
    sseu->engine.engineInstance = ioctlHelper->getDrmParamValue(DrmParam::execDefault);
    contextParam.value = reinterpret_cast<uint64_t>(sseu);
    contextParam.size = sizeof(struct GemContextParamSseu);

    return ioctlHelper->ioctl(DrmIoctl::gemContextGetparam, &contextParam);
}

uint64_t Drm::getSliceMask(uint64_t sliceCount) {
    return maxNBitValue(sliceCount);
}
bool Drm::setQueueSliceCount(uint64_t sliceCount) {
    if (sliceCountChangeSupported) {
        GemContextParam contextParam = {};
        sseu.sliceMask = getSliceMask(sliceCount);

        contextParam.param = ioctlHelper->getDrmParamValue(DrmParam::contextParamSseu);
        contextParam.contextId = 0;
        contextParam.value = reinterpret_cast<uint64_t>(&sseu);
        contextParam.size = sizeof(struct GemContextParamSseu);
        int retVal = ioctlHelper->ioctl(DrmIoctl::gemContextSetparam, &contextParam);
        if (retVal == 0) {
            return true;
        }
    }
    return false;
}

void Drm::checkNonPersistentContextsSupport() {
    GemContextParam contextParam = {};
    contextParam.param = ioctlHelper->getDrmParamValue(DrmParam::contextParamPersistence);

    auto retVal = ioctlHelper->ioctl(DrmIoctl::gemContextGetparam, &contextParam);
    if (retVal == 0 && contextParam.value == 1) {
        nonPersistentContextsSupported = true;
    } else {
        nonPersistentContextsSupported = false;
    }
}

void Drm::setNonPersistentContext(uint32_t drmContextId) {
    GemContextParam contextParam = {};
    contextParam.contextId = drmContextId;
    contextParam.param = ioctlHelper->getDrmParamValue(DrmParam::contextParamPersistence);

    ioctlHelper->ioctl(DrmIoctl::gemContextSetparam, &contextParam);
}

void Drm::setUnrecoverableContext(uint32_t drmContextId) {
    GemContextParam contextParam = {};
    contextParam.contextId = drmContextId;
    contextParam.param = ioctlHelper->getDrmParamValue(DrmParam::contextParamRecoverable);
    contextParam.value = 0;
    contextParam.size = 0;

    ioctlHelper->ioctl(DrmIoctl::gemContextSetparam, &contextParam);
}

int Drm::createDrmContext(uint32_t drmVmId, bool isDirectSubmissionRequested, bool isCooperativeContextRequested) {
    GemContextCreateExt gcc{};

    if (debugManager.flags.DirectSubmissionDrmContext.get() != -1) {
        isDirectSubmissionRequested = debugManager.flags.DirectSubmissionDrmContext.get();
    }
    if (isDirectSubmissionRequested) {
        gcc.flags |= ioctlHelper->getDirectSubmissionFlag();
    }

    GemContextCreateExtSetParam extSetparam = {};
    GemContextCreateExtSetParam extSetparamLowLatency = {};
    if (drmVmId > 0) {
        extSetparam.base.name = ioctlHelper->getDrmParamValue(DrmParam::contextCreateExtSetparam);
        extSetparam.param.param = ioctlHelper->getDrmParamValue(DrmParam::contextParamVm);
        extSetparam.param.value = drmVmId;
        if (ioctlHelper->hasContextFreqHint()) {
            extSetparam.base.nextExtension = reinterpret_cast<uint64_t>(&extSetparamLowLatency.base);
            ioctlHelper->fillExtSetparamLowLatency(extSetparamLowLatency);
        }
        gcc.extensions = reinterpret_cast<uint64_t>(&extSetparam);
        gcc.flags |= ioctlHelper->getDrmParamValue(DrmParam::contextCreateFlagsUseExtensions);
    }

    if (debugManager.flags.CreateContextWithAccessCounters.get() > 0) {
        return ioctlHelper->createContextWithAccessCounters(gcc);
    }

    if (debugManager.flags.ForceRunAloneContext.get() != -1) {
        isCooperativeContextRequested = debugManager.flags.ForceRunAloneContext.get();
    }
    if (isCooperativeContextRequested) {
        return ioctlHelper->createCooperativeContext(gcc);
    }
    auto ioctlResult = ioctlHelper->ioctl(DrmIoctl::gemContextCreateExt, &gcc);

    if (ioctlResult < 0) {
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: GemContextCreateExt ioctl failed. Not exposing this root device\n");
        return ioctlResult;
    }

    return gcc.contextId;
}

void Drm::destroyDrmContext(uint32_t drmContextId) {
    GemContextDestroy destroy{};
    destroy.contextId = drmContextId;
    auto retVal = ioctlHelper->ioctl(DrmIoctl::gemContextDestroy, &destroy);
    UNRECOVERABLE_IF((retVal != 0) && (errno != ENODEV));
}

void Drm::destroyDrmVirtualMemory(uint32_t drmVmId) {
    GemVmControl ctl = {};
    ctl.vmId = drmVmId;
    auto ret = ioctlHelper->ioctl(DrmIoctl::gemVmDestroy, &ctl);
    UNRECOVERABLE_IF((ret != 0) && (errno != ENODEV));
}

int Drm::queryVmId(uint32_t drmContextId, uint32_t &vmId) {
    GemContextParam param{};
    param.contextId = drmContextId;
    param.value = 0;
    param.param = ioctlHelper->getDrmParamValue(DrmParam::contextParamVm);
    auto retVal = ioctlHelper->ioctl(DrmIoctl::gemContextGetparam, &param);

    vmId = static_cast<uint32_t>(param.value);

    return retVal;
}

std::unique_lock<std::mutex> Drm::lockBindFenceMutex() {
    return std::unique_lock<std::mutex>(this->bindFenceMutex);
}

int Drm::getEuTotal(int &euTotal) {
    return getParamIoctl(DrmParam::paramEuTotal, &euTotal);
}

int Drm::getSubsliceTotal(int &subsliceTotal) {
    return getParamIoctl(DrmParam::paramSubsliceTotal, &subsliceTotal);
}

int Drm::getMinEuInPool(int &minEUinPool) {
    return getParamIoctl(DrmParam::paramMinEuInPool, &minEUinPool);
}

int Drm::getErrno() {
    return errno;
}

int Drm::setupHardwareInfo(const DeviceDescriptor *device, bool setupFeatureTableAndWorkaroundTable) {
    const auto usDeviceIdOverride = rootDeviceEnvironment.getHardwareInfo()->platform.usDeviceID;
    const auto usRevIdOverride = rootDeviceEnvironment.getHardwareInfo()->platform.usRevId;

    // reset hwInfo and apply overrides
    rootDeviceEnvironment.setHwInfo(device->pHwInfo);
    HardwareInfo *hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    hwInfo->platform.usDeviceID = usDeviceIdOverride;
    hwInfo->platform.usRevId = usRevIdOverride;

    rootDeviceEnvironment.initProductHelper();
    rootDeviceEnvironment.initGfxCoreHelper();
    rootDeviceEnvironment.initializeGfxCoreHelperFromProductHelper();
    rootDeviceEnvironment.initApiGfxCoreHelper();
    rootDeviceEnvironment.initCompilerProductHelper();
    rootDeviceEnvironment.initAilConfigurationHelper();
    rootDeviceEnvironment.initWaitUtils();
    auto result = rootDeviceEnvironment.initAilConfiguration();
    if (false == result) {
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: AIL creation failed!\n");
        return -1;
    }

    const auto productFamily = hwInfo->platform.eProductFamily;
    setupIoctlHelper(productFamily);
    ioctlHelper->setupIpVersion();
    rootDeviceEnvironment.initReleaseHelper();

    auto releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    device->setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
    this->adjustSharedSystemMemCapabilities();

    querySystemInfo();

    if (systemInfo) {
        systemInfo->checkSysInfoMismatch(hwInfo);
        setupSystemInfo(hwInfo, systemInfo.get());

        auto numRegions = systemInfo->getNumRegions();
        if (numRegions > 0) {
            hwInfo->featureTable.regionCount = numRegions;
        }

        hwInfo->gtSystemInfo.NumThreadsPerEu = systemInfo->getNumThreadsPerEu();
    }

    auto &productHelper = rootDeviceEnvironment.getProductHelper();
    auto capsReader = productHelper.getDeviceCapsReader(*this);
    if (capsReader) {
        if (!productHelper.setupHardwareInfo(*hwInfo, *capsReader))
            return -1;
    }

    if (!queryMemoryInfo()) {
        setPerContextVMRequired(true);
        printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Failed to query memory info\n");
    } else if (getMemoryInfo()->isSmallBarDetected()) {
        IoFunctions::fprintf(stderr, "WARNING: Small BAR detected for device %s\n", getPciPath().c_str());
        if (!ioctlHelper->isSmallBarConfigAllowed()) {
            return -1;
        }
    }

    if (!queryEngineInfo()) {
        setPerContextVMRequired(true);
        printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Failed to query engine info\n");
    }

    if (!hwInfo->gtSystemInfo.L3BankCount) {
        hwInfo->gtSystemInfo.L3BankCount = hwInfo->gtSystemInfo.MaxDualSubSlicesSupported;
    }

    DrmQueryTopologyData topologyData = {};

    if (!queryTopology(*hwInfo, topologyData)) {
        topologyData.sliceCount = hwInfo->gtSystemInfo.SliceCount;
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Topology query failed!\n");

        auto ret = getEuTotal(topologyData.euCount);
        if (ret != 0) {
            PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query EU total parameter!\n");
            return ret;
        }

        ret = getSubsliceTotal(topologyData.subSliceCount);
        if (ret != 0) {
            PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query subslice total parameter!\n");
            return ret;
        }
    }

    hwInfo->gtSystemInfo.SliceCount = static_cast<uint32_t>(topologyData.sliceCount);
    if (!topologyMap.empty()) {
        hwInfo->gtSystemInfo.IsDynamicallyPopulated = true;
        std::bitset<GT_MAX_SLICE> totalSliceMask{maxNBitValue(GT_MAX_SLICE)};
        uint32_t latestSliceIndex = 0;
        for (auto &mapping : topologyMap) {
            std::bitset<GT_MAX_SLICE> sliceMask;
            DEBUG_BREAK_IF(mapping.second.sliceIndices.empty());
            for (auto &slice : mapping.second.sliceIndices) {
                sliceMask.set(slice);
                latestSliceIndex = slice;
            }
            totalSliceMask &= sliceMask;
        }
        for (uint32_t slice = 0; slice < GT_MAX_SLICE; slice++) {
            hwInfo->gtSystemInfo.SliceInfo[slice].Enabled = totalSliceMask.test(slice);
        }
        if (totalSliceMask.none()) {
            PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Incorrect slice mask from topology map!\n");
            return -1;
        }
        if (totalSliceMask.count() == 1u) {
            std::bitset<GT_MAX_SUBSLICE_PER_SLICE> totalSubSliceMask{maxNBitValue(GT_MAX_SUBSLICE_PER_SLICE)};
            for (auto &mapping : topologyMap) {
                std::bitset<GT_MAX_SUBSLICE_PER_SLICE> subSliceMask;
                DEBUG_BREAK_IF(mapping.second.subsliceIndices.empty());
                for (auto &subslice : mapping.second.subsliceIndices) {
                    if (subslice >= GT_MAX_SUBSLICE_PER_SLICE) {
                        subSliceMask = {};
                        break;
                    }
                    subSliceMask.set(subslice);
                }
                totalSubSliceMask &= subSliceMask;
            }

            for (uint32_t subslice = 0; subslice < GT_MAX_SUBSLICE_PER_SLICE; subslice++) {
                hwInfo->gtSystemInfo.SliceInfo[latestSliceIndex].SubSliceInfo[subslice].Enabled = totalSubSliceMask.test(subslice);
            }
        }
    }

    hwInfo->gtSystemInfo.SubSliceCount = static_cast<uint32_t>(topologyData.subSliceCount);
    hwInfo->gtSystemInfo.DualSubSliceCount = static_cast<uint32_t>(topologyData.subSliceCount);

    if (!hwInfo->gtSystemInfo.MaxEuPerSubSlice) {
        hwInfo->gtSystemInfo.MaxEuPerSubSlice = topologyData.maxEusPerSubSlice;
    }

    auto maxEuCount = static_cast<uint32_t>(topologyData.subSliceCount) * hwInfo->gtSystemInfo.MaxEuPerSubSlice;

    if (topologyData.euCount == 0 || static_cast<uint32_t>(topologyData.euCount) > maxEuCount) {
        hwInfo->gtSystemInfo.EUCount = maxEuCount;
    } else {
        hwInfo->gtSystemInfo.EUCount = static_cast<uint32_t>(topologyData.euCount);
    }

    if (!hwInfo->gtSystemInfo.EUCount) {
        return -1;
    }

    if (debugManager.flags.OverrideNumThreadsPerEu.get() != -1) {
        hwInfo->gtSystemInfo.NumThreadsPerEu = debugManager.flags.OverrideNumThreadsPerEu.get();
    }

    hwInfo->gtSystemInfo.ThreadCount = hwInfo->gtSystemInfo.NumThreadsPerEu * hwInfo->gtSystemInfo.EUCount;

    if (ioctlHelper->overrideMaxSlicesSupported()) {
        hwInfo->gtSystemInfo.MaxSlicesSupported = hwInfo->gtSystemInfo.SliceCount;
    }

    auto calculatedMaxSubSliceCount = topologyData.maxSlices * topologyData.maxSubSlicesPerSlice;
    auto maxSubSliceCount = std::max(static_cast<uint32_t>(calculatedMaxSubSliceCount), hwInfo->gtSystemInfo.MaxSubSlicesSupported);

    hwInfo->gtSystemInfo.MaxSubSlicesSupported = maxSubSliceCount;
    hwInfo->gtSystemInfo.MaxDualSubSlicesSupported = maxSubSliceCount;

    if (topologyData.numL3Banks > 0) {
        hwInfo->gtSystemInfo.L3BankCount = topologyData.numL3Banks;
    }

    if (systemInfo) {
        if (systemInfo->getNumL3BanksPerGroup() > 0 && systemInfo->getNumL3BankGroups() > 0) {
            hwInfo->gtSystemInfo.L3BankCount = systemInfo->getNumL3BanksPerGroup() * systemInfo->getNumL3BankGroups();
        }

        hwInfo->gtSystemInfo.L3CacheSizeInKb = systemInfo->getL3BankSizeInKb() * hwInfo->gtSystemInfo.L3BankCount;
    }

    rootDeviceEnvironment.setRcsExposure();

    setupCacheInfo(*hwInfo);
    hwInfo->capabilityTable.deviceName = device->devName;

    rootDeviceEnvironment.initializeGfxCoreHelperFromHwInfo();

    return 0;
}

void appendHwDeviceId(std::vector<std::unique_ptr<HwDeviceId>> &hwDeviceIds, int fileDescriptor, const char *pciPath, const char *devNodePath) {
    if (fileDescriptor >= 0) {
        if (Drm::isDrmSupported(fileDescriptor)) {
            hwDeviceIds.push_back(std::make_unique<HwDeviceIdDrm>(fileDescriptor, pciPath, devNodePath));
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
    if (debugManager.flags.CreateMultipleRootDevices.get()) {
        numRootDevices = debugManager.flags.CreateMultipleRootDevices.get();
    }

    errno = 0;
    std::vector<std::string> files = Directory::getFiles(Os::pciDevicesDirectory);
    int returnedErrno = errno;

    if (files.size() == 0) {
        if (returnedErrno == EACCES || returnedErrno == EPERM) {
            executionEnvironment.setDevicePermissionError(true);
        }

        const char *pathPrefix = "/dev/dri/renderD";
        const unsigned int maxDrmDevices = 64;
        unsigned int startNum = 128;

        for (unsigned int i = 0; i < maxDrmDevices; i++) {
            std::string path = std::string(pathPrefix) + std::to_string(i + startNum);
            int fileDescriptor = SysCalls::open(path.c_str(), O_RDWR | O_CLOEXEC);

            if (fileDescriptor < 0) {
                returnedErrno = errno;
                if (returnedErrno == EACCES || returnedErrno == EPERM) {
                    executionEnvironment.setDevicePermissionError(true);
                }
                continue;
            }
            auto pciPath = NEO::getPciPath(fileDescriptor);

            appendHwDeviceId(hwDeviceIds, fileDescriptor, pciPath.value_or("0000:00:02.0").c_str(), path.c_str());
            if (!hwDeviceIds.empty() && hwDeviceIds.size() == numRootDevices) {
                break;
            }
        }

        if (!hwDeviceIds.empty()) {
            executionEnvironment.setDevicePermissionError(false);
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
                    // if osPciPath is non-empty, then interest is only in discovering device having same bdf as osPciPath. Skip all other devices.
                    continue;
                }
            }

            if (debugManager.flags.FilterBdfPath.get() != "unk") {
                if (devicePathView.find(debugManager.flags.FilterBdfPath.get().c_str()) == std::string::npos) {
                    continue;
                }
            }
            int fileDescriptor = SysCalls::open(file->c_str(), O_RDWR | O_CLOEXEC);

            if (fileDescriptor < 0) {
                returnedErrno = errno;
                if (returnedErrno == EACCES || returnedErrno == EPERM) {
                    executionEnvironment.setDevicePermissionError(true);
                }
                continue;
            }

            appendHwDeviceId(hwDeviceIds, fileDescriptor, pciPath.c_str(), file->c_str());
            if (!hwDeviceIds.empty() && hwDeviceIds.size() == numRootDevices) {
                break;
            }
        }
        if (hwDeviceIds.empty()) {
            return hwDeviceIds;
        }
    } while (hwDeviceIds.size() < numRootDevices);

    if (!hwDeviceIds.empty()) {
        executionEnvironment.setDevicePermissionError(false);
    }

    return hwDeviceIds;
}

std::string Drm::getDrmVersion(int fileDescriptor) {
    DrmVersion version = {};
    char name[5] = {};
    version.name = name;
    version.nameLen = 5;

    auto requestValue = getIoctlRequestValue(DrmIoctl::version, nullptr);

    int ret = SysCalls::ioctl(fileDescriptor, requestValue, &version);
    if (ret) {
        return {};
    }

    name[4] = '\0';
    return std::string(name);
}

template <typename DataType>
std::vector<DataType> Drm::query(uint32_t queryId, uint32_t queryItemFlags) {
    Query query{};
    QueryItem queryItem{};
    queryItem.queryId = queryId;
    queryItem.length = 0; // query length first
    queryItem.flags = queryItemFlags;
    query.itemsPtr = reinterpret_cast<uint64_t>(&queryItem);
    query.numItems = 1;

    auto ret = ioctlHelper->ioctl(DrmIoctl::query, &query);
    if (ret != 0 || queryItem.length <= 0) {
        return {};
    }

    auto data = std::vector<DataType>(Math::divideAndRoundUp(queryItem.length, sizeof(DataType)), 0);
    queryItem.dataPtr = castToUint64(data.data());

    ret = ioctlHelper->ioctl(DrmIoctl::query, &query);
    if (ret != 0 || queryItem.length <= 0) {
        return {};
    }
    return data;
}

void Drm::printIoctlStatistics() {
    if (!debugManager.flags.PrintKmdTimes.get()) {
        return;
    }

    printf("\n--- Ioctls statistics ---\n");
    printf("%41s %15s %10s %20s %20s %20s", "Request", "Total time(ns)", "Count", "Avg time per ioctl", "Min", "Max\n");
    for (const auto &ioctlData : this->ioctlStatistics) {
        printf("%41s %15llu %10lu %20f %20lld %20lld\n",
               ioctlHelper->getIoctlString(ioctlData.first).c_str(),
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
    if (!this->rootDeviceEnvironment.getProductHelper().isTlbFlushRequired()) {
        return;
    }
    const auto &engines = this->rootDeviceEnvironment.executionEnvironment.memoryManager->getRegisteredEngines(bo->getRootDeviceIndex());
    for (const auto &engine : engines) {
        if (engine.osContext->getDeviceBitfield().test(vmHandleId)) {
            auto osContextLinux = static_cast<OsContextLinux *>(engine.osContext);
            osContextLinux->setNewResourceBound();
        }
    }
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

    StackVec<std::unique_lock<NEO::CommandStreamReceiver::MutexType>, 1> locks{};
    if (this->rootDeviceEnvironment.executionEnvironment.memoryManager) {
        const auto &mulitEngines = this->rootDeviceEnvironment.executionEnvironment.memoryManager->getRegisteredEngines();
        for (const auto &engines : mulitEngines) {
            for (const auto &engine : engines) {
                if (engine.osContext->isDirectSubmissionLightActive()) {
                    locks.push_back(engine.commandStreamReceiver->obtainUniqueOwnership());
                    engine.commandStreamReceiver->stopDirectSubmission(false, false);
                }
            }
        }
    }

    int ret = ioctlHelper->ioctl(DrmIoctl::gemWait, &wait);
    if (ret != 0) {
        int err = errno;
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(I915_GEM_WAIT) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
    }

    return ret;
}

int Drm::getTimestampFrequency(int &frequency) {
    frequency = 0;
    return getParamIoctl(DrmParam::paramCsTimestampFrequency, &frequency);
}

int Drm::getOaTimestampFrequency(int &frequency) {
    frequency = 0;
    return getParamIoctl(DrmParam::paramOATimestampFrequency, &frequency);
}

bool Drm::queryEngineInfo() {
    UNRECOVERABLE_IF(!memoryInfoQueried);
    UNRECOVERABLE_IF(engineInfoQueried);
    engineInfoQueried = true;
    return Drm::queryEngineInfo(false);
}

bool Drm::sysmanQueryEngineInfo() {
    return Drm::queryEngineInfo(true);
}

bool Drm::isDebugAttachAvailable() {
    int enableEuDebug = getEuDebugSysFsEnable();
    return (enableEuDebug == 1) && ioctlHelper->isDebugAttachAvailable();
}

int Drm::getEuDebugSysFsEnable() {
    return ioctlHelper->getEuDebugSysFsEnable();
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
    if (memoryInfo == nullptr || memoryInfo->getLocalMemoryRegions().size() == 0U) {
        physicalSize = 0U;
        return false;
    }

    physicalSize = memoryInfo->getLocalMemoryRegionSize(tileId);
    return true;
}

bool Drm::useVMBindImmediate() const {
    bool useBindImmediate = isDirectSubmissionActive() || hasPageFaultSupport() || ioctlHelper->isImmediateVmBindRequired();

    if (debugManager.flags.EnableImmediateVmBindExt.get() != -1) {
        useBindImmediate = debugManager.flags.EnableImmediateVmBindExt.get();
    }

    return useBindImmediate;
}

void Drm::setupSystemInfo(HardwareInfo *hwInfo, SystemInfo *sysInfo) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->MaxEuPerSubSlice = sysInfo->getMaxEuPerDualSubSlice();
    gtSysInfo->MemoryType = sysInfo->getMemoryType();
    gtSysInfo->MaxSlicesSupported = sysInfo->getMaxSlicesSupported();
    gtSysInfo->MaxSubSlicesSupported = sysInfo->getMaxDualSubSlicesSupported();
    gtSysInfo->MaxDualSubSlicesSupported = sysInfo->getMaxDualSubSlicesSupported();
    gtSysInfo->CsrSizeInMb = sysInfo->getCsrSizeInMb();
    gtSysInfo->SLMSizeInKb = sysInfo->getSlmSizePerDss();
}

void Drm::setupCacheInfo(const HardwareInfo &hwInfo) {
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();

    if (debugManager.flags.ForceStaticL2ClosReservation.get()) {
        if (debugManager.flags.L2ClosNumCacheWays.get() == -1) {
            debugManager.flags.L2ClosNumCacheWays.set(2U);
        }
    }

    auto getL2CacheReservationLimits{[&productHelper]() {
        CacheReservationParameters out{};
        if (productHelper.getNumCacheRegions() == 0) {
            return out;
        }

        if (auto numCacheWays{debugManager.flags.L2ClosNumCacheWays.get()}; numCacheWays != -1) {
            out.maxSize = 1U;
            out.maxNumRegions = 1U;
            out.maxNumWays = static_cast<uint16_t>(numCacheWays);
            return out;
        }
        return out;
    }};

    auto getL3CacheReservationLimits{[&hwInfo, &productHelper]() {
        CacheReservationParameters out{};
        if (debugManager.flags.ClosEnabled.get() == 0 || productHelper.getNumCacheRegions() == 0) {
            return out;
        }

        constexpr uint16_t totalMaxNumWays = 32U;
        constexpr uint16_t globalReservationLimit = 16U;
        constexpr uint16_t clientReservationLimit = 8U;
        const size_t totalCacheSize = hwInfo.gtSystemInfo.L3CacheSizeInKb * MemoryConstants::kiloByte;

        out.maxNumWays = std::min(globalReservationLimit, clientReservationLimit);
        out.maxSize = (totalCacheSize * out.maxNumWays) / totalMaxNumWays;
        out.maxNumRegions = productHelper.getNumCacheRegions() - 1;

        return out;
    }};

    this->cacheInfo.reset(new CacheInfo(*ioctlHelper, getL2CacheReservationLimits(), getL3CacheReservationLimits()));

    if (debugManager.flags.ForceStaticL2ClosReservation.get()) {
        [[maybe_unused]] bool isReserved{this->cacheInfo->getCacheRegion(getL2CacheReservationLimits().maxSize, CacheRegion::region3)};
        DEBUG_BREAK_IF(!isReserved);
    }
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

int Drm::waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags, bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) {
    return ioctlHelper->waitUserFence(ctxId, address, value, static_cast<uint32_t>(dataWidth), timeout, flags, userInterrupt, externalInterruptId, allocForInterruptWait);
}

bool Drm::querySystemInfo() {
    if (systemInfoQueried) {
        return this->systemInfo != nullptr;
    }
    systemInfoQueried = true;
    auto request = ioctlHelper->getDrmParamValue(DrmParam::queryHwconfigTable);
    auto deviceBlobQuery = this->query<uint32_t>(request, 0);
    if (deviceBlobQuery.empty()) {
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stdout, "%s", "INFO: System Info query failed!\n");
        return false;
    }
    this->systemInfo.reset(new SystemInfo(deviceBlobQuery));
    return true;
}

std::vector<uint64_t> Drm::getMemoryRegions() {
    auto request = ioctlHelper->getDrmParamValue(DrmParam::queryMemoryRegions);
    return this->query<uint64_t>(request, 0);
}

bool Drm::queryMemoryInfo() {
    UNRECOVERABLE_IF(memoryInfoQueried);
    this->memoryInfo = ioctlHelper->createMemoryInfo();
    memoryInfoQueried = true;
    return this->memoryInfo != nullptr;
}

bool Drm::queryEngineInfo(bool isSysmanEnabled) {
    this->engineInfo = ioctlHelper->createEngineInfo(isSysmanEnabled);
    if (this->engineInfo && (this->engineInfo->hasEngines() == false)) {
        printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Engine info size is equal to 0.\n");
    }

    return this->engineInfo != nullptr;
}

bool Drm::completionFenceSupport() {
    std::call_once(checkCompletionFenceOnce, [this]() {
        const bool vmBindAvailable = isVmBindAvailable();
        bool support = ioctlHelper->completionFenceExtensionSupported(vmBindAvailable);
        int32_t overrideCompletionFence = debugManager.flags.EnableDrmCompletionFence.get();
        if (overrideCompletionFence != -1) {
            support = !!overrideCompletionFence;
        }

        completionFenceSupported = support;
        if (debugManager.flags.PrintCompletionFenceUsage.get()) {
            std::cout << "Completion fence supported: " << completionFenceSupported << std::endl;
        }
    });
    return completionFenceSupported;
}

void Drm::setupIoctlHelper(const PRODUCT_FAMILY productFamily) {
    if (!this->ioctlHelper) {
        auto drmVersion = Drm::getDrmVersion(getFileDescriptor());
        auto productSpecificIoctlHelperCreator = ioctlHelperFactory[productFamily];
        if (productSpecificIoctlHelperCreator && !debugManager.flags.IgnoreProductSpecificIoctlHelper.get()) {
            this->ioctlHelper = productSpecificIoctlHelperCreator.value()(*this);
        } else if ("xe" == drmVersion) {
            this->ioctlHelper = IoctlHelperXe::create(*this);
        } else {
            std::string prelimVersion = "";
            getPrelimVersion(prelimVersion);
            this->ioctlHelper = IoctlHelper::getI915Helper(productFamily, prelimVersion, *this);
        }
        this->ioctlHelper->initialize();
    }
}

bool Drm::queryTopology(const HardwareInfo &hwInfo, DrmQueryTopologyData &topologyData) {
    UNRECOVERABLE_IF(!systemInfoQueried);
    UNRECOVERABLE_IF(!engineInfoQueried);
    UNRECOVERABLE_IF(topologyQueried);
    topologyQueried = true;
    auto result = this->ioctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    return result;
}

void Drm::queryPageFaultSupport() {
    const auto &productHelper = this->getRootDeviceEnvironment().getHelper<ProductHelper>();
    if (!productHelper.isPageFaultSupported()) {
        return;
    }

    pageFaultSupported = this->ioctlHelper->isPageFaultSupported();
}

bool Drm::hasPageFaultSupport() const {
    if (debugManager.flags.EnableRecoverablePageFaults.get() != -1) {
        return !!debugManager.flags.EnableRecoverablePageFaults.get();
    }

    return pageFaultSupported;
}

bool Drm::hasKmdMigrationSupport() const {
    const auto &productHelper = this->getRootDeviceEnvironment().getHelper<ProductHelper>();
    auto kmdMigrationSupported = hasPageFaultSupport() && productHelper.isKmdMigrationSupported();

    if (debugManager.flags.UseKmdMigration.get() != -1) {
        return !!debugManager.flags.UseKmdMigration.get();
    }

    return kmdMigrationSupported;
}

void Drm::configureScratchPagePolicy() {
    if (debugManager.flags.DisableScratchPages.get() != -1) {
        disableScratch = !!debugManager.flags.DisableScratchPages.get();
        return;
    }
    const auto &productHelper = this->getRootDeviceEnvironment().getHelper<ProductHelper>();
    if (rootDeviceEnvironment.executionEnvironment.isDebuggingEnabled()) {
        disableScratch = productHelper.isDisableScratchPagesRequiredForDebugger();
    } else {
        disableScratch = productHelper.isDisableScratchPagesSupported();
    }
}

void Drm::configureGpuFaultCheckThreshold() {
    if (debugManager.flags.GpuFaultCheckThreshold.get() != -1) {
        gpuFaultCheckThreshold = debugManager.flags.GpuFaultCheckThreshold.get();
    }
}

unsigned int Drm::bindDrmContext(uint32_t drmContextId, uint32_t deviceIndex, aub_stream::EngineType engineType) {
    auto engineInfo = this->engineInfo.get();

    auto retVal = static_cast<unsigned int>(ioctlHelper->getDrmParamValue(DrmEngineMapper::engineNodeMap(engineType)));

    if (!engineInfo) {
        return retVal;
    }
    auto engine = engineInfo->getEngineInstance(deviceIndex, engineType);
    if (!engine) {
        return retVal;
    }

    bool useVirtualEnginesForCcs = true;
    if (debugManager.flags.UseDrmVirtualEnginesForCcs.get() != -1) {
        useVirtualEnginesForCcs = !!debugManager.flags.UseDrmVirtualEnginesForCcs.get();
    }

    auto numberOfCCS = rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
    constexpr uint32_t maxEngines = 9u;

    bool useVirtualEnginesForBcs = EngineHelpers::isBcsVirtualEngineEnabled(engineType);
    auto numberOfBCS = rootDeviceEnvironment.getHardwareInfo()->featureTable.ftrBcsInfo.count();

    if (debugManager.flags.LimitEngineCountForVirtualBcs.get() != -1) {
        numberOfBCS = debugManager.flags.LimitEngineCountForVirtualBcs.get();
    }

    if (debugManager.flags.LimitEngineCountForVirtualCcs.get() != -1) {
        numberOfCCS = debugManager.flags.LimitEngineCountForVirtualCcs.get();
    }

    uint32_t numEnginesInContext = 1;

    ContextParamEngines<> contextEngines{};
    ContextEnginesLoadBalance<maxEngines> balancer{};

    ioctlHelper->insertEngineToContextParams(contextEngines, 0u, engine, deviceIndex, false);

    bool setupVirtualEngines = false;
    unsigned int engineCount = static_cast<unsigned int>(numberOfCCS);
    if (useVirtualEnginesForCcs && engine->engineClass == ioctlHelper->getDrmParamValue(DrmParam::engineClassCompute) && numberOfCCS > 1u) {
        numEnginesInContext = numberOfCCS + 1;
        balancer.numSiblings = numberOfCCS;
        setupVirtualEngines = true;
    }

    bool includeMainCopyEngineInGroup = false;
    if (useVirtualEnginesForBcs && engine->engineClass == ioctlHelper->getDrmParamValue(DrmParam::engineClassCopy) && numberOfBCS > 1u) {
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
        balancer.base.name = ioctlHelper->getDrmParamValue(DrmParam::contextEnginesExtLoadBalance);
        contextEngines.extensions = castToUint64(&balancer);
        ioctlHelper->insertEngineToContextParams(contextEngines, 0u, nullptr, deviceIndex, true);

        for (auto engineIndex = 0u; engineIndex < engineCount; engineIndex++) {
            if (useVirtualEnginesForBcs && engine->engineClass == ioctlHelper->getDrmParamValue(DrmParam::engineClassCopy)) {
                auto mappedBcsEngineType = static_cast<aub_stream::EngineType>(EngineHelpers::mapBcsIndexToEngineType(engineIndex, includeMainCopyEngineInGroup));
                bool isBcsEnabled = rootDeviceEnvironment.getHardwareInfo()->featureTable.ftrBcsInfo.test(EngineHelpers::getBcsIndex(mappedBcsEngineType));

                if (!isBcsEnabled) {
                    continue;
                }

                engine = engineInfo->getEngineInstance(deviceIndex, mappedBcsEngineType);
            }
            UNRECOVERABLE_IF(!engine);

            if (useVirtualEnginesForCcs && engine->engineClass == ioctlHelper->getDrmParamValue(DrmParam::engineClassCompute)) {
                engine = engineInfo->getEngineInstance(deviceIndex, static_cast<aub_stream::EngineType>(EngineHelpers::mapCcsIndexToEngineType(engineIndex)));
            }
            UNRECOVERABLE_IF(!engine);
            balancer.engines[engineIndex] = {engine->engineClass, engine->engineInstance};
            ioctlHelper->insertEngineToContextParams(contextEngines, engineIndex, engine, deviceIndex, true);
        }
    }

    GemContextParam param{};
    param.contextId = drmContextId;
    param.size = static_cast<uint32_t>(ptrDiff(contextEngines.enginesData, &contextEngines) + sizeof(EngineClassInstance) * numEnginesInContext);
    param.param = ioctlHelper->getDrmParamValue(DrmParam::contextParamEngines);
    param.value = castToUint64(&contextEngines);

    auto ioctlValue = ioctlHelper->ioctl(DrmIoctl::gemContextSetparam, &param);
    UNRECOVERABLE_IF(ioctlValue != 0);

    retVal = static_cast<unsigned int>(ioctlHelper->getDrmParamValue(DrmParam::execDefault));
    return retVal;
}

void Drm::waitForBind(uint32_t vmHandleId) {

    auto fenceAddressAndValToWait = getFenceAddressAndValToWait(vmHandleId, false);

    const auto fenceAddressToWait = fenceAddressAndValToWait.first;
    const auto fenceValToWait = fenceAddressAndValToWait.second;

    if (fenceAddressToWait != 0u) {
        waitUserFence(0u, fenceAddressToWait, fenceValToWait, ValueWidth::u64, -1, ioctlHelper->getWaitUserFenceSoftFlag(), false, NEO::InterruptId::notUsed, nullptr);
    }
}

std::pair<uint64_t, uint64_t> Drm::getFenceAddressAndValToWait(uint32_t vmHandleId, bool isLocked) {

    std::pair<uint64_t, uint64_t> fenceAddressAndValToWait = std::make_pair(0, 0);
    std::unique_lock<std::mutex> lock;

    if (!isLocked) {
        lock = this->lockBindFenceMutex();
    }

    if (!(*ioctlHelper->getPagingFenceAddress(vmHandleId, nullptr) >= fenceVal[vmHandleId])) {

        auto fenceAddress = castToUint64(ioctlHelper->getPagingFenceAddress(vmHandleId, nullptr));
        auto fenceValue = this->fenceVal[vmHandleId];
        fenceAddressAndValToWait = std::make_pair(fenceAddress, fenceValue);
    }

    if (!isLocked) {
        lock.unlock();
    }

    return fenceAddressAndValToWait;
}

bool Drm::isSetPairAvailable() {
    if (debugManager.flags.EnableSetPair.get() == 1) {
        std::call_once(checkSetPairOnce, [this]() {
            int ret = ioctlHelper->isSetPairAvailable();
            setPairAvailable = ret;
        });
    }

    return setPairAvailable;
}

bool Drm::isChunkingAvailable() {
    if (debugManager.flags.EnableBOChunking.get() != 0) {
        std::call_once(checkChunkingOnce, [this]() {
            int ret = ioctlHelper->isChunkingAvailable();
            if (ret) {
                if (debugManager.flags.EnableBOChunking.get() == -1) {
                    chunkingMode = chunkingModeDevice;
                } else {
                    chunkingMode = debugManager.flags.EnableBOChunking.get();
                    if (!(hasKmdMigrationSupport())) {
                        chunkingMode &= (~(chunkingModeShared));
                    }
                }
            }
            if (chunkingMode > 0) {
                chunkingAvailable = true;
            }
            if (debugManager.flags.MinimalAllocationSizeForChunking.get() != -1) {
                minimalChunkingSize = debugManager.flags.MinimalAllocationSizeForChunking.get();
            }

            printDebugString(debugManager.flags.PrintBOChunkingLogs.get(), stdout,
                             "Chunking available: %d; enabled for: shared allocations %d, device allocations %d; minimalChunkingSize: %zd\n",
                             chunkingAvailable,
                             (chunkingMode & chunkingModeShared),
                             (chunkingMode & chunkingModeDevice),
                             minimalChunkingSize);
        });
    }
    return chunkingAvailable;
}

bool Drm::isVmBindAvailable() {
    std::call_once(checkBindOnce, [this]() {
        bindAvailable = ioctlHelper->isVmBindAvailable();

        Drm::overrideBindSupport(bindAvailable);

        queryAndSetVmBindPatIndexProgrammingSupport();
    });

    return bindAvailable;
}

uint64_t Drm::getPatIndex(Gmm *gmm, AllocationType allocationType, CacheRegion cacheRegion, CachePolicy cachePolicy, bool closEnabled, bool isSystemMemory) const {
    if ((debugManager.flags.OverridePatIndexForSystemMemory.get() != -1) && isSystemMemory) {
        return static_cast<uint64_t>(debugManager.flags.OverridePatIndexForSystemMemory.get());
    }

    if ((debugManager.flags.OverridePatIndexForDeviceMemory.get() != -1) && !isSystemMemory) {
        return static_cast<uint64_t>(debugManager.flags.OverridePatIndexForDeviceMemory.get());
    }

    if (debugManager.flags.OverridePatIndex.get() != -1) {
        return static_cast<uint64_t>(debugManager.flags.OverridePatIndex.get());
    }

    auto &productHelper = rootDeviceEnvironment.getProductHelper();
    GMM_RESOURCE_USAGE_TYPE usageType = CacheSettingsHelper::getGmmUsageType(allocationType, false, productHelper, getHardwareInfo());
    auto isUncachedType = CacheSettingsHelper::isUncachedType(usageType);

    if (isUncachedType && debugManager.flags.OverridePatIndexForUncachedTypes.get() != -1) {
        return static_cast<uint64_t>(debugManager.flags.OverridePatIndexForUncachedTypes.get());
    }

    if (!isUncachedType && debugManager.flags.OverridePatIndexForCachedTypes.get() != -1) {
        return static_cast<uint64_t>(debugManager.flags.OverridePatIndexForCachedTypes.get());
    }

    if (!this->vmBindPatIndexProgrammingSupported) {
        return CommonConstants::unsupportedPatIndex;
    }

    GMM_RESOURCE_INFO *resourceInfo = nullptr;
    bool cacheable = !CacheSettingsHelper::isUncachedType(usageType);
    bool compressed = false;

    if (gmm) {
        resourceInfo = gmm->gmmResourceInfo->peekGmmResourceInfo();
        usageType = gmm->resourceParams.Usage;
        compressed = gmm->isCompressionEnabled();
        cacheable = gmm->gmmResourceInfo->getResourceFlags()->Info.Cacheable;
    }

    uint64_t patIndex = rootDeviceEnvironment.getGmmClientContext()->cachePolicyGetPATIndex(resourceInfo, usageType, compressed, cacheable);
    patIndex = productHelper.overridePatIndex(isUncachedType, patIndex, allocationType);

    UNRECOVERABLE_IF(patIndex == static_cast<uint64_t>(GMM_PAT_ERROR));

    if (debugManager.flags.ClosEnabled.get() != -1) {
        closEnabled = !!debugManager.flags.ClosEnabled.get();
    }

    if (closEnabled) {
        patIndex = productHelper.getPatIndex(cacheRegion, cachePolicy);
    }

    return patIndex;
}

void programUserFence(Drm *drm, OsContext *osContext, BufferObject *bo, VmBindExtUserFenceT &vmBindExtUserFence, uint32_t vmHandleId, uint64_t nextExtension) {
    auto ioctlHelper = drm->getIoctlHelper();
    uint64_t address = 0;
    uint64_t value = 0;

    if (drm->isPerContextVMRequired()) {
        auto osContextLinux = static_cast<OsContextLinux *>(osContext);
        address = castToUint64(ioctlHelper->getPagingFenceAddress(vmHandleId, osContextLinux));
        value = osContextLinux->getNextFenceVal(vmHandleId);
    } else {
        address = castToUint64(ioctlHelper->getPagingFenceAddress(vmHandleId, nullptr));
        value = drm->getNextFenceVal(vmHandleId);
    }

    ioctlHelper->fillVmBindExtUserFence(vmBindExtUserFence, address, value, nextExtension);
}

int changeBufferObjectBinding(Drm *drm, OsContext *osContext, uint32_t vmHandleId, BufferObject *bo, bool bind, const bool forcePagingFence) {
    auto vmId = drm->getVirtualMemoryAddressSpace(vmHandleId);
    auto ioctlHelper = drm->getIoctlHelper();

    uint64_t flags = 0u;

    if (drm->isPerContextVMRequired()) {
        auto osContextLinux = static_cast<const OsContextLinux *>(osContext);
        UNRECOVERABLE_IF(osContextLinux->getDrmVmIds().size() <= vmHandleId);
        vmId = osContextLinux->getDrmVmIds()[vmHandleId];
    }

    // Use only when debugger is disabled
    auto debuggingEnabled = drm->getRootDeviceEnvironment().executionEnvironment.isDebuggingEnabled();
    const bool guaranteePagingFence = forcePagingFence && !debuggingEnabled;

    std::unique_ptr<uint8_t[]> extensions;
    if (bind) {
        bool allowUUIDsForDebug = !osContext->isInternalEngine() && !EngineHelpers::isBcs(osContext->getEngineType());
        if (bo->getBindExtHandles().size() > 0 && allowUUIDsForDebug) {
            extensions = ioctlHelper->prepareVmBindExt(bo->getBindExtHandles(), bo->getRegisteredBindHandleCookie());
        }
        bool bindCapture = debuggingEnabled && bo->isMarkedForCapture();
        bool bindImmediate = bo->isImmediateBindingRequired();
        bool bindMakeResident = false;
        bool readOnlyResource = bo->isReadOnlyGpuResource();

        if (drm->useVMBindImmediate() || guaranteePagingFence) {
            bindMakeResident = bo->isExplicitResidencyRequired();
            bindImmediate = true;
        }
        bool bindLock = bo->isExplicitLockedMemoryRequired();
        flags |= ioctlHelper->getFlagsForVmBind(bindCapture, bindImmediate, bindMakeResident, bindLock, readOnlyResource);
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
        vmBind.userptr = bo->getUserptr();
        vmBind.sharedSystemUsmEnabled = drm->isSharedSystemAllocEnabled();
        vmBind.sharedSystemUsmBind = false;

        if (bo->getColourWithBind()) {
            vmBind.length = bo->getColourChunk();
            vmBind.offset = bo->getColourChunk() * i;
            vmBind.start = bindAddresses[i];
        }

        VmBindExtSetPatT vmBindExtSetPat{};

        if (drm->isVmBindPatIndexProgrammingSupported()) {
            UNRECOVERABLE_IF(bo->peekPatIndex() == CommonConstants::unsupportedPatIndex);
            if (ioctlHelper->isVmBindPatIndexExtSupported()) {
                ioctlHelper->fillVmBindExtSetPat(vmBindExtSetPat, bo->peekPatIndex(), castToUint64(extensions.get()));
                vmBind.extensions = castToUint64(vmBindExtSetPat);
            } else {
                vmBind.extensions = castToUint64(extensions.get());
            }
            vmBind.patIndex = bo->peekPatIndex();
        } else {
            vmBind.extensions = castToUint64(extensions.get());
        }

        std::unique_lock<std::mutex> lock;
        VmBindExtUserFenceT vmBindExtUserFence{};
        bool incrementFenceValue = false;

        if ((ioctlHelper->isWaitBeforeBindRequired(bind) && drm->useVMBindImmediate()) || guaranteePagingFence) {

            lock = drm->lockBindFenceMutex();
            auto nextExtension = vmBind.extensions;
            incrementFenceValue = true;
            programUserFence(drm, osContext, bo, vmBindExtUserFence, vmHandleId, nextExtension);
            ioctlHelper->setVmBindUserFence(vmBind, vmBindExtUserFence);
        }

        if (bind) {
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

        if (incrementFenceValue) {

            auto osContextLinux = static_cast<OsContextLinux *>(osContext);
            std::pair<uint64_t, uint64_t> fenceAddressAndValToWait = osContextLinux->getFenceAddressAndValToWait(vmHandleId, true);
            if (drm->isPerContextVMRequired()) {
                osContextLinux->incFenceVal(vmHandleId);
            } else {
                drm->incFenceVal(vmHandleId);
            }

            lock.unlock();

            const auto fenceAddressToWait = fenceAddressAndValToWait.first;
            const auto fenceValToWait = fenceAddressAndValToWait.second;

            if (fenceAddressToWait != 0u) {

                bool waitOnUserFenceAfterBindAndUnbind = false;
                if (debugManager.flags.EnableWaitOnUserFenceAfterBindAndUnbind.get() != -1) {
                    waitOnUserFenceAfterBindAndUnbind = !!debugManager.flags.EnableWaitOnUserFenceAfterBindAndUnbind.get();
                }

                if ((ioctlHelper->isWaitBeforeBindRequired(bind) && waitOnUserFenceAfterBindAndUnbind && drm->useVMBindImmediate()) || guaranteePagingFence) {

                    drm->waitUserFence(0u, fenceAddressToWait, fenceValToWait, Drm::ValueWidth::u64, -1, ioctlHelper->getWaitUserFenceSoftFlag(), false, NEO::InterruptId::notUsed, nullptr);
                }
            }
        }
    }

    return ret;
}

int Drm::bindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo, const bool forcePagingFence) {
    auto ret = changeBufferObjectBinding(this, osContext, vmHandleId, bo, true, forcePagingFence);
    if (ret != 0) {
        static_cast<DrmMemoryOperationsHandlerBind *>(this->rootDeviceEnvironment.memoryOperationsInterface.get())->evictUnusedAllocations(false, false);
        ret = changeBufferObjectBinding(this, osContext, vmHandleId, bo, true, forcePagingFence);
    }
    return ret;
}

int Drm::unbindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo) {
    return changeBufferObjectBinding(this, osContext, vmHandleId, bo, false, false);
}

int Drm::createDrmVirtualMemory(uint32_t &drmVmId) {
    GemVmControl ctl{};

    std::optional<MemoryClassInstance> regionInstanceClass;

    uint32_t memoryBank = 1 << drmVmId;

    auto hwInfo = this->getRootDeviceEnvironment().getHardwareInfo();
    auto memInfo = this->getMemoryInfo();
    if (debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.get() != 0) {
        if (memInfo && rootDeviceEnvironment.getHelper<GfxCoreHelper>().getEnableLocalMemory(*hwInfo)) {
            regionInstanceClass = memInfo->getMemoryRegionClassAndInstance(memoryBank, *this->rootDeviceEnvironment.getHardwareInfo());
        }
    }

    auto vmControlExtRegion = ioctlHelper->createVmControlExtRegion(regionInstanceClass);

    if (vmControlExtRegion) {
        ctl.extensions = castToUint64(vmControlExtRegion.get());
    }

    bool useVmBind = isVmBindAvailable();
    bool enablePageFault = hasPageFaultSupport() && useVmBind;

    ctl.flags = ioctlHelper->getFlagsForVmCreate(checkToDisableScratchPage(), enablePageFault, useVmBind);

    auto ret = ioctlHelper->ioctl(DrmIoctl::gemVmCreate, &ctl);

    if (ret == 0) {
        drmVmId = ctl.vmId;
        if (isSharedSystemAllocEnabled()) {
            auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
            VmBindParams vmBind{};
            vmBind.vmId = static_cast<uint32_t>(ctl.vmId);
            vmBind.flags = DRM_XE_VM_BIND_FLAG_CPU_ADDR_MIRROR;
            vmBind.length = this->getSharedSystemAllocAddressRange();
            vmBind.sharedSystemUsmEnabled = true;
            vmBind.sharedSystemUsmBind = true;
            vmBind.patIndex = productHelper.getSharedSystemPatIndex();
            VmBindExtUserFenceT vmBindExtUserFence{};
            ioctlHelper->fillVmBindExtUserFence(vmBindExtUserFence,
                                                castToUint64(ioctlHelper->getPagingFenceAddress(0, nullptr)),
                                                getNextFenceVal(0),
                                                vmBind.extensions);
            ioctlHelper->setVmBindUserFence(vmBind, vmBindExtUserFence);

            if (ioctlHelper->vmBind(vmBind)) {
                setSharedSystemAllocEnable(false);
                printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr,
                                 "INFO:  Shared System USM capability not detected\n");
            }
        }
        if (ctl.vmId == 0) {
            // 0 is reserved for invalid/unassigned ppgtt
            return -1;
        }
    } else {
        printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr,
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

int Drm::waitOnUserFences(OsContextLinux &osContext, uint64_t address, uint64_t value, uint32_t numActiveTiles, int64_t timeout, uint32_t postSyncOffset, bool userInterrupt,
                          uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) {

    int ret = waitOnUserFencesImpl(static_cast<const OsContextLinux &>(osContext), address, value, numActiveTiles,
                                   timeout, postSyncOffset, userInterrupt, externalInterruptId, allocForInterruptWait);
    if (ret != 0 && getErrno() == EIO && checkGpuPageFaultRequired()) {
        checkResetStatus(osContext);
    }
    return ret;
}
int Drm::waitOnUserFencesImpl(const OsContextLinux &osContext, uint64_t address, uint64_t value, uint32_t numActiveTiles, int64_t timeout, uint32_t postSyncOffset, bool userInterrupt,
                              uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) {
    auto &drmContextIds = osContext.getDrmContextIds();
    UNRECOVERABLE_IF(numActiveTiles > drmContextIds.size());
    auto completionFenceCpuAddress = address;
    const auto selectedTimeout = osContext.isHangDetected() ? 1 : timeout;

    for (auto drmIterator = 0u; drmIterator < numActiveTiles; drmIterator++) {
        if (*reinterpret_cast<uint32_t *>(completionFenceCpuAddress) < value) {
            static constexpr uint16_t flags = 0;
            int retVal = waitUserFence(drmContextIds[drmIterator], completionFenceCpuAddress, value, Drm::ValueWidth::u64, selectedTimeout, flags, userInterrupt, externalInterruptId, allocForInterruptWait);
            if (debugManager.flags.PrintCompletionFenceUsage.get()) {
                std::cout << "Completion fence waited."
                          << " Status: " << retVal
                          << ", CPU address: " << std::hex << completionFenceCpuAddress << std::dec
                          << ", current value: " << *reinterpret_cast<uint32_t *>(completionFenceCpuAddress)
                          << ", wait value: " << value << std::endl;
            }
            if (retVal != 0) {
                return retVal;
            }
        } else if (debugManager.flags.PrintCompletionFenceUsage.get()) {
            std::cout << "Completion fence already completed."
                      << " CPU address: " << std::hex << completionFenceCpuAddress << std::dec
                      << ", current value: " << *reinterpret_cast<uint32_t *>(completionFenceCpuAddress)
                      << ", wait value: " << value << std::endl;
        }

        if (externalInterruptId != NEO::InterruptId::notUsed) {
            break;
        }

        completionFenceCpuAddress = ptrOffset(completionFenceCpuAddress, postSyncOffset);
    }

    return 0;
}
const HardwareInfo *Drm::getHardwareInfo() const { return rootDeviceEnvironment.getHardwareInfo(); }

uint64_t Drm::alignUpGttSize(uint64_t inputGttSize) {

    constexpr uint64_t gttSize47bit = (1ull << 47);
    constexpr uint64_t gttSize48bit = (1ull << 48);

    if (inputGttSize > gttSize47bit && inputGttSize < gttSize48bit) {
        return gttSize48bit;
    }
    return inputGttSize;
}

bool Drm::isDrmSupported(int fileDescriptor) {
    auto drmVersion = Drm::getDrmVersion(fileDescriptor);
    return "i915" == drmVersion || "xe" == drmVersion;
}

bool Drm::queryDeviceIdAndRevision() {
    auto drmVersion = Drm::getDrmVersion(getFileDescriptor());
    if ("xe" == drmVersion) {
        this->setPerContextVMRequired(false);
        return IoctlHelperXe::queryDeviceIdAndRevision(*this);
    }
    return IoctlHelperI915::queryDeviceIdAndRevision(*this);
}

void Drm::adjustSharedSystemMemCapabilities() {
    if (this->isSharedSystemAllocEnabled()) {
        uint64_t gpuAddressLength = (this->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.gpuAddressSpace + 1);
        uint64_t cpuAddressLength = (0x1ull << (NEO::CpuInfo::getInstance().getVirtualAddressSize()));
        if ((cpuAddressLength > (maxNBitValue(48) + 1)) && !(CpuInfo::getInstance().isCpuFlagPresent("la57"))) {
            cpuAddressLength = (maxNBitValue(48) + 1);
        }
        if (gpuAddressLength < cpuAddressLength) {
            this->setSharedSystemAllocEnable(false);
            this->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = 0;
            printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "Shared System USM NOT allowed: CPU address range > GPU address range\n");
        } else {
            this->setSharedSystemAllocAddressRange(cpuAddressLength);
            this->setPageFaultSupported(true);
        }
    } else {
        this->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = 0;
    }
}

uint32_t Drm::getAggregatedProcessCount() const {
    return ioctlHelper->getNumProcesses();
}

uint32_t Drm::getVmIdForContext(OsContext &osContext, uint32_t vmHandleId) const {
    auto osContextLinux = static_cast<const OsContextLinux *>(&osContext);
    return osContextLinux->getDrmVmIds().size() > 0 ? osContextLinux->getDrmVmIds()[vmHandleId] : getVirtualMemoryAddressSpace(vmHandleId);
}

template std::vector<uint16_t> Drm::query<uint16_t>(uint32_t queryId, uint32_t queryItemFlags);
template std::vector<uint32_t> Drm::query<uint32_t>(uint32_t queryId, uint32_t queryItemFlags);
template std::vector<uint64_t> Drm::query<uint64_t>(uint32_t queryId, uint32_t queryItemFlags);
} // namespace NEO
