/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"

#include "test.h"

using namespace NEO;

TEST(XeHPDeviceIdTest, GivenSupportedDeviceIdThenConfigIsCorrect) {
    DeviceDescriptor expectedDescriptor = {0x0201, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo, GTTYPE_GT4};

    auto compareStructs = [](const DeviceDescriptor *first, const DeviceDescriptor *second) {
        return first->deviceId == second->deviceId && first->pHwInfo == second->pHwInfo &&
               first->setupHardwareInfo == second->setupHardwareInfo && first->eGtType == second->eGtType;
    };

    bool found = false;

    for (size_t i = 0; deviceDescriptorTable[i].deviceId != 0; i++) {
        if (compareStructs(&expectedDescriptor, &deviceDescriptorTable[i])) {
            found = true;
            break;
        }
    };

    EXPECT_TRUE(found);
}
