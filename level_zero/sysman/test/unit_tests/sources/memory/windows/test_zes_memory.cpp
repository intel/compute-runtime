/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/memory/windows/mock_memory.h"

namespace L0 {
namespace Sysman {
namespace ult {

TEST_F(SysmanDeviceMemoryFixture, DISABLED_GivenComponentCountZeroWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    setLocalSupportedAndReinit(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, DISABLED_GivenInvalidComponentCountWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    setLocalSupportedAndReinit(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, DISABLED_GivenComponentCountZeroWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidPowerHandlesIsReturned) {
    setLocalSupportedAndReinit(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);

    std::vector<zes_mem_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceMemoryFixture, DISABLED_GivenComponentCountZeroWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenZeroCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    setLocalSupportedAndReinit(false);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, DISABLED_GivenValidMemoryHandleWhenCallingGettingPropertiesWithLocalMemoryThenCallSucceeds) {
    pKmdSysManager->mockMemoryLocation = KmdSysman::MemoryLocationsType::DeviceMemory;
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties;

        ze_result_t result = zesMemoryGetProperties(handle, &properties);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(properties.type, ZES_MEM_TYPE_GDDR6);
        EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.physicalSize, pKmdSysManager->mockMemoryPhysicalSize);
        EXPECT_EQ(static_cast<uint32_t>(properties.numChannels), pKmdSysManager->mockMemoryChannels);
        EXPECT_EQ(static_cast<uint32_t>(properties.busWidth), pKmdSysManager->mockMemoryBus);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingGettingPropertiesThenCallSucceeds) {
    pKmdSysManager->mockMemoryDomains = 1;
    clearMemHandleListAndReinit();

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 1u);

    std::vector<zes_mem_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);

    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_mem_properties_t properties{};
        EXPECT_EQ(zesMemoryGetProperties(handle, &properties), ZE_RESULT_SUCCESS);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
            EXPECT_EQ(properties.location, ZES_MEM_LOC_SYSTEM);
            EXPECT_GT(properties.physicalSize, 0u);
            EXPECT_EQ(properties.numChannels, -1);
            EXPECT_EQ(properties.busWidth, -1);
        } else {
            EXPECT_EQ(properties.type, ZES_MEM_TYPE_GDDR6);
            EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
            EXPECT_EQ(properties.physicalSize, pKmdSysManager->mockMemoryPhysicalSize);
            EXPECT_EQ(static_cast<uint32_t>(properties.numChannels), pKmdSysManager->mockMemoryChannels);
            EXPECT_EQ(static_cast<uint32_t>(properties.busWidth), pKmdSysManager->mockMemoryBus);
        }
    }
}

TEST_F(SysmanDeviceMemoryFixture, DISABLED_GivenValidMemoryHandleWhenGettingStateThenCallSucceeds) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_state_t state;

        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_OK);
        EXPECT_GT(state.size, 0u);
        EXPECT_GT(state.free, 0u);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenGettingStateThenCallSucceedsAndProperSizeIsReturned) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    pKmdSysManager->mockMemoryCurrentTotalAllocableMem = 4294813695;

    for (auto handle : handles) {
        zes_mem_state_t state;

        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_OK);
        EXPECT_EQ(state.size, pKmdSysManager->mockMemoryCurrentTotalAllocableMem);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidOsMemoryObjectWhenGettingMemoryBandWidthThenCallSucceeds) {
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelperMemory>();
    std::swap(pWddmSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    std::unique_ptr<WddmMemoryImp> pWddmMemoryImp = std::make_unique<WddmMemoryImp>(pOsSysman, false, 0);
    zes_mem_bandwidth_t mockedBandwidth = {
        mockMemoryCurrentBandwidthRead,
        mockMemoryCurrentBandwidthWrite,
        mockMemoryMaxBandwidth, mockMemoryBandwidthTimestamp};
    zes_mem_bandwidth_t bandwidth = mockedBandwidth;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pWddmMemoryImp->getBandwidth(&bandwidth));
    EXPECT_EQ(mockMemoryCurrentBandwidthRead, bandwidth.readCounter);
    EXPECT_EQ(mockMemoryCurrentBandwidthWrite, bandwidth.writeCounter);
    EXPECT_EQ(mockMemoryMaxBandwidth, bandwidth.maxBandwidth);
    EXPECT_EQ(mockMemoryBandwidthTimestamp, bandwidth.timestamp);
}

TEST_F(SysmanDeviceMemoryFixture, GivenMockedComponentCountZeroWhenEnumeratingMemoryModulesThenExpectNonZeroCountAndValidHandlesForIntegratedPlatforms) {
    pKmdSysManager->mockMemoryDomains = 0;
    clearMemHandleListAndReinit();

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);

    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(count, 1u);
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        for (auto handle : handles) {
            EXPECT_NE(handle, nullptr);
        }
    } else {
        EXPECT_EQ(count, 0u);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenMockedComponentCountZeroWhenEnumeratingMemoryModulesThenExpectNonZeroCountAndValidPropertiesForIntegratedPlatforms) {
    pKmdSysManager->mockMemoryDomains = 0;
    clearMemHandleListAndReinit();

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);

    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(count, 1u);
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        for (auto handle : handles) {
            EXPECT_NE(handle, nullptr);
            zes_mem_properties_t properties{};
            EXPECT_EQ(zesMemoryGetProperties(handle, &properties), ZE_RESULT_SUCCESS);

            EXPECT_EQ(properties.location, ZES_MEM_LOC_SYSTEM);
            EXPECT_FALSE(properties.onSubdevice);
            EXPECT_EQ(properties.subdeviceId, 0u);
            EXPECT_GT(properties.physicalSize, 0u);
        }
    } else {
        EXPECT_EQ(count, 0u);
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
