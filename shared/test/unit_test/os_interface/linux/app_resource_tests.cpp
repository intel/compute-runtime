/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/app_resource_helper.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "test.h"

using namespace NEO;

TEST(AppResourceLinuxTests, givenGraphicsAllocationTypeWhenCreatingStorageInfoFromPropertiesThenResourceTagAlwaysEmpty) {
    MockMemoryManager mockMemoryManager;
    const DeviceBitfield singleTileMask{static_cast<uint32_t>(1u << 2)};

    auto allocationType = GraphicsAllocation::AllocationType::BUFFER;
    AllocationProperties properties{mockRootDeviceIndex, false, 1u, allocationType, false, singleTileMask};

    auto tag = AppResourceHelper::getResourceTagStr(properties.allocationType);
    EXPECT_STREQ("", tag);

    auto storageInfo = mockMemoryManager.createStorageInfoFromProperties(properties);
    EXPECT_STREQ(tag, storageInfo.resourceTag);
}
