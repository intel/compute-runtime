/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/engine/windows/os_engine_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/engine/windows/mock_engine.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr uint32_t engineHandleComponentCount = 3u;
class SysmanDeviceEngineFixture : public SysmanDeviceFixture {
  protected:
    Mock<EngineKmdSysManager> *pKmdSysManager = nullptr;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();

        pKmdSysManager = new Mock<EngineKmdSysManager>;

        EXPECT_CALL(*pKmdSysManager, escape(_, _, _, _, _))
            .WillRepeatedly(::testing::Invoke(pKmdSysManager, &Mock<EngineKmdSysManager>::mock_escape));

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager;

        for (auto handle : pSysmanDeviceImp->pEngineHandleContext->handleList) {
            delete handle;
        }

        pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
        pSysmanDeviceImp->pEngineHandleContext->init();
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::TearDown();
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        if (pKmdSysManager != nullptr) {
            delete pKmdSysManager;
            pKmdSysManager = nullptr;
        }
    }

    std::vector<zes_engine_handle_t> get_engine_handles(uint32_t count) {
        std::vector<zes_engine_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceEngineFixture, GivenComponentCountZeroWhenEnumeratingEngineGroupsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, engineHandleComponentCount);
}

TEST_F(SysmanDeviceEngineFixture, GivenInvalidComponentCountWhenEnumeratingEngineGroupsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, engineHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, engineHandleComponentCount);
}

TEST_F(SysmanDeviceEngineFixture, GivenComponentCountZeroWhenEnumeratingEngineGroupsThenValidPowerHandlesIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, engineHandleComponentCount);

    std::vector<zes_engine_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceEngineFixture, GivenValidHandleGetPropertiesThenCorrectEngineGroupIsReturned) {
    auto handles = get_engine_handles(engineHandleComponentCount);
    uint32_t engineGroupIndex = 0;
    for (auto handle : handles) {
        zes_engine_properties_t properties = {};

        ze_result_t result = zesEngineGetProperties(handle, &properties);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0);
        EXPECT_EQ(properties.type, pKmdSysManager->mockEngineTypes[engineGroupIndex++]);
    }
}

TEST_F(SysmanDeviceEngineFixture, GivenValidHandleGetAvtivityThenCorrectValuesAreReturned) {
    auto handles = get_engine_handles(engineHandleComponentCount);
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

} // namespace ult
} // namespace L0
