/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_os_layer.h"

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/i915_prelim.h"

#include <cassert>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

int (*openFunc)(const char *pathname, int flags, ...) = nullptr;
int (*openFull)(const char *pathname, int flags, ...) = nullptr;

int fakeFd = 1023;
int haveDri = 0;                                       // index of dri to serve, -1 - none
int deviceId = NEO::deviceDescriptorTable[0].deviceId; // default supported DeviceID
int revisionId = 17;
int havePreemption = I915_SCHEDULER_CAP_ENABLED |
                     I915_SCHEDULER_CAP_PRIORITY |
                     I915_SCHEDULER_CAP_PREEMPTION;
int vmId = 0;
int failOnDeviceId = 0;
int failOnEuTotal = 0;
int failOnSubsliceTotal = 0;
int failOnRevisionId = 0;
int failOnParamBoost = 0;
int failOnSetParamSseu = 0;
int failOnGetParamSseu = 0;
int failOnContextCreate = 0;
int failOnVirtualMemoryCreate = 0;
int failOnSetPriority = 0;
int failOnPreemption = 0;
int failOnDrmVersion = 0;
int captureVirtualMemoryCreate = 0;
int accessCalledTimes = 0;
int readLinkCalledTimes = 0;
int fstatCalledTimes = 0;
bool forceExtraIoctlDuration = 0;
char providedDrmVersion[5] = {'i', '9', '1', '5', '\0'};
uint64_t gpuTimestamp = 0;
int ioctlSeq[8] = {0, 0, 0, 0, 0, 0, 0, 0};
size_t ioctlCnt = 0;
std::vector<NEO::GemVmControl> capturedVmCreate{};

int fstat(int fd, struct stat *buf) {
    ++fstatCalledTimes;
    buf->st_rdev = 0x0;
    return 0;
}

int access(const char *pathname, int mode) {
    ++accessCalledTimes;
    return 0;
}

ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
    ++readLinkCalledTimes;

    if (readLinkCalledTimes % 2 == 1) {
        return -1;
    }

    constexpr size_t sizeofPath = sizeof("../../devices/pci0000:4a/0000:4a:02.0/0000:4b:00.0/0000:4c:01.0/0000:00:03.0/drm/renderD128");

    strcpy_s(buf, sizeofPath, "../../devices/pci0000:4a/0000:4a:02.0/0000:4b:00.0/0000:4c:01.0/0000:00:03.0/drm/renderD128");

    return sizeofPath;
}

int open(const char *pathname, int flags, ...) {
    if (openFull != nullptr) {
        return openFull(pathname, flags);
    }
    if (openFunc == nullptr) {
        openFunc = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, "open");
    }

    if (strncmp("/dev/dri/", pathname, 9) == 0) {
        if (haveDri >= 0) {
            return fakeFd;
        } else {
            return -1;
        }
    }

    return openFunc(pathname, flags);
}
bool failOnOpenDir = false;
DIR *validDir = reinterpret_cast<DIR *>(0xc001);
DIR *opendir(const char *name) {
    if (failOnOpenDir) {
        return nullptr;
    }
    return validDir;
}
int closedir(DIR *dirp) {
    return 0u;
}

struct dirent entries[] = {
    {0, 0, 0, 0, "."},
    {0, 0, 0, 0, "pci-0000:00:03.1-render"},
    {0, 0, 0, 0, "platform-4010000000.pcie-pci-0000:00:02.0-render"},
    {0, 0, 0, 0, "pci-0000:test1-render"},
    {0, 0, 0, 0, "pci-0000:test2-render"},
    {0, 0, 0, 0, "pci-0000:1234-render"},
    {0, 0, 0, 0, "pci-0000:3:0.0-render"},
    {0, 0, 0, 0, "pci-0a00:00:03.1-render"},
    {0, 0, 0, 0, "pci-0000:b3:03.1-render"},
    {0, 0, 0, 0, "pci-0000:00:b3.1-render"},
    {0, 0, 0, 0, "pci-0000:00:03.a-render"},
    {0, 0, 0, 0, "pci-0000:00:03.a-render-12"},
    {0, 0, 0, 0, "pcii0000:00:03.a-render"},
    {0, 0, 0, 0, "pcii-render"},
};

uint32_t entryIndex = 0u;
const uint32_t numEntries = sizeof(entries) / sizeof(entries[0]);

struct dirent *readdir(DIR *dir) {
    if (entryIndex >= numEntries) {
        entryIndex = 0;
        return nullptr;
    }
    return &entries[entryIndex++];
}

int drmGetParam(NEO::GetParam *param) {
    assert(param);
    int ret = 0;

    switch (param->param) {
    case I915_PARAM_CHIPSET_ID:
        *param->value = deviceId;
        ret = failOnDeviceId;
        break;
    case I915_PARAM_EU_TOTAL:
        *param->value = 3;
        ret = failOnEuTotal;
        break;
    case I915_PARAM_SUBSLICE_TOTAL:
        *param->value = 1;
        ret = failOnSubsliceTotal;
        break;
    case I915_PARAM_REVISION:
        *param->value = revisionId;
        ret = failOnRevisionId;
        break;
#if defined(I915_PARAM_HAS_SCHEDULER)
    case I915_PARAM_HAS_SCHEDULER:
        *param->value = havePreemption;
        ret = failOnPreemption;
        break;
#endif
    default:
        ret = -1;
        break;
    }
    return ret;
}

int drmSetContextParam(NEO::GemContextParam *param) {
    assert(param);
    int ret = 0;

    switch (param->param) {
    case NEO::contextPrivateParamBoost:
        ret = failOnParamBoost;
        break;
    case I915_CONTEXT_PARAM_VM:
        break;
#if defined(I915_PARAM_HAS_SCHEDULER)
    case I915_CONTEXT_PARAM_PRIORITY:
        ret = failOnSetPriority;
        break;
#endif
    case I915_CONTEXT_PARAM_SSEU:
        if (param->size == sizeof(NEO::GemContextParamSseu) && param->value != 0 && param->contextId == 0) {
            ret = failOnSetParamSseu;
        } else {
            ret = -1;
        }
        break;
    default:
        ret = -1;
        break;
    }
    return ret;
}
int drmGetContextParam(NEO::GemContextParam *param) {
    int ret = 0;

    switch (param->param) {
    case I915_CONTEXT_PARAM_SSEU:
        if (param->size == sizeof(NEO::GemContextParamSseu) && param->value != 0 && param->contextId == 0) {
            ret = failOnGetParamSseu;
        } else {
            ret = -1;
        }
        break;
    default:
        ret = -1;
        break;
    }
    return ret;
}

int drmContextCreate(NEO::GemContextCreateExt *create) {
    assert(create);

    create->contextId = 1;
    return failOnContextCreate;
}

int drmContextDestroy(NEO::GemContextDestroy *destroy) {
    assert(destroy);

    if (destroy->contextId == 1)
        return 0;
    else
        return -1;
}

int drmVirtualMemoryCreate(NEO::GemVmControl *control) {
    assert(control);
    if (!failOnVirtualMemoryCreate) {
        control->vmId = ++vmId;
    }
    if (captureVirtualMemoryCreate) {
        capturedVmCreate.push_back(*control);
    }
    return failOnVirtualMemoryCreate;
}

int drmVirtualMemoryDestroy(NEO::GemVmControl *control) {
    assert(control);

    vmId--;
    return (control->vmId > 0) ? 0 : -1;
}

int drmVersion(NEO::DrmVersion *version) {
    memcpy_s(version->name, version->nameLen, providedDrmVersion, strlen(providedDrmVersion) + 1);

    return failOnDrmVersion;
}

int drmQueryItem(NEO::Query *query) {
    auto queryItemArg = reinterpret_cast<NEO::QueryItem *>(query->itemsPtr);

    constexpr int sliceCount = 1;
    constexpr int subsliceCount = 1;
    constexpr int euCount = 3;
    const auto dataSize = static_cast<size_t>(Math::divideAndRoundUp(sliceCount, 8u) + Math::divideAndRoundUp(subsliceCount, 8u) + Math::divideAndRoundUp(euCount, 8u));

    if (queryItemArg->length == 0) {
        if (queryItemArg->queryId == DRM_I915_QUERY_TOPOLOGY_INFO) {
            queryItemArg->length = static_cast<int32_t>(sizeof(NEO::QueryTopologyInfo) + dataSize);
            return 0;
        }
    } else if (queryItemArg->queryId == DRM_I915_QUERY_TOPOLOGY_INFO) {
        auto topologyArg = reinterpret_cast<NEO::QueryTopologyInfo *>(queryItemArg->dataPtr);
        topologyArg->maxSlices = 1;
        topologyArg->maxSubslices = 1;
        topologyArg->maxEusPerSubslice = 3;
        topologyArg->subsliceOffset = 1;
        topologyArg->subsliceStride = 1;
        topologyArg->euOffset = 2;
        topologyArg->euStride = 1;
        memset(topologyArg->data, 0xFF, dataSize);
        return failOnEuTotal || failOnSubsliceTotal;
    }

    return drmQuery(query);
}

int ioctl(int fd, unsigned long int request, ...) throw() {
    using namespace std::chrono_literals;

    if (forceExtraIoctlDuration) {
        auto start = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point end;

        do {
            end = std::chrono::steady_clock::now();
        } while ((end - start) == 0ns);
    }

    int res;
    va_list vl;
    va_start(vl, request);
    if (fd == fakeFd) {
        res = ioctlSeq[ioctlCnt % (sizeof(ioctlSeq) / sizeof(int))];
        ioctlCnt++;

        if (res == 0) {
            switch (request) {
            case DRM_IOCTL_I915_GETPARAM:
                res = drmGetParam(va_arg(vl, NEO::GetParam *));
                break;
            case DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM:
                res = drmSetContextParam(va_arg(vl, NEO::GemContextParam *));
                break;
            case DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM:
                res = drmGetContextParam(va_arg(vl, NEO::GemContextParam *));
                break;
            case DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT:
                res = drmContextCreate(va_arg(vl, NEO::GemContextCreateExt *));
                break;
            case DRM_IOCTL_I915_GEM_CONTEXT_DESTROY:
                res = drmContextDestroy(va_arg(vl, NEO::GemContextDestroy *));
                break;
            case DRM_IOCTL_I915_GEM_VM_CREATE:
                res = drmVirtualMemoryCreate(va_arg(vl, NEO::GemVmControl *));
                break;
            case DRM_IOCTL_I915_GEM_VM_DESTROY:
                res = drmVirtualMemoryDestroy(va_arg(vl, NEO::GemVmControl *));
                break;
            case DRM_IOCTL_VERSION:
                res = drmVersion(va_arg(vl, NEO::DrmVersion *));
                break;
            case DRM_IOCTL_I915_QUERY:
                res = drmQueryItem(va_arg(vl, NEO::Query *));
                break;
            }
        }
        va_end(vl);
        return res;
    }

    va_end(vl);
    return -1;
}

namespace DrmQueryConfig {
int failOnQueryEngineInfo = 0;
int failOnQueryMemoryInfo = 0;
unsigned int retrieveQueryMemoryInfoRegionCount = 3;
} // namespace DrmQueryConfig

uint32_t getTileFromEngineOrMemoryInstance(uint16_t instanceValue) {
    uint8_t tileMask = (instanceValue >> 8);

    return Math::log2(static_cast<uint64_t>(tileMask));
}

uint16_t getEngineOrMemoryInstanceValue(uint16_t tile) {
    return ((1 << tile) << 8);
}

int drmQuery(NEO::Query *param) {
    auto expectedQueryEngineLength = static_cast<int32_t>(sizeof(drm_i915_query_engine_info) + (sizeof(drm_i915_engine_info) * DrmQueryConfig::retrieveQueryMemoryInfoRegionCount));
    assert(param);
    int ret = -1;
    int32_t requiredLength = 0u;

    for (uint32_t i = 0; i < param->numItems; i++) {
        auto itemArray = reinterpret_cast<NEO::QueryItem *>(param->itemsPtr);
        auto item = &itemArray[i];
        switch (item->queryId) {
        case DRM_I915_QUERY_ENGINE_INFO:

            if (0 == item->length) {
                item->length = expectedQueryEngineLength;
            } else {
                assert(expectedQueryEngineLength == item->length);
                auto queryEngineInfo = reinterpret_cast<drm_i915_query_engine_info *>(item->dataPtr);
                auto engineInfo = queryEngineInfo->engines;

                for (uint32_t i = 0; i < DrmQueryConfig::retrieveQueryMemoryInfoRegionCount; i++) {
                    engineInfo++->engine = {I915_ENGINE_CLASS_RENDER, getEngineOrMemoryInstanceValue(i)};
                }

                queryEngineInfo->num_engines = DrmQueryConfig::retrieveQueryMemoryInfoRegionCount;
            }
            ret = DrmQueryConfig::failOnQueryEngineInfo ? -1 : 0;
            break;
        case DRM_I915_QUERY_MEMORY_REGIONS:
            requiredLength = static_cast<int32_t>(sizeof(drm_i915_query_memory_regions) +
                                                  DrmQueryConfig::retrieveQueryMemoryInfoRegionCount * sizeof(drm_i915_memory_region_info));

            if (0 == item->length) {
                item->length = requiredLength;
            } else {
                assert(requiredLength == item->length);
                auto region = reinterpret_cast<drm_i915_query_memory_regions *>(item->dataPtr);
                region->num_regions = DrmQueryConfig::retrieveQueryMemoryInfoRegionCount;
                for (uint32_t i = 0; i < DrmQueryConfig::retrieveQueryMemoryInfoRegionCount; i++) {
                    drm_i915_memory_region_info regionInfo = {};
                    regionInfo.region.memory_class = (i == 0) ? I915_MEMORY_CLASS_SYSTEM : I915_MEMORY_CLASS_DEVICE;
                    if (i >= 1) {
                        regionInfo.region.memory_instance = getEngineOrMemoryInstanceValue(i - 1);
                    }
                    region->regions[i] = regionInfo;
                }
            }
            ret = DrmQueryConfig::failOnQueryMemoryInfo ? -1 : 0;
            break;
        case PRELIM_DRM_I915_QUERY_DISTANCE_INFO:
            auto queryDistanceInfo = reinterpret_cast<prelim_drm_i915_query_distance_info *>(item->dataPtr);
            switch (queryDistanceInfo->region.memory_class) {
            case I915_MEMORY_CLASS_SYSTEM:
                queryDistanceInfo->distance = -1;
                break;
            case I915_MEMORY_CLASS_DEVICE: {
                auto engineTile = getTileFromEngineOrMemoryInstance(queryDistanceInfo->engine.engine_instance);
                auto memoryTile = getTileFromEngineOrMemoryInstance(queryDistanceInfo->region.memory_instance);

                queryDistanceInfo->distance = (memoryTile == engineTile) ? 0 : 100;
                break;
            }
            default:
                item->length = -EINVAL;
                break;
            }
            ret = 0;
            break;
        }
    }

    return ret;
}
