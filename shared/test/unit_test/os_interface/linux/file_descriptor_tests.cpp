/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/file_descriptor.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(FileDescriptorTest, whenOpeningFileReturnsNonNegativeFileDescriptorThenCloseIsCalled) {
    int mockHandle = 12345;
    VariableBackup<decltype(SysCalls::openFuncRetVal)> openRetValBackup(&SysCalls::openFuncRetVal, mockHandle);
    VariableBackup<decltype(SysCalls::openFuncCalled)> openCalledBackup(&SysCalls::openFuncCalled, 0u);
    VariableBackup<decltype(SysCalls::closeFuncCalled)> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);

    {
        auto fileDescriptor = FileDescriptor("", 0);
        EXPECT_EQ(SysCalls::openFuncRetVal, fileDescriptor);
        EXPECT_EQ(1u, SysCalls::openFuncCalled);
        SysCalls::closeFuncArgPassed = 0;
    }
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(SysCalls::openFuncRetVal, SysCalls::closeFuncArgPassed);

    SysCalls::openFuncRetVal = -1;

    {
        auto fileDescriptor = FileDescriptor("", 0);
        EXPECT_EQ(SysCalls::openFuncRetVal, fileDescriptor);
        EXPECT_EQ(2u, SysCalls::openFuncCalled);
        SysCalls::closeFuncArgPassed = 0;
    }

    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
}
