/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/firmware_util/mock_fw_util_fixture.h"

#include <dlfcn.h>

namespace NEO {
namespace SysCalls {
extern int dlOpenFlags;
extern bool dlOpenCalled;
} // namespace SysCalls
} // namespace NEO

namespace L0 {
namespace Sysman {
namespace ult {

TEST(FwUtilTest, GivenLibraryWasSetWhenCreatingFirmwareUtilInterfaceThenLibraryIsLoadedWithDeepbindDisabled) {
    VariableBackup<bool> dlOpenCalledBackup{&NEO::SysCalls::dlOpenCalled, false};
    VariableBackup<int> dlOpenFlagsBackup{&NEO::SysCalls::dlOpenFlags, 0};

    L0::Sysman::FirmwareUtil *pFwUtil = L0::Sysman::FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(NEO::SysCalls::dlOpenCalled, true);
    EXPECT_EQ(NEO::SysCalls::dlOpenFlags, RTLD_LAZY);
    EXPECT_EQ(pFwUtil, nullptr);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
