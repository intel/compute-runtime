/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/extensions/public/cl_gl_private_intel.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/sharings/gl/leo_gl_sync_event.h"
#include "level_zero/api/opencl/source/sharings/gl/windows/leo_gl_sharing_windows.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_command_list.h"
#include "level_zero/api/opencl/test/common/fixtures/command_list_create_immediate_hook.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

#include <memory>

namespace NEO {
namespace LEO {
namespace ult {

struct StubGlSyncSharingFunctionsWindows : public GLSharingFunctionsWindows {
    StubGlSyncSharingFunctionsWindows() {
        glHGLRCHandle = currentContext;
        glGetCurrentContext = &getCurrentContextStub;
        glGetCurrentDisplay = &getCurrentDisplayStub;
        glRetainSync = &retainSyncStub;
        glReleaseSync = &releaseSyncStub;
        glGetSynciv = &getSyncivStub;
        retainSyncCalled = 0u;
        releaseSyncCalled = 0u;
    }

    static GLContext OSAPI getCurrentContextStub() {
        return currentContext;
    }

    static GLDisplay OSAPI getCurrentDisplayStub() {
        return nullptr;
    }

    static GLboolean OSAPI retainSyncStub(GLDisplay, GLContext, GLContext, GLvoid *pSyncInfo) {
        retainSyncCalled++;
        static_cast<GL_CL_SYNC_INFO *>(pSyncInfo)->pSync = retainedSync;
        return GL_TRUE;
    }

    static GLboolean OSAPI releaseSyncStub(GLDisplay, GLContext, GLContext, GLvoid *pSync) {
        releaseSyncCalled++;
        EXPECT_EQ(retainedSync, pSync);
        return GL_TRUE;
    }

    static void OSAPI getSyncivStub(GLvoid *, GLenum, GLint *value) {
        *value = GL_SIGNALED;
    }

    inline static GLContext currentContext = reinterpret_cast<GLContext>(0x1234);
    inline static GLvoid *retainedSync = reinterpret_cast<GLvoid *>(0x5678);
    inline static uint32_t retainSyncCalled = 0u;
    inline static uint32_t releaseSyncCalled = 0u;
};

struct WhiteBoxGlSyncContext : public Context {
    using Context::Context;
    using Context::internalComputeCmdLists;
};

struct LeoGlSyncEventWindowsTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        cl_device_id clDeviceId = platform->getDevices()[0].get();
        leoContext = std::make_unique<WhiteBoxGlSyncContext>(nullptr, context->toHandle(), 1, &clDeviceId, true);
        ASSERT_EQ(CL_SUCCESS, leoContext->initialize());
        leoContext->registerSharing(new StubGlSyncSharingFunctionsWindows());
    }

    void TearDown() override {
        leoContext->internalComputeCmdLists.clear();
        leoContext.reset();
        Test<OclFixture>::TearDown();
    }

    std::unique_ptr<WhiteBoxGlSyncContext> leoContext;
    cl_GLsync glSync = reinterpret_cast<cl_GLsync>(0x9abc);
};

TEST_F(LeoGlSyncEventWindowsTest, givenInternalComputeCmdListCreationFailureWhenCreatingGlSyncEventThenErrorIsReturnedAndGlSyncIsNotRetained) {
    CommandListCreateImmediateHook createImmediateHook;
    createImmediateHook.resultToReturn = ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;

    cl_int err = CL_SUCCESS;
    auto glSyncEvent = GlSyncEvent::create(*leoContext, glSync, &err);

    EXPECT_EQ(nullptr, glSyncEvent);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, err);
    EXPECT_EQ(0u, StubGlSyncSharingFunctionsWindows::retainSyncCalled);
    EXPECT_TRUE(leoContext->internalComputeCmdLists.empty());
}

TEST_F(LeoGlSyncEventWindowsTest, givenInternalComputeCmdListWhenCreatingGlSyncEventThenSignalHostFunctionIsAppendedToIt) {
    CapturingCommandList capturingCmdList{};
    leoContext->internalComputeCmdLists[leoContext->getDefaultRootDeviceIndex()] = capturingCmdList.toHandle();

    cl_int err = CL_INVALID_VALUE;
    auto glSyncEvent = GlSyncEvent::create(*leoContext, glSync, &err);
    ASSERT_NE(nullptr, glSyncEvent);
    EXPECT_EQ(1u, StubGlSyncSharingFunctionsWindows::retainSyncCalled);

    ASSERT_EQ(1u, capturingCmdList.appendHostFunctionArgs.count());
    EXPECT_EQ(reinterpret_cast<ze_host_function_callback_t>(&GlSyncEvent::signalGlEvent), capturingCmdList.appendHostFunctionArgs[0].hostFunction);
    EXPECT_EQ(static_cast<void *>(glSyncEvent), capturingCmdList.appendHostFunctionArgs[0].userData);

    GlSyncEvent::signalGlEvent(glSyncEvent);
    cl_int eventStatus = CL_QUEUED;
    EXPECT_EQ(CL_SUCCESS, clGetEventInfo(glSyncEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(eventStatus), &eventStatus, nullptr));
    EXPECT_EQ(CL_COMPLETE, eventStatus);

    EXPECT_EQ(CL_SUCCESS, clReleaseEvent(glSyncEvent));
    EXPECT_EQ(1u, StubGlSyncSharingFunctionsWindows::releaseSyncCalled);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
