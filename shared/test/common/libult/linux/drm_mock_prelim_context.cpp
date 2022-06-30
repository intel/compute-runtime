/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_mock_prelim_context.h"

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
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

int DrmMockPrelimContext::handlePrelimRequest(DrmIoctl request, void *arg) {
    switch (request) {
    case DrmIoctl::Getparam: {
        auto gp = static_cast<GetParam *>(arg);
        if (gp->param == PRELIM_I915_PARAM_HAS_PAGE_FAULT) {
            *gp->value = hasPageFaultQueryValue;
            return hasPageFaultQueryReturn;
        } else if (gp->param == PRELIM_I915_PARAM_HAS_VM_BIND) {
            vmBindQueryCalled++;
            *gp->value = vmBindQueryValue;
            return vmBindQueryReturn;
        }
    } break;
    case DrmIoctl::GemContextGetparam: {
        auto gp = static_cast<GemContextParam *>(arg);
        if (gp->param == PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAGS) {
            gp->value = contextDebugSupported ? PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAG_SIP << 32 : 0;
            return 0;
        }
    } break;
    case DrmIoctl::GemContextCreateExt: {
        auto create = static_cast<GemContextCreateExt *>(arg);
        auto setParam = reinterpret_cast<GemContextCreateExtSetParam *>(create->extensions);
        if (setParam->param.param == PRELIM_I915_CONTEXT_PARAM_ACC) {
            const auto paramAcc = reinterpret_cast<prelim_drm_i915_gem_context_param_acc *>(setParam->param.value);
            receivedContextParamAcc = GemContextParamAcc{paramAcc->trigger, paramAcc->notify, paramAcc->granularity};
        } else if (setParam->param.param == PRELIM_I915_CONTEXT_PARAM_RUNALONE) {
            receivedContextCreateExtSetParamRunaloneCount++;
        }
        return 0;
    } break;
    case DrmIoctl::GemMmapOffset: {
        auto mmapArg = static_cast<GemMmapOffset *>(arg);
        mmapArg->offset = 0;
        return mmapOffsetReturn;
    } break;
    case DrmIoctl::GemClosReserve: {
        auto closReserveArg = static_cast<prelim_drm_i915_gem_clos_reserve *>(arg);
        closIndex++;
        if (closIndex == 0) {
            return EINVAL;
        }
        closReserveArg->clos_index = closIndex;
        return 0;
    } break;
    case DrmIoctl::GemClosFree: {
        auto closFreeArg = static_cast<prelim_drm_i915_gem_clos_free *>(arg);
        if (closFreeArg->clos_index > closIndex) {
            return EINVAL;
        }
        closIndex--;
        return 0;
    } break;
    case DrmIoctl::GemCacheReserve: {
        auto cacheReserveArg = static_cast<prelim_drm_i915_gem_cache_reserve *>(arg);
        if (cacheReserveArg->clos_index > closIndex) {
            return EINVAL;
        }
        auto maxReservationNumWays = cacheInfo ? cacheInfo->getMaxReservationNumWays() : maxNumWays;
        if (cacheReserveArg->num_ways > maxReservationNumWays) {
            return EINVAL;
        }
        auto freeNumWays = maxReservationNumWays - allocNumWays;
        if (cacheReserveArg->num_ways > freeNumWays) {
            return EINVAL;
        }
        allocNumWays += cacheReserveArg->num_ways;
        return 0;
    } break;
    case DrmIoctl::GemVmBind: {
        vmBindCalled++;
        const auto vmBind = reinterpret_cast<prelim_drm_i915_gem_vm_bind *>(arg);
        receivedVmBind = VmBindParams{
            vmBind->vm_id,
            vmBind->handle,
            vmBind->start,
            vmBind->offset,
            vmBind->length,
            vmBind->flags,
            vmBind->extensions,
        };
        storeVmBindExtensions(vmBind->extensions, true);
        return vmBindReturn;
    } break;
    case DrmIoctl::GemVmUnbind: {
        vmUnbindCalled++;
        const auto vmBind = reinterpret_cast<prelim_drm_i915_gem_vm_bind *>(arg);
        receivedVmUnbind = VmBindParams{
            vmBind->vm_id,
            vmBind->handle,
            vmBind->start,
            vmBind->offset,
            vmBind->length,
            vmBind->flags,
            vmBind->extensions,
        };
        storeVmBindExtensions(vmBind->extensions, false);
        return vmUnbindReturn;
    } break;
    case DrmIoctl::GemCreateExt: {
        auto createExt = static_cast<prelim_drm_i915_gem_create_ext *>(arg);
        if (createExt->size == 0) {
            return EINVAL;
        }

        auto extension = reinterpret_cast<prelim_drm_i915_gem_create_ext_setparam *>(createExt->extensions);
        if (!extension) {
            return EINVAL;
        }

        if (extension->base.name != PRELIM_I915_GEM_CREATE_EXT_SETPARAM) {
            return EINVAL;
        }

        if ((extension->param.size == 0) ||
            (extension->param.param != (PRELIM_I915_OBJECT_PARAM | PRELIM_I915_PARAM_MEMORY_REGIONS))) {
            return EINVAL;
        }

        prelim_drm_i915_gem_create_ext_vm_private *vmPrivateExt = nullptr;
        if (extension->base.next_extension != 0) {
            vmPrivateExt = reinterpret_cast<prelim_drm_i915_gem_create_ext_vm_private *>(extension->base.next_extension);
            if (vmPrivateExt->base.name != PRELIM_I915_GEM_CREATE_EXT_VM_PRIVATE) {
                return EINVAL;
            }
        }

        auto data = reinterpret_cast<MemoryClassInstance *>(extension->param.data);
        if (!data) {
            return EINVAL;
        }

        constexpr uint32_t createExtHandle{1u};
        createExt->handle = createExtHandle;
        receivedCreateGemExt = CreateGemExt{createExt->size, createExtHandle};
        receivedCreateGemExt->setParamExt = CreateGemExt::SetParam{extension->param.handle, extension->param.size, extension->param.param};
        if (vmPrivateExt != nullptr) {
            receivedCreateGemExt->vmPrivateExt = CreateGemExt::VmPrivate{vmPrivateExt->vm_id};
        }

        receivedCreateGemExt->memoryRegions.clear();
        for (uint32_t i = 0; i < extension->param.size; i++) {
            receivedCreateGemExt->memoryRegions.push_back({data[i].memoryClass, data[i].memoryInstance});
        }

        const auto firstMemoryRegion = receivedCreateGemExt->memoryRegions[0];
        if ((firstMemoryRegion.memoryClass != PRELIM_I915_MEMORY_CLASS_SYSTEM) && (firstMemoryRegion.memoryClass != PRELIM_I915_MEMORY_CLASS_DEVICE)) {
            return EINVAL;
        }

        return gemCreateExtReturn;
    } break;
    case DrmIoctl::GemWaitUserFence: {
        waitUserFenceCalled++;
        const auto wait = reinterpret_cast<prelim_drm_i915_gem_wait_user_fence *>(arg);
        receivedWaitUserFence = WaitUserFence{
            wait->extensions,
            wait->addr,
            wait->ctx_id,
            wait->op,
            wait->flags,
            wait->value,
            wait->mask,
            wait->timeout,
        };
        return 0;
    } break;
    case DrmIoctl::GemContextSetparam: {
        const auto req = reinterpret_cast<GemContextParam *>(arg);
        if (req->param == PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAGS) {
            receivedSetContextParamValue = req->value;
            receivedSetContextParamCtxId = req->contextId;
        }

        return !contextDebugSupported ? EINVAL : 0;
    } break;
    case DrmIoctl::GemVmAdvise: {
        const auto req = reinterpret_cast<prelim_drm_i915_gem_vm_advise *>(arg);
        receivedVmAdvise = VmAdvise{req->handle, req->attribute};
        return vmAdviseReturn;
    } break;
    case DrmIoctl::UuidRegister: {
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
    case DrmIoctl::UuidUnregister: {
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

    case DrmIoctl::DebuggerOpen: {
        auto debuggerOpen = reinterpret_cast<prelim_drm_i915_debugger_open_param *>(arg);
        if (debuggerOpen->pid != 0 && debuggerOpen->events == 0) {
            return debuggerOpenRetval;
        }
    } break;
    default:
        return -1;
    }

    return 0;
}

bool DrmMockPrelimContext::handlePrelimQueryItem(void *arg) {
    auto queryItem = reinterpret_cast<QueryItem *>(arg);

    auto &gtSystemInfo = hwInfo->gtSystemInfo;
    const auto numberOfCCS = gtSystemInfo.CCSInfo.IsValid && !disableCcsSupport ? gtSystemInfo.CCSInfo.NumberOfCCSEnabled : 0u;

    switch (queryItem->queryId) {
    case DRM_I915_QUERY_ENGINE_INFO: {
        auto numberOfTiles = gtSystemInfo.MultiTileArchInfo.IsValid ? gtSystemInfo.MultiTileArchInfo.TileCount : 1u;
        uint32_t numberOfEngines = numberOfTiles * (4u + numberOfCCS + static_cast<uint32_t>(supportedCopyEnginesMask.count()));
        int engineInfoSize = sizeof(drm_i915_query_engine_info) + numberOfEngines * sizeof(drm_i915_engine_info);
        if (queryItem->length == 0) {
            queryItem->length = engineInfoSize;
        } else {
            EXPECT_EQ(engineInfoSize, queryItem->length);
            auto queryEngineInfo = reinterpret_cast<drm_i915_query_engine_info *>(queryItem->dataPtr);
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

    case DRM_I915_QUERY_MEMORY_REGIONS: {
        if (queryMemoryRegionInfoSuccessCount == 0) {
            queryItem->length = -EINVAL;
            return true;
        } else {
            queryMemoryRegionInfoSuccessCount--;
        }

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
            queryItem->length = -EINVAL;
            return false;
        }

        auto queryDistanceInfo = reinterpret_cast<prelim_drm_i915_query_distance_info *>(queryItem->dataPtr);
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
            queryItem->length = static_cast<int32_t>(sizeof(QueryTopologyInfo) + dataSize);
            break;
        } else {
            auto topologyArg = reinterpret_cast<QueryTopologyInfo *>(queryItem->dataPtr);
            if (failRetTopology) {
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

void DrmMockPrelimContext::storeVmBindExtensions(uint64_t ptr, bool bind) {
    if (ptr == 0) {
        return;
    }

    size_t uuidIndex{0};
    auto baseExt = reinterpret_cast<DrmUserExtension *>(ptr);
    while (baseExt) {
        if (baseExt->name == PRELIM_I915_VM_BIND_EXT_USER_FENCE) {
            const auto *ext = reinterpret_cast<prelim_drm_i915_vm_bind_ext_user_fence *>(baseExt);
            receivedVmBindUserFence = {ext->addr, ext->val};
        } else if (baseExt->name == PRELIM_I915_VM_BIND_EXT_UUID) {
            const auto *ext = reinterpret_cast<prelim_drm_i915_vm_bind_ext_uuid *>(baseExt);
            receivedVmBindUuidExt[uuidIndex++] = UuidVmBindExt{ext->uuid_handle, ext->base.next_extension};
        } else if (baseExt->name == PRELIM_I915_VM_BIND_EXT_SET_PAT) {
            const auto *ext = reinterpret_cast<prelim_drm_i915_vm_bind_ext_set_pat *>(baseExt);
            if (bind) {
                receivedVmBindPatIndex = ext->pat_index;
            } else {
                receivedVmUnbindPatIndex = ext->pat_index;
            }
        }

        baseExt = reinterpret_cast<DrmUserExtension *>(baseExt->nextExtension);
    }
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

uint32_t DrmPrelimHelper::getLongRunningContextCreateFlag() {
    return PRELIM_I915_CONTEXT_CREATE_FLAGS_LONG_RUNNING;
}

uint32_t DrmPrelimHelper::getRunAloneContextParam() {
    return PRELIM_I915_CONTEXT_PARAM_RUNALONE;
}

uint32_t DrmPrelimHelper::getAccContextParam() {
    return PRELIM_I915_CONTEXT_PARAM_ACC;
}

uint32_t DrmPrelimHelper::getAccContextParamSize() {
    return sizeof(prelim_drm_i915_gem_context_param_acc);
}

std::array<uint32_t, 4> DrmPrelimHelper::getContextAcgValues() {
    return {PRELIM_I915_CONTEXT_ACG_128K, PRELIM_I915_CONTEXT_ACG_2M, PRELIM_I915_CONTEXT_ACG_16M, PRELIM_I915_CONTEXT_ACG_64M};
}

uint32_t DrmPrelimHelper::getEnablePageFaultVmCreateFlag() {
    return PRELIM_I915_VM_CREATE_FLAGS_ENABLE_PAGE_FAULT;
}

uint32_t DrmPrelimHelper::getDisableScratchVmCreateFlag() {
    return PRELIM_I915_VM_CREATE_FLAGS_DISABLE_SCRATCH;
}

uint64_t DrmPrelimHelper::getU8WaitUserFenceFlag() {
    return PRELIM_I915_UFENCE_WAIT_U8;
}

uint64_t DrmPrelimHelper::getU16WaitUserFenceFlag() {
    return PRELIM_I915_UFENCE_WAIT_U16;
}

uint64_t DrmPrelimHelper::getU32WaitUserFenceFlag() {
    return PRELIM_I915_UFENCE_WAIT_U32;
}

uint64_t DrmPrelimHelper::getU64WaitUserFenceFlag() {
    return PRELIM_I915_UFENCE_WAIT_U64;
}

uint64_t DrmPrelimHelper::getGTEWaitUserFenceFlag() {
    return PRELIM_I915_UFENCE_WAIT_GTE;
}

uint64_t DrmPrelimHelper::getSoftWaitUserFenceFlag() {
    return PRELIM_I915_UFENCE_WAIT_SOFT;
}

uint64_t DrmPrelimHelper::getCaptureVmBindFlag() {
    return PRELIM_I915_GEM_VM_BIND_CAPTURE;
}

uint64_t DrmPrelimHelper::getImmediateVmBindFlag() {
    return PRELIM_I915_GEM_VM_BIND_IMMEDIATE;
}

uint64_t DrmPrelimHelper::getMakeResidentVmBindFlag() {
    return PRELIM_I915_GEM_VM_BIND_MAKE_RESIDENT;
}

uint64_t DrmPrelimHelper::getSIPContextParamDebugFlag() {
    return PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAG_SIP;
}

uint64_t DrmPrelimHelper::getMemoryRegionsParamFlag() {
    return PRELIM_I915_OBJECT_PARAM | PRELIM_I915_PARAM_MEMORY_REGIONS;
}

uint32_t DrmPrelimHelper::getVmAdviseNoneFlag() {
    return PRELIM_I915_VM_ADVISE_ATOMIC_NONE;
}

uint32_t DrmPrelimHelper::getVmAdviseDeviceFlag() {
    return PRELIM_I915_VM_ADVISE_ATOMIC_DEVICE;
}

uint32_t DrmPrelimHelper::getVmAdviseSystemFlag() {
    return PRELIM_I915_VM_ADVISE_ATOMIC_SYSTEM;
}
