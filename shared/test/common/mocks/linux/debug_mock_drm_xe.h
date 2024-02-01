/*
 * Copyright (C) 2023-2024 Intel Corporation
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
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_os_time_linux.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include "uapi-eudebug/drm/xe_drm.h"
#include "uapi-eudebug/drm/xe_drm_tmp.h"

using namespace NEO;

struct MockIoctlHelperXeDebug : IoctlHelperXe {
    using IoctlHelperXe::debugMetadata;
    using IoctlHelperXe::freeDebugMetadata;
    using IoctlHelperXe::getRunaloneExtProperty;
    using IoctlHelperXe::IoctlHelperXe;
};

inline constexpr int testValueVmId = 0x5764;
inline constexpr int testValueMapOff = 0x7788;
inline constexpr int testValuePrime = 0x4321;
inline constexpr uint32_t testValueGemCreate = 0x8273;

class DrmMockXeDebug : public DrmMockCustom {
  public:
    DrmMockXeDebug(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMockCustom(rootDeviceEnvironment){};

    int getErrno() override {
        if (baseErrno) {
            return Drm::getErrno();
        }
        return errnoRetVal;
    }
    bool baseErrno = false;
    int errnoRetVal = 0;

    unsigned int bindDrmContext(uint32_t drmContextId, uint32_t deviceIndex, aub_stream::EngineType engineType, bool engineInstancedDevice) override {
        return static_cast<unsigned int>(DrmParam::execDefault);
    }

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
        case DrmIoctl::query: {
            struct drm_xe_device_query *deviceQuery = static_cast<struct drm_xe_device_query *>(arg);
            switch (deviceQuery->query) {
            case DRM_XE_DEVICE_QUERY_ENGINES:
                if (deviceQuery->data) {
                    memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryEngines, sizeof(queryEngines));
                }
                deviceQuery->size = sizeof(queryEngines);
                break;
            };
            ret = 0;
        } break;
        case DrmIoctl::gemContextCreateExt: {
            auto create = static_cast<struct drm_xe_exec_queue_create *>(arg);
            if (create->extensions) {
                receivedContextCreateSetParam = *reinterpret_cast<struct drm_xe_ext_set_property *>(create->extensions);
            }
            ret = 0;
        } break;
        case DrmIoctl::gemVmCreate: {
            struct drm_xe_vm_create *v = static_cast<struct drm_xe_vm_create *>(arg);
            drm_xe_ext_vm_set_debug_metadata *metadata = reinterpret_cast<drm_xe_ext_vm_set_debug_metadata *>(v->extensions);
            while (metadata) {
                vmCreateMetadata.push_back(*metadata);
                metadata = reinterpret_cast<drm_xe_ext_vm_set_debug_metadata *>(metadata->base.next_extension);
            }
            ret = 0;
        } break;
        case DrmIoctl::debuggerOpen: {
            auto debuggerOpen = reinterpret_cast<drm_xe_eudebug_connect *>(arg);

            if (debuggerOpen->version != 0) {
                return -1;
            }
            if (debuggerOpenVersion != 0) {
                debuggerOpen->version = debuggerOpenVersion;
            }
            return debuggerOpenRetval;
        } break;
        case DrmIoctl::metadataCreate: {
            auto metadata = reinterpret_cast<drm_xe_debug_metadata_create *>(arg);
            metadataAddr = reinterpret_cast<void *>(metadata->user_addr);
            metadataSize = metadata->len;
            metadataType = metadata->type;
            metadata->id = metadataID;
            return 0;
        } break;
        case DrmIoctl::metadataDestroy: {
            auto metadata = reinterpret_cast<drm_xe_debug_metadata_destroy *>(arg);
            metadataID = metadata->id;
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

    const drm_xe_engine_class_instance queryEngines[11] = {
        {DRM_XE_ENGINE_CLASS_RENDER, 0, 0},
        {DRM_XE_ENGINE_CLASS_COPY, 1, 0},
        {DRM_XE_ENGINE_CLASS_COPY, 2, 0},
        {DRM_XE_ENGINE_CLASS_COMPUTE, 3, 0},
        {DRM_XE_ENGINE_CLASS_COMPUTE, 4, 0},
        {DRM_XE_ENGINE_CLASS_COMPUTE, 5, 1},
        {DRM_XE_ENGINE_CLASS_COMPUTE, 6, 1},
        {DRM_XE_ENGINE_CLASS_COMPUTE, 7, 1},
        {DRM_XE_ENGINE_CLASS_COMPUTE, 8, 1},
        {DRM_XE_ENGINE_CLASS_VIDEO_DECODE, 9, 1},
        {DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE, 10, 0}};

    struct drm_xe_ext_set_property receivedContextCreateSetParam = {};
    bool allowDebugAttachCallBase = false;
    bool allowDebugAttach = false;
    bool ioctlCalled = false;

    int forceIoctlAnswer = 0;
    int setIoctlAnswer = 0;
    int gemVmBindReturn = 0;

    uint64_t metadataID = 20;
    void *metadataAddr = nullptr;
    size_t metadataSize = 0;
    uint64_t metadataType = 9999;

    alignas(64) std::vector<uint8_t> queryTopology;
    std::vector<drm_xe_ext_vm_set_debug_metadata> vmCreateMetadata;

    // Debugger ioctls
    int debuggerOpenRetval = 10; // debugFd
    uint32_t debuggerOpenVersion = 0;
};
