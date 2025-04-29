/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/source/os_interface/linux/xe/xedrm_prelim.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_os_time_linux.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/os_interface/linux/xe/eudebug/mock_eudebug_interface.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct MockIoctlHelperXeDebug : IoctlHelperXe {
    using IoctlHelperXe::bindInfo;
    using IoctlHelperXe::euDebugInterface;
    using IoctlHelperXe::getEudebugExtProperty;
    using IoctlHelperXe::getEudebugExtPropertyValue;
    using IoctlHelperXe::IoctlHelperXe;
    using IoctlHelperXe::tileIdToGtId;
};

inline constexpr int testValueVmId = 0x5764;
inline constexpr int testValueMapOff = 0x7788;
inline constexpr int testValuePrime = 0x4321;
inline constexpr uint32_t testValueGemCreate = 0x8273;

struct DrmMockXeDebug : public DrmMockCustom {
    using Drm::engineInfo;

    static auto create(RootDeviceEnvironment &rootDeviceEnvironment) {
        auto drm = std::unique_ptr<DrmMockXeDebug>(new DrmMockXeDebug{rootDeviceEnvironment});

        drm->reset();
        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<NEO::GfxCoreHelper>();
        drm->ioctlExpected.contextCreate = static_cast<int>(gfxCoreHelper.getGpgpuEngineInstances(rootDeviceEnvironment).size());
        drm->ioctlExpected.contextDestroy = drm->ioctlExpected.contextCreate.load();
        drm->ioctlHelper = std::make_unique<MockIoctlHelperXeDebug>(*drm);
        drm->isVmBindAvailable();
        drm->reset();

        drm->ioctlHelper = std::make_unique<IoctlHelperXe>(*drm);

        auto xeQueryConfig = reinterpret_cast<drm_xe_query_config *>(drm->queryConfig);
        xeQueryConfig->num_params = 6;
        xeQueryConfig->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] = 0; // this should be queried by ioctl sys call
        xeQueryConfig->info[DRM_XE_QUERY_CONFIG_VA_BITS] = 48;
        xeQueryConfig->info[DRM_XE_QUERY_CONFIG_MAX_EXEC_QUEUE_PRIORITY] = mockMaxExecQueuePriority;

        drm->queryGtList.resize(49); // 1 qword for num gts and 12 qwords per gt
        auto xeQueryGtList = reinterpret_cast<drm_xe_query_gt_list *>(drm->queryGtList.begin());
        xeQueryGtList->num_gt = 4;

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

        xeQueryGtList->gt_list[0] = tile0MainGt;
        xeQueryGtList->gt_list[1] = tile1MediaGt;
        xeQueryGtList->gt_list[2] = tile1MainGt;
        xeQueryGtList->gt_list[3] = tile2MainGt;

        drm->ioctlHelper->initialize();
        EXPECT_EQ(1, drm->ioctlHelper->getEuDebugSysFsEnable());
        auto xeQueryEngines = reinterpret_cast<drm_xe_query_engines *>(drm->queryEngines);
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

        return drm;
    }

    int getErrno() override {
        if (baseErrno) {
            return Drm::getErrno();
        }
        return errnoRetVal;
    }
    bool baseErrno = false;
    int errnoRetVal = 0;

    void setPciPath(const char *pciPath) {
        hwDeviceId = std::make_unique<HwDeviceIdDrm>(getFileDescriptor(), pciPath);
    }

    int ioctl(DrmIoctl request, void *arg) override {
        int ret = -1;
        ioctlCalled = true;
        if (forceIoctlAnswer) {
            return setIoctlAnswer;
        }
        switch (request) {
        case DrmIoctl::gemVmBind: {
            return 0;
        } break;
        case DrmIoctl::query: {
            struct drm_xe_device_query *deviceQuery = static_cast<struct drm_xe_device_query *>(arg);
            switch (deviceQuery->query) {
            case DRM_XE_DEVICE_QUERY_ENGINES:
                if (deviceQuery->data) {
                    memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryEngines, sizeof(queryEngines));
                }
                deviceQuery->size = sizeof(queryEngines);
                break;
            case DRM_XE_DEVICE_QUERY_CONFIG:
                if (deviceQuery->data) {
                    memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryConfig, sizeof(queryConfig));
                }
                deviceQuery->size = sizeof(queryConfig);
                break;
            case DRM_XE_DEVICE_QUERY_GT_LIST:
                if (deviceQuery->data) {
                    memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryGtList.begin(), sizeof(queryGtList[0]) * queryGtList.size());
                }
                deviceQuery->size = static_cast<uint32_t>(sizeof(queryGtList[0]) * queryGtList.size());
                break;
            };
            ret = 0;
        } break;
        case DrmIoctl::gemContextCreateExt: {
            auto create = static_cast<struct drm_xe_exec_queue_create *>(arg);
            execQueueCreateParams = *create;
            if (create->extensions) {
                receivedContextCreateSetParam = *reinterpret_cast<struct drm_xe_ext_set_property *>(create->extensions);
            }
            execQueueEngineInstances.clear();
            for (uint16_t i = 0; i < create->num_placements; i++) {
                execQueueEngineInstances.push_back(reinterpret_cast<drm_xe_engine_class_instance *>(create->instances)[i]);
            }
            ret = 0;
        } break;
        case DrmIoctl::gemVmCreate: {
            ret = 0;
        } break;
        case DrmIoctl::debuggerOpen: {
            auto debuggerOpen = reinterpret_cast<EuDebugConnect *>(arg);

            if (debuggerOpen->version != 0) {
                return -1;
            }
            if (debuggerOpenVersion != 0) {
                debuggerOpen->version = debuggerOpenVersion;
            }
            return debuggerOpenRetval;
        } break;
        case DrmIoctl::metadataCreate: {
            auto metadata = reinterpret_cast<DebugMetadataCreate *>(arg);
            metadataAddr = reinterpret_cast<void *>(metadata->userAddr);
            metadataSize = metadata->len;
            metadataType = metadata->type;
            metadata->metadataId = metadataID;
            return 0;
        } break;
        case DrmIoctl::metadataDestroy: {
            auto metadata = reinterpret_cast<DebugMetadataDestroy *>(arg);
            metadataID = metadata->metadataId;
            return 0;
        } break;
        case DrmIoctl::gemWaitUserFence: {
            auto waitUserFenceInput = static_cast<drm_xe_wait_user_fence *>(arg);
            waitUserFenceInputs.push_back(*waitUserFenceInput);
            return 0;
        } break;

        default:
            break;
        }
        return ret;
    }

    void addMockedQueryTopologyData(uint16_t tileId, uint16_t maskType, uint32_t nBytes, const std::vector<uint8_t> &mask) {

        ASSERT_EQ(nBytes, mask.size());

        auto additionalSize = 8u + nBytes;
        auto oldSize = queryTopology.size();
        auto newSize = oldSize + additionalSize;
        queryTopology.resize(newSize, 0u);

        uint8_t *dataPtr = queryTopology.data() + oldSize;

        drm_xe_query_topology_mask *topo = reinterpret_cast<drm_xe_query_topology_mask *>(dataPtr);
        topo->gt_id = tileId;
        topo->type = maskType;
        topo->num_bytes = nBytes;

        memcpy_s(reinterpret_cast<void *>(topo->mask), nBytes, mask.data(), nBytes);
    }

    bool isDebugAttachAvailable() override {
        if (allowDebugAttachCallBase) {
            return Drm::isDebugAttachAvailable();
        }
        return allowDebugAttach;
    }
    static constexpr const char *mockSysFsPciPath = "mock_sys_fs_pci_path";
    std::string getSysFsPciPath() override { return mockSysFsPciPath; }

    static_assert(sizeof(drm_xe_engine) == 4 * sizeof(uint64_t), "");
    uint64_t queryEngines[45]{}; // 1 qword for num engines and 4 qwords per engine

    struct drm_xe_ext_set_property receivedContextCreateSetParam = {};
    bool allowDebugAttachCallBase = false;
    bool allowDebugAttach = false;
    bool ioctlCalled = false;

    int forceIoctlAnswer = 0;
    int setIoctlAnswer = 0;
    int gemVmBindReturn = 0;

    uint32_t metadataID = 20;
    void *metadataAddr = nullptr;
    size_t metadataSize = 0;
    uint64_t metadataType = 9999;

    alignas(64) std::vector<uint8_t> queryTopology;
    std::vector<drm_xe_engine_class_instance> execQueueEngineInstances;
    drm_xe_exec_queue_create execQueueCreateParams = {};
    StackVec<drm_xe_wait_user_fence, 1> waitUserFenceInputs;
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen{&NEO::IoFunctions::fopenPtr};
    VariableBackup<size_t (*)(void *, size_t, size_t, FILE *)> mockFread{&NEO::IoFunctions::freadPtr};

    // Debugger ioctls
    int debuggerOpenRetval = 10; // debugFd
    uint32_t debuggerOpenVersion = 0;
    StackVec<uint64_t, 49> queryGtList{}; // 1 qword for num gts and 12 qwords per gt
    static constexpr uint32_t mockTimestampFrequency = 12500000;
    uint64_t queryConfig[7]{}; // 1 qword for num params and 1 qwords per param
    static constexpr int32_t mockMaxExecQueuePriority = 3;

  protected:
    // Don't call directly, use the create() function
    DrmMockXeDebug(RootDeviceEnvironment &rootDeviceEnvironment)
        : DrmMockCustom(std::make_unique<HwDeviceIdDrm>(mockFd, mockPciPath), rootDeviceEnvironment) {
        NEO::IoFunctions::fopenPtr = [](const char *filename, const char *mode) -> FILE * {
            std::string fsEntry(filename);
            std::string expectedPath = std::string(DrmMockXeDebug::mockSysFsPciPath) + MockEuDebugInterface::sysFsXeEuDebugFile;
            if (fsEntry == expectedPath) {
                return reinterpret_cast<FILE *>(MockEuDebugInterface::sysFsFd);
            }

            return NEO::IoFunctions::mockFopen(filename, mode);
        };
        NEO::IoFunctions::freadPtr = [](void *ptr, size_t size, size_t count, FILE *stream) -> size_t {
            if (stream == reinterpret_cast<FILE *>(MockEuDebugInterface::sysFsFd)) {

                memcpy_s(ptr, size, &MockEuDebugInterface::sysFsContent, sizeof(MockEuDebugInterface::sysFsContent));
                return sizeof(MockEuDebugInterface::sysFsContent);
            }
            return NEO::IoFunctions::mockFread(ptr, size, count, stream);
        };
    }
};
