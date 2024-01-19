/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_execution_environment.h"

#include "level_zero/sysman/test/unit_tests/sources/global_operations/linux/mock_global_operations.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {

namespace Sysman {

namespace ult {

using SysmanGlobalOperationsXeFixture = SysmanDeviceFixture;

TEST_F(SysmanGlobalOperationsXeFixture, GivenValidDeviceHandleWhenCallingDeviceGetStateThenVerifyDeviceIsNotWedged) {
    pLinuxSysmanImp->pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceXe>(pLinuxSysmanImp->getProductFamily());
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto pDrm = new DrmGlobalOpsMock(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
    auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    osInterface->setDriverModel(std::unique_ptr<DrmGlobalOpsMock>(pDrm));

    zes_device_state_t deviceState;
    ze_result_t result = zesDeviceGetState(pSysmanDevice, &deviceState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, deviceState.reset & ZES_RESET_REASON_FLAG_WEDGED);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
