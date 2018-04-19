/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/kmd_notify_properties.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/windows/sys_calls.h"

#include "unit_tests/helpers/variable_backup.h"
#include "test.h"

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
