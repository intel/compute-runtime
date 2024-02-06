/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/tools/source/debug/linux/debug_session.h"
#include "level_zero/tools/test/unit_tests/sources/debug/linux/debug_session_fixtures_linux.h"

namespace NEO {
namespace SysCalls {
extern uint32_t closeFuncCalled;
extern int closeFuncArgPassed;
extern int closeFuncRetVal;
extern int setErrno;
extern uint32_t preadFuncCalled;
extern uint32_t pwriteFuncCalled;
extern uint32_t mmapFuncCalled;
extern uint32_t munmapFuncCalled;
} // namespace SysCalls
} // namespace NEO

namespace L0 {
namespace ult {

using DebugSessionLinuxCreateTest = Test<DebugApiLinuxFixture>;

TEST(IoctlHandler, GivenHandlerWhenPreadCalledThenSysCallIsCalled) {
    L0::DebugSessionLinux::IoctlHandler handler;
    NEO::SysCalls::preadFuncCalled = 0;
    auto retVal = handler.pread(0, nullptr, 0, 0);

    EXPECT_EQ(0, retVal);
    EXPECT_EQ(1u, NEO::SysCalls::preadFuncCalled);
    NEO::SysCalls::preadFuncCalled = 0;
}

TEST(IoctlHandler, GivenHandlerWhenPwriteCalledThenSysCallIsCalled) {
    L0::DebugSessionLinux::IoctlHandler handler;
    NEO::SysCalls::pwriteFuncCalled = 0;
    auto retVal = handler.pwrite(0, nullptr, 0, 0);

    EXPECT_EQ(0, retVal);
    EXPECT_EQ(1u, NEO::SysCalls::pwriteFuncCalled);
    NEO::SysCalls::pwriteFuncCalled = 0;
}

TEST(IoctlHandler, GivenHandlerWhenMmapAndMunmapCalledThenRedirectedToSysCall) {
    L0::DebugSessionLinux::IoctlHandler handler;
    NEO::SysCalls::mmapFuncCalled = 0;
    NEO::SysCalls::munmapFuncCalled = 0;
    auto mappedPtr = handler.mmap(0, 0, 0, 0, 0, 0);

    EXPECT_EQ(0, mappedPtr);

    auto retVal = handler.munmap(0, 0);
    EXPECT_EQ(0, retVal);

    EXPECT_EQ(1u, NEO::SysCalls::mmapFuncCalled);
    EXPECT_EQ(1u, NEO::SysCalls::munmapFuncCalled);
    NEO::SysCalls::mmapFuncCalled = 0;
    NEO::SysCalls::munmapFuncCalled = 0;
}

TEST(IoctlHandler, GivenHandlerWhenFsynCalledThenRedirectedToSysCall) {
    L0::DebugSessionLinux::IoctlHandler handler;
    NEO::SysCalls::fsyncCalled = 0;
    auto retVal = handler.fsync(0);
    EXPECT_EQ(0, retVal);
    EXPECT_EQ(1, NEO::SysCalls::fsyncCalled);
    NEO::SysCalls::fsyncCalled = 0;
}

} // namespace ult
} // namespace L0
