/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/drm_mock_extended.h"

#include "shared/test/common/libult/linux/drm_mock_helper.h"

DrmMockExtended::DrmMockExtended(RootDeviceEnvironment &rootDeviceEnvironmentIn, const HardwareInfo *inputHwInfo) : DrmMock(rootDeviceEnvironmentIn) {
    rootDeviceEnvironment.setHwInfoAndInitHelpers(inputHwInfo);
    EXPECT_TRUE(queryMemoryInfo());
    EXPECT_EQ(3u, ioctlCallsCount);
    ioctlCallsCount = 0;
}

int DrmMockExtended::handleRemainingRequests(DrmIoctl request, void *arg) {
    if ((request == DrmIoctl::query) && (arg != nullptr)) {
        if (i915QuerySuccessCount == 0) {
            return EINVAL;
        }
        i915QuerySuccessCount--;

        auto query = static_cast<Query *>(arg);
        if (query->itemsPtr == 0) {
            return EINVAL;
        }
        for (auto i = 0u; i < query->numItems; i++) {
            if (!handleQueryItem(reinterpret_cast<QueryItem *>(query->itemsPtr) + i)) {
                return EINVAL;
            }
        }
        return 0;
    } else if (request == DrmIoctl::gemContextSetparam && arg != nullptr && receivedContextParamRequest.param == I915_CONTEXT_PARAM_ENGINES) {
        EXPECT_LE(receivedContextParamRequest.size, sizeof(receivedContextParamEngines));
        memcpy(&receivedContextParamEngines, reinterpret_cast<const void *>(receivedContextParamRequest.value), receivedContextParamRequest.size);
        auto srcBalancer = reinterpret_cast<const i915_context_engines_load_balance *>(receivedContextParamEngines.extensions);
        if (srcBalancer) {
            EXPECT_EQ(static_cast<__u32>(I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE), srcBalancer->base.name);
            auto balancerSize = ptrDiff(srcBalancer->engines + srcBalancer->num_siblings, srcBalancer);
            EXPECT_LE(balancerSize, sizeof(receivedContextEnginesLoadBalance));
            memcpy(&receivedContextEnginesLoadBalance, srcBalancer, balancerSize);
        }
        return this->storedRetValForSetParamEngines;
    } else if (request == DrmIoctl::gemContextSetparam && arg != nullptr && receivedContextParamRequest.param == PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAGS) {
        if (!this->contextDebugSupported) {
            return EINVAL;
        } else {
            return 0;
        }
    } else if (request == DrmIoctl::gemContextGetparam && arg != nullptr) {
        auto argIn = static_cast<GemContextParam *>(arg);
        if (argIn->param == PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAGS) {
            if (this->contextDebugSupported) {
                argIn->value = PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAG_SIP << 32;
            } else {
                argIn->value = 0;
            }
            return 0;
        }
    } else if (request == DrmIoctl::gemCreateExt) {
        auto createExtParams = static_cast<prelim_drm_i915_gem_create_ext *>(arg);
        if (createExtParams->size == 0) {
            return EINVAL;
        }
        this->createExt.size = createExtParams->size;
        this->createExt.handle = createExtParams->handle = 1u;
        auto extensions = reinterpret_cast<prelim_drm_i915_gem_create_ext_setparam *>(createExtParams->extensions);
        if (extensions == nullptr) {
            return EINVAL;
        }
        this->setparamRegion = *extensions;
        if (this->setparamRegion.base.name != PRELIM_I915_GEM_CREATE_EXT_SETPARAM) {
            return EINVAL;
        }
        if ((this->setparamRegion.param.size == 0) ||
            (this->setparamRegion.param.param != (PRELIM_I915_OBJECT_PARAM | PRELIM_I915_PARAM_MEMORY_REGIONS))) {
            return EINVAL;
        }
        if (setparamRegion.base.next_extension != 0) {
            auto vmPrivate = reinterpret_cast<prelim_drm_i915_gem_create_ext_vm_private *>(setparamRegion.base.next_extension);
            if (vmPrivate->base.name != PRELIM_I915_GEM_CREATE_EXT_VM_PRIVATE) {
                return EINVAL;
            }
        }
        auto data = reinterpret_cast<MemoryClassInstance *>(this->setparamRegion.param.data);
        if (data == nullptr) {
            return EINVAL;
        }
        this->memRegions = *data;
        if (this->setparamRegion.param.size > 1) {
            this->allMemRegions.clear();
            for (uint32_t i = 0; i < this->setparamRegion.param.size; i++) {
                this->allMemRegions.push_back(data[i]);
            }
        }
        if ((this->memRegions.memoryClass != prelim_drm_i915_gem_memory_class::PRELIM_I915_MEMORY_CLASS_SYSTEM) && (this->memRegions.memoryClass != prelim_drm_i915_gem_memory_class::PRELIM_I915_MEMORY_CLASS_DEVICE)) {
            return EINVAL;
        }
        return gemCreateExtRetVal;
    } else if (request == DrmIoctl::gemMmapOffset) {
        auto mmapArg = static_cast<GemMmapOffset *>(arg);
        mmapArg->offset = 0;
        return mmapOffsetRetVal;
    } else if (request == DrmIoctl::gemClosReserve && (arg != nullptr)) {
        auto closReserveArg = static_cast<prelim_drm_i915_gem_clos_reserve *>(arg);
        this->closIndex++;
        if (closIndex == 0) {
            return EINVAL;
        }
        closReserveArg->clos_index = closIndex;
        return 0;
    } else if (request == DrmIoctl::gemClosFree && (arg != nullptr)) {
        auto closFreeArg = static_cast<prelim_drm_i915_gem_clos_free *>(arg);
        if (closFreeArg->clos_index > closIndex) {
            return EINVAL;
        }
        this->closIndex--;
        return 0;
    } else if (request == DrmIoctl::gemCacheReserve && (arg != nullptr)) {
        auto cacheReserveArg = static_cast<prelim_drm_i915_gem_cache_reserve *>(arg);
        if (cacheReserveArg->clos_index > closIndex) {
            return EINVAL;
        }
        const auto cacheLevel{toCacheLevel(cacheReserveArg->cache_level)};
        auto cacheInfo = this->getCacheInfo();
        auto maxReservationNumWays = cacheInfo ? cacheInfo->getMaxReservationNumWays(cacheLevel) : maxNumWays;
        if (cacheReserveArg->num_ways > maxReservationNumWays) {
            return EINVAL;
        }
        auto freeNumWays = maxReservationNumWays - this->allocNumWays;
        if (cacheReserveArg->num_ways > freeNumWays) {
            return EINVAL;
        }
        this->allocNumWays += cacheReserveArg->num_ways;
        return 0;
    } else if ((request == DrmIoctl::debuggerOpen) && (arg != nullptr)) {
        auto debuggerOpen = reinterpret_cast<prelim_drm_i915_debugger_open_param *>(arg);
        inputDebuggerOpen = *debuggerOpen;
        return debuggerOpenRetval;
    } else if ((request == DrmIoctl::gemVmAdvise) && (arg != nullptr)) {
        auto vmAdvise = reinterpret_cast<prelim_drm_i915_gem_vm_advise *>(arg);
        vmAdviseCalled = true;
        vmAdviseFlags = vmAdvise->attribute;
        vmAdviseHandle = vmAdvise->handle;
        vmAdviseRegion.memoryClass = vmAdvise->region.memory_class;
        vmAdviseRegion.memoryInstance = vmAdvise->region.memory_instance;
        return vmAdviseRetValue;
    }

    return DrmMock::handleRemainingRequests(request, arg);
}

bool DrmMockExtended::handleQueryItem(QueryItem *queryItem) {
    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    auto &gtSystemInfo = hwInfo->gtSystemInfo;
    switch (queryItem->queryId) {
    case DRM_I915_QUERY_MEMORY_REGIONS: {
        auto numberOfLocalMemories = gtSystemInfo.MultiTileArchInfo.IsValid ? gtSystemInfo.MultiTileArchInfo.TileCount : 0u;
        auto numberOfRegions = 1u + numberOfLocalMemories;

        int regionInfoSize = sizeof(drm_i915_query_memory_regions) + numberOfRegions * sizeof(drm_i915_memory_region_info);
        if (queryItem->length == 0) {
            queryItem->length = regionInfoSize;
        } else {
            EXPECT_EQ(regionInfoSize, queryItem->length);
            auto queryMemoryRegionInfo = reinterpret_cast<drm_i915_query_memory_regions *>(queryItem->dataPtr);
            EXPECT_EQ(0u, queryMemoryRegionInfo->num_regions);
            queryMemoryRegionInfo->num_regions = numberOfRegions;
            queryMemoryRegionInfo->regions[0].region.memory_class = prelim_drm_i915_gem_memory_class::PRELIM_I915_MEMORY_CLASS_SYSTEM;
            queryMemoryRegionInfo->regions[0].region.memory_instance = 1;
            queryMemoryRegionInfo->regions[0].probed_size = 2 * MemoryConstants::gigaByte;
            for (auto tile = 0u; tile < numberOfLocalMemories; tile++) {
                queryMemoryRegionInfo->regions[1 + tile].region.memory_class = prelim_drm_i915_gem_memory_class::PRELIM_I915_MEMORY_CLASS_DEVICE;
                queryMemoryRegionInfo->regions[1 + tile].region.memory_instance = DrmMockHelper::getEngineOrMemoryInstanceValue(tile, 0);
                queryMemoryRegionInfo->regions[1 + tile].probed_size = 2 * MemoryConstants::gigaByte;
            }
        }
        break;
    }
    case PRELIM_DRM_I915_QUERY_DISTANCE_INFO:
        if (failQueryDistanceInfo) {
            queryItem->length = -EINVAL;
        } else {
            auto queryDistanceInfo = reinterpret_cast<prelim_drm_i915_query_distance_info *>(queryItem->dataPtr);
            switch (queryDistanceInfo->region.memory_class) {
            case prelim_drm_i915_gem_memory_class::PRELIM_I915_MEMORY_CLASS_SYSTEM:
                EXPECT_EQ(sizeof(prelim_drm_i915_query_distance_info), static_cast<size_t>(queryItem->length));
                queryDistanceInfo->distance = -1;
                break;
            case prelim_drm_i915_gem_memory_class::PRELIM_I915_MEMORY_CLASS_DEVICE: {
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
        }
        break;
    case PRELIM_DRM_I915_QUERY_HWCONFIG_TABLE: {
        int deviceBlobSize = sizeof(dummyDeviceBlobData);
        if (queryItem->length == 0) {
            queryItem->length = deviceBlobSize;
        } else {
            EXPECT_EQ(deviceBlobSize, queryItem->length);
            auto deviceBlobData = reinterpret_cast<uint32_t *>(queryItem->dataPtr);
            memcpy(deviceBlobData, &dummyDeviceBlobData, deviceBlobSize);
        }
    } break;
    case PRELIM_DRM_I915_QUERY_COMPUTE_SUBSLICES: {
        auto maxEuPerSubslice = rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo.MaxEuPerSubSlice;
        auto maxSlices = rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo.MaxSlicesSupported;
        auto maxSubslices = rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo.MaxSubSlicesSupported / maxSlices;
        auto threadsPerEu = rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo.ThreadCount / rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo.EUCount;
        auto realEuCount = threadsPerEu * maxEuPerSubslice * maxSubslices * maxSlices;

        auto dataSize = static_cast<size_t>(std::ceil(realEuCount / 8.0)) + maxSlices * static_cast<uint16_t>(std::ceil(maxSubslices / 8.0)) +
                        static_cast<uint16_t>(std::ceil(maxSlices / 8.0));

        if (queryItem->length == 0) {
            queryItem->length = static_cast<int32_t>(sizeof(QueryTopologyInfo) + dataSize);
            break;
        } else {
            auto topologyArg = reinterpret_cast<QueryTopologyInfo *>(queryItem->dataPtr);
            if (this->failRetTopology) {
                return false;
            }
            topologyArg->maxSlices = maxSlices;
            topologyArg->maxSubslices = maxSubslices;
            topologyArg->maxEusPerSubslice = maxEuPerSubslice;

            topologyArg->subsliceStride = static_cast<uint16_t>(std::ceil(maxSubslices / 8.0));
            topologyArg->euStride = static_cast<uint16_t>(std::ceil(maxEuPerSubslice / 8.0));
            topologyArg->subsliceOffset = static_cast<uint16_t>(std::ceil(maxSlices / 8.0));
            topologyArg->euOffset = static_cast<uint16_t>(std::ceil(maxSubslices / 8.0)) * maxSlices;

            int threadData = (threadsPerEu == 8) ? 0xff : 0x7f;

            uint8_t *data = topologyArg->data;
            for (uint32_t sliceId = 0; sliceId < maxSlices; sliceId++) {
                data[0] |= 1 << (sliceId % 8);
                if (sliceId == 7 || sliceId == maxSlices - 1) {
                    data++;
                }
            }

            DEBUG_BREAK_IF(ptrDiff(data, topologyArg->data) != topologyArg->subsliceOffset);

            data = ptrOffset(topologyArg->data, topologyArg->subsliceOffset);
            for (uint32_t sliceId = 0; sliceId < maxSlices; sliceId++) {
                for (uint32_t i = 0; i < maxSubslices; i++) {
                    data[0] |= 1 << (i % 8);

                    if (i == 7 || i == maxSubslices - 1) {
                        data++;
                    }
                }
            }

            DEBUG_BREAK_IF(ptrDiff(data, topologyArg->data) != topologyArg->euOffset);
            auto size = dataSize - topologyArg->euOffset;
            memset(ptrOffset(topologyArg->data, topologyArg->euOffset), threadData, size);
        }
    } break;
    default:
        queryItem->length = -EINVAL;
        break;
    }

    return true;
}
