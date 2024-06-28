/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_fixture_xe.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

TEST_F(SysmanFixtureDeviceXe, GivenGroupEngineTypeAndSysmanKmdInterfaceInstanceWhenGetEngineActivityFdIsCalledThenInValidFdIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();

    EXPECT_LT(pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_ALL, 0, 0, pPmuInterface.get()), 0);
    EXPECT_LT(pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_COMPUTE_ALL, 0, 0, pPmuInterface.get()), 0);
    EXPECT_LT(pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_COPY_ALL, 0, 0, pPmuInterface.get()), 0);
    EXPECT_LT(pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_RENDER_ALL, 0, 0, pPmuInterface.get()), 0);
    EXPECT_LT(pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_MEDIA_ALL, 0, 0, pPmuInterface.get()), 0);
}

TEST_F(SysmanFixtureDeviceXe, GivenSingleEngineTypeAndSysmanKmdInterfaceInstanceWhenGetEngineActivityFdIsCalledThenInvalidFdIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_LT(pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0, pPmuInterface.get()), 0);
}

} // namespace ult
} // namespace Sysman
} // namespace L0