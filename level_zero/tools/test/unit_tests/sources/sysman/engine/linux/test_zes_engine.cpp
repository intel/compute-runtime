/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_engine.h"

using ::testing::Matcher;
using ::testing::Return;
class OsEngine;
namespace L0 {
namespace ult {
constexpr uint32_t handleComponentCount = 4u;
class ZesEngineFixture : public SysmanDeviceFixture {
  protected:
    Mock<EngineNeoDrm> *pDrm = nullptr;
    Drm *pOriginalDrm = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        EngineHandleContext *pEngineHandleContext = pSysmanDeviceImp->pEngineHandleContext;
        pDrm = new NiceMock<Mock<EngineNeoDrm>>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment()));
        pOriginalDrm = pLinuxSysmanImp->pDrm;
        pLinuxSysmanImp->pDrm = pDrm;
        ON_CALL(*pDrm, queryEngineInfo())
            .WillByDefault(::testing::Invoke(pDrm, &Mock<EngineNeoDrm>::queryEngineInfoMockPositiveTest));

        pEngineHandleContext->init();
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pDrm = pOriginalDrm;
        if (pDrm != nullptr) {
            delete pDrm;
            pDrm = nullptr;
        }
    }

    std::vector<zes_engine_handle_t> get_engine_handles(uint32_t count) {
        std::vector<zes_engine_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesEngineFixture, GivenComponentCountZeroWhenCallingzesDeviceEnumEngineGroupsThenNonZeroCountIsReturnedAndVerifyCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, handleComponentCount);

    uint32_t testcount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &testcount, NULL));
    EXPECT_EQ(testcount, count);

    count = 0;
    std::vector<zes_engine_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, handleComponentCount);
}

TEST_F(ZesEngineFixture, GivenValidEngineHandlesWhenCallingZesEngineGetPropertiesThenVerifyCallSucceeds) {
    zes_engine_properties_t properties;
    auto handle = get_engine_handles(handleComponentCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handle[0], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_RENDER_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handle[1], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handle[2], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handle[3], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_COPY_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleWhenCallingZesEngineGetActivityThenVerifyCallReturnsUnsupportedErrorStatus) {
    zes_engine_stats_t Stats;
    auto handle = get_engine_handles(handleComponentCount);

    for (uint32_t i = 0; i < handleComponentCount; i++) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesEngineGetActivity(handle[i], &Stats));
        EXPECT_EQ(0u, Stats.activeTime);
        EXPECT_EQ(0u, Stats.timestamp);
    }
}
} // namespace ult
} // namespace L0
