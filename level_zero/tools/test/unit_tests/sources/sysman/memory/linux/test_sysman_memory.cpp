/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/memory/linux/os_memory_imp.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/memory/linux/mock_memory.h"

#include "gtest/gtest.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr uint32_t memoryHandleComponentCount = 1u;
class SysmanDeviceMemoryFixture : public SysmanDeviceFixture {
  protected:
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();

        pMemoryManagerOld = device->getDriverHandle()->getMemoryManager();
        pMemoryManager = new ::testing::NiceMock<MockMemoryManagerSysman>(*neoDevice->getExecutionEnvironment());
        pMemoryManager->localMemorySupported[0] = false;
        device->getDriverHandle()->setMemoryManager(pMemoryManager);

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
        getMemoryHandles(0);
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        device->getDriverHandle()->setMemoryManager(pMemoryManagerOld);
        if (pMemoryManager != nullptr) {
            delete pMemoryManager;
            pMemoryManager = nullptr;
        }
        SysmanDeviceFixture::TearDown();
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

    std::vector<zes_mem_handle_t> getMemoryHandles(uint32_t count) {
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }

    MockMemoryManagerSysman *pMemoryManager = nullptr;
    MemoryManager *pMemoryManagerOld;
};

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    setLocalSupportedAndReinit(false);
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDeviceMemoryFixture, GivenInvalidComponentCountWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    setLocalSupportedAndReinit(false);
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    setLocalSupportedAndReinit(true);
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, GivenInvalidComponentCountWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    setLocalSupportedAndReinit(true);
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesThenValidPowerHandlesIsReturned) {
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

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetPropertiesThenVerifySysmanMemoryGetPropertiesCallReturnSuccess) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties;
        EXPECT_EQ(zesMemoryGetProperties(handle, &properties), ZE_RESULT_SUCCESS);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetStateThenVerifySysmanMemoryGetStateCallReturnUnsupportedFeature) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_state_t state;
        EXPECT_EQ(zesMemoryGetState(handle, &state), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetBandwidthThenVerifySysmanMemoryGetBandwidthCallReturnUnsupportedFeature) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_bandwidth_t bandwidth;
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

TEST_F(SysmanMultiDeviceFixture, GivenValidDevicePointerWhenGettingMemoryPropertiesThenValidMemoryPropertiesRetrieved) {
    zes_mem_properties_t properties = {};
    ze_device_properties_t deviceProperties = {};
    ze_bool_t isSubDevice = deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE;
    Device::fromHandle(device)->getProperties(&deviceProperties);
    LinuxMemoryImp *pLinuxMemoryImp = new LinuxMemoryImp(pOsSysman, isSubDevice, deviceProperties.subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxMemoryImp->getProperties(&properties));
    EXPECT_EQ(properties.subdeviceId, deviceProperties.subdeviceId);
    EXPECT_EQ(properties.onSubdevice, isSubDevice);
    delete pLinuxMemoryImp;
}

} // namespace ult
} // namespace L0
