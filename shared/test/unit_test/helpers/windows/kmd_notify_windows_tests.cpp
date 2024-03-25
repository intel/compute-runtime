/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/kmd_notify_properties.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

namespace SysCalls {
extern BOOL systemPowerStatusRetVal;
extern BYTE systemPowerStatusACLineStatusOverride;
} // namespace SysCalls

class PublicKmdNotifyHelper : public KmdNotifyHelper {
  public:
    using KmdNotifyHelper::acLineConnected;
    using KmdNotifyHelper::updateAcLineStatus;

    PublicKmdNotifyHelper(const KmdNotifyProperties *newProperties) : KmdNotifyHelper(newProperties){};
};

TEST(KmdNotifyWindowsTests, whenGetSystemPowerStatusReturnSuccessThenUpdateAcLineStatus) {
    auto properties = &(defaultHwInfo->capabilityTable.kmdNotifyProperties);
    PublicKmdNotifyHelper helper(properties);
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
    PublicKmdNotifyHelper helper(properties);
    EXPECT_TRUE(helper.acLineConnected);

    VariableBackup<BOOL> systemPowerStatusRetValBkp(&SysCalls::systemPowerStatusRetVal);
    VariableBackup<BYTE> systemPowerStatusACLineStatusOverrideBkp(&SysCalls::systemPowerStatusACLineStatusOverride);
    systemPowerStatusRetValBkp = 0;
    systemPowerStatusACLineStatusOverrideBkp = 0;

    helper.updateAcLineStatus();
    EXPECT_TRUE(helper.acLineConnected);
}

} // namespace NEO
