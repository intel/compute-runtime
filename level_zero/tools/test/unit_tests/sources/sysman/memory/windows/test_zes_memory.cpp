/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/memory/windows/os_memory_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/memory/windows/mock_memory.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr uint32_t memoryHandleComponentCount = 1u;
class SysmanDeviceMemoryFixture : public SysmanDeviceFixture {
  protected:
    Mock<MemoryKmdSysManager> *pKmdSysManager = nullptr;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();

        pMemoryManagerOld = device->getDriverHandle()->getMemoryManager();

        pMemoryManager = new ::testing::NiceMock<MockMemoryManagerSysman>(*neoDevice->getExecutionEnvironment());

        pMemoryManager->localMemorySupported[0] = false;

        device->getDriverHandle()->setMemoryManager(pMemoryManager);

        pKmdSysManager = new Mock<MemoryKmdSysManager>;

        EXPECT_CALL(*pKmdSysManager, escape(_, _, _, _, _))
            .WillRepeatedly(::testing::Invoke(pKmdSysManager, &Mock<MemoryKmdSysManager>::mock_escape));

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager;

        for (auto handle : pSysmanDeviceImp->pMemoryHandleContext->handleList) {
            delete handle;
        }

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

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        device->getDriverHandle()->setMemoryManager(pMemoryManagerOld);
        SysmanDeviceFixture::TearDown();
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        if (pKmdSysManager != nullptr) {
            delete pKmdSysManager;
            pKmdSysManager = nullptr;
        }
        if (pMemoryManager != nullptr) {
            delete pMemoryManager;
            pMemoryManager = nullptr;
        }
    }

    void setLocalSupportedAndReinit(bool supported) {
        pMemoryManager->localMemorySupported[0] = supported;

        for (auto handle : pSysmanDeviceImp->pMemoryHandleContext->handleList) {
            delete handle;
        }

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

    std::vector<zes_mem_handle_t> get_memory_handles(uint32_t count) {
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }

    MockMemoryManagerSysman *pMemoryManager = nullptr;
    MemoryManager *pMemoryManagerOld;
};

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    setLocalSupportedAndReinit(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, GivenInvalidComponentCountWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    setLocalSupportedAndReinit(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidPowerHandlesIsReturned) {
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

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenZeroCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    setLocalSupportedAndReinit(false);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingGettingPropertiesWithLocalMemoryThenCallSucceeds) {
    pKmdSysManager->mockMemoryLocation = KmdSysman::MemoryLocationsType::DeviceMemory;
    setLocalSupportedAndReinit(true);

    auto handles = get_memory_handles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties;

        ze_result_t result = zesMemoryGetProperties(handle, &properties);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(properties.type, ZES_MEM_TYPE_GDDR6);
        EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.physicalSize, pKmdSysManager->mockMemoryPhysicalSize);
        EXPECT_EQ(properties.numChannels, pKmdSysManager->mockMemoryChannels);
        EXPECT_EQ(properties.busWidth, pKmdSysManager->mockMemoryBus);
    }
}

TEST_F(SysmanDeviceMemoryFixture, DISABLED_GivenValidMemoryHandleWhenGettingStateThenCallSucceeds) {
    setLocalSupportedAndReinit(true);
    auto handles = get_memory_handles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_state_t state;

        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_OK);
        EXPECT_GT(state.size, 0u);
        EXPECT_GT(state.free, 0u);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenGettingBandwidthThenCallSucceeds) {
    setLocalSupportedAndReinit(true);
    auto handles = get_memory_handles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_bandwidth_t bandwidth;

        ze_result_t result = zesMemoryGetBandwidth(handle, &bandwidth);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(bandwidth.maxBandwidth, pKmdSysManager->mockMemoryMaxBandwidth * MbpsToBytesPerSecond);
        EXPECT_EQ(bandwidth.readCounter, pKmdSysManager->mockMemoryCurrentBandwidthRead);
        EXPECT_EQ(bandwidth.writeCounter, pKmdSysManager->mockMemoryCurrentBandwidthWrite);
        EXPECT_GT(bandwidth.timestamp, 0u);
    }
}

} // namespace ult
} // namespace L0
