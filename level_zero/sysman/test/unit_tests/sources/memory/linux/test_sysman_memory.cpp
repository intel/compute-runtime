/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/memory/linux/mock_memory.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_i915.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

static const uint32_t memoryHandleComponentCount = 1u;

class SysmanMemoryMockIoctlHelper : public NEO::MockIoctlHelper {

  public:
    using NEO::MockIoctlHelper::MockIoctlHelper;
    bool returnEmptyMemoryInfo = false;
    int32_t mockErrorNumber = 0;

    std::unique_ptr<MemoryInfo> createMemoryInfo() override {
        if (returnEmptyMemoryInfo) {
            errno = mockErrorNumber;
            return {};
        }
        return NEO::MockIoctlHelper::createMemoryInfo();
    }
};

class SysmanDeviceMemoryFixture : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    DebugManagerStateRestore restorer;

    std::vector<zes_mem_handle_t> getMemoryHandles(uint32_t count) {
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }

    void setLocalSupportedAndReinit(bool supported) {
        debugManager.flags.EnableLocalMemory.set(supported == true ? 1 : 0);
        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        pSysmanDeviceImp->pMemoryHandleContext->init(pOsSysman->getSubDeviceCount());
    }
};

class SysmanDeviceMemoryFixtureI915 : public SysmanDeviceMemoryFixture {
  protected:
    MockMemoryNeoDrm *pDrm = nullptr;

    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        SysmanDeviceFixture::SetUp();
        device = pSysmanDeviceImp;
        auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        pLinuxSysmanImp->pSysmanProductHelper = std::move(pSysmanProductHelper);
        pDrm = new MockMemoryNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockMemoryNeoDrm>(pDrm));
        pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
        pDrm->ioctlHelper = static_cast<std::unique_ptr<NEO::IoctlHelper>>(std::make_unique<SysmanMemoryMockIoctlHelper>(*pDrm));
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanDeviceMemoryFixtureI915, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenZeroCountIsReturnedForDiscretePlatforms) {
    if (!defaultHwInfo->capabilityTable.isIntegratedDevice) {
        setLocalSupportedAndReinit(false);
    }
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(count, 1u);
    } else {
        EXPECT_EQ(count, 0u);
    }
}

TEST_F(SysmanDeviceMemoryFixtureI915, GivenInvalidComponentCountWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenZeroCountIsReturnedForDiscretePlatforms) {
    if (!defaultHwInfo->capabilityTable.isIntegratedDevice) {
        setLocalSupportedAndReinit(false);
    }
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(count, 1u);
    } else {
        EXPECT_EQ(count, 0u);
    }

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(count, 1u);
    } else {
        EXPECT_EQ(count, 0u);
    }
}

TEST_F(SysmanDeviceMemoryFixtureI915, GivenComponentCountZeroWhenEnumeratingMemoryModulesThenValidCountIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixtureI915, GivenInvalidComponentCountWhenEnumeratingMemoryModulesThenValidCountIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixtureI915, GivenComponentCountZeroWhenEnumeratingMemoryModulesThenValidHandlesAreReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);

    std::vector<zes_mem_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingMemoryGetStateAndFwUtilInterfaceIsAbsentThenMemoryHealthWillBeUnknown) {
    VariableBackup<L0::Sysman::FirmwareUtil *> backup(&pLinuxSysmanImp->pFwUtilInterface);
    pLinuxSysmanImp->pFwUtilInterface = nullptr;
    auto pLinuxMemoryImp = std::make_unique<PublicLinuxMemoryImp>(pOsSysman, true, 0);
    zes_mem_state_t state;
    ze_result_t result = pLinuxMemoryImp->getState(&state);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(state.health, ZES_MEM_HEALTH_UNKNOWN);
}

TEST_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesSysmanMemoryGetStateAndIoctlReturnedErrorThenApiReturnsError) {
    REQUIRE_DISCRETE_DEVICE_OR_SKIP(*defaultHwInfo);
    auto ioctlHelper = static_cast<SysmanMemoryMockIoctlHelper *>(pDrm->ioctlHelper.get());
    ioctlHelper->returnEmptyMemoryInfo = true;
    ioctlHelper->mockErrorNumber = EBUSY;
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_state_t state;

        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
        EXPECT_EQ(state.size, 0u);
        EXPECT_EQ(state.free, 0u);
    }
}

TEST_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesSysmanMemoryGetStateAndDeviceIsNotAvailableThenDeviceLostErrorIsReturned) {
    REQUIRE_DISCRETE_DEVICE_OR_SKIP(*defaultHwInfo);
    auto ioctlHelper = static_cast<SysmanMemoryMockIoctlHelper *>(pDrm->ioctlHelper.get());
    ioctlHelper->returnEmptyMemoryInfo = true;
    ioctlHelper->mockErrorNumber = ENODEV;
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_state_t state;

        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_ERROR_DEVICE_LOST);
        EXPECT_EQ(state.size, 0u);
        EXPECT_EQ(state.free, 0u);
        errno = 0;
    }
}

class SysmanMultiDeviceMemoryFixture : public SysmanMultiDeviceFixture {
  protected:
    MockMemoryNeoDrm *pDrm = nullptr;
    L0::Sysman::SysmanDevice *device = nullptr;
    MockMemorySysFsAccessInterface *pSysfsAccess = nullptr;
    MockSysmanKmdInterfacePrelim *pSysmanKmdInterface = nullptr;
    MockMemoryFsAccessInterface *pFsAccess = nullptr;

    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        SysmanMultiDeviceFixture::SetUp();

        pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
        pSysfsAccess = new MockMemorySysFsAccessInterface();
        pFsAccess = new MockMemoryFsAccessInterface();
        pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
        pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pLinuxSysmanImp->pSysfsAccess = pLinuxSysmanImp->pSysmanKmdInterface->getSysFsAccess();
        pLinuxSysmanImp->pFsAccess = pLinuxSysmanImp->pSysmanKmdInterface->getFsAccess();
        pDrm = new MockMemoryNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->ioctlHelper = static_cast<std::unique_ptr<NEO::IoctlHelper>>(std::make_unique<NEO::MockIoctlHelper>(*pDrm));
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockMemoryNeoDrm>(pDrm));
        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        device = pSysmanDeviceImp;
    }

    void TearDown() override {
        SysmanMultiDeviceFixture::TearDown();
    }

    std::vector<zes_mem_handle_t> getMemoryHandles(uint32_t count) {
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanMultiDeviceMemoryFixture, GivenMemFreeAndMemAvailableMissingInMemInfoWhenCallingGetStateThenFreeAndSizeValuesAreZero) {
    REQUIRE_INTEGRATED_DEVICE_OR_SKIP(*defaultHwInfo);
    pFsAccess->customMemInfo = {
        "Buffers: 158772 kB",
        "Cached: 11744244 kB"};
    auto handles = getMemoryHandles(pOsSysman->getSubDeviceCount());
    zes_mem_state_t state = {};
    ze_result_t result = zesMemoryGetState(handles[0], &state);

    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(state.free, 0u);
    EXPECT_EQ(state.size, 0u);
}

TEST_F(SysmanMultiDeviceMemoryFixture, GivenMemInfoWithoutColonSeparatorWhenCallingGetStateThenFreeAndSizeValuesAreZero) {
    REQUIRE_INTEGRATED_DEVICE_OR_SKIP(*defaultHwInfo);
    pFsAccess->customMemInfo = {
        "Buffers 158772 kB",
        "Cached 11744244 kB"};
    auto handles = getMemoryHandles(pOsSysman->getSubDeviceCount());
    zes_mem_state_t state = {};
    ze_result_t result = zesMemoryGetState(handles[0], &state);

    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(state.free, 0u);
    EXPECT_EQ(state.size, 0u);
}

TEST_F(SysmanMultiDeviceMemoryFixture, GivenMemInfoIsEmptyWhenCallingGetStateThenFreeAndSizeValuesAreZero) {
    REQUIRE_INTEGRATED_DEVICE_OR_SKIP(*defaultHwInfo);
    pFsAccess->customMemInfo = {""};
    auto handles = getMemoryHandles(pOsSysman->getSubDeviceCount());
    zes_mem_state_t state = {};
    ze_result_t result = zesMemoryGetState(handles[0], &state);

    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(state.free, 0u);
    EXPECT_EQ(state.size, 0u);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
