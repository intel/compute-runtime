/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/test/common/test_macros/test.h"

#include "device_ids_configs.h"

#include <map>
#include <vector>

using namespace NEO;

namespace {
// ARL-S device ids are considered as MTL-S ip version, so they are part of MTL product config (mtlmDeviceIds)
const std::map<unsigned short, PRODUCT_FAMILY> runtimeProductFamilyOverrides = {
    {0x7D41, IGFX_ARROWLAKE},
    {0x7D67, IGFX_ARROWLAKE},
};

std::map<unsigned short, PRODUCT_FAMILY> getDeviceIdsFromDeviceDescriptorTable() {
    std::map<unsigned short, PRODUCT_FAMILY> tableDeviceIds;
    for (auto descriptor = deviceDescriptorTable; descriptor->deviceId != 0; descriptor++) {
        auto inserted = tableDeviceIds.emplace(descriptor->deviceId, descriptor->productFamily).second;
        EXPECT_TRUE(inserted) << "duplicated device id 0x" << std::hex << descriptor->deviceId;
    }
    return tableDeviceIds;
}

std::map<unsigned short, PRODUCT_FAMILY> getDeviceIdsFromProductConfigs() {
    std::map<unsigned short, PRODUCT_FAMILY> configDeviceIds;
    auto addDeviceIds = [&configDeviceIds](const std::vector<unsigned short> &deviceIds, PRODUCT_FAMILY configProductFamily) {
        for (auto deviceId : deviceIds) {
            auto productFamily = configProductFamily;
            auto overrideIt = runtimeProductFamilyOverrides.find(deviceId);
            if (overrideIt != runtimeProductFamilyOverrides.end()) {
                if (hardwareInfoTable[overrideIt->second] == nullptr) {
                    continue;
                }
                productFamily = overrideIt->second;
            }
            auto entry = configDeviceIds.emplace(deviceId, productFamily).first;
            EXPECT_EQ(productFamily, entry->second) << "device id 0x" << std::hex << deviceId << " assigned to multiple products";
        }
    };
#define DEVICE_CONFIG(ignoredProductConfig, productFamily, deviceIds, ignoredAotFamily, ignoredRelease) addDeviceIds(NEO::deviceIds, productFamily);
#include "product_config.inl"
#undef DEVICE_CONFIG
    return configDeviceIds;
}
} // namespace

TEST(DeviceDescriptorTableTest, givenDeviceDescriptorTableThenDeviceIdsAreUniqueAndMapToSupportedProducts) {
    auto tableDeviceIds = getDeviceIdsFromDeviceDescriptorTable();
    EXPECT_FALSE(tableDeviceIds.empty());

    for (const auto &[deviceId, productFamily] : tableDeviceIds) {
        EXPECT_NE(IGFX_UNKNOWN, productFamily) << "device id 0x" << std::hex << deviceId;
        EXPECT_NE(nullptr, hardwareInfoTable[productFamily]) << "device id 0x" << std::hex << deviceId;
    }
}

TEST(DeviceDescriptorTableTest, givenDeviceDescriptorTableAndProductConfigsThenSameDeviceIdsAreMappedToSameProducts) {
    auto tableDeviceIds = getDeviceIdsFromDeviceDescriptorTable();
    auto configDeviceIds = getDeviceIdsFromProductConfigs();

    for (const auto &[deviceId, productFamily] : tableDeviceIds) {
        auto configIt = configDeviceIds.find(deviceId);
        if (configIt == configDeviceIds.end()) {
            ADD_FAILURE() << "device id 0x" << std::hex << deviceId << " missing in product config device ids";
            continue;
        }
        EXPECT_EQ(productFamily, configIt->second) << "device id 0x" << std::hex << deviceId;
    }

    for (const auto &[deviceId, productFamily] : configDeviceIds) {
        EXPECT_NE(tableDeviceIds.end(), tableDeviceIds.find(deviceId)) << "device id 0x" << std::hex << deviceId << " missing in device descriptor table";
    }
}
