/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include <fcntl.h>
#include <sys/resource.h>

namespace L0 {
namespace ult {

struct WhiteBoxContext : public Context {
    using Context::Context;
    using Context::initOpaqueHandleResourcesImpl;
};

class InitOpaqueHandleResourcesFixture : public DeviceFixture {
  public:
    void setUp() {
        DeviceFixture::setUp();

        // Backup mock state (restored on tearDown)
        mockRlimitCurBackup = std::make_unique<VariableBackup<rlim_t>>(&mockRlimitCur);
        mockRlimitMaxBackup = std::make_unique<VariableBackup<rlim_t>>(&mockRlimitMax);
        mockGetrlimitRetValBackup = std::make_unique<VariableBackup<int>>(&mockGetrlimitRetVal);
        mockOpenShouldFailBackup = std::make_unique<VariableBackup<bool>>(&mockOpenShouldFail);
        mockOpenFailAfterBackup = std::make_unique<VariableBackup<uint32_t>>(&mockOpenFailAfter);

        // Backup and install mocks
        getrlimitBackup = std::make_unique<VariableBackup<decltype(NEO::SysCalls::sysCallsGetrlimit)>>(&NEO::SysCalls::sysCallsGetrlimit, mockGetrlimitSuccess);
        openBackup = std::make_unique<VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)>>(&NEO::SysCalls::sysCallsOpen, mockOpenSuccess);
        closeBackup = std::make_unique<VariableBackup<decltype(NEO::SysCalls::sysCallsClose)>>(&NEO::SysCalls::sysCallsClose, mockCloseSuccess);

        whiteBoxCtx = std::make_unique<WhiteBoxContext>(driverHandle.get());

        // Reset counters after construction to start each test fresh
        NEO::SysCalls::getrlimitCalled = 0;
        NEO::SysCalls::openFuncCalled = 0;
        NEO::SysCalls::closeFuncCalled = 0;
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }

    // Mock functions
    static int mockGetrlimitSuccess(int resource, struct rlimit *rlim) {
        if (resource == RLIMIT_NOFILE) {
            rlim->rlim_cur = mockRlimitCur;
            rlim->rlim_max = mockRlimitMax;
            return mockGetrlimitRetVal;
        }
        return 0;
    }

    static int mockOpenSuccess(const char *pathname, int flags) {
        if (mockOpenShouldFail && NEO::SysCalls::openFuncCalled > mockOpenFailAfter) {
            return -1;
        }
        return 100 + static_cast<int>(NEO::SysCalls::openFuncCalled);
    }

    static int mockCloseSuccess(int fd) {
        return 0;
    }

    std::unique_ptr<WhiteBoxContext> whiteBoxCtx;

    std::unique_ptr<VariableBackup<decltype(NEO::SysCalls::sysCallsGetrlimit)>> getrlimitBackup;
    std::unique_ptr<VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)>> openBackup;
    std::unique_ptr<VariableBackup<decltype(NEO::SysCalls::sysCallsClose)>> closeBackup;

    std::unique_ptr<VariableBackup<rlim_t>> mockRlimitCurBackup;
    std::unique_ptr<VariableBackup<rlim_t>> mockRlimitMaxBackup;
    std::unique_ptr<VariableBackup<int>> mockGetrlimitRetValBackup;
    std::unique_ptr<VariableBackup<bool>> mockOpenShouldFailBackup;
    std::unique_ptr<VariableBackup<uint32_t>> mockOpenFailAfterBackup;

    static rlim_t mockRlimitCur;
    static rlim_t mockRlimitMax;
    static int mockGetrlimitRetVal;
    static bool mockOpenShouldFail;
    static uint32_t mockOpenFailAfter;
};

// Static member initialization
rlim_t InitOpaqueHandleResourcesFixture::mockRlimitCur = 10240;
rlim_t InitOpaqueHandleResourcesFixture::mockRlimitMax = 10240;
int InitOpaqueHandleResourcesFixture::mockGetrlimitRetVal = 0;
bool InitOpaqueHandleResourcesFixture::mockOpenShouldFail = false;
uint32_t InitOpaqueHandleResourcesFixture::mockOpenFailAfter = 0;

using InitOpaqueHandleResourcesTest = Test<InitOpaqueHandleResourcesFixture>;

TEST_F(InitOpaqueHandleResourcesTest, GivenValidSystemWhenCallingInitOpaqueHandleResourcesImplThenFdsArePreallocated) {
    mockRlimitCur = 10240;
    mockOpenShouldFail = false;

    whiteBoxCtx->initOpaqueHandleResourcesImpl();

    EXPECT_GT(static_cast<int>(NEO::SysCalls::getrlimitCalled), 0);
    EXPECT_GT(static_cast<int>(NEO::SysCalls::openFuncCalled), 0);
    EXPECT_EQ(static_cast<int>(NEO::SysCalls::openFuncCalled), static_cast<int>(NEO::SysCalls::closeFuncCalled));
}

TEST_F(InitOpaqueHandleResourcesTest, GivenGetrlimitFailsWhenCallingInitOpaqueHandleResourcesImplThenEarlyReturn) {
    mockGetrlimitRetVal = -1;

    whiteBoxCtx->initOpaqueHandleResourcesImpl();

    EXPECT_GT(static_cast<int>(NEO::SysCalls::getrlimitCalled), 0);
    EXPECT_EQ(0, static_cast<int>(NEO::SysCalls::openFuncCalled));
}

TEST_F(InitOpaqueHandleResourcesTest, GivenOverrideCountWhenCallingInitOpaqueHandleResourcesImplThenOverrideIsUsed) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.IpcFdPreallocationCount.set(100);

    mockRlimitCur = 10240;
    mockOpenShouldFail = false;

    whiteBoxCtx->initOpaqueHandleResourcesImpl();

    EXPECT_EQ(100, static_cast<int>(NEO::SysCalls::openFuncCalled));
    EXPECT_EQ(100, static_cast<int>(NEO::SysCalls::closeFuncCalled));
}

TEST_F(InitOpaqueHandleResourcesTest, GivenInvalidOverrideCountWhenCallingInitOpaqueHandleResourcesImplThenDefaultIsUsed) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.IpcFdPreallocationCount.set(0);

    mockRlimitCur = 10240;
    mockOpenShouldFail = false;

    whiteBoxCtx->initOpaqueHandleResourcesImpl();

    rlim_t expectedFds = std::min(mockRlimitCur / 10, static_cast<rlim_t>(4096));
    EXPECT_EQ(static_cast<int>(expectedFds), static_cast<int>(NEO::SysCalls::openFuncCalled));
}

TEST_F(InitOpaqueHandleResourcesTest, GivenOverrideCountExceedsUlimitWhenCallingInitOpaqueHandleResourcesImplThenDefaultIsUsed) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.IpcFdPreallocationCount.set(20000);

    mockRlimitCur = 10240;
    mockOpenShouldFail = false;

    whiteBoxCtx->initOpaqueHandleResourcesImpl();

    rlim_t expectedFds = std::min(mockRlimitCur / 10, static_cast<rlim_t>(4096));
    EXPECT_EQ(static_cast<int>(expectedFds), static_cast<int>(NEO::SysCalls::openFuncCalled));
}

TEST_F(InitOpaqueHandleResourcesTest, GivenOpenFailsPartiallyWhenCallingInitOpaqueHandleResourcesImplThenPartialPreallocation) {
    mockRlimitCur = 10240;
    mockOpenShouldFail = true;
    mockOpenFailAfter = 50;

    whiteBoxCtx->initOpaqueHandleResourcesImpl();

    EXPECT_EQ(51, static_cast<int>(NEO::SysCalls::openFuncCalled));
    EXPECT_EQ(50, static_cast<int>(NEO::SysCalls::closeFuncCalled));
}

TEST_F(InitOpaqueHandleResourcesTest, GivenLowUlimitWhenCallingInitOpaqueHandleResourcesImplThenUsesUlimitDividedBy10) {
    mockRlimitCur = 1000;
    mockOpenShouldFail = false;

    whiteBoxCtx->initOpaqueHandleResourcesImpl();

    EXPECT_EQ(100, static_cast<int>(NEO::SysCalls::openFuncCalled));
    EXPECT_EQ(100, static_cast<int>(NEO::SysCalls::closeFuncCalled));
}

TEST_F(InitOpaqueHandleResourcesTest, GivenHighUlimitWhenCallingInitOpaqueHandleResourcesImplThenUsesMaxPreallocation) {
    mockRlimitCur = 100000;
    mockOpenShouldFail = false;

    whiteBoxCtx->initOpaqueHandleResourcesImpl();

    EXPECT_EQ(4096, static_cast<int>(NEO::SysCalls::openFuncCalled));
    EXPECT_EQ(4096, static_cast<int>(NEO::SysCalls::closeFuncCalled));
}

TEST_F(InitOpaqueHandleResourcesTest, GivenOpenFailsImmediatelyWhenCallingInitOpaqueHandleResourcesImplThenNoFdsAreClosed) {
    mockRlimitCur = 10240;
    mockOpenShouldFail = true;
    mockOpenFailAfter = 0;

    whiteBoxCtx->initOpaqueHandleResourcesImpl();

    EXPECT_EQ(1, static_cast<int>(NEO::SysCalls::openFuncCalled));
    EXPECT_EQ(0, static_cast<int>(NEO::SysCalls::closeFuncCalled));
}

TEST_F(InitOpaqueHandleResourcesTest, GivenValidOverrideAt1WhenCallingInitOpaqueHandleResourcesImplThenExactly1FdIsPreallocated) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.IpcFdPreallocationCount.set(1);

    mockRlimitCur = 10240;
    mockOpenShouldFail = false;

    whiteBoxCtx->initOpaqueHandleResourcesImpl();

    EXPECT_EQ(1, static_cast<int>(NEO::SysCalls::openFuncCalled));
    EXPECT_EQ(1, static_cast<int>(NEO::SysCalls::closeFuncCalled));
}

TEST_F(InitOpaqueHandleResourcesTest, GivenValidOverrideAtUlimitWhenCallingInitOpaqueHandleResourcesImplThenExactlyUlimitFdsArePreallocated) {
    DebugManagerStateRestore restorer;

    mockRlimitCur = 1000;
    NEO::debugManager.flags.IpcFdPreallocationCount.set(static_cast<int32_t>(mockRlimitCur));

    mockOpenShouldFail = false;

    whiteBoxCtx->initOpaqueHandleResourcesImpl();

    EXPECT_EQ(static_cast<int>(mockRlimitCur), static_cast<int>(NEO::SysCalls::openFuncCalled));
    EXPECT_EQ(static_cast<int>(mockRlimitCur), static_cast<int>(NEO::SysCalls::closeFuncCalled));
}

} // namespace ult
} // namespace L0
