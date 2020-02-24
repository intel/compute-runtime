/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/os_interface.h"

#include "opencl/source/context/context.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/gl/gl_arb_sync_event.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/test/unit_test/mocks/gl/windows/mock_gl_arb_sync_event_windows.h"
#include "opencl/test/unit_test/mocks/gl/windows/mock_gl_sharing_windows.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "test.h"

using namespace NEO;

TEST(GlArbSyncEvent, whenCreateArbSyncEventNameIsCalledMultipleTimesThenEachCallReturnsUniqueName) {
    char *name1 = NEO::createArbSyncEventName();
    EXPECT_NE(nullptr, name1);
    EXPECT_STRNE("", name1);

    char *name2 = NEO::createArbSyncEventName();
    EXPECT_NE(nullptr, name2);
    EXPECT_STRNE("", name2);

    char *name3 = NEO::createArbSyncEventName();
    EXPECT_NE(nullptr, name3);
    EXPECT_STRNE("", name3);

    EXPECT_STRNE(name1, name2);
    EXPECT_STRNE(name1, name3);
    EXPECT_STRNE(name2, name3);

    NEO::destroyArbSyncEventName(name1);
    NEO::destroyArbSyncEventName(name2);
    NEO::destroyArbSyncEventName(name3);
}

template <bool SignalWaited>
inline void glArbSyncObjectWaitServerMock(NEO::OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo) {
    glSyncInfo.waitCalled = SignalWaited;
}

struct MockBaseEvent : Event {
    using Event::Event;
    bool wasUpdated = false;
    void updateExecutionStatus() override {
        Event::updateExecutionStatus();
        wasUpdated = true;
    }
};

struct GlArbSyncEventTest : public ::testing::Test {
    GlArbSyncEventTest(void) {
    }

    void SetUp() override {
        executionEnvironment = platform()->peekExecutionEnvironment();
        auto mockCsr = new MockCommandStreamReceiver(*executionEnvironment, 0);
        executionEnvironment->memoryManager = std::make_unique<OsAgnosticMemoryManager>(*executionEnvironment);
        device = std::make_unique<MockClDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0u));
        device->resetCommandStreamReceiver(mockCsr);
        ctx.reset(new MockContext);
        cmdQ.reset(new MockCommandQueue(ctx.get(), device.get(), nullptr));
        sharing = new GlSharingFunctionsMock();
        ctx->setSharingFunctions(sharing);
        sharing->pfnGlArbSyncObjectCleanup = glArbSyncObjectCleanupMockDoNothing;
        sharing->pfnGlArbSyncObjectSetup = mockGlArbSyncObjectSetup<false>;
        sharing->pfnGlArbSyncObjectSignal = glArbSyncObjectSignalMockDoNothing;
        sharing->pfnGlArbSyncObjectWaitServer = glArbSyncObjectWaitServerMock<false>;
        osInterface = new OSInterface;
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    }

    void TearDown() override {
        if (baseEvent) {
            triggerEvent->setStatus(-1);
            baseEvent->release();
            triggerEvent->release();
        }
    }

    template <typename T>
    T *createArbEventMock() {
        T *ret = new T(*ctx);
        ret->osInterface = osInterface;

        ret->baseEvent = getBaseEvent();
        baseEvent->incRefInternal();
        baseEvent->addChild(*ret);

        return ret;
    }

    MockBaseEvent *getBaseEvent() {
        if (baseEvent == nullptr) {
            triggerEvent = new UserEvent(ctx.get());
            baseEvent = new MockBaseEvent(cmdQ.get(), CL_COMMAND_RELEASE_GL_OBJECTS, CompletionStamp::levelNotReady, CompletionStamp::levelNotReady);
            triggerEvent->addChild(*baseEvent);
        }
        return baseEvent;
    }

    void failSyncObjectCreation() {
        sharing->pfnGlArbSyncObjectSetup = mockGlArbSyncObjectSetup<true>;
    }

    void setWaitCalledFlagOnServerWait() {
        sharing->pfnGlArbSyncObjectWaitServer = glArbSyncObjectWaitServerMock<true>;
    }

    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> ctx;
    std::unique_ptr<MockCommandQueue> cmdQ;
    OSInterface *osInterface = nullptr;
    Event *triggerEvent = nullptr;
    MockBaseEvent *baseEvent = nullptr;
    GlSharingFunctionsMock *sharing = nullptr;
    ExecutionEnvironment *executionEnvironment = nullptr;
};

TEST_F(GlArbSyncEventTest, whenGlArbEventIsCreatedThenBaseEventObjectIsConstructedWithProperContextAndCommandType) {
    auto *syncEv = createArbEventMock<DummyArbEvent<false>>();

    EXPECT_EQ(static_cast<unsigned int>(CL_COMMAND_GL_FENCE_SYNC_OBJECT_KHR), syncEv->getCommandType());
    EXPECT_EQ(ctx.get(), syncEv->getContext());
    EXPECT_NE(nullptr, syncEv->glSyncInfo);

    syncEv->release();
}

TEST_F(GlArbSyncEventTest, whenGetSyncInfoisCalledThenEventsSyncInfoIsReturned) {
    auto *syncEv = createArbEventMock<DummyArbEvent<false>>();

    EXPECT_NE(nullptr, syncEv->glSyncInfo);
    EXPECT_EQ(syncEv->glSyncInfo.get(), syncEv->getSyncInfo());

    syncEv->release();
}

TEST_F(GlArbSyncEventTest, whenSetBaseEventIsCalledThenProperMembersOfParentEventAreCopiedToSyncEventAndReferenceCountersAreUpdated) {
    ASSERT_NE(nullptr, getBaseEvent()->getCommandQueue());
    EXPECT_EQ(2, getBaseEvent()->getRefInternalCount());
    EXPECT_EQ(2, getBaseEvent()->getCommandQueue()->getRefInternalCount());
    EXPECT_FALSE(getBaseEvent()->peekHasChildEvents());
    auto *syncEv = new DummyArbEvent<false>(*ctx);

    EXPECT_EQ(nullptr, syncEv->baseEvent);
    EXPECT_EQ(nullptr, syncEv->osInterface);
    EXPECT_EQ(nullptr, syncEv->getCommandQueue());

    syncEv->useBaseSetEvent = true;
    bool ret = syncEv->setBaseEvent(*getBaseEvent());
    EXPECT_TRUE(ret);

    EXPECT_TRUE(getBaseEvent()->peekHasChildEvents());
    EXPECT_EQ(getBaseEvent(), syncEv->baseEvent);
    EXPECT_EQ(getBaseEvent()->getCommandQueue(), syncEv->getCommandQueue());
    EXPECT_EQ(syncEv->getCommandQueue()->getGpgpuCommandStreamReceiver().getOSInterface(), syncEv->osInterface);

    EXPECT_EQ(3, getBaseEvent()->getRefInternalCount());
    EXPECT_EQ(3, getBaseEvent()->getCommandQueue()->getRefInternalCount());
    EXPECT_TRUE(getBaseEvent()->peekHasChildEvents());

    syncEv->release();
}

TEST_F(GlArbSyncEventTest, whenSetBaseEventIsCalledButGlArbSyncObjectCreationFailsThenOperationIsAborted) {
    ASSERT_NE(nullptr, getBaseEvent()->getCommandQueue());
    EXPECT_EQ(2, getBaseEvent()->getRefInternalCount());
    EXPECT_EQ(2, getBaseEvent()->getCommandQueue()->getRefInternalCount());
    EXPECT_FALSE(getBaseEvent()->peekHasChildEvents());
    auto *syncEv = new DummyArbEvent<false>(*ctx);

    EXPECT_EQ(nullptr, syncEv->baseEvent);
    EXPECT_EQ(nullptr, syncEv->osInterface);
    EXPECT_EQ(nullptr, syncEv->getCommandQueue());

    syncEv->useBaseSetEvent = true;
    failSyncObjectCreation();
    bool ret = syncEv->setBaseEvent(*getBaseEvent());
    EXPECT_FALSE(ret);

    EXPECT_EQ(2, getBaseEvent()->getRefInternalCount());
    EXPECT_EQ(2, getBaseEvent()->getCommandQueue()->getRefInternalCount());
    EXPECT_FALSE(getBaseEvent()->peekHasChildEvents());

    EXPECT_EQ(nullptr, syncEv->baseEvent);
    EXPECT_EQ(nullptr, syncEv->osInterface);
    EXPECT_EQ(nullptr, syncEv->getCommandQueue());

    syncEv->osInterface = this->osInterface;
    syncEv->baseEvent = getBaseEvent();
    getBaseEvent()->incRefInternal();
    syncEv->release();
}

TEST_F(GlArbSyncEventTest, whenGlArbSyncEventGetsUnblockedByTerminatedBaseEventThenSyncObjectDoesntGetSignalled) {
    auto *syncEv = createArbEventMock<DummyArbEvent<false>>();
    triggerEvent->setStatus(-1);
    EXPECT_FALSE(syncEv->getSyncInfo()->waitCalled);
    syncEv->release();
}

TEST_F(GlArbSyncEventTest, whenGlArbSyncEventGetsUnblockedByQueuedBaseEventThenSyncObjectDoesntGetSignalled) {
    auto *syncEv = createArbEventMock<DummyArbEvent<false>>();
    syncEv->unblockEventBy(*this->baseEvent, 0, CL_QUEUED);
    EXPECT_FALSE(syncEv->getSyncInfo()->waitCalled);
    syncEv->release();
}

TEST_F(GlArbSyncEventTest, whenGlArbSyncEventGetsUnblockedBySubmittedOrCompletedEventThenSyncObjectGetsSignalled) {
    setWaitCalledFlagOnServerWait();

    auto *syncEv = createArbEventMock<DummyArbEvent<false>>();
    triggerEvent->setStatus(CL_COMPLETE);
    EXPECT_TRUE(syncEv->getSyncInfo()->waitCalled);
    syncEv->release();
}

TEST_F(GlArbSyncEventTest, whenGlArbSyncEventIsCreatedFromBaseEventWithoutValidContextThenCreationFails) {
    Event *baseEvent = new Event(nullptr, CL_COMMAND_RELEASE_GL_OBJECTS, CompletionStamp::levelNotReady, CompletionStamp::levelNotReady);
    auto *arbEvent = GlArbSyncEvent::create(*baseEvent);
    EXPECT_EQ(nullptr, arbEvent);
    baseEvent->release();
}

TEST_F(GlArbSyncEventTest, whenGlArbSyncEventIsCreatedAndSetEventFailsThenCreationFails) {
    failSyncObjectCreation();
    auto *arbEvent = GlArbSyncEvent::create(*this->getBaseEvent());
    EXPECT_EQ(nullptr, arbEvent);
}

TEST_F(GlArbSyncEventTest, whenGlArbSyncEventIsCreatedTheBaseEventIsProperlySet) {
    auto *arbEvent = GlArbSyncEvent::create(*this->getBaseEvent());
    EXPECT_NE(nullptr, arbEvent);
    EXPECT_TRUE(this->baseEvent->peekHasChildEvents());
    EXPECT_EQ(arbEvent, this->baseEvent->peekChildEvents()->ref);
    arbEvent->release();
}

TEST_F(GlArbSyncEventTest, whenClEnqueueMarkerWithSyncObjectINTELIsCalledThenInvalidOperationErrorCodeIsReturned) {
    cl_command_queue queue = static_cast<cl_command_queue>(this->cmdQ.get());
    auto ret = clEnqueueMarkerWithSyncObjectINTEL(queue, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, ret);
}

TEST_F(GlArbSyncEventTest, whenClGetCLObjectInfoINTELIsCalledThenInvalidOperationErrorCodeIsReturned) {
    cl_mem mem = {};
    auto ret = clGetCLObjectInfoINTEL(mem, nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, ret);
}

TEST_F(GlArbSyncEventTest, givenNullSynInfoParameterWhenClGetCLEventInfoINTELIsCalledThenInvalidArgValueErrorCodeIsReturned) {
    cl_event ev = getBaseEvent();
    cl_context ctxRet = {};
    auto ret = clGetCLEventInfoINTEL(ev, nullptr, &ctxRet);
    EXPECT_EQ(CL_INVALID_ARG_VALUE, ret);
}

TEST_F(GlArbSyncEventTest, givenNullContextParameterWhenClGetCLEventInfoINTELIsCalledThenInvalidArgValueErrorCodeIsReturned) {
    cl_event ev = getBaseEvent();
    CL_GL_SYNC_INFO *synInfoRet = nullptr;
    auto ret = clGetCLEventInfoINTEL(ev, &synInfoRet, nullptr);
    EXPECT_EQ(CL_INVALID_ARG_VALUE, ret);
}

TEST_F(GlArbSyncEventTest, givenUnknownEventWhenclGetCLEventInfoINTELIsCalledThenInvalidEventErrorCodeIsReturned) {
    auto deadEvent = new MockEvent<Event>(nullptr, 0, 0, 0);
    deadEvent->magic = Event::deadMagic;
    cl_event unknownEvent = deadEvent;
    CL_GL_SYNC_INFO *synInfoRet = nullptr;
    cl_context ctxRet = {};
    auto ret = clGetCLEventInfoINTEL(unknownEvent, &synInfoRet, &ctxRet);
    EXPECT_EQ(CL_INVALID_EVENT, ret);
    deadEvent->release();
}

TEST_F(GlArbSyncEventTest, givenEventWithCommandDifferentThanReleaseGlObjectsWhenClGetCLEventInfoINTELIsCalledThenValidContextIsReturned) {
    getBaseEvent();
    cl_event ev = triggerEvent;
    CL_GL_SYNC_INFO *synInfoRet = reinterpret_cast<CL_GL_SYNC_INFO *>(static_cast<uintptr_t>(0xFF));
    cl_context ctxRet = {};
    auto ret = clGetCLEventInfoINTEL(ev, &synInfoRet, &ctxRet);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(nullptr, synInfoRet);
    EXPECT_EQ(ctxRet, ctx.get());
}

TEST_F(GlArbSyncEventTest, givenDisabledSharingWhenClGetCLEventInfoINTELIsCalledThenInvalidOperationErrorCodeIsReturned) {
    getBaseEvent();
    cl_event ev = baseEvent;
    CL_GL_SYNC_INFO *synInfoRet = reinterpret_cast<CL_GL_SYNC_INFO *>(static_cast<uintptr_t>(0xFF));
    cl_context ctxRet = {};
    auto sharing = ctx->getSharing<NEO::GLSharingFunctions>();
    ctx->sharingFunctions[sharing->getId()] = nullptr;
    auto ret = clGetCLEventInfoINTEL(ev, &synInfoRet, &ctxRet);
    ctx->setSharingFunctions(new GlSharingFunctionsMock());
    EXPECT_EQ(CL_INVALID_OPERATION, ret);
}

TEST_F(GlArbSyncEventTest, givenCallToClGetCLEventInfoINTELWhenGetOrCreateGlArbSyncFailsThenOutOfMemoryErrorCodeIsReturned) {
    getBaseEvent();
    cl_event ev = this->baseEvent;
    CL_GL_SYNC_INFO *synInfoRet = reinterpret_cast<CL_GL_SYNC_INFO *>(static_cast<uintptr_t>(0xFF));
    cl_context ctxRet = {};
    sharing->pfnGlArbSyncObjectSetup = mockGlArbSyncObjectSetup<true>;
    auto ret = clGetCLEventInfoINTEL(ev, &synInfoRet, &ctxRet);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, ret);
}

TEST_F(GlArbSyncEventTest, givenCallToClGetCLEventInfoINTELWhenFunctionSucceedsThenEventsGetUpdatedAndValidContextAndSyncInfoAreReturned) {
    auto *arbEvent = GlArbSyncEvent::create(*this->getBaseEvent());
    this->sharing->glArbEventMapping[this->baseEvent] = arbEvent;
    cl_event ev = this->baseEvent;
    CL_GL_SYNC_INFO *synInfoRet = reinterpret_cast<CL_GL_SYNC_INFO *>(static_cast<uintptr_t>(0xFF));
    cl_context ctxRet = {};
    EXPECT_FALSE(this->baseEvent->wasUpdated);
    auto ret = clGetCLEventInfoINTEL(ev, &synInfoRet, &ctxRet);
    EXPECT_TRUE(this->baseEvent->wasUpdated);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(ctx.get(), ctxRet);
    EXPECT_EQ(arbEvent->getSyncInfo(), synInfoRet);
    arbEvent->release();
}

TEST_F(GlArbSyncEventTest, givenUnknownEventWhenClReleaseGlSharedEventINTELIsCalledThenInvalidEventErrorCodeIsReturned) {
    auto deadEvent = new MockEvent<Event>(nullptr, 0, 0, 0);
    deadEvent->magic = Event::deadMagic;
    cl_event unknownEvent = deadEvent;
    auto ret = clReleaseGlSharedEventINTEL(unknownEvent);
    EXPECT_EQ(CL_INVALID_EVENT, ret);
    deadEvent->release();
}

TEST_F(GlArbSyncEventTest, givenEventWithoutArbSyncWhenClReleaseGlSharedEventINTELIsCalledThenThisEventsRefcountIsDecreased) {
    this->getBaseEvent();
    triggerEvent->retain();
    EXPECT_EQ(2, triggerEvent->getRefInternalCount());
    cl_event ev = triggerEvent;
    auto ret = clReleaseGlSharedEventINTEL(ev);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(1, triggerEvent->getRefInternalCount());
}

TEST_F(GlArbSyncEventTest, givenEventWithArbSyncWhenClReleaseGlSharedEventINTELIsCalledThenThisEventsAndArbSyncsRefcountsAreDecreased) {
    auto *arbEvent = GlArbSyncEvent::create(*this->getBaseEvent());
    baseEvent->retain();
    arbEvent->retain();
    this->sharing->glArbEventMapping[baseEvent] = arbEvent;
    EXPECT_EQ(4, baseEvent->getRefInternalCount());
    EXPECT_EQ(3, arbEvent->getRefInternalCount());
    cl_event ev = baseEvent;
    auto ret = clReleaseGlSharedEventINTEL(ev);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(3, baseEvent->getRefInternalCount());
    EXPECT_EQ(2, arbEvent->getRefInternalCount());
    arbEvent->release();
}
