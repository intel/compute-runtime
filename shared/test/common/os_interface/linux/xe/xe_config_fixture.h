/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
namespace NEO {
class XeConfigFixture {
  public:
    XeConfigFixture() {
        const auto &hwInfo = *defaultHwInfo;
        static uint64_t queryConfig[2]{}; // 1 qword for num params and 1 qwords per param
        auto xeQueryConfig = reinterpret_cast<drm_xe_query_config *>(queryConfig);
        xeQueryConfig->num_params = 1;
        xeQueryConfig->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] = (hwInfo.platform.usRevId << 16) | hwInfo.platform.usDeviceID;

        SysCalls::sysCallsIoctl = [](int fileDescriptor, unsigned long int request, void *arg) -> int {
            if (request == DRM_IOCTL_XE_DEVICE_QUERY) {
                struct drm_xe_device_query *deviceQuery = static_cast<struct drm_xe_device_query *>(arg);
                if (deviceQuery->query == DRM_XE_DEVICE_QUERY_CONFIG) {
                    if (deviceQuery->data) {
                        memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryConfig, sizeof(queryConfig));
                    }
                    deviceQuery->size = sizeof(queryConfig);
                    return 0;
                }
            }
            return -1;
        };
    }

    void setUp(){};
    void tearDown(){};

    VariableBackup<decltype(SysCalls::sysCallsIoctl)> mockIoctl{&SysCalls::sysCallsIoctl};
};
} // namespace NEO
