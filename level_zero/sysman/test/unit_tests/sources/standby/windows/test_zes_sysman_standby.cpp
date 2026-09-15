/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/standby/sysman_standby_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanDeviceStandbyFixture = SysmanDeviceFixture;

TEST_F(SysmanDeviceStandbyFixture, GivenStandbyHandleWhenCallingReInitOnStandbyHandleContextThenHandlesRemainValid) {
    auto pStandby = std::make_unique<StandbyImp>(pOsSysman, false, 0);
    pSysmanDeviceImp->pStandbyHandleContext->handleList.push_back(std::move(pStandby));

    pSysmanDeviceImp->pStandbyHandleContext->reInit();

    EXPECT_EQ(1u, static_cast<uint32_t>(pSysmanDeviceImp->pStandbyHandleContext->handleList.size()));
    for (auto &handle : pSysmanDeviceImp->pStandbyHandleContext->handleList) {
        EXPECT_NE(nullptr, handle);
    }
}

TEST_F(SysmanDeviceStandbyFixture, GivenStandbyContextNotInitializedWhenQueryingInitDoneThenFalseIsReturned) {
    EXPECT_FALSE(pSysmanDeviceImp->pStandbyHandleContext->isStandbyInitDone());
}

TEST_F(SysmanDeviceStandbyFixture, GivenStandbyContextInitializedViaEnumWhenQueryingInitDoneThenTrueIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumStandbyDomains(pSysmanDevice->toHandle(), &count, nullptr));

    EXPECT_TRUE(pSysmanDeviceImp->pStandbyHandleContext->isStandbyInitDone());
}

} // namespace ult
} // namespace Sysman
} // namespace L0
