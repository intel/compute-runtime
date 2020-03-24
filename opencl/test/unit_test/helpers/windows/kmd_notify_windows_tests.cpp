/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/kmd_notify_properties.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"

#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "test.h"

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
    auto properties = &(defaultHwInfo->capabilityTable.kmdNotifyProperties);
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
    auto properties = &(defaultHwInfo->capabilityTable.kmdNotifyProperties);
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
    auto localProperties = (defaultHwInfo->capabilityTable.kmdNotifyProperties);
    localProperties.delayKmdNotifyMicroseconds = 10;
    const int64_t multiplier = 10;

    MockKmdNotifyHelper helper(&localProperties);
    EXPECT_EQ(localProperties.delayKmdNotifyMicroseconds, helper.getBaseTimeout(multiplier));
}

} // namespace NEO
