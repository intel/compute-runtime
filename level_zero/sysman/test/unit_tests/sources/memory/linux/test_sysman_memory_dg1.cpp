/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/memory/linux/mock_memory.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t memoryHandleComponentCount = 1u;
class SysmanDeviceMemoryFixture : public SysmanDeviceFixture {

    DebugManagerStateRestore restorer;

  protected:
    MockMemoryNeoDrm *pDrm = nullptr;
    Drm *pOriginalDrm = nullptr;

    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        SysmanDeviceFixture::SetUp();
        pDrm = new MockMemoryNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->ioctlHelper = static_cast<std::unique_ptr<NEO::IoctlHelper>>(std::make_unique<IoctlHelperUpstream>(*pDrm));
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockMemoryNeoDrm>(pDrm));

        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        getMemoryHandles(0);
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    void setLocalSupportedAndReinit(bool supported) {

        debugManager.flags.EnableLocalMemory.set(supported == true ? 1 : 0);

        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        pSysmanDeviceImp->pMemoryHandleContext->init(pOsSysman->getSubDeviceCount());
    }

    std::vector<zes_mem_handle_t> getMemoryHandles(uint32_t count) {
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturned) {
    setLocalSupportedAndReinit(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, GivenInvalidComponentCountWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturned) {
    setLocalSupportedAndReinit(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidHandlesIsReturned) {
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

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenZeroCountIsReturned) {
    setLocalSupportedAndReinit(false);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDeviceMemoryFixture, GivenInvalidComponentCountWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenZeroCountIsReturned) {
    setLocalSupportedAndReinit(false);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenValidHandlesIsReturned) {
    setLocalSupportedAndReinit(false);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);

    std::vector<zes_mem_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenGettingPropertiesWithLocalMemoryThenCallSucceeds) {
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties;

        ze_result_t result = zesMemoryGetProperties(handle, &properties);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(properties.type, ZES_MEM_TYPE_DDR);
        EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.physicalSize, 0u);
        EXPECT_EQ(properties.numChannels, -1);
        EXPECT_EQ(properties.busWidth, -1);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenGettingStateThenCallSucceeds) {
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_state_t state;

        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_OK);
        EXPECT_EQ(state.size, probedSizeRegionOne);
        EXPECT_EQ(state.free, unallocatedSizeRegionOne);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleAndIfQueryMemoryInfoFailsWhenGettingStateThenErrorIsReturned) {
    setLocalSupportedAndReinit(true);
    pDrm->mockQueryMemoryInfoReturnStatus.push_back(false);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_state_t state;
        EXPECT_EQ(zesMemoryGetState(handle, &state), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleAndIfQueryMemoryInfoAndIfMemoryInfoIsNotCorrectWhenGettingStateThenErrorIsReturned) {
    setLocalSupportedAndReinit(true);
    pDrm->mockQueryMemoryInfoReturnStatus.push_back(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_state_t state;
        EXPECT_EQ(zesMemoryGetState(handle, &state), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenGettingBandwidthThenZeResultErrorUnsupportedFeatureIsReturned) {
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_bandwidth_t bandwidth;
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

TEST_F(SysmanMultiDeviceFixture, GivenValidDevicePointerWhenGettingMemoryPropertiesThenValidMemoryPropertiesRetrieved) {
    zes_mem_properties_t properties = {};
    ze_bool_t isSubdevice = pLinuxSysmanImp->getSubDeviceCount() == 0 ? false : true;
    auto subDeviceId = std::max(0u, pLinuxSysmanImp->getSubDeviceCount() - 1);
    std::unique_ptr<L0::Sysman::LinuxMemoryImp> pLinuxMemoryImp = std::make_unique<L0::Sysman::LinuxMemoryImp>(pOsSysman, isSubdevice, subDeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxMemoryImp->getProperties(&properties));
    EXPECT_EQ(properties.subdeviceId, subDeviceId);
    EXPECT_EQ(properties.onSubdevice, isSubdevice);
}

} // namespace ult
} // namespace Sysman
} // namespace L0