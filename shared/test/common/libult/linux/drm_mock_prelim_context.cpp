/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_mock_prelim_context.h"

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/cache_info_impl.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"

#include "third_party/uapi/prelim/drm/i915_drm.h"
#include <gtest/gtest.h>

#include <cmath>
#include <errno.h>

namespace {

constexpr std::array<uint64_t, 9> copyEnginesCapsMap = {{
    PRELIM_I915_COPY_CLASS_CAP_SATURATE_LMEM,
    PRELIM_I915_COPY_CLASS_CAP_SATURATE_PCIE,
    PRELIM_I915_COPY_CLASS_CAP_SATURATE_PCIE,
    PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK,
    PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK,
    PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK,
    PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK,
    PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK,
    PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK,
}};

} // namespace

int DrmMockPrelimContext::handlePrelimRequest(unsigned long request, void *arg) {
    switch (request) {
    case DRM_IOCTL_I915_GETPARAM: {
        auto gp = static_cast<drm_i915_getparam_t *>(arg);
        if (gp->param == PRELIM_I915_PARAM_HAS_PAGE_FAULT) {
            *gp->value = hasPageFaultQueryValue;
            return hasPageFaultQueryReturn;
        } else if (gp->param == PRELIM_I915_PARAM_HAS_VM_BIND) {
            vmBindQueryCalled++;
            *gp->value = vmBindQueryValue;
            return vmBindQueryReturn;
        }
    } break;
    case DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM: {
        auto gp = static_cast<drm_i915_gem_context_param *>(arg);
        if (gp->param == PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAGS) {
            gp->value = contextDebugSupported ? PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAG_SIP << 32 : 0;
            return 0;
        }
    } break;
    case PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE: {
        auto closReserveArg = static_cast<prelim_drm_i915_gem_clos_reserve *>(arg);
        closIndex++;
        if (closIndex == 0) {
            return EINVAL;
        }
        closReserveArg->clos_index = closIndex;
    } break;
    case PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE: {
        auto closFreeArg = static_cast<prelim_drm_i915_gem_clos_free *>(arg);
        if (closFreeArg->clos_index > closIndex) {
            return EINVAL;
        }
        closIndex--;
    } break;
    case PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE: {
        auto cacheReserveArg = static_cast<prelim_drm_i915_gem_cache_reserve *>(arg);
        if (cacheReserveArg->clos_index > closIndex) {
            return EINVAL;
        }
        auto cacheInfoImpl = static_cast<const CacheInfoImpl *>(cacheInfo);
        auto maxReservationNumWays = cacheInfoImpl ? cacheInfoImpl->getMaxReservationNumWays() : maxNumWays;
        if (cacheReserveArg->num_ways > maxReservationNumWays) {
            return EINVAL;
        }
        auto freeNumWays = maxReservationNumWays - allocNumWays;
        if (cacheReserveArg->num_ways > freeNumWays) {
            return EINVAL;
        }
        allocNumWays += cacheReserveArg->num_ways;
    } break;
    case PRELIM_DRM_IOCTL_I915_GEM_VM_BIND: {
        vmBindCalled++;
        return vmBindReturn;
    } break;
    case PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND: {
        vmUnbindCalled++;
        return vmUnbindReturn;
    } break;
    case PRELIM_DRM_IOCTL_I915_UUID_REGISTER: {
        auto uuidControl = reinterpret_cast<prelim_drm_i915_uuid_control *>(arg);

        if (uuidControl->uuid_class != uint32_t(PRELIM_I915_UUID_CLASS_STRING) && uuidControl->uuid_class > uuidHandle) {
            return -1;
        }

        uuidControl->handle = uuidHandle++;

        receivedRegisterUuid = UuidControl{
            {},
            uuidControl->uuid_class,
            reinterpret_cast<void *>(uuidControl->ptr),
            uuidControl->size,
            uuidControl->handle,
            uuidControl->flags,
            uuidControl->extensions,
        };
        memcpy_s(receivedRegisterUuid->uuid, sizeof(receivedRegisterUuid->uuid), uuidControl->uuid, sizeof(uuidControl->uuid));
        return uuidControlReturn;
    } break;
    case PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER: {
        auto uuidControl = reinterpret_cast<prelim_drm_i915_uuid_control *>(arg);
        receivedUnregisterUuid = UuidControl{
            {},
            uuidControl->uuid_class,
            reinterpret_cast<void *>(uuidControl->ptr),
            uuidControl->size,
            uuidControl->handle,
            uuidControl->flags,
            uuidControl->extensions,
        };
        memcpy_s(receivedUnregisterUuid->uuid, sizeof(receivedUnregisterUuid->uuid), uuidControl->uuid, sizeof(uuidControl->uuid));
        return uuidControlReturn;
    } break;

    default:
        return -1;
    }

    return 0;
}

bool DrmMockPrelimContext::handlePrelimQueryItem(void *arg) {
    auto queryItem = reinterpret_cast<drm_i915_query_item *>(arg);

    auto &gtSystemInfo = hwInfo->gtSystemInfo;
    const auto numberOfCCS = gtSystemInfo.CCSInfo.IsValid && !disableCcsSupport ? gtSystemInfo.CCSInfo.NumberOfCCSEnabled : 0u;

    switch (queryItem->query_id) {
    case PRELIM_DRM_I915_QUERY_ENGINE_INFO: {
        auto numberOfTiles = gtSystemInfo.MultiTileArchInfo.IsValid ? gtSystemInfo.MultiTileArchInfo.TileCount : 1u;
        uint32_t numberOfEngines = numberOfTiles * (4u + numberOfCCS + static_cast<uint32_t>(supportedCopyEnginesMask.count()));
        int engineInfoSize = sizeof(prelim_drm_i915_query_engine_info) + numberOfEngines * sizeof(prelim_drm_i915_engine_info);
        if (queryItem->length == 0) {
            queryItem->length = engineInfoSize;
        } else {
            EXPECT_EQ(engineInfoSize, queryItem->length);
            auto queryEngineInfo = reinterpret_cast<prelim_drm_i915_query_engine_info *>(queryItem->data_ptr);
            EXPECT_EQ(0u, queryEngineInfo->num_engines);
            queryEngineInfo->num_engines = numberOfEngines;
            auto p = queryEngineInfo->engines;
            for (uint32_t tile = 0u; tile < numberOfTiles; tile++) {
                p++->engine = {I915_ENGINE_CLASS_RENDER, DrmMockHelper::getEngineOrMemoryInstanceValue(tile, 0)};
                for (uint32_t i = 0u; i < supportedCopyEnginesMask.size(); i++) {
                    if (supportedCopyEnginesMask.test(i)) {
                        auto copyEngineInfo = p++;
                        copyEngineInfo->engine = {I915_ENGINE_CLASS_COPY, DrmMockHelper::getEngineOrMemoryInstanceValue(tile, i)};
                        copyEngineInfo->capabilities = copyEnginesCapsMap[i];
                    }
                }
                p++->engine = {I915_ENGINE_CLASS_VIDEO, DrmMockHelper::getEngineOrMemoryInstanceValue(tile, 0)};
                p++->engine = {I915_ENGINE_CLASS_VIDEO, DrmMockHelper::getEngineOrMemoryInstanceValue(tile, 0)};
                p++->engine = {I915_ENGINE_CLASS_VIDEO_ENHANCE, DrmMockHelper::getEngineOrMemoryInstanceValue(tile, 0)};
                for (auto i = 0u; i < numberOfCCS; i++) {
                    p++->engine = {PRELIM_I915_ENGINE_CLASS_COMPUTE, DrmMockHelper::getEngineOrMemoryInstanceValue(tile, i)};
                }
            }
        }
        break;
    }

    case PRELIM_DRM_I915_QUERY_MEMORY_REGIONS: {
        auto numberOfLocalMemories = gtSystemInfo.MultiTileArchInfo.IsValid ? gtSystemInfo.MultiTileArchInfo.TileCount : 0u;
        auto numberOfRegions = 1u + numberOfLocalMemories;

        int regionInfoSize = sizeof(prelim_drm_i915_query_memory_regions) + numberOfRegions * sizeof(prelim_drm_i915_memory_region_info);
        if (queryItem->length == 0) {
            queryItem->length = regionInfoSize;
        } else {
            EXPECT_EQ(regionInfoSize, queryItem->length);
            auto queryMemoryRegionInfo = reinterpret_cast<prelim_drm_i915_query_memory_regions *>(queryItem->data_ptr);
            EXPECT_EQ(0u, queryMemoryRegionInfo->num_regions);
            queryMemoryRegionInfo->num_regions = numberOfRegions;
            queryMemoryRegionInfo->regions[0].region.memory_class = PRELIM_I915_MEMORY_CLASS_SYSTEM;
            queryMemoryRegionInfo->regions[0].region.memory_instance = 1;
            queryMemoryRegionInfo->regions[0].probed_size = 2 * MemoryConstants::gigaByte;
            for (auto tile = 0u; tile < numberOfLocalMemories; tile++) {
                queryMemoryRegionInfo->regions[1 + tile].region.memory_class = PRELIM_I915_MEMORY_CLASS_DEVICE;
                queryMemoryRegionInfo->regions[1 + tile].region.memory_instance = DrmMockHelper::getEngineOrMemoryInstanceValue(tile, 0);
                queryMemoryRegionInfo->regions[1 + tile].probed_size = 2 * MemoryConstants::gigaByte;
            }
        }
    } break;

    case PRELIM_DRM_I915_QUERY_DISTANCE_INFO: {
        if (failDistanceInfoQuery) {
            return false;
        }

        auto queryDistanceInfo = reinterpret_cast<prelim_drm_i915_query_distance_info *>(queryItem->data_ptr);
        switch (queryDistanceInfo->region.memory_class) {
        case PRELIM_I915_MEMORY_CLASS_SYSTEM:
            EXPECT_EQ(sizeof(prelim_drm_i915_query_distance_info), static_cast<size_t>(queryItem->length));
            queryDistanceInfo->distance = -1;
            break;
        case PRELIM_I915_MEMORY_CLASS_DEVICE: {
            EXPECT_EQ(sizeof(prelim_drm_i915_query_distance_info), static_cast<size_t>(queryItem->length));

            auto engineTile = DrmMockHelper::getTileFromEngineOrMemoryInstance(queryDistanceInfo->engine.engine_instance);
            auto memoryTile = DrmMockHelper::getTileFromEngineOrMemoryInstance(queryDistanceInfo->region.memory_instance);

            queryDistanceInfo->distance = (memoryTile == engineTile) ? 0 : 100;
            break;
        }
        default:
            queryItem->length = -EINVAL;
            break;
        }
    } break;

    case PRELIM_DRM_I915_QUERY_COMPUTE_SLICES: {
        auto &gtSystemInfo = rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo;
        auto maxEuPerSubslice = gtSystemInfo.MaxEuPerSubSlice;
        auto maxSlices = gtSystemInfo.MaxSlicesSupported;
        auto maxSubslices = gtSystemInfo.MaxSubSlicesSupported / maxSlices;
        auto threadsPerEu = gtSystemInfo.ThreadCount / gtSystemInfo.EUCount;
        auto realEuCount = threadsPerEu * maxEuPerSubslice * maxSubslices * maxSlices;

        auto dataSize = static_cast<size_t>(std::ceil(realEuCount / 8.0)) + maxSlices * static_cast<uint16_t>(std::ceil(maxSubslices / 8.0)) +
                        static_cast<uint16_t>(std::ceil(maxSlices / 8.0));

        if (queryItem->length == 0) {
            queryItem->length = static_cast<int32_t>(sizeof(drm_i915_query_topology_info) + dataSize);
            break;
        } else {
            auto topologyArg = reinterpret_cast<drm_i915_query_topology_info *>(queryItem->data_ptr);
            if (failRetTopology) {
                return false;
            }
            topologyArg->max_slices = maxSlices;
            topologyArg->max_subslices = maxSubslices;
            topologyArg->max_eus_per_subslice = maxEuPerSubslice;

            topologyArg->subslice_stride = static_cast<uint16_t>(std::ceil(maxSubslices / 8.0));
            topologyArg->eu_stride = static_cast<uint16_t>(std::ceil(maxEuPerSubslice / 8.0));
            topologyArg->subslice_offset = static_cast<uint16_t>(std::ceil(maxSlices / 8.0));
            topologyArg->eu_offset = static_cast<uint16_t>(std::ceil(maxSubslices / 8.0)) * maxSlices;

            int threadData = (threadsPerEu == 8) ? 0xff : 0x7f;

            uint8_t *data = topologyArg->data;
            for (uint32_t sliceId = 0; sliceId < maxSlices; sliceId++) {
                data[0] |= 1 << (sliceId % 8);
                if (sliceId == 7 || sliceId == maxSlices - 1) {
                    data++;
                }
            }

            DEBUG_BREAK_IF(ptrDiff(data, topologyArg->data) != topologyArg->subslice_offset);

            data = ptrOffset(topologyArg->data, topologyArg->subslice_offset);
            for (uint32_t sliceId = 0; sliceId < maxSlices; sliceId++) {
                for (uint32_t i = 0; i < maxSubslices; i++) {
                    data[0] |= 1 << (i % 8);

                    if (i == 7 || i == maxSubslices - 1) {
                        data++;
                    }
                }
            }

            DEBUG_BREAK_IF(ptrDiff(data, topologyArg->data) != topologyArg->eu_offset);
            auto size = dataSize - topologyArg->eu_offset;
            memset(ptrOffset(topologyArg->data, topologyArg->eu_offset), threadData, size);
        }
    } break;

    default:
        queryItem->length = -EINVAL;
        break;
    }
    return true;
}

uint32_t DrmPrelimHelper::getQueryComputeSlicesIoctl() {
    return PRELIM_DRM_I915_QUERY_COMPUTE_SLICES;
}

uint32_t DrmPrelimHelper::getDistanceInfoQueryId() {
    return PRELIM_DRM_I915_QUERY_DISTANCE_INFO;
}

uint32_t DrmPrelimHelper::getComputeEngineClass() {
    return PRELIM_I915_ENGINE_CLASS_COMPUTE;
}

uint32_t DrmPrelimHelper::getStringUuidClass() {
    return PRELIM_I915_UUID_CLASS_STRING;
}
