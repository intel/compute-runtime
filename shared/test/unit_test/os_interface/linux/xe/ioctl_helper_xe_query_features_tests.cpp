/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/linux/xe/ioctl_helper_xe_tests.h"

class DrmMockXeQueryFeatures : public DrmMockXe {
  public:
    using DrmMockXe::DrmMockXe;

    int ioctl(DrmIoctl request, void *arg) override {
        switch (request) {
        case DrmIoctl::gemVmCreate: {
            auto vmCreate = static_cast<drm_xe_vm_create *>(arg);

            if ((vmCreate->flags & DRM_XE_VM_CREATE_FLAG_FAULT_MODE) == DRM_XE_VM_CREATE_FLAG_FAULT_MODE &&
                (vmCreate->flags & DRM_XE_VM_CREATE_FLAG_LR_MODE) == DRM_XE_VM_CREATE_FLAG_LR_MODE &&
                (!supportsRecoverablePageFault)) {
                return -EINVAL;
            }
            return 0;
        } break;

        default:
            return DrmMockXe::ioctl(request, arg);
        }
    };
    bool supportsRecoverablePageFault = true;
};

TEST(IoctlHelperXeQueryFeaturesTest, whenInitializeIoctlHelperThenQueryRecoverablePageFaultSupport) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXeQueryFeatures drm{*executionEnvironment->rootDeviceEnvironments[0]};

    for (const auto &recoverablePageFault : ::testing::Bool()) {
        drm.supportsRecoverablePageFault = recoverablePageFault;
        auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);
        xeIoctlHelper->initialize();
        EXPECT_EQ(xeIoctlHelper->supportedFeatures.flags.pageFault, recoverablePageFault);
    }
}