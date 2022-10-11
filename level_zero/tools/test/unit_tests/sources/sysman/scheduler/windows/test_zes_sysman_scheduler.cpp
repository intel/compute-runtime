/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/scheduler/windows/mock_zes_sysman_scheduler.h"

extern bool sysmanUltsEnable;

using ::testing::_;
namespace L0 {
namespace ult {

class ZesSchedulerFixture : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<OsSchedulerMock> pOsSchedulerMock;
    FirmwareUtil *pFwUtilInterfaceOld = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(ZesSchedulerFixture, GivenComponentCountZeroWhenQueryingSchedulerHandlesThenNoHandlesAreReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumSchedulers(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(ZesSchedulerFixture, GivenSetExclusiveModeNotFailWhenTryingToSetComputeUnitDebugModeThenErrorIsReturned) {
    std::vector<std::string> listOfEngines = {"rcs0"};
    auto pSchedulerImp = std::make_unique<SchedulerImp>(pOsSysman, ZES_ENGINE_TYPE_FLAG_RENDER, listOfEngines, device->toHandle());
    auto pOsSchedulerBackup = pSchedulerImp->pOsScheduler;
    auto pOsSchedulerMock = new OsSchedulerMock();
    pSchedulerImp->pOsScheduler = pOsSchedulerMock;
    ze_bool_t pNeedReload = false;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSchedulerImp->setComputeUnitDebugMode(&pNeedReload));
    pSchedulerImp->pOsScheduler = pOsSchedulerBackup;
    delete pOsSchedulerMock;
}

TEST_F(ZesSchedulerFixture, GivenSetExclusiveModeNotSuccessWhenTryingToSetComputeUnitDebugModeThenErrorIsReturned) {
    std::vector<std::string> listOfEngines = {"rcs0"};
    auto pSchedulerImp = std::make_unique<SchedulerImp>(pOsSysman, ZES_ENGINE_TYPE_FLAG_RENDER, listOfEngines, device->toHandle());
    auto pOsSchedulerBackup = pSchedulerImp->pOsScheduler;
    auto pOsSchedulerMock = new OsSchedulerMock();
    pOsSchedulerMock->setPreemptTimeoutResult = ZE_RESULT_SUCCESS;
    pOsSchedulerMock->setTimesliceDurationResult = ZE_RESULT_SUCCESS;
    pOsSchedulerMock->setHeartbeatIntervalResult = ZE_RESULT_SUCCESS;
    pSchedulerImp->pOsScheduler = pOsSchedulerMock;
    ze_bool_t pNeedReload = false;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSchedulerImp->setComputeUnitDebugMode(&pNeedReload));
    pSchedulerImp->pOsScheduler = pOsSchedulerBackup;
    delete pOsSchedulerMock;
}

TEST_F(ZesSchedulerFixture, whenCallingOsSpecificSetComputeUnitDebugModeThenErrorIsReturned) {
    std::vector<std::string> listOfEngines = {"rcs0"};
    auto pSchedulerImp = std::make_unique<SchedulerImp>(pOsSysman, ZES_ENGINE_TYPE_FLAG_RENDER, listOfEngines, device->toHandle());
    ze_bool_t pNeedReload = false;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSchedulerImp->pOsScheduler->setComputeUnitDebugMode(&pNeedReload));
}

} // namespace ult
} // namespace L0