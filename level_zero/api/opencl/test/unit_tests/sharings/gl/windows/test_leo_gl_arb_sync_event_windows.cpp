/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/extensions/public/cl_gl_private_intel.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/event/leo_event.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/sharings/gl/leo_gl_arb_sync_event.h"
#include "level_zero/api/opencl/source/sharings/gl/windows/leo_gl_sharing_windows.h"
#include "level_zero/api/opencl/test/common/fixtures/leo_capture_fixture.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

struct StubGlArbSyncSharingFunctionsWindows : public GLSharingFunctionsWindows {
    StubGlArbSyncSharingFunctionsWindows() {
        pfnGlArbSyncObjectSetup = [](GLSharingFunctions &, OSInterface &, CL_GL_SYNC_INFO &) { return true; };
        pfnGlArbSyncObjectCleanup = [](OSInterface &, CL_GL_SYNC_INFO *) {};
        pfnGlArbSyncObjectSignal = [](OsContext &, CL_GL_SYNC_INFO &, bool signalAtSubmission) {
            signalCount++;
            lastSignalAtSubmission = signalAtSubmission;
        };
        pfnGlArbSyncObjectWaitServer = [](OSInterface &, CL_GL_SYNC_INFO &, bool) {};
        signalCount = 0u;
        lastSignalAtSubmission = false;
    }

    inline static uint32_t signalCount = 0u;
    inline static bool lastSignalAtSubmission = false;
};

struct LeoGlArbSyncEventWindowsTest : public Test<LeoCaptureFixture> {
    void SetUp() override {
        Test<LeoCaptureFixture>::SetUp();
        castToObject<Context>(clContext)->registerSharing(new StubGlArbSyncSharingFunctionsWindows());
        neoDevice->getRootDeviceEnvironmentRef().osInterface = std::make_unique<OSInterface>();
    }

    void TearDown() override {
        neoDevice->getRootDeviceEnvironmentRef().osInterface.reset();
        Test<LeoCaptureFixture>::TearDown();
    }

    void createArbSyncEventAndSetUserEventStatus(cl_int executionStatus) {
        cl_int errcode = CL_SUCCESS;
        cl_event userEvent = clCreateUserEvent(clContext, &errcode);
        ASSERT_EQ(CL_SUCCESS, errcode);
        ASSERT_NE(nullptr, GlArbSyncEvent::create(*castToObject<Event>(userEvent)));
        EXPECT_EQ(0u, StubGlArbSyncSharingFunctionsWindows::signalCount);

        EXPECT_EQ(CL_SUCCESS, clSetUserEventStatus(userEvent, executionStatus));
        EXPECT_EQ(CL_SUCCESS, clReleaseEvent(userEvent));
    }
};

TEST_F(LeoGlArbSyncEventWindowsTest, givenUserEventWhenItCompletesThenGlArbSyncObjectIsSignalledAtSubmission) {
    createArbSyncEventAndSetUserEventStatus(CL_COMPLETE);

    EXPECT_EQ(1u, StubGlArbSyncSharingFunctionsWindows::signalCount);
    EXPECT_TRUE(StubGlArbSyncSharingFunctionsWindows::lastSignalAtSubmission);
}

TEST_F(LeoGlArbSyncEventWindowsTest, givenUserEventWhenItTerminatesThenGlArbSyncObjectIsNotSignalled) {
    createArbSyncEventAndSetUserEventStatus(-1);

    EXPECT_EQ(0u, StubGlArbSyncSharingFunctionsWindows::signalCount);
}

TEST_F(LeoGlArbSyncEventWindowsTest, givenTerminatedContextWhenEventCompletesThenGlArbSyncObjectIsNotSignalled) {
    castToObject<Context>(clContext)->terminateExecution();
    createArbSyncEventAndSetUserEventStatus(CL_COMPLETE);

    EXPECT_EQ(0u, StubGlArbSyncSharingFunctionsWindows::signalCount);
}

struct LeoGlArbSyncEventOsTest : public ::testing::Test {
    struct SignalSyncObjectMock {
        static NTSTATUS __stdcall signal(_In_ CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *obj) {
            if (obj->ObjectCount == 2u) {
                serverClientSignalAtSubmission = obj->Flags.SignalAtSubmission;
            }
            return STATUS_SUCCESS;
        }

        inline static UINT serverClientSignalAtSubmission = 0u;
    };

    void SetUp() override {
        rootDeviceEnvironment = std::make_unique<RootDeviceEnvironment>(executionEnvironment);
        wddm = new WddmMock(*rootDeviceEnvironment);
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        gdi = new MockGdi();
        wddm->resetGdi(gdi);
        wddm->init();
        gdi->signalSynchronizationObject.mFunc = SignalSyncObjectMock::signal;
    }

    uint32_t signalAndGetServerClientSignalAtSubmission(bool signalAtSubmission) {
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
        OsContextWin osContext(*wddm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment)[0], preemptionMode));

        CL_GL_SYNC_INFO syncInfo = {};
        SignalSyncObjectMock::serverClientSignalAtSubmission = signalAtSubmission ? 0u : 1u;
        signalArbSyncObject(osContext, syncInfo, signalAtSubmission);
        return SignalSyncObjectMock::serverClientSignalAtSubmission;
    }

    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironment;
    MockGdi *gdi = nullptr;
    WddmMock *wddm = nullptr;
};

TEST_F(LeoGlArbSyncEventOsTest, givenSignalAtSubmissionWhenSignalArbSyncObjectIsCalledThenServerClientSyncIsSignalledAtSubmission) {
    EXPECT_EQ(1u, signalAndGetServerClientSignalAtSubmission(true));
}

TEST_F(LeoGlArbSyncEventOsTest, givenNoSignalAtSubmissionWhenSignalArbSyncObjectIsCalledThenServerClientSyncIsSignalledAfterGpuCompletion) {
    EXPECT_EQ(0u, signalAndGetServerClientSignalAtSubmission(false));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
