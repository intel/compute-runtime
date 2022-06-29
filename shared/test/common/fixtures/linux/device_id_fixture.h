/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <array>

namespace NEO {
struct DeviceIdTests : public ::testing::Test {
    template <typename ArrayT>
    void testImpl(const ArrayT &expectedDescriptors) {
        size_t startIndex = 0;
        while (!compareStructs(&expectedDescriptors[0], &deviceDescriptorTable[startIndex]) &&
               deviceDescriptorTable[startIndex].deviceId != 0) {
            startIndex++;
        };
        EXPECT_NE(0u, deviceDescriptorTable[startIndex].deviceId);

        for (auto &expected : expectedDescriptors) {
            EXPECT_TRUE(compareStructs(&expected, &deviceDescriptorTable[startIndex]));
            startIndex++;
        }
    }

    bool compareStructs(const DeviceDescriptor *first, const DeviceDescriptor *second) {
        return first->deviceId == second->deviceId && first->pHwInfo == second->pHwInfo &&
               first->setupHardwareInfo == second->setupHardwareInfo;
    }
};

} // namespace NEO