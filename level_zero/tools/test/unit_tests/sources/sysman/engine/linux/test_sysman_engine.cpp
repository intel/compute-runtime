/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/engine/linux/os_engine_imp.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_sysfs_engine.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Return;

namespace L0 {
namespace ult {

ACTION_P(SetUint64_t, value) {
    arg0 = value;
}
ACTION_P(SetEngGroup_t, value) {
    arg0 = value;
}

ACTION_P(SetString_t, value) {
    arg1 = value;
}

class SysmanEngineFixture : public DeviceFixture, public ::testing::Test {

  protected:
    std::unique_ptr<SysmanImp> sysmanImp;
    zet_sysman_handle_t hSysman;
    zet_sysman_engine_handle_t hSysmanEngine;
    Mock<EngineSysfsAccess> *pSysfsAccess = nullptr;

    OsEngine *pOsEngine = nullptr;
    PublicLinuxEngineImp linuxEngineImp;
    EngineImp *pEngineImp = nullptr;

    void SetUp() override {

        DeviceFixture::SetUp();
        sysmanImp = std::make_unique<SysmanImp>(device->toHandle());
        pSysfsAccess = new NiceMock<Mock<EngineSysfsAccess>>;
        linuxEngineImp.pSysfsAccess = pSysfsAccess;
        pOsEngine = static_cast<OsEngine *>(&linuxEngineImp);
        pEngineImp = new EngineImp();
        pEngineImp->pOsEngine = pOsEngine;

        ON_CALL(*pSysfsAccess, read(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<EngineSysfsAccess>::getVal));

        pEngineImp->init();
        sysmanImp->pEngineHandleContext->handleList.push_back(pEngineImp);

        hSysman = sysmanImp->toHandle();
        hSysmanEngine = pEngineImp->toHandle();
    }
    void TearDown() override {
        //pOsEngine is static_cast of LinuxEngineImp class , hence in cleanup assign to nullptr
        pEngineImp->pOsEngine = nullptr;

        if (pSysfsAccess != nullptr) {
            delete pSysfsAccess;
            pSysfsAccess = nullptr;
        }
        DeviceFixture::TearDown();
    }
};

TEST_F(SysmanEngineFixture, GivenComponentCountZeroWhenCallingZetSysmanEngineGetThenNonZeroCountIsReturnedAndVerifySysmanEngineGetCallSucceeds) {
    zet_sysman_engine_handle_t engineHandle;
    uint32_t count = 0;
    ze_result_t result = zetSysmanEngineGet(hSysman, &count, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_GT(count, 0u);

    uint32_t testcount = count + 1;

    result = zetSysmanEngineGet(hSysman, &testcount, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, count);

    result = zetSysmanEngineGet(hSysman, &count, &engineHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, engineHandle);
    EXPECT_GT(count, 0u);
}
TEST_F(SysmanEngineFixture, GivenValidEngineHandleWhenCallingZetSysmanEngineGetPropertiesThenVerifySysmanEngineGetPropertiesCallSucceeds) {
    zet_engine_properties_t properties;

    ze_result_t result = zetSysmanEngineGetProperties(hSysmanEngine, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_ENGINE_GROUP_COMPUTE_ALL, properties.type);
    EXPECT_FALSE(properties.onSubdevice);
}
TEST_F(SysmanEngineFixture, GivenValidEngineHandleWhenCallingZetSysmanGetActivityThenVerifySysmanEngineGetActivityCallReturnsUnsupportedErrorStatus) {
    zet_engine_stats_t Stats;

    ze_result_t result = zetSysmanEngineGetActivity(hSysmanEngine, &Stats);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

} // namespace ult
} // namespace L0
