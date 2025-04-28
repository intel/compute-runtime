/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/memory/windows/os_memory_imp.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/memory/windows/mock_memory.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr uint32_t memoryHandleComponentCount = 1u;
class SysmanDeviceMemoryFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<Mock<MemoryKmdSysManager>> pKmdSysManager;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();

        pKmdSysManager.reset(new Mock<MemoryKmdSysManager>);

        pKmdSysManager->allowSetCalls = true;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        std::vector<ze_device_handle_t> deviceHandles;
        // We received a device handle. Check for subdevices in this device
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        getMemoryHandles(0);
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }

    void setLocalSupportedAndReinit(bool supported) {
        pMemoryManager->localMemorySupported[0] = supported;

        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        std::vector<ze_device_handle_t> deviceHandles;
        // We received a device handle. Check for subdevices in this device
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        pSysmanDeviceImp->pMemoryHandleContext->init(deviceHandles);
    }

    void clearMemHandleListAndReinit() {
        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        std::vector<ze_device_handle_t> deviceHandles{};
        device->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            device->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        pSysmanDeviceImp->pMemoryHandleContext->init(deviceHandles);
    }

    std::vector<zes_mem_handle_t> getMemoryHandles(uint32_t count) {
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }

    MockMemoryManagerSysman *pMemoryManager = nullptr;
};

TEST_F(SysmanDeviceMemoryFixture, DISABLED_GivenComponentCountZeroWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    setLocalSupportedAndReinit(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, DISABLED_GivenInvalidComponentCountWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    setLocalSupportedAndReinit(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, DISABLED_GivenComponentCountZeroWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidPowerHandlesIsReturned) {
    setLocalSupportedAndReinit(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);

    std::vector<zes_mem_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceMemoryFixture, DISABLED_GivenComponentCountZeroWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenZeroCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    setLocalSupportedAndReinit(false);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
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
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 1u);

    std::vector<zes_mem_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);

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

TEST_F(SysmanDeviceMemoryFixture, DISABLED_GivenValidMemoryHandleWhenGettingBandwidthThenCallSucceeds) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_bandwidth_t bandwidth;

        ze_result_t result = zesMemoryGetBandwidth(handle, &bandwidth);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(bandwidth.maxBandwidth, pKmdSysManager->mockMemoryMaxBandwidth * mbpsToBytesPerSecond);
        EXPECT_EQ(bandwidth.readCounter, pKmdSysManager->mockMemoryCurrentBandwidthRead);
        EXPECT_EQ(bandwidth.writeCounter, pKmdSysManager->mockMemoryCurrentBandwidthWrite);
        EXPECT_GT(bandwidth.timestamp, 0u);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenMockedComponentCountZeroWhenEnumeratingMemoryModulesThenExpectNonZeroCountAndValidHandlesForIntegratedPlatforms) {
    pKmdSysManager->mockMemoryDomains = 0;
    clearMemHandleListAndReinit();

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);

    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(count, 1u);
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
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
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);

    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(count, 1u);
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
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
} // namespace L0
