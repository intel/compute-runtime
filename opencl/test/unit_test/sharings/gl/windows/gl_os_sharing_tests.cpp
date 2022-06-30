/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"

#include "opencl/extensions/public/cl_gl_private_intel.h"
#include "opencl/source/sharings/gl/gl_arb_sync_event.h"
#include "opencl/source/sharings/gl/windows/gl_sharing_windows.h"
#include "opencl/test/unit_test/mocks/gl/windows/mock_gl_sharing_windows.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"
#include <GL/gl.h>

using namespace NEO;

struct MockOSInterface : OSInterface {
    static HANDLE createEvent(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName, void *data) {
        MockOSInterface *self = reinterpret_cast<MockOSInterface *>(data);
        if (self->eventNum++ == self->failEventNum) {
            return INVALID_HANDLE;
        }
        return handleValue;
    }
    static BOOL closeHandle(HANDLE hObject, void *data) {
        MockOSInterface *self = reinterpret_cast<MockOSInterface *>(data);
        ++self->closedEventsCount;
        return (reinterpret_cast<HANDLE>(dummyHandle) == hObject) ? TRUE : FALSE;
    }
    int eventNum = 1;
    int failEventNum = 0;
    int closedEventsCount = 0;
};

TEST(glSharingBasicTest, GivenSharingFunctionsWhenItIsConstructedThenBackupContextIsCreated) {
    GLType GLHDCType = CL_WGL_HDC_KHR;
    GLContext GLHGLRCHandle = 0;
    GLDisplay GLHDCHandle = 0;
    int32_t expectedContextAttrs[3] = {0};
    GlDllHelper dllHelper;

    auto glSharingFunctions = new GlSharingFunctionsMock(GLHDCType, GLHGLRCHandle, GLHGLRCHandle, GLHDCHandle);

    EXPECT_EQ(1, dllHelper.getParam("WGLCreateContextCalled"));
    EXPECT_EQ(1, dllHelper.getParam("WGLShareListsCalled"));
    EXPECT_EQ(0, EGLChooseConfigCalled);
    EXPECT_EQ(0, EGLCreateContextCalled);
    EXPECT_EQ(0, GlxChooseFBConfigCalled);
    EXPECT_EQ(0, GlxQueryContextCalled);
    EXPECT_EQ(0, GlxCreateNewContextCalled);
    EXPECT_EQ(0, GlxIsDirectCalled);
    EXPECT_EQ(0, eglBkpContextParams.configAttrs);
    EXPECT_EQ(0, eglBkpContextParams.numConfigs);
    EXPECT_TRUE(glSharingFunctions->getBackupContextHandle() != 0);
    EXPECT_TRUE(memcmp(eglBkpContextParams.contextAttrs, expectedContextAttrs, 3 * sizeof(int32_t)) == 0);
    EXPECT_EQ(0, glxBkpContextParams.FBConfigAttrs);
    EXPECT_EQ(0, glxBkpContextParams.queryAttribute);
    EXPECT_EQ(0, glxBkpContextParams.renderType);
    delete glSharingFunctions;
    EXPECT_EQ(1, dllHelper.getParam("WGLDeleteContextCalled"));
    EXPECT_EQ(1, dllHelper.getParam("GLDeleteContextCalled"));
}

struct GlArbSyncEventOsTest : public ::testing::Test {
    void SetUp() override {
        rootDeviceEnvironment = std::make_unique<RootDeviceEnvironment>(executionEnvironment);
        sharing.GLContextHandle = 0x2cU;
        sharing.GLDeviceHandle = 0x3cU;
        wddm = new WddmMock(*rootDeviceEnvironment);
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        osInterface = rootDeviceEnvironment->osInterface.get();
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        gdi = new MockGdi();
        wddm->resetGdi(gdi);
    }
    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironment;
    GlSharingFunctionsMock sharing;
    MockGdi *gdi = nullptr;
    WddmMock *wddm = nullptr;
    CL_GL_SYNC_INFO syncInfo = {};
    OSInterface *osInterface = nullptr;
};

TEST_F(GlArbSyncEventOsTest, WhenCreateSynchronizationObjectSucceedsThenAllHAndlesAreValid) {
    struct CreateSyncObjectMock {
        static int &getHandle() {
            static int handle = 1;
            return handle;
        }

        static void reset() {
            getHandle() = 1;
        }

        static NTSTATUS __stdcall createSynchObject(D3DKMT_CREATESYNCHRONIZATIONOBJECT *pData) {
            if (pData == nullptr) {
                return STATUS_INVALID_PARAMETER;
            }

            EXPECT_NE(NULL, pData->hDevice);
            EXPECT_EQ(D3DDDI_SEMAPHORE, pData->Info.Type);

            EXPECT_EQ(32, pData->Info.Semaphore.MaxCount);
            EXPECT_EQ(0, pData->Info.Semaphore.InitialCount);

            pData->hSyncObject = getHandle()++;
            return STATUS_SUCCESS;
        }

        static NTSTATUS __stdcall createSynchObject2(D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *pData) {
            if (pData == nullptr) {
                return STATUS_INVALID_PARAMETER;
            }

            EXPECT_NE(NULL, pData->hDevice);
            EXPECT_EQ(D3DDDI_CPU_NOTIFICATION, pData->Info.Type);
            EXPECT_NE(nullptr, pData->Info.CPUNotification.Event);

            pData->hSyncObject = getHandle()++;
            return STATUS_SUCCESS;
        }
    };
    CreateSyncObjectMock::reset();

    wddm->init();
    gdi->createSynchronizationObject.mFunc = CreateSyncObjectMock::createSynchObject;
    gdi->createSynchronizationObject2.mFunc = CreateSyncObjectMock::createSynchObject2;
    auto ret = setupArbSyncObject(sharing, *osInterface, syncInfo);
    EXPECT_TRUE(ret);
    EXPECT_EQ(1U, syncInfo.serverSynchronizationObject);
    EXPECT_EQ(2U, syncInfo.clientSynchronizationObject);
    EXPECT_EQ(3U, syncInfo.submissionSynchronizationObject);
    EXPECT_EQ(sharing.GLContextHandle, syncInfo.hContextToBlock);
    EXPECT_NE(nullptr, syncInfo.event);
    EXPECT_NE(nullptr, syncInfo.eventName);
    EXPECT_NE(nullptr, syncInfo.submissionEvent);
    EXPECT_NE(nullptr, syncInfo.submissionEventName);
    EXPECT_FALSE(syncInfo.waitCalled);
    cleanupArbSyncObject(*osInterface, &syncInfo);
}

TEST_F(GlArbSyncEventOsTest, GivenNewGlSyncInfoWhenCreateSynchronizationObjectFailsThenSetupArbSyncObjectFails) {
    struct CreateSyncObjectMock {
        static int &getHandle() {
            static int handle = 1;
            return handle;
        }

        static int &getFailHandleId() {
            static int failHandleId = 0;
            return failHandleId;
        }

        static void reset() {
            getHandle() = 1;
            getFailHandleId() = 0;
        }

        static NTSTATUS __stdcall createSynchObject(D3DKMT_CREATESYNCHRONIZATIONOBJECT *pData) {
            auto newHandle = getHandle()++;
            if (newHandle == getFailHandleId()) {
                return STATUS_INVALID_PARAMETER;
            }
            return STATUS_SUCCESS;
        }

        static NTSTATUS __stdcall createSynchObject2(D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *pData) {
            auto newHandle = getHandle()++;
            if (newHandle == getFailHandleId()) {
                return STATUS_INVALID_PARAMETER;
            }
            return STATUS_SUCCESS;
        }
    };
    CreateSyncObjectMock::reset();
    wddm->init();
    gdi->createSynchronizationObject.mFunc = CreateSyncObjectMock::createSynchObject;
    gdi->createSynchronizationObject2.mFunc = CreateSyncObjectMock::createSynchObject2;

    CreateSyncObjectMock::getFailHandleId() = CreateSyncObjectMock::getHandle();
    int failuresCount = 0;
    auto ret = setupArbSyncObject(sharing, *osInterface, syncInfo);
    while (false == ret) {
        ++failuresCount;
        CreateSyncObjectMock::getHandle() = 1;
        ++CreateSyncObjectMock::getFailHandleId();
        ret = setupArbSyncObject(sharing, *osInterface, syncInfo);
    }
    EXPECT_EQ(3, failuresCount);
    cleanupArbSyncObject(*osInterface, &syncInfo);
}

TEST_F(GlArbSyncEventOsTest, GivenNewGlSyncInfoWhenCreateEventFailsThenSetupArbSyncObjectFails) {
    auto rootDeviceEnvironment = platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();

    MockOSInterface mockOsInterface;
    auto createEventMock = changeSysCallMock(mockCreateEventClb, mockCreateEventClbData, MockOSInterface::createEvent, &mockOsInterface);

    auto wddm = new WddmMock(*rootDeviceEnvironment);
    auto gdi = new MockGdi();
    wddm->resetGdi(gdi);
    wddm->init();

    mockOsInterface.setDriverModel(std::unique_ptr<DriverModel>(wddm));

    mockOsInterface.failEventNum = mockOsInterface.eventNum;
    int failuresCount = 0;
    auto ret = setupArbSyncObject(sharing, mockOsInterface, syncInfo);
    while (false == ret) {
        ++failuresCount;
        mockOsInterface.eventNum = 1;
        ++mockOsInterface.failEventNum;
        ret = setupArbSyncObject(sharing, mockOsInterface, syncInfo);
    }
    EXPECT_EQ(2, failuresCount);
    cleanupArbSyncObject(mockOsInterface, &syncInfo);
}

TEST_F(GlArbSyncEventOsTest, GivenInvalidGlSyncInfoWhenCleanupArbSyncObjectIsCalledThenDestructorsOfSyncOrEventsAreNotInvoked) {
    struct DestroySyncObjectMock {
        static NTSTATUS __stdcall destroySynchObject(_In_ CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *sync) {
            EXPECT_FALSE(true);
            return STATUS_INVALID_PARAMETER;
        }
    };
    auto rootDeviceEnvironment = platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();

    auto wddm = new WddmMock(*rootDeviceEnvironment);
    auto gdi = new MockGdi();
    wddm->resetGdi(gdi);
    wddm->init();

    MockOSInterface mockOsInterface;
    auto closeHandleMock = changeSysCallMock(mockCloseHandleClb, mockCloseHandleClbData, MockOSInterface::closeHandle, &mockOsInterface);
    mockOsInterface.setDriverModel(std::unique_ptr<DriverModel>(wddm));

    gdi->destroySynchronizationObject = DestroySyncObjectMock::destroySynchObject;
    cleanupArbSyncObject(mockOsInterface, nullptr);
    EXPECT_EQ(0, mockOsInterface.closedEventsCount);
}

TEST_F(GlArbSyncEventOsTest, GivenValidGlSyncInfoWhenCleanupArbSyncObjectIsCalledThenProperCountOfDestructorsOfSyncAndEventsIsNotInvoked) {
    struct CreateDestroySyncObjectMock {
        static int &getDestroyCounter() {
            static int counter = 0;
            return counter;
        }
        static NTSTATUS __stdcall destroySynchObject(_In_ CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *sync) {
            ++getDestroyCounter();
            return STATUS_SUCCESS;
        }
        static void reset() {
            getDestroyCounter() = 0;
        }
    };
    auto rootDeviceEnvironment = platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();

    auto wddm = new WddmMock(*rootDeviceEnvironment);
    auto gdi = new MockGdi();
    wddm->resetGdi(gdi);
    wddm->init();

    MockOSInterface mockOsInterface;
    auto closeHandleMock = changeSysCallMock(mockCloseHandleClb, mockCloseHandleClbData, MockOSInterface::closeHandle, &mockOsInterface);
    mockOsInterface.setDriverModel(std::unique_ptr<DriverModel>(wddm));

    CreateDestroySyncObjectMock::reset();
    gdi->destroySynchronizationObject = CreateDestroySyncObjectMock::destroySynchObject;

    auto ret = setupArbSyncObject(sharing, mockOsInterface, syncInfo);
    EXPECT_TRUE(ret);
    syncInfo.serverSynchronizationObject = 0x5cU;
    syncInfo.clientSynchronizationObject = 0x7cU;
    syncInfo.submissionSynchronizationObject = 0x13cU;

    cleanupArbSyncObject(mockOsInterface, &syncInfo);
    EXPECT_EQ(2, mockOsInterface.closedEventsCount);
    EXPECT_EQ(3, CreateDestroySyncObjectMock::getDestroyCounter());
}

TEST_F(GlArbSyncEventOsTest, GivenCallToSignalArbSyncObjectWhenSignalSynchronizationObjectForServerClientSyncFailsThenSubmissionSyncDoesNotGetSignalled) {
    struct FailSignalSyncObjectMock {
        static NTSTATUS __stdcall signal(_In_ CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *obj) {
            EXPECT_NE(nullptr, obj);
            if (obj == nullptr) {
                return STATUS_INVALID_PARAMETER;
            }

            EXPECT_EQ(2, obj->ObjectCount);
            EXPECT_EQ(getExpectedSynchHandle0(), obj->ObjectHandleArray[0]);
            EXPECT_EQ(getExpectedSynchHandle1(), obj->ObjectHandleArray[1]);
            EXPECT_EQ(0, obj->Flags.SignalAtSubmission);
            EXPECT_EQ(getExpectedContextHandle(), obj->hContext);
            return STATUS_INVALID_PARAMETER;
        }

        static D3DKMT_HANDLE &getExpectedSynchHandle0() {
            static D3DKMT_HANDLE handle = INVALID_HANDLE;
            return handle;
        }

        static D3DKMT_HANDLE &getExpectedSynchHandle1() {
            static D3DKMT_HANDLE handle = INVALID_HANDLE;
            return handle;
        }

        static D3DKMT_HANDLE &getExpectedContextHandle() {
            static D3DKMT_HANDLE handle = INVALID_HANDLE;
            return handle;
        }

        static void reset() {
            getExpectedSynchHandle0() = INVALID_HANDLE;
            getExpectedSynchHandle1() = INVALID_HANDLE;
            getExpectedContextHandle() = INVALID_HANDLE;
        }
    };
    FailSignalSyncObjectMock::reset();
    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
    wddm->init();
    OsContextWin osContext(*wddm, 0u,
                           EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0], preemptionMode));

    CL_GL_SYNC_INFO syncInfo = {};
    syncInfo.serverSynchronizationObject = 0x5cU;
    syncInfo.clientSynchronizationObject = 0x6cU;

    gdi->signalSynchronizationObject.mFunc = FailSignalSyncObjectMock::signal;
    FailSignalSyncObjectMock::getExpectedContextHandle() = osContext.getWddmContextHandle();
    FailSignalSyncObjectMock::getExpectedSynchHandle0() = syncInfo.serverSynchronizationObject;
    FailSignalSyncObjectMock::getExpectedSynchHandle1() = syncInfo.clientSynchronizationObject;

    signalArbSyncObject(osContext, syncInfo);
}

TEST_F(GlArbSyncEventOsTest, GivenCallToSignalArbSyncObjectWhenSignalSynchronizationObjectForServerClientSyncSucceedsThenSubmissionSyncGetsSignalledAsWell) {
    struct FailSignalSyncObjectMock {
        static NTSTATUS __stdcall signal(_In_ CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *obj) {
            EXPECT_NE(nullptr, obj);
            if (obj == nullptr) {
                return STATUS_INVALID_PARAMETER;
            }

            // validating only second call to signal
            if (getCounter()++ != 1) {
                return STATUS_SUCCESS;
            }

            EXPECT_EQ(1, obj->ObjectCount);
            EXPECT_EQ(getExpectedSynchHandle0(), obj->ObjectHandleArray[0]);
            EXPECT_EQ(1, obj->Flags.SignalAtSubmission);
            EXPECT_EQ(getExpectedContextHandle(), obj->hContext);

            return STATUS_SUCCESS;
        }

        static D3DKMT_HANDLE &getExpectedSynchHandle0() {
            static D3DKMT_HANDLE handle = INVALID_HANDLE;
            return handle;
        }

        static int &getCounter() {
            static int counter = 0;
            return counter;
        }

        static D3DKMT_HANDLE &getExpectedContextHandle() {
            static D3DKMT_HANDLE handle = INVALID_HANDLE;
            return handle;
        }

        static void reset() {
            getExpectedSynchHandle0() = INVALID_HANDLE;
            getCounter() = 0;
            getExpectedContextHandle() = INVALID_HANDLE;
        }
    };
    FailSignalSyncObjectMock::reset();
    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
    wddm->init();
    OsContextWin osContext(*wddm, 0u, EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0], preemptionMode));

    CL_GL_SYNC_INFO syncInfo = {};
    syncInfo.submissionSynchronizationObject = 0x7cU;

    gdi->signalSynchronizationObject.mFunc = FailSignalSyncObjectMock::signal;
    FailSignalSyncObjectMock::getExpectedContextHandle() = osContext.getWddmContextHandle();
    FailSignalSyncObjectMock::getExpectedSynchHandle0() = syncInfo.submissionSynchronizationObject;

    signalArbSyncObject(osContext, syncInfo);
}

TEST_F(GlArbSyncEventOsTest, GivenCallToServerWaitForArbSyncObjectWhenWaitForSynchronizationObjectFailsThenWaitFlagDoesNotGetSet) {
    struct FailWaitSyncObjectMock {
        static NTSTATUS __stdcall waitForSynchObject(_In_ CONST_FROM_WDK_10_0_18328_0 D3DKMT_WAITFORSYNCHRONIZATIONOBJECT *waitData) {
            EXPECT_NE(nullptr, waitData);
            if (waitData == nullptr) {
                return STATUS_INVALID_PARAMETER;
            }

            EXPECT_EQ(1, waitData->ObjectCount);
            EXPECT_EQ(getExpectedSynchHandle0(), waitData->ObjectHandleArray[0]);
            EXPECT_EQ(getExpectedContextHandle(), waitData->hContext);
            return STATUS_INVALID_PARAMETER;
        }

        static D3DKMT_HANDLE &getExpectedSynchHandle0() {
            static D3DKMT_HANDLE handle = INVALID_HANDLE;
            return handle;
        }

        static D3DKMT_HANDLE &getExpectedContextHandle() {
            static D3DKMT_HANDLE handle = INVALID_HANDLE;
            return handle;
        }

        static void reset() {
            getExpectedSynchHandle0() = INVALID_HANDLE;
            getExpectedContextHandle() = INVALID_HANDLE;
        }
    };

    FailWaitSyncObjectMock::reset();

    CL_GL_SYNC_INFO syncInfo = {};
    syncInfo.hContextToBlock = 0x4cU;

    FailWaitSyncObjectMock::getExpectedSynchHandle0() = syncInfo.serverSynchronizationObject;
    FailWaitSyncObjectMock::getExpectedContextHandle() = syncInfo.hContextToBlock;
    gdi->waitForSynchronizationObject.mFunc = FailWaitSyncObjectMock::waitForSynchObject;

    EXPECT_FALSE(syncInfo.waitCalled);
    serverWaitForArbSyncObject(*osInterface, syncInfo);
    EXPECT_FALSE(syncInfo.waitCalled);
}

TEST_F(GlArbSyncEventOsTest, GivenCallToServerWaitForArbSyncObjectWhenWaitForSynchronizationObjectSucceedsThenWaitFlagGetsSet) {
    CL_GL_SYNC_INFO syncInfo = {};
    syncInfo.serverSynchronizationObject = 0x7cU;

    EXPECT_FALSE(syncInfo.waitCalled);
    serverWaitForArbSyncObject(*osInterface, syncInfo);
    EXPECT_TRUE(syncInfo.waitCalled);
}
