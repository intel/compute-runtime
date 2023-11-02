/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/tools/source/debug/linux/debug_session.h"
#include "level_zero/tools/test/unit_tests/sources/debug/linux/debug_session_fixtures_linux.h"

namespace L0 {
namespace ult {

using DebugSessionLinuxCreateTest = Test<DebugApiLinuxFixture>;

TEST_F(DebugSessionLinuxCreateTest, GivenDebuggerDriverTypeXeThenErrorReturned) {

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    mockDrm->setFileDescriptor(SysCalls::fakeFileDescriptor);
    VariableBackup<const char *> backup(&SysCalls::drmVersion);
    SysCalls::drmVersion = "xe";
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto session = std::unique_ptr<DebugSession>(DebugSession::create(config, device, result, false));

    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

} // namespace ult
} // namespace L0
