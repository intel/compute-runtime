/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/kmd_notify_properties.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/windows/sys_calls.h"
#include "test.h"
#include "unit_tests/helpers/variable_backup.h"

namespace OCLRT {

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

} // namespace OCLRT
