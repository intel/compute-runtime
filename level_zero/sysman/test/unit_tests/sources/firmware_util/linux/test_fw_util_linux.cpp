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

extern void *(*dlopenFunc)(const char *filename, int flags);

namespace L0 {
namespace Sysman {
namespace ult {

static int dlOpenFlags = 0;
static bool dlOpenCalled = false;

void *mockDlopen(const char *filename, int flags) {
    dlOpenCalled = true;
    dlOpenFlags = flags;
    return nullptr;
}

TEST(FwUtilTest, GivenLibraryWasSetWhenCreatingFirmwareUtilInterfaceThenLibraryIsLoadedWithDeepbindDisabled) {
    VariableBackup<decltype(dlopenFunc)> dlopenFuncBackup(&dlopenFunc, nullptr);
    dlopenFunc = mockDlopen;
    VariableBackup<bool> dlOpenCalledBackup{&dlOpenCalled, false};
    VariableBackup<int> dlOpenFlagsBackup{&dlOpenFlags, 0};

    auto flags = RTLD_LAZY;
    NEO::OsLibrary::loadFlagsOverwrite = &flags;
    L0::Sysman::FirmwareUtil *pFwUtil = L0::Sysman::FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(dlOpenCalled, true);
    EXPECT_EQ(dlOpenFlags, RTLD_LAZY);
    EXPECT_EQ(pFwUtil, nullptr);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
