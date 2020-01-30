/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/kmd_notify_properties.h"
#include "core/os_interface/windows/sys_calls.h"
#include "core/unit_tests/helpers/default_hw_info.h"
#include "test.h"
#include "unit_tests/helpers/variable_backup.h"

namespace NEO {

namespace SysCalls {
extern BOOL systemPowerStatusRetVal;
extern BYTE systemPowerStatusACLineStatusOverride;
} // namespace SysCalls

class MockKmdNotifyHelper : public KmdNotifyHelper {
  public:
    using KmdNotifyHelper::acLineConnected;
    using KmdNotifyHelper::getBaseTimeout;
    using KmdNotifyHelper::updateAcLineStatus;

    MockKmdNotifyHelper(const KmdNotifyProperties *newProperties) : KmdNotifyHelper(newProperties){};
};

TEST(KmdNotifyWindowsTests, whenGetSystemPowerStatusReturnSuccessThenUpdateAcLineStatus) {
    auto properties = &(platformDevices[0]->capabilityTable.kmdNotifyProperties);
    MockKmdNotifyHelper helper(properties);
    EXPECT_TRUE(helper.acLineConnected);

    VariableBackup<BOOL> systemPowerStatusRetValBkp(&SysCalls::systemPowerStatusRetVal);
    VariableBackup<BYTE> systemPowerStatusACLineStatusOverrideBkp(&SysCalls::systemPowerStatusACLineStatusOverride);
    systemPowerStatusRetValBkp = 1;
    systemPowerStatusACLineStatusOverrideBkp = 0;

    helper.updateAcLineStatus();
    EXPECT_FALSE(helper.acLineConnected);

    systemPowerStatusACLineStatusOverrideBkp = 1;

    helper.updateAcLineStatus();
    EXPECT_TRUE(helper.acLineConnected);
}

TEST(KmdNotifyWindowsTests, whenGetSystemPowerStatusReturnErrorThenDontUpdateAcLineStatus) {
    auto properties = &(platformDevices[0]->capabilityTable.kmdNotifyProperties);
    MockKmdNotifyHelper helper(properties);
    EXPECT_TRUE(helper.acLineConnected);

    VariableBackup<BOOL> systemPowerStatusRetValBkp(&SysCalls::systemPowerStatusRetVal);
    VariableBackup<BYTE> systemPowerStatusACLineStatusOverrideBkp(&SysCalls::systemPowerStatusACLineStatusOverride);
    systemPowerStatusRetValBkp = 0;
    systemPowerStatusACLineStatusOverrideBkp = 0;

    helper.updateAcLineStatus();
    EXPECT_TRUE(helper.acLineConnected);
}

TEST(KmdNotifyWindowsTests, givenTaskCountDiffGreaterThanOneWhenBaseTimeoutRequestedThenDontMultiply) {
    auto localProperties = (platformDevices[0]->capabilityTable.kmdNotifyProperties);
    localProperties.delayKmdNotifyMicroseconds = 10;
    const int64_t multiplier = 10;

    MockKmdNotifyHelper helper(&localProperties);
    EXPECT_EQ(localProperties.delayKmdNotifyMicroseconds, helper.getBaseTimeout(multiplier));
}

} // namespace NEO
