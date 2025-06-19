/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/os_interface/linux/xe/mock_ioctl_helper_xe.h"
#include "shared/test/common/test_macros/test.h"

std::unique_ptr<DrmMockXe> DrmMockXe::create(RootDeviceEnvironment &rootDeviceEnvironment) {
    auto drm = std::unique_ptr<DrmMockXe>(new DrmMockXe{rootDeviceEnvironment});
    drm->initInstance();

    return drm;
}

void DrmMockXe::testMode(int f, int a) {
    forceIoctlAnswer = f;
    setIoctlAnswer = a;
}
int DrmMockXe::ioctl(DrmIoctl request, void *arg) {
    int ret = -1;
    ioctlCalled = true;
    if (forceIoctlAnswer) {
        return setIoctlAnswer;
    }
    switch (request) {
    case DrmIoctl::gemVmCreate: {
        struct drm_xe_vm_create *v = static_cast<struct drm_xe_vm_create *>(arg);
        v->vm_id = testValueVmId;
        ret = 0;
    } break;
    case DrmIoctl::gemUserptr: {
        ret = 0;
    } break;
    case DrmIoctl::gemClose: {
        auto gemClose = reinterpret_cast<GemClose *>(arg);
        passedGemClose = *gemClose;
        gemCloseCalled++;
        ret = 0;
    } break;
    case DrmIoctl::gemVmDestroy: {
        struct drm_xe_vm_destroy *v = static_cast<struct drm_xe_vm_destroy *>(arg);
        if (v->vm_id == testValueVmId)
            ret = 0;
    } break;
    case DrmIoctl::gemMmapOffset: {
        gemMmapOffsetCalled++;

        if (forceMmapOffsetFail) {
            ret = -1;
            break;
        }
        struct drm_xe_gem_mmap_offset *v = static_cast<struct drm_xe_gem_mmap_offset *>(arg);
        v->offset = v->handle;
        ret = 0;

    } break;
    case DrmIoctl::primeFdToHandle: {
        PrimeHandle *v = static_cast<PrimeHandle *>(arg);
        if (v->fileDescriptor == testValuePrime) {
            v->handle = testValuePrime;
            ret = 0;
        }
    } break;
    case DrmIoctl::primeHandleToFd: {
        PrimeHandle *v = static_cast<PrimeHandle *>(arg);
        if (v->handle == testValuePrime) {
            v->fileDescriptor = testValuePrime;
            ret = 0;
        }
    } break;
    case DrmIoctl::syncObjFdToHandle: {
        ret = 0;
    } break;
    case DrmIoctl::syncObjTimelineWait: {
        ret = 0;
    } break;
    case DrmIoctl::syncObjWait: {
        ret = 0;
    } break;
    case DrmIoctl::syncObjSignal: {
        ret = 0;
    } break;
    case DrmIoctl::syncObjTimelineSignal: {
        ret = 0;
    } break;
    case DrmIoctl::gemCreate: {
        ioctlCnt.gemCreate++;
        auto createParams = static_cast<drm_xe_gem_create *>(arg);
        this->createParamsSize = createParams->size;
        this->createParamsPlacement = createParams->placement;
        this->createParamsFlags = createParams->flags;
        this->createParamsHandle = createParams->handle = testValueGemCreate;
        this->createParamsCpuCaching = createParams->cpu_caching;
        if (0 == this->createParamsSize || 0 == this->createParamsPlacement || 0 == this->createParamsCpuCaching) {
            return EINVAL;
        }
        ret = 0;
    } break;
    case DrmIoctl::getparam:
        ret = -2;
        break;
    case DrmIoctl::getResetStats: {
        auto execQueueProperty = static_cast<drm_xe_exec_queue_get_property *>(arg);
        EXPECT_EQ(execQueueProperty->property, static_cast<uint32_t>(DRM_XE_EXEC_QUEUE_GET_PROPERTY_BAN));
        execQueueProperty->value = execQueueBanPropertyReturn;
        ret = 0;
    } break;
    case DrmIoctl::query: {
        struct drm_xe_device_query *deviceQuery = static_cast<struct drm_xe_device_query *>(arg);
        switch (deviceQuery->query) {
        case DRM_XE_DEVICE_QUERY_CONFIG:
            if (deviceQuery->data) {
                memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryConfig, sizeof(queryConfig));
            }
            deviceQuery->size = sizeof(queryConfig);
            break;
        case DRM_XE_DEVICE_QUERY_ENGINES:
            if (deviceQuery->data) {
                memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryEngines, sizeof(queryEngines));
            }
            deviceQuery->size = sizeof(queryEngines);
            break;
        case DRM_XE_DEVICE_QUERY_MEM_REGIONS:
            if (deviceQuery->data) {
                memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryMemUsage, sizeof(queryMemUsage));
            }
            deviceQuery->size = sizeof(queryMemUsage);
            break;
        case DRM_XE_DEVICE_QUERY_GT_LIST:
            if (deviceQuery->data) {
                memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryGtList.begin(), sizeof(queryGtList[0]) * queryGtList.size());
            }
            deviceQuery->size = static_cast<uint32_t>(sizeof(queryGtList[0]) * queryGtList.size());
            break;
        case DRM_XE_DEVICE_QUERY_GT_TOPOLOGY:
            if (deviceQuery->data) {
                memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryTopology.data(), queryTopology.size());
            }
            deviceQuery->size = static_cast<unsigned int>(queryTopology.size());
            break;
        case DRM_XE_DEVICE_QUERY_ENGINE_CYCLES:
            if (deviceQuery->data) {
                memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryEngineCycles, sizeof(queryEngineCycles));
            }
            deviceQuery->size = sizeof(queryEngineCycles);
            break;
        };
        ret = 0;
    } break;
    case DrmIoctl::gemVmBind: {
        ret = gemVmBindReturn;
        auto vmBindInput = static_cast<drm_xe_vm_bind *>(arg);
        vmBindInputs.push_back(*vmBindInput);

        if (vmBindInput->num_syncs == 1) {
            auto &syncInput = reinterpret_cast<drm_xe_sync *>(vmBindInput->syncs)[0];
            syncInputs.push_back(syncInput);
        }
    } break;

    case DrmIoctl::gemWaitUserFence: {
        ret = waitUserFenceReturn;
        auto waitUserFenceInput = static_cast<drm_xe_wait_user_fence *>(arg);
        waitUserFenceInputs.push_back(*waitUserFenceInput);
        handleUserFenceWaitExtensions(waitUserFenceInput);
    } break;

    case DrmIoctl::gemContextCreateExt: {
        auto queueCreate = static_cast<drm_xe_exec_queue_create *>(arg);
        latestExecQueueCreate = *queueCreate;
        latestQueueEngineClassInstances.clear();

        auto instances = reinterpret_cast<drm_xe_engine_class_instance *>(queueCreate->instances);

        for (uint16_t i = 0; i < queueCreate->num_placements; i++) {
            latestQueueEngineClassInstances.push_back(instances[i]);
        }

        auto extension = queueCreate->extensions;
        while (extension) {
            auto ext = reinterpret_cast<drm_xe_user_extension *>(extension);
            if (ext->name == DRM_XE_EXEC_QUEUE_EXTENSION_SET_PROPERTY) {
                auto setProperty = reinterpret_cast<drm_xe_ext_set_property *>(ext);
                execQueueProperties.push_back(*setProperty);
            }
            handleContextCreateExtensions(ext);
            extension = ext->next_extension;
        }
        queueCreate->exec_queue_id = ++mockExecQueueId;
        ret = 0;
    } break;
    case DrmIoctl::gemContextDestroy: {
        gemDestroyContextCalled++;
        ret = 0;
    } break;
    case DrmIoctl::perfOpen: {
        ret = 0;
    } break;
    case DrmIoctl::gemContextSetparam:
    case DrmIoctl::gemContextGetparam:

    default:
        break;
    }
    return ret;
}
void DrmMockXe::addMockedQueryTopologyData(uint16_t gtId, uint16_t maskType, uint32_t nBytes, const std::vector<uint8_t> &mask) {

    ASSERT_EQ(nBytes, mask.size());

    auto additionalSize = 8u + nBytes;
    auto oldSize = queryTopology.size();
    auto newSize = oldSize + additionalSize;
    queryTopology.resize(newSize, 0u);

    uint8_t *dataPtr = queryTopology.data() + oldSize;

    drm_xe_query_topology_mask *topo = reinterpret_cast<drm_xe_query_topology_mask *>(dataPtr);
    topo->gt_id = gtId;
    topo->type = maskType;
    topo->num_bytes = nBytes;

    memcpy_s(reinterpret_cast<void *>(topo->mask), nBytes, mask.data(), nBytes);
}
void DrmMockXe::initInstance() {
    this->reset();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<NEO::GfxCoreHelper>();
    this->ioctlExpected.contextCreate = static_cast<int>(gfxCoreHelper.getGpgpuEngineInstances(rootDeviceEnvironment).size());
    this->ioctlExpected.contextDestroy = this->ioctlExpected.contextCreate.load();

    this->ioctlHelper = std::make_unique<MockIoctlHelperXe>(*this);

    this->createVirtualMemoryAddressSpace(NEO::GfxCoreHelper::getSubDevicesCount(rootDeviceEnvironment.getHardwareInfo()));
    this->isVmBindAvailable();

    auto xeQueryConfig = reinterpret_cast<drm_xe_query_config *>(this->queryConfig);
    xeQueryConfig->num_params = 6;
    xeQueryConfig->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] = 0; // this should be queried by ioctl sys call
    xeQueryConfig->info[DRM_XE_QUERY_CONFIG_VA_BITS] = 48;
    xeQueryConfig->info[DRM_XE_QUERY_CONFIG_MAX_EXEC_QUEUE_PRIORITY] = mockMaxExecQueuePriority;

    constexpr drm_xe_gt tile0MainGt = {
        .type = DRM_XE_QUERY_GT_TYPE_MAIN,
        .tile_id = 0,
        .gt_id = 0,
        .pad = {0},
        .reference_clock = mockTimestampFrequency,
        .near_mem_regions = 0b100,
        .far_mem_regions = 0x011,
    };

    constexpr drm_xe_gt tile1MediaGt = {
        .type = DRM_XE_QUERY_GT_TYPE_MEDIA,
        .tile_id = 1,
        .gt_id = 1,
        .pad = {0},
        .reference_clock = mockTimestampFrequency,
        .near_mem_regions = 0b001,
        .far_mem_regions = 0x110,
    };

    constexpr drm_xe_gt tile1MainGt = {
        .type = DRM_XE_QUERY_GT_TYPE_MAIN,
        .tile_id = 1,
        .gt_id = 2,
        .pad = {0},
        .reference_clock = mockTimestampFrequency,
        .near_mem_regions = 0b010,
        .far_mem_regions = 0x101,
    };

    constexpr drm_xe_gt tile2MainGt = {
        .type = DRM_XE_QUERY_GT_TYPE_MAIN,
        .tile_id = 2,
        .gt_id = 3,
        .pad = {0},
        .reference_clock = mockTimestampFrequency,
        .near_mem_regions = 0b100,
        .far_mem_regions = 0x011,
    };

    auto xeQueryEngines = reinterpret_cast<drm_xe_query_engines *>(this->queryEngines);
    xeQueryEngines->num_engines = 11;
    xeQueryEngines->engines[0] = {{DRM_XE_ENGINE_CLASS_RENDER, 0, tile0MainGt.gt_id}, {}};
    xeQueryEngines->engines[1] = {{DRM_XE_ENGINE_CLASS_COPY, 1, tile0MainGt.gt_id}, {}};
    xeQueryEngines->engines[2] = {{DRM_XE_ENGINE_CLASS_COPY, 2, tile0MainGt.gt_id}, {}};
    xeQueryEngines->engines[3] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 3, tile0MainGt.gt_id}, {}};
    xeQueryEngines->engines[4] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 4, tile0MainGt.gt_id}, {}};
    xeQueryEngines->engines[5] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 5, tile1MainGt.gt_id}, {}};
    xeQueryEngines->engines[6] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 6, tile1MainGt.gt_id}, {}};
    xeQueryEngines->engines[7] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 7, tile1MainGt.gt_id}, {}};
    xeQueryEngines->engines[8] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 8, tile1MainGt.gt_id}, {}};
    xeQueryEngines->engines[9] = {{DRM_XE_ENGINE_CLASS_VIDEO_DECODE, 9, tile1MainGt.gt_id}, {}};
    xeQueryEngines->engines[10] = {{DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE, 10, tile0MainGt.gt_id}, {}};

    auto xeQueryMemUsage = reinterpret_cast<drm_xe_query_mem_regions *>(this->queryMemUsage);
    xeQueryMemUsage->num_mem_regions = 3;
    xeQueryMemUsage->mem_regions[0] = {
        .mem_class = DRM_XE_MEM_REGION_CLASS_VRAM,
        .instance = 1,
        .min_page_size = MemoryConstants::pageSize,
        .total_size = 2 * MemoryConstants::gigaByte,
        .used = MemoryConstants::megaByte,
        .cpu_visible_size = 2 * MemoryConstants::gigaByte,
    };
    xeQueryMemUsage->mem_regions[1] = {
        .mem_class = DRM_XE_MEM_REGION_CLASS_SYSMEM,
        .instance = 0,
        .min_page_size = MemoryConstants::pageSize,
        .total_size = MemoryConstants::gigaByte,
        .used = MemoryConstants::kiloByte,
        .cpu_visible_size = MemoryConstants::gigaByte,
    };
    xeQueryMemUsage->mem_regions[2] = {
        .mem_class = DRM_XE_MEM_REGION_CLASS_VRAM,
        .instance = 2,
        .min_page_size = MemoryConstants::pageSize,
        .total_size = 4 * MemoryConstants::gigaByte,
        .used = MemoryConstants::gigaByte,
        .cpu_visible_size = 4 * MemoryConstants::gigaByte,
    };

    this->queryGtList.resize(1 + (6 * 12)); // 1 qword for num gts and 12 qwords per gt
    auto xeQueryGtList = reinterpret_cast<drm_xe_query_gt_list *>(this->queryGtList.begin());
    xeQueryGtList->num_gt = 4;
    xeQueryGtList->gt_list[0] = tile0MainGt;
    xeQueryGtList->gt_list[1] = tile1MediaGt;
    xeQueryGtList->gt_list[2] = tile1MainGt;
    xeQueryGtList->gt_list[3] = tile2MainGt;
    this->reset();
}
