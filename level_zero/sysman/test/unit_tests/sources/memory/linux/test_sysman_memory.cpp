/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/memory/linux/mock_memory.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t memoryHandleComponentCount = 1u;
class SysmanDeviceMemoryFixture : public SysmanDeviceFixture {
  protected:
    void SetUp() override {
        DebugManager.flags.EnableLocalMemory.set(1);
        SysmanDeviceFixture::SetUp();

        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        device = pSysmanDeviceImp;
        getMemoryHandles(0);
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    void setLocalSupportedAndReinit(bool supported) {

        DebugManager.flags.EnableLocalMemory.set(supported == true ? 1 : 0);

        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        pSysmanDeviceImp->pMemoryHandleContext->init(pOsSysman->getSubDeviceCount());
    }

    std::vector<zes_mem_handle_t> getMemoryHandles(uint32_t count) {
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }

    L0::Sysman::SysmanDevice *device = nullptr;
};

HWTEST2_F(SysmanDeviceMemoryFixture, GivenSysmanProductHelperInstanceWhenCallingMemoryAPIsThenErrorIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, static_cast<const L0::Sysman::LinuxSysmanImp *>(pLinuxSysmanImp));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    zes_mem_bandwidth_t bandwidth;
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, static_cast<const L0::Sysman::LinuxSysmanImp *>(pLinuxSysmanImp));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenSysmanProductHelperInstanceWhenCallingMemoryAPIsThenErrorIsReturned, IsDG1) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, static_cast<const L0::Sysman::LinuxSysmanImp *>(pLinuxSysmanImp));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    zes_mem_bandwidth_t bandwidth;
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, static_cast<const L0::Sysman::LinuxSysmanImp *>(pLinuxSysmanImp));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenSysmanProductHelperInstanceWhenCallingMemoryAPIsThenErrorIsReturned, IsDG2) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, static_cast<const L0::Sysman::LinuxSysmanImp *>(pLinuxSysmanImp));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    zes_mem_bandwidth_t bandwidth;
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, static_cast<const L0::Sysman::LinuxSysmanImp *>(pLinuxSysmanImp));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturned) {
    setLocalSupportedAndReinit(false);
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDeviceMemoryFixture, GivenInvalidComponentCountWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturned) {
    setLocalSupportedAndReinit(false);
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenValidCountIsReturned) {
    setLocalSupportedAndReinit(true);
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, GivenInvalidComponentCountWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenValidCountIsReturned) {
    setLocalSupportedAndReinit(true);
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesThenValidHandlesIsReturned) {
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
