/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/tools/source/debug/linux/debug_session.h"
#include "level_zero/tools/test/unit_tests/sources/debug/linux/debug_session_fixtures_linux.h"
#include "level_zero/tools/test/unit_tests/sources/debug/linux/prelim/debug_session_fixtures_linux.h"

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

struct DebugApiSlmAccessProtocolTest : public ::testing::Test {
    struct MockDebugSessionSlm : public MockDebugSessionLinuxi915 {
        using MockDebugSessionLinuxi915::MockDebugSessionLinuxi915;
        template <typename T>
        struct ArgsRecorder {
            EuThread::ThreadId threadId;
            const zet_debug_memory_space_desc_t *desc;
            size_t size;
            T buffer;
        };

        DebugSessionImp::SlmAccessProtocol getSlmAccessProtocol() const override {
            return DebugSessionImp::SlmAccessProtocol::v2;
        }

        std::optional<ArgsRecorder<void *>> slmReadV2Args;
        ze_result_t slmMemoryReadV2(EuThread::ThreadId threadId, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) override {
            slmReadV2Args = ArgsRecorder<void *>{threadId, desc, size, buffer};
            return ZE_RESULT_SUCCESS;
        }

        std::optional<ArgsRecorder<const void *>> slmWriteV2Args;
        ze_result_t slmMemoryWriteV2(EuThread::ThreadId threadId, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) override {
            slmWriteV2Args = ArgsRecorder<const void *>{threadId, desc, size, buffer};
            return ZE_RESULT_SUCCESS;
        }
    };

    MockDeviceImp deviceImp;
    MockDebugSessionSlm session;
    EuThread::ThreadId threadId;
    zet_debug_memory_space_desc_t desc;
    DebugApiSlmAccessProtocolTest() : deviceImp(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(defaultHwInfo.get(), 0)),
                                      session(zet_debug_config_t{}, &deviceImp, 0),
                                      threadId(1, 2, 3, 4, 5) {}
};

TEST_F(DebugApiSlmAccessProtocolTest, whenSlmAccessProtocolIsV2ThenSlmMemoryReadV2IsCalled) {
    size_t size = 16;
    uint8_t buffer[16] = {};

    auto result = session.readSlm(threadId, &desc, size, buffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_TRUE(session.slmReadV2Args.has_value());
    EXPECT_EQ(threadId, session.slmReadV2Args->threadId);
    EXPECT_EQ(&desc, session.slmReadV2Args->desc);
    EXPECT_EQ(size, session.slmReadV2Args->size);
    EXPECT_EQ(buffer, session.slmReadV2Args->buffer);
}

TEST_F(DebugApiSlmAccessProtocolTest, whenSlmAccessProtocolIsV2ThenSlmMemoryWriteV2IsCalled) {
    size_t size = 16;
    uint8_t buffer[16] = {};

    auto result = session.writeSlm(threadId, &desc, size, buffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_TRUE(session.slmWriteV2Args.has_value());
    EXPECT_EQ(threadId, session.slmWriteV2Args->threadId);
    EXPECT_EQ(&desc, session.slmWriteV2Args->desc);
    EXPECT_EQ(size, session.slmWriteV2Args->size);
    EXPECT_EQ(buffer, session.slmWriteV2Args->buffer);
}

} // namespace ult
} // namespace L0
