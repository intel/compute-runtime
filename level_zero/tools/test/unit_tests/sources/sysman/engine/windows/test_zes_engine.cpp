/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/engine/windows/os_engine_imp.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/engine/windows/mock_engine.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr uint32_t engineHandleComponentCount = 3u;
class SysmanDeviceEngineFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<Mock<EngineKmdSysManager>> pKmdSysManager = nullptr;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();

        pKmdSysManager.reset(new Mock<EngineKmdSysManager>);

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_engine_handle_t> getEngineHandles(uint32_t count) {
        std::vector<zes_engine_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceEngineFixture, GivenComponentCountZeroWhenEnumeratingEngineGroupsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, Mock<EngineKmdSysManager>::mockEngineHandleComponentCount);
}

TEST_F(SysmanDeviceEngineFixture, GivenInvalidComponentCountWhenEnumeratingEngineGroupsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, Mock<EngineKmdSysManager>::mockEngineHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, Mock<EngineKmdSysManager>::mockEngineHandleComponentCount);
}

TEST_F(SysmanDeviceEngineFixture, GivenComponentCountZeroWhenEnumeratingEngineGroupsThenValidPowerHandlesIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, Mock<EngineKmdSysManager>::mockEngineHandleComponentCount);

    std::vector<zes_engine_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceEngineFixture, GivenComponentCountZeroWhenEnumeratingEngineGroupsAndRequestMultipleFailsThenZeroHandlesAreReturned) {
    pKmdSysManager->mockRequestMultiple = true;
    pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDeviceEngineFixture, GivenComponentCountZeroWhenEnumeratingEngineGroupsIfEngineSupportIsAbsentThenZeroHandlesAreReturned) {
    pKmdSysManager->mockNumSupportedEngineGroups = 0;
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDeviceEngineFixture, GivenUnsupportedEngineHandleWhenGettingEngineActivityThenFailureIsReturned) {
    std::unique_ptr<L0::WddmEngineImp> pEngineImp = std::make_unique<L0::WddmEngineImp>(pOsSysman, ZES_ENGINE_GROUP_FORCE_UINT32, 0, 0);
    zes_engine_stats_t pStats = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pEngineImp->getActivity(&pStats));
}

TEST_F(SysmanDeviceEngineFixture, GivenValidHandleGetPropertiesThenCorrectEngineGroupIsReturned) {
    auto handles = getEngineHandles(Mock<EngineKmdSysManager>::mockEngineHandleComponentCount);
    for (auto handle : handles) {
        zes_engine_properties_t properties = {};

        ze_result_t result = zesEngineGetProperties(handle, &properties);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
    }
}

TEST_F(SysmanDeviceEngineFixture, GivenValidHandleGetAvtivityThenCorrectValuesAreReturned) {
    auto handles = getEngineHandles(Mock<EngineKmdSysManager>::mockEngineHandleComponentCount);
    uint32_t engineGroupIndex = 0;
    for (auto handle : handles) {
        zes_engine_stats_t stats;

        ze_result_t result = zesEngineGetActivity(handle, &stats);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(stats.activeTime, pKmdSysManager->mockActivityCounters[engineGroupIndex]);
        EXPECT_EQ(stats.timestamp, pKmdSysManager->mockActivityTimeStamps[engineGroupIndex]);
        engineGroupIndex++;
    }
}

TEST_F(SysmanDeviceEngineFixture, GivenValidHandleWhenGettingEngineActivityAndRequestSingleFailsThenFailureIsReturned) {
    auto handles = getEngineHandles(Mock<EngineKmdSysManager>::mockEngineHandleComponentCount);
    for (auto handle : handles) {
        pKmdSysManager->mockRequestSingle = true;
        pKmdSysManager->mockRequestSingleResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        zes_engine_stats_t stats;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesEngineGetActivity(handle, &stats));
    }
}

TEST_F(SysmanDeviceEngineFixture, GivenValidHandleWhenCallingZesEngineGetActivityExtThenUnsupportedFeatureErrorIsReturned) {
    auto handles = getEngineHandles(engineHandleComponentCount);
    for (auto handle : handles) {
        zes_engine_stats_t stats;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesEngineGetActivityExt(handle, nullptr, &stats));
    }
}

TEST_F(SysmanDeviceEngineFixture, GivenZeroResponseSizeFromKmdWhenEnumeratingEngineGroupsThenZeroHandlesAreReturned) {
    pKmdSysManager->useZeroResponseSizeForEngineInstancesPerGroup = true;
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

} // namespace ult
} // namespace L0
