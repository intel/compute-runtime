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

    int ioctl(DrmIoctl request, void *arg) override {
        int ret = -1;
        ioctlCalled = true;
        if (forceIoctlAnswer) {
            return setIoctlAnswer;
        }
        switch (request) {

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

    bool allowDebugAttachCallBase = false;
    bool allowDebugAttach = false;
    bool ioctlCalled = false;

    int forceIoctlAnswer = 0;
    int setIoctlAnswer = 0;
    int gemVmBindReturn = 0;

    alignas(64) std::vector<uint8_t> queryTopology;

    // Debugger ioctls
    int debuggerOpenRetval = 10; // debugFd
    uint32_t debuggerOpenVersion = 0;
};
