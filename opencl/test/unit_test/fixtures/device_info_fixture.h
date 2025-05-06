/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include <cstring>

namespace NEO {

struct GetDeviceInfoMemCapabilitiesTest : ::testing::Test {
    struct TestParams {
        cl_uint paramName;
        cl_unified_shared_memory_capabilities_intel expectedCapabilities;
    };

    void check(std::vector<TestParams> &params) {
        DebugManagerStateRestore restore;
        debugManager.flags.EnableSharedSystemUsmSupport.set(0);
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
        const bool isKmdMigrationAvailable{device->getMemoryManager()->isKmdMigrationAvailable(device->getRootDeviceIndex())};

        for (auto &param : params) {
            cl_unified_shared_memory_capabilities_intel unifiedSharedMemoryCapabilities{};
            size_t paramRetSize;

            const auto retVal = device->getDeviceInfo(param.paramName,
                                                      sizeof(cl_unified_shared_memory_capabilities_intel),
                                                      &unifiedSharedMemoryCapabilities, &paramRetSize);
            EXPECT_EQ(CL_SUCCESS, retVal);
            if ((param.paramName == CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL) && (isKmdMigrationAvailable)) {
                EXPECT_EQ((param.expectedCapabilities | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS_INTEL), unifiedSharedMemoryCapabilities);
            } else {
                EXPECT_EQ(param.expectedCapabilities, unifiedSharedMemoryCapabilities);
            }
            EXPECT_EQ(sizeof(cl_unified_shared_memory_capabilities_intel), paramRetSize);
        }
    }
};

struct QueueFamilyNameTest : ::testing::Test {
    void SetUp() override {
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    }

    void verify(EngineGroupType type, const char *expectedName) {
        char name[CL_QUEUE_FAMILY_MAX_NAME_SIZE_INTEL];
        device->getQueueFamilyName(name, type);
        EXPECT_EQ(0, std::strcmp(name, expectedName));
    }

    std::unique_ptr<MockClDevice> device = {};
};

} // namespace NEO
